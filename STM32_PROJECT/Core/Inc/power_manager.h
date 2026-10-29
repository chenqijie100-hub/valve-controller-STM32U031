#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H
#include <stdint.h>
#include "power_config.h"
#include "valve_control.h"
enum {
  POWER_BLOCK_DEBUG=1U, POWER_BLOCK_MOTOR=2U, POWER_BLOCK_SAFETY=4U,
  POWER_BLOCK_VALVE=8U, POWER_BLOCK_ADC=16U, POWER_BLOCK_UART=32U,
  POWER_BLOCK_INPUT=64U, POWER_BLOCK_EVENT=128U, POWER_BLOCK_OUTPUT=256U,
  POWER_BLOCK_PORT=512U, POWER_BLOCK_CONTEXT=1024U
};
typedef struct {
  uint8_t motor_running, safety_pending, valve_busy, adc_busy, uart_busy, inputs_busy;
  uint32_t sleep_ms;
} PowerRequest;
void PowerManager_Init(void);
void PowerManager_NotifyGPIOFromISR(void);
uint32_t PowerManager_EventGeneration(void);
uint32_t PowerManager_Signature(const ValveInputs *inputs);
void PowerManager_MarkServiced(uint32_t generation, uint32_t signature);
/* Main-loop idle hook only; never call inside HAL waits or IRQ handlers. */
uint8_t PowerManager_TrySleep(const PowerRequest *request);
extern volatile uint32_t power_sleep_entries, power_sleep_aborts, power_last_blockers;
#endif
