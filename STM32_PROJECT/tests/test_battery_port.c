#include "adc.h"
#include "battery_adc_port.h"
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
unsigned tests_passed;
ADC_HandleTypeDef hadc1;
MockADC mock_adc;
MockADCCommon mock_adc_common;
uint32_t mock_adc_flags;
uint16_t mock_vref_cal=1650;
static HAL_StatusTypeDef result;
static unsigned polls,reads,starts;
static uint32_t tick;
uint32_t HAL_GetTick(void) { return tick; }
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *a) { (void)a;starts++;return result; }
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
  const uint32_t paths=ADC_CCR_VREFEN|ADC_CCR_TSEN|ADC_CCR_VBATEN;
  ADC1->CR=ADC_CR_ADVREGEN|ADC_CR_ADEN;ADC1_COMMON->CCR=paths|123U;
  CHECK(!BatteryAdc_SuspendForStop() && (ADC1->CR&ADC_CR_ADVREGEN));tests_passed++;
  ADC1->CR=ADC_CR_ADVREGEN;ADC1->CALFACT=91;
  CHECK(BatteryAdc_SuspendForStop());
  CHECK(!(ADC1->CR&ADC_CR_ADVREGEN) && ADC1_COMMON->CCR==123U && ADC1->CALFACT==91);tests_passed++;
  CHECK(!BatteryAdc_SuspendForStop()); /* Never overwrite the saved state. */
  BatteryAdc_ResumeFromStop();
  CHECK((ADC1->CR&ADC_CR_ADVREGEN) && ADC1_COMMON->CCR==(paths|123U) && ADC1->CALFACT==91);tests_passed++;
  unsigned before=starts;
  CHECK(!BatteryAdc_Ready(tick) && !BatteryAdc_Start() && starts==before);tests_passed++;
  tick++;CHECK(!BatteryAdc_Ready(tick));tick++;
  CHECK(BatteryAdc_Ready(tick) && BatteryAdc_Start());tests_passed++;
  ADC1->CR=0;ADC1_COMMON->CCR=123U;
  CHECK(BatteryAdc_SuspendForStop());BatteryAdc_ResumeFromStop();
  CHECK(ADC1->CR==0 && ADC1_COMMON->CCR==123U && BatteryAdc_Ready(tick));tests_passed++;
  ADC1->CR=ADC_CR_ADVREGEN;ADC1_COMMON->CCR=ADC_CCR_VREFEN;
  tick=UINT32_MAX;CHECK(BatteryAdc_SuspendForStop());BatteryAdc_ResumeFromStop();
  tick=0;CHECK(!BatteryAdc_Ready(tick));tick=1;CHECK(BatteryAdc_Ready(tick));tests_passed++;
  for(unsigned i=0;i<4;++i)
  {
    const uint32_t busy[]={ADC_CR_ADSTART,ADC_CR_ADSTP,ADC_CR_ADDIS,ADC_CR_ADCAL};
    ADC1->CR=ADC_CR_ADVREGEN|busy[i];CHECK(!BatteryAdc_SuspendForStop());tests_passed++;
  }
  return 0;
}
