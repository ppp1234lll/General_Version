/*
*********************************************************************************************************
*    函 数 名: 串口7
*    功能说明: 串口7模块
*    形    参: ZHLE
*    返 回 值: 
*********************************************************************************************************
*/
#include "bsp_uart7.h"
#include "bsp.h"

/*
*********************************************************************************************************
*                                 选择DMA，中断或者查询方式
*********************************************************************************************************
*/
// #define USE_UART7_TX_DMA           /* DMA发送 */

// #define USE_UART7_INT              /* 中断方式 */
// #define USE_UART7_IDEL             /* DMA接收+空闲中断方式 */

/*
*********************************************************************************************************
*                                时钟，引脚，DMA，中断等宏定义
*********************************************************************************************************
*/
#define UART7_TX_GPIO_CLK              RCU_GPIOE
#define UART7_TX_GPIO_PORT             GPIOE
#define UART7_TX_PIN                   GPIO_PIN_1
#define UART7_TX_PIN_AF                GPIO_AF_8

#define UART7_RX_GPIO_CLK              RCU_GPIOE
#define UART7_RX_GPIO_PORT             GPIOE
#define UART7_RX_PIN                   GPIO_PIN_0
#define UART7_RX_PIN_AF                GPIO_AF_8

#define UART7_DMA_CLK                  RCU_DMA0
#define UART7_DMAx                     DMA0
#define UART7_TX_DMA_CHANNEL           DMA_CH0
#define UART7_RX_DMA_CHANNEL           DMA_CH6

#define UART7_TX_DMA_PERIEN            DMA_SUBPERI5
#define UART7_RX_DMA_PERIEN            DMA_SUBPERI5

#define UART7_DMA_TX_IRQn              DMA0_Channel0_IRQn
#define UART7_DMA_RX_IRQn              DMA0_Channel6_IRQn   

#define UART7_DMA_TX_IRQHandler        DMA0_Channel0_IRQHandler
#define UART7_DMA_RX_IRQHandler        DMA0_Channel6_IRQHandler
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

#define UART7_BUFF_SIZE  2048

uint8_t *g_uart7_tx_buff = NULL;
uint8_t g_uart7_rx_buff[UART7_BUFF_SIZE] = {0};
uint32_t g_uart7_TransferState = TRANSFER_WAIT;
uint32_t g_uart7_Len;   
    
/*
*********************************************************************************************************
*    函 数 名: bsp_InitUart7
*    功能说明: 初始化串口7硬件
*    形    参: baudrate: 波特率
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUart7(uint32_t bound)
{
    bsp_InitUart7_GPIO();
    bsp_InitUart7_Config(bound);
    bsp_InitUart7_DMA();
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitUart7_GPIO
*    功能说明: 初始化串口7 GPIO引脚
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUart7_GPIO(void)
{
    /* enable GPIO clock */
    rcu_periph_clock_enable(UART7_TX_GPIO_CLK);
    rcu_periph_clock_enable(UART7_RX_GPIO_CLK);

    /* configure the USART7 TX pin and USART7 RX pin */
    gpio_af_set(UART7_TX_GPIO_PORT, UART7_TX_PIN_AF, UART7_TX_PIN);
    gpio_af_set(UART7_RX_GPIO_PORT, UART7_RX_PIN_AF, UART7_RX_PIN);

    /* configure USART7 TX as alternate function push-pull */
    gpio_mode_set(UART7_TX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, UART7_TX_PIN);
    gpio_output_options_set(UART7_TX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, UART7_TX_PIN);

    /* configure USART7 RX as alternate function push-pull */
    gpio_mode_set(UART7_RX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, UART7_RX_PIN);
    gpio_output_options_set(UART7_RX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, UART7_RX_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitUart7_Config
*    功能说明: 初始化串口7硬件
*    形    参: baudrate: 波特率
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUart7_Config(uint32_t bound)
{
    /* enable USART clock */
    rcu_periph_clock_enable(RCU_UART7);
    
    /* USART7 configure */
    usart_deinit(UART7);
    usart_word_length_set(UART7, USART_WL_8BIT);
    usart_stop_bit_set(UART7, USART_STB_1BIT);
    usart_parity_config(UART7, USART_PM_NONE);
    usart_baudrate_set(UART7, bound);
    usart_receive_config(UART7, USART_RECEIVE_ENABLE);
    usart_transmit_config(UART7, USART_TRANSMIT_ENABLE);
    usart_enable(UART7);    

    #ifdef USE_UART7_INT    
    {
        usart_interrupt_flag_clear(UART7, USART_INT_FLAG_RBNE);
        /* enable USART7 interrupt */
        usart_interrupt_enable(UART7, USART_INT_RBNE);
        nvic_irq_enable(UART7_IRQn, 7, 0);
    }
    #endif
    
    #ifdef USE_UART7_IDEL
    {
        usart_interrupt_enable(UART7, USART_INT_IDLE);
        nvic_irq_enable(UART7_IRQn, 7, 0);
    }
    #endif
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitUart7_DMA
*    功能说明: 初始化串口7DMA硬件
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUart7_DMA(void)
{
    /* 配置TX DMA和NVIC */
    #ifdef USE_UART7_TX_DMA
    {
        dma_single_data_parameter_struct dma_tx_struct;
        
        /* enable DMA0 */
        rcu_periph_clock_enable(UART7_DMA_CLK);

        /* deinitialize DMA USART7 TX */
        dma_deinit(UART7_DMAx, UART7_TX_DMA_CHANNEL);
        dma_tx_struct.periph_addr         = (uint32_t)&USART_DATA(UART7);
        dma_tx_struct.memory0_addr        = (uint32_t)g_uart7_tx_buff;
        dma_tx_struct.direction           = DMA_MEMORY_TO_PERIPH;
        dma_tx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
        dma_tx_struct.priority            = DMA_PRIORITY_MEDIUM;
        dma_tx_struct.number              = UART7_BUFF_SIZE;
        dma_tx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
        dma_tx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
        dma_tx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
        dma_single_data_mode_init(UART7_DMAx, UART7_TX_DMA_CHANNEL, &dma_tx_struct);    
        dma_channel_subperipheral_select(UART7_DMAx, UART7_TX_DMA_CHANNEL, UART7_TX_DMA_PERIEN);
        
        /* USART DMA enable for transmission and reception */
        usart_dma_transmit_config(UART7, USART_TRANSMIT_DMA_ENABLE);                                                                                                                                                                                                                                    

        /* enable DMA7 transfer complete interrupt */
		dma_interrupt_flag_clear(UART7_DMAx, UART7_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
        dma_interrupt_enable(UART7_DMAx, UART7_TX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
        nvic_irq_enable(UART7_DMA_TX_IRQn, 7, 0);
    }
    #endif
    /* 配置RX DMA和NVIC */
    #if defined(USE_UART7_IDEL)
    {
        dma_single_data_parameter_struct dma_rx_struct;

        /* enable DMA0 */
        rcu_periph_clock_enable(UART7_DMA_CLK);

        /* deinitialize DMA USART7 RX */
        dma_deinit(UART7_DMAx, UART7_RX_DMA_CHANNEL);
        dma_rx_struct.periph_addr         = (uint32_t)&USART_DATA(UART7);
        dma_rx_struct.memory0_addr        = (uint32_t)g_uart7_rx_buff;
        dma_rx_struct.direction           = DMA_PERIPH_TO_MEMORY;
        dma_rx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
        dma_rx_struct.priority            = DMA_PRIORITY_MEDIUM;
        dma_rx_struct.number              = UART7_BUFF_SIZE;
        dma_rx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
        dma_rx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
        dma_rx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
        dma_single_data_mode_init(UART7_DMAx, UART7_RX_DMA_CHANNEL, &dma_rx_struct);
        dma_channel_subperipheral_select(UART7_DMAx, UART7_RX_DMA_CHANNEL, UART7_RX_DMA_PERIEN);
        
        /* USART DMA enable for transmission and reception */
        usart_dma_receive_config(UART7, USART_RECEIVE_DMA_ENABLE);    

        // dma_interrupt_enable(UART7_DMAx, UART7_RX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
        // nvic_irq_enable(UART7_DMA_RX_IRQn, 7, 0);

        /* enable DMA7 channel7 */
        dma_channel_enable(UART7_DMAx, UART7_RX_DMA_CHANNEL);
    }
    #endif
}
/*
*********************************************************************************************************
*    函 数 名: uart7_send_char
*    功能说明: 向串口发送1个字节。
*    形    参: 
*    返 回 值: 待发送的字节数据
*    返 回 值: 无
*********************************************************************************************************
*/
void uart7_send_char(uint8_t ch)
{
    usart_data_transmit(UART7, (uint8_t)ch);
    while (RESET == usart_flag_get(UART7, USART_FLAG_TBE));
}

/*
*********************************************************************************************************
*    函 数 名: uart7_send_str
*    功能说明: 向串口发送字符串。
*    形    参: 
*    返 回 值: 字符串指针
*    @len        : 发送数据长度
*    返 回 值: 无
*********************************************************************************************************
*/
void uart7_send_str(uint8_t *buff, uint16_t len)
{
    g_uart7_TransferState = TRANSFER_WAIT;
    #ifdef USE_UART7_INT
    {
        g_uart7_tx_buff = buff;
        g_uart7_Len = len;
        usart_interrupt_enable(UART7, USART_INT_TBE);   
        while(g_uart7_TransferState != TRANSFER_TX_COMPLETE);
    }
    #elif defined(USE_UART7_TX_DMA)
    {    
        dma_memory_address_config(UART7_DMAx, UART7_TX_DMA_CHANNEL, DMA_MEMORY_0, (uint32_t)buff);
        dma_transfer_number_config(UART7_DMAx, UART7_TX_DMA_CHANNEL, len);
        dma_channel_enable(UART7_DMAx, UART7_TX_DMA_CHANNEL);
        while(g_uart7_TransferState != TRANSFER_TX_COMPLETE);
    }
    #else
    {
        while(len--) 
        {
            uart7_send_char(buff[0]);
            buff++;
        }
    }
    #endif
}

#ifdef USE_UART7_TX_DMA
/*
*********************************************************************************************************
*    函 数 名: UART7_DMA_TX_IRQHandler
*    功能说明: 供中断服务程序调用，DMA发送完成中断处理函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void UART7_DMA_TX_IRQHandler(void)
{
    if(dma_interrupt_flag_get(UART7_DMAx, UART7_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF)) 
    {
        dma_interrupt_flag_clear(UART7_DMAx, UART7_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
        g_uart7_TransferState = TRANSFER_TX_COMPLETE;
    }
}
#endif
/*
*********************************************************************************************************
*    函 数 名: uart7_dma_rx_enable
*    功能说明: 使能UART7接收DMA。
*    形    参: 
*    返 回 值: 接收缓冲区指针
*    @len        : 接收数据长度
*    返 回 值: 无
*********************************************************************************************************
*/
void uart7_dma_rx_enable(void)  
{
    dma_memory_address_config(UART7_DMAx, UART7_RX_DMA_CHANNEL, DMA_MEMORY_0, (uint32_t)g_uart7_rx_buff);
    dma_transfer_number_config(UART7_DMAx, UART7_RX_DMA_CHANNEL, UART7_BUFF_SIZE);
    dma_channel_enable(UART7_DMAx, UART7_RX_DMA_CHANNEL);
}

/*
*********************************************************************************************************
*    函 数 名: UART7_IRQHandler
*    功能说明: 供中断服务程序调用，通用串口中断处理函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void UART7_IRQHandler(void)
{
    #ifdef USE_UART7_INT
    {
        static uint16_t rxcount = 0;
        static uint16_t txcount = 0;
        if((RESET != usart_interrupt_flag_get(UART7, USART_INT_FLAG_RBNE)) &&
            (RESET != usart_flag_get(UART7, USART_FLAG_RBNE)))                                                                                                                  
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
Q       if((RESET != usart_flag_get(UART7, USART_FLAG_TBE)) &&
            (RESET != usart_interrupt_flag_get(UART7, USART_INT_FLAG_TBE))) 
        {
            /* transmit data */
            usart_data_transmit(UART7, g_uart7_tx_buff[txcount++]);
            if(txcount == g_uart7_Len)
            {
                txcount = 0;
                g_uart7_TransferState = TRANSFER_TX_COMPLETE;
                usart_interrupt_disable(UART7, USART_INT_TBE);
            }
        }
    }
    #endif
    
    #ifdef USE_UART7_IDEL
    {
        if(RESET != usart_interrupt_flag_get(UART7, USART_INT_FLAG_IDLE))
        {
            /* clear IDLE flag */
            usart_data_receive(UART7);

            /* disable DMA and reconfigure */
            dma_channel_disable(UART7_DMAx, UART7_RX_DMA_CHANNEL);
            dma_flag_clear(UART7_DMAx, UART7_RX_DMA_CHANNEL, DMA_FLAG_FTF);                                                                                                                                                                                                                                                                         
            
            /* number of data received */
            g_uart7_Len = UART7_BUFF_SIZE - (dma_transfer_number_get(UART7_DMAx, UART7_RX_DMA_CHANNEL));
            g_uart7_TransferState = TRANSFER_RX_COMPLETE;
            
            uart7_dma_rx_enable();
        }
    }
    #endif  

    if((RESET != usart_flag_get(UART7, USART_FLAG_ORERR)) ||
        (RESET != usart_flag_get(UART7, USART_FLAG_FERR)) ||
        (RESET != usart_flag_get(UART7, USART_FLAG_PERR)))
    {
        USART_STAT0(UART7);
        usart_data_receive(UART7);
    }
}

/*
*********************************************************************************************************
*    函 数 名: uart7_rx_get_frame
*    功能说明: 获取接收到的一帧数据
*    形    参: 无
*    返 回 值: 接收到的一帧数据指针，或NULL
*********************************************************************************************************
*/
uint8_t *uart7_rx_get_frame(void)
{
    if (g_uart7_TransferState == TRANSFER_RX_COMPLETE)
    {
        g_uart7_TransferState = TRANSFER_WAIT;
        g_uart7_rx_buff[g_uart7_Len] = '\0';
        return g_uart7_rx_buff;
    }
    else
    {
        return NULL;
    }
}

/*
*********************************************************************************************************
*    函 数 名: uart7_test
*    功能说明: UART7测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void uart7_test(void)
{                                                                                                                                                                                                             
    while(1)
    {
        #if defined(USE_UART7_INT)
        {
            while(g_uart7_TransferState != TRANSFER_RX_COMPLETE);
            uart7_send_str(g_uart7_rx_buff,10); 
        }
        #elif defined(USE_UART7_IDEL) || defined(USE_UART7_TX_DMA)
        {
            while(g_uart7_TransferState != TRANSFER_RX_COMPLETE);
            uart7_send_str(g_uart7_rx_buff,g_uart7_Len);
        }
        #else
        {
            uart7_send_str("uart7_rx_test\n",12);
            dwt_delay_ms(1000);
        }
        #endif
    }
}

/******************************************  (END OF FILE) **********************************************/


