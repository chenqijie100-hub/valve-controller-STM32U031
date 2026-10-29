#include "battery_monitor.h"
#include "battery_adc_port.h"
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
unsigned tests_passed;
void *memset(void *p,int v,unsigned n) { unsigned char *s=p; while(n--) *s++=(unsigned char)v; return p; }
static uint16_t cal, samples[3];
static uint8_t start_ok, stop_ok, analog_ready;
static unsigned index, starts;
static BatteryAdcResult read_result;
static uint32_t now;
static BatteryStatus out;
uint16_t BatteryAdc_Calibration(void) { return cal; }
uint8_t BatteryAdc_Ready(uint32_t tick) { (void)tick;return analog_ready; }
uint8_t BatteryAdc_Start(void) { index=0; starts++; return start_ok; }
BatteryAdcResult BatteryAdc_Read(uint16_t *v) { *v=samples[index < 3 ? index++ : 2]; return read_result; }
uint8_t BatteryAdc_Stop(void) { return stop_ok; }
static void reset(uint32_t tick)
{
  cal=1650; samples[0]=2787; samples[1]=512; samples[2]=1500;
  start_ok=stop_ok=analog_ready=1; read_result=BATTERY_ADC_READY; starts=0; now=tick;
  Battery_Init(now,1);
}
static void step(uint32_t ms) { now+=ms; Battery_Service(now); Battery_Get(now,&out); }
static void finish(void) { step(1); step(1); step(1); step(1); }
int run_tests(void)
{
  uint32_t vdda,mv;
  CHECK(Battery_Convert(2787,1500,1650,&vdda,&mv));
  CHECK(vdda==3300 && mv>=7199 && mv<=7201); tests_passed++;
  CHECK(Battery_Convert(3066,1650,1650,&vdda,&mv));
  CHECK(vdda==3000 && mv>=7199 && mv<=7202); tests_passed++;
  CHECK(!Battery_Convert(4095,1500,1650,&vdda,&mv));
  CHECK(!Battery_Convert(0,1500,1650,&vdda,&mv));
  CHECK(!Battery_Convert(2000,0,1650,&vdda,&mv));
  CHECK(!Battery_Convert(2000,4095,1650,&vdda,&mv));
  CHECK(!Battery_Convert(2000,1500,0xffff,&vdda,&mv)); tests_passed++;
  CHECK(!Battery_Convert(2000,1,1650,&vdda,&mv));
  CHECK(!Battery_Convert(2000,4000,1650,&vdda,&mv)); tests_passed++;
  reset(0); Battery_Get(now,&out); CHECK(!out.valid && !out.has_value);
  CHECK(!Battery_CanStop());
  CHECK(Battery_SleepBudget(now)==500);tests_passed++;
  step(499); CHECK(starts==0); step(1); CHECK(starts==1); finish();
  CHECK(Battery_SleepBudget(now)==250);tests_passed++;
  CHECK(out.valid && out.has_value && out.sequence==1 && out.motor_raw==512);
  CHECK(out.updated_ms==now && out.age_ms==0); tests_passed++;
  CHECK(Battery_CanStop());tests_passed++;
  uint32_t first=out.millivolts, updated=out.updated_ms;
  step(249); CHECK(starts==1); samples[0]=2900; step(1); finish();
  CHECK(out.sequence==2 && out.millivolts>first && out.millivolts<out.unfiltered_mv); tests_passed++;
  updated=out.updated_ms; uint32_t good=out.millivolts;
  read_result=BATTERY_ADC_WAIT; step(250); step(19);
  CHECK(Battery_SleepBudget(now)==0);tests_passed++;
  CHECK(out.updated_ms==updated); step(1); step(1);
  CHECK(!out.valid && out.has_value && out.result==BATTERY_TIMEOUT);
  CHECK(out.millivolts==good && out.updated_ms==updated && out.sequence==2); tests_passed++;
  read_result=BATTERY_ADC_READY; samples[0]=2787; step(250); finish();
  CHECK(out.valid && out.sequence==3 && out.millivolts==first); tests_passed++;
  Battery_Get(now+BATTERY_MAX_AGE_MS,&out); CHECK(out.valid);
  Battery_Get(now+BATTERY_MAX_AGE_MS+1,&out); CHECK(!out.valid && out.has_value); tests_passed++;
  /* A failed final conversion never publishes the partial first two ranks. */
  step(250); step(1); step(1); read_result=BATTERY_ADC_ERROR; step(1); step(1);
  CHECK(!out.valid && out.sequence==3 && out.result==BATTERY_READ_ERROR); tests_passed++;
  reset(0); start_ok=0; step(500); step(1);
  CHECK(!out.has_value && !out.valid && out.result==BATTERY_START_ERROR); tests_passed++;
  reset(0); step(500); stop_ok=0; finish();
  CHECK(!out.valid && out.result==BATTERY_STOP_ERROR);
  unsigned failed_starts=starts; step(5000); CHECK(starts==failed_starts); tests_passed++;
  reset(0); samples[2]=0; step(500); finish();
  CHECK(out.result==BATTERY_RANGE_ERROR && !out.valid); tests_passed++;
  reset(0); samples[1]=4096; step(500); finish();
  CHECK(out.result==BATTERY_RANGE_ERROR && !out.valid); tests_passed++;
  reset(0); Battery_Init(0,0); step(1000);
  CHECK(out.result==BATTERY_CAL_ERROR && starts==0 && !out.valid); tests_passed++;
  reset(0); cal=0; Battery_Init(0,1); step(1000);
  CHECK(out.result==BATTERY_CAL_ERROR && starts==0); tests_passed++;
  reset(0xfffffff0U); step(500); finish();
  CHECK(out.valid && out.age_ms==0); Battery_Get(now+20,&out); CHECK(out.age_ms==20); tests_passed++;
  reset(0);step(500);finish();updated=out.updated_ms;good=out.millivolts;
  now+=60000;Battery_AfterStop(now);Battery_Get(now,&out);
  CHECK(!Battery_CanStop());
  CHECK(!out.valid && out.has_value && out.millivolts==good && out.updated_ms==updated);
  CHECK(out.age_ms==60000 && Battery_SleepBudget(now)==0);tests_passed++;
  samples[0]=2900;step(0);finish();
  CHECK(out.valid && out.millivolts==out.unfiltered_mv && out.sequence==2);tests_passed++;
  reset(0);analog_ready=0;step(500);step(100);
  CHECK(starts==0 && out.failures==0 && out.result==BATTERY_NOT_READY);tests_passed++;
  analog_ready=1;step(0);finish();CHECK(out.valid && out.failures==0 && starts==1);tests_passed++;
  return 0;
}
