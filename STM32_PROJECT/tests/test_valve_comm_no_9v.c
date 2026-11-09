#include "valve_comm_service.h"
#include "comm_port.h"

unsigned tests_passed;
#define CHECK(condition) do { if (!(condition)) return __LINE__; ++tests_passed; } while (0)
static unsigned sends;
static uint8_t port_enabled, reply_ready;
static uint8_t echo[6];

/* Deliberately no CommPower_* symbols. This profile must link without them. */
void CommPort_Enable(void) { port_enabled = 1; }
void CommPort_Disable(void) { port_enabled = 0; }
uint8_t CommPort_TrySend(const uint8_t *request)
{
  if (!port_enabled) return 0;
  for (unsigned i = 0; i < sizeof(echo); ++i) echo[i] = request[i];
  ++sends;
  return 1;
}
uint8_t CommPort_TxDone(void) { return 1; }
int CommPort_Receive(uint8_t *reply)
{
  if (!reply_ready) return 0;
  for (unsigned i = 0; i < sizeof(echo); ++i) reply[i] = echo[i];
  uint16_t crc = Modbus_Crc(reply, sizeof(echo));
  reply[6] = (uint8_t)crc;
  reply[7] = (uint8_t)(crc >> 8);
  return 8;
}

int run_tests(void)
{
  AppStatus snapshot = {.valve.state = VALVE_CLOSED,
                        .battery = {.millivolts = 7200, .valid = 1}};
  ValveComm_Init();
  ValveComm_Service(10, &snapshot, 0);
  ValveComm_Service(11, &snapshot, 0);
  ValveComm_Service(12, &snapshot, 0);
  CHECK(sends == 1 && ValveComm_Busy());
  ValveComm_Service(13, &snapshot, 0);
  reply_ready = 1;
  ValveComm_Service(25, &snapshot, 0);
  CHECK(!port_enabled && !ValveComm_Busy() && ValveComm_Get()->result == MB_OK);
  ValveComm_RequestPeriodic();
  ValveComm_Service(26, &snapshot, 0);
  ValveComm_Service(27, &snapshot, 0);
  ValveComm_Service(28, &snapshot, 0);
  CHECK(sends == 2 && ValveComm_Busy());
  snapshot.valve.safety_close_pending = 1;
  ValveComm_Service(29, &snapshot, 0);
  CHECK(!port_enabled && ValveComm_Get()->result == MB_PREEMPTED &&
        ValveComm_Get()->pending_events);
  return 0;
}
