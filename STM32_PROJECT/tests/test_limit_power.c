#include "limit_power.h"
#include "power_port.h"
#define CHECK(x) do { if(!(x))return __LINE__; } while(0)
unsigned tests_passed;
void *memset(void *p,int v,unsigned n) { unsigned char *s=p;while(n--)*s++=(unsigned char)v;return p; }
static uint32_t now,generation,mask;
static uint8_t enabled,idle_safe;
static ValveInputs in;
static ValveStatus status;
uint32_t PowerPort_Lock(void) { uint32_t old=mask;mask=1;return old; }
void PowerPort_Unlock(uint32_t old) { mask=old; }
void LimitPowerPort_SetEnabled(uint8_t on) { enabled=on; }
uint8_t LimitPowerPort_IsEnabled(void) { return enabled; }
uint8_t LimitPowerPort_IdleSafe(const ValveInputs *i,uint32_t g)
{ (void)i;return idle_safe && g==generation; }
static void reset(void)
{
 now=generation=mask=0;enabled=0;idle_safe=1;
 in=(ValveInputs){.close_limit=1};status=(ValveStatus){.state=VALVE_CLOSED};
 LimitPower_Init(now);
}
static void sample(uint32_t ms,uint8_t can_idle)
{ now+=ms;LimitPower_Filter(now,&in);LimitPower_Service(now,&in,&status,can_idle,generation); }
#if POWER_LOW_POWER_MODE == 2
static void idle_off(void)
{ sample(LIMIT_POWER_SETTLE_MS,1);sample(LIMIT_POWER_CONFIRM_MS,1); }
#endif
int run_tests(void)
{
 reset();CHECK(enabled && !LimitPower_Valid() && !LimitPower_CanSleep());tests_passed++;
 sample(LIMIT_POWER_SETTLE_MS-1,1);CHECK(in.limits_invalid && !LimitPower_DriveAllowed());tests_passed++;
 in.close_limit=1;sample(1,1);CHECK(!in.limits_invalid && enabled && LimitPower_DriveAllowed());tests_passed++;
#if POWER_LOW_POWER_MODE == 2
 CHECK(!LimitPower_CanSleep());tests_passed++; /* Finish confirmation before untimed Stop. */
#endif
 sample(LIMIT_POWER_CONFIRM_MS,1);
#if POWER_LOW_POWER_MODE != 2
 CHECK(enabled && LimitPower_Valid());tests_passed++;return 0;
#else
 CHECK(!enabled && !LimitPower_Valid() && LimitPower_CanSleep());tests_passed++;
 in.open_limit=in.close_limit=1;sample(1,1);
 CHECK(in.limits_invalid && !in.open_limit && in.close_limit && !enabled);tests_passed++;
 in.wire_break=1;status.safety_close_pending=1;sample(1,0);
 CHECK(enabled && !LimitPower_Valid() && !LimitPower_CanSleep() && limit_power_phase==LIMIT_POWER_SETTLING);tests_passed++;
 in.open_limit=0;in.close_limit=1;sample(LIMIT_POWER_SETTLE_MS,0);
 CHECK(LimitPower_Valid() && enabled && !in.limits_invalid);tests_passed++;
 for(unsigned which=0;which<6;++which)
 {
  reset();sample(LIMIT_POWER_SETTLE_MS,1);
  switch(which) {
   case 0:status.state=VALVE_UNKNOWN;break;
   case 1:status.state=VALVE_CLOSING;break;
   case 2:status.safety_close_pending=1;break;
   case 3:status.latched_faults=VALVE_FAULT_DRIVER;break;
   case 4:status.current_faults=VALVE_FAULT_WIRE;break;
   default:in.close_limit=0;break;
  }
  sample(LIMIT_POWER_CONFIRM_MS,1);CHECK(enabled);tests_passed++;
 }
 reset();sample(LIMIT_POWER_SETTLE_MS,1);idle_safe=0;
 sample(LIMIT_POWER_CONFIRM_MS,1);CHECK(enabled && mask==0);tests_passed++;
 reset();idle_off();generation++;sample(1,1);
 CHECK(enabled && !LimitPower_Valid());tests_passed++;
 reset();idle_off();in.request_open=1;sample(1,0);
 CHECK(enabled && !LimitPower_Valid());tests_passed++;
 reset();idle_off();status.state=VALVE_FAULT;sample(1,1);CHECK(enabled);tests_passed++;
 reset();idle_off();sample(1,0);CHECK(enabled);tests_passed++;
 reset();sample(LIMIT_POWER_SETTLE_MS,1);enabled=0;sample(1,0);
 CHECK(enabled && in.limits_invalid && !LimitPower_Valid());tests_passed++;
 now=UINT32_MAX-50;LimitPower_Init(now);in=(ValveInputs){.close_limit=1};
 sample(LIMIT_POWER_SETTLE_MS,1);CHECK(LimitPower_Valid());
 sample(LIMIT_POWER_CONFIRM_MS,1);CHECK(!enabled);tests_passed++;
 reset();sample(LIMIT_POWER_SETTLE_MS,1);in.close_limit=0;sample(99,1);
 in.close_limit=1;sample(1,1);sample(LIMIT_POWER_CONFIRM_MS-1,1);
 CHECK(enabled);sample(1,1);CHECK(!enabled);tests_passed++;
 reset();idle_off();enabled=1;sample(1,1);
 CHECK(enabled && !LimitPower_Valid() && !LimitPower_CanSleep() && limit_power_phase==LIMIT_POWER_SETTLING);tests_passed++;
 return 0;
#endif
}
