#ifndef TEST_MAIN_H
#define TEST_MAIN_H
#include <stdint.h>
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { uint32_t MODER, ODR, IDR, BSRR; } GPIO_TypeDef;
typedef struct { uint32_t CCR4, BDTR, ARR, CR1, CNT, PSC, EGR, DIER, SR, CCR1, CCMR1, CCER; } TIM_TypeDef;
typedef struct { volatile uint32_t IOPENR, APBENR2, APBENR1, APBSMENR1, APBRSTR1, CR, CFGR, CCIPR, BDCR; } RCC_TypeDef;
typedef struct { volatile uint32_t SSR,TR,DR,PRER,ICSR,CR,WPR; } MockRTC;
typedef struct { volatile uint32_t CR,CALFACT; } MockADC;
typedef struct { volatile uint32_t CCR; } MockADCCommon;
typedef struct { volatile uint32_t ACR; } MockFLASH;
extern MockRTC mock_rtc;
extern MockADC mock_adc;
extern MockADCCommon mock_adc_common;
extern MockFLASH mock_flash;
#define RTC (&mock_rtc)
#define ADC1 (&mock_adc)
#define ADC1_COMMON (&mock_adc_common)
#define ADC_CR_ADEN 1U
#define ADC_CR_ADVREGEN (1UL<<28)
#define ADC_CCR_VREFEN (1UL<<22)
#define ADC_CCR_TSEN (1UL<<23)
#define ADC_CCR_VBATEN (1UL<<24)
#define FLASH (&mock_flash)
#define RTC_CR_BYPSHAD (1U<<5)
#define RTC_CR_FMT (1U<<6)
#define RTC_ICSR_INIT (1U<<7)
#define RTC_ICSR_BIN (3U<<8)
#define RCC_BDCR_LSERDY 2U
#define RCC_BDCR_RTCEN (1U<<15)
#define RCC_BDCR_RTCSEL (3U<<8)
#define RCC_BDCR_RTCSEL_0 (1U<<8)
#define RCC_APBENR1_RTCAPBEN (1U<<10)
#define RCC_CR_HSIRDY (1U<<10)
#define RCC_CFGR_SW 7U
#define RCC_CFGR_SW_0 1U
#define RCC_CFGR_SWS (7U<<3)
#define RCC_CFGR_SWS_0 (1U<<3)
#define RCC_CFGR_HPRE (15U<<8)
#define RCC_CFGR_PPRE (7U<<12)
#define RCC_CFGR_STOPWUCK (1U<<15)
#define RCC_CCIPR_USART1SEL 3U
#define RCC_CCIPR_USART1SEL_1 2U
#define RCC_CCIPR_ADCSEL (3U<<28)
#define RCC_CCIPR_TIM1SEL (1U<<24)
#define RCC_CCIPR_ADCSEL_1 (2U<<28)
#define FLASH_ACR_LATENCY 7U
#define ADC_CR_ADSTART 4U
#define ADC_CR_ADDIS 2U
#define ADC_CR_ADSTP 16U
#define ADC_CR_ADCAL (1UL<<31)
#define USART_ISR_TC 64U
#define USART_CR1_UE 1U
#define USART_CR1_UESM 2U
#define USART_RQR_RXFRQ 8U
#define PWR_STOPENTRY_WFI 1U
extern uint32_t SystemCoreClock,uwTickPrio;
void SystemCoreClockUpdate(void);
HAL_StatusTypeDef HAL_InitTick(uint32_t priority);
void HAL_PWREx_EnterSTOP1Mode(uint8_t entry);
typedef struct { uint32_t RPR1,FPR1; } EXTI_TypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
extern GPIO_TypeDef mock_gpioa, mock_gpiob;
extern TIM_TypeDef mock_tim1;
extern RCC_TypeDef mock_rcc;
extern EXTI_TypeDef mock_exti;
#define GPIOA (&mock_gpioa)
#define GPIOB (&mock_gpiob)
#define TIM1 (&mock_tim1)
#define RCC (&mock_rcc)
#define EXTI (&mock_exti)
extern TIM_TypeDef mock_tim2;
#define TIM2 (&mock_tim2)
typedef struct { uint32_t ISPR[1]; } MockNVIC;
typedef struct { uint32_t ICSR,SCR; } MockSCB;
extern MockNVIC mock_nvic;
extern MockSCB mock_scb;
#define NVIC (&mock_nvic)
#define SCB (&mock_scb)
#define SCB_ICSR_PENDSTCLR_Msk (1UL<<25)
#define RCC_APBENR1_TIM2EN 1U
#define RCC_APBSMENR1_TIM2SMEN 1U
#define RCC_APBRSTR1_TIM2RST 1U
#define TIM_EGR_UG 1U
#define TIM_CR1_CEN 1U
#define TIM_DIER_CC1IE 2U
#define TIM_SR_CC1IF 2U
#define TIM2_IRQn 15
#define PWR_MAINREGULATOR_ON 0U
#define PWR_SLEEPENTRY_WFI 1U
#define USART_ISR_BUSY (1UL<<16)
uint32_t __get_IPSR(void);
uint32_t HAL_RCC_GetHCLKFreq(void);
uint32_t HAL_RCC_GetPCLK1Freq(void);
void HAL_NVIC_SetPriority(int irq,uint32_t priority,uint32_t subpriority);
void HAL_NVIC_EnableIRQ(int irq);
void HAL_NVIC_ClearPendingIRQ(int irq);
void HAL_SuspendTick(void);
void HAL_ResumeTick(void);
void HAL_PWR_EnterSLEEPMode(uint32_t regulator,uint8_t entry);
typedef struct { volatile uint32_t ISR, ICR, RDR, CR1, CR3, RQR; } MockUSART;
extern MockUSART mock_usart1;
#define USART1 (&mock_usart1)
#define USART_ISR_PE 1U
#define USART_ISR_FE 2U
#define USART_ISR_NE 4U
#define USART_ISR_ORE 8U
#define USART_ISR_RXNE_RXFNE 32U
#define USART_CR1_RXNEIE_RXFNEIE 32U
#define USART_CR1_PEIE 256U
#define USART_CR3_EIE 1U
#define GPIO_PIN_0 1U
#define GPIO_PIN_1 2U
#define GPIO_PIN_2 4U
#define GPIO_PIN_3 8U
#define GPIO_PIN_4 16U
#define GPIO_PIN_5 32U
#define GPIO_PIN_7 128U
#define GPIO_PIN_8 256U
#define GPIO_PIN_9 512U
#define GPIO_PIN_10 1024U
#define GPIO_PIN_11 2048U
#define GPIO_PIN_15 32768U
#define LIMIT4_GPIO_Port GPIOA
#define LIMIT4_Pin GPIO_PIN_3
#define LIMIT5_GPIO_Port GPIOB
#define LIMIT5_Pin GPIO_PIN_0
#define LIMIT3_GPIO_Port GPIOB
#define LIMIT3_Pin GPIO_PIN_1
#define _1Y_GPIO_Port GPIOA
#define _1Y_Pin GPIO_PIN_15
#define _2Y_GPIO_Port GPIOB
#define _2Y_Pin GPIO_PIN_3
#define PH_GPIO_Port GPIOA
#define PH_Pin GPIO_PIN_10
#define nSLEEP_GPIO_Port GPIOA
#define nSLEEP_Pin GPIO_PIN_9
#define nFLAUT_GPIO_Port GPIOA
#define nFLAUT_Pin GPIO_PIN_8
#define VREF_GPIO_Port GPIOA
#define VREF_Pin GPIO_PIN_5
#define _485DE_RE_GPIO_Port GPIOB
#define _485DE_RE_Pin GPIO_PIN_4
#define LIMIT_EN_GPIO_Port GPIOA
#define LIMIT_EN_Pin GPIO_PIN_2
#define RCC_IOPENR_GPIOAEN 1U
#define RCC_APBENR2_TIM1EN 1U
#define TIM_BDTR_MOE (1U << 15)
#define TIM_CHANNEL_4 4U
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t value);
void __DSB(void);
void __ISB(void);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint32_t pins, GPIO_PinState state);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint32_t pin);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *timer, uint32_t channel);
void Mock_Compare(uint32_t value);
#define __HAL_TIM_SET_COMPARE(handle, channel, value) ((void)(handle), (void)(channel), Mock_Compare(value))
#define __HAL_TIM_GET_AUTORELOAD(handle) ((handle)->Instance->ARR)

typedef struct { uint32_t PLLState; } MockPLL;
typedef struct { uint32_t OscillatorType, LSEState, HSIState, HSICalibrationValue; MockPLL PLL; } RCC_OscInitTypeDef;
typedef struct { uint32_t ClockType, SYSCLKSource, AHBCLKDivider, APB1CLKDivider; } RCC_ClkInitTypeDef;
#define PWR_REGULATOR_VOLTAGE_SCALE2 0U
#define RCC_LSEDRIVE_LOW 0U
#define RCC_OSCILLATORTYPE_HSI 1U
#define RCC_OSCILLATORTYPE_LSE 2U
#define RCC_LSE_ON 1U
#define RCC_HSI_ON 1U
#define RCC_HSICALIBRATION_DEFAULT 0U
#define RCC_PLL_NONE 0U
#define RCC_CLOCKTYPE_HCLK 1U
#define RCC_CLOCKTYPE_SYSCLK 2U
#define RCC_CLOCKTYPE_PCLK1 4U
#define RCC_SYSCLKSOURCE_HSI 0U
#define RCC_SYSCLK_DIV1 1U
#define RCC_HCLK_DIV1 1U
#define FLASH_LATENCY_1 1U
#define __HAL_RCC_LSEDRIVE_CONFIG(value) ((void)(value))
HAL_StatusTypeDef HAL_Init(void);
void HAL_Delay(uint32_t delay);
uint32_t HAL_GetTick(void);
void HAL_PWREx_ControlVoltageScaling(uint32_t scale);
void HAL_PWR_EnableBkUpAccess(void);
HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef *config);
HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef *config, uint32_t latency);
void Error_Handler(void);
#endif
