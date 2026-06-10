#include "bsp_key.h"
#include "bsp.h"
/*
	4、输入检测
		按键(恢复出厂设置):     PA8
		箱门检测:              PD13
		12V电源输入监测:       PA5
		水浸                   PD14 
*/

/*
*********************************************************************************************************
*	函 数 名: bsp_InitKey
*	功能说明: 初始化按键.  
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitKey(void)
{
	rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_GPIOD);

    /* configure button pin as input */
    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE,GPIO_PIN_5);
	  gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE,GPIO_PIN_8);
	  gpio_mode_set(GPIOD, GPIO_MODE_INPUT, GPIO_PUPD_NONE,GPIO_PIN_13);
	  gpio_mode_set(GPIOD, GPIO_MODE_INPUT, GPIO_PUPD_NONE,GPIO_PIN_14);
	
}

/*
*********************************************************************************************************
*	函 数 名: key_test
*	功能说明: 按键测试.  
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void key_test(void)
{
	while(1)
	{
		printf("PWR     = %d...",PWR_TST_READ);
		printf("RESET   = %d...",RESET_TST_READ);
		printf("DOOR   = %d...",DOOR_TST_READ);
		printf("WATER   = %d...",WATER_TST_READ);
		
		delay_ms(1000);		
	}
}

