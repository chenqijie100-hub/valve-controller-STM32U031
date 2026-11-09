#include "main.h"
#include "comm_port.h"

unsigned tests_passed;
#define CHECK(condition) do { if (!(condition)) return __LINE__; ++tests_passed; } while (0)
uint32_t HAL_RCC_GetPCLK1Freq(void) { return 16000000U; }

int run_tests(void)
{
  uint8_t request[MB_REQUEST_SIZE], reply[MB_RESPONSE_SIZE];
  for (unsigned i = 0; i < sizeof(request); ++i) request[i] = (uint8_t)(i + 1);
  CommPort_Enable();
  CHECK(TIM3->PSC == 15 && (TIM3->CR1 & TIM_CR1_CEN));
  TIM3->CNT = MB_T35_US - 1;
  CHECK(!CommPort_TrySend(request) && GPIOB->BSRR == 0);
  TIM3->CNT++;
  CHECK(!CommPort_TrySend(request) && GPIOB->BSRR == _485DE_RE_Pin);
  TIM3->CNT += MB_DE_SETUP_US;
  CHECK(CommPort_TrySend(request));
  for (unsigned i = 0; i < sizeof(request); ++i)
  {
    USART1->ISR = USART_ISR_TXE_TXFNF;
    CommPort_IRQ();
    CHECK(USART1->TDR == request[i]);
  }
  CHECK(!CommPort_TxDone() && GPIOB->BRR == 0 && (USART1->CR1 & USART_CR1_TCIE));
  USART1->ISR = USART_ISR_TC;
  CommPort_IRQ();
  CHECK(CommPort_TxDone() && GPIOB->BRR == _485DE_RE_Pin && !(USART1->CR1 & USART_CR1_TCIE));

  for (unsigned i = 0; i < sizeof(reply); ++i)
  {
    TIM3->CNT += MB_CHAR_US;
    USART1->ISR = USART_ISR_RXNE_RXFNE;
    USART1->RDR = i;
    CommPort_IRQ();
  }
  USART1->ISR = 0;
  TIM3->CNT += MB_T35_US - 1;
  CHECK(!CommPort_Receive(reply));
  TIM3->CNT++;
  CHECK(CommPort_Receive(reply) == MB_RESPONSE_SIZE && reply[7] == 7);
  USART1->ISR = USART_ISR_RXNE_RXFNE;
  CommPort_IRQ();
  TIM3->CNT += MB_RX_MAX_INTERVAL_US + 1;
  CommPort_IRQ();
  USART1->ISR = 0;
  TIM3->CNT += MB_T35_US;
  CHECK(CommPort_Receive(reply) < 0);
  CommPort_Disable();
  CHECK(!(USART1->CR1 & USART_CR1_RXNEIE_RXFNEIE) && !TIM3->CR1);
  return 0;
}
