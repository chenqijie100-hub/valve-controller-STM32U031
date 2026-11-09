#ifndef MODBUS_RTU_MASTER_H
#define MODBUS_RTU_MASTER_H
#include <stdint.h>
#include "modbus_protocol.h"

typedef enum {
  MB_OK, MB_LENGTH, MB_CRC, MB_ADDRESS, MB_FUNCTION, MB_ECHO,
  MB_EXCEPTION, MB_TIMEOUT, MB_TRANSPORT, MB_POWER_FAULT, MB_PREEMPTED,
  MB_BATTERY_UNAVAILABLE
} ModbusResult;
uint16_t Modbus_Crc(const uint8_t *data, unsigned length);
void Modbus_Encode(const uint16_t *registers, uint8_t *frame);
ModbusResult Modbus_Check(const uint8_t *frame, unsigned length, uint8_t *exception);
#endif
