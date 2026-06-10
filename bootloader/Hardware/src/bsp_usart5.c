#include "bsp_usart5.h"
#include "bsp.h"

#define USART5_TX_GPIO_CLK			RCU_GPIOC
#define USART5_TX_GPIO_PORT			GPIOC
#define USART5_TX_PIN				GPIO_PIN_6
#define USART5_TX_PIN_AF			GPIO_AF_8

#define USART5_RX_GPIO_CLK			RCU_GPIOC
#define USART5_RX_GPIO_PORT			GPIOC
#define USART5_RX_PIN				GPIO_PIN_7
#define USART5_RX_PIN_AF			GPIO_AF_8

//////////////////////////////////////////////////////////////// 
///重定向c库函数printf到串口USARTx，重定向后可使用printf函数
int fputc(int ch, FILE *f)
{
    /* 发送一个字节数据到串口USARTx */
	usart_data_transmit(USART5, (uint8_t) ch);
	while(RESET == usart_flag_get(USART5, USART_FLAG_TBE));
	return ch;
}

///重定向c库函数scanf到串口USARTx，重写向后可使用scanf、getchar等函数
int fgetc(FILE *f)
{	
	/* 等待串口USARTx输入数据 */
	while(RESET == usart_flag_get(USART5, USART_FLAG_RBNE)); //等待串口收到数据
	return usart_data_receive(USART5); //将收到的数据返回给上一层函数
}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart5
*	功能说明: 初始化串口硬件，并对全局变量赋初值.
*	形    参: baudrate: 波特率
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart5(uint32_t baudrate)
{	
	 /* enable GPIO clock */
	rcu_periph_clock_enable(USART5_TX_GPIO_CLK); 
	rcu_periph_clock_enable(USART5_RX_GPIO_CLK);
	/* enable USART clock */
	rcu_periph_clock_enable(RCU_USART5);
	
	//串口5对应引脚复用映射
	gpio_af_set(USART5_TX_GPIO_PORT,USART5_TX_PIN_AF,USART5_TX_PIN); 
	gpio_af_set(USART5_RX_GPIO_PORT,USART5_RX_PIN_AF,USART5_RX_PIN); 
	
	/* configure USART5 TX as alternate function push-pull */
	gpio_mode_set(USART5_TX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART5_TX_PIN);
	gpio_output_options_set(USART5_TX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART5_TX_PIN);

	/* configure USART5 RX as alternate function push-pull */
	gpio_mode_set(USART5_RX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART5_RX_PIN);
	gpio_output_options_set(USART5_RX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART5_RX_PIN);

	/* USART5 configure */	
	usart_deinit(USART5);
    usart_word_length_set(USART5, USART_WL_8BIT);
    usart_stop_bit_set(USART5, USART_STB_1BIT);
    usart_parity_config(USART5, USART_PM_NONE);
    usart_baudrate_set(USART5, baudrate);
    usart_receive_config(USART5, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART5, USART_TRANSMIT_ENABLE);
    usart_enable(USART5);
}

/*
*********************************************************************************************************
*	函 数 名: Usart5_Send_Char
*	功能说明: 向串口发送1个字节。
*	形    参: 
*	@ch			: 待发送的字节数据
*	返 回 值: 无
*********************************************************************************************************
*/
void Usart5_Send_Char(uint8_t ch)
{
	usart_data_transmit(USART5, (uint8_t)ch);
	while (RESET == usart_flag_get(USART5, USART_FLAG_TBE));
}

/*
*********************************************************************************************************
*	函 数 名: Usart5_Send_Str
*	功能说明: 向串口发送字符串。
*	形    参:  
*	@buff		: 字符串指针
*	@len		: 发送数据长度
*	返 回 值: 无
*********************************************************************************************************
*/
void Usart5_Send_Str(uint8_t *buff, uint16_t len)
{
	while(len--) {
		Usart5_Send_Char(buff[0]);
		buff++;
	}
}

/*
*********************************************************************************************************
*	函 数 名: USART5_IRQHandler
*	功能说明: 供中断服务程序调用，通用串口中断处理函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void USART5_IRQHandler(void)
{
	if (usart_flag_get(USART5, USART_FLAG_RBNE) != RESET) {
		usart_flag_clear(USART5, USART_FLAG_RBNE);
	}
}

/*
*********************************************************************************************************
*	函 数 名: usart5_test
*	功能说明: 串口测试
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void usart5_test(void)
{
	while(1)
	{
//		usart_debug_send_str(usart_debug_rx_buff,sizeof(usart_debug_rx_buff));
		printf("串口测试\n");
		// delay_ms(1000);	
		dwt_delay_ms(1000);	
	}
}

/******************************************  (END OF FILE) **********************************************/




