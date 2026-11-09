#include "main.h"
#include "comm_port.h"

#define COMM_EN GPIO_PIN_1
#define COMM_FLT GPIO_PIN_4

void CommPower_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  HAL_GPIO_WritePin(GPIOA, COMM_EN, GPIO_PIN_RESET);
  gpio.Pin = COMM_EN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin = COMM_FLT;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP; /* Open-drain FLT#, pulled to MCU VDD. */
  HAL_GPIO_Init(GPIOA, &gpio);
}

void CommPower_Set(uint8_t enabled)
{
  HAL_GPIO_WritePin(GPIOA, COMM_EN, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint8_t CommPower_Fault(void)
{
  return HAL_GPIO_ReadPin(GPIOA, COMM_FLT) == GPIO_PIN_RESET;
}

void CommPort_EmergencyOff(void)
{
#if VALVE_MODBUS_ENABLE && VALVE_COMM_9V_ENABLE
  if (RCC->IOPENR & RCC_IOPENR_GPIOAEN) GPIOA->BRR = COMM_EN;
#endif
  if (RCC->IOPENR & RCC_IOPENR_GPIOBEN) _485DE_RE_GPIO_Port->BRR = _485DE_RE_Pin;
}
