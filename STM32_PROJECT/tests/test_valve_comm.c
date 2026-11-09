#include "valve_comm_service.h"
#include "comm_port.h"

unsigned tests_passed;
#define CHECK(condition) do { if (!(condition)) return __LINE__; ++tests_passed; } while (0)
static unsigned power_cycles, sends;
static uint8_t power_on, port_on, flt, tx_done, reply_ready;
static uint8_t packet[MB_REQUEST_SIZE];

void CommPower_Set(uint8_t enabled) { power_on = enabled; power_cycles += enabled; }
uint8_t CommPower_Fault(void) { return flt; }
void CommPort_Enable(void) { port_on = 1; }
void CommPort_Disable(void) { port_on = reply_ready = 0; }
uint8_t CommPort_TrySend(const uint8_t *request)
{
  if (!power_on || !port_on) return 0;
  ++sends;
  for (unsigned i = 0; i < sizeof(packet); ++i) packet[i] = request[i];
  return 1;
}
uint8_t CommPort_TxDone(void) { return tx_done; }
int CommPort_Receive(uint8_t *reply)
{
  if (!reply_ready) return 0;
  reply_ready = 0;
  for (unsigned i = 0; i < 6; ++i) reply[i] = packet[i];
  uint16_t crc = Modbus_Crc(reply, 6);
  reply[6] = (uint8_t)crc;
  reply[7] = (uint8_t)(crc >> 8);
  return 8;
}

int run_tests(void)
{
  AppStatus s = {.valve.state = VALVE_CLOSED};
  const CommStatus *cs = ValveComm_Get();
  ValveComm_Init();
  ValveComm_Service(0, &s, 0);
  CHECK(!power_on && !ValveComm_Busy() && cs->pending_events);
  s.battery.valid = 1;
  s.battery.millivolts = MB_MIN_BATTERY_MV - 1;
  ValveComm_Service(1, &s, 0);
  CHECK(!power_on);
  s.battery.millivolts = 7200;
  ValveComm_Service(10, &s, 1);
  CHECK(!power_on);
  ValveComm_Service(10, &s, 0);
  CHECK(power_on && ValveComm_Busy());
  flt = 1;
  ValveComm_Service(509, &s, 0);
  CHECK(power_on && !sends);
  flt = 0;
  ValveComm_Service(510, &s, 0);
  ValveComm_Service(511, &s, 0);
  CHECK(sends == 1 && cs->state == COMM_WAIT_TX);
  ValveComm_Service(512, &s, 0);
  CHECK(cs->state == COMM_WAIT_TX);
  tx_done = 1;
  ValveComm_Service(513, &s, 0);
  CHECK(cs->state == COMM_WAIT_RESPONSE);
  uint32_t first = cs->sequence;
  ValveComm_RequestPeriodic();
  reply_ready = 1;
  ValveComm_Service(520, &s, 0);
  CHECK(cs->acknowledged == first && (cs->pending_events & MB_EVENT_PERIODIC));
  CHECK(!power_on && !port_on && ValveComm_Busy());
  ValveComm_Service(1019, &s, 0);
  CHECK(!power_on);
  ValveComm_Service(1020, &s, 0);
  CHECK(power_on);
  ValveComm_Service(1520, &s, 0);
  ValveComm_Service(1521, &s, 0);
  ValveComm_Service(1522, &s, 0);
  s.inputs.wire_break = 1;
  s.valve.safety_close_pending = 1;
  ValveComm_Service(1523, &s, 0);
  CHECK(!power_on && !port_on && cs->result == MB_PREEMPTED);
  CHECK(cs->pending_events & MB_EVENT_WIRE);
  ValveComm_Service(3000, &s, 1);
  CHECK(!power_on);

  s.valve.safety_close_pending = 0;
  ValveComm_Service(3001, &s, 0);
  ValveComm_Service(3501, &s, 0);
  ValveComm_Service(3502, &s, 0);
  ValveComm_Service(3503, &s, 0);
  unsigned first_send = sends;
  for (unsigned i = 0; i <= MB_RETRIES; ++i)
  {
    uint32_t timeout = 3803 + i * 302;
    ValveComm_Service(timeout, &s, 0);
    if (i < MB_RETRIES)
    {
      ValveComm_Service(timeout + 1, &s, 0);
      ValveComm_Service(timeout + 2, &s, 0);
    }
  }
  CHECK(!power_on && !ValveComm_Busy() && cs->result == MB_TIMEOUT);
  CHECK(sends == first_send + MB_RETRIES && cs->pending_events);
  unsigned cycles = power_cycles;
  ValveComm_Service(6000, &s, 0);
  CHECK(power_cycles == cycles);
  ValveComm_RequestPeriodic();
  ValveComm_Service(6001, &s, 0);
  flt = 1;
  ValveComm_Service(6501, &s, 0);
  CHECK(!power_on && !ValveComm_Busy() && cs->power_fault && cs->result == MB_POWER_FAULT);
  ValveComm_Service(8000, &s, 0);
  CHECK(!power_on && power_cycles == cycles + 1);
  flt = 0;
  ValveComm_RequestPeriodic();
  ValveComm_Service(8001, &s, 0);
  ValveComm_Service(8501, &s, 0);
  ValveComm_Service(8502, &s, 0);
  CHECK(packet[14] & MB_FAULT_9V);
  ValveComm_Service(8503, &s, 0);
  reply_ready = 1;
  ValveComm_Service(8504, &s, 0);
  CHECK(!power_on && cs->pending_events == MB_EVENT_FAULT && !cs->power_fault);
  ValveComm_Service(9004, &s, 0);
  ValveComm_Service(9504, &s, 0);
  ValveComm_Service(9505, &s, 0);
  ValveComm_Service(9506, &s, 0);
  reply_ready = 1;
  ValveComm_Service(9507, &s, 0);
  CHECK(!power_on && !ValveComm_Busy() && !cs->pending_events && cs->result == MB_OK);
  ValveComm_RequestPeriodic();
  ValveComm_Service(10007, &s, 0);
  CHECK(power_on);
  s.battery.millivolts = MB_MIN_BATTERY_MV - 1;
  ValveComm_Service(10008, &s, 0);
  CHECK(!power_on && !ValveComm_Busy() && cs->battery_low &&
        cs->result == MB_BATTERY_UNAVAILABLE);
  s.battery.millivolts = 7200;
  ValveComm_Service(12000, &s, 0);
  CHECK(!power_on);
  ValveComm_RequestPeriodic();
  ValveComm_Service(12001, &s, 0);
  CHECK(power_on);
  ValveComm_Service(12501, &s, 0);
  ValveComm_Service(12502, &s, 0);
  CHECK(packet[14] & MB_FAULT_BATTERY);
  flt = 1;
  ValveComm_Service(12503, &s, 0);
  CHECK(!power_on && !port_on && cs->power_fault && cs->result == MB_POWER_FAULT);
  return 0;
}
