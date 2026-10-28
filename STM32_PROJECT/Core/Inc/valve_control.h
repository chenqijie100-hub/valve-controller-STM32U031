#ifndef VALVE_CONTROL_H
#define VALVE_CONTROL_H
#include <stdint.h>
#include "valve_config.h"

typedef enum { VALVE_UNKNOWN, VALVE_OPENING, VALVE_OPENED,
               VALVE_CLOSING, VALVE_CLOSED, VALVE_FAULT } ValveState;
typedef enum { VALVE_DIR_NONE, VALVE_DIR_OPEN, VALVE_DIR_CLOSE } ValveDirection;
enum {
  VALVE_FAULT_WIRE = 1U << 0,
  VALVE_FAULT_TIMEOUT = 1U << 1, /* Reserved diagnostic bit; runtime timeout removed. */
  VALVE_FAULT_DRIVER = 1U << 2,
  VALVE_FAULT_BOTH_LIMITS = 1U << 3,
  VALVE_FAULT_SAFETY_INCOMPLETE = 1U << 4,
  VALVE_FAULT_START = 1U << 5
};
typedef struct {
  uint8_t open_limit, close_limit, wire_break, request_open, driver_fault, break_event;
} ValveInputs;
typedef struct {
  ValveState state;
  uint32_t current_faults, latched_faults, history_faults;
  uint8_t safety_close_pending, opening_permitted, digital_enabled;
} ValveStatus;

/* Main-context API only; all output writes are owned by the controller/port. */
void Valve_Init(uint32_t now);
void Valve_Step(uint32_t now, const ValveInputs *inputs);
const ValveStatus *Valve_Get(void);
uint8_t Valve_Request(ValveDirection direction, uint32_t jog_ms, uint32_t duty);
uint8_t Valve_Stop(uint32_t now);
uint8_t Valve_SetDigital(uint8_t enabled, uint32_t now);
uint8_t Valve_ClearFaults(uint32_t now, const ValveInputs *inputs);
const char *Valve_StateName(ValveState state);
#endif
