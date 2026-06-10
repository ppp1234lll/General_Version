/********************************************************************************
* @File name  : 按键模块
* @Description: 头文件
* @Author     : ZHLE
*  Version Date        Modification Description
	4、输入检测
        按键(恢复出厂设置):    PA8
        箱门检测:              PD13
        12V电源输入监测:       PA5
				水浸                   PD14

********************************************************************************/

#ifndef _KEY_H_
#define _KEY_H_

#include "./SYSTEM/sys/sys.h"

#define PWR_TST_READ   	   gpio_input_bit_get(GPIOA,GPIO_PIN_5)     // 12V检测
#define RESET_TST_READ   	 gpio_input_bit_get(GPIOA,GPIO_PIN_8)     // 按键检测
#define DOOR_TST_READ   	 gpio_input_bit_get(GPIOD,GPIO_PIN_13)     // 箱门检测
#define WATER_TST_READ   	 gpio_input_bit_get(GPIOD,GPIO_PIN_14)     // 水浸检测

/* 函数声明 */
void bsp_InitKey(void);

void key_test(void);

#endif
