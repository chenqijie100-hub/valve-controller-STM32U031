#ifndef BATTERY_ADC_PORT_H
#define BATTERY_ADC_PORT_H
#include <stdint.h>
typedef enum { BATTERY_ADC_WAIT, BATTERY_ADC_READY, BATTERY_ADC_ERROR } BatteryAdcResult;
uint16_t BatteryAdc_Calibration(void);
uint8_t BatteryAdc_Start(void);
BatteryAdcResult BatteryAdc_Read(uint16_t *value);
uint8_t BatteryAdc_Stop(void);
/* Stop1-only caller, with IRQs masked and ADC already disabled. No HAL waits. */
uint8_t BatteryAdc_SuspendForStop(void);
void BatteryAdc_ResumeFromStop(void);
/* Nonblocking analog startup qualification; safety service keeps running. */
uint8_t BatteryAdc_Ready(uint32_t now);
#endif
