#include "battery_adc_port.h"
#include "adc.h"
uint16_t BatteryAdc_Calibration(void) { return *VREFINT_CAL_ADDR; }
uint8_t BatteryAdc_Start(void) { return HAL_ADC_Start(&hadc1)==HAL_OK; }
BatteryAdcResult BatteryAdc_Read(uint16_t *value)
{
  /* Check EOC before HAL polling: ordinary "not ready" never sets a HAL
   * timeout bit, and no busy wait is introduced into safety scheduling. */
  if (__HAL_ADC_GET_FLAG(&hadc1,ADC_FLAG_OVR)) return BATTERY_ADC_ERROR;
  if (!__HAL_ADC_GET_FLAG(&hadc1,ADC_FLAG_EOC)) return BATTERY_ADC_WAIT;
  if (HAL_ADC_PollForConversion(&hadc1,0U)!=HAL_OK) return BATTERY_ADC_ERROR;
  *value=(uint16_t)HAL_ADC_GetValue(&hadc1);
  return BATTERY_ADC_READY;
}
uint8_t BatteryAdc_Stop(void) { return HAL_ADC_Stop(&hadc1)==HAL_OK; }
