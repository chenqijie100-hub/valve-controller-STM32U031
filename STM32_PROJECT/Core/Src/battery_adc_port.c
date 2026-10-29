#include "battery_adc_port.h"
#include "adc.h"
static uint32_t saved_regulator,saved_paths,resume_tick;
static uint8_t suspended,settling;
#define INTERNAL_PATHS (ADC_CCR_VREFEN|ADC_CCR_TSEN|ADC_CCR_VBATEN)
/* HAL tick resolution is1ms. Two tick boundaries guarantee >=1ms, exceeding
 * U0 LL's20us ADC regulator /12us VREFINT /120us temperature startup times.
 * Never busy-wait with IRQs masked, and do not start conversion timeouts yet. */
#define ANALOG_SETTLE_TICKS 2U
uint8_t BatteryAdc_Ready(uint32_t now)
{
  if(suspended || (settling && (uint32_t)(now-resume_tick)<ANALOG_SETTLE_TICKS))return 0;
  settling=0;return 1;
}
uint8_t BatteryAdc_SuspendForStop(void)
{
  if(suspended || !BatteryAdc_Ready(HAL_GetTick()) ||
     (ADC1->CR&(ADC_CR_ADEN|ADC_CR_ADSTART|ADC_CR_ADSTP|ADC_CR_ADDIS|ADC_CR_ADCAL)))return 0;
  saved_regulator=ADC1->CR&ADC_CR_ADVREGEN;
  saved_paths=ADC1_COMMON->CCR&INTERNAL_PATHS;
  suspended=1;
  ADC1_COMMON->CCR&=~INTERNAL_PATHS;
  ADC1->CR&=~ADC_CR_ADVREGEN;
  /* No peripheral reset: U0 retains internal calibration with ADVREGEN=0. */
  return 1;
}
void BatteryAdc_ResumeFromStop(void)
{
  if(!suspended)return;
  /* Called after clocks/TIM2 are live, including aborted Stop entries. */
  ADC1->CR=(ADC1->CR&~ADC_CR_ADVREGEN)|saved_regulator;
  ADC1_COMMON->CCR=(ADC1_COMMON->CCR&~INTERNAL_PATHS)|saved_paths;
  resume_tick=HAL_GetTick();settling=(saved_regulator||saved_paths)!=0;
  suspended=0;
}
uint16_t BatteryAdc_Calibration(void) { return *VREFINT_CAL_ADDR; }
uint8_t BatteryAdc_Start(void)
{ return BatteryAdc_Ready(HAL_GetTick()) && HAL_ADC_Start(&hadc1)==HAL_OK; }
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
