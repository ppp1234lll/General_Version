/*
*********************************************************************************************************
*    函 数 名: 串口3
*    功能说明: 串口3模块
*    形    参: ZHLE
*    返 回 值: 
*********************************************************************************************************
*/
#include "bsp_uart3.h"
#include "bsp.h"

/*
*********************************************************************************************************
*                                 选择DMA，中断或者查询方式
*********************************************************************************************************
*/
// #define USE_UART3_TX_DMA           /* DMA发送 */

// #define USE_UART3_INT              /* 中断方式 */
// #define USE_UART3_IDEL             /* DMA接收+空闲中断方式 */

/*
*********************************************************************************************************
*                                时钟，引脚，DMA，中断等宏定义
*********************************************************************************************************
*/
#define UART3_TX_GPIO_CLK              RCU_GPIOC
#define UART3_TX_GPIO_PORT             GPIOC
#define UART3_TX_PIN                   GPIO_PIN_10
#define UART3_TX_PIN_AF                GPIO_AF_8

#define UART3_RX_GPIO_CLK              RCU_GPIOC
#define UART3_RX_GPIO_PORT             GPIOC
#define UART3_RX_PIN                   GPIO_PIN_11
#define UART3_RX_PIN_AF                GPIO_AF_8

#define UART3_DMA_CLK                  RCU_DMA0
#define UART3_DMAx                     DMA0
#define UART3_TX_DMA_CHANNEL           DMA_CH4
#define UART3_RX_DMA_CHANNEL           DMA_CH2

#define UART3_TX_DMA_PERIEN            DMA_SUBPERI4
#define UART3_RX_DMA_PERIEN            DMA_SUBPERI4

#define UART3_DMA_TX_IRQn              DMA0_Channel4_IRQn
#define UART3_DMA_RX_IRQn              DMA0_Channel2_IRQn   

#define UART3_DMA_TX_IRQHandler        DMA0_Channel4_IRQHandler
#define UART3_DMA_RX_IRQHandler        DMA0_Channel2_IRQHandler
/*
*********************************************************************************************************
*                                               变量
*********************************************************************************************************
*/
enum {
    TRANSFER_WAIT,
    TRANSFER_TX_COMPLETE,
    TRANSFER_RX_COMPLETE,
    TRANSFER_ERROR,
};

#define UART3_BUFF_SIZE  2048

uint8_t *g_uart3_tx_buff = NULL;
uint8_t g_uart3_rx_buff[UART3_BUFF_SIZE] = {0};
uint32_t g_uart3_TransferState = TRANSFER_WAIT;
uint32_t g_uart3_Len;   
    
/*
*********************************************************************************************************
*    函 数 名: bsp_InitUart3
*    功能说明: 初始化串口硬件
*    形    参: baudrate: 波特率
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUart3(uint32_t bound)
{
    bsp_InitUart3_GPIO();
    bsp_InitUart3_Config(bound);
    bsp_InitUart3_DMA();
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitUart3_GPIO
*    功能说明: 初始化串口3 GPIO引脚
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUart3_GPIO(void)
{
    /* enable GPIO clock */
    rcu_periph_clock_enable(UART3_TX_GPIO_CLK);
    rcu_periph_clock_enable(UART3_RX_GPIO_CLK);

    /* configure the USART3 TX pin and USART3 RX pin */
    gpio_af_set(UART3_TX_GPIO_PORT, UART3_TX_PIN_AF, UART3_TX_PIN);
    gpio_af_set(UART3_RX_GPIO_PORT, UART3_RX_PIN_AF, UART3_RX_PIN);

    /* configure USART3 TX as alternate function push-pull */
    gpio_mode_set(UART3_TX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, UART3_TX_PIN);
    gpio_output_options_set(UART3_TX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, UART3_TX_PIN);

    /* configure USART3 RX as alternate function push-pull */
    gpio_mode_set(UART3_RX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, UART3_RX_PIN);
    gpio_output_options_set(UART3_RX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, UART3_RX_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitUart3_Config
*    功能说明: 初始化串口3硬件
*    形    参: baudrate: 波特率
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUart3_Config(uint32_t bound)
{
    /* enable USART clock */
    rcu_periph_clock_enable(RCU_UART3);
    
    /* USART3 configure */
    usart_deinit(UART3);
    usart_word_length_set(UART3, USART_WL_8BIT);
    usart_stop_bit_set(UART3, USART_STB_1BIT);
    usart_parity_config(UART3, USART_PM_NONE);
    usart_baudrate_set(UART3, bound);
    usart_receive_config(UART3, USART_RECEIVE_ENABLE);
    usart_transmit_config(UART3, USART_TRANSMIT_ENABLE);
    usart_enable(UART3);

    #ifdef USE_UART3_INT
    {
        usart_interrupt_flag_clear(UART3, USART_INT_FLAG_RBNE);
        /* enable USART3 interrupt */
        usart_interrupt_enable(UART3, USART_INT_RBNE);
        nvic_irq_enable(UART3_IRQn, 7, 0);
    }
    #endif
    
    #ifdef USE_UART3_IDEL
    {
        usart_interrupt_enable(UART3, USART_INT_IDLE);
        nvic_irq_enable(UART3_IRQn, 7, 0);
    }
    #endif
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitUart3_DMA
*    功能说明: 初始化串口3DMA硬件
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUart3_DMA(void)
{
    /* 配置TX DMA和NVIC */
    #ifdef USE_UART3_TX_DMA
    {
        dma_single_data_parameter_struct dma_tx_struct;
        
        /* enable DMA0 */
        rcu_periph_clock_enable(UART3_DMA_CLK);

        /* deinitialize DMA USART3 TX */
        dma_deinit(UART3_DMAx, UART3_TX_DMA_CHANNEL);
        dma_tx_struct.periph_addr         = (uint32_t)&USART_DATA(UART3);
        dma_tx_struct.memory0_addr        = (uint32_t)g_uart3_tx_buff;
        dma_tx_struct.direction           = DMA_MEMORY_TO_PERIPH;
        dma_tx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
        dma_tx_struct.priority            = DMA_PRIORITY_MEDIUM;
        dma_tx_struct.number              = UART3_BUFF_SIZE;
        dma_tx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
        dma_tx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
        dma_tx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
        dma_single_data_mode_init(UART3_DMAx, UART3_TX_DMA_CHANNEL, &dma_tx_struct);
        dma_channel_subperipheral_select(UART3_DMAx, UART3_TX_DMA_CHANNEL, UART3_TX_DMA_PERIEN);
        
        /* USART DMA enable for transmission and reception */
        usart_dma_transmit_config(UART3, USART_TRANSMIT_DMA_ENABLE);    

        /* enable DMA3 channel7 transfer complete interrupt */
		dma_interrupt_flag_clear(UART3_DMAx, UART3_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
        dma_interrupt_enable(UART3_DMAx, UART3_TX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
        nvic_irq_enable(UART3_DMA_TX_IRQn, 7, 0);
    }
    #endif
    /* 配置RX DMA和NVIC */
    #if defined(USE_UART3_IDEL)
    {
        dma_single_data_parameter_struct dma_rx_struct;

        /* enable DMA0 */
        rcu_periph_clock_enable(UART3_DMA_CLK);

        /* deinitialize DMA USART3 RX */
        dma_deinit(UART3_DMAx, UART3_RX_DMA_CHANNEL);
        dma_rx_struct.periph_addr         = (uint32_t)&USART_DATA(UART3);
        dma_rx_struct.memory0_addr        = (uint32_t)g_uart3_rx_buff;
        dma_rx_struct.direction           = DMA_PERIPH_TO_MEMORY;
        dma_rx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
        dma_rx_struct.priority            = DMA_PRIORITY_MEDIUM;
        dma_rx_struct.number              = UART3_BUFF_SIZE;
        dma_rx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
        dma_rx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
        dma_rx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
        dma_single_data_mode_init(UART3_DMAx, UART3_RX_DMA_CHANNEL, &dma_rx_struct);
        dma_channel_subperipheral_select(UART3_DMAx, UART3_RX_DMA_CHANNEL, UART3_RX_DMA_PERIEN);
        
        /* USART DMA enable for transmission and reception */
        usart_dma_receive_config(UART3, USART_RECEIVE_DMA_ENABLE);    

        // dma_interrupt_enable(UART3_DMAx, UART3_RX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
        // nvic_irq_enable(UART3_DMA_RX_IRQn, 7, 0);

        /* enable DMA3 channel7 */
        dma_channel_enable(UART3_DMAx, UART3_RX_DMA_CHANNEL);
    }
    #endif
}
/*
*********************************************************************************************************
*    函 数 名: uart3_send_char
*    功能说明: 向串口发送1个字节。
*    形    参: 
*    返 回 值: 待发送的字节数据
*    返 回 值: 无
*********************************************************************************************************
*/
void uart3_send_char(uint8_t ch)
{
    usart_data_transmit(UART3, (uint8_t)ch);
    while (RESET == usart_flag_get(UART3, USART_FLAG_TBE));
}

/*
*********************************************************************************************************
*    函 数 名: uart3_send_str
*    功能说明: 向串口发送字符串。
*    形    参: 
*    返 回 值: 字符串指针
*    @len        : 发送数据长度
*    返 回 值: 无
*********************************************************************************************************
*/
void uart3_send_str(uint8_t *buff, uint16_t len)
{
    g_uart3_TransferState = TRANSFER_WAIT;
    #ifdef USE_UART3_INT
    {
        g_uart3_tx_buff = buff;
        g_uart3_Len = len;
        usart_interrupt_enable(UART3, USART_INT_TBE);   
        while(g_uart3_TransferState != TRANSFER_TX_COMPLETE);
    }
    #elif defined(USE_UART3_TX_DMA)
    {    
        dma_memory_address_config(UART3_DMAx, UART3_TX_DMA_CHANNEL, DMA_MEMORY_0, (uint32_t)buff);
        dma_transfer_number_config(UART3_DMAx, UART3_TX_DMA_CHANNEL, len);
        dma_channel_enable(UART3_DMAx, UART3_TX_DMA_CHANNEL);
        while(g_uart3_TransferState != TRANSFER_TX_COMPLETE);
    }
    #else
    {
        while(len--) 
        {
            uart3_send_char(buff[0]);
            buff++;
        }
    }
    #endif
}

#ifdef USE_UART3_TX_DMA
/*
*********************************************************************************************************
*    函 数 名: UART3_DMA_TX_IRQHandler
*    功能说明: 供中断服务程序调用，DMA发送完成中断处理函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void UART3_DMA_TX_IRQHandler(void)
{
    if(dma_interrupt_flag_get(UART3_DMAx, UART3_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF)) 
    {
        dma_interrupt_flag_clear(UART3_DMAx, UART3_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
        g_uart3_TransferState = TRANSFER_TX_COMPLETE;
    }
}
#endif
/*
*********************************************************************************************************
*    函 数 名: uart3_dma_rx_enable
*    功能说明: 使能UART3接收DMA。
*    形    参: 
*    返 回 值: 接收缓冲区指针
*    @len        : 接收数据长度
*    返 回 值: 无
*********************************************************************************************************
*/
void uart3_dma_rx_enable(void)  
{
    dma_memory_address_config(UART3_DMAx, UART3_RX_DMA_CHANNEL, DMA_MEMORY_0, (uint32_t)g_uart3_rx_buff);
    dma_transfer_number_config(UART3_DMAx, UART3_RX_DMA_CHANNEL, UART3_BUFF_SIZE);
    dma_channel_enable(UART3_DMAx, UART3_RX_DMA_CHANNEL);
}

/*
*********************************************************************************************************
*    函 数 名: UART3_IRQHandler
*    功能说明: 供中断服务程序调用，通用串口中断处理函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void UART3_IRQHandler(void)
{
    #ifdef USE_UART3_INT
    {
        static uint16_t rxcount = 0;
        static uint16_t txcount = 0;
        if((RESET != usart_interrupt_flag_get(UART3, USART_INT_FLAG_RBNE)) &&
            (RESET != usart_flag_get(UART3, USART_FLAG_RBNE))) 
        {
            /* receive data */
            g_uart3_rx_buff[rxcount++] = usart_data_receive(UART3);
            if(rxcount == 10) 
            {
                rxcount = 0;
                g_uart3_TransferState = TRANSFER_RX_COMPLETE;
                // usart_interrupt_disable(UART3, USART_INT_RBNE);
            }
        }
        if((RESET != usart_flag_get(UART3, USART_FLAG_TBE)) &&
            (RESET != usart_interrupt_flag_get(UART3, USART_INT_FLAG_TBE))) 
        {
            /* transmit data */
            usart_data_transmit(UART3, g_uart3_tx_buff[txcount++]);
            if(txcount == g_uart3_Len)
            {
                txcount = 0;
                g_uart3_TransferState = TRANSFER_TX_COMPLETE;
                usart_interrupt_disable(UART3, USART_INT_TBE);
            }
        }
    }
    #endif
    
    #ifdef USE_UART3_IDEL
    {
        if(RESET != usart_interrupt_flag_get(UART3, USART_INT_FLAG_IDLE))
        {
            /* clear IDLE flag */
            usart_data_receive(UART3);

            /* disable DMA and reconfigure */
            dma_channel_disable(UART3_DMAx, UART3_RX_DMA_CHANNEL);
            dma_flag_clear(UART3_DMAx, UART3_RX_DMA_CHANNEL, DMA_FLAG_FTF);
            
            /* number of data received */
            g_uart3_Len = UART3_BUFF_SIZE - (dma_transfer_number_get(UART3_DMAx, UART3_RX_DMA_CHANNEL));
            g_uart3_TransferState = TRANSFER_RX_COMPLETE;
            
            uart3_dma_rx_enable();
        }
    }
    #endif  

    if((RESET != usart_flag_get(UART3, USART_FLAG_ORERR)) ||
        (RESET != usart_flag_get(UART3, USART_FLAG_FERR)) ||
        (RESET != usart_flag_get(UART3, USART_FLAG_PERR)))
    {
        USART_STAT0(UART3);
        usart_data_receive(UART3);
    }
}

/*
*********************************************************************************************************
*    函 数 名: uart3_rx_get_frame
*    功能说明: 获取接收到的一帧数据
*    形    参: 无
*    返 回 值: 接收到的一帧数据指针，或NULL
*********************************************************************************************************
*/
uint8_t *uart3_rx_get_frame(void)
{
    if (g_uart3_TransferState == TRANSFER_RX_COMPLETE)
    {
        g_uart3_TransferState = TRANSFER_WAIT;
        g_uart3_rx_buff[g_uart3_Len] = '\0';
        return g_uart3_rx_buff;
    }
    else
    {
        return NULL;
    }
}

/*
*********************************************************************************************************
*    函 数 名: uart3_test
*    功能说明: UART3测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void uart3_test(void)
{                                                                                                                                                                                                             
    while(1)
    {
        #if defined(USE_UART3_INT)
        {
            while(g_uart3_TransferState != TRANSFER_RX_COMPLETE);
            uart3_send_str(g_uart3_rx_buff,10);
        }
        #elif defined(USE_UART3_IDEL) || defined(USE_UART3_TX_DMA)
        {
            while(g_uart3_TransferState != TRANSFER_RX_COMPLETE);
            uart3_send_str(g_uart3_rx_buff,g_uart3_Len);
        }
        #else
        {
            uart3_send_str("uart3_rx_test\n",12); 
            dwt_delay_ms(1000);
        }
        #endif
    }
}

/******************************************  (END OF FILE) **********************************************/


