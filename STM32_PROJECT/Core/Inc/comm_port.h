#ifndef COMM_PORT_H
#define COMM_PORT_H
#include "modbus_rtu_master.h"

void CommPower_Init(void);
void CommPower_Set(uint8_t enabled);
uint8_t CommPower_Fault(void);
void CommPort_EmergencyOff(void);
void CommPort_Enable(void);
void CommPort_Disable(void);
void CommPort_IRQ(void);
uint8_t CommPort_TrySend(const uint8_t *request);
uint8_t CommPort_TxDone(void);
/* 0 waiting, -1 invalid UART/framing, positive frame byte count. */
int CommPort_Receive(uint8_t *reply);
#endif
