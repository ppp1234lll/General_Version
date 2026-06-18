/*
*********************************************************************************************************
*    函 数 名: 利用STM32F4内部TIM输出PWM信号，顺便实现
*    功能说明: bsp_tim_pwm.h
*    形    参: V1.0
*    返 回 值: 头文件
*********************************************************************************************************
*/

#ifndef __BSP_TIMERS_H
#define __BSP_TIMERS_H

#include "./SYSTEM/sys/sys.h"

void bsp_RCC_TIM_Enable(uint32_t TIMx);

void bsp_InitTimers(uint32_t TIMx, uint32_t _ulFreq, uint8_t _PreemptionPriority, uint8_t _SubPriority);

uint32_t HAL_GetTick(void);
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
