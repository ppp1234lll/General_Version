/********************************************************************************
* @File name  : 串口0
* @Description: 串口0-对应4G模块
* @Author     : ZHLE
*  Version Date        Modification Description
	12、ML307（4G模块）：串口0，波特率115200，引脚分配为：   
        4G-TXD：    PA9
        4G-RXD：    PA10
********************************************************************************/

#include "bsp_usart0.h"
#include "bsp.h"
// #include "GPRS.h"
/*
*********************************************************************************************************
*	                             选择DMA，中断或者查询方式
*********************************************************************************************************
*/
// #define USE_USART0_TX_DMA    /* 发送 DMA方式  */
 #define USE_USART0_RX_DMA    /* 接收 DMA方式  */
// #define USE_USART0_RX_INT    /* 中断方式 */
//#define USE_USART0_DMA_POLL   /* 查询方式 */
/*
*********************************************************************************************************
*	                            时钟，引脚，DMA，中断等宏定义
*********************************************************************************************************
*/
#define USART0_TX_GPIO_CLK				RCU_GPIOA
#define USART0_TX_GPIO_PORT				GPIOA
#define USART0_TX_PIN					GPIO_PIN_9
#define USART0_TX_PIN_AF				GPIO_AF_7

#define USART0_RX_GPIO_CLK				RCU_GPIOA
#define USART0_RX_GPIO_PORT				GPIOA
#define USART0_RX_PIN					GPIO_PIN_10
#define USART0_RX_PIN_AF				GPIO_AF_7

#define USART0_DMA_CLK					RCU_DMA1
#define USART0_DMAx						DMA1
#define USART0_TX_DMA_CHANNEL			DMA_CH7
#define USART0_RX_DMA_CHANNEL			DMA_CH5

#define USART0_TX_DMA_PERIEN			DMA_SUBPERI4
#define USART0_RX_DMA_PERIEN			DMA_SUBPERI4

#define USART0_DMA_TX_IRQn				DMA1_Channel7_IRQn
#define USART0_DMA_RX_IRQn				DMA1_Channel5_IRQn

/*
*********************************************************************************************************
*	                                           变量
*********************************************************************************************************
*/
enum {
	TRANSFER_WAIT,
	TRANSFER_COMPLETE,
	TRANSFER_ERROR
};

#define USART0_BUFF_SIZE  2048

uint8_t g_usart0_rx_buff[USART0_BUFF_SIZE] = {0};
uint8_t g_usart0_tx_buff[USART0_BUFF_SIZE] = {0};
uint32_t g_usart0_TransferState = TRANSFER_WAIT;
uint32_t g_usart0_Len;	
/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart0
*	功能说明: 初始化串口硬件 
*	形    参: baudrate: 波特率
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart0(uint32_t bound)
{
	bsp_InitUsart0_GPIO();
	bsp_InitUsart0_Config(bound);
	bsp_InitUsart0_DMA();
}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart0
*	功能说明: 初始化串口硬件 
*	形    参: baudrate: 波特率
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart0_GPIO(void)
{
	/* enable GPIO clock */
	rcu_periph_clock_enable(USART0_TX_GPIO_CLK);
	rcu_periph_clock_enable(USART0_RX_GPIO_CLK);

	/* configure the USART0 TX pin and USART0 RX pin */
	gpio_af_set(USART0_TX_GPIO_PORT, USART0_TX_PIN_AF, USART0_TX_PIN);
	gpio_af_set(USART0_RX_GPIO_PORT, USART0_RX_PIN_AF, USART0_RX_PIN);

	/* configure USART0 TX as alternate function push-pull */
	gpio_mode_set(USART0_TX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART0_TX_PIN);
	gpio_output_options_set(USART0_TX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART0_TX_PIN);

	/* configure USART0 RX as alternate function push-pull */
	gpio_mode_set(USART0_RX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART0_RX_PIN);
	gpio_output_options_set(USART0_RX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART0_RX_PIN);

}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart0
*	功能说明: 初始化串口硬件 
*	形    参: baudrate: 波特率
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart0_Config(uint32_t bound)
{
	/* enable USART clock */
	rcu_periph_clock_enable(RCU_USART0);
	
	/* USART0 configure */
	usart_deinit(USART0);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_baudrate_set(USART0, bound);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_enable(USART0);

	#ifdef USE_USART0_RX_INT
	{
		usart_interrupt_flag_clear(USART0, USART_INT_FLAG_RBNE);
		/* enable USART0 interrupt */
		usart_interrupt_enable(USART0, USART_INT_RBNE);
		nvic_irq_enable(USART0_IRQn, 7, 0);
	}
	#endif
	
		#ifdef USE_USART0_RX_DMA
	{
		usart_interrupt_enable(USART0, USART_INT_IDLE);
		nvic_irq_enable(USART0_IRQn, 7, 0);
	}
	#endif

}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart0_DMA
*	功能说明: 初始化串口DMA硬件 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart0_DMA(void)
{
	/* 配置TX DMA和NVIC */
	#ifdef USE_USART0_TX_DMA
	{
		dma_single_data_parameter_struct dma_tx_struct;
		
		/* enable DMA1 */
		rcu_periph_clock_enable(USART0_DMA_CLK);

		/* deinitialize DMA USART0 TX */
		dma_deinit(USART0_DMAx, USART0_TX_DMA_CHANNEL);
		dma_tx_struct.periph_addr         = (uint32_t)&USART_DATA(USART0);
		dma_tx_struct.memory0_addr        = (uint32_t)g_usart0_tx_buff;
		dma_tx_struct.direction           = DMA_MEMORY_TO_PERIPH;
		dma_tx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
		dma_tx_struct.priority            = DMA_PRIORITY_MEDIUM;
		dma_tx_struct.number              = USART0_BUFF_SIZE;
		dma_tx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
		dma_tx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
		dma_tx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
		dma_single_data_mode_init(USART0_DMAx, USART0_TX_DMA_CHANNEL, &dma_tx_struct);
		dma_channel_subperipheral_select(USART0_DMAx, USART0_TX_DMA_CHANNEL, USART0_TX_DMA_PERIEN);
		
		/* USART DMA enable for transmission and reception */
		usart_dma_transmit_config(USART0, USART_TRANSMIT_DMA_ENABLE);	

		/* enable DMA1 channel7 transfer complete interrupt */
		dma_interrupt_enable(USART0_DMAx, USART0_TX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
		nvic_irq_enable(USART0_DMA_TX_IRQn, 7, 0);
	}
	#endif
	/* 配置RX DMA和NVIC */
	#ifdef USE_USART0_RX_DMA
	{
		dma_single_data_parameter_struct dma_rx_struct;

		/* enable DMA1 */
		rcu_periph_clock_enable(USART0_DMA_CLK);

		/* deinitialize DMA USART0 RX */
		dma_deinit(USART0_DMAx, USART0_RX_DMA_CHANNEL);
		dma_rx_struct.periph_addr         = (uint32_t)&USART_DATA(USART0);
		dma_rx_struct.memory0_addr        = (uint32_t)g_usart0_rx_buff;
		dma_rx_struct.direction           = DMA_PERIPH_TO_MEMORY;
		dma_rx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
		dma_rx_struct.priority            = DMA_PRIORITY_MEDIUM;
		dma_rx_struct.number              = USART0_BUFF_SIZE;
		dma_rx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
		dma_rx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
		dma_rx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
		dma_single_data_mode_init(USART0_DMAx, USART0_RX_DMA_CHANNEL, &dma_rx_struct);
		dma_channel_subperipheral_select(USART0_DMAx, USART0_RX_DMA_CHANNEL, USART0_RX_DMA_PERIEN);
		
		/* USART DMA enable for transmission and reception */
		usart_dma_receive_config(USART0, USART_RECEIVE_DMA_ENABLE);	

		/* enable DMA1 channel2 transfer complete interrupt */
		dma_interrupt_enable(USART0_DMAx, USART0_RX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
//		nvic_irq_enable(USART0_DMA_RX_IRQn, 7, 0);
	}
	#endif
}
/*
*********************************************************************************************************
*	函 数 名: usart1_send_char
*	功能说明: 向串口发送1个字节。
*	形    参: 
*	@ch			: 待发送的字节数据
*	返 回 值: 无
*********************************************************************************************************
*/
void usart0_send_char(uint8_t ch)
{
	usart_data_transmit(USART0, (uint8_t)ch);
	while (RESET == usart_flag_get(USART0, USART_FLAG_TBE));
}

/*
*********************************************************************************************************
*	函 数 名: usart0_send_str
*	功能说明: 向串口发送字符串。
*	形    参:  
*	@buff		: 字符串指针
*	@len		: 发送数据长度
*	返 回 值: 无
*********************************************************************************************************
*/
void usart0_send_str(uint8_t *buff, uint16_t len)
{
	while(len--) 
	{
		usart0_send_char(buff[0]);
		buff++;
	}
}
/*
*********************************************************************************************************
*	函 数 名: USART0_IRQHandler
*	功能说明: 供中断服务程序调用，通用串口中断处理函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void USART0_IRQHandler(void)
{
#ifdef USE_USART0_RX_DMA
	uint16_t size = 0;
	
		if(RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE))
		{
			/* clear IDLE flag */
			USART_STAT0(USART0);   // 第一步：读状态寄存器
			usart_data_receive(USART0);
			
			/* number of data received */
			g_usart0_Len = USART0_BUFF_SIZE - (dma_transfer_number_get(USART0_DMAx, USART0_RX_DMA_CHANNEL));
//			g_usart5_TransferState = TRANSFER_RX_COMPLETE;
			gprs_get_receive_data_function(g_usart0_rx_buff,g_usart0_Len);
			
			/* disable DMA and reconfigure */
			dma_channel_disable(USART0_DMAx, USART0_RX_DMA_CHANNEL);
			dma_flag_clear(USART0_DMAx, USART0_RX_DMA_CHANNEL, DMA_FLAG_FTF);
			dma_transfer_number_config(USART0_DMAx, USART0_RX_DMA_CHANNEL, USART0_BUFF_SIZE);
			dma_channel_enable(USART0_DMAx, USART0_RX_DMA_CHANNEL);
		}
#else
	static uint8_t rxcount = 0;
		
    if((RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE)) &&
		(RESET != usart_flag_get(USART0, USART_FLAG_RBNE))) 
	{
        /* receive data */
        g_usart0_rx_buff[rxcount++] = usart_data_receive(USART0);
        if(rxcount == USART0_BUFF_SIZE) 
		{
			rxcount = 0;
            usart_interrupt_disable(USART0, USART_INT_RBNE);
        }
		gprs_get_receive_data_function(g_usart0_rx_buff,rxcount);
    }
#endif
}



