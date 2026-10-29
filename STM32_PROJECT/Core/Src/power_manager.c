#include "power_manager.h"
#include "power_port.h"
volatile uint32_t power_sleep_entries, power_sleep_aborts, power_last_blockers;
static volatile uint32_t generation;
static uint32_t processed_generation,serviced_signature;
#if POWER_LOW_POWER_MODE
static uint8_t port_ready;
#endif
void PowerManager_Init(void)
{
  generation=0;processed_generation=UINT32_MAX;serviced_signature=0;
  power_sleep_entries=power_sleep_aborts=power_last_blockers=0;
#if POWER_LOW_POWER_MODE
  port_ready=PowerPort_Init();
#endif
}
void PowerManager_NotifyGPIOFromISR(void) { generation++; }
uint32_t PowerManager_EventGeneration(void) { return generation; }
uint32_t PowerManager_Signature(const ValveInputs *i)
{ return (uint32_t)i->wire_break | ((uint32_t)i->request_open<<1) |
         (i->limits_invalid?16U:((uint32_t)i->open_limit<<2)|((uint32_t)i->close_limit<<3)); }
void PowerManager_MarkServiced(uint32_t g,uint32_t s)
{ processed_generation=g;serviced_signature=s; }
uint8_t PowerManager_TrySleep(const PowerRequest *r)
{
#if !POWER_LOW_POWER_MODE
  (void)r;power_last_blockers=POWER_BLOCK_DEBUG;return 0;
#else
  uint32_t blockers=0;
  if(!port_ready)blockers|=POWER_BLOCK_PORT;
  if(r->motor_running)blockers|=POWER_BLOCK_MOTOR;
  if(r->safety_pending)blockers|=POWER_BLOCK_SAFETY;
  if(r->valve_busy)blockers|=POWER_BLOCK_VALVE;
  if(r->adc_busy || !r->sleep_ms)blockers|=POWER_BLOCK_ADC;
  if(r->uart_busy)blockers|=POWER_BLOCK_UART;
  if(r->inputs_busy)blockers|=POWER_BLOCK_INPUT;
  if(PowerPort_InInterrupt())blockers|=POWER_BLOCK_CONTEXT;
  power_last_blockers=blockers;
  if(blockers)return 0;
  uint32_t mask=PowerPort_Lock();
  if(mask)blockers|=POWER_BLOCK_CONTEXT;
  if(generation!=processed_generation || PowerPort_Pending() ||
     PowerPort_GpioSignature()!=serviced_signature)blockers|=POWER_BLOCK_EVENT;
  if(!PowerPort_OutputsSafe())blockers|=POWER_BLOCK_OUTPUT;
  if(!blockers)
  {
    uint32_t duration=r->sleep_ms<POWER_MAX_SLEEP_MS?r->sleep_ms:POWER_MAX_SLEEP_MS;
    if(PowerPort_Sleep(duration))power_sleep_entries++;
    else { blockers|=POWER_BLOCK_EVENT;power_sleep_aborts++; }
  }
  else power_sleep_aborts++;
  power_last_blockers=blockers;
  PowerPort_Unlock(mask);
  return blockers==0;
#endif
}
