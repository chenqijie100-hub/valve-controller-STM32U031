#include "main.h"
#include "tim.h"
#include "valve_port.h"
#include "limit_power.h"
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
unsigned tests_passed;
void *memset(void *p, int v, unsigned size)
{ unsigned char *b = p; while (size--) *b++ = (unsigned char)v; return p; }
GPIO_TypeDef mock_gpioa, mock_gpiob;
TIM_TypeDef mock_tim1;
RCC_TypeDef mock_rcc;
EXTI_TypeDef mock_exti;
TIM_HandleTypeDef htim1 = { &mock_tim1 };
static uint32_t mask, inject_pin;
static uint8_t inject_pwm, pulse;
static ValveStatus status;
static uint8_t limit_ready=1;
uint8_t LimitPower_DriveAllowed(void) { return limit_ready; }
void LimitPower_Filter(uint32_t now,ValveInputs *in) { (void)now;in->limits_invalid=!limit_ready; }
uint32_t PowerManager_EventGeneration(void) { return 0; }
uint32_t HAL_GetTick(void) { return 0; }
const ValveStatus *Valve_Get(void) { return &status; }
uint32_t __get_PRIMASK(void) { return mask; }
void __disable_irq(void) { mask = 1; }
void __DSB(void) { }
void __ISB(void) { }
void __set_PRIMASK(uint32_t value)
{
  mask = value;
  if (!mask && (EXTI->RPR1 & LIMIT4_Pin))
  { EXTI->RPR1 &= ~LIMIT4_Pin; ValvePort_WireBreakISR(); }
}
static void inject(void)
{
  GPIOA->IDR |= LIMIT4_Pin; EXTI->RPR1 |= LIMIT4_Pin;
  if (pulse) GPIOA->IDR &= ~LIMIT4_Pin;
  if (!mask) __set_PRIMASK(0);
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint32_t pins, GPIO_PinState state)
{
  if (state) port->ODR |= pins; else port->ODR &= ~pins;
  if (state && (pins & inject_pin)) { inject_pin = 0; inject(); }
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint32_t pin)
{ return (port->IDR & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET; }
void Mock_Compare(uint32_t value)
{ TIM1->CCR4 = value; if (value && inject_pwm) { inject_pwm = 0; inject(); } }
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *t, uint32_t ch)
{ (void)t; (void)ch; return HAL_OK; }

static void reset(void)
{
  mask = inject_pin = inject_pwm = pulse = 0;
  limit_ready=1;
  GPIOA->IDR = nFLAUT_Pin; GPIOB->IDR = 0;
  EXTI->RPR1 = 0; RCC->APBENR2 = RCC_APBENR2_TIM1EN; TIM1->ARR = 799;
  status = (ValveStatus){.opening_permitted = 1};
  ValvePort_Init();
  ValveInputs in; ValvePort_Read(&in); /* consume previous test's sticky event */
}
int run_tests(void)
{
  ValveInputs in;
  reset(); CHECK(ValvePort_Apply(VALVE_DRIVE_REFERENCE, VALVE_DIR_OPEN, 100) == VALVE_PORT_OK);
  CHECK(!(GPIOA->ODR & PH_Pin) && (GPIOA->ODR & VREF_Pin));
  CHECK(ValvePort_Apply(VALVE_DRIVE_WAKE, VALVE_DIR_OPEN, 100) == VALVE_PORT_OK);
  CHECK(ValvePort_Apply(VALVE_DRIVE_RUN, VALVE_DIR_OPEN, 100) == VALVE_PORT_OK);
  CHECK(TIM1->CCR4 == 800); tests_passed++;

  /* Rising ISR immediately removes opening drive and records even a pulse. */
  inject(); CHECK(!(GPIOA->ODR & nSLEEP_Pin) && TIM1->CCR4 == 0);
  GPIOA->IDR &= ~LIMIT4_Pin; ValvePort_Read(&in); CHECK(in.break_event);
  ValvePort_Read(&in); CHECK(!in.break_event); tests_passed++;

  /* Closing is never cut by the wire-break ISR. */
  reset(); status.safety_close_pending = 1;
  CHECK(ValvePort_Apply(VALVE_DRIVE_REFERENCE, VALVE_DIR_CLOSE, 100) == VALVE_PORT_OK);
  CHECK(ValvePort_Apply(VALVE_DRIVE_WAKE, VALVE_DIR_CLOSE, 100) == VALVE_PORT_OK);
  CHECK(ValvePort_Apply(VALVE_DRIVE_RUN, VALVE_DIR_CLOSE, 100) == VALVE_PORT_OK);
  inject(); CHECK((GPIOA->ODR & nSLEEP_Pin) && TIM1->CCR4 == 800); tests_passed++;

  /* Raw high, pending EXTI, and already recorded pulse each block energization. */
  for (unsigned source = 0; source < 3; ++source)
  {
    reset();
    if (source == 0) GPIOA->IDR |= LIMIT4_Pin;
    if (source == 1) EXTI->RPR1 |= LIMIT4_Pin;
    if (source == 2) ValvePort_WireBreakISR();
    CHECK(ValvePort_Apply(VALVE_DRIVE_REFERENCE, VALVE_DIR_OPEN, 100) == VALVE_PORT_BREAK);
    CHECK(!(GPIOA->ODR & nSLEEP_Pin) && TIM1->CCR4 == 0); tests_passed++;
  }

  /* A rising/pulsed wire signal DURING each enable write cannot escape the guard. */
  for (unsigned boundary = 0; boundary < 3; ++boundary)
    for (unsigned transient = 0; transient < 2; ++transient)
    {
      reset(); pulse = transient;
      if (boundary == 0) inject_pin = VREF_Pin;
      ValvePortResult r = ValvePort_Apply(VALVE_DRIVE_REFERENCE, VALVE_DIR_OPEN, 100);
      if (boundary == 1) inject_pin = nSLEEP_Pin;
      if (r == VALVE_PORT_OK) r = ValvePort_Apply(VALVE_DRIVE_WAKE, VALVE_DIR_OPEN, 100);
      if (boundary == 2) inject_pwm = 1;
      if (r == VALVE_PORT_OK) r = ValvePort_Apply(VALVE_DRIVE_RUN, VALVE_DIR_OPEN, 100);
      CHECK(r == VALVE_PORT_BREAK && !(GPIOA->ODR & nSLEEP_Pin) && TIM1->CCR4 == 0);
      ValvePort_Read(&in); CHECK(in.break_event); tests_passed++;
    }
  reset(); status.latched_faults = VALVE_FAULT_TIMEOUT;
  CHECK(ValvePort_Apply(VALVE_DRIVE_REFERENCE, VALVE_DIR_CLOSE, 100) == VALVE_PORT_ERROR);
  reset(); GPIOA->IDR |= _1Y_Pin;
  CHECK(ValvePort_Apply(VALVE_DRIVE_REFERENCE, VALVE_DIR_OPEN, 100) == VALVE_PORT_LIMIT);
  reset(); GPIOB->IDR |= _2Y_Pin;
  CHECK(ValvePort_Apply(VALVE_DRIVE_REFERENCE, VALVE_DIR_CLOSE, 100) == VALVE_PORT_LIMIT);
  tests_passed++;
  reset(); ValvePort_EmergencyOff();
  CHECK(GPIOA->BSRR & (nSLEEP_Pin << 16)); CHECK(!(TIM1->BDTR & TIM_BDTR_MOE));
  CHECK(GPIOA->BSRR & (LIMIT_EN_Pin << 16));
  tests_passed++;
  reset();limit_ready=0;
  for(unsigned stage=VALVE_DRIVE_REFERENCE;stage<=VALVE_DRIVE_RUN;++stage)
  {
    CHECK(ValvePort_Apply((ValveDriveStage)stage,VALVE_DIR_CLOSE,100)==VALVE_PORT_ERROR);
    CHECK(!(GPIOA->ODR&nSLEEP_Pin) && TIM1->CCR4==0);tests_passed++;
  }
  return 0;
}
