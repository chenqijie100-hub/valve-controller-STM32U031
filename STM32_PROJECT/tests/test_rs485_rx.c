#include "main.h"
#include "rs485_rx.h"
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
unsigned tests_passed;
MockUSART mock_usart1;
static uint32_t mask;
uint32_t __get_PRIMASK(void) { return mask; }
void __disable_irq(void) { mask = 1; }
void __set_PRIMASK(uint32_t value) { mask = value; }
static void receive(uint8_t byte, uint32_t errors)
{ USART1->RDR = byte; USART1->ISR = USART_ISR_RXNE_RXFNE | errors; RS485_RxIRQ(); }
int run_tests(void)
{
  uint8_t byte;
  RS485_RxInit();
  CHECK(USART1->CR1 & USART_CR1_RXNEIE_RXFNEIE);
  const char *ping = "PING\r\n";
  for (unsigned i = 0; i < 6; ++i) receive((uint8_t)ping[i], 0);
  CHECK(RS485_RxBusy());tests_passed++;
  for (unsigned i = 0; i < 6; ++i) { CHECK(RS485_RxPoll(&byte) == 1); CHECK(byte == (uint8_t)ping[i]); }
  CHECK(RS485_RxPoll(&byte) == 0); tests_passed++;
  CHECK(!RS485_RxBusy());tests_passed++;
  /* Full burst accumulated while main is busy; no per-byte main scheduling. */
  for (unsigned i = 0; i < 100; ++i) receive((uint8_t)i, 0);
  for (unsigned i = 0; i < 100; ++i) { CHECK(RS485_RxPoll(&byte) == 1); CHECK(byte == i); }
  tests_passed++;
  for (unsigned error = 1; error <= 8; error <<= 1)
  {
    receive('P', 0); receive('X', error);
    CHECK(USART1->ICR & error);
    CHECK(RS485_RxPoll(&byte) == -1); CHECK(RS485_RxPoll(&byte) == 0);
    receive('\n', 0); CHECK(RS485_RxPoll(&byte) == 1 && byte == '\n');
    tests_passed++;
  }
  for (unsigned i = 0; i < 200; ++i) receive('X', 0);
  CHECK(rs485_rx_overflows > 0 && RS485_RxPoll(&byte) == -1);
  receive('P', 0); CHECK(RS485_RxPoll(&byte) == 1 && byte == 'P'); tests_passed++;
  /* Producer/consumer wrap repeatedly, retaining order. */
  for (unsigned i = 0; i < 1000; ++i)
  { receive((uint8_t)i, 0); CHECK(RS485_RxPoll(&byte) == 1 && byte == (uint8_t)i); }
  tests_passed++;
  mask = 1; CHECK(RS485_RxPoll(&byte) == 0 && mask == 1); tests_passed++;
  RS485_RxAfterStop();CHECK(!RS485_RxBusy());
  receive('R',0);receive('U',0);receive('N',0);receive('A',0);
  CHECK(RS485_RxPoll(&byte)==0 && !RS485_RxBusy());tests_passed++;
  receive('\n',0);for(unsigned i=0;i<6;++i)receive((uint8_t)ping[i],0);
  for(unsigned i=0;i<6;++i) { CHECK(RS485_RxPoll(&byte)==1 && byte==(uint8_t)ping[i]); }
  tests_passed++;
  RS485_RxAfterStop();receive('X',USART_ISR_FE);CHECK(RS485_RxPoll(&byte)==0);
  receive('\n',0);receive('P',0);CHECK(RS485_RxPoll(&byte)==1 && byte=='P');tests_passed++;
  return 0;
}
