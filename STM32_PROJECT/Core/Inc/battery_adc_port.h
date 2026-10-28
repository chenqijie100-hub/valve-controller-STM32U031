#ifndef BATTERY_ADC_PORT_H
#define BATTERY_ADC_PORT_H
#include <stdint.h>
typedef enum { BATTERY_ADC_WAIT, BATTERY_ADC_READY, BATTERY_ADC_ERROR } BatteryAdcResult;
uint16_t BatteryAdc_Calibration(void);
uint8_t BatteryAdc_Start(void);
BatteryAdcResult BatteryAdc_Read(uint16_t *value);
uint8_t BatteryAdc_Stop(void);
#endif
