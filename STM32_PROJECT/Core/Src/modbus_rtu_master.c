#include "modbus_rtu_master.h"

uint16_t Modbus_Crc(const uint8_t *data, unsigned length)
{
  uint16_t crc = 0xffffU;
  for (unsigned i = 0; i < length; ++i)
  {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc & 1U) ? (crc >> 1) ^ 0xa001U : crc >> 1;
  }
  return crc;
}

static void put_word(uint8_t *out, uint16_t value)
{
  out[0] = (uint8_t)(value >> 8);
  out[1] = (uint8_t)value;
}

void Modbus_Encode(const uint16_t *registers, uint8_t *frame)
{
  frame[0] = MB_SLAVE;
  frame[1] = MB_WRITE_MULTIPLE;
  put_word(frame + 2, MB_FIRST_REGISTER);
  put_word(frame + 4, MB_REGISTER_COUNT);
  frame[6] = 2U * MB_REGISTER_COUNT;
  for (unsigned i = 0; i < MB_REGISTER_COUNT; ++i)
    put_word(frame + 7 + 2 * i, registers[i]);
  uint16_t crc = Modbus_Crc(frame, MB_REQUEST_SIZE - 2);
  frame[MB_REQUEST_SIZE - 2] = (uint8_t)crc;
  frame[MB_REQUEST_SIZE - 1] = (uint8_t)(crc >> 8);
}

ModbusResult Modbus_Check(const uint8_t *frame, unsigned length, uint8_t *exception)
{
  *exception = 0;
  if (length < 5) return MB_LENGTH;
  if (Modbus_Crc(frame, length) != 0) return MB_CRC;
  if (frame[0] != MB_SLAVE) return MB_ADDRESS;
  if (frame[1] == (MB_WRITE_MULTIPLE | 0x80U))
  {
    if (length != 5) return MB_LENGTH;
    *exception = frame[2];
    return MB_EXCEPTION;
  }
  if (frame[1] != MB_WRITE_MULTIPLE) return MB_FUNCTION;
  if (length != MB_RESPONSE_SIZE) return MB_LENGTH;
  if (((frame[2] << 8) | frame[3]) != MB_FIRST_REGISTER ||
      ((frame[4] << 8) | frame[5]) != MB_REGISTER_COUNT) return MB_ECHO;
  return MB_OK;
}
