#ifndef APP_STATUS_H
#define APP_STATUS_H
#include "valve_control.h"
#include "battery_monitor.h"
typedef struct {
  uint32_t captured_ms, sequence, pwm_percent;
  ValveStatus valve;
  ValveInputs inputs;
  BatteryStatus battery;
  uint8_t limit3, nfault, nsleep, vref_enabled;
} AppStatus;
/* Main context only. Publish after safety/ADC service; getter copies values
 * and never consumes EXTI events, reads GPIO or changes safety state. */
void AppStatus_Init(void);
void AppStatus_Publish(uint32_t now, const ValveInputs *inputs, uint8_t limit3,
                       uint8_t nfault, uint8_t nsleep, uint8_t vref, uint32_t pwm);
void AppStatus_Get(uint32_t now, AppStatus *out);
#endif
