#include "modbus_rtu_master.h"

unsigned tests_passed;
static uint16_t holding[MB_REGISTER_COUNT];
#define CHECK(condition) do { if (!(condition)) return __LINE__; ++tests_passed; } while (0)

/* Simulated slave validates the V1.0 request and echoes address/count. */
static int slave(const uint8_t *request, uint8_t *reply)
{
  if (request[0] != 1 || request[1] != 0x10 || request[2] != 1 ||
      request[3] != 0 || request[4] != 0 || request[5] != 7 ||
      request[6] != 14 || request[7] != 0 || request[8] != 1 ||
      Modbus_Crc(request, 23) != 0) return 0;
  for (unsigned i = 0; i < MB_REGISTER_COUNT; ++i)
    holding[i] = (uint16_t)((request[7 + 2 * i] << 8) | request[8 + 2 * i]);
  for (unsigned i = 0; i < 6; ++i) reply[i] = request[i];
  uint16_t crc = Modbus_Crc(reply, 6);
  reply[6] = (uint8_t)crc;
  reply[7] = (uint8_t)(crc >> 8);
  return 1;
}

int run_tests(void)
{
  uint16_t registers[MB_REGISTER_COUNT] = {1, 2, 7200, 0, 1, 0x1234, 0x5678};
  uint8_t request[MB_REQUEST_SIZE], reply[8], exception = 0;
  CHECK(Modbus_Crc((const uint8_t *)"123456789", 9) == 0x4b37);
  Modbus_Encode(registers, request);
  CHECK(request[11] == 0x1c && request[12] == 0x20);
  CHECK(request[17] == 0x12 && request[20] == 0x78);
  CHECK(slave(request, reply) && holding[MB_REG_BATTERY_MV] == 7200 &&
        holding[MB_REG_SEQUENCE_LOW] == 0x5678);
  CHECK(Modbus_Check(reply, 8, &exception) == MB_OK);
  reply[7] ^= 1;
  CHECK(Modbus_Check(reply, 8, &exception) == MB_CRC);
  CHECK(slave(request, reply) && reply[6] == 0x80 && reply[7] == 0x37);
  reply[0] = 2;
  uint16_t wrong_crc = Modbus_Crc(reply, 6);
  reply[6] = (uint8_t)wrong_crc;
  reply[7] = (uint8_t)(wrong_crc >> 8);
  CHECK(Modbus_Check(reply, 8, &exception) == MB_ADDRESS);
  reply[0] = 1;
  reply[1] = 3;
  wrong_crc = Modbus_Crc(reply, 6);
  reply[6] = (uint8_t)wrong_crc;
  reply[7] = (uint8_t)(wrong_crc >> 8);
  CHECK(Modbus_Check(reply, 8, &exception) == MB_FUNCTION);
  CHECK(slave(request, reply));
  reply[5] = 6;
  wrong_crc = Modbus_Crc(reply, 6);
  reply[6] = (uint8_t)wrong_crc;
  reply[7] = (uint8_t)(wrong_crc >> 8);
  CHECK(Modbus_Check(reply, 8, &exception) == MB_ECHO);
  uint8_t error[5] = {1, 0x90, 2};
  uint16_t crc = Modbus_Crc(error, 3);
  error[3] = (uint8_t)crc;
  error[4] = (uint8_t)(crc >> 8);
  CHECK(Modbus_Check(error, 5, &exception) == MB_EXCEPTION && exception == 2);
  return 0;
}
