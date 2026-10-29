#include "power_port.h"
#include "power_config.h"
#include "main.h"
#include "valve_port.h"
#include "rs485_rx.h"
#include "limit_power.h"
#if POWER_LOW_POWER_MODE == 2
#include "power_rtc.h"
#include "battery_adc_port.h"
#endif
volatile uint32_t power_stop_entries,power_stop_last_ms,power_stop_wake_exti;
volatile uint32_t power_stop_faults,power_stop_clock_ok;
static uint8_t stop_woke;
uint8_t PowerPort_TakeStopWake(void)
{ uint8_t woke=stop_woke;stop_woke=0;return woke; }
#if POWER_LOW_POWER_MODE == 2
static uint32_t rtc_fraction;
/* Only the board's already-tested HSI16/div1 clock tree is supported.
 * Fail closed if someone later changes clock sources without adapting Stop. */
static uint8_t stop_clocks_valid(void)
{
  return (RCC->CR&RCC_CR_HSIRDY) &&
      (RCC->CFGR&(RCC_CFGR_SW|RCC_CFGR_SWS|RCC_CFGR_HPRE|RCC_CFGR_PPRE))==
      (RCC_CFGR_SW_0|RCC_CFGR_SWS_0) &&
      (RCC->CCIPR&(RCC_CCIPR_USART1SEL|RCC_CCIPR_ADCSEL|RCC_CCIPR_TIM1SEL))==
      RCC_CCIPR_USART1SEL_1 && /* ADCSEL=0 is SYSCLK on STM32U0. */
      (FLASH->ACR&FLASH_ACR_LATENCY)==FLASH_LATENCY_1;
}
#endif
#if POWER_LOW_POWER_MODE
extern volatile uint32_t uwTick;
static volatile uint8_t timebase_ready;
static uint32_t tick_origin;
uint32_t HAL_GetTick(void)
{ return timebase_ready ? tick_origin+TIM2->CNT : uwTick; }
#endif
uint8_t PowerPort_Init(void)
{
  power_stop_entries=power_stop_last_ms=power_stop_wake_exti=0;
  power_stop_faults=power_stop_clock_ok=0;
  stop_woke=0;
#if POWER_LOW_POWER_MODE
  timebase_ready=0;
  uint32_t frequency=HAL_RCC_GetPCLK1Freq();
  /* Current board has APB divider1; refuse doubled/unknown timer clocks and
   * a timer already owned by another module rather than stealing it. */
  if(frequency<1000U || frequency%1000U || frequency/1000U>65536U ||
     HAL_RCC_GetHCLKFreq()!=frequency || (RCC->APBENR1&RCC_APBENR1_TIM2EN))return 0;
  RCC->APBENR1|=RCC_APBENR1_TIM2EN;
  RCC->APBSMENR1|=RCC_APBSMENR1_TIM2SMEN;
  RCC->APBRSTR1|=RCC_APBRSTR1_TIM2RST;
  RCC->APBRSTR1&=~RCC_APBRSTR1_TIM2RST;
  TIM2->CR1=0;TIM2->DIER=0;TIM2->CCER=0;TIM2->CCMR1=0;
  TIM2->PSC=frequency/1000U-1U;TIM2->ARR=UINT32_MAX;
  TIM2->EGR=TIM_EGR_UG;TIM2->SR=0;TIM2->CNT=0;
  HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
  HAL_NVIC_SetPriority(TIM2_IRQn,2,0);HAL_NVIC_EnableIRQ(TIM2_IRQn);
  tick_origin=uwTick;TIM2->CR1=TIM_CR1_CEN;timebase_ready=1;
#if POWER_LOW_POWER_MODE == 2
  rtc_fraction=0;
  if(!stop_clocks_valid()) { power_stop_faults|=1U;return 0; }
  if(!PowerRtc_Init()) { power_stop_faults|=2U;return 0; }
  /* HSI16 is already SYSCLK before Stop. Hardware selects the same clock on
   * exit; bus dividers, flash latency and peripheral muxes are retained. */
  RCC->CFGR|=RCC_CFGR_STOPWUCK;
  power_stop_clock_ok=1;
#endif
  return 1;
#else
  return 0;
#endif
}
uint32_t PowerPort_Lock(void)
{ uint32_t mask=__get_PRIMASK();__disable_irq();__DSB();return mask; }
void PowerPort_Unlock(uint32_t mask) { __set_PRIMASK(mask);__ISB(); }
uint8_t PowerPort_InInterrupt(void) { return __get_IPSR()!=0; }
uint8_t PowerPort_Pending(void)
{
  return ValvePort_EventPending() || RS485_RxBusy() || NVIC->ISPR[0]!=0 ||
      ((EXTI->RPR1|EXTI->FPR1)&(LIMIT4_Pin|LIMIT5_Pin|LIMIT3_Pin))!=0 ||
      (USART1->ISR&(USART_ISR_RXNE_RXFNE|USART_ISR_ORE|USART_ISR_FE|
                    USART_ISR_NE|USART_ISR_PE|USART_ISR_BUSY))!=0 ||
      (GPIOB->ODR&_485DE_RE_Pin)!=0;
}
uint8_t PowerPort_OutputsSafe(void)
{
  return LimitPower_CanSleep() && (GPIOA->ODR&(nSLEEP_Pin|VREF_Pin))==0 && TIM1->CCR4==0;
}
uint32_t PowerPort_GpioSignature(void)
{
  return (HAL_GPIO_ReadPin(LIMIT4_GPIO_Port,LIMIT4_Pin)==GPIO_PIN_SET?1U:0U) |
         (HAL_GPIO_ReadPin(LIMIT5_GPIO_Port,LIMIT5_Pin)==GPIO_PIN_SET?2U:0U) |
         (!LimitPower_Valid()?16U:
         (HAL_GPIO_ReadPin(_1Y_GPIO_Port,_1Y_Pin)==GPIO_PIN_SET?4U:0U) |
         (HAL_GPIO_ReadPin(_2Y_GPIO_Port,_2Y_Pin)==GPIO_PIN_SET?8U:0U));
}
void PowerPort_AlarmIRQ(void)
{
  TIM2->DIER&=~TIM_DIER_CC1IE;
  TIM2->SR=~TIM_SR_CC1IF;
}
#if POWER_LOW_POWER_MODE == 2
static uint8_t stop1(void)
{
  uint64_t before,after;
  uint32_t uart_cr1=USART1->CR1,signature=PowerPort_GpioSignature();
  if(!stop_clocks_valid()) { power_stop_faults|=1U;return 0; }
  if((ADC1->CR&(ADC_CR_ADSTART|ADC_CR_ADSTP|ADC_CR_ADDIS|ADC_CR_ADCAL)) ||
     !(USART1->ISR&USART_ISR_TC) || PowerPort_Pending() ||
     EXTI->RPR1 || EXTI->FPR1 || !PowerPort_OutputsSafe())return 0;
  if(!BatteryAdc_SuspendForStop())return 0;
  /* Stop the active timer BEFORE the RTC interval. No HAL wait/delay may run
   * until that interval has been accounted for and TIM2 restarted below. */
  TIM2->DIER=0;TIM2->CR1&=~TIM_CR1_CEN;
  if(!PowerRtc_Now(&before))
  {
    TIM2->CR1|=TIM_CR1_CEN;power_stop_faults|=2U;
    BatteryAdc_ResumeFromStop();return 0;
  }
  USART1->CR1=uart_cr1&~(USART_CR1_UE|USART_CR1_UESM);
  HAL_SuspendTick();SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk;
  __DSB();__ISB();
  uint8_t entered=0;
  if(!PowerPort_Pending() && !EXTI->RPR1 && !EXTI->FPR1 &&
     PowerPort_GpioSignature()==signature && PowerPort_OutputsSafe())
  {
    /* PRIMASK spans last check/WFI/restore. Pending EXTI is never cleared here.
     * Enabled IRQ arriving at WFI completes it, even while PRIMASK is set. */
    HAL_PWREx_EnterSTOP1Mode(PWR_STOPENTRY_WFI);entered=1;
  }
  power_stop_clock_ok=0;
  /* Hardware normally already restored HSI16. Bound checks by iteration count,
   * never by HAL_GetTick (TIM2 is stopped). Do not re-run full clock startup. */
  unsigned retry=10000U;
  while(!stop_clocks_valid() && --retry) { __DSB(); }
  if(!retry) { power_stop_faults|=4U;Error_Handler();return 0; }
  if(!PowerRtc_Now(&after) || after<before)
  { power_stop_faults|=2U;Error_Handler();return 0; }
  uint64_t elapsed=(after-before)*1000U+rtc_fraction;
  uint32_t ms=(uint32_t)(elapsed/256U);
  rtc_fraction=(uint32_t)(elapsed%256U);tick_origin+=ms;
  TIM2->CR1|=TIM_CR1_CEN; /* Live timebase before any HAL operation. */
  SystemCoreClockUpdate();
  if(SystemCoreClock!=16000000U || HAL_RCC_GetHCLKFreq()!=16000000U ||
     HAL_RCC_GetPCLK1Freq()!=16000000U || HAL_InitTick(uwTickPrio)!=HAL_OK)
  { power_stop_faults|=8U;Error_Handler();return 0; }
  power_stop_clock_ok=1;
  if(entered)
  {
    stop_woke=1;
    power_stop_entries++;power_stop_last_ms=ms;
    power_stop_wake_exti=EXTI->RPR1|EXTI->FPR1;
  }
  /* UE was disabled even if a late event aborted WFI. Re-sync both paths. */
  RS485_RxAfterStop();
  USART1->RQR=USART_RQR_RXFRQ;
  USART1->ICR=USART_ISR_ORE|USART_ISR_FE|USART_ISR_NE|USART_ISR_PE;
  USART1->CR1=uart_cr1; /* Same BRR/HSI clock; do not touch GPIO or DE. */
  BatteryAdc_ResumeFromStop(); /* No waiting; main keeps servicing safety first. */
  HAL_ResumeTick();
  return entered;
}
#endif
uint8_t PowerPort_Sleep(uint32_t duration)
{
#if POWER_LOW_POWER_MODE
  if(!timebase_ready || !__get_PRIMASK() || !duration || duration>POWER_MAX_SLEEP_MS)return 0;
#if POWER_LOW_POWER_MODE == 2
  return stop1(); /* No timed wake; duration only proves background ADC idle. */
#else
  TIM2->DIER&=~TIM_DIER_CC1IE;
  /* Clear stale flags BEFORE setting the new deadline, never after it. */
  TIM2->SR=~TIM_SR_CC1IF;HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
  uint32_t started=TIM2->CNT;
  TIM2->CCR1=started+duration;TIM2->DIER|=TIM_DIER_CC1IE;
  HAL_SuspendTick();
  /* HAL time uses TIM2, so clearing a pending SysTick loses no elapsed time. */
  SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk;
  __DSB();__ISB();
  uint8_t entered=0;
  if(!PowerPort_Pending() && (uint32_t)(TIM2->CNT-started)<duration)
  {
    /* PRIMASK stays set: an enabled interrupt becoming pending completes WFI,
     * then the caller restores PRIMASK after tick/compare cleanup. */
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON,PWR_SLEEPENTRY_WFI);
    entered=1;
  }
  PowerPort_AlarmIRQ();HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
  HAL_ResumeTick(); /* Before IRQs/main/HAL waits can execute again. */
  return entered;
#endif
#else
  (void)duration;return 0;
#endif
}
