#include "bsp_adc.h"
#include "bsp.h"
#include "appconfig.h"

/*
	2、ADC（原理图）：
        市电火-地：           PA3
        市电零-地：           PA4
*/
#if (configUSE_LN_PGND == 1)

#define ADC_CHANNEL_NUM         2   // 大于1时，开启DMA

#define AC_LP_GPIO_CLK          RCU_GPIOA
#define AC_LP_GPIO_PORT         GPIOA
#define AC_LP_GPIO_PIN          GPIO_PIN_3
#define AC_LP_ADC_CHANNEL       ADC_CHANNEL_3

#define AC_NP_GPIO_CLK          RCU_GPIOA
#define AC_NP_GPIO_PORT         GPIOA
#define AC_NP_GPIO_PIN          GPIO_PIN_4
#define AC_NP_ADC_CHANNEL       ADC_CHANNEL_4

#define ADCx                    ADC0
#define ADCx_CLK                RCU_ADC0

#if (ADC_CHANNEL_NUM > 1)
#define ADCx_DMA                DMA1  
#define ADCx_DMA_CLK            RCU_DMA1    
#define ADCx_DMA_CHANNEL        DMA_CH4
#endif

// 定义存储ADC转换结果的数组
uint16_t g_adc_value[CHANNEL_MAX] = {0};

/* 本文件调用的函数 */
void bsp_InitADC_GPIO(void);
void bsp_InitADC_DMA(void);
void bsp_InitADC_Config(void);

/*
*********************************************************************************************************
*    函 数 名: bsp_InitADC
*    功能说明: 初始化ADC
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitADC(void)
{
    bsp_InitADC_GPIO();
    bsp_InitADC_DMA();
    bsp_InitADC_Config();
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitADC_GPIO
*    功能说明: 初始化ADC GPIO
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitADC_GPIO(void)
{
    // 使能GPIO时钟
    rcu_periph_clock_enable(AC_LP_GPIO_CLK);
    rcu_periph_clock_enable(AC_NP_GPIO_CLK);
    
    // 配置为模拟输入模式
    gpio_mode_set(AC_LP_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, AC_LP_GPIO_PIN);
    gpio_mode_set(AC_NP_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, AC_NP_GPIO_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitADC_Config
*    功能说明: 初始化ADC配置
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitADC_Config(void)
{
    adc_deinit();
    rcu_periph_clock_enable(ADCx_CLK);
    adc_clock_config(ADC_ADCCK_HCLK_DIV20);
    
    /* ADC mode config */
    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);
    /* ADC contineous function disable */
    adc_special_function_config(ADCx, ADC_CONTINUOUS_MODE, ENABLE);
    /* ADC scan mode disable */
    adc_special_function_config(ADCx, ADC_SCAN_MODE, ENABLE);
    /* ADC data alignment config */
    adc_data_alignment_config(ADCx, ADC_DATAALIGN_RIGHT);
    /* ADC channel length config */
    adc_channel_length_config(ADCx, ADC_ROUTINE_CHANNEL, CHANNEL_MAX);
    
    adc_routine_channel_config(ADCx, CHANNEL_LP, AC_LP_ADC_CHANNEL, ADC_SAMPLETIME_480);
    adc_routine_channel_config(ADCx, CHANNEL_NP, AC_NP_ADC_CHANNEL, ADC_SAMPLETIME_480);

    // 关闭外部触发，使用软件触发
    adc_external_trigger_config(ADCx, ADC_ROUTINE_CHANNEL, EXTERNAL_TRIGGER_DISABLE);

#if (ADC_CHANNEL_NUM > 1)
    // 使能 ADC 在最后一次传输后继续发出 DMA 请求
    adc_dma_request_after_last_enable(ADCx);
    adc_dma_mode_enable(ADCx);
#endif    

    // 10. 使能ADC并进行校准
    adc_enable(ADCx);
    delay_ms(1);               // 等待稳定
    adc_calibration_enable(ADCx);
    
    // 11. 软件触发启动转换
    adc_software_trigger_enable(ADCx, ADC_ROUTINE_CHANNEL);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitADC_DMA
*    功能说明: 初始化ADC DMA
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitADC_DMA(void)
{
#if (ADC_CHANNEL_NUM > 1)
    dma_single_data_parameter_struct dma_init_struct;  
    rcu_periph_clock_enable(ADCx_DMA_CLK);  // ADC0使用DMA1

    dma_single_data_para_struct_init(&dma_init_struct);
    dma_deinit(ADCx_DMA, ADCx_DMA_CHANNEL);
    
    /* initialize DMA single data mode */
    dma_init_struct.periph_addr = (uint32_t)(&ADC_RDATA(ADCx));
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory0_addr = (uint32_t)g_adc_value;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.number = CHANNEL_MAX;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(ADCx_DMA, ADCx_DMA_CHANNEL, &dma_init_struct);
    dma_channel_subperipheral_select(ADCx_DMA, ADCx_DMA_CHANNEL, DMA_SUBPERI0);

    dma_circulation_enable(ADCx_DMA, ADCx_DMA_CHANNEL);
    dma_channel_enable(ADCx_DMA, ADCx_DMA_CHANNEL);
#endif
}

/*
*********************************************************************************************************
*    函 数 名: Get_Adc_Channel_Sample
*    功能说明: 获得ADC 单通道采样电压值
*    形    参: 无
*    返 回 值: 电压值
*********************************************************************************************************
*/		
uint16_t Get_Adc_Channel_Sample(uint8_t channel)
{
    /* ADC routine channel config */
    adc_routine_channel_config(ADCx, 0U, channel, ADC_SAMPLETIME_480);
    /* ADC software trigger enable */
    adc_software_trigger_enable(ADCx, ADC_ROUTINE_CHANNEL);

    /* wait the end of conversion flag */
    while(!adc_flag_get(ADCx, ADC_FLAG_EOC));
    /* clear the end of conversion flag */
    adc_flag_clear(ADCx, ADC_FLAG_EOC);
    /* return regular channel sample value */
    return (adc_routine_data_read(ADCx));
}

/*
*********************************************************************************************************
*    函 数 名: Get_Adc_Values
*    功能说明: 获得电压值
*    形    参: 无
*    返 回 值: 电压值
*********************************************************************************************************
*/		
uint16_t Get_Adc_Values(ADC_CHANNEL_t channel_index)
{
#if (ADC_CHANNEL_NUM > 1)
	return g_adc_value[channel_index]*3300/4096;
#else
	return Get_Adc_Channel_Sample(ADC_CHANNEL_4)*3300/4096;
#endif
}

/*
*********************************************************************************************************
*    函 数 名: Adc_Test
*    功能说明: Adc测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/		
void adc_test(void)
{
    while(1) 
    {
        printf("adc_lp:%d mV....",Get_Adc_Values(CHANNEL_LP));
        printf("adc_np:%d mV\n",Get_Adc_Values(CHANNEL_NP));
        delay_ms(1000);
    }
}
#endif

