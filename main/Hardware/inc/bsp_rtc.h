#ifndef __BSP_RTC_H
#define __BSP_RTC_H

#include "./SYSTEM/sys/sys.h"

typedef struct
{
    uint16_t year;
    uint8_t  month;
    uint8_t  data;
    uint8_t  week;
    uint8_t  hour;
    uint8_t  min;
    uint8_t  sec;
} rtc_time_t;

uint8_t bsp_InitRTC(void);                                        // RTC初始化
ErrStatus RTC_Set_Time(rtc_time_t rtc);    // RTC时间设置    

void RTC_Set_AlarmA(uint8_t week,uint8_t hour,uint8_t min,uint8_t sec);            // 设置闹钟时间(按星期闹铃,24小时制)
void RTC_Set_WakeUp(uint32_t wksel,uint16_t cnt);                        // 周期性唤醒定时器设置
void RTC_Get_Time(rtc_time_t *rtc);
void RTC_set_Time(rtc_time_t rtc);
void TimeBySecond(uint32_t second);
void RTC_AlarmSet(void);
uint8_t DecToBcd(uint8_t dec);
void local_to_utc_time(rtc_time_t *utc_time, int8_t timezone, rtc_time_t local_time);

void rtc_test(void);

#endif

/******************************************  (END OF FILE) **********************************************/



