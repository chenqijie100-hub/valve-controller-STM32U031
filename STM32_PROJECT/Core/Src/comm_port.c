#include "main.h"
#include "comm_port.h"

#define UART_ERRORS (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE)
#define UART_IRQS (USART_CR1_TXEIE_TXFNFIE | USART_CR1_TCIE | \
                   USART_CR1_RXNEIE_RXFNEIE | USART_CR1_PEIE)

static uint8_t tx_frame[MB_REQUEST_SIZE];
static volatile uint8_t rx_frame[MB_RESPONSE_SIZE];
static volatile uint8_t tx_index, sending, tx_done, rx_length, rx_bad;
static volatile uint16_t last_bus;
static volatile uint8_t arming;
static uint16_t de_since;

static uint16_t clock_us(void) { return (uint16_t)TIM3->CNT; }
static uint16_t silence(void) { return (uint16_t)(clock_us() - last_bus); }
static void receive_direction(void) { _485DE_RE_GPIO_Port->BRR = _485DE_RE_Pin; }

void CommPort_Enable(void)
{
  /* HSI16 HCLK/PCLK (also after Stop 1). TIM2 remains the HAL timebase. */
  __HAL_RCC_TIM3_CLK_ENABLE();
  TIM3->PSC = HAL_RCC_GetPCLK1Freq() / 1000000U - 1U;
  TIM3->ARR = UINT16_MAX;
  TIM3->EGR = TIM_EGR_UG;
  TIM3->CR1 = TIM_CR1_CEN;
  sending = tx_done = arming = rx_length = rx_bad = 0;
  last_bus = clock_us();
  USART1->RQR = USART_RQR_RXFRQ;
  USART1->ICR = UART_ERRORS;
  USART1->CR3 |= USART_CR3_EIE;
  USART1->CR1 |= USART_CR1_RXNEIE_RXFNEIE | USART_CR1_PEIE;
}

void CommPort_Disable(void)
{
  USART1->CR1 &= ~UART_IRQS;
  USART1->CR3 &= ~USART_CR3_EIE;
  receive_direction();
  USART1->RQR = USART_RQR_TXFRQ | USART_RQR_RXFRQ;
  USART1->ICR = UART_ERRORS;
  TIM3->CR1 = 0;
  __HAL_RCC_TIM3_CLK_DISABLE();
}

uint8_t CommPort_TrySend(const uint8_t *request)
{
  uint8_t accepted = 0;
  uint32_t mask = __get_PRIMASK();
  __disable_irq();
  if (!arming && silence() >= MB_T35_US &&
      !(USART1->ISR & (USART_ISR_BUSY | USART_ISR_RXNE_RXFNE | UART_ERRORS)))
  {
    _485DE_RE_GPIO_Port->BSRR = _485DE_RE_Pin;
    de_since = clock_us();
    arming = 1;
  }
  if (arming && (uint16_t)(clock_us() - de_since) >= MB_DE_SETUP_US)
  {
    for (unsigned i = 0; i < MB_REQUEST_SIZE; ++i) tx_frame[i] = request[i];
    rx_length = rx_bad = tx_index = tx_done = 0;
    sending = 1;
    arming = 0;
    USART1->CR1 |= USART_CR1_TXEIE_TXFNFIE;
    accepted = 1;
  }
  __set_PRIMASK(mask);
  return accepted;
}

void CommPort_IRQ(void)
{
  uint32_t flags = USART1->ISR;
  uint32_t enables = USART1->CR1;
  if (flags & (USART_ISR_RXNE_RXFNE | UART_ERRORS))
  {
    uint8_t byte = (uint8_t)USART1->RDR;
    uint16_t interval = silence();
    last_bus = clock_us();
    USART1->ICR = flags & UART_ERRORS;
    if (!sending && !arming)
    {
      if ((flags & UART_ERRORS) || rx_length == MB_RESPONSE_SIZE ||
          (rx_length && interval > MB_RX_MAX_INTERVAL_US))
        rx_bad = 1;
      else rx_frame[rx_length++] = byte;
    }
  }
  if ((enables & USART_CR1_TXEIE_TXFNFIE) && (flags & USART_ISR_TXE_TXFNF))
  {
    USART1->TDR = tx_frame[tx_index++];
    if (tx_index == MB_REQUEST_SIZE)
    {
      USART1->CR1 = (USART1->CR1 & ~USART_CR1_TXEIE_TXFNFIE) | USART_CR1_TCIE;
    }
  }
  else if ((enables & USART_CR1_TCIE) && (flags & USART_ISR_TC))
  {
    receive_direction();
    USART1->CR1 &= ~USART_CR1_TCIE;
    last_bus = clock_us();
    sending = 0;
    tx_done = 1;
  }
}

uint8_t CommPort_TxDone(void) { return tx_done; }

int CommPort_Receive(uint8_t *reply)
{
  int length = 0;
  uint32_t mask = __get_PRIMASK();
  __disable_irq();
  if ((rx_length || rx_bad) && silence() >= MB_T35_US &&
      !(USART1->ISR & (USART_ISR_BUSY | USART_ISR_RXNE_RXFNE | UART_ERRORS)))
  {
    length = rx_bad ? -1 : rx_length;
    for (int i = 0; i < length; ++i) reply[i] = rx_frame[i];
    rx_length = rx_bad = 0;
  }
  __set_PRIMASK(mask);
  return length;
}
