/********************************************************************************
* @File name  : 风扇驱动
* @Description:  
* @Author     : ZHLE
*  Version Date        Modification Description
	16、输出控制：
		风扇:          PD15

********************************************************************************/

#include "bsp_fan.h"
#include "bsp.h"

#define FAN_GPIO_CLK 		RCU_GPIOD
#define FAN_GPIO_PORT 		GPIOD
#define FAN_GPIO_PIN 		GPIO_PIN_15

/*
*********************************************************************************************************
*	函 数 名: bsp_InitFan
*	功能说明: 风扇初始化
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitFan(void)
{
	/* enable the led clock */
	rcu_periph_clock_enable(FAN_GPIO_CLK);

	/* configure led GPIO port */ 
	gpio_mode_set(FAN_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,FAN_GPIO_PIN);
	gpio_output_options_set(FAN_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,FAN_GPIO_PIN);
	
	gpio_bit_set(FAN_GPIO_PORT,FAN_GPIO_PIN);
	
}

/*
*********************************************************************************************************
*	函 数 名: fan_control
*	功能说明: 继电器控制
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void fan_control( FAN_STATUS state)
{
	gpio_bit_write(FAN_GPIO_PORT, FAN_GPIO_PIN, (state == FAN_OFF) ? SET : RESET);
}

/*
*********************************************************************************************************
*	函 数 名: fan_test
*	功能说明: 风扇测试
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void fan_test(void)
{
	while(1)
	{
		fan_control(FAN_ON); 
		dwt_delay_ms(5000);
		fan_control(FAN_OFF);  
		dwt_delay_ms(5000);	
	}
}


