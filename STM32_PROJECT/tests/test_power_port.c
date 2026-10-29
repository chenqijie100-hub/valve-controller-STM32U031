#include "main.h"
#include "power_port.h"
#include "power_manager.h"
static uint8_t limits_valid=1,limits_sleep=1;
uint8_t LimitPower_Valid(void) { return limits_valid; }
uint8_t LimitPower_CanSleep(void) { return limits_sleep; }
#define CHECK(x) do { if(!(x))return __LINE__; } while(0)
unsigned tests_passed;
GPIO_TypeDef mock_gpioa,mock_gpiob;
TIM_TypeDef mock_tim1,mock_tim2;
RCC_TypeDef mock_rcc;
EXTI_TypeDef mock_exti;
MockUSART mock_usart1;
MockNVIC mock_nvic;
MockSCB mock_scb;
volatile uint32_t uwTick;
static uint32_t mask,ipsr,freq=16000000,sleeps,suspended,resumed,elapsed;
static uint8_t rx_busy,wire_pending,inject_at_suspend,expire_at_suspend,wire_at_wfi;
void *memset(void *p,int v,unsigned n) { unsigned char *s=p;while(n--)*s++=(unsigned char)v;return p; }
uint32_t __get_PRIMASK(void) { return mask; }
void __disable_irq(void) { mask=1; }
void __set_PRIMASK(uint32_t v) { mask=v; }
uint32_t __get_IPSR(void) { return ipsr; }
void __DSB(void) { }
void __ISB(void) { }
uint32_t HAL_RCC_GetHCLKFreq(void) { return freq; }
uint32_t HAL_RCC_GetPCLK1Freq(void) { return freq; }
void HAL_NVIC_SetPriority(int irq,uint32_t p,uint32_t s) { (void)irq;(void)p;(void)s; }
void HAL_NVIC_EnableIRQ(int irq) { (void)irq; }
void HAL_NVIC_ClearPendingIRQ(int irq) { NVIC->ISPR[0]&=~(1UL<<irq); }
uint8_t RS485_RxBusy(void) { return rx_busy; }
uint8_t ValvePort_EventPending(void) { return wire_pending; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p,uint32_t pin)
{ return p->IDR&pin?GPIO_PIN_SET:GPIO_PIN_RESET; }
void HAL_SuspendTick(void)
{
 suspended++;
 if(inject_at_suspend) { EXTI->RPR1|=LIMIT4_Pin; GPIOA->IDR|=LIMIT4_Pin; }
 if(expire_at_suspend)TIM2->CNT+=300;
}
void HAL_ResumeTick(void) { resumed++; }
void HAL_PWR_EnterSLEEPMode(uint32_t regulator,uint8_t entry)
{
 if(regulator==PWR_MAINREGULATOR_ON && entry==PWR_SLEEPENTRY_WFI && mask) sleeps++;
 if(wire_at_wfi) { EXTI->RPR1|=LIMIT4_Pin;GPIOA->IDR|=LIMIT4_Pin; }
 TIM2->CNT+=elapsed;
}
static void reset_hardware(void)
{
 mock_rcc=(RCC_TypeDef){0}; mock_gpioa=(GPIO_TypeDef){0};mock_gpiob=(GPIO_TypeDef){0};
 mock_tim1=(TIM_TypeDef){0};mock_tim2=(TIM_TypeDef){0};mock_exti=(EXTI_TypeDef){0};
 mock_usart1=(MockUSART){0};mock_nvic=(MockNVIC){0};
 mask=ipsr=sleeps=suspended=resumed=0;
 rx_busy=wire_pending=inject_at_suspend=expire_at_suspend=wire_at_wfi=0;
 elapsed=100;freq=16000000;
}
int run_tests(void)
{
 reset_hardware();uwTick=123;
 CHECK(HAL_GetTick()==123); CHECK(PowerPort_Init());
 CHECK(TIM2->PSC==15999 && TIM2->ARR==UINT32_MAX && TIM2->CR1==TIM_CR1_CEN);
 CHECK(RCC->APBSMENR1&RCC_APBSMENR1_TIM2SMEN);tests_passed++;
 TIM2->CNT=250;CHECK(HAL_GetTick()==373);tests_passed++;
 uint32_t old=PowerPort_Lock();CHECK(old==0 && mask==1);PowerPort_Unlock(old);CHECK(!mask);tests_passed++;
 ipsr=1;CHECK(PowerPort_InInterrupt());ipsr=0;CHECK(!PowerPort_InInterrupt());tests_passed++;
 CHECK(!PowerPort_Pending());
 rx_busy=1;CHECK(PowerPort_Pending());rx_busy=0;
 wire_pending=1;CHECK(PowerPort_Pending());wire_pending=0;
 EXTI->FPR1=LIMIT5_Pin;CHECK(PowerPort_Pending());EXTI->FPR1=0;
 NVIC->ISPR[0]=1U<<10;CHECK(PowerPort_Pending());NVIC->ISPR[0]=0;
 USART1->ISR=USART_ISR_RXNE_RXFNE;CHECK(PowerPort_Pending());USART1->ISR=0;tests_passed++;
 GPIOA->IDR=LIMIT4_Pin|_1Y_Pin;GPIOB->IDR=LIMIT5_Pin|_2Y_Pin;
 CHECK(PowerPort_GpioSignature()==15);GPIOA->IDR=GPIOB->IDR=0;tests_passed++;
 CHECK(PowerPort_OutputsSafe());GPIOA->ODR=nSLEEP_Pin;CHECK(!PowerPort_OutputsSafe());
 GPIOA->ODR=VREF_Pin;CHECK(!PowerPort_OutputsSafe());GPIOA->ODR=0;TIM1->CCR4=400;
 CHECK(!PowerPort_OutputsSafe());TIM1->CCR4=0;tests_passed++;
 limits_valid=0;GPIOA->IDR=_1Y_Pin;GPIOB->IDR=_2Y_Pin;
 CHECK(PowerPort_GpioSignature()==16);limits_sleep=0;CHECK(!PowerPort_OutputsSafe());
 limits_valid=limits_sleep=1;GPIOA->IDR=GPIOB->IDR=0;tests_passed++;
 mask=1;uint32_t before=HAL_GetTick();
 CHECK(PowerPort_Sleep(250) && sleeps==1 && suspended==1 && resumed==1 && mask==1);
 CHECK(HAL_GetTick()-before==100 && !(TIM2->DIER&TIM_DIER_CC1IE));tests_passed++;
 TIM2->CNT=UINT32_MAX-20;before=HAL_GetTick();elapsed=50;
 CHECK(PowerPort_Sleep(100));CHECK((uint32_t)(HAL_GetTick()-before)==50);tests_passed++;
 inject_at_suspend=1;before=sleeps;
 CHECK(!PowerPort_Sleep(250) && sleeps==before && resumed==suspended);tests_passed++;
 CHECK(EXTI->RPR1&LIMIT4_Pin); /* Abort never consumes a safety event. */
 inject_at_suspend=0;EXTI->RPR1=0;expire_at_suspend=1;before=sleeps;
 CHECK(!PowerPort_Sleep(100) && sleeps==before && resumed==suspended);tests_passed++;
 expire_at_suspend=0;wire_at_wfi=1;before=sleeps;
 CHECK(PowerPort_Sleep(100) && sleeps==before+1 && resumed==suspended);
 CHECK(EXTI->RPR1&LIMIT4_Pin);tests_passed++;
 mask=0;before=suspended;CHECK(!PowerPort_Sleep(100) && suspended==before);tests_passed++;
 TIM2->SR=TIM_SR_CC1IF;TIM2->DIER=TIM_DIER_CC1IE;PowerPort_AlarmIRQ();
 CHECK(!(TIM2->DIER&TIM_DIER_CC1IE));tests_passed++;
 reset_hardware();freq=1234567;uwTick=555;CHECK(!PowerPort_Init() && HAL_GetTick()==555);tests_passed++;
 reset_hardware();RCC->APBENR1=RCC_APBENR1_TIM2EN;
 CHECK(!PowerPort_Init() && HAL_GetTick()==555);tests_passed++;
 return 0;
}
