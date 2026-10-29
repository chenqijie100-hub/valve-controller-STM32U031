#ifndef LIMIT_POWER_H
#define LIMIT_POWER_H
#include "valve_control.h"
#include "power_config.h"
/* Conservative board-test starting values, not final measured RC limits. */
#define LIMIT_POWER_SETTLE_MS 100U
#define LIMIT_POWER_CONFIRM_MS 100U
typedef enum { LIMIT_POWER_UNINITIALIZED, LIMIT_POWER_SETTLING,
               LIMIT_POWER_READY, LIMIT_POWER_OFF } LimitPowerPhase;
extern volatile uint32_t limit_power_phase,limit_power_off_count,limit_power_enable_count;
void LimitPower_Init(uint32_t now);
void LimitPower_Filter(uint32_t now,ValveInputs *inputs);
void LimitPower_Service(uint32_t now,const ValveInputs *inputs,
                        const ValveStatus *status,uint8_t valve_idle,uint32_t generation);
uint8_t LimitPower_Valid(void);
uint8_t LimitPower_CanSleep(void);
uint8_t LimitPower_DriveAllowed(void);
/* Board hooks. Service owns output changes; motor code only checks readiness. */
void LimitPowerPort_SetEnabled(uint8_t enabled);
uint8_t LimitPowerPort_IsEnabled(void);
uint8_t LimitPowerPort_IdleSafe(const ValveInputs *inputs,uint32_t generation);
#endif
