#ifndef MODBUS_PROTOCOL_H
#define MODBUS_PROTOCOL_H

/* V1.0 project mapping. Explicit opt-in after the slave has implemented it. */
#ifndef VALVE_MODBUS_ENABLE
#define VALVE_MODBUS_ENABLE 0
#endif
#ifndef VALVE_COMM_9V_ENABLE
#define VALVE_COMM_9V_ENABLE 0 /* PA1/PA4 power hardware is for the next board. */
#endif
#define MB_BAUD 19200U
#define MB_SLAVE 0x01U
#define MB_READ_HOLDING 0x03U /* Reserved */
#define MB_WRITE_SINGLE 0x06U /* Reserved */
#define MB_WRITE_MULTIPLE 0x10U
#define MB_VERSION 1U
#define MB_FIRST_REGISTER 0x0100U
enum {
  MB_REG_VERSION, MB_REG_POSITION, MB_REG_BATTERY_MV, MB_REG_FAULTS,
  MB_REG_EVENTS, MB_REG_SEQUENCE_HIGH, MB_REG_SEQUENCE_LOW, MB_REGISTER_COUNT
};
#define MB_REQUEST_SIZE (9U + 2U * MB_REGISTER_COUNT)
#define MB_RESPONSE_SIZE 8U
#define MB_RESPONSE_MS 300U
#define MB_RETRIES 2U
#define MB_POWER_SETTLE_MS 500U
#define MB_SEND_DEADLINE_MS 50U
#define MB_POWER_OFF_MIN_MS 500U /* Bench initial value, not a discharge guarantee. */
#ifndef MB_MIN_BATTERY_MV
#define MB_MIN_BATTERY_MV 6500U /* Bench-only 2S threshold; validate under load. */
#endif
#define MB_BITS_PER_CHAR 11U
#define MB_CHAR_US ((MB_BITS_PER_CHAR * 1000000U + MB_BAUD - 1U) / MB_BAUD)
#define MB_T35_US ((MB_BITS_PER_CHAR * 3500000U + MB_BAUD - 1U) / MB_BAUD)
/* RX IRQ timestamps the end of characters, so add one character to t1.5. */
#define MB_RX_MAX_INTERVAL_US ((MB_BITS_PER_CHAR * 2500000U + MB_BAUD - 1U) / MB_BAUD)
#define MB_DE_SETUP_US 10U

enum { MB_POS_UNKNOWN, MB_POS_OPEN, MB_POS_CLOSED, MB_POS_OPENING,
       MB_POS_CLOSING, MB_POS_MIDDLE, MB_POS_FAULT };
enum { MB_FAULT_WIRE = 1U, MB_FAULT_DRIVER = 2U, MB_FAULT_TIMEOUT = 4U,
       MB_FAULT_LIMITS = 8U, MB_FAULT_BATTERY = 16U, MB_FAULT_9V = 32U };
enum { MB_EVENT_START = 1U, MB_EVENT_POSITION = 2U, MB_EVENT_WIRE = 4U,
       MB_EVENT_FAULT = 8U, MB_EVENT_PERIODIC = 16U };

#endif
