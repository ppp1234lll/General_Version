#include "bsp_usart5.h"
#include "bsp.h"

/*
*********************************************************************************************************
*	                             选择DMA，中断或者查询方式
*********************************************************************************************************
*/
#define USE_USART5_TX_DMA		/* 发送 DMA方式  */

// #define USE_USART5_INT		/* 中断方式 */
// #define USE_USART5_IDEL		/* DMA接收+空闲中断方式 */
#define USE_USART5_TIMEOUT		/* DMA接收+超时检测中断方式 */
/*
*********************************************************************************************************
*	                            时钟，引脚，DMA，中断等宏定义
*********************************************************************************************************
*/
#define USART5_TX_GPIO_CLK			RCU_GPIOC
#define USART5_TX_GPIO_PORT			GPIOC
#define USART5_TX_PIN				GPIO_PIN_6
#define USART5_TX_PIN_AF			GPIO_AF_8

#define USART5_RX_GPIO_CLK			RCU_GPIOC
#define USART5_RX_GPIO_PORT			GPIOC
#define USART5_RX_PIN				GPIO_PIN_7
#define USART5_RX_PIN_AF			GPIO_AF_8

#define USART5_DMA_CLK					RCU_DMA1
#define USART5_DMAx						DMA1
#define USART5_TX_DMA_CHANNEL			DMA_CH6
#define USART5_RX_DMA_CHANNEL			DMA_CH1

#define USART5_TX_DMA_PERIEN			DMA_SUBPERI5
#define USART5_RX_DMA_PERIEN			DMA_SUBPERI5

#define USART5_DMA_TX_IRQn				DMA1_Channel6_IRQn
#define USART5_DMA_RX_IRQn				DMA1_Channel1_IRQn

#define USART5_DMA_TX_IRQHandler		DMA1_Channel6_IRQHandler
#define USART5_DMA_RX_IRQHandler 		DMA1_Channel1_IRQHandler
/*
*********************************************************************************************************
*	                                           变量
*********************************************************************************************************
*/
enum {
	TRANSFER_WAIT,
	TRANSFER_TX_COMPLETE,
	TRANSFER_RX_COMPLETE,
	TRANSFER_ERROR,
};

#define USART5_BUFF_SIZE  2048

uint8_t *g_usart5_tx_buff = NULL;
uint8_t g_usart5_rx_buff[USART5_BUFF_SIZE] = {0};
uint32_t g_usart5_TransferState = TRANSFER_WAIT;
uint32_t g_usart5_Len;	

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
	bsp_InitUsart5_GPIO();
	bsp_InitUsart5_Config(baudrate);
	bsp_InitUsart5_DMA();
}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart5
*	功能说明: 初始化串口硬件 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart5_GPIO(void)
{
	/* enable GPIO clock */
	rcu_periph_clock_enable(USART5_TX_GPIO_CLK);
	rcu_periph_clock_enable(USART5_RX_GPIO_CLK);

	/* configure the USART5 TX pin and USART5 RX pin */
	gpio_af_set(USART5_TX_GPIO_PORT, USART5_TX_PIN_AF, USART5_TX_PIN);
	gpio_af_set(USART5_RX_GPIO_PORT, USART5_RX_PIN_AF, USART5_RX_PIN);

	/* configure USART5 TX as alternate function push-pull */
	gpio_mode_set(USART5_TX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART5_TX_PIN);
	gpio_output_options_set(USART5_TX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART5_TX_PIN);

	/* configure USART5 RX as alternate function push-pull */
	gpio_mode_set(USART5_RX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART5_RX_PIN);
	gpio_output_options_set(USART5_RX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART5_RX_PIN);

}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart5_Config
*	功能说明: 初始化串口硬件 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart5_Config(uint32_t baudrate)
{
	/* enable USART clock */
	rcu_periph_clock_enable(RCU_USART5);
	
	/* USART5 configure */
	usart_deinit(USART5);
    usart_word_length_set(USART5, USART_WL_8BIT);
    usart_stop_bit_set(USART5, USART_STB_1BIT);
    usart_parity_config(USART5, USART_PM_NONE);
    usart_baudrate_set(USART5, baudrate);
    usart_receive_config(USART5, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART5, USART_TRANSMIT_ENABLE);
    usart_enable(USART5);
	
	#ifdef USE_USART5_INT
	{
		usart_interrupt_flag_clear(USART5, USART_INT_FLAG_RBNE);
		usart_interrupt_flag_clear(USART5, USART_INT_FLAG_TC);
		usart_interrupt_enable(USART5, USART_INT_RBNE);
		nvic_irq_enable(USART5_IRQn, 7, 0);
	}
	#endif

	#ifdef USE_USART5_IDEL
	{
		usart_interrupt_enable(USART5, USART_INT_IDLE);
		nvic_irq_enable(USART5_IRQn, 7, 0);
	}
	#endif

	#ifdef USE_USART5_TIMEOUT
	{
		/* enable the USART receive timeout and configure the time of timeout */
		usart_interrupt_flag_clear(USART5, USART_INT_FLAG_RT);
		/*向寄存器填入需要超时的长度，单位为一个波特时长,3.5个字节*11波特长度 = 39  */
		usart_receiver_timeout_threshold_config(USART5, 39);
		usart_receiver_timeout_enable(USART5);
		usart_interrupt_enable(USART5, USART_INT_RT);
		nvic_irq_enable(USART5_IRQn, 7, 0);
	}
	#endif
}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart5_DMA
*	功能说明: 初始化串口DMA硬件 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart5_DMA(void)
{
	/* 配置TX DMA和NVIC */
	#ifdef USE_USART5_TX_DMA
	{
		dma_single_data_parameter_struct dma_tx_struct;
		
		/* enable DMA1 */
		rcu_periph_clock_enable(USART5_DMA_CLK);

		/* deinitialize DMA USART5 TX */
		dma_deinit(USART5_DMAx, USART5_TX_DMA_CHANNEL);
		dma_tx_struct.periph_addr         = (uint32_t)&USART_DATA(USART5);
		dma_tx_struct.memory0_addr        = (uint32_t)g_usart5_tx_buff;
		dma_tx_struct.direction           = DMA_MEMORY_TO_PERIPH;
		dma_tx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
		dma_tx_struct.priority            = DMA_PRIORITY_MEDIUM;
		dma_tx_struct.number              = USART5_BUFF_SIZE;
		dma_tx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
		dma_tx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
		dma_tx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
		dma_single_data_mode_init(USART5_DMAx, USART5_TX_DMA_CHANNEL, &dma_tx_struct);
		dma_channel_subperipheral_select(USART5_DMAx, USART5_TX_DMA_CHANNEL, USART5_TX_DMA_PERIEN);
		
		/* USART DMA enable for transmission and reception */
		usart_dma_transmit_config(USART5, USART_TRANSMIT_DMA_ENABLE);	
		
		/* enable DMA1 channel7 transfer complete interrupt */
		dma_interrupt_flag_clear(USART5_DMAx, USART5_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
		dma_interrupt_enable(USART5_DMAx, USART5_TX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
		nvic_irq_enable(USART5_DMA_TX_IRQn, 7, 0);

	}
	#endif
	/* 配置RX DMA和NVIC */
	#if defined(USE_USART5_IDEL) || defined(USE_USART5_TIMEOUT)
	{
		dma_single_data_parameter_struct dma_rx_struct;

		/* enable DMA1 */
		rcu_periph_clock_enable(USART5_DMA_CLK);

		/* deinitialize DMA USART5 RX */
		dma_deinit(USART5_DMAx, USART5_RX_DMA_CHANNEL);
		dma_rx_struct.periph_addr         = (uint32_t)&USART_DATA(USART5);
		dma_rx_struct.memory0_addr        = (uint32_t)g_usart5_rx_buff;
		dma_rx_struct.direction           = DMA_PERIPH_TO_MEMORY;
		dma_rx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
		dma_rx_struct.priority            = DMA_PRIORITY_MEDIUM;
		dma_rx_struct.number              = USART5_BUFF_SIZE;
		dma_rx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
		dma_rx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
		dma_rx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
		dma_single_data_mode_init(USART5_DMAx, USART5_RX_DMA_CHANNEL, &dma_rx_struct);
		dma_channel_subperipheral_select(USART5_DMAx, USART5_RX_DMA_CHANNEL, USART5_RX_DMA_PERIEN);
		
		/* USART DMA enable for transmission and reception */
		usart_dma_receive_config(USART5, USART_RECEIVE_DMA_ENABLE);	

		/* enable DMA1 channel2 transfer complete interrupt */
		// dma_interrupt_enable(USART5_DMAx, USART5_RX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
		// nvic_irq_enable(USART5_DMA_RX_IRQn, 7, 0);

		/* enable DMA1 channel2 */
		dma_channel_enable(USART5_DMAx, USART5_RX_DMA_CHANNEL);
	}
	#endif
}

/*
*********************************************************************************************************
*	函 数 名: usart5_send_char
*	功能说明: 向串口5发送1个字节。
*	形    参: 
*	@ch			: 待发送的字节数据
*	返 回 值: 无
*********************************************************************************************************
*/
void usart5_send_char(uint8_t ch)
{
	usart_data_transmit(USART5, (uint8_t)ch);
	while (RESET == usart_flag_get(USART5, USART_FLAG_TBE));
}

/*
*********************************************************************************************************
*	函 数 名: usart5_send_str
*	功能说明: 向串口5发送字符串。
*	形    参:  
*	@buff		: 字符串指针
*	@len		: 发送数据长度
*	返 回 值: 无
*********************************************************************************************************
*/
void usart5_send_str(uint8_t *buff, uint16_t len)
{
	g_usart5_TransferState = TRANSFER_WAIT;
	#ifdef USE_USART5_INT
	{
		g_usart5_tx_buff = buff;
		g_usart5_Len = len;
		usart_interrupt_enable(USART5, USART_INT_TBE);
		while(g_usart5_TransferState != TRANSFER_TX_COMPLETE);
	}
	#elif defined(USE_USART5_TX_DMA)
	{	
		dma_memory_address_config(USART5_DMAx, USART5_TX_DMA_CHANNEL, DMA_MEMORY_0, (uint32_t)buff);
		dma_transfer_number_config(USART5_DMAx, USART5_TX_DMA_CHANNEL, len);
		dma_channel_enable(USART5_DMAx, USART5_TX_DMA_CHANNEL);
		while(g_usart5_TransferState != TRANSFER_TX_COMPLETE);
	}
	#else
	{
		while(len--) 
		{
			usart5_send_char(buff[0]);
			buff++;
		}
	}
	#endif
}

#ifdef USE_USART5_TX_DMA
/*
*********************************************************************************************************
*	函 数 名: USART5_DMA_TX_IRQHandler
*	功能说明: 供中断服务程序调用，DMA发送完成中断处理函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void USART5_DMA_TX_IRQHandler(void)
{
    if(dma_interrupt_flag_get(USART5_DMAx, USART5_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF)) 
	{
        dma_interrupt_flag_clear(USART5_DMAx, USART5_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
        g_usart5_TransferState = TRANSFER_TX_COMPLETE;
    }
}
#endif

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
	#ifdef USE_USART5_INT
	{
		static uint16_t rxcount = 0;
		static uint16_t txcount = 0;
		if((RESET != usart_interrupt_flag_get(USART5, USART_INT_FLAG_RBNE)) &&
			(RESET != usart_flag_get(USART5, USART_FLAG_RBNE))) 
		{
			/* receive data */
			g_usart5_rx_buff[rxcount++] = usart_data_receive(USART5);
			if(rxcount == 10) 
			{
				rxcount = 0;
				g_usart5_TransferState = TRANSFER_RX_COMPLETE;
				// usart_interrupt_disable(USART5, USART_INT_RBNE);
			}
		}
		if((RESET != usart_flag_get(USART5, USART_FLAG_TBE)) &&
			(RESET != usart_interrupt_flag_get(USART5, USART_INT_FLAG_TBE))) 
		{
			/* transmit data */
			usart_data_transmit(USART5, g_usart5_tx_buff[txcount++]);
			if(txcount == g_usart5_Len)
			{
				txcount = 0;
				g_usart5_TransferState = TRANSFER_TX_COMPLETE;
				usart_interrupt_disable(USART5, USART_INT_TBE);
			}
		}
	}
	#endif

	#ifdef USE_USART5_IDEL
	{
		if(RESET != usart_interrupt_flag_get(USART5, USART_INT_FLAG_IDLE))
		{
			/* clear IDLE flag */
			usart_data_receive(USART5);
			
			/* number of data received */
			g_usart5_Len = USART5_BUFF_SIZE - (dma_transfer_number_get(USART5_DMAx, USART5_RX_DMA_CHANNEL));
			g_usart5_TransferState = TRANSFER_RX_COMPLETE;
			
			/* disable DMA and reconfigure */
			dma_channel_disable(USART5_DMAx, USART5_RX_DMA_CHANNEL);
			dma_flag_clear(USART5_DMAx, USART5_RX_DMA_CHANNEL, DMA_FLAG_FTF);
			dma_transfer_number_config(USART5_DMAx, USART5_RX_DMA_CHANNEL, USART5_BUFF_SIZE);
			dma_channel_enable(USART5_DMAx, USART5_RX_DMA_CHANNEL);
		}
    }
	#endif

	#ifdef USE_USART5_TIMEOUT
	{
		if(RESET != usart_interrupt_flag_get(USART5, USART_INT_FLAG_RT))
		{
			usart_interrupt_flag_clear(USART5, USART_INT_FLAG_RT);
			
			/* number of data received */
			g_usart5_Len = USART5_BUFF_SIZE - (dma_transfer_number_get(USART5_DMAx, USART5_RX_DMA_CHANNEL));
			g_usart5_TransferState = TRANSFER_RX_COMPLETE;
			
			/* disable DMA and reconfigure */
			dma_channel_disable(USART5_DMAx, USART5_RX_DMA_CHANNEL);
			dma_flag_clear(USART5_DMAx, USART5_RX_DMA_CHANNEL, DMA_FLAG_FTF);
			dma_transfer_number_config(USART5_DMAx, USART5_RX_DMA_CHANNEL, USART5_BUFF_SIZE);
			dma_channel_enable(USART5_DMAx, USART5_RX_DMA_CHANNEL);
		}
	}
	#endif

	if((RESET != usart_flag_get(USART5, USART_FLAG_ORERR)) ||
		(RESET != usart_flag_get(USART5, USART_FLAG_FERR)) ||
		(RESET != usart_flag_get(USART5, USART_FLAG_PERR)))
	{
		USART_STAT0(USART5);
		usart_data_receive(USART5);
	}

}

/*
*********************************************************************************************************
*	函 数 名: usart5_test
*	功能说明: 串口5测试
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void usart5_test(void)
{
	uint8_t usart5_rx_test[] = "Hello, World!\n";
	while(1)
	{
		#if defined(USE_USART5_INT)
		{
			while(g_usart5_TransferState != TRANSFER_RX_COMPLETE);
			usart5_send_str(usart5_rx_test,sizeof(usart5_rx_test));
		}
		#elif defined(USE_USART5_IDEL) || defined(USE_USART5_TIMEOUT) || defined(USE_USART5_TX_DMA)
		{
			while(g_usart5_TransferState != TRANSFER_RX_COMPLETE);
			usart5_send_str(g_usart5_rx_buff,g_usart5_Len);
		}
		#else
		{
			usart5_send_str(usart5_rx_test,sizeof(usart5_rx_test));
			// printf("串口5测试\n");
			// delay_ms(1000);	
			dwt_delay_ms(1000);
		}
		#endif
	}
}

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


/******************************************  (END OF FILE) **********************************************/




