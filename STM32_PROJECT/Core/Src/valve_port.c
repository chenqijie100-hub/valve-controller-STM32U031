#include "valve_port.h"
#include "main.h"
#include "tim.h"

static uint8_t ready;
static volatile uint8_t wire_event, opening, awake;
static volatile uint32_t applied_duty;

static void quick_off(void)
{
  /* nSLEEP first: CCR4 is preloaded and zero alone is not immediate. */
  HAL_GPIO_WritePin(nSLEEP_GPIO_Port, nSLEEP_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(VREF_GPIO_Port, VREF_Pin, GPIO_PIN_RESET);
  if (RCC->APBENR2 & RCC_APBENR2_TIM1EN) TIM1->CCR4 = 0;
  awake = 0; applied_duty = 0;
}

void ValvePort_Init(void)
{
  ready = 0; opening = 0;
  quick_off();
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0U);
  ready = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) == HAL_OK;
  /* Do not clear wire_event: a pulse during initialization still matters. */
}

static uint8_t break_pending(void)
{
  return wire_event || HAL_GPIO_ReadPin(LIMIT4_GPIO_Port, LIMIT4_Pin) == GPIO_PIN_SET ||
         (EXTI->RPR1 & LIMIT4_Pin) != 0;
}

ValvePortResult ValvePort_Apply(ValveDriveStage next, ValveDirection dir, uint32_t percent)
{
  ValvePortResult result = VALVE_PORT_OK;
  uint32_t mask = __get_PRIMASK();
  __disable_irq();
  if (next == VALVE_DRIVE_OFF)
  { quick_off(); opening = 0; __set_PRIMASK(mask); return VALVE_PORT_OK; }

  /* Every write capable of enabling the bridge passes this same guard. */
  if (!ready || Valve_Get()->latched_faults || percent == 0 || percent > 100 ||
      (dir != VALVE_DIR_OPEN && dir != VALVE_DIR_CLOSE) ||
      next < VALVE_DRIVE_REFERENCE || next > VALVE_DRIVE_RUN)
    result = VALVE_PORT_ERROR;
  else if (dir == VALVE_DIR_OPEN && (break_pending() || !Valve_Get()->opening_permitted))
    result = VALVE_PORT_BREAK;
  else if ((HAL_GPIO_ReadPin(_1Y_GPIO_Port, _1Y_Pin) == GPIO_PIN_SET && dir == VALVE_DIR_OPEN) ||
           (HAL_GPIO_ReadPin(_2Y_GPIO_Port, _2Y_Pin) == GPIO_PIN_SET && dir == VALVE_DIR_CLOSE))
    result = VALVE_PORT_LIMIT;
  else if (next == VALVE_DRIVE_RUN && HAL_GPIO_ReadPin(nFLAUT_GPIO_Port, nFLAUT_Pin) == GPIO_PIN_RESET)
    result = VALVE_PORT_DRIVER;

  if (result == VALVE_PORT_OK)
  {
    /* Publish intent BEFORE any enable, so a pending ISR also catches startup. */
    opening = dir == VALVE_DIR_OPEN;
    if (next == VALVE_DRIVE_REFERENCE)
    {
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0);
      HAL_GPIO_WritePin(PH_GPIO_Port, PH_Pin, opening ? GPIO_PIN_RESET : GPIO_PIN_SET);
      HAL_GPIO_WritePin(VREF_GPIO_Port, VREF_Pin, GPIO_PIN_SET);
    }
    else if (next == VALVE_DRIVE_WAKE)
    { awake = 1; HAL_GPIO_WritePin(nSLEEP_GPIO_Port, nSLEEP_Pin, GPIO_PIN_SET); }
    else
    {
      applied_duty = percent;
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4,
                          ((__HAL_TIM_GET_AUTORELOAD(&htim1) + 1U) * percent) / 100U);
    }
    /* Catch raw changes / a short pulse while interrupts were masked. */
    __DSB();
    if (opening && break_pending()) result = VALVE_PORT_BREAK;
  }
  if (result != VALVE_PORT_OK)
  {
    if (result == VALVE_PORT_BREAK) wire_event = 1;
    quick_off(); opening = 0;
  }
  __set_PRIMASK(mask);
  return result;
}

void ValvePort_Read(ValveInputs *in)
{
  uint32_t mask = __get_PRIMASK();
  __disable_irq();
  in->break_event = wire_event;
  wire_event = 0;
  in->wire_break = HAL_GPIO_ReadPin(LIMIT4_GPIO_Port, LIMIT4_Pin) == GPIO_PIN_SET;
  in->open_limit = HAL_GPIO_ReadPin(_1Y_GPIO_Port, _1Y_Pin) == GPIO_PIN_SET;
  in->close_limit = HAL_GPIO_ReadPin(_2Y_GPIO_Port, _2Y_Pin) == GPIO_PIN_SET;
  in->request_open = HAL_GPIO_ReadPin(LIMIT5_GPIO_Port, LIMIT5_Pin) == GPIO_PIN_SET;
  /* An opening interrupted into sleep must not manufacture an awake fault. */
  in->driver_fault = awake && HAL_GPIO_ReadPin(nFLAUT_GPIO_Port, nFLAUT_Pin) == GPIO_PIN_RESET;
  __set_PRIMASK(mask);
}

void ValvePort_WireBreakISR(void)
{
  wire_event = 1;
  if (opening) quick_off();
  /* No delays, logging, protocol, state transitions or reversing in the ISR. */
}

uint32_t ValvePort_Duty(void) { return applied_duty; }
uint8_t ValvePort_DriverAwake(void) { return awake; }

void ValvePort_EmergencyOff(void)
{
  /* HAL/tick/handle independent, including failures before GPIO/TIM init.
   * Best effort only: no software can close a valve with a dead CPU/supply. */
  __disable_irq();
  RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
  (void)RCC->IOPENR;
  GPIOA->BSRR = ((uint32_t)(nSLEEP_Pin | VREF_Pin | GPIO_PIN_11)) << 16;
  GPIOA->MODER = (GPIOA->MODER & ~((3UL << 18) | (3UL << 10) | (3UL << 22))) |
                 (1UL << 18) | (1UL << 10) | (1UL << 22);
  if (RCC->APBENR2 & RCC_APBENR2_TIM1EN)
  { TIM1->CCR4 = 0; TIM1->BDTR &= ~TIM_BDTR_MOE; }
  awake = 0; applied_duty = 0; ready = 0;
  __DSB(); __ISB();
}
