#include "bsp_key.h"
#include "bsp.h"
/*
    4、输入检测
        按键(恢复出厂设置):      PD2
        箱门检测:               PA11
        12V电源输入监测:         PD0
        水浸 :                 PD13	
        输入检测1：             PD14
        输入检测2：             PD15
        输入检测3：             PC8
*/

/*
*********************************************************************************************************
*    函 数 名: bsp_InitKey
*    功能说明: 初始化按键.
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitKey(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);

    /* configure button pin as input */
    /* 箱门检测: PA11 */
    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_11);
    
    /* 输入检测3: PC8 */
    gpio_mode_set(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_8);

    /* 12V电源输入监测: PD0, 按键(恢复出厂设置): PD2, 水浸: PD13, 输入检测1: PD14, 输入检测2: PD15 */
    gpio_mode_set(GPIOD, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
    gpio_mode_set(GPIOD, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_mode_set(GPIOD, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_13);
    gpio_mode_set(GPIOD, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_14);
    gpio_mode_set(GPIOD, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_15);
}

/*
*********************************************************************************************************
*    函 数 名: key_demo
*    功能说明: 按键测试.
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void key_demo(void)
{
    while(1)
    {
        printf("PWR     = %d...",PWR_TST_READ);
        printf("RESET   = %d...",RESET_TST_READ);
        printf("DOOR    = %d...",DOOR_TST_READ);
        printf("WATER   = %d...",WATER_TST_READ);
        printf("IN1     = %d...",IN1_TST_READ);
        printf("IN2     = %d...",IN2_TST_READ);
        printf("IN3     = %d\r\n",IN3_TST_READ);
        
        delay_ms(1000);        
    }
}

