#ifndef RS485_RX_H
#define RS485_RX_H
#include <stdint.h>
void RS485_RxInit(void);
void RS485_RxIRQ(void);
/* 1 = byte, 0 = empty, -1 = damaged stream; discard to next LF. */
int RS485_RxPoll(uint8_t *byte);
extern volatile uint32_t rs485_rx_irq_bytes, rs485_rx_hw_errors, rs485_rx_overflows;
#endif
