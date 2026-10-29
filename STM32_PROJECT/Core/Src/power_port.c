#include "power_port.h"
#include "power_config.h"
#include "main.h"
#include "valve_port.h"
#include "rs485_rx.h"
#if POWER_LOW_POWER_MODE
extern volatile uint32_t uwTick;
static volatile uint8_t timebase_ready;
static uint32_t tick_origin;
uint32_t HAL_GetTick(void)
{ return timebase_ready ? tick_origin+TIM2->CNT : uwTick; }
#endif
uint8_t PowerPort_Init(void)
{
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
  return (GPIOA->ODR&(nSLEEP_Pin|VREF_Pin))==0 && TIM1->CCR4==0;
}
uint32_t PowerPort_GpioSignature(void)
{
  return (HAL_GPIO_ReadPin(LIMIT4_GPIO_Port,LIMIT4_Pin)==GPIO_PIN_SET?1U:0U) |
         (HAL_GPIO_ReadPin(LIMIT5_GPIO_Port,LIMIT5_Pin)==GPIO_PIN_SET?2U:0U) |
         (HAL_GPIO_ReadPin(_1Y_GPIO_Port,_1Y_Pin)==GPIO_PIN_SET?4U:0U) |
         (HAL_GPIO_ReadPin(_2Y_GPIO_Port,_2Y_Pin)==GPIO_PIN_SET?8U:0U);
}
void PowerPort_AlarmIRQ(void)
{
  TIM2->DIER&=~TIM_DIER_CC1IE;
  TIM2->SR=~TIM_SR_CC1IF;
}
uint8_t PowerPort_Sleep(uint32_t duration)
{
#if POWER_LOW_POWER_MODE
  if(!timebase_ready || !__get_PRIMASK() || !duration || duration>POWER_MAX_SLEEP_MS)return 0;
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
#else
  (void)duration;return 0;
#endif
}
