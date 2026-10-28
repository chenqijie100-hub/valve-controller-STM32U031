#include "main.h"
typedef struct { uint32_t error; } UART_HandleTypeDef;
extern UART_HandleTypeDef huart1;
void MX_USART1_UART_Init(void);
HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *u, uint8_t *d, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *u, const uint8_t *d, uint16_t size, uint32_t timeout);
uint32_t HAL_UART_GetError(UART_HandleTypeDef *u);
#define HAL_UART_ERROR_NONE 0U
#define UART_CLEAR_OREF 1U
#define UART_CLEAR_NEF 2U
#define UART_CLEAR_FEF 4U
#define UART_CLEAR_PEF 8U
#define UART_RXDATA_FLUSH_REQUEST 1U
#define __HAL_UART_CLEAR_FLAG(u, f) ((void)(f), (u)->error = 0)
#define __HAL_UART_SEND_REQ(u, r) ((void)(u), (void)(r))
