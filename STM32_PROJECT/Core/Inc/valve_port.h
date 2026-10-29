#ifndef VALVE_PORT_H
#define VALVE_PORT_H
#include "valve_control.h"
typedef enum { VALVE_DRIVE_OFF, VALVE_DRIVE_REFERENCE,
               VALVE_DRIVE_WAKE, VALVE_DRIVE_RUN } ValveDriveStage;
typedef enum { VALVE_PORT_OK, VALVE_PORT_BREAK, VALVE_PORT_LIMIT,
               VALVE_PORT_DRIVER, VALVE_PORT_ERROR } ValvePortResult;
ValvePortResult ValvePort_Apply(ValveDriveStage stage, ValveDirection direction, uint32_t duty);
/* The following functions are STM32 adapter only (not used by portable core). */
void ValvePort_Init(void);
void ValvePort_Read(ValveInputs *inputs);
void ValvePort_WireBreakISR(void);
void ValvePort_EmergencyOff(void);
uint32_t ValvePort_Duty(void);
uint8_t ValvePort_DriverAwake(void);
uint8_t ValvePort_EventPending(void);
#endif
