#include "app_status.h"
static AppStatus snapshot;
void AppStatus_Init(void) { snapshot=(AppStatus){0}; }
void AppStatus_Publish(uint32_t now,const ValveInputs *i,uint8_t l3,uint8_t f,uint8_t s,uint8_t r,uint32_t p)
{
  snapshot.captured_ms=now;
  snapshot.sequence++;
  snapshot.inputs=*i;
  snapshot.valve=*Valve_Get();
  Battery_Get(now,&snapshot.battery);
  snapshot.limit3=l3; snapshot.nfault=f; snapshot.nsleep=s;
  snapshot.vref_enabled=r; snapshot.pwm_percent=p;
}
void AppStatus_Get(uint32_t now, AppStatus *out)
{
  *out=snapshot;
  out->battery.age_ms=out->battery.has_value ? (uint32_t)(now-out->battery.updated_ms) : UINT32_MAX;
  out->battery.valid=out->battery.has_value && out->battery.result==BATTERY_OK &&
                     out->battery.age_ms<=BATTERY_MAX_AGE_MS;
}
