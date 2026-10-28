#include "app_status.h"
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
unsigned tests_passed;
void *memset(void *p,int v,unsigned n) { unsigned char *s=p; while(n--) *s++=(unsigned char)v; return p; }
void *memcpy(void *p,const void *v,unsigned n)
{ unsigned char *d=p; const unsigned char *s=v; while(n--) *d++=*s++; return p; }
static ValveStatus valve;
static BatteryStatus battery;
const ValveStatus *Valve_Get(void) { return &valve; }
void Battery_Get(uint32_t now,BatteryStatus *out) { (void)now; *out=battery; }
int run_tests(void)
{
  AppStatus out;
  AppStatus_Init(); AppStatus_Get(0,&out); CHECK(!out.battery.valid && out.sequence==0); tests_passed++;
  ValveInputs inputs={.open_limit=1,.request_open=1,.break_event=1};
  valve=(ValveStatus){.state=VALVE_CLOSING,.current_faults=VALVE_FAULT_WIRE,
                     .history_faults=VALVE_FAULT_WIRE,.safety_close_pending=1};
  battery=(BatteryStatus){.millivolts=7200,.updated_ms=100,.has_value=1,.result=BATTERY_OK};
  AppStatus_Publish(150,&inputs,1,1,1,1,100);
  AppStatus_Get(160,&out);
  CHECK(out.sequence==1 && out.captured_ms==150 && out.battery.age_ms==60);
  CHECK(out.battery.valid && out.battery.millivolts==7200);
  CHECK(out.valve.state==VALVE_CLOSING && out.inputs.open_limit && out.inputs.break_event);
  CHECK(out.inputs.wire_break==0 && out.valve.current_faults==VALVE_FAULT_WIRE);
  CHECK(out.limit3 && out.nfault && out.nsleep && out.vref_enabled && out.pwm_percent==100); tests_passed++;
  out.valve.current_faults=0; out.battery.millivolts=9999; inputs.open_limit=0;
  valve.current_faults=0; battery.millivolts=6000;
  AppStatus_Get(200,&out);
  CHECK(out.valve.current_faults==VALVE_FAULT_WIRE && out.battery.millivolts==7200 && out.inputs.open_limit);
  CHECK(out.inputs.break_event==1); tests_passed++;
  AppStatus_Get(1101,&out); CHECK(!out.battery.valid && out.battery.has_value); tests_passed++;
  battery.updated_ms=0xfffffff0U; battery.millivolts=7000;
  AppStatus_Publish(0xfffffff0U,&inputs,0,1,0,0,0);
  AppStatus_Get(0x10,&out); CHECK(out.battery.valid && out.battery.age_ms==32); tests_passed++;
  return 0;
}
