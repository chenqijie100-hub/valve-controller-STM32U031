#include "limit_power.h"
#include "power_port.h"
volatile uint32_t limit_power_phase,limit_power_off_count,limit_power_enable_count;
static uint32_t enable_tick,pair_tick,off_generation;
static uint8_t last_open,last_close,pair,have_pair,off_request,off_wire,confirm_pending;
static void enable(uint32_t now)
{
  limit_power_phase=LIMIT_POWER_SETTLING;enable_tick=now;have_pair=confirm_pending=0;
  LimitPowerPort_SetEnabled(1);limit_power_enable_count++;
}
void LimitPower_Init(uint32_t now)
{
  last_open=last_close=pair=have_pair=off_request=off_wire=0;
  off_generation=limit_power_off_count=limit_power_enable_count=0;
  pair_tick=now;enable(now);
}
uint8_t LimitPower_Valid(void)
{ return limit_power_phase==LIMIT_POWER_READY && LimitPowerPort_IsEnabled(); }
uint8_t LimitPower_DriveAllowed(void) { return LimitPower_Valid(); }
uint8_t LimitPower_CanSleep(void)
{
  return (LimitPower_Valid() && !confirm_pending) ||
      (limit_power_phase==LIMIT_POWER_OFF && !LimitPowerPort_IsEnabled());
}
void LimitPower_Filter(uint32_t now,ValveInputs *in)
{
  if(limit_power_phase==LIMIT_POWER_SETTLING && LimitPowerPort_IsEnabled() &&
     (uint32_t)(now-enable_tick)>=LIMIT_POWER_SETTLE_MS)
  { limit_power_phase=LIMIT_POWER_READY;have_pair=0; }
  in->limits_invalid=!LimitPower_Valid();
  if(in->limits_invalid)
  {
    /* Cache is explicitly invalid: it may describe only the last observation,
     * never a newly reached destination or a live pair of limit inputs. */
    in->open_limit=last_open;in->close_limit=last_close;return;
  }
  uint8_t observed=(uint8_t)(in->open_limit|(in->close_limit<<1));
  if(!have_pair || observed!=pair) { have_pair=1;pair=observed;pair_tick=now; }
  last_open=in->open_limit;last_close=in->close_limit;
}
void LimitPower_Service(uint32_t now,const ValveInputs *in,const ValveStatus *s,
                        uint8_t idle,uint32_t generation)
{
  if(limit_power_phase==LIMIT_POWER_UNINITIALIZED)
  { enable(now);return; }
  if(limit_power_phase!=LIMIT_POWER_OFF)
  {
    if(!LimitPowerPort_IsEnabled()) { enable(now);return; }
  }
#if POWER_LOW_POWER_MODE == 2
  if(limit_power_phase==LIMIT_POWER_OFF)
  {
    /* L4 has already been processed by Valve_Step before enabling this domain.
     * A generation change also catches a short pulse whose level recovered. */
    if(LimitPowerPort_IsEnabled() || in->break_event || s->safety_close_pending || !idle ||
       s->current_faults || s->latched_faults ||
       (s->state!=VALVE_OPENED && s->state!=VALVE_CLOSED) ||
       in->request_open!=off_request || in->wire_break!=off_wire || generation!=off_generation ||
       !LimitPowerPort_IdleSafe(in,generation))enable(now);
    return;
  }
  uint8_t endpoint=(s->state==VALVE_OPENED && in->open_limit && !in->close_limit) ||
                   (s->state==VALVE_CLOSED && in->close_limit && !in->open_limit);
  confirm_pending=0;
  if(!LimitPower_Valid() || in->limits_invalid || !have_pair || !idle || !endpoint ||
     s->safety_close_pending || s->current_faults || s->latched_faults)return;
  if((uint32_t)(now-pair_tick)<LIMIT_POWER_CONFIRM_MS)
  { confirm_pending=1;return; } /* No RTC wake: finish this timed task before Stop. */
  uint32_t mask=PowerPort_Lock();
  if(!mask && LimitPowerPort_IdleSafe(in,generation))
  {
    off_generation=generation;off_request=in->request_open;off_wire=in->wire_break;
    /* Invalidate BEFORE disconnecting the pull-ups. Never clear EXTI here. */
    limit_power_phase=LIMIT_POWER_OFF;
    LimitPowerPort_SetEnabled(0);limit_power_off_count++;
  }
  PowerPort_Unlock(mask);
#else
  (void)in;(void)s;(void)idle;(void)generation;
#endif
}
