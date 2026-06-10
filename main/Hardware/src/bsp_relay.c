#include "bsp_relay.h"
#include "bsp.h"

/*
	3、继电器
			继电器1：    PB8 
			继电器2：    PB7
			继电器3：    PB6
*/
#define RELAY1_GPIO_CLK 		RCU_GPIOB
#define RELAY1_GPIO_PORT 		GPIOB
#define RELAY1_GPIO_PIN 		GPIO_PIN_8

#define RELAY2_GPIO_CLK 		RCU_GPIOB
#define RELAY2_GPIO_PORT 		GPIOB
#define RELAY2_GPIO_PIN 		GPIO_PIN_7

#define RELAY3_GPIO_CLK 		RCU_GPIOB
#define RELAY3_GPIO_PORT 		GPIOB
#define RELAY3_GPIO_PIN 		GPIO_PIN_6

static rcu_periph_enum RELAY_GPIO_CLK[RELAY_NUM] = {RELAY1_GPIO_CLK, RELAY2_GPIO_CLK, 
													RELAY3_GPIO_CLK};
static uint32_t RELAY_GPIO_PORT[RELAY_NUM] = {RELAY1_GPIO_PORT, RELAY2_GPIO_PORT,
											RELAY3_GPIO_PORT};
static uint32_t RELAY_GPIO_PIN[RELAY_NUM] = {RELAY1_GPIO_PIN, RELAY2_GPIO_PIN, RELAY3_GPIO_PIN};


/*
*********************************************************************************************************
*	函 数 名: bsp_InitRelay
*	功能说明: 继电器初始化
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitRelay(void)
{
	for(uint8_t i=0;i<RELAY_NUM;i++)
	{
		rcu_periph_clock_enable(RELAY_GPIO_CLK[i]);
//		gpio_bit_reset(RELAY_GPIO_PORT[i],RELAY_GPIO_PIN[i]);
		gpio_mode_set(RELAY_GPIO_PORT[i], GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,RELAY_GPIO_PIN[i]);
		gpio_output_options_set(RELAY_GPIO_PORT[i], GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,RELAY_GPIO_PIN[i]);
    gpio_bit_reset(RELAY_GPIO_PORT[i],RELAY_GPIO_PIN[i]);		
	}
}

/*
*********************************************************************************************************
*	函 数 名: relay_control
*	功能说明: 继电器控制
*	形    参:  dev  : 继电器序号
*	           state: 继电器状态
*	返 回 值: 无
*********************************************************************************************************
*/
void relay_control(RELAY_DEV dev, RELAY_STATUS state)
{
	gpio_bit_write(RELAY_GPIO_PORT[dev],RELAY_GPIO_PIN[dev],(bit_status)(state?0:1));
}

/*
*********************************************************************************************************
*	函 数 名: relay_get_status
*	功能说明: 获取继电器状态
*	形    参: 无
*	返 回 值: 继电器状态
*********************************************************************************************************
*/
uint8_t relay_get_status(RELAY_DEV dev)
{
	return (gpio_output_bit_get(RELAY_GPIO_PORT[dev],RELAY_GPIO_PIN[dev]))?0:1;
}

/*
*********************************************************************************************************
*	函 数 名: relay_test
*	功能说明: 继电器测试
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void relay_test(void)
{
	while(1)
	{
		for(uint8_t i=0;i<RELAY_NUM;i++)
		{
			relay_control((RELAY_DEV)i,RELAY_ON); 
			delay_ms(500);
		}
		delay_ms(2000);
		
		for(uint8_t i=0;i<RELAY_NUM;i++)
		{
			relay_control((RELAY_DEV)i,RELAY_OFF); 
			delay_ms(500);
		}
		delay_ms(2000);
	}
}


