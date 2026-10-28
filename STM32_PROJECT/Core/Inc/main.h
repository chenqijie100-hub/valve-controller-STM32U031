/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADC1_BAT_Pin GPIO_PIN_0
#define ADC1_BAT_GPIO_Port GPIOA
#define UNUSED4_Pin GPIO_PIN_1
#define UNUSED4_GPIO_Port GPIOA
#define LIMIT_EN_Pin GPIO_PIN_2
#define LIMIT_EN_GPIO_Port GPIOA
#define LIMIT4_Pin GPIO_PIN_3
#define LIMIT4_GPIO_Port GPIOA
#define LIMIT4_EXTI_IRQn EXTI2_3_IRQn
#define UNUSED0_Pin GPIO_PIN_4
#define UNUSED0_GPIO_Port GPIOA
#define VREF_Pin GPIO_PIN_5
#define VREF_GPIO_Port GPIOA
#define UNUSED1_Pin GPIO_PIN_6
#define UNUSED1_GPIO_Port GPIOA
#define ADC1_MOTOR_Pin GPIO_PIN_7
#define ADC1_MOTOR_GPIO_Port GPIOA
#define LIMIT5_Pin GPIO_PIN_0
#define LIMIT5_GPIO_Port GPIOB
#define LIMIT5_EXTI_IRQn EXTI0_1_IRQn
#define LIMIT3_Pin GPIO_PIN_1
#define LIMIT3_GPIO_Port GPIOB
#define LIMIT3_EXTI_IRQn EXTI0_1_IRQn
#define nFLAUT_Pin GPIO_PIN_8
#define nFLAUT_GPIO_Port GPIOA
#define nSLEEP_Pin GPIO_PIN_9
#define nSLEEP_GPIO_Port GPIOA
#define PH_Pin GPIO_PIN_10
#define PH_GPIO_Port GPIOA
#define UNUSED2_Pin GPIO_PIN_12
#define UNUSED2_GPIO_Port GPIOA
#define _1Y_Pin GPIO_PIN_15
#define _1Y_GPIO_Port GPIOA
#define _2Y_Pin GPIO_PIN_3
#define _2Y_GPIO_Port GPIOB
#define _485DE_RE_Pin GPIO_PIN_4
#define _485DE_RE_GPIO_Port GPIOB
#define UNUSED3_Pin GPIO_PIN_5
#define UNUSED3_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
