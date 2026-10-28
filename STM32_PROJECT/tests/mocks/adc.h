#ifndef MOCK_ADC_H
#define MOCK_ADC_H
#include "main.h"
typedef struct { uint32_t unused; } ADC_HandleTypeDef;
extern ADC_HandleTypeDef hadc1;
void MX_ADC1_Init(void);
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *a);
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *a);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *a);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *a, uint32_t timeout);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *a);
extern uint32_t mock_adc_flags;
extern uint16_t mock_vref_cal;
#define ADC_FLAG_EOC 1U
#define ADC_FLAG_OVR 2U
#define __HAL_ADC_GET_FLAG(a,f) ((void)(a), (mock_adc_flags & (f)) != 0)
#define VREFINT_CAL_ADDR (&mock_vref_cal)
#endif
