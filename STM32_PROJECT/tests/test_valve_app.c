/* Compile the actual application parser, STATUS, ADC and cooperative TX. */
#define main phase1_firmware_main
#include "../Core/Src/main.c"
#undef main
#include "battery_monitor.h"
#include "app_status.h"
#include "power_port.h"
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
unsigned tests_passed;
GPIO_TypeDef mock_gpioa, mock_gpiob;
TIM_TypeDef mock_tim1;
RCC_TypeDef mock_rcc;
EXTI_TypeDef mock_exti;
TIM_HandleTypeDef htim1 = { &mock_tim1 };
ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart1;
MockUSART mock_usart1;
uint32_t mock_adc_flags;
uint16_t mock_vref_cal=1650;
static unsigned adc_rank,adc_starts;
static uint32_t tick, tick_calls, mask, sent, arrive_after, break_after;
static uint8_t rx_error, tx_error, adc_timeout;
static char response[1024];
static const char *rx_data;
static uint32_t mock_sleeps;
static uint8_t power_inject_wire;
static uint8_t mock_stop_woke;
uint8_t PowerPort_TakeStopWake(void)
{ uint8_t woke=mock_stop_woke;mock_stop_woke=0;return woke; }
uint8_t PowerPort_Init(void) { return 1; }
uint32_t PowerPort_Lock(void) { uint32_t old=mask;mask=1;return old; }
void PowerPort_Unlock(uint32_t value) { mask=value; }
uint8_t PowerPort_InInterrupt(void) { return 0; }
uint8_t PowerPort_Pending(void) { return RS485_RxBusy() || ValvePort_EventPending(); }
uint8_t PowerPort_OutputsSafe(void) { return !ValvePort_Duty() && !ValvePort_DriverAwake(); }
uint32_t PowerPort_GpioSignature(void)
{ return (GPIOA->IDR&LIMIT4_Pin?1U:0U)|(GPIOB->IDR&LIMIT5_Pin?2U:0U)|
         (GPIOA->IDR&_1Y_Pin?4U:0U)|(GPIOB->IDR&_2Y_Pin?8U:0U); }
uint8_t PowerPort_Sleep(uint32_t ms)
{
  mock_sleeps++; tick+=ms;
#if POWER_LOW_POWER_MODE == 2
  mock_stop_woke=1;
#endif
  if(power_inject_wire) { GPIOA->IDR|=LIMIT4_Pin;ValvePort_WireBreakISR();PowerManager_NotifyGPIOFromISR(); }
  return 1;
}

uint32_t __get_PRIMASK(void) { return mask; }
void __disable_irq(void) { mask = 1; }
void __set_PRIMASK(uint32_t value) { mask = value; }
void __DSB(void) { }
void __ISB(void) { }
/* A register read does not consume a whole millisecond. Advance simulated
 * background time every 16 reads, keeping UART byte transfers below 100 ms. */
uint32_t HAL_GetTick(void) { if (++tick_calls % 16U == 0) tick++; return tick; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint32_t pins, GPIO_PinState state)
{ if (state) p->ODR |= pins; else p->ODR &= ~pins; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint32_t pin)
{ return (p->IDR & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET; }
void Mock_Compare(uint32_t value) { TIM1->CCR4 = value; }
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *t, uint32_t ch)
{ (void)t; (void)ch; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *u, uint8_t *d, uint16_t size, uint32_t timeout)
{
  (void)size; (void)timeout;
  if (rx_error) { u->error = 1; return HAL_ERROR; }
  if (!rx_data || !*rx_data) return HAL_TIMEOUT;
  *d = (uint8_t)*rx_data++; return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *u, const uint8_t *d, uint16_t size, uint32_t timeout)
{
  (void)u; (void)timeout;
  while (size--)
  {
    if (sent < sizeof(response) - 1) response[sent] = (char)*d++;
    sent++; response[sent < sizeof(response) ? sent : sizeof(response)-1] = 0;
    if (arrive_after && sent == arrive_after) GPIOB->IDR |= _2Y_Pin;
    if (break_after && sent == break_after)
    { GPIOA->IDR |= LIMIT4_Pin; ValvePort_WireBreakISR(); }
  }
  return tx_error ? HAL_ERROR : HAL_OK;
}
uint32_t HAL_UART_GetError(UART_HandleTypeDef *u) { return u->error; }
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *a) { (void)a; adc_rank=0; adc_starts++; return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *a) { (void)a; return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *a, uint32_t timeout)
{ (void)a; (void)timeout; return adc_timeout ? HAL_TIMEOUT : HAL_OK; }
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *a)
{ (void)a; const uint32_t raw[3]={2787,512,1500}; return raw[adc_rank++ % 3]; }

static void advance(uint32_t elapsed)
{ tick += elapsed; for (unsigned i=0;i<8;++i) Valve_Service(); }
static void command(const char *line)
{
  sent = 0; response[0] = 0; rx_data = line;
  unsigned length = (unsigned)strlen(line);
  while (*rx_data)
  { USART1->ISR = USART_ISR_RXNE_RXFNE; USART1->RDR = (uint8_t)*rx_data++; RS485_RxIRQ(); }
  for (unsigned i = 0; i < length; ++i) RS485_ProcessInput();
}
static void reset(uint8_t open_request)
{
  tick = tick_calls = mask = sent = arrive_after = break_after = 0;
  rx_error = tx_error = adc_timeout = 0; huart1.error = 0;
  mock_adc_flags=ADC_FLAG_EOC; adc_starts=0;
  mock_sleeps=0;power_inject_wire=mock_stop_woke=0;PowerManager_Init();
  command_length = command_overflow = 0; rx_data = 0;
  GPIOA->IDR = nFLAUT_Pin; GPIOB->IDR = open_request ? LIMIT5_Pin : 0;
  EXTI->RPR1 = 0; RCC->APBENR2 = RCC_APBENR2_TIM1EN; TIM1->ARR = 799;
  ValvePort_Init(); ValveInputs in; ValvePort_Read(&in);
  RS485_RxInit();
  Battery_Init(tick,1); AppStatus_Init();
  Valve_Init(tick); valve_initialized = 1; adc_cal_status = HAL_OK;
  advance(0); advance(50); advance(2000); advance(20); advance(2);
}
int run_tests(void)
{
  reset(1); CHECK(ValvePort_Duty() == 100);
  command("PING\r\n"); CHECK(strstr(response, "PONG"));
  CHECK(ValvePort_Duty() == 100 && Valve_Get()->digital_enabled); tests_passed++;
  unsigned before_query=adc_starts;
  command("STATUS\n"); CHECK(strstr(response, "STATE=OPENING"));
  CHECK(strstr(response, "ADC=OK") && strstr(response,"BAT_VALID=1") && ValvePort_Duty() == 100);
  CHECK(adc_starts==before_query); tests_passed++;

  /* Wire break during reply gets immediate output-off and then closing. */
  break_after = 2; command("PING\n"); break_after = 0;
  CHECK(ValvePort_Duty() == 0 && Valve_Get()->safety_close_pending);
  advance(2000); advance(20); advance(2);
  CHECK(ValvePort_Duty() == 100 && Valve_Get()->state == VALVE_CLOSING); tests_passed++;

  const char *attempts[] = {"STOP\n", "SLEEP\n", "DINOFF\n", "RUNA\n", "RUNB\n", "JOGA\n", "JOGB\n",
                          "PWM0\n", "PWM25\n", "PWM50\n", "PWM75\n", "VREFON\n", "WAKE\n", "FAULTCLR\n"};
  for (unsigned i = 0; i < sizeof(attempts)/sizeof(attempts[0]); ++i)
  {
    command(attempts[i]); CHECK(strstr(response, "ERR"));
    CHECK(ValvePort_Duty() == 100 && Valve_Get()->safety_close_pending); tests_passed++;
  }
  USART1->ISR = USART_ISR_ORE; RS485_RxIRQ(); RS485_ProcessInput();
  CHECK(ValvePort_Duty() == 100 && Valve_Get()->safety_close_pending);
  command("PING\n"); CHECK(strstr(response, "ERR"));
  command("PING\n"); CHECK(strstr(response, "PONG")); tests_passed++;

  tx_error = 1; command("PING\n"); tx_error = 0;
  CHECK(ValvePort_Duty() == 100 && Valve_Get()->safety_close_pending); tests_passed++;
  arrive_after = 10; command("STATUS\n"); arrive_after = 0;
  CHECK(ValvePort_Duty() == 0 && Valve_Get()->state == VALVE_CLOSED); tests_passed++;

  /* Even a failed ADC sample cooperatively monitors the motor. */
  reset(0); CHECK(Valve_Get()->state == VALVE_CLOSING);
  adc_timeout = 1; mock_adc_flags=0; advance(250); advance(21); advance(1);
  command("STATUS\n"); CHECK(strstr(response, "ADC=ERR") && strstr(response,"BAT_VALID=0"));
  CHECK(ValvePort_Duty() == 100); tests_passed++;
  GPIOA->IDR &= ~nFLAUT_Pin; command("PING\n");
  CHECK(ValvePort_Duty() == 0 && Valve_Get()->state == VALVE_FAULT); tests_passed++;
  GPIOA->IDR |= nFLAUT_Pin; command("FAULTCLR\n"); CHECK(strstr(response, "FAULTCLR OK"));
  CHECK(Valve_Get()->history_faults & VALVE_FAULT_DRIVER); tests_passed++;

  /* Periodic acquisition is independent of repeated STATUS; queries do not
   * stop normal motion or truncate the added snapshot metadata. */
  reset(1);
  for (unsigned i=0;i<30;++i)
  {
    command("STATUS\n");
    CHECK(strstr(response,"STATE=OPENING") && strstr(response,"SNAP_MS="));
    CHECK(ValvePort_Duty()==100 && rs485_last_status==HAL_OK);
  }
  CHECK(adc_starts>1); tests_passed++;

  /* An ADC sequence that never completes cannot delay wire-break takeover. */
  reset(1); adc_timeout=1; mock_adc_flags=0; advance(250);
  GPIOA->IDR |= LIMIT4_Pin; ValvePort_WireBreakISR(); advance(0);
  CHECK(ValvePort_Duty()==0 && Valve_Get()->safety_close_pending);
  advance(2000); advance(20); advance(2);
  CHECK(ValvePort_Duty()==100 && Valve_Get()->state==VALVE_CLOSING);
  command("STATUS\n");
  CHECK(strstr(response,"BAT_VALID=0") && strstr(response,"SAFE_PENDING=1"));
  CHECK(ValvePort_Duty()==100); tests_passed++;
  GPIOB->IDR |= _2Y_Pin; advance(1); command("STATUS\n");
  CHECK(strstr(response,"STATE=CLOSED") && ValvePort_Duty()==0); tests_passed++;

  /* Idle hook refuses all running phases in both profiles. */
  reset(1);Power_Idle();CHECK(mock_sleeps==0);tests_passed++;
  GPIOA->IDR|=_1Y_Pin;advance(1);Power_Idle();
#if POWER_LOW_POWER_MODE
  CHECK(mock_sleeps==1);tests_passed++;
  advance(0); /* Timer wake services the now-due ADC sequence before next idle. */
  /* Wake during idle-open: reread wire high, close request cannot sleep. */
  power_inject_wire=1;Power_Idle();power_inject_wire=0;advance(0);
  CHECK(Valve_Get()->safety_close_pending && !Valve_Get()->opening_permitted);
  uint32_t asleep=mock_sleeps;Power_Idle();CHECK(mock_sleeps==asleep);
  advance(2000);advance(20);advance(2);Power_Idle();
  CHECK(Valve_Get()->state==VALVE_CLOSING && ValvePort_Duty()==100 && mock_sleeps==asleep);tests_passed++;
  GPIOB->IDR|=_2Y_Pin;GPIOA->IDR&=~_1Y_Pin;advance(1);
  /* A queued/partial serial command keeps main active without losing it. */
  USART1->ISR=USART_ISR_RXNE_RXFNE;USART1->RDR='P';RS485_RxIRQ();
  asleep=mock_sleeps;Power_Idle();CHECK(mock_sleeps==asleep);
  RS485_ProcessInput();Power_Idle();CHECK(mock_sleeps==asleep);tests_passed++;
#else
  CHECK(mock_sleeps==0 && (power_last_blockers&POWER_BLOCK_DEBUG));tests_passed++;
#endif
#if POWER_LOW_POWER_MODE == 2
  reset(0);GPIOB->IDR|=_2Y_Pin;advance(50);
  Battery_Init(tick,1);Power_Idle();
  CHECK(mock_sleeps==0);tests_passed++;
  advance(500);Power_Idle();CHECK(mock_sleeps==1);tests_passed++;
#endif
  return 0;
}
