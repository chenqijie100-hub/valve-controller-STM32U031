#include "main.h"
#include "power_rtc.h"
#define CHECK(x) do { if(!(x))return __LINE__; } while(0)
unsigned tests_passed;
RCC_TypeDef mock_rcc;
MockRTC mock_rtc;
/* run_phase1.py injects asynchronous register changes during CPU reads. */
volatile uint32_t rtc_inject_mode,rtc_inject_reads;
static uint32_t bcd(unsigned n) { return (n/10U)*16U+n%10U; }
static uint32_t date(unsigned y,unsigned m,unsigned d)
{ return bcd(y)<<16|bcd(m)<<8|bcd(d); }
int run_tests(void)
{
 uint64_t t=0,u=0;
 CHECK(PowerRtc_Decode(0,date(0,1,1),255,&t) && t==0);tests_passed++;
 CHECK(PowerRtc_Decode(0,date(0,1,1),0,&t) && t==255);tests_passed++;
 CHECK(PowerRtc_Decode(0x235959,date(24,2,28),0,&t));
 CHECK(PowerRtc_Decode(0,date(24,2,29),255,&u) && u-t==1);tests_passed++;
 CHECK(PowerRtc_Decode(0x235959,date(24,12,31),0,&t));
 CHECK(PowerRtc_Decode(0,date(25,1,1),255,&u) && u-t==1);tests_passed++;
 CHECK(PowerRtc_Decode(0,date(0,3,1),255,&t) && t==60ULL*86400*256);tests_passed++;
 CHECK(!PowerRtc_Decode(0,date(23,2,29),255,&t));tests_passed++;
 CHECK(!PowerRtc_Decode(0,date(24,4,31),255,&t));tests_passed++;
 CHECK(!PowerRtc_Decode(0,date(24,0,1),255,&t));tests_passed++;
 CHECK(!PowerRtc_Decode(0x240000,date(24,1,1),255,&t));tests_passed++;
 CHECK(!PowerRtc_Decode(0x006000,date(24,1,1),255,&t));tests_passed++;
 CHECK(!PowerRtc_Decode(0x00005A,date(24,1,1),255,&t));tests_passed++;
 CHECK(!PowerRtc_Decode(0,date(24,1,1),256,&t));tests_passed++;
 CHECK(!PowerRtc_Init());tests_passed++;
 RCC->BDCR=RCC_BDCR_LSERDY|RCC_BDCR_RTCEN|RCC_BDCR_RTCSEL_0;
 RCC->APBENR1=RCC_APBENR1_RTCAPBEN;
 RTC->PRER=(127U<<16)|255U;RTC->DR=date(24,10,29);RTC->SSR=255;
 CHECK(PowerRtc_Init() && (RTC->CR&RTC_CR_BYPSHAD) && RTC->WPR==255);tests_passed++;
 CHECK(PowerRtc_Now(&t));RTC->SSR=128;CHECK(PowerRtc_Now(&u) && u-t==127);tests_passed++;
 rtc_inject_mode=1;rtc_inject_reads=0;
 CHECK(PowerRtc_Now(&u) && u-t==128 && rtc_inject_reads>=4);tests_passed++;
 rtc_inject_mode=2;rtc_inject_reads=0;
 CHECK(!PowerRtc_Now(&u) && rtc_inject_reads==8);tests_passed++;
 rtc_inject_mode=0;
 RTC->ICSR=RTC_ICSR_INIT;CHECK(!PowerRtc_Now(&t));RTC->ICSR=0;tests_passed++;
 RTC->PRER=0;CHECK(!PowerRtc_Now(&t));tests_passed++;
 return 0;
}
