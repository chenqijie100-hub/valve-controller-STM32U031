/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "rtc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "valve_control.h"
#include "valve_port.h"
#include "rs485_rx.h"
#include "battery_monitor.h"
#include "app_status.h"
#include "power_manager.h"
#include "power_port.h"
#include "limit_power.h"
#include "comm_port.h"
#include "valve_comm_service.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Battery conversion and acquisition are owned by battery_monitor. */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Inspect these variables in the debugger if no text reaches the PC. */
volatile uint32_t rs485_tx_count = 0;
volatile uint32_t rs485_tx_errors = 0;
volatile HAL_StatusTypeDef rs485_last_status = HAL_OK;
volatile uint32_t rs485_rx_bytes = 0;
volatile uint32_t rs485_rx_errors = 0;
#if !VALVE_MODBUS_ENABLE
static char command_buffer[32];
static uint32_t command_length = 0;
static uint8_t command_overflow = 0;
#endif
volatile HAL_StatusTypeDef adc_cal_status = HAL_ERROR;
volatile HAL_StatusTypeDef adc_last_status = HAL_ERROR;
volatile uint32_t adc_bat_raw = 0;
volatile uint32_t adc_motor_raw = 0;
volatile uint32_t bat_sys_mv = 0;
static uint8_t valve_initialized = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
#if !VALVE_MODBUS_ENABLE
static HAL_StatusTypeDef RS485_Send(const uint8_t *data, uint16_t length);
static void RS485_Reply(const char *text);
static void RS485_ProcessInput(void);
static void SendStatus(void);
static void Service_Wait(uint32_t duration_ms);
#endif
static void Valve_Service(void);
static void Power_Idle(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  MX_GPIO_Init(); /* Safe driver outputs before clock/peripheral setup. */

  /* USER CODE BEGIN Init */
#if VALVE_MODBUS_ENABLE && VALVE_COMM_9V_ENABLE
  CommPower_Init(); /* Preload PA1 low before configuring it as output. */
#endif

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  /* Detection starts enabled; automatic management runs only after safety service. */
  HAL_GPIO_WritePin(nSLEEP_GPIO_Port, nSLEEP_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(VREF_GPIO_Port, VREF_Pin, GPIO_PIN_RESET);
  LimitPower_Init(HAL_GetTick());
  HAL_GPIO_WritePin(_485DE_RE_GPIO_Port, _485DE_RE_Pin, GPIO_PIN_RESET);
  adc_cal_status = HAL_ADCEx_Calibration_Start(&hadc1);
  HAL_Delay(100); /* Existing limit-circuit RC settling, with drive asleep. */
  ValvePort_Init();
  Valve_Init(HAL_GetTick());
#if VALVE_MODBUS_ENABLE
  ValveComm_Init();
#else
  RS485_RxInit();
#endif
  Battery_Init(HAL_GetTick(), adc_cal_status == HAL_OK);
  AppStatus_Init();
  PowerManager_Init();
  valve_initialized = 1;
  Valve_Service(); /* Sample safety first; digital control defaults on. */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    Valve_Service();
#if VALVE_MODBUS_ENABLE
    AppStatus snapshot;
    uint32_t now = HAL_GetTick();
    AppStatus_Get(now, &snapshot);
    ValveComm_Service(now, &snapshot,
        !Valve_CanSleep(now) || !LimitPower_CanSleep() ||
        ValvePort_DriverAwake() || ValvePort_Duty() ||
        ValvePort_EventPending() || ((EXTI->RPR1 | EXTI->FPR1) & (LIMIT4_Pin | LIMIT5_Pin)));
#else
    RS485_ProcessInput();
#endif
    Power_Idle(); /* Wake returns to fresh safety sampling at loop start. */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
static void Valve_Service(void)
{
  ValveInputs inputs;
  if (!valve_initialized) return;
  uint32_t observed_generation = PowerManager_EventGeneration();
  ValvePort_Read(&inputs);
  uint32_t now = HAL_GetTick();
  Valve_Step(now, &inputs); /* Safety always runs before ADC work. */
  LimitPower_Service(now,&inputs,Valve_Get(),Valve_CanSleep(now),observed_generation);
  inputs.limits_invalid=!LimitPower_Valid(); /* Publish a domain change in this same snapshot. */
#if POWER_LOW_POWER_MODE == 2
  if (PowerPort_TakeStopWake()) Battery_AfterStop(now);
#endif
  Battery_Service(now);
  AppStatus_Publish(now, &inputs,
      HAL_GPIO_ReadPin(LIMIT3_GPIO_Port, LIMIT3_Pin) == GPIO_PIN_SET,
      HAL_GPIO_ReadPin(nFLAUT_GPIO_Port, nFLAUT_Pin) == GPIO_PIN_SET,
      HAL_GPIO_ReadPin(nSLEEP_GPIO_Port, nSLEEP_Pin) == GPIO_PIN_SET,
      HAL_GPIO_ReadPin(VREF_GPIO_Port, VREF_Pin) == GPIO_PIN_SET,
      ValvePort_Duty());
  PowerManager_MarkServiced(observed_generation, PowerManager_Signature(&inputs));
}

static void Power_Idle(void)
{
  if (!valve_initialized) return;
  uint32_t now = HAL_GetTick();
  const ValveStatus *valve = Valve_Get();
  uint32_t budget = Battery_SleepBudget(now);
  uint8_t adc_busy = budget==0;
#if POWER_LOW_POWER_MODE == 2
  adc_busy |= !Battery_CanStop(); /* Finish first post-boot/post-wake sample. */
#endif
  PowerRequest request = {
    .motor_running = ValvePort_Duty()!=0 || ValvePort_DriverAwake() ||
                     valve->state==VALVE_OPENING || valve->state==VALVE_CLOSING,
    .safety_pending = valve->safety_close_pending,
    .valve_busy = !Valve_CanSleep(now), /* Includes input qualification/deferred direction. */
    .adc_busy = adc_busy,
#if VALVE_MODBUS_ENABLE
    .uart_busy = ValveComm_Busy(),
#else
    .uart_busy = RS485_RxBusy() || command_length!=0 || command_overflow!=0,
#endif
    .inputs_busy = !LimitPower_CanSleep(),
    .sleep_ms = budget
  };
  (void)PowerManager_TrySleep(&request);
}

#if !VALVE_MODBUS_ENABLE
static void Service_Wait(uint32_t duration_ms)
{
  uint32_t start = HAL_GetTick();
  while ((uint32_t)(HAL_GetTick() - start) < duration_ms) Valve_Service();
}

static void RS485_Reply(const char *text)
{
  Service_Wait(2);
  rs485_last_status = RS485_Send((const uint8_t *)text, (uint16_t)strlen(text));
  if (rs485_last_status == HAL_OK) rs485_tx_count++;
  else rs485_tx_errors++;
}

static void RS485_ProcessInput(void)
{
  uint8_t byte;
  int rx = RS485_RxPoll(&byte);
  Valve_Service();
  if (rx != 1)
  {
    if (rx < 0)
    {
      rs485_rx_errors++;
      /* Discard through LF. Communication never cancels safety closure. */
      command_length = 0; command_overflow = 1;
    }
    return;
  }
  rs485_rx_bytes++;
  if (byte == '\r') return;
  if (byte != '\n')
  {
    if (!command_overflow)
    {
      if (byte < 32U || byte > 126U || command_length >= sizeof(command_buffer) - 1U)
        command_overflow = 1;
      else command_buffer[command_length++] = (char)byte;
    }
    return;
  }
  if (command_overflow) RS485_Reply("ERR invalid or too long command\r\n");
  else if (command_length)
  {
    uint8_t accepted = 0;
    command_buffer[command_length] = '\0';
    if (strcmp(command_buffer, "PING") == 0) RS485_Reply("PONG\r\n");
    else if (strcmp(command_buffer, "STATUS") == 0) SendStatus();
    else if (strcmp(command_buffer, "DINON") == 0)
    {
      accepted = Valve_SetDigital(1, HAL_GetTick());
      RS485_Reply(accepted ? "DIN=ON\r\n" : "ERR safety/fault\r\n");
    }
    else if (strcmp(command_buffer, "STOP") == 0 || strcmp(command_buffer, "SLEEP") == 0 ||
             strcmp(command_buffer, "DINOFF") == 0)
    {
      accepted = Valve_SetDigital(0, HAL_GetTick());
      RS485_Reply(accepted ? "MOTOR STOP; DIN=OFF\r\n" : "ERR safety close has priority\r\n");
    }
    else if (strcmp(command_buffer, "RUNA") == 0 || strcmp(command_buffer, "RUNB") == 0 ||
             strcmp(command_buffer, "JOGA") == 0 || strcmp(command_buffer, "JOGB") == 0)
    {
      uint8_t jog = command_buffer[0] == 'J';
      uint8_t open = command_buffer[3] == 'A';
      accepted = Valve_Request(open ? VALVE_DIR_OPEN : VALVE_DIR_CLOSE, jog ? 100U : 0U, jog ? 50U : 100U);
      Valve_Service();
      RS485_Reply(accepted ? "MOTOR request accepted; safety arbiter active\r\n" : "ERR busy/safety/fault\r\n");
    }
    else if (strcmp(command_buffer, "FAULTCLR") == 0)
    {
      ValveInputs inputs;
      ValvePort_Read(&inputs);
      /* Reset requires a physically healthy nFAULT, including while asleep. */
      inputs.driver_fault = HAL_GPIO_ReadPin(nFLAUT_GPIO_Port, nFLAUT_Pin) == GPIO_PIN_RESET;
      accepted = Valve_ClearFaults(HAL_GetTick(), &inputs);
      /* Never lose an event consumed while checking a rejected reset. */
      Valve_Step(HAL_GetTick(), &inputs);
      RS485_Reply(accepted ? "FAULTCLR OK; history retained; low input required to reopen\r\n" :
                            "ERR fault reset conditions not met\r\n");
    }
    else if (strcmp(command_buffer, "PWM0") == 0 || strcmp(command_buffer, "PWM25") == 0 ||
             strcmp(command_buffer, "PWM50") == 0 || strcmp(command_buffer, "PWM75") == 0 ||
             strcmp(command_buffer, "VREFON") == 0 || strcmp(command_buffer, "WAKE") == 0)
      RS485_Reply("ERR raw driver tests disabled in phase1; use guarded RUN/JOG\r\n");
    else RS485_Reply("ERR use PING STATUS DINON DINOFF STOP RUNA RUNB JOGA JOGB FAULTCLR\r\n");
  }
  command_length = 0; command_overflow = 0;
}

static void SendStatus(void)
{
  AppStatus snapshot;
  char message[640];
  char adc_message[256];
  AppStatus_Get(HAL_GetTick(), &snapshot);
  const BatteryStatus *battery = &snapshot.battery;
  const ValveStatus *valve = &snapshot.valve;
  /* Debugger compatibility: these are cached last-good values, not new samples. */
  adc_bat_raw = battery->bat_raw;
  adc_motor_raw = battery->motor_raw;
  bat_sys_mv = battery->millivolts;
  adc_last_status = battery->valid ? HAL_OK : HAL_ERROR;
  int adc_length = snprintf(adc_message, sizeof(adc_message),
      "ADC=%s BAT_RAW=%u BAT_SYS=%lumV MOTOR_RAW=%u VDDA=%lumV VREF_RAW=%u "
      "BAT_VALID=%u BAT_HAS=%u BAT_UPDATED=%lu BAT_AGE=%lu BAT_SEQ=%lu ADC_RESULT=%s ADC_FAILURES=%lu",
      battery->valid ? "OK" : "ERR", (unsigned int)battery->bat_raw,
      (unsigned long)battery->millivolts, (unsigned int)battery->motor_raw,
      (unsigned long)battery->vdda_mv, (unsigned int)battery->vref_raw,
      (unsigned int)battery->valid, (unsigned int)battery->has_value,
      (unsigned long)battery->updated_ms, (unsigned long)battery->age_ms,
      (unsigned long)battery->sequence, Battery_ResultName(battery->result),
      (unsigned long)battery->failures);
  if (adc_length <= 0 || (size_t)adc_length >= sizeof(adc_message))
  { RS485_Reply("ERR ADC status formatting\r\n"); return; }
  int length = snprintf(message, sizeof(message),
      "STATUS count=%lu L1=%u L2=%u L3=%u L4=%u L5=%u nFAULT=%u %s PWM=%lu%% "
      "nSLEEP=%u VREF_EN=%u DIN=%u STATE=%s CURRENT=0x%lX LATCHED=0x%lX HISTORY=0x%lX "
      "SAFE_PENDING=%u SAFE_INCOMPLETE=%u OPEN_OK=%u SNAP_MS=%lu LIM_VALID=%u\r\n",
      (unsigned long)rs485_tx_count,
      (unsigned int)snapshot.inputs.open_limit, (unsigned int)snapshot.inputs.close_limit,
      (unsigned int)snapshot.limit3, (unsigned int)snapshot.inputs.wire_break,
      (unsigned int)snapshot.inputs.request_open, (unsigned int)snapshot.nfault, adc_message,
      (unsigned long)snapshot.pwm_percent, (unsigned int)snapshot.nsleep,
      (unsigned int)snapshot.vref_enabled, (unsigned int)valve->digital_enabled,
      Valve_StateName(valve->state), (unsigned long)valve->current_faults,
      (unsigned long)valve->latched_faults, (unsigned long)valve->history_faults,
      (unsigned int)valve->safety_close_pending,
      (unsigned int)((valve->latched_faults & VALVE_FAULT_SAFETY_INCOMPLETE) != 0),
      (unsigned int)valve->opening_permitted, (unsigned long)snapshot.captured_ms,
      (unsigned int)!snapshot.inputs.limits_invalid);
  if (length > 0 && (size_t)length < sizeof(message)) RS485_Reply(message);
  else RS485_Reply("ERR status formatting\r\n");
}

static HAL_StatusTypeDef RS485_Send(const uint8_t *data, uint16_t length)
{
  HAL_StatusTypeDef tx = HAL_OK;
  uint32_t start;
  HAL_GPIO_WritePin(_485DE_RE_GPIO_Port, _485DE_RE_Pin, GPIO_PIN_SET);
  Service_Wait(1);
  start = HAL_GetTick();
  /* Keep the ASCII protocol and TC-complete direction release; service control
     between bytes so diagnostics cannot suspend limits/timeout/safe closure. */
  for (uint16_t i = 0; i < length; ++i)
  {
    Valve_Service();
    tx = HAL_UART_Transmit(&huart1, &data[i], 1U, 2U);
    Valve_Service();
    if (tx != HAL_OK) break;
    if ((uint32_t)(HAL_GetTick() - start) >= 100U) { tx = HAL_TIMEOUT; break; }
  }
  HAL_GPIO_WritePin(_485DE_RE_GPIO_Port, _485DE_RE_Pin, GPIO_PIN_RESET);
  return tx;
}

#endif
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  ValvePort_EmergencyOff();
  CommPort_EmergencyOff();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
