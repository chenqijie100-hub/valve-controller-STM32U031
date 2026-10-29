#include "power_rtc.h"
#include "main.h"
static uint8_t bcd(uint32_t value,uint32_t *result)
{
  if((value&15U)>9U || (value>>4)>9U)return 0;
  *result=(value>>4)*10U+(value&15U);return 1;
}
uint8_t PowerRtc_Decode(uint32_t tr,uint32_t dr,uint32_t ss,uint64_t *out)
{
  static const uint8_t lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
  uint32_t y,m,d,h,min,s;
  if(ss>255U || !bcd((dr>>16)&255U,&y) || !bcd((dr>>8)&31U,&m) ||
     !bcd(dr&63U,&d) || !bcd((tr>>16)&63U,&h) ||
     !bcd((tr>>8)&127U,&min) || !bcd(tr&127U,&s) ||
     !m || m>12 || !d || h>23 || min>59 || s>59)return 0;
  uint32_t leap=(y%4U)==0;
  if(d>(uint32_t)lengths[m-1]+(m==2U?leap:0U))return 0;
  /* RTC's 00..99 calendar represents 2000..2099, including leap year2000. */
  uint32_t days=y*365U+(y+3U)/4U+d-1U;
  for(uint32_t i=1;i<m;++i)days+=lengths[i-1]+(i==2U?leap:0U);
  *out=((uint64_t)days*86400U+h*3600U+min*60U+s)*256U+255U-ss;
  return 1;
}
static uint8_t configured(void)
{
  return (RCC->BDCR&(RCC_BDCR_RTCEN|RCC_BDCR_LSERDY|RCC_BDCR_RTCSEL))==
      (RCC_BDCR_RTCEN|RCC_BDCR_LSERDY|RCC_BDCR_RTCSEL_0) &&
      (RCC->APBENR1&RCC_APBENR1_RTCAPBEN) && RTC->PRER==((127U<<16)|255U) &&
      !(RTC->ICSR&(RTC_ICSR_INIT|RTC_ICSR_BIN)) && !(RTC->CR&RTC_CR_FMT);
}
uint8_t PowerRtc_Init(void)
{
  if(!configured())return 0;
  /* Same write-protection sequence as HAL_RTCEx_EnableBypassShadow.
   * Bypass avoids stale shadow registers after Stop; use bounded double reads.
   * No calendar, prescaler, alarm or wakeup timer configuration is changed. */
  RTC->WPR=0xCAU;RTC->WPR=0x53U;RTC->CR|=RTC_CR_BYPSHAD;RTC->WPR=0xFFU;
  uint64_t stamp;
  return PowerRtc_Now(&stamp);
}
uint8_t PowerRtc_Now(uint64_t *out)
{
  if(!configured() || !(RTC->CR&RTC_CR_BYPSHAD))return 0;
  for(unsigned attempt=0;attempt<4;++attempt)
  {
    uint32_t ss=RTC->SSR,tr=RTC->TR,dr=RTC->DR;
    /* No rollover or calendar change may straddle the register snapshot. */
    uint32_t ss2=RTC->SSR,tr2=RTC->TR,dr2=RTC->DR;
    if(ss==ss2 && tr==tr2 && dr==dr2)return PowerRtc_Decode(tr,dr,ss,out);
  }
  return 0; /* Never wait for RTC using the stopped HAL timebase. */
}
