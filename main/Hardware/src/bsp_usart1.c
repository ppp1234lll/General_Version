/*
*********************************************************************************************************
*    函 数 名: 串口1
*    功能说明: 串口1模块
*    形    参: ZHLE
*    返 回 值: 
*********************************************************************************************************
*/
#include "bsp_usart1.h"
#include "bsp.h"
#include "./Driver/inc/GPRS.h"
#include "./Task/inc/gprs_rx.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"

/*
*********************************************************************************************************
*                                 选择DMA，中断或者查询方式
*********************************************************************************************************
*/
// #define USE_USART1_TX_DMA           /* DMA发送 */

//#define USE_USART1_INT              /* 中断方式 */
//#define USE_USART1_IDEL             /* DMA接收+空闲中断方式 */
#define USE_USART1_TIMEOUT          /* DMA接收+超时检测中断方式 */   
/*
*********************************************************************************************************
*                                时钟，引脚，DMA，中断等宏定义
*********************************************************************************************************
*/
#define USART1_TX_GPIO_CLK              RCU_GPIOD
#define USART1_TX_GPIO_PORT             GPIOD
#define USART1_TX_PIN                   GPIO_PIN_5
#define USART1_TX_PIN_AF                GPIO_AF_7

#define USART1_RX_GPIO_CLK              RCU_GPIOD
#define USART1_RX_GPIO_PORT             GPIOD
#define USART1_RX_PIN                   GPIO_PIN_6
#define USART1_RX_PIN_AF                GPIO_AF_7

#define USART1_DMA_CLK                  RCU_DMA0
#define USART1_DMAx                     DMA0
#define USART1_TX_DMA_CHANNEL           DMA_CH6
#define USART1_RX_DMA_CHANNEL           DMA_CH5

#define USART1_TX_DMA_PERIEN            DMA_SUBPERI4
#define USART1_RX_DMA_PERIEN            DMA_SUBPERI4

#define USART1_DMA_TX_IRQn              DMA0_Channel6_IRQn
#define USART1_DMA_RX_IRQn              DMA0_Channel5_IRQn

#define USART1_DMA_TX_IRQHandler        DMA0_Channel6_IRQHandler
#define USART1_DMA_RX_IRQHandler        DMA0_Channel5_IRQHandler
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

#define USART1_BUFF_SIZE  2048

uint8_t *g_usart1_tx_buff = NULL;
uint8_t g_usart1_rx_buff[USART1_BUFF_SIZE] = {0};
uint32_t g_usart1_TransferState = TRANSFER_WAIT;
uint32_t g_usart1_Len;   
    
/*
*********************************************************************************************************
*    函 数 名: bsp_InitUsart1
*    功能说明: 初始化串口硬件
*    形    参: baudrate: 波特率
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart1(uint32_t bound)
{
    bsp_InitUsart1_GPIO();
    bsp_InitUsart1_Config(bound);
    bsp_InitUsart1_DMA();
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitUsart1_GPIO
*    功能说明: 初始化串口1 GPIO引脚
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart1_GPIO(void)
{
    /* enable GPIO clock */
    rcu_periph_clock_enable(USART1_TX_GPIO_CLK);
    rcu_periph_clock_enable(USART1_RX_GPIO_CLK);

    /* configure the USART1 TX pin and USART1 RX pin */
    gpio_af_set(USART1_TX_GPIO_PORT, USART1_TX_PIN_AF, USART1_TX_PIN);
    gpio_af_set(USART1_RX_GPIO_PORT, USART1_RX_PIN_AF, USART1_RX_PIN);

    /* configure USART1 TX as alternate function push-pull */
    gpio_mode_set(USART1_TX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART1_TX_PIN);
    gpio_output_options_set(USART1_TX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART1_TX_PIN);

    /* configure USART1 RX as alternate function push-pull */
    gpio_mode_set(USART1_RX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART1_RX_PIN);
    gpio_output_options_set(USART1_RX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART1_RX_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitUsart1_Config
*    功能说明: 初始化串口1硬件
*    形    参: baudrate: 波特率
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart1_Config(uint32_t bound)
{
    /* enable USART clock */
    rcu_periph_clock_enable(RCU_USART1);
    
    /* USART1 configure */
    usart_deinit(USART1);
    usart_word_length_set(USART1, USART_WL_8BIT);
    usart_stop_bit_set(USART1, USART_STB_1BIT);
    usart_parity_config(USART1, USART_PM_NONE);
    usart_baudrate_set(USART1, bound);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
    usart_enable(USART1);

    #ifdef USE_USART1_INT
    {
        usart_interrupt_flag_clear(USART1, USART_INT_FLAG_RBNE);
        /* enable USART1 interrupt */
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
*    函 数 名: bsp_InitUsart1_DMA
*    功能说明: 初始化串口DMA硬件
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart1_DMA(void)
{
    /* 配置TX DMA和NVIC */
    #ifdef USE_USART1_TX_DMA
    {
        dma_single_data_parameter_struct dma_tx_struct;
        
        /* enable DMA0 */
        rcu_periph_clock_enable(USART1_DMA_CLK);

        /* deinitialize DMA USART1 TX */
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

        /* enable DMA0 */
        rcu_periph_clock_enable(USART1_DMA_CLK);

        /* deinitialize DMA USART1 RX */
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

        // dma_interrupt_enable(USART0_DMAx, USART0_RX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
        // nvic_irq_enable(USART0_DMA_RX_IRQn, 7, 0);

        /* enable DMA0 channel7 */
        dma_channel_enable(USART1_DMAx, USART1_RX_DMA_CHANNEL);
    }
    #endif
}
/*
*********************************************************************************************************
*    函 数 名: usart1_send_char
*    功能说明: 向串口发送1个字节。
*    形    参: 
*    返 回 值: 待发送的字节数据
*    返 回 值: 无
*********************************************************************************************************
*/
void usart1_send_char(uint8_t ch)
{
    usart_data_transmit(USART1, (uint8_t)ch);
    while (RESET == usart_flag_get(USART1, USART_FLAG_TBE));
}

/*
*********************************************************************************************************
*    函 数 名: usart1_send_str
*    功能说明: 向串口发送字符串。
*    形    参: 
*    返 回 值: 字符串指针
*    @len        : 发送数据长度
*    返 回 值: 无
*********************************************************************************************************
*/
void usart1_send_str(uint8_t *buff, uint32_t len)
{
    g_usart1_TransferState = TRANSFER_WAIT;
    #ifdef USE_USART1_INT
    {
        g_usart1_tx_buff = buff;
        g_usart1_Len = len;
        usart_interrupt_enable(USART1, USART_INT_TBE);
        while(g_usart1_TransferState != TRANSFER_TX_COMPLETE);
    }
    #elif defined(USE_USART1_TX_DMA)
    {    
        dma_memory_address_config(USART1_DMAx, USART1_TX_DMA_CHANNEL, DMA_MEMORY_0, (uint32_t)buff);
        dma_transfer_number_config(USART1_DMAx, USART1_TX_DMA_CHANNEL, len);
        dma_channel_enable(USART1_DMAx, USART1_TX_DMA_CHANNEL);
        while(g_usart1_TransferState != TRANSFER_TX_COMPLETE);
    }
    #else
    {
        while(len--) 
        {
            usart1_send_char(buff[0]);
            buff++;
        }
    }
    #endif
}

#ifdef USE_USART1_TX_DMA
/*
*********************************************************************************************************
*    函 数 名: USART1_DMA_TX_IRQHandler
*    功能说明: 供中断服务程序调用，DMA发送完成中断处理函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void USART1_DMA_TX_IRQHandler(void)
{
    if(dma_interrupt_flag_get(USART1_DMAx, USART1_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF)) 
    {
        dma_interrupt_flag_clear(USART1_DMAx, USART1_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
        g_usart1_TransferState = TRANSFER_TX_COMPLETE;
    }
}
#endif
/*
*********************************************************************************************************
*    函 数 名: usart1_dma_rx_enable
*    功能说明: 使能USART1接收DMA。
*    形    参: 
*    返 回 值: 接收缓冲区指针
*    @len        : 接收数据长度
*    返 回 值: 无
*********************************************************************************************************
*/
void usart1_dma_rx_enable(void)
{
    dma_memory_address_config(USART1_DMAx, USART1_RX_DMA_CHANNEL, DMA_MEMORY_0, (uint32_t)g_usart1_rx_buff);
    dma_transfer_number_config(USART1_DMAx, USART1_RX_DMA_CHANNEL, USART1_BUFF_SIZE);
    dma_channel_enable(USART1_DMAx, USART1_RX_DMA_CHANNEL);
}

/*
*********************************************************************************************************
*    函 数 名: USART1_IRQHandler
*    功能说明: 供中断服务程序调用，通用串口中断处理函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void USART1_IRQHandler(void)
{
    #ifdef USE_USART1_INT
    {
        static uint16_t rxcount = 0;
        static uint16_t txcount = 0;
        if((RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_RBNE)) &&
            (RESET != usart_flag_get(USART1, USART_FLAG_RBNE))) 
        {
            /* receive data */
            g_usart1_rx_buff[rxcount++] = usart_data_receive(USART1);
            if(rxcount == 10) 
            {
                rxcount = 0;
                g_usart1_TransferState = TRANSFER_RX_COMPLETE;
                // usart_interrupt_disable(USART1, USART_INT_RBNE);
            }
        }
        if((RESET != usart_flag_get(USART1, USART_FLAG_TBE)) &&
            (RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_TBE))) 
        {
            /* transmit data */
            usart_data_transmit(USART1, g_usart1_tx_buff[txcount++]);
            if(txcount == g_usart0_Len)
            {
                txcount = 0;
                g_usart1_TransferState = TRANSFER_TX_COMPLETE;
                usart_interrupt_disable(USART1, USART_INT_TBE);
            }
        }
    }
    #endif
    
    #ifdef USE_USART1_IDEL
    {
        if(RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_IDLE))
        {
            /* clear IDLE flag */
            usart_data_receive(USART1);

            /* disable DMA and reconfigure */
            dma_channel_disable(USART1_DMAx, USART1_RX_DMA_CHANNEL);
            dma_flag_clear(USART1_DMAx, USART1_RX_DMA_CHANNEL, DMA_FLAG_FTF);
            
            /* number of data received */
            g_usart1_Len = USART1_BUFF_SIZE - (dma_transfer_number_get(USART1_DMAx, USART1_RX_DMA_CHANNEL));
            g_usart1_TransferState = TRANSFER_RX_COMPLETE;
            
            usart1_dma_rx_enable();
        }
    }
    #endif

    #ifdef USE_USART1_TIMEOUT
    {
        if(RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_RT))
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;

            usart_interrupt_flag_clear(USART1, USART_INT_FLAG_RT);

            /* disable DMA and reconfigure */
            dma_channel_disable(USART1_DMAx, USART1_RX_DMA_CHANNEL);
            dma_flag_clear(USART1_DMAx, USART1_RX_DMA_CHANNEL, DMA_FLAG_FTF);

            /* number of data received */
            g_usart1_Len = USART1_BUFF_SIZE - (dma_transfer_number_get(USART1_DMAx, USART1_RX_DMA_CHANNEL));
            g_usart1_TransferState = TRANSFER_RX_COMPLETE;

            /* 通过 StreamBuffer 将数据从 ISR 传递给任务上下文, 不在中断中处理 */
            if (g_gprs_rx_streambuf != NULL && g_usart1_Len > 0U)
            {
                xStreamBufferSendFromISR(g_gprs_rx_streambuf,
                    g_usart1_rx_buff, g_usart1_Len,
                    &xHigherPriorityTaskWoken);
            }

            usart1_dma_rx_enable();

            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
    #endif

    if((RESET != usart_flag_get(USART1, USART_FLAG_ORERR)) ||
        (RESET != usart_flag_get(USART1, USART_FLAG_FERR)) ||
        (RESET != usart_flag_get(USART1, USART_FLAG_PERR)))
    {
        USART_STAT0(USART1);
        usart_data_receive(USART1);
    }
}

/*
*********************************************************************************************************
*    函 数 名: usart1_rx_get_frame
*    功能说明: 获取接收到的一帧数据
*    形    参: 无
*    返 回 值: 接收到的一帧数据指针，或NULL
*********************************************************************************************************
*/
uint8_t *usart1_rx_get_frame(void)
{
    if (g_usart1_TransferState == TRANSFER_RX_COMPLETE)
    {
        g_usart1_TransferState = TRANSFER_WAIT;
        g_usart1_rx_buff[g_usart1_Len] = '\0';
        return g_usart1_rx_buff;
    }
    else
    {
        return NULL;
    }
}


/*
*********************************************************************************************************
*    函 数 名: usart1_test
*    功能说明: 串口1测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void usart1_test(void)
{
    while(1)
    {
        #if defined(USE_USART1_INT)
        {
            while(g_usart1_TransferState != TRANSFER_RX_COMPLETE);
            usart1_send_str(g_usart1_rx_buff,10);
        }
        #elif defined(USE_USART1_IDEL) || defined(USE_USART1_TIMEOUT) || defined(USE_USART1_TX_DMA)
        {
            while(g_usart1_TransferState != TRANSFER_RX_COMPLETE);
            usart1_send_str(g_usart1_rx_buff,g_usart1_Len);
        }
        #else
        {
            usart1_send_str("Hello, World!\n",strlen("Hello, World!\n"));
            // printf("串口1测试\n");
            // delay_ms(1000);    
            dwt_delay_ms(1000);
        }
        #endif
    }
        
}

/******************************************  (END OF FILE) **********************************************/


