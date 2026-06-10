#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include "gd32f4xx.h"
#include "gd32f4xx_adc.h"
#include "./SYSTEM/sys/sys.h"

// ADC采样值存储数组 (2个通道)
extern uint16_t adc_value[2];

// ADC初始化函数
void adc_gpio_config(void);
void adc_dma_config(void);
void adc_config(void);

// 获取电压值 (单位: mV)
float get_voltage_v(uint8_t channel_index);

int adc_test(void);

#endif