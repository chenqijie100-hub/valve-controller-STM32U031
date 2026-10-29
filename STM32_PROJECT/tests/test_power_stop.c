#include "main.h"
#include "power_port.h"
#include "power_rtc.h"
#include "rs485_rx.h"
#define CHECK(x) do { if(!(x))return __LINE__; } while(0)
unsigned tests_passed;
GPIO_TypeDef mock_gpioa,mock_gpiob;
TIM_TypeDef mock_tim1,mock_tim2;
RCC_TypeDef mock_rcc;
EXTI_TypeDef mock_exti;
MockUSART mock_usart1;
MockNVIC mock_nvic;
MockSCB mock_scb;
MockADC mock_adc;
MockFLASH mock_flash;
volatile uint32_t uwTick;
uint32_t SystemCoreClock=16000000;
uint32_t uwTickPrio=3;
static uint32_t mask,stops,resumed,suspended,errors,resync;
static uint64_t rtc_stamp,stop_ticks;
static uint32_t preparation_ticks,restoration_ticks;
static uint8_t rtc_ok,rtc_backwards,bad_clock,bad_tick,inject,busy,wire_wfi;
void *memset(void *p,int v,unsigned n) { unsigned char *s=p;while(n--)*s++=(unsigned char)v;return p; }
uint32_t __get_PRIMASK(void) { return mask; }
void __disable_irq(void) { mask=1; }
void __set_PRIMASK(uint32_t v) { mask=v; }
uint32_t __get_IPSR(void) { return 0; }
void __DSB(void) { }
void __ISB(void) { }
uint32_t HAL_RCC_GetHCLKFreq(void) { return SystemCoreClock; }
uint32_t HAL_RCC_GetPCLK1Freq(void) { return SystemCoreClock; }
void HAL_NVIC_SetPriority(int irq,uint32_t p,uint32_t s) { (void)irq;(void)p;(void)s; }
void HAL_NVIC_EnableIRQ(int irq) { (void)irq; }
void HAL_NVIC_ClearPendingIRQ(int irq) { NVIC->ISPR[0]&=~(1UL<<irq); }
uint8_t RS485_RxBusy(void) { return busy; }
void RS485_RxAfterStop(void) { resync++; }
uint8_t ValvePort_EventPending(void) { return 0; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p,uint32_t pin) { return p->IDR&pin?GPIO_PIN_SET:GPIO_PIN_RESET; }
uint8_t PowerRtc_Init(void) { return rtc_ok; }
uint8_t PowerRtc_Now(uint64_t *out)
{ *out=rtc_stamp;rtc_stamp+=preparation_ticks;preparation_ticks=0;return rtc_ok; }
void HAL_SuspendTick(void) { suspended++;if(inject)EXTI->RPR1|=LIMIT4_Pin; }
void HAL_ResumeTick(void) { resumed++; }
HAL_StatusTypeDef HAL_InitTick(uint32_t priority) { (void)priority;return bad_tick?HAL_ERROR:HAL_OK; }
void SystemCoreClockUpdate(void) { SystemCoreClock=16000000; }
void Error_Handler(void) { errors++; }
void HAL_PWREx_EnterSTOP1Mode(uint8_t entry)
{
 if(entry==PWR_STOPENTRY_WFI && mask && !(TIM2->CR1&TIM_CR1_CEN) && !(USART1->CR1&USART_CR1_UE))stops++;
 if(wire_wfi) { EXTI->RPR1|=LIMIT4_Pin;GPIOA->IDR|=LIMIT4_Pin; }
 rtc_stamp=rtc_backwards?rtc_stamp-1:rtc_stamp+stop_ticks+restoration_ticks;
 if(bad_clock)RCC->CR=0;
}
static void reset(void)
{
 mock_gpioa=(GPIO_TypeDef){0};mock_gpiob=(GPIO_TypeDef){0};mock_tim1=(TIM_TypeDef){0};mock_tim2=(TIM_TypeDef){0};
 mock_rcc=(RCC_TypeDef){0};mock_exti=(EXTI_TypeDef){0};mock_usart1=(MockUSART){0};mock_nvic=(MockNVIC){0};
 mock_adc=(MockADC){0};mock_flash=(MockFLASH){0};
 mask=stops=resumed=suspended=errors=resync=0;rtc_ok=1;rtc_backwards=bad_clock=bad_tick=inject=busy=wire_wfi=0;
 rtc_stamp=100000;stop_ticks=2560;SystemCoreClock=16000000;uwTick=100;
 preparation_ticks=restoration_ticks=0;
 RCC->CR=RCC_CR_HSIRDY;RCC->CFGR=RCC_CFGR_SW_0|RCC_CFGR_SWS_0;
 RCC->CCIPR=RCC_CCIPR_USART1SEL_1;FLASH->ACR=FLASH_LATENCY_1;
 USART1->CR1=USART_CR1_UE|USART_CR1_RXNEIE_RXFNEIE;USART1->ISR=USART_ISR_TC;
}
int run_tests(void)
{
 reset();CHECK(PowerPort_Init());TIM2->CNT=500;mask=1;uint32_t before=HAL_GetTick();
 CHECK(PowerPort_Sleep(250) && stops==1 && errors==0);
 CHECK(HAL_GetTick()-before==10000 && resumed==suspended && mask==1);tests_passed++;
 TIM2->CNT+=37;CHECK(HAL_GetTick()-before==10037);
 CHECK(PowerPort_TakeStopWake() && !PowerPort_TakeStopWake());tests_passed++;
 CHECK((TIM2->CR1&TIM_CR1_CEN) && !TIM2->DIER && (USART1->CR1&USART_CR1_UE) && resync==1);tests_passed++;
 CHECK(RCC->CFGR&RCC_CFGR_STOPWUCK);tests_passed++;
 stop_ticks=1;before=HAL_GetTick();for(unsigned i=0;i<32;++i)CHECK(PowerPort_Sleep(250));
 CHECK(HAL_GetTick()-before==125);tests_passed++;
 reset();CHECK(PowerPort_Init());mask=1;inject=1;CHECK(!PowerPort_Sleep(250) && stops==0);
 CHECK(EXTI->RPR1&LIMIT4_Pin);CHECK(TIM2->CR1&TIM_CR1_CEN);tests_passed++;
 reset();CHECK(PowerPort_Init());mask=1;wire_wfi=1;CHECK(PowerPort_Sleep(250));
 CHECK(EXTI->RPR1&LIMIT4_Pin);CHECK(PowerPort_GpioSignature()&1U);tests_passed++;
 reset();CHECK(PowerPort_Init());mask=1;busy=1;CHECK(!PowerPort_Sleep(250) && !stops);tests_passed++;
 reset();CHECK(PowerPort_Init());mask=1;USART1->ISR=0;CHECK(!PowerPort_Sleep(250) && !stops);tests_passed++;
 reset();CHECK(PowerPort_Init());mask=1;ADC1->CR=ADC_CR_ADSTART;CHECK(!PowerPort_Sleep(250) && !stops);tests_passed++;
 reset();CHECK(PowerPort_Init());mask=1;ADC1->CR=ADC_CR_ADDIS;CHECK(!PowerPort_Sleep(250) && !stops);tests_passed++;
 reset();CHECK(PowerPort_Init());mask=1;RCC->CFGR|=RCC_CFGR_PPRE;
 CHECK(!PowerPort_Sleep(250) && !stops);tests_passed++;
 reset();rtc_ok=0;CHECK(!PowerPort_Init());tests_passed++;
 reset();RCC->CCIPR=0;CHECK(!PowerPort_Init());tests_passed++;
 reset();CHECK(PowerPort_Init());mask=1;bad_clock=1;
 CHECK(!PowerPort_Sleep(250) && errors==1 && !(USART1->CR1&USART_CR1_UE));tests_passed++;
 reset();CHECK(PowerPort_Init());mask=1;rtc_backwards=1;CHECK(!PowerPort_Sleep(250) && errors==1);tests_passed++;
 reset();CHECK(PowerPort_Init());mask=1;bad_tick=1;CHECK(!PowerPort_Sleep(250) && errors==1);tests_passed++;
 reset();CHECK(PowerPort_Init());mask=0;CHECK(!PowerPort_Sleep(250) && !stops);tests_passed++;
 reset();CHECK(PowerPort_Init());mask=1;stop_ticks=0;before=HAL_GetTick();
 CHECK(PowerPort_Sleep(250) && HAL_GetTick()==before);tests_passed++;
 /* Running TIM2, entry work, Stop residency and restoration count once.
  * Two fractional RTC ticks per call carry .8125ms into the next call. */
 reset();CHECK(PowerPort_Init());mask=1;TIM2->CNT=500;before=HAL_GetTick();
 preparation_ticks=restoration_ticks=1;CHECK(PowerPort_Sleep(250));
 CHECK(HAL_GetTick()-before==10007);TIM2->CNT+=37;
 stop_ticks=0;preparation_ticks=1;CHECK(PowerPort_Sleep(250));
 CHECK(HAL_GetTick()-before==10052);tests_passed++;
 return 0;
}
