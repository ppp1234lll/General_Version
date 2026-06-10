/*
*********************************************************************************************************
*
*	模块名称 : LED指示灯驱动模块
*	文件名称 : bsp_led.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_LED_H_
#define _BSP_LED_H_

#include "./SYSTEM/sys/sys.h"

/* 参数 */
typedef enum
{
	LD_STATE   = 0, // 系统指示灯
	LD_GPRS    = 1, // 4G指示灯
	LD_LAN     = 2, // 网口
	LD_PWR_O   = 3, // 电源-外接
	LD_LAN_O   = 4, // 网口-外接
} LD_DEV;

typedef enum
{
	LD_ON		= 0, // 常亮
	LD_OFF		= 1, // 熄灭
	LD_FLICKER 	= 2, // 闪烁
	LD_FLIC_Q  	= 3, // 快速闪烁
} LED_STATUS;

/* 供外部调用的函数声明 */
void bsp_InitLed(void);	// 初始化函数
void led_flicker_control_timer_function(void);

void led_control_function(LD_DEV dev, LED_STATUS state);
void led_out_control_function(LD_DEV dev, LED_STATUS state);

void led_all_on(void);
void led_all_off(void);

void led_test(void);

#endif
/******************************************  (END OF FILE) **********************************************/
