#include "adc.h"
#include "battery_adc_port.h"
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
unsigned tests_passed;
ADC_HandleTypeDef hadc1;
uint32_t mock_adc_flags;
uint16_t mock_vref_cal=1650;
static HAL_StatusTypeDef result;
static unsigned polls,reads;
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *a) { (void)a; return result; }
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *a) { (void)a; return result; }
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *a,uint32_t timeout)
{ (void)a; if(timeout) return HAL_ERROR; polls++; return result; }
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *a) { (void)a; reads++; return 2787; }
int run_tests(void)
{
  uint16_t value=42;
  CHECK(BatteryAdc_Calibration()==1650); tests_passed++;
  result=HAL_OK; CHECK(BatteryAdc_Start() && BatteryAdc_Stop()); tests_passed++;
  result=HAL_ERROR; CHECK(!BatteryAdc_Start() && !BatteryAdc_Stop()); tests_passed++;
  CHECK(BatteryAdc_Read(&value)==BATTERY_ADC_WAIT && polls==0 && value==42); tests_passed++;
  mock_adc_flags=ADC_FLAG_EOC|ADC_FLAG_OVR;
  CHECK(BatteryAdc_Read(&value)==BATTERY_ADC_ERROR && polls==0 && reads==0); tests_passed++;
  mock_adc_flags=ADC_FLAG_EOC;
  CHECK(BatteryAdc_Read(&value)==BATTERY_ADC_ERROR && reads==0); tests_passed++;
  result=HAL_OK;
  CHECK(BatteryAdc_Read(&value)==BATTERY_ADC_READY && value==2787 && reads==1); tests_passed++;
  return 0;
}
