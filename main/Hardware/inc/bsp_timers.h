/*
*********************************************************************************************************
*
*	模块名称 : 利用STM32F4内部TIM输出PWM信号，顺便实现
*	文件名称 : bsp_tim_pwm.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2012-2013, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_TIMERS_H
#define __BSP_TIMERS_H

#include "./SYSTEM/sys/sys.h"

void bsp_RCC_TIM_Enable(uint32_t TIMx);

void bsp_InitTimers(uint32_t TIMx, uint32_t _ulFreq, uint8_t _PreemptionPriority, uint8_t _SubPriority);

uint32_t HAL_GetTick(void);

void set_reboot_time_function(uint32_t time);
static void device_reboot_timer_function(void);
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
