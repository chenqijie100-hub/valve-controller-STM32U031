#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H
#include <stdint.h>
#define BATTERY_STARTUP_SETTLE_MS 500U
#define BATTERY_SAMPLE_PERIOD_MS 250U
#define BATTERY_SEQUENCE_TIMEOUT_MS 20U
#define BATTERY_MAX_AGE_MS 1000U
/* Broad diagnostic plausibility for this 3.3V board, not a control threshold
 * or a statement of the MCU's electrical operating ratings. */
#define BATTERY_VDDA_SANITY_MIN_MV 1500U
#define BATTERY_VDDA_SANITY_MAX_MV 4000U
typedef enum {
  BATTERY_NOT_READY, BATTERY_OK, BATTERY_CAL_ERROR, BATTERY_START_ERROR,
  BATTERY_TIMEOUT, BATTERY_READ_ERROR, BATTERY_STOP_ERROR, BATTERY_RANGE_ERROR
} BatteryResult;
typedef struct {
  uint32_t millivolts, unfiltered_mv, vdda_mv;
  uint32_t updated_ms, attempt_ms, age_ms, sequence, failures;
  uint16_t bat_raw, motor_raw, vref_raw;
  uint8_t has_value, valid;
  BatteryResult result;
} BatteryStatus;
void Battery_Init(uint32_t now, uint8_t adc_calibrated);
void Battery_Service(uint32_t now);
void Battery_Get(uint32_t now, BatteryStatus *out);
/* Zero blocks sleep; otherwise milliseconds until next ADC work is due. */
uint32_t Battery_SleepBudget(uint32_t now);
uint8_t Battery_Convert(uint16_t bat, uint16_t vref, uint16_t cal,
                        uint32_t *vdda_mv, uint32_t *battery_mv);
const char *Battery_ResultName(BatteryResult result);
#endif
