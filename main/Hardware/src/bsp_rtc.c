/********************************************************************************
* @File name  : RTC实时时钟驱动文件
* @Description: 基于STM32的RTC驱动，支持时间/日期设置、读取、闹钟配置及唤醒定时器功能
* @Author     : ZHLE
*  Version Date        Modification Description
********************************************************************************/
#include "bsp_rtc.h"
#include "bsp.h"
#include "time.h"
//#include "app.h"
//#include "gsm.h"

// 时钟源宏定义
// #define RTC_CLOCK_SOURCE_LXTAL      
#define RTC_CLOCK_SOURCE_IRC32K

// 分频因子 32768 = ASYNCHPREDIV *  SYNCHPREDIV
#ifdef RTC_CLOCK_SOURCE_IRC32K
#define ASYNCHPREDIV         0x63 // 异步分频因子
#define SYNCHPREDIV          0x13F // 同步分频因子
// 写入到备份寄存器的数据宏定义
#define RTC_BKP_DATA         0x3232
#else
#define ASYNCHPREDIV         0X7F
#define SYNCHPREDIV          0XFF
// 写入到备份寄存器的数据宏定义
#define RTC_BKP_DATA         0x32F0
#endif

// 闹钟相关宏定义
#define ALARM_HOURS               1   // 0~23
#define ALARM_MINUTES             00  // 0~59
#define ALARM_SECONDS             00  // 0~59


// 初始化默认时间
rtc_time_t default_rtc_time = {2023,12,25,2,11,48,0};

static rtc_parameter_struct rtc_initpara;
static rtc_alarm_struct  rtc_alarm;

// 声明内部函数：通过年月日计算星期
static uint8_t get_week_form_time(uint16_t year,uint8_t month,uint8_t day);
static uint8_t RTC_ByteToBcd2(uint8_t Value);
static uint8_t RTC_Bcd2ToByte(uint8_t Value);
/*
*********************************************************************************************************
*    函 数 名: bsp_InitRTC
*    功能说明: RTC初始化。
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
#define LSE_STARTUP_TIMEOUT     ((uint16_t)0x05000)  // LSE启动超时计数

uint8_t bsp_InitRTC(void)
{    
    /*使能 PWR 时钟*/
    rcu_periph_clock_enable(RCU_PMU);
    /* PWR_CR:DBF置1，使能RTC、RTC备份寄存器和备份SRAM的访问 */
    pmu_backup_write_enable();
    rcu_bkp_reset_enable();
    rcu_bkp_reset_disable();

#if defined (RTC_CLOCK_SOURCE_IRC32K) 
    /* 使用IRC32K作为RTC时钟源会有误差 */
    /* 使能IRC32K */ 
    rcu_osci_on(RCU_IRC32K);
    // 检查IRC32K就绪标志
    if(rcu_osci_stab_wait(RCU_IRC32K) == ERROR)
    {
        printf("IRC32K CLK INT ERROR!!! \r\n");
    }
    /* 选择IRC32K做为RTC的时钟源 */
    rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);

#elif defined (RTC_CLOCK_SOURCE_LXTAL)

    /* 使能LSE */ 
    rcu_osci_on(RCU_LXTAL);
    /* 等待LSE稳定 */   
    if(rcu_osci_stab_wait(RCU_LXTAL) == ERROR)
    {
        printf("LSE CLK INIT ERROR!!! \r\n");
    }

    /* 选择LSE做为RTC的时钟源 */
    rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);    

#endif /* */

    /* 使能RTC时钟 */
    rcu_periph_clock_enable(RCU_RTC);

    /* 等待 RTC APB 寄存器同步 */
    if(rtc_register_sync_wait() == ERROR)
    {
        printf("RTC SYNC ERROR!!!\r\n");
    }
    
    /* check if RTC has aready been configured */
    if(RTC_BKP_DATA != RTC_BKP0)
    {
        // 初始化默认时间（2:11:48 上午）
        RTC_Set_Time(default_rtc_time);
    }
    RTC_AlarmSet();  // 每天12:00:00触发
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: RTC_Set_Time
*    功能说明: 设置RTC时间（支持12小时制）
*    形    参: 
*    @hour        : 小时（12小时制：1-12；24小时制：0-23，需配合HourFormat）
*    @min        : 分钟（0-59）
*    @sec        : 秒钟（0-59）
*    @ampm        : 12小时制上下午标识 @ref RTC_AM_PM_Definitions：RTC_H12_AM（上午）/RTC_H12_PM（下午）
*    返 回 值:     ErrStatus枚举值
*                SUCCESS(1)：设置成功
*                ERROR(0)：设置失败（如RTC未进入初始化模式）
*********************************************************************************************************
*/
ErrStatus RTC_Set_Time(rtc_time_t rtc)
{    
    rtc_initpara.factor_asyn = ASYNCHPREDIV;/* 设置异步预分频器的值 */
    rtc_initpara.factor_syn = SYNCHPREDIV;/* 设置同步预分频器的值 */
    // 设置日期
    rtc.week = get_week_form_time(rtc.year,rtc.month,rtc.data);
    rtc_initpara.year = RTC_ByteToBcd2(rtc.year - 2000);
    rtc_initpara.day_of_week = RTC_ByteToBcd2(rtc.week);
    rtc_initpara.month = RTC_ByteToBcd2(rtc.month);
    rtc_initpara.date = RTC_ByteToBcd2(rtc.data);

    // 设置时间
    rtc_initpara.display_format = RTC_24HOUR; // 24小时制
    // 根据小时判断12小时制的上下午（小时>12为下午，否则为上午）
    if(rtc.hour > 12)
        rtc_initpara.am_pm = RTC_PM;
    else
        rtc_initpara.am_pm = RTC_AM;
    rtc_initpara.hour    = RTC_ByteToBcd2(rtc.hour);
    rtc_initpara.minute    = RTC_ByteToBcd2(rtc.min);      
    rtc_initpara.second    = RTC_ByteToBcd2(rtc.sec);      
    /* RTC current time configuration */
    if(ERROR == rtc_init(&rtc_initpara))
    {
        printf("\n\r** RTC time configuration failed! **\n\r");
        return ERROR;
    }
    else
    {
        printf("\n\r** RTC time configuration success! **\n\r");
        RTC_BKP0 = RTC_BKP_DATA;
        return SUCCESS;
    }
}

/* 月份天数对照表（用于计算星期） */                                         
const uint8_t cg_table_week[12]={0,3,3,6,1,4,6,2,5,0,3,5};
/*
*********************************************************************************************************
*    函 数 名: get_week_form_time
*    功能说明: 通过年月日计算对应的星期几
*    形    参: 
*    @year        : 4位完整年份（如2024）
*    @month        : 月份（1-12）
*    @day        : 日期（1-31）
*    返 回 值: 星期标识（0=周日，1=周一，...，6=周六，需根据实际需求调整）
*********************************************************************************************************
*/
static uint8_t get_week_form_time(uint16_t year,uint8_t month,uint8_t day)
{    
    uint16_t temp2;
    uint8_t yearH,yearL;
    
    yearH=year/100;    yearL=year%100; 
    // 若为21世纪（yearH>19），年份低位加100（适配计算公式）
    if (yearH>19)yearL+=100;
    // 计算核心逻辑（仅支持1900年以后）  
    temp2=yearL+yearL/4;
    temp2=temp2%7; 
    temp2=temp2+day+cg_table_week[month-1];
    // 若为闰年且月份小于3，需减1（修正闰年2月的影响）
    if (yearL%4==0&&month<3)temp2--;
    
    return(temp2%7);
}    

/*
*********************************************************************************************************
*    函 数 名: RTC_set_Time
*    功能说明: 统一设置RTC时间和日期（自动计算星期）
*    形    参: 
*    @rtc        : rtc_time_t类型结构体，包含时间日期信息
*                  rtc.year：4位完整年份（如2024）
*                  rtc.month：月份（1-12）
*                  rtc.data：日期（1-31，变量名data实际为day）
*                  rtc.hour：小时（24小时制，0-23）
*                  rtc.min：分钟（0-59）
*                  rtc.sec：秒钟（0-59）
*    返 回 值: 无
*********************************************************************************************************
*/
void RTC_set_Time(rtc_time_t rtc)
{
    // 自动计算星期并赋值
    rtc.week = get_week_form_time(rtc.year,rtc.month,rtc.data);
    
    // 使能PWR时钟（RTC属于备份域，需先开启PWR时钟）
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    // 设置时间
    if(ERROR == RTC_Set_Time(rtc))
    {
        printf("\n\r** RTC time configuration failed! **\n\r");
        return;
    }
}

/*
*********************************************************************************************************
*    函 数 名: RTC_Get_Time
*    功能说明: 读取当前RTC时间和日期
*    形    参: 
*    @rtc        : 指向rtc_time_t类型的指针，用于存储读取到的时间日期
*                  rtc.year：4位完整年份（如2024，由硬件两位数年份+2000得到）
*                  rtc.month：月份（1-12）
*                  rtc.data：日期（1-31，变量名data实际为day）
*                  rtc.hour：小时（24小时制，0-23）
*                  rtc.min：分钟（0-59）
*                  rtc.sec：秒钟（0-59）
*                  rtc.week：星期（1-7，由硬件直接读取）
*    返 回 值: 无
*********************************************************************************************************
*/
void RTC_Get_Time(rtc_time_t *rtc)
{
    rtc_parameter_struct rtc_current_para;

    // 读取当前时间
    rtc_current_time_get(&rtc_current_para);
    rtc->hour = RTC_Bcd2ToByte(rtc_current_para.hour);
    rtc->min  = RTC_Bcd2ToByte(rtc_current_para.minute);
    rtc->sec  = RTC_Bcd2ToByte(rtc_current_para.second);

    rtc->year  = RTC_Bcd2ToByte(rtc_current_para.year)+2000;  // 转为4位完整年份
    rtc->month = RTC_Bcd2ToByte(rtc_current_para.month);
    rtc->data  = RTC_Bcd2ToByte(rtc_current_para.date);
    rtc->week  = RTC_Bcd2ToByte(rtc_current_para.day_of_week);
}

/*
*********************************************************************************************************
*    函 数 名: RTC_AlarmSet
*    功能说明: 使能 RTC 闹钟中断
*    形    参: 无
*    返 回 值: 无
*    要使能 RTC 闹钟中断，需按照以下顺序操作：
* 1. 将 EXTI 线 17 配置为中断模式并将其使能，然后选择上升沿有效。
* 2. 配置 NVIC 中的 RTC_Alarm IRQ 通道并将其使能。
* 3. 配置 RTC 以生成 RTC 闹钟（闹钟 A 或闹钟 B）。
*********************************************************************************************************
*/
void RTC_AlarmSet(void)
{
    rtc_flag_clear(RTC_FLAG_ALRM0);
    exti_flag_clear(EXTI_17);
    /*=============================第①步=============================*/
    /* RTC 闹钟中断配置 */
    /* EXTI 配置 */
    exti_init(EXTI_17,EXTI_INTERRUPT,EXTI_TRIG_RISING);

    /*=============================第②步=============================*/
    /* 使能RTC闹钟中断 */
    nvic_irq_enable(RTC_Alarm_IRQn,15,0);

    /*=============================第③步=============================*/
    /* 失能闹钟 ，在设置闹钟时间的时候必须先失能闹钟*/
    rtc_alarm_disable(RTC_ALARM0);
    /* 设置闹钟时间 */
    rtc_alarm.alarm_mask = RTC_ALARM_DATE_MASK;
    rtc_alarm.weekday_or_date = RTC_ALARM_DATE_SELECTED; // 选择日期模式
    rtc_alarm.alarm_day = 0x20;
    rtc_alarm.alarm_hour = RTC_ByteToBcd2(ALARM_HOURS);
    rtc_alarm.alarm_minute = RTC_ByteToBcd2(ALARM_MINUTES);
    rtc_alarm.alarm_second = RTC_ByteToBcd2(ALARM_SECONDS);
    rtc_alarm.am_pm = RTC_AM;

    /* 配置RTC Alarm X（X=A或B） 寄存器 */
    rtc_alarm_config(RTC_ALARM0, &rtc_alarm);

    /* 使能 RTC Alarm X 中断 */
    rtc_interrupt_enable(RTC_INT_ALARM0);

    /* 使能闹钟 */
    rtc_alarm_enable(RTC_ALARM0);
}
/*
*********************************************************************************************************
*    函 数 名: RTC_Alarm_IRQHandler
*    功能说明: RTC闹钟中断服务函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void RTC_Alarm_IRQHandler(void)
{
    if(RESET != rtc_flag_get(RTC_FLAG_ALRM0))
    {
        printf("rtc alarm start success\n");
        // app_system_softreset();
        rtc_flag_clear(RTC_FLAG_ALRM0);
        exti_flag_clear(EXTI_17);
    }
}

/*
*********************************************************************************************************
*    函 数 名: time_to_second_function
*    功能说明: 时间（年月日时分秒）转换为Unix时间戳（秒数）- 适配NB通信
*    形    参: 
*    @time        : 时间数组，格式为[年,月,日,时,分,秒]（年为4位完整年份，如2024）
*    @second        : 指向uint32_t的指针，用于存储转换后的Unix时间戳
*    返 回 值: 无
*********************************************************************************************************
*/
void time_to_second_function(uint32_t *time, uint32_t *second)
{
    struct tm pt;
    
    pt.tm_year = time[0]+100;  // tm_year是从1900年开始的偏移（如2024→124=2024-1900）
    pt.tm_mon  = time[1] - 1;  // tm_mon是0-11，需将1-12的月份减1
    pt.tm_mday = time[2];      // 日期（1-31）
    pt.tm_hour = time[3];      // 小时（24小时制，0-23）
    pt.tm_min  = time[4];      // 分钟（0-59）
    pt.tm_sec  = time[5];      // 秒钟（0-59）
    
    *second = mktime(&pt);  // 转换为Unix时间戳
}

/*
*********************************************************************************************************
*    函 数 名: local_to_utc_time
*    功能说明: 本地时间转换为UTC时间（支持时区偏移计算，处理跨月/跨年）
*    形    参:  
*    @utc_time    : 指向rtc_time_t的指针，用于存储转换后的UTC时间
*    @timezone    : 时区偏移（本地时间=UTC时间+timezone，如UTC+8时区传入-8，UTC-5传入5）
*    @local_time    : 输入的本地时间（rtc_time_t结构体，含年/月/日/时/分/秒/星期）
*    返 回 值: 无
*********************************************************************************************************
*/
void local_to_utc_time(rtc_time_t *utc_time, int8_t timezone, rtc_time_t local_time)
{
    int year,month,day,hour,week;
    int lastday = 0;            // 当前月份的总天数
    int lastlastday = 0;        // 上一个月份的总天数

    // 初始化本地时间参数
    year    = local_time.year;    // 4位完整年份
    month = local_time.month;    // 月份（1-12）
    day     = local_time.data;    // 日期（1-31，变量名data实际为day）
    hour     = local_time.hour + timezone;  // 计算UTC小时（本地小时+时区偏移）
    week  = local_time.week;    // 星期（1-7）
    
    // 计算当前月份和上一个月份的总天数（处理闰年2月）
    // 大月（1,3,5,7,8,10,12）：31天；小月（4,6,9,11）：30天；2月：平年28天，闰年29天
    if(month==1 || month==3 || month==5 || month==7 || month==8 || month==10 || month==12)
    {
        lastday = 31;  // 当前月份为大月，31天
        lastlastday = 30;  // 上一个月份默认30天（需特殊处理3月和1月）
        
        if(month == 3)  // 当前月份是3月，上一个月份是2月（需判断闰年）
        {
            if((year%400 == 0)||(year%4 == 0 && year%100 != 0))// 闰年判断
                lastlastday = 29;
            else
                lastlastday = 28;
        }
        
        if(month == 8 || month == 1)  // 当前月份是8月（上一个月7月，31天）或1月（上一个月12月，31天）
            lastlastday = 31;
    }
    else if(month == 4 || month == 6 || month == 9 || month == 11)  // 当前月份为小月，30天
    {
        lastday = 30;
        lastlastday = 31;  // 上一个月份为大月，31天
    }
    else  // 当前月份是2月（需判断闰年）
    {
        lastlastday = 31;  // 上一个月份是1月，31天
        if((year%400 == 0)||(year%4 == 0 && year%100 != 0))// 闰年
            lastday = 29;
        else
            lastday = 28;
    }

    // 处理小时>=24的情况（跨天，日期+1，星期+1）
    if(hour >= 24)
    {                    
        hour -= 24;
        day += 1;
        week += 1;        // 星期加1
        if(week > 7)
            week = 1;  // 星期超过7则重置为1（1=周一）  
        // 处理日期超过当前月份总天数（跨月，月份+1）
        if(day > lastday)
        {         
            day -= lastday;
            month += 1;
            // 处理月份超过12（跨年，年份+1）
            if(month > 12)
            {
                month -= 12;
                year += 1;
            }
        }
    }
    
    // 处理小时<0的情况（跨天，日期-1，星期-1）
    if(hour < 0)
    {
        hour += 24;
        day -= 1;
        week -= 1;
        if(week < 1)
            week = 7;  // 星期小于1则重置为7（7=周日）
        // 处理日期<1（跨月，月份-1）
        if(day < 1)
        {
            day = lastlastday;  // 日期设为上一个月份的总天数
            month -= 1;
            // 处理月份<1（跨年，年份-1）
            if(month < 1)
            {
                month = 12;
                year -= 1;
            }
        }
    }

    // 赋值转换后的UTC时间
    utc_time->year  = year;
    utc_time->month = month;
    utc_time->data  = day;
    utc_time->week     = week;
    utc_time->hour  = hour;
    utc_time->min   = local_time.min;  // 分钟不变
    utc_time->sec   = local_time.sec;  // 秒钟不变
}

/*
*********************************************************************************************************
*    函 数 名: RTC_ByteToBcd2
*    功能说明: 将字节转换为BCD格式
*    形    参: 
*    @Value        : 待转换的字节值
*    返 回 值: 转换后的BCD格式字节值
*********************************************************************************************************
*/
static uint8_t RTC_ByteToBcd2(uint8_t Value)
{
    uint8_t bcdhigh = 0;
    
    while (Value >= 10)
    {
    bcdhigh++;
    Value -= 10;
    }
    
    return  ((uint8_t)(bcdhigh << 4) | Value);
}

/*
*********************************************************************************************************
*    函 数 名: RTC_Bcd2ToByte
*    功能说明: 将BCD格式的字节转换为字节值
*    形    参: 
*    @Value        : 待转换的BCD格式字节值
*    返 回 值: 转换后的字节值
*********************************************************************************************************
*/
static uint8_t RTC_Bcd2ToByte(uint8_t Value)
{
    uint8_t tmp = 0;
    tmp = ((uint8_t)(Value & (uint8_t)0xF0) >> (uint8_t)0x4) * 10;
    return (tmp + (Value & (uint8_t)0x0F));
}
/*
*********************************************************************************************************
*    函 数 名: TimeBySecond
*    功能说明: 根据Unix时间戳（秒数）设置RTC时间（含时区转换：UTC+8）
*    形    参: 
*    @second        : Unix时间戳（从1970-01-01 00:00:00 UTC开始的秒数）
*    返 回 值: 无
*********************************************************************************************************
*/
void TimeBySecond(uint32_t second)
{
    struct tm *pt,t;
    rtc_time_t time_t;
    second += 8*60*60;  // 时区转换：UTC+8，将传入的UTC时间戳转为本地时间
    pt = localtime(&second);
    
    if(pt == NULL)
        return;
    t=*pt;
    t.tm_year+=1900;  // tm_year是从1900年开始的偏移，转为4位完整年份
    t.tm_mon++;        // tm_mon是0-11，转为1-12的月份
    time_t.year  = t.tm_year;  // 4位年份（如2024）
    time_t.month = t.tm_mon;   // 月份（1-12）
    time_t.data  = t.tm_mday;  // 日期（1-31，变量名data实际为day）
    time_t.hour  = t.tm_hour;  // 小时（24小时制，0-23）
    time_t.min   = t.tm_min;   // 分钟（0-59）
    time_t.sec   = t.tm_sec;   // 秒钟（0-59）
    /* 调用统一时间设置接口 */
    RTC_set_Time(time_t);
}

/*
*********************************************************************************************************
*    函 数 名: rtc_test
*    功能说明: RTC时间读取测试函数（循环打印当前时间）
*    形    参:  无
*    返 回 值: 无
*********************************************************************************************************
*/
void rtc_test(void)
{
    static rtc_time_t rtc_test;
    while(1)
    {
        RTC_Get_Time(&rtc_test);
        // 打印格式：日期-月份-年份，星期，时间（时:分:秒）
        printf("data:%d-%02d-%02d,week:%d,time:%02d:%02d:%02d\n",rtc_test.year,rtc_test.month,rtc_test.data,
                                        rtc_test.week,rtc_test.hour,rtc_test.min,rtc_test.sec);
        dwt_delay_ms(1000);  // 每秒刷新一次
    }
}






