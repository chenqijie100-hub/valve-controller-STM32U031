#include "main.h"
#include "rs485_rx.h"
volatile uint32_t rs485_rx_irq_bytes, rs485_rx_hw_errors, rs485_rx_overflows;
#define RX_SIZE 128U
#define RX_ERRORS (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE)
static volatile uint8_t bytes[RX_SIZE];
static volatile uint32_t head, tail;
static volatile uint8_t damaged;
uint8_t RS485_RxBusy(void) { return damaged || head!=tail; }

void RS485_RxInit(void)
{
  uint32_t mask = __get_PRIMASK();
  __disable_irq();
  head = tail = 0; damaged = 0;
  if (USART1->ISR & USART_ISR_RXNE_RXFNE) (void)USART1->RDR;
  USART1->ICR = RX_ERRORS; /* Error bits and their W1C positions match. */
  USART1->CR3 |= USART_CR3_EIE;
  USART1->CR1 |= USART_CR1_RXNEIE_RXFNEIE | USART_CR1_PEIE;
  __set_PRIMASK(mask);
}

void RS485_RxIRQ(void)
{
  uint32_t flags = USART1->ISR;
  uint8_t byte = 0;
  if (flags & USART_ISR_RXNE_RXFNE)
  { byte = (uint8_t)USART1->RDR; rs485_rx_irq_bytes++; }
  if (flags & RX_ERRORS)
  {
    USART1->ICR = flags & RX_ERRORS;
    rs485_rx_hw_errors++; damaged = 1;
    return; /* Never execute a command containing a lost/corrupt byte. */
  }
  if (flags & USART_ISR_RXNE_RXFNE)
  {
    uint32_t next = (head + 1U) % RX_SIZE;
    if (next == tail) { rs485_rx_overflows++; damaged = 1; return; }
    bytes[head] = byte;
    head = next;
  }
  /* No HAL waits, motor operations or command parsing in interrupt context. */
}

int RS485_RxPoll(uint8_t *byte)
{
  int result = 0;
  uint32_t mask = __get_PRIMASK();
  __disable_irq();
  if (damaged)
  { tail = head; damaged = 0; result = -1; }
  else if (tail != head)
  { *byte = bytes[tail]; tail = (tail + 1U) % RX_SIZE; result = 1; }
  __set_PRIMASK(mask);
  return result;
}
