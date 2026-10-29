#ifndef POWER_PORT_H
#define POWER_PORT_H
#include <stdint.h>
uint8_t PowerPort_Init(void);
uint32_t PowerPort_Lock(void);
void PowerPort_Unlock(uint32_t mask);
uint8_t PowerPort_InInterrupt(void);
uint8_t PowerPort_Pending(void);
uint8_t PowerPort_OutputsSafe(void);
uint32_t PowerPort_GpioSignature(void);
/* Called with interrupts masked. Always restores SysTick before returning. */
uint8_t PowerPort_Sleep(uint32_t duration_ms);
void PowerPort_AlarmIRQ(void);
#endif
