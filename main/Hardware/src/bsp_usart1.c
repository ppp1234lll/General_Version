/********************************************************************************
* @File name  : GPS模块
* @Description: 串口2-对应GPS
* @Author     : ZHLE
*  Version Date        Modification Description
	13、GPS(4G模块)： 串口2， 波特率：9600，引脚分配为： 
		BDS_TX：    PD5
        BDS_RX：    PD6	
********************************************************************************/

#include "bsp_usart1.h"
//#include "bsp_dma.h"

#define USART1_TX_GPIO_CLK               RCU_GPIOD
#define USART1_TX_GPIO_PORT              GPIOD
#define USART1_TX_PIN                    GPIO_PIN_5
#define USART1_TX_PIN_AF			           GPIO_AF_7

#define USART1_RX_GPIO_CLK               RCU_GPIOD
#define USART1_RX_GPIO_PORT              GPIOD
#define USART1_RX_PIN                    GPIO_PIN_6
#define USART1_RX_PIN_AF			           GPIO_AF_7

#define USART1_RX_DMA 

#ifdef USART1_RX_DMA
#define USART1_TX_DMA_STREAM             DMA1_Stream6
#define USART1_RX_DMA_STREAM             DMA1_Stream5

#define USART1_TX_DMA_CH               	 DMA_Channel_4
#define USART1_RX_DMA_CH                 DMA_Channel_4
#endif

/* 参数 */
#define USART1_RX_MAX  512

uint8_t usart1_rx_buff[USART1_RX_MAX] = {0};

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUart2
*	功能说明: 初始化串口硬件 
*	形    参: baudrate: 波特率
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart1(uint32_t baudrate)
{
	bsp_InitUsart1_GPIO();
	bsp_InitUsart1_Config(baudrate);
	bsp_InitUsart1_DMA();                     // 使能串口 
}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart5
*	功能说明: 初始化串口硬件 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart1_GPIO(void)
{
	/* enable GPIO clock */
	rcu_periph_clock_enable(USART1_TX_GPIO_CLK);
	rcu_periph_clock_enable(USART1_RX_GPIO_CLK);

	/* configure the USART5 TX pin and USART5 RX pin */
	gpio_af_set(USART1_TX_GPIO_PORT, USART1_TX_PIN_AF, USART1_TX_PIN);
	gpio_af_set(USART1_RX_GPIO_PORT, USART1_RX_PIN_AF, USART1_RX_PIN);

	/* configure USART5 TX as alternate function push-pull */
	gpio_mode_set(USART1_TX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART1_TX_PIN);
	gpio_output_options_set(USART1_TX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART1_TX_PIN);

	/* configure USART5 RX as alternate function push-pull */
	gpio_mode_set(USART1_RX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART1_RX_PIN);
	gpio_output_options_set(USART1_RX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART1_RX_PIN);

}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart5_Config
*	功能说明: 初始化串口硬件 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart1_Config(uint32_t baudrate)
{
	/* enable USART clock */
	rcu_periph_clock_enable(RCU_USART1);
	
	/* USART5 configure */
	usart_deinit(USART1);
    usart_word_length_set(USART1, USART_WL_8BIT);
    usart_stop_bit_set(USART1, USART_STB_1BIT);
    usart_parity_config(USART1, USART_PM_NONE);
    usart_baudrate_set(USART1, baudrate);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
    usart_enable(USART1);
	
	#ifdef USE_USART1_INT
	{
		usart_interrupt_flag_clear(USART1, USART_INT_FLAG_RBNE);
		usart_interrupt_flag_clear(USART1, USART_INT_FLAG_TC);
		usart_interrupt_enable(USART1, USART_INT_RBNE);
		nvic_irq_enable(USART1_IRQn, 7, 0);
	}
	#endif

	#ifdef USE_USART1_IDEL
	{
		usart_interrupt_enable(USART1, USART_INT_IDLE);
		nvic_irq_enable(USART1_IRQn, 7, 0);
	}
	#endif

	#ifdef USE_USART1_TIMEOUT
	{
		/* enable the USART receive timeout and configure the time of timeout */
		usart_interrupt_flag_clear(USART1, USART_INT_FLAG_RT);
		/*向寄存器填入需要超时的长度，单位为一个波特时长,3.5个字节*11波特长度 = 39  */
		usart_receiver_timeout_threshold_config(USART1, 39);
		usart_receiver_timeout_enable(USART1);
		usart_interrupt_enable(USART1, USART_INT_RT);
		nvic_irq_enable(USART1_IRQn, 7, 0);
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
void bsp_InitUsart1_DMA(void)
{
	/* 配置TX DMA和NVIC */
	#ifdef USE_USART1_TX_DMA
	{
		dma_single_data_parameter_struct dma_tx_struct;
		
		/* enable DMA1 */
		rcu_periph_clock_enable(USART1_DMA_CLK);

		/* deinitialize DMA USART5 TX */
		dma_deinit(USART1_DMAx, USART1_TX_DMA_CHANNEL);
		dma_tx_struct.periph_addr         = (uint32_t)&USART_DATA(USART1);
		dma_tx_struct.memory0_addr        = (uint32_t)g_usart1_tx_buff;
		dma_tx_struct.direction           = DMA_MEMORY_TO_PERIPH;
		dma_tx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
		dma_tx_struct.priority            = DMA_PRIORITY_MEDIUM;
		dma_tx_struct.number              = USART1_BUFF_SIZE;
		dma_tx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
		dma_tx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
		dma_tx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
		dma_single_data_mode_init(USART1_DMAx, USART1_TX_DMA_CHANNEL, &dma_tx_struct);
		dma_channel_subperipheral_select(USART1_DMAx, USART1_TX_DMA_CHANNEL, USART1_TX_DMA_PERIEN);
		
		/* USART DMA enable for transmission and reception */
		usart_dma_transmit_config(USART1, USART_TRANSMIT_DMA_ENABLE);	
		
		/* enable DMA1 channel7 transfer complete interrupt */
		dma_interrupt_flag_clear(USART1_DMAx, USART1_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
		dma_interrupt_enable(USART1_DMAx, USART1_TX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
		nvic_irq_enable(USART1_DMA_TX_IRQn, 7, 0);

	}
	#endif
	/* 配置RX DMA和NVIC */
	#if defined(USE_USART1_IDEL) || defined(USE_USART1_TIMEOUT)
	{
		dma_single_data_parameter_struct dma_rx_struct;

		/* enable DMA1 */
		rcu_periph_clock_enable(USART1_DMA_CLK);

		/* deinitialize DMA USART5 RX */
		dma_deinit(USART1_DMAx, USART1_RX_DMA_CHANNEL);
		dma_rx_struct.periph_addr         = (uint32_t)&USART_DATA(USART1);
		dma_rx_struct.memory0_addr        = (uint32_t)g_usart1_rx_buff;
		dma_rx_struct.direction           = DMA_PERIPH_TO_MEMORY;
		dma_rx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
		dma_rx_struct.priority            = DMA_PRIORITY_MEDIUM;
		dma_rx_struct.number              = USART1_BUFF_SIZE;
		dma_rx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
		dma_rx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
		dma_rx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
		dma_single_data_mode_init(USART1_DMAx, USART1_RX_DMA_CHANNEL, &dma_rx_struct);
		dma_channel_subperipheral_select(USART1_DMAx, USART1_RX_DMA_CHANNEL, USART1_RX_DMA_PERIEN);
		
		/* USART DMA enable for transmission and reception */
		usart_dma_receive_config(USART1, USART_RECEIVE_DMA_ENABLE);	

		/* enable DMA1 channel2 transfer complete interrupt */
		// dma_interrupt_enable(USART5_DMAx, USART5_RX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
		// nvic_irq_enable(USART5_DMA_RX_IRQn, 7, 0);

		/* enable DMA1 channel2 */
		dma_channel_enable(USART1_DMAx, USART1_RX_DMA_CHANNEL);
	}
	#endif
}

/*
*********************************************************************************************************
*	函 数 名: usart2_send_char
*	功能说明: 向串口2发送1个字节。
*	形    参: 
*	@ch			: 待发送的字节数据
*	返 回 值: 无
*********************************************************************************************************
*/
void usart1_send_char(uint8_t ch)
{
	usart_data_transmit(USART1, (uint8_t)ch);
	while (RESET == usart_flag_get(USART1, USART_FLAG_TBE));
}

/*
*********************************************************************************************************
*	函 数 名: usart2_send_str
*	功能说明: 向串口2发送字符串。
*	形    参:  
*	@buff		: 字符串指针
*	@len		: 发送数据长度
*	返 回 值: 无
*********************************************************************************************************
*/
void usart1_send_str(uint8_t *buff, uint16_t len)
{
	while(len--) {
		usart1_send_char(buff[0]);
		buff++;
	}
}

/*
*********************************************************************************************************
*	函 数 名: USART2_IRQHandler
*	功能说明: 供中断服务程序调用，通用串口中断处理函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void USART1_IRQHandler(void)
{
#ifdef USART2_RX_DMA
	uint16_t size = 0;
	
	if (USART_GetITStatus(USART2, USART_IT_IDLE) != RESET) 
	{
		USART_ClearITPendingBit(USART2, USART_IT_IDLE);
		USART_ReceiveData(USART2);
		
		DMA_Cmd(USART2_RX_DMA_STREAM, DISABLE);/* 停止DMA */
		size = USART2_RX_MAX - DMA_GetCurrDataCounter(USART2_RX_DMA_STREAM);
//		gps_get_receive_data_function(usart2_rx_buff,size);

		DMA_Enable(USART2_RX_DMA_STREAM,USART2_RX_MAX);/* 设置传输模式 */
	}
#else
	static uint8_t test = 0;
	uint8_t res = 0;
	
		if((RESET != usart_flag_get(USART5, USART_FLAG_ORERR)) ||
		(RESET != usart_flag_get(USART5, USART_FLAG_FERR)) ||
		(RESET != usart_flag_get(USART5, USART_FLAG_PERR)))
		{
		USART_STAT0(USART5);
		res = usart_data_receive(USART5);
//		printf("%c",res);
		usart1_rx_buff[test++] = res;
	}
#endif
}





