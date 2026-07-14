/*
*********************************************************************************************************
*    函 数 名: fwdgt操作模块
*    功能说明: bsp_fwdgt.c
*    形    参: V1.0
*    返 回 值: 提供看门狗的函数
*    修改记录 :
*        版本号   日期          作者     说明
*        V1.0    2026-06-10   zb/zh    正式发布
*    Copyright (C), 2026-2026, 蜂鸟 www.flybee.com
*********************************************************************************************************
*/
#include "bsp_fwdgt.h"
#include "bsp.h"

#define configUSE_SOFT_FWDGT        0       /* 是否使用软件定时器 */
#define configUSE_HARD_FWDGT        0       /* 是否使用硬件定时器 */

/*
*********************************************************************************************************
*                                 硬件看门狗GPIO配置
*********************************************************************************************************
*/
#if configUSE_HARD_FWDGT > 0
#define FWDGT_SCK_CLK                    RCU_GPIOA
#define FWDGT_SCK_GPIO                   GPIOA
#define FWDGT_SCK_PIN                    GPIO_PIN_0
#endif

/*
*********************************************************************************************************
*    函 数 名: bsp_InitFwdgt
*    功能说明: 初始化看门狗
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitFwdgt(uint8_t prer,uint16_t rlr)
{
    #if configUSE_SOFT_FWDGT > 0U

    /* 看门狗溢出时间Tout=64× 500/32=1000ms */
    fwdgt_config(rlr, prer);

    #endif
    
    #if configUSE_HARD_FWDGT > 0U
    rcu_periph_clock_enable(FWDGT_SCK_CLK);
    gpio_mode_set(FWDGT_SCK_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,FWDGT_SCK_PIN);
    gpio_output_options_set(FWDGT_SCK_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,FWDGT_SCK_PIN);
    gpio_bit_set(FWDGT_SCK_GPIO,FWDGT_SCK_PIN);    
    #endif
}

/*
*********************************************************************************************************
*    函 数 名: FeedFwdgt
*    功能说明: 喂独立看门狗
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void FeedFwdgt(void)
{
    #if configUSE_SOFT_FWDGT > 0U
    fwdgt_counter_reload(); 
    #endif
    
    #if configUSE_HARD_FWDGT > 0U    
    gpio_bit_toggle(FWDGT_SCK_GPIO,FWDGT_SCK_PIN);
    #endif
}

/*
*********************************************************************************************************
*    函 数 名: fwdgt_test
*    功能说明: FWDGT测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void fwdgt_test(void)
{
    /* confiure FWDGT counter clock: 32KHz(IRC32K) / 64 = 0.5 KHz */
    bsp_InitFwdgt(FWDGT_PSC_DIV64,500);

    while(1)
    {
        printf("fwdgt_test\n");
        FeedFwdgt();
        delay_ms(500);
    }
}
        
/******************************************  (END OF FILE) **********************************************/
