#ifndef TEST_MAIN_H
#define TEST_MAIN_H
#include <stdint.h>
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { uint32_t MODER, ODR, IDR, BSRR; } GPIO_TypeDef;
typedef struct { uint32_t CCR4, BDTR, ARR; } TIM_TypeDef;
typedef struct { uint32_t IOPENR, APBENR2; } RCC_TypeDef;
typedef struct { uint32_t RPR1; } EXTI_TypeDef;
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
typedef struct { volatile uint32_t ISR, ICR, RDR, CR1, CR3; } MockUSART;
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
