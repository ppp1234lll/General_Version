#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include "./SYSTEM/sys/sys.h"

typedef enum
{
    CHANNEL_LP = 0,
    CHANNEL_NP = 1,
    CHANNEL_MAX,
} ADC_CHANNEL_t;

/* 提供给其他C文件调用的函数 */
void bsp_InitADC(void);
uint16_t Get_Adc_Channel_Sample(uint8_t channel);
uint16_t Get_Adc_Values(ADC_CHANNEL_t channel_index);

void adc_test(void);

#endif

