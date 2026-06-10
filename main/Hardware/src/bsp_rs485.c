
#include "bsp_rs485.h"
#include "bsp.h"

/*
	5、485通信：串口3 ,波特率115200	
			485_TX : PD8
			485_RX : PD9
			485_RE : PD10	
*/

//#define USE_RS485_TX_DMA		/* 发送 DMA方式  */

// #define USE_RS485_INT		/* 中断方式 */
// #define USE_RS485_IDEL		/* DMA接收+空闲中断方式 */
//#define USE_RS485_TIMEOUT		/* DMA接收+超时检测中断方式 */

#define RS485_TX_GPIO_CLK				   RCU_GPIOD
#define RS485_TX_GPIO_PORT         GPIOD
#define RS485_TX_PIN               GPIO_PIN_8
#define RS485_TX_PIN_AF			       GPIO_AF_7

#define RS485_RX_GPIO_CLK				   RCU_GPIOD
#define RS485_RX_GPIO_PORT         GPIOD
#define RS485_RX_PIN               GPIO_PIN_9
#define RS485_RX_PIN_AF			       GPIO_AF_7

#define RS485_RE_GPIO_CLK				   RCU_GPIOD
#define RS485_RE_GPIO_PORT         GPIOD
#define RS485_RE_PIN               GPIO_PIN_10

#define RS485_USART_CLK            RCU_USART2
#define RS485_USART                USART2
#define RS485_IRQn						     USART2_IRQn
#define RS485_IRQHandler				   USART2_IRQHandler

#define RS485_DMA_CLK					     RCU_DMA0
#define RS485_DMAx						     DMA0
#define RS485_TX_DMA_CHANNEL			 DMA_CH3
#define RS485_RX_DMA_CHANNEL			 DMA_CH1

#define RS485_TX_DMA_PERIEN			   DMA_SUBPERI4
#define RS485_RX_DMA_PERIEN			   DMA_SUBPERI4

#define RS485_DMA_TX_IRQn				   DMA0_Channel3_IRQn
#define RS485_DMA_RX_IRQn				   DMA0_Channel1_IRQn

#define RS485_DMA_TX_IRQHandler		 DMA0_Channel3_IRQHandler
#define RS485_DMA_RX_IRQHandler 	 DMA0_Channel1_IRQHandler

enum {
	TRANSFER_WAIT,
	TRANSFER_TX_COMPLETE,
	TRANSFER_RX_COMPLETE,
	TRANSFER_ERROR,
};

#define RS485_RX_MAX  20
uint8_t *g_rs485_tx_buff = NULL;
uint8_t rs485_recv_buff[RS485_RX_MAX] = {0};
uint32_t g_rs485_TransferState = TRANSFER_WAIT;
uint8_t rs485_recv_len = 0;

//#define RS485_RE    PDout(10)
#define RS485_RE_SET()    gpio_bit_set(GPIOD, GPIO_PIN_10)   // RE 引脚置高
#define RS485_RE_RESET()  gpio_bit_reset(GPIOD, GPIO_PIN_10) // RE 引脚置低

/*
*********************************************************************************************************
*    函 数 名: bsp_InitRS485
*    功能说明: 485初始化函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitRS485(uint32_t baudrate)
{
	bsp_InitRs485_GPIO();
	bsp_InitRs485_Config(baudrate);
	bsp_InitRs485_DMA();
}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart5
*	功能说明: 初始化串口硬件 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitRs485_GPIO(void)
{
	/* enable GPIO clock */
	rcu_periph_clock_enable(RS485_TX_GPIO_CLK);
	rcu_periph_clock_enable(RS485_RX_GPIO_CLK);

	/* configure the USART5 TX pin and USART5 RX pin */
	gpio_af_set(RS485_TX_GPIO_PORT, RS485_TX_PIN_AF, RS485_TX_PIN);
	gpio_af_set(RS485_RX_GPIO_PORT, RS485_RX_PIN_AF, RS485_RX_PIN);

	/* configure USART5 TX as alternate function push-pull */
	gpio_mode_set(RS485_TX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, RS485_TX_PIN);
	gpio_output_options_set(RS485_TX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_TX_PIN);

	/* configure USART5 RX as alternate function push-pull */
	gpio_mode_set(RS485_RX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, RS485_RX_PIN);
	gpio_output_options_set(RS485_RX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_RX_PIN);
	
	gpio_mode_set(RS485_RE_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, RS485_RE_PIN);
	gpio_output_options_set(RS485_RE_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_RE_PIN);

}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart5_Config
*	功能说明: 初始化串口硬件 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitRs485_Config(uint32_t baudrate)
{
	/* enable USART clock */
	rcu_periph_clock_enable(RCU_USART2);
	
	/* USART5 configure */
	usart_deinit(USART2);
    usart_word_length_set(USART2, USART_WL_8BIT);
    usart_stop_bit_set(USART2, USART_STB_1BIT);
    usart_parity_config(USART2, USART_PM_NONE);
    usart_baudrate_set(USART2, baudrate);
    usart_receive_config(USART2, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART2, USART_TRANSMIT_ENABLE);
    usart_enable(USART2);
	
	#ifdef USE_RS485_INT
	{
		usart_interrupt_flag_clear(USART5, USART_INT_FLAG_RBNE);
		usart_interrupt_flag_clear(USART5, USART_INT_FLAG_TC);
		usart_interrupt_enable(USART5, USART_INT_RBNE);
		nvic_irq_enable(USART5_IRQn, 7, 0);
	}
	#endif

	#ifdef USE_RS485_IDEL
	{
		usart_interrupt_enable(USART5, USART_INT_IDLE);
		nvic_irq_enable(USART5_IRQn, 7, 0);
	}
	#endif

	#ifdef USE_RS485_TIMEOUT
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
void bsp_InitRs485_DMA(void)
{
	/* 配置TX DMA和NVIC */
	#ifdef USE_RS485_TX_DMA
	{
		dma_single_data_parameter_struct dma_tx_struct;
		
		/* enable DMA1 */
		rcu_periph_clock_enable(RS485_DMA_CLK);

		/* deinitialize DMA USART5 TX */
		dma_deinit(RS485_DMAx, RS485_TX_DMA_CHANNEL);
		dma_tx_struct.periph_addr         = (uint32_t)&USART_DATA(USART2);
		dma_tx_struct.memory0_addr        = (uint32_t)g_rs485_tx_buff;
		dma_tx_struct.direction           = DMA_MEMORY_TO_PERIPH;
		dma_tx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
		dma_tx_struct.priority            = DMA_PRIORITY_MEDIUM;
		dma_tx_struct.number              = RS485_RX_MAX;
		dma_tx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
		dma_tx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
		dma_tx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
		dma_single_data_mode_init(RS485_DMAx, RS485_TX_DMA_CHANNEL, &dma_tx_struct);
		dma_channel_subperipheral_select(RS485_DMAx, RS485_TX_DMA_CHANNEL, RS485_TX_DMA_PERIEN);
		
		/* USART DMA enable for transmission and reception */
		usart_dma_transmit_config(USART5, USART_TRANSMIT_DMA_ENABLE);	
		
		/* enable DMA1 channel7 transfer complete interrupt */
		dma_interrupt_flag_clear(RS485_DMAx, RS485_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
		dma_interrupt_enable(RS485_DMAx, RS485_TX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
		nvic_irq_enable(RS485_DMA_TX_IRQn, 7, 0);

	}
	#endif
	/* 配置RX DMA和NVIC */
	#if defined(USE_RS485_IDEL) || defined(USE_RS485_TIMEOUT)
	{
		dma_single_data_parameter_struct dma_rx_struct;

		/* enable DMA1 */
		rcu_periph_clock_enable(RS485_DMA_CLK);

		/* deinitialize DMA USART5 RX */
		dma_deinit(RS485_DMAx, RS485_RX_DMA_CHANNEL);
		dma_rx_struct.periph_addr         = (uint32_t)&USART_DATA(USART2);
		dma_rx_struct.memory0_addr        = (uint32_t)rs485_recv_buff;
		dma_rx_struct.direction           = DMA_PERIPH_TO_MEMORY;
		dma_rx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
		dma_rx_struct.priority            = DMA_PRIORITY_MEDIUM;
		dma_rx_struct.number              = RS485_RX_MAX;
		dma_rx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
		dma_rx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
		dma_rx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
		dma_single_data_mode_init(RS485_DMAx, RS485_RX_DMA_CHANNEL, &dma_rx_struct);
		dma_channel_subperipheral_select(RS485_DMAx, RS485_RX_DMA_CHANNEL, RS485_RX_DMA_PERIEN);
		
		/* USART DMA enable for transmission and reception */
		usart_dma_receive_config(USART5, USART_RECEIVE_DMA_ENABLE);	

		/* enable DMA1 channel2 transfer complete interrupt */
		// dma_interrupt_enable(USART5_DMAx, USART5_RX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
		// nvic_irq_enable(USART5_DMA_RX_IRQn, 7, 0);

		/* enable DMA1 channel2 */
		dma_channel_enable(RS485_DMAx, RS485_RX_DMA_CHANNEL);
	}
	#endif
}

/*
*********************************************************************************************************
*	函 数 名: rs485_send_char
*	功能说明: rs485发送1个字节。
*	形    参: 
*	@ch			: 待发送的字节数据
*	返 回 值: 无
*********************************************************************************************************
*/
void rs485_send_char(uint8_t ch)
{
	usart_data_transmit(USART2, (uint8_t)ch);
	while (RESET == usart_flag_get(USART2, USART_FLAG_TBE));
}

/*
*********************************************************************************************************
*	函 数 名: rs485_send_str
*	功能说明: rs485发送1个字节。
*	形    参:  
*	@buff		: 字符串指针
*	@len		: 发送数据长度
*	返 回 值: 无
*********************************************************************************************************
*/
void rs485_send_str(uint8_t *data, uint16_t len)
{
	uint32_t timeout = 0xFFFFF;  // 超时计数值
	g_rs485_TransferState = TRANSFER_WAIT;
	#ifdef USE_RS485_INT
	{
	  RS485_RE_SET();
		g_rs485_tx_buff = data;
		rs485_recv_len = len;
		usart_interrupt_enable(USART5, USART_INT_TBE);
		while(g_rs485_TransferState != TRANSFER_TX_COMPLETE);
		RS485_RE_RESET();
	}
	#elif defined(USE_RS485_TX_DMA)
	{	
	  RS485_RE_SET();  
		dma_memory_address_config(RS485_DMAx, RS485_TX_DMA_CHANNEL, DMA_MEMORY_0, (uint32_t)data);
		dma_transfer_number_config(RS485_DMAx, RS485_TX_DMA_CHANNEL, len);
		dma_channel_enable(RS485_DMAx, RS485_TX_DMA_CHANNEL);
		while(g_usart5_TransferState != TRANSFER_TX_COMPLETE);
		RS485_RE_RESET();
	}
	#else
	{
		RS485_RE_SET();
		while(len--) 
		{
			rs485_send_char(data[0]);
			data++;
		}
	while ((usart_flag_get(USART2, USART_FLAG_TC) == RESET) && (timeout--));
	RS485_RE_RESET();
	}
	#endif
	
}

/*
*********************************************************************************************************
*	函 数 名: RS485_IRQHandler
*	功能说明: 供中断服务程序调用，通用串口中断处理函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void RS485_IRQHandler(void)
{
	uint8_t res = 0;
		#ifdef USE_RS485_INT
	{
		static uint16_t rxcount = 0;
		static uint16_t txcount = 0;
		if((RESET != usart_interrupt_flag_get(USART2, USART_INT_FLAG_RBNE)) &&
			(RESET != usart_flag_get(USART2, USART_FLAG_RBNE))) 
		{
			/* receive data */
			rs485_recv_buff[rxcount++] = usart_data_receive(USART5);
			if(rxcount == 10) 
			{
				rxcount = 0;
				g_rs485_TransferState = TRANSFER_RX_COMPLETE;
				// usart_interrupt_disable(USART5, USART_INT_RBNE);
			}
		}
		if((RESET != usart_flag_get(USART2, USART_FLAG_TBE)) &&
			(RESET != usart_interrupt_flag_get(USART2, USART_INT_FLAG_TBE))) 
		{
			/* transmit data */
			usart_data_transmit(USART2, g_rs485_tx_buff[txcount++]);
			if(txcount == rs485_recv_len)
			{
				txcount = 0;
				g_rs485_TransferState = TRANSFER_TX_COMPLETE;
				usart_interrupt_disable(USART5, USART_INT_TBE);
			}
		}
	}
	#endif

	#ifdef USE_RS485_IDEL
	{
		if(RESET != usart_interrupt_flag_get(USART5, USART_INT_FLAG_IDLE))
		{
			/* clear IDLE flag */
			usart_data_receive(USART5);
			
			/* number of data received */
			rs485_recv_len = RS485_RX_MAX - (dma_transfer_number_get(RS485_DMAx, RS485_RX_DMA_CHANNEL));
			g_rs485_TransferState = TRANSFER_RX_COMPLETE;
			
			/* disable DMA and reconfigure */
			dma_channel_disable(RS485_DMAx, RS485_RX_DMA_CHANNEL);
			dma_flag_clear(RS485_DMAx, RS485_RX_DMA_CHANNEL, DMA_FLAG_FTF);
			dma_transfer_number_config(RS485_DMAx, RS485_RX_DMA_CHANNEL, RS485_RX_MAX);
			dma_channel_enable(RS485_DMAx, RS485_RX_DMA_CHANNEL);
		}
    }
	#endif

	#ifdef USE_RS485_TIMEOUT
	{
		if(RESET != usart_interrupt_flag_get(USART5, USART_INT_FLAG_RT))
		{
			usart_interrupt_flag_clear(USART5, USART_INT_FLAG_RT);
			
			/* number of data received */
			rs485_recv_len = RS485_RX_MAX - (dma_transfer_number_get(RS485_DMAx, RS485_RX_DMA_CHANNEL));
			g_rs485_TransferState = TRANSFER_RX_COMPLETE;
			
			/* disable DMA and reconfigure */
			dma_channel_disable(RS485_DMAx, RS485_RX_DMA_CHANNEL);
			dma_flag_clear(RS485_DMAx, RS485_RX_DMA_CHANNEL, DMA_FLAG_FTF);
			dma_transfer_number_config(RS485_DMAx, RS485_RX_DMA_CHANNEL, RS485_RX_MAX);
			dma_channel_enable(RS485_DMAx, RS485_RX_DMA_CHANNEL);
		}
	}
	#endif

	if((RESET != usart_flag_get(USART2, USART_FLAG_ORERR)) ||
		(RESET != usart_flag_get(USART2, USART_FLAG_FERR)) ||
		(RESET != usart_flag_get(USART2, USART_FLAG_PERR)))
	{
		USART_STAT0(USART2);
		res = usart_data_receive(USART2);
	}

}


/*
*********************************************************************************************************
*	函 数 名: rs485_test
*	功能说明: 485测试
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void rs485_test(void)
{
	while(1)
	{
		rs485_send_str((uint8_t*)"rs485_test\n",11);
		delay_ms(1000);		
	}
}
