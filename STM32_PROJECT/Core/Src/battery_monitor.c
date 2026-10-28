#include "battery_monitor.h"
#include "battery_adc_port.h"
static BatteryStatus status;
static enum { IDLE, READING, STOPPING, UNAVAILABLE } stage;
static uint32_t wait_tick, wait_ms, start_tick;
static uint16_t calibration, raw[3];
static uint8_t rank, count, position;
static uint32_t window[4], sum;
static BatteryResult attempt_result;

uint8_t Battery_Convert(uint16_t bat, uint16_t vref, uint16_t cal, uint32_t *vdda, uint32_t *mv)
{
  if (!bat || bat>=4095 || !vref || vref>=4095 || !cal || cal>=4095) return 0;
  /* Factory calibration is at 3000mV, not 3300mV (STM32U0 LL header).
   * One rounded division avoids adding VDDA intermediate rounding error. */
  *vdda=((uint32_t)cal*3000U+vref/2U)/vref;
  if (*vdda<BATTERY_VDDA_SANITY_MIN_MV || *vdda>BATTERY_VDDA_SANITY_MAX_MV) return 0;
  uint64_t denominator=(uint64_t)vref*4095U*680U;
  *mv=(uint32_t)(((uint64_t)bat*cal*3000U*2180U+denominator/2U)/denominator);
  return 1;
}

void Battery_Init(uint32_t now, uint8_t ok)
{
  status=(BatteryStatus){.result=BATTERY_NOT_READY};
  calibration=BatteryAdc_Calibration();
  rank=count=position=0; sum=0;
  wait_tick=now; wait_ms=BATTERY_STARTUP_SETTLE_MS; stage=IDLE;
  if (!ok || !calibration || calibration>=4095)
  { status.result=BATTERY_CAL_ERROR; stage=UNAVAILABLE; }
}

void Battery_Get(uint32_t now, BatteryStatus *out)
{
  *out=status;
  out->age_ms=status.has_value ? (uint32_t)(now-status.updated_ms) : UINT32_MAX;
  out->valid=status.has_value && status.result==BATTERY_OK && out->age_ms<=BATTERY_MAX_AGE_MS;
}

void Battery_Service(uint32_t now)
{
  if (stage==UNAVAILABLE) return;
  if (stage==IDLE)
  {
    if ((uint32_t)(now-wait_tick)<wait_ms) return;
    status.attempt_ms=now; start_tick=now; rank=0;
    attempt_result=BATTERY_OK;
    if (BatteryAdc_Start()) stage=READING;
    else { attempt_result=BATTERY_START_ERROR; stage=STOPPING; }
    return;
  }
  if (stage==READING)
  {
    if ((uint32_t)(now-start_tick)>=BATTERY_SEQUENCE_TIMEOUT_MS)
    { attempt_result=BATTERY_TIMEOUT; stage=STOPPING; return; }
    BatteryAdcResult result=BatteryAdc_Read(&raw[rank]);
    if (result==BATTERY_ADC_ERROR) { attempt_result=BATTERY_READ_ERROR; stage=STOPPING; }
    else if (result==BATTERY_ADC_READY && ++rank==3) stage=STOPPING;
    return; /* At most one conversion read per call; never spin for EOC. */
  }
  uint8_t stopped=BatteryAdc_Stop();
  if (!stopped) attempt_result=BATTERY_STOP_ERROR;
  uint32_t vdda=0,mv=0;
  if (attempt_result==BATTERY_OK && (raw[1]>4095 ||
      !Battery_Convert(raw[0],raw[2],calibration,&vdda,&mv))) attempt_result=BATTERY_RANGE_ERROR;
  status.result=attempt_result;
  if (attempt_result==BATTERY_OK)
  {
    if (count==4) sum-=window[position]; else count++;
    window[position]=mv; sum+=mv; position=(position+1U)%4U;
    status.millivolts=(sum+count/2U)/count;
    status.unfiltered_mv=mv; status.vdda_mv=vdda;
    status.bat_raw=raw[0]; status.motor_raw=raw[1]; status.vref_raw=raw[2];
    status.updated_ms=now; status.sequence++; status.has_value=1;
  }
  else
  {
    status.failures++;
    count=position=0; sum=0; /* Do not mix pre-failure samples into recovery. */
  }
  wait_tick=now; wait_ms=BATTERY_SAMPLE_PERIOD_MS;
  stage=stopped ? IDLE : UNAVAILABLE; /* Stuck hardware: no repeated blocking cleanup. */
}

const char *Battery_ResultName(BatteryResult r)
{
  switch (r) {
    case BATTERY_OK: return "OK";
    case BATTERY_CAL_ERROR: return "CAL_ERROR";
    case BATTERY_START_ERROR: return "START_ERROR";
    case BATTERY_TIMEOUT: return "TIMEOUT";
    case BATTERY_READ_ERROR: return "READ_ERROR";
    case BATTERY_STOP_ERROR: return "STOP_ERROR";
    case BATTERY_RANGE_ERROR: return "RANGE_ERROR";
    default: return "NOT_READY";
  }
}
