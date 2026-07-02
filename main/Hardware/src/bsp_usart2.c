/********************************************************************************
* @File name  : 串口2
* @Description: 串口2
* @Author     : ZHLE
*  Version Date        Modification Description
    12、串口2，波特率115200，引脚分配为：   
        USART2_TXD    PD8
        USART2_RXD    PD9
********************************************************************************/

#include "bsp_usart2.h"     
#include "bsp.h"

/*
*********************************************************************************************************
*                                 选择DMA，中断或者查询方式
*********************************************************************************************************
*/
#define USE_USART2_TX_DMA           /* DMA发送 */

//#define USE_USART2_INT              /* 中断方式 */
//#define USE_USART2_IDEL              /* DMA接收+空闲中断方式 */
#define USE_USART2_TIMEOUT          /* DMA接收+超时检测中断方式 */   
/*
*********************************************************************************************************
*                                时钟，引脚，DMA，中断等宏定义
*********************************************************************************************************
*/
#define USART2_TX_GPIO_CLK              RCU_GPIOD
#define USART2_TX_GPIO_PORT             GPIOD
#define USART2_TX_PIN                   GPIO_PIN_8
#define USART2_TX_PIN_AF                GPIO_AF_7

#define USART2_RX_GPIO_CLK              RCU_GPIOD
#define USART2_RX_GPIO_PORT             GPIOD
#define USART2_RX_PIN                   GPIO_PIN_9
#define USART2_RX_PIN_AF                GPIO_AF_7

#define USART2_DMA_CLK                  RCU_DMA0
#define USART2_DMAx                     DMA0
#define USART2_TX_DMA_CHANNEL           DMA_CH3
#define USART2_RX_DMA_CHANNEL           DMA_CH1

#define USART2_TX_DMA_PERIEN            DMA_SUBPERI4
#define USART2_RX_DMA_PERIEN            DMA_SUBPERI4

#define USART2_DMA_TX_IRQn              DMA0_Channel3_IRQn
#define USART2_DMA_RX_IRQn              DMA0_Channel1_IRQn

#define USART2_DMA_TX_IRQHandler        DMA0_Channel3_IRQHandler
#define USART2_DMA_RX_IRQHandler        DMA0_Channel1_IRQHandler
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

#define USART2_BUFF_SIZE  2048

uint8_t *g_usart2_tx_buff = NULL;
uint8_t g_usart2_rx_buff[USART2_BUFF_SIZE] = {0};
uint32_t g_usart2_TransferState = TRANSFER_WAIT;
uint32_t g_usart2_Len;   
    
/*
*********************************************************************************************************
*    函 数 名: bsp_InitUsart2
*    功能说明: 初始化串口硬件 
*    形    参: baudrate: 波特率
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart2(uint32_t bound)
{
    bsp_InitUsart2_GPIO();
    bsp_InitUsart2_Config(bound);
    bsp_InitUsart2_DMA();
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitUsart2
*    功能说明: 初始化串口硬件 
*    形    参: baudrate: 波特率
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart2_GPIO(void)
{
    /* enable GPIO clock */
    rcu_periph_clock_enable(USART2_TX_GPIO_CLK);
    rcu_periph_clock_enable(USART2_RX_GPIO_CLK);

    /* configure the USART2 TX pin and USART2 RX pin */
    gpio_af_set(USART2_TX_GPIO_PORT, USART2_TX_PIN_AF, USART2_TX_PIN);
    gpio_af_set(USART2_RX_GPIO_PORT, USART2_RX_PIN_AF, USART2_RX_PIN);

    /* configure USART2 TX as alternate function push-pull */
    gpio_mode_set(USART2_TX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART2_TX_PIN);
    gpio_output_options_set(USART2_TX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART2_TX_PIN);

    /* configure USART2 RX as alternate function push-pull */
    gpio_mode_set(USART2_RX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART2_RX_PIN);
    gpio_output_options_set(USART2_RX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART2_RX_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitUsart2
*    功能说明: 初始化串口硬件 
*    形    参: baudrate: 波特率
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart2_Config(uint32_t bound)
{
    /* enable USART clock */
    rcu_periph_clock_enable(RCU_USART2);
    
    /* USART2 configure */
    usart_deinit(USART2);
    usart_word_length_set(USART2, USART_WL_8BIT);
    usart_stop_bit_set(USART2, USART_STB_1BIT);
    usart_parity_config(USART2, USART_PM_NONE);
    usart_baudrate_set(USART2, bound);
    usart_receive_config(USART2, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART2, USART_TRANSMIT_ENABLE);
    usart_enable(USART2);

    #ifdef USE_USART2_INT
    {
        usart_interrupt_flag_clear(USART2, USART_INT_FLAG_RBNE);
        /* enable USART2 interrupt */
        usart_interrupt_enable(USART2, USART_INT_RBNE);
        nvic_irq_enable(USART2_IRQn, 14, 0);
    }
    #endif
    
    #ifdef USE_USART2_IDEL
    {
        usart_interrupt_enable(USART2, USART_INT_IDLE);
        nvic_irq_enable(USART2_IRQn, 14, 0);
    }
    #endif

    #ifdef USE_USART2_TIMEOUT
    {
        /* enable the USART receive timeout and configure the time of timeout */
        usart_interrupt_flag_clear(USART2, USART_INT_FLAG_RT);
        /*向寄存器填入需要超时的长度，单位为一个波特时长,3.5个字节*11波特长度 = 39  */
        usart_receiver_timeout_threshold_config(USART2, 39);
        usart_receiver_timeout_enable(USART2);
        usart_interrupt_enable(USART2, USART_INT_RT);
        nvic_irq_enable(USART2_IRQn, 14, 0);
    }
    #endif
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitUsart2_DMA
*    功能说明: 初始化串口DMA硬件 
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart2_DMA(void)
{
    /* 配置TX DMA和NVIC */
    #ifdef USE_USART2_TX_DMA
    {
        dma_single_data_parameter_struct dma_tx_struct;
        
        /* enable DMA1 */
        rcu_periph_clock_enable(USART2_DMA_CLK);

        /* deinitialize DMA USART2 TX */
        dma_deinit(USART2_DMAx, USART2_TX_DMA_CHANNEL);
        dma_tx_struct.periph_addr         = (uint32_t)&USART_DATA(USART2);
        dma_tx_struct.memory0_addr        = (uint32_t)g_usart2_tx_buff;
        dma_tx_struct.direction           = DMA_MEMORY_TO_PERIPH;
        dma_tx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
        dma_tx_struct.priority            = DMA_PRIORITY_MEDIUM;
        dma_tx_struct.number              = USART2_BUFF_SIZE;
        dma_tx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
        dma_tx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
        dma_tx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
        dma_single_data_mode_init(USART2_DMAx, USART2_TX_DMA_CHANNEL, &dma_tx_struct);
        dma_channel_subperipheral_select(USART2_DMAx, USART2_TX_DMA_CHANNEL, USART2_TX_DMA_PERIEN);
        
        /* USART DMA enable for transmission and reception */
        usart_dma_transmit_config(USART2, USART_TRANSMIT_DMA_ENABLE);    

        /* enable DMA1 channel7 transfer complete interrupt */
        dma_interrupt_flag_clear(USART2_DMAx, USART2_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
        dma_interrupt_enable(USART2_DMAx, USART2_TX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
        nvic_irq_enable(USART2_DMA_TX_IRQn, 14, 0);
    }
    #endif
    /* 配置RX DMA和NVIC */
    #if defined(USE_USART2_IDEL) || defined(USE_USART2_TIMEOUT)
    {
        dma_single_data_parameter_struct dma_rx_struct;

        /* enable DMA1 */
        rcu_periph_clock_enable(USART2_DMA_CLK);

        /* deinitialize DMA USART2 RX */
        dma_deinit(USART2_DMAx, USART2_RX_DMA_CHANNEL);
        dma_rx_struct.periph_addr         = (uint32_t)&USART_DATA(USART2);
        dma_rx_struct.memory0_addr        = (uint32_t)g_usart2_rx_buff;
        dma_rx_struct.direction           = DMA_PERIPH_TO_MEMORY;
        dma_rx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
        dma_rx_struct.priority            = DMA_PRIORITY_MEDIUM;
        dma_rx_struct.number              = USART2_BUFF_SIZE;
        dma_rx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
        dma_rx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
        dma_rx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
        dma_single_data_mode_init(USART2_DMAx, USART2_RX_DMA_CHANNEL, &dma_rx_struct);
        dma_channel_subperipheral_select(USART2_DMAx, USART2_RX_DMA_CHANNEL, USART2_RX_DMA_PERIEN);
        
        /* USART DMA enable for transmission and reception */
        usart_dma_receive_config(USART2, USART_RECEIVE_DMA_ENABLE);    

        // dma_interrupt_enable(USART2_DMAx, USART2_RX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
        // nvic_irq_enable(USART2_DMA_RX_IRQn, 14, 0);

        /* enable DMA1 channel7 */
        dma_channel_enable(USART2_DMAx, USART2_RX_DMA_CHANNEL);
    }
    #endif
}
/*
*********************************************************************************************************
*    函 数 名: usart2_send_char
*    功能说明: 向串口发送1个字节。
*    形    参: 
*    @ch            : 待发送的字节数据
*    返 回 值: 无
*********************************************************************************************************
*/
void usart2_send_char(uint8_t ch)
{
    usart_data_transmit(USART2, (uint8_t)ch);
    while (RESET == usart_flag_get(USART2, USART_FLAG_TBE));
}

/*
*********************************************************************************************************
*    函 数 名: usart2_send_str
*    功能说明: 向串口发送字符串。
*    形    参:  
*    @buff        : 字符串指针
*    @len        : 发送数据长度
*    返 回 值: 无
*********************************************************************************************************
*/
void usart2_send_str(uint8_t *buff, uint16_t len)
{
    g_usart2_TransferState = TRANSFER_WAIT;
    #ifdef USE_USART2_INT
    {
        g_usart2_tx_buff = buff;
        g_usart2_Len = len;
        usart_interrupt_enable(USART2, USART_INT_TBE);
        while(g_usart2_TransferState != TRANSFER_TX_COMPLETE);
    }
    #elif defined(USE_USART2_TX_DMA)
    {    
        dma_memory_address_config(USART2_DMAx, USART2_TX_DMA_CHANNEL, DMA_MEMORY_0, (uint32_t)buff);
        dma_transfer_number_config(USART2_DMAx, USART2_TX_DMA_CHANNEL, len);
        dma_channel_enable(USART2_DMAx, USART2_TX_DMA_CHANNEL);
        while(g_usart2_TransferState != TRANSFER_TX_COMPLETE);
    }
    #else
    {
        while(len--) 
        {
            usart2_send_char(buff[0]);
            buff++;
        }
    }
    #endif
}

#ifdef USE_USART2_TX_DMA
/*
*********************************************************************************************************
*    函 数 名: USART2_DMA_TX_IRQHandler
*    功能说明: 供中断服务程序调用，DMA发送完成中断处理函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void USART2_DMA_TX_IRQHandler(void)
{
    if(dma_interrupt_flag_get(USART2_DMAx, USART2_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF)) 
    {
        dma_interrupt_flag_clear(USART2_DMAx, USART2_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
        g_usart2_TransferState = TRANSFER_TX_COMPLETE;
    }
}
#endif

/*
*********************************************************************************************************
*    函 数 名: usart2_dma_rx_enable
*    功能说明: 使能USART2接收DMA。
*    形    参:  
*    @buff        : 接收缓冲区指针
*    @len        : 接收数据长度
*    返 回 值: 无
*********************************************************************************************************
*/
void usart2_dma_rx_enable(void)
{
    dma_memory_address_config(USART2_DMAx, USART2_RX_DMA_CHANNEL, DMA_MEMORY_0, (uint32_t)g_usart2_rx_buff);
    dma_transfer_number_config(USART2_DMAx, USART2_RX_DMA_CHANNEL, USART2_BUFF_SIZE);
    dma_channel_enable(USART2_DMAx, USART2_RX_DMA_CHANNEL);
}

/*
*********************************************************************************************************
*    函 数 名: USART2_IRQHandler
*    功能说明: 供中断服务程序调用，通用串口中断处理函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void USART2_IRQHandler(void)
{
    #ifdef USE_USART2_INT
    {
        static uint16_t rxcount = 0;
        static uint16_t txcount = 0;
        if((RESET != usart_interrupt_flag_get(USART2, USART_INT_FLAG_RBNE)) &&
            (RESET != usart_flag_get(USART2, USART_FLAG_RBNE))) 
        {
            /* receive data */
            g_usart2_rx_buff[rxcount++] = usart_data_receive(USART2);
            if(rxcount == 10) 
            {
                rxcount = 0;
                g_usart0_TransferState = TRANSFER_RX_COMPLETE;
                // usart_interrupt_disable(USART2, USART_INT_RBNE);
            }
        }
        if((RESET != usart_flag_get(USART2, USART_FLAG_TBE)) &&
            (RESET != usart_interrupt_flag_get(USART2, USART_INT_FLAG_TBE))) 
        {
            /* transmit data */
            usart_data_transmit(USART2, g_usart2_tx_buff[txcount++]);
            if(txcount == g_usart2_Len)
            {
                txcount = 0;
                g_usart2_TransferState = TRANSFER_TX_COMPLETE;
                usart_interrupt_disable(USART2, USART_INT_TBE);
            }
        }
    }
    #endif

    #ifdef USE_USART2_IDEL
    {
        if(RESET != usart_interrupt_flag_get(USART2, USART_INT_FLAG_IDLE))
        {
            /* clear IDLE flag */
            usart_data_receive(USART2);
            
            /* disable DMA and reconfigure */
            dma_channel_disable(USART2_DMAx, USART2_RX_DMA_CHANNEL);
            dma_flag_clear(USART2_DMAx, USART2_RX_DMA_CHANNEL, DMA_FLAG_FTF);

            /* number of data received */
            g_usart2_Len = USART2_BUFF_SIZE - (dma_transfer_number_get(USART2_DMAx, USART2_RX_DMA_CHANNEL));
            g_usart2_TransferState = TRANSFER_RX_COMPLETE;
            
			usart2_dma_rx_enable();
        }
    }
    #endif

    #ifdef USE_USART2_TIMEOUT
    {
        if(RESET != usart_interrupt_flag_get(USART2, USART_INT_FLAG_RT))
        {
            usart_interrupt_flag_clear(USART2, USART_INT_FLAG_RT);

            /* disable DMA and reconfigure */
            dma_channel_disable(USART2_DMAx, USART2_RX_DMA_CHANNEL);
            dma_flag_clear(USART2_DMAx, USART2_RX_DMA_CHANNEL, DMA_FLAG_FTF);

            /* number of data received */
            g_usart2_Len = USART2_BUFF_SIZE - (dma_transfer_number_get(USART2_DMAx, USART2_RX_DMA_CHANNEL));
            g_usart2_TransferState = TRANSFER_RX_COMPLETE;
            
            usart2_dma_rx_enable();
        }
    }
    #endif
    
    if((RESET != usart_flag_get(USART2, USART_FLAG_ORERR)) ||
        (RESET != usart_flag_get(USART2, USART_FLAG_FERR)) ||
        (RESET != usart_flag_get(USART2, USART_FLAG_PERR)))
    {
        USART_STAT0(USART2);
        usart_data_receive(USART2);
    }
}
/*
*********************************************************************************************************
*    函 数 名: usart2_rx_get_frame
*    功能说明: 获取接收到的一帧数据
*    形    参: 无
*    返 回 值: 接收到的一帧数据指针，或NULL
*********************************************************************************************************
*/
uint8_t *usart2_rx_get_frame(void)
{
    if (g_usart2_TransferState == TRANSFER_RX_COMPLETE)
    {
        g_usart2_TransferState = TRANSFER_WAIT;
        g_usart2_rx_buff[g_usart2_Len] = '\0';
        return g_usart2_rx_buff;
    }
    else
    {
        return NULL;
    }
}

/*
*********************************************************************************************************
*    函 数 名: usart2_test
*    功能说明: 串口2测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void usart2_test(void)
{
    uint8_t usart2_rx_test[] = "Hello, World!\n";
    while(1)
    {
        #if defined(USE_USART2_INT)
        {
            while(g_usart2_TransferState != TRANSFER_RX_COMPLETE);
            usart2_send_str(g_usart2_rx_buff,10);
        }
        #elif defined(USE_USART2_IDEL) || defined(USE_USART2_TIMEOUT) || defined(USE_USART2_TX_DMA)
        {
            while(g_usart2_TransferState != TRANSFER_RX_COMPLETE);
            usart2_send_str(g_usart2_rx_buff,g_usart2_Len);
        }
        #else
        {
            usart2_send_str(usart2_rx_test,sizeof(usart2_rx_test));
            // printf("串口2测试\n");
            // delay_ms(1000);    
            dwt_delay_ms(1000);
        }
        #endif
    }
        
}

/******************************************  (END OF FILE) **********************************************/


