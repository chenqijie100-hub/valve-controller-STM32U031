#ifndef POWER_RTC_H
#define POWER_RTC_H
#include <stdint.h>
/* Read-only elapsed-time source in Stop1: never programs an RTC wake/alarm. */
uint8_t PowerRtc_Init(void);
uint8_t PowerRtc_Now(uint64_t *ticks256);
uint8_t PowerRtc_Decode(uint32_t time_bcd,uint32_t date_bcd,uint32_t ss,
                        uint64_t *ticks256);
#endif
