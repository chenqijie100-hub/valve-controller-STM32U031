#include "power_manager.h"
#include "power_port.h"
#define CHECK(x) do { if(!(x)) return __LINE__; } while(0)
unsigned tests_passed;
void *memset(void *p,int v,unsigned n) { unsigned char *s=p;while(n--)*s++=(unsigned char)v;return p; }
static uint8_t ready,pending,safe,isr,inject_lock,inject_sleep;
static uint32_t mask,signature,sleeps,duration;
uint8_t PowerPort_Init(void) { return ready; }
uint32_t PowerPort_Lock(void)
{ uint32_t old=mask; mask=1; if(inject_lock)PowerManager_NotifyGPIOFromISR(); return old; }
void PowerPort_Unlock(uint32_t value) { mask=value; }
uint8_t PowerPort_InInterrupt(void) { return isr; }
uint8_t PowerPort_Pending(void) { return pending; }
uint8_t PowerPort_OutputsSafe(void) { return safe; }
uint32_t PowerPort_GpioSignature(void) { return signature; }
uint8_t PowerPort_Sleep(uint32_t ms)
{ sleeps++; duration=ms; if(inject_sleep)PowerManager_NotifyGPIOFromISR(); return 1; }
static void reset(void)
{
 ready=safe=1; pending=isr=inject_lock=inject_sleep=0;
 mask=signature=sleeps=duration=0; PowerManager_Init();
 PowerManager_MarkServiced(PowerManager_EventGeneration(),0);
}
int run_tests(void)
{
 PowerRequest r={.sleep_ms=250}; reset();
#if !POWER_LOW_POWER_MODE
 CHECK(!PowerManager_TrySleep(&r) && !sleeps && (power_last_blockers&POWER_BLOCK_DEBUG));
 tests_passed++; return 0;
#else
 CHECK(PowerManager_TrySleep(&r) && sleeps==1 && duration==250 && mask==0); tests_passed++;
 for(unsigned which=0;which<7;++which)
 {
  reset(); r=(PowerRequest){.sleep_ms=250};
  switch(which) {
   case 0:r.motor_running=1;break; case 1:r.safety_pending=1;break;
   case 2:r.valve_busy=1;break; case 3:r.adc_busy=1;break;
   case 4:r.uart_busy=1;break; case 5:r.inputs_busy=1;break; default:r.sleep_ms=0;
  }
  CHECK(!PowerManager_TrySleep(&r) && !sleeps); tests_passed++;
 }
 r=(PowerRequest){.sleep_ms=5000};reset();
 CHECK(PowerManager_TrySleep(&r) && duration==POWER_MAX_SLEEP_MS); tests_passed++;
 reset(); r.sleep_ms=100;
 PowerManager_NotifyGPIOFromISR();
 CHECK(!PowerManager_TrySleep(&r) && !sleeps && (power_last_blockers&POWER_BLOCK_EVENT)); tests_passed++;
 /* Event arrives during service after the old generation was sampled. */
 reset();uint32_t observed=PowerManager_EventGeneration();PowerManager_NotifyGPIOFromISR();
 PowerManager_MarkServiced(observed,0);CHECK(!PowerManager_TrySleep(&r));tests_passed++;
 reset();inject_lock=1;CHECK(!PowerManager_TrySleep(&r) && mask==0 && !sleeps);tests_passed++;
 reset();pending=1;CHECK(!PowerManager_TrySleep(&r) && mask==0);tests_passed++;
 reset();signature=1;CHECK(!PowerManager_TrySleep(&r));tests_passed++;
 reset();safe=0;CHECK(!PowerManager_TrySleep(&r));tests_passed++;
 reset();mask=1;CHECK(!PowerManager_TrySleep(&r) && mask==1);tests_passed++;
 reset();isr=1;CHECK(!PowerManager_TrySleep(&r));tests_passed++;
 reset();ready=0;PowerManager_Init();CHECK(!PowerManager_TrySleep(&r));tests_passed++;
 /* Event at WFI boundary wakes; it is NOT marked serviced by sleep logic. */
 reset();inject_sleep=1;CHECK(PowerManager_TrySleep(&r));
 CHECK(!PowerManager_TrySleep(&r) && sleeps==1);
 inject_sleep=0;PowerManager_MarkServiced(PowerManager_EventGeneration(),signature);
 CHECK(PowerManager_TrySleep(&r));tests_passed++;
 ValveInputs in={.wire_break=1,.request_open=1,.open_limit=1,.close_limit=1};
 CHECK(PowerManager_Signature(&in)==15); tests_passed++;
 return 0;
#endif
}
