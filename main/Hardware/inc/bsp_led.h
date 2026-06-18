/*
*********************************************************************************************************
*    函 数 名: LED指示灯驱动模块
*    功能说明: bsp_led.h
*    形    参: V1.0
*    返 回 值: 头文件
*********************************************************************************************************
*/

#ifndef _BSP_LED_H_
#define _BSP_LED_H_

#include "./SYSTEM/sys/sys.h"

/* 参数 */
typedef enum
{
    LD_STATE    = 0, // 系统指示灯
    LD_GPRS     = 1, // 4G指示灯
    LD_LAN      = 2, // 网口
    LD_LAN_EXT  = 3, // 网口-外接
} LD_DEV;

typedef enum
{
    LD_ON           = 0, // 常亮
    LD_OFF          = 1, // 熄灭
    LD_FLICKER      = 2, // 闪烁
    LD_FLIC_Q       = 3, // 快速闪烁
} LED_STATUS;

/* 供外部调用的函数声明 */
void bsp_InitLed(void);    // 初始化函数
void led_flicker_control_timer_function(void);

void led_control_function(LD_DEV dev, LED_STATUS state);

void led_all_on(void);
void led_all_off(void);

void led_test(void);

#endif
/******************************************  (END OF FILE) **********************************************/
