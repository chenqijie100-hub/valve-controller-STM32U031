#ifndef VALVE_COMM_SERVICE_H
#define VALVE_COMM_SERVICE_H
#include "app_status.h"
#include "modbus_rtu_master.h"

typedef enum { COMM_IDLE, COMM_PREPARE, COMM_TX_REQUEST, COMM_WAIT_TX,
               COMM_WAIT_RESPONSE, COMM_RETRY } CommState;
typedef struct {
  CommState state;
  ModbusResult result;
  uint32_t sequence, acknowledged;
  uint16_t pending_events;
  uint8_t exception, power_fault, battery_low;
} CommStatus;

void ValveComm_Init(void);
void ValveComm_Service(uint32_t now, const AppStatus *snapshot, uint8_t safety_busy);
/* Main context only. Future scheduler calls after a separately implemented wake. */
void ValveComm_RequestPeriodic(void);
uint8_t ValveComm_Busy(void);
const CommStatus *ValveComm_Get(void);
#endif
