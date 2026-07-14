/*
*********************************************************************************************************
*
*    模块名称 : SPI总线驱动
*    文件名称 : bsp_hspi0.c
*    版    本 : V1.3
*    说    明 : SPI总线底层驱动。提供SPI配置、收发数据、多设备共享SPI功能.
*    修改记录 :
*        版本号  日期        作者    说明
*       v1.0    2014-10-24 armfly   首版。将串行FLASH、TSC2046、VS1053、AD7705、ADS1256等SPI设备的配置
*                                    和收发数据的函数进行汇总分类。并解决不同速度的设备间的共享问题。
*        V1.1    2015-02-25 armfly   硬件SPI时，没有开启GPIOB时钟，已解决。
*        V1.2    2015-07-23 armfly   修改 bsp_SPI_Init() 函数，增加开关SPI时钟的语句。规范硬件SPI和软件SPI的宏定义。
*        V1.3    2020-03-14 Eric2013 适配STM32H7。
*
*    Copyright (C), 2020-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp_hspi0.h"
#include "bsp.h"

/*
    安富莱STM32-V7开发板口线分配
    PB3/SPI3_SCK/SPI1_SCK
    PB4/SPI3_MISO/SPI1_MISO
    PB5/SPI3_MOSI/SPI1_MOSI    
*/

/*
*********************************************************************************************************
*                                 选择DMA，中断或者查询方式
*********************************************************************************************************
*/
#define USE_SPI_DMA    /* DMA方式  */
// #define USE_SPI_POLL   /* 查询方式 */

/*
*********************************************************************************************************
*                                时钟，引脚，DMA，中断等宏定义
*********************************************************************************************************
*/
#define HSPI0_SCK_CLK                    RCU_GPIOB
#define HSPI0_SCK_GPIO                   GPIOB
#define HSPI0_SCK_PIN                    GPIO_PIN_3
#define HSPI0_SCK_AF                     GPIO_AF_5

#define HSPI0_MISO_CLK                   RCU_GPIOB
#define HSPI0_MISO_GPIO                  GPIOB
#define HSPI0_MISO_PIN                   GPIO_PIN_4
#define HSPI0_MISO_AF                    GPIO_AF_5

#define HSPI0_MOSI_CLK                   RCU_GPIOB
#define HSPI0_MOSI_GPIO                  GPIOB
#define HSPI0_MOSI_PIN                   GPIO_PIN_5
#define HSPI0_MOSI_AF                    GPIO_AF_5

#define HSPI0_DMA_CLK                    RCU_DMA1
#define HSPI0_DMA                        DMA1
#define HSPI0_TX_DMA_CHANNEL             DMA_CH3
#define HSPI0_RX_DMA_CHANNEL             DMA_CH2

#define HSPI0_TX_DMA_PERIEN              DMA_SUBPERI3
#define HSPI0_RX_DMA_PERIEN              DMA_SUBPERI3

#define HSPI0_DMA_TX_IRQn                DMA1_Channel3_IRQn
#define HSPI0_DMA_RX_IRQn                DMA1_Channel2_IRQn

#define HSPI0_DMA_TX_IRQHandler          DMA1_Channel3_IRQHandler
#define HSPI0_DMA_RX_IRQHandler          DMA1_Channel2_IRQHandler

enum {
    TRANSFER_WAIT,
    TRANSFER_COMPLETE,
    TRANSFER_ERROR
};

/*
*********************************************************************************************************
*                                               变量
*********************************************************************************************************
*/
static uint32_t s_BaudRatePrescaler;
static uint32_t s_CLKPolarity_Phase;

__IO uint32_t wTransferState = TRANSFER_WAIT;

#define HSPI0_BUFFER_SIZE  4*1024
uint8_t *g_hspi0_TxBuf;  
uint8_t *g_hspi0_RxBuf;

/*
*********************************************************************************************************
*    函 数 名: bsp_InitHSPI0
*    功能说明: 配置SPI总线。
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitHSPI0(void)
{    
    bsp_InitHSPI0_GPIO();
    bsp_InitHSPI0_DMA();
    bsp_InitHSPI0_Param(HSPI0_BAUDRATEPRESCALER_15M, SPI_CK_PL_HIGH_PH_2EDGE);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitHSPI0_Param
*    功能说明: 配置SPI总线参数，时钟分频，时钟相位和时钟极性。
*    形    参: _BaudRatePrescaler  SPI总线时钟分频设置，支持的参数如下：
*                                 SPI_BAUDRATEPRESCALER_2    2分频
*                                 SPI_BAUDRATEPRESCALER_4    4分频
*                                 SPI_BAUDRATEPRESCALER_8    8分频
*                                 SPI_BAUDRATEPRESCALER_16   16分频
*                                 SPI_BAUDRATEPRESCALER_32   32分频
*                                 SPI_BAUDRATEPRESCALER_64   64分频
*                                 SPI_BAUDRATEPRESCALER_128  128分频
*                                 SPI_BAUDRATEPRESCALER_256  256分频
*                                                        
*             _CLKPhase           时钟相位，支持的参数如下：
*                                 SPI_PHASE_1EDGE     SCK引脚的第1个边沿捕获传输的第1个数据
*                                 SPI_PHASE_2EDGE     SCK引脚的第2个边沿捕获传输的第1个数据
*                                 
*             _CLKPolarity        时钟极性，支持的参数如下：
*                                 SPI_POLARITY_LOW    SCK引脚在空闲状态处于低电平
*                                 SPI_POLARITY_HIGH   SCK引脚在空闲状态处于高电平
*
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitHSPI0_Param(uint32_t _BaudRatePrescaler, uint32_t _CLKPolarity_Phase)
{
    spi_parameter_struct  spi_init_struct;
    /* 提高执行效率，只有在SPI硬件参数发生变化时，才执行HAL_Init */
    if (s_BaudRatePrescaler == _BaudRatePrescaler && s_CLKPolarity_Phase == _CLKPolarity_Phase)
    {        
        return;
    }

    s_BaudRatePrescaler = _BaudRatePrescaler;    
    s_CLKPolarity_Phase = _CLKPolarity_Phase;
    
    /* 设置SPI参数 */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = _CLKPolarity_Phase;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = _BaudRatePrescaler;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(SPI0, &spi_init_struct);

    spi_enable(SPI0);    
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitHSPI0_GPIO
*    功能说明: 配置SPI总线时钟，GPIO
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitHSPI0_GPIO(void)
{
    /* SPI和GPIP时钟 */
    rcu_periph_clock_enable(HSPI0_SCK_CLK);
    rcu_periph_clock_enable(HSPI0_MISO_CLK);
    rcu_periph_clock_enable(HSPI0_MOSI_CLK);
    rcu_periph_clock_enable(RCU_SPI0);

    /* SPI SCK */
    gpio_af_set(HSPI0_SCK_GPIO, HSPI0_SCK_AF, HSPI0_SCK_PIN);
    gpio_mode_set(HSPI0_SCK_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, HSPI0_SCK_PIN);
    gpio_output_options_set(HSPI0_SCK_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, HSPI0_SCK_PIN);

    /* SPI MISO */
    gpio_af_set(HSPI0_MISO_GPIO, HSPI0_MISO_AF, HSPI0_MISO_PIN);
    gpio_mode_set(HSPI0_MISO_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, HSPI0_MISO_PIN);
    gpio_output_options_set(HSPI0_MISO_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, HSPI0_MISO_PIN);

    /* SPI MOSI */
    gpio_af_set(HSPI0_MOSI_GPIO, HSPI0_MOSI_AF, HSPI0_MOSI_PIN);
    gpio_mode_set(HSPI0_MOSI_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, HSPI0_MOSI_PIN);
    gpio_output_options_set(HSPI0_MOSI_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, HSPI0_MOSI_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitHSPI0_DMA
*    功能说明: 配置SPI DMA
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitHSPI0_DMA(void)
{
    /* 配置DMA和NVIC */
    #ifdef USE_SPI_DMA
    {
        dma_single_data_parameter_struct dma_tx_struct;
        dma_single_data_parameter_struct dma_rx_struct;

        /* 使能DMA时钟 */
        rcu_periph_clock_enable(HSPI0_DMA_CLK);      

        /* SPI DMA发送配置 */    
        dma_deinit(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL);    
        dma_tx_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI0);
        dma_tx_struct.memory0_addr        = (uint32_t)g_hspi0_TxBuf;
        dma_tx_struct.direction           = DMA_MEMORY_TO_PERIPH;
        dma_tx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
        dma_tx_struct.priority            = DMA_PRIORITY_LOW;
        dma_tx_struct.number              = HSPI0_BUFFER_SIZE;
        dma_tx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
        dma_tx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
        dma_tx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
        dma_single_data_mode_init(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL, &dma_tx_struct);    
        dma_channel_subperipheral_select(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL, HSPI0_TX_DMA_PERIEN);
        
        /* enable SPI DMA */
        spi_dma_enable(SPI0, SPI_DMA_TRANSMIT);
    
        /* SPI DMA接收配置 */    
        dma_deinit(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL);
        dma_rx_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI0);
        dma_rx_struct.memory0_addr        = (uint32_t)g_hspi0_RxBuf;
        dma_rx_struct.direction           = DMA_PERIPH_TO_MEMORY;
        dma_rx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
        dma_rx_struct.priority            = DMA_PRIORITY_HIGH;
        dma_rx_struct.number              = HSPI0_BUFFER_SIZE;
        dma_rx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
        dma_rx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
        dma_rx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
        dma_single_data_mode_init(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL, &dma_rx_struct);
        dma_channel_subperipheral_select(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL, HSPI0_RX_DMA_PERIEN);

        /* enable SPI DMA */
        spi_dma_enable(SPI0, SPI_DMA_RECEIVE);

        /* 配置DMA发送中断 */
        nvic_irq_enable(HSPI0_DMA_TX_IRQn, 5, 0);
        dma_interrupt_enable(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL, DMA_INT_FTF);
        
        /* 配置DMA接收中断 */
        nvic_irq_enable(HSPI0_DMA_RX_IRQn, 5, 0);
        dma_interrupt_enable(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL, DMA_INT_FTF);
    }
    #endif
}

/*
*********************************************************************************************************
*    函 数 名: bsp_hspi0_Send_Byte
*    功能说明: 发送一个字节
*    形    参: TxData - 发送的字节
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_hspi0_Send_Byte(uint8_t TxData)
{
    uint8_t retry = 0;                     
    // 等待发送缓冲区为空（TXE标志置1）
    while(RESET == spi_i2s_flag_get(SPI0, SPI_FLAG_TBE))
    {
        retry++;
        if(retry > 200) return 0; // 超时返回0（发送失败）
    }              
    spi_i2s_data_transmit(SPI0, TxData);         // 将数据写入SPI数据寄存器（开始发送）
}

/*
*********************************************************************************************************
*    函 数 名: bsp_hspi0_send_buffer
*    功能说明: 发送一个缓冲区
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_hspi0_send_buffer(uint8_t* pbuffer, uint16_t plen)
{
    /* DMA方式传输 */
#ifdef USE_SPI_DMA
    uint16_t retry = 0; 
    wTransferState = TRANSFER_WAIT;

    // 关闭SPI DMA
    dma_channel_disable(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL);
    // 设置发送数据量
    dma_transfer_number_config(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL, plen);
    // 清除DMA标志位
    dma_interrupt_flag_clear(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
    // 使能 DMA发送通道
    dma_channel_enable(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL);

    while (wTransferState == TRANSFER_WAIT) 
    {
        retry++;
        if(retry > 10000)    break;
    }
#endif

    /* 查询方式传输 */    
#ifdef USE_SPI_POLL
    for(uint16_t i = 0; i < plen; i++)
    {
        bsp_hspi0_Send_Byte(pbuffer[i]);
    }
#endif
}

/*
*********************************************************************************************************
*    函 数 名: bsp_hspi0_Transfer_Byte
*    功能说明: 发送一个字节并接收一个字节
*    形    参: TxData - 发送的字节
*    返 回 值: 接收的字节
*********************************************************************************************************
*/
uint8_t bsp_hspi0_Transfer_Byte(uint8_t TxData)
{
    uint8_t retry = 0;                     
    // 等待发送缓冲区为空（TXE标志置1）
    while(RESET == spi_i2s_flag_get(SPI0, SPI_FLAG_TBE))
    {
        retry++;
        if(retry > 200) return 0; // 超时返回0（发送失败）
    }              
    spi_i2s_data_transmit(SPI0, TxData);         // 将数据写入SPI数据寄存器（开始发送）
    retry = 0;

    // 等待接收缓冲区非空（RXNE标志置1，接收完成）
    while(RESET == spi_i2s_flag_get(SPI0, SPI_FLAG_RBNE))
    {
        retry++;
        if(retry > 200) return 0; // 超时返回0（接收失败）
    }                                  
    return spi_i2s_data_receive(SPI0); // 返回接收的数据（从SPI数据寄存器读取）
}

/*
*********************************************************************************************************
*    函 数 名: bsp_hspi0_Transfer
*    功能说明: 启动数据传输
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_hspi0_Transfer(uint8_t* pbuffer, uint16_t plen)
{
    /* DMA方式传输 */
#ifdef USE_SPI_DMA
    uint16_t retry = 0;   
    wTransferState = TRANSFER_WAIT;

    // 关闭SPI DMA
    dma_channel_disable(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL);
    dma_channel_disable(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL);

    // 设置发送数据量
    dma_transfer_number_config(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL, plen);
    // 设置接收数据量
    dma_transfer_number_config(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL, plen);
    spi_i2s_data_receive(SPI0);
    // 清除DMA标志位
    dma_interrupt_flag_clear(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
    dma_interrupt_flag_clear(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL, DMA_INT_FLAG_FTF);

    // 使能 DMA发送通道
    dma_channel_enable(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL);
    dma_channel_enable(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL);

    while (wTransferState == TRANSFER_WAIT) 
    {
        retry++;
        if(retry > 10000)    break;
    }
#endif

    /* 查询方式传输 */    
#ifdef USE_SPI_POLL
    for(uint16_t i = 0; i < plen; i++)
    {
        pbuffer[i] = bsp_hspi0_Transfer_Byte(0xFF);
    }
#endif
}

/*
*********************************************************************************************************
*    函 数 名: SPIx_IRQHandler，SPIx_DMA_RX_IRQHandler，SPIx_DMA_TX_IRQHandler
*    功能说明: 中断服务程序
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
#ifdef USE_SPI_DMA
    void HSPI0_DMA_RX_IRQHandler(void)
    {
        // 清除接收标志位
        if(dma_interrupt_flag_get(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL, DMA_INT_FLAG_FTF) != RESET)
        {
            dma_interrupt_flag_clear(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
            wTransferState = TRANSFER_COMPLETE;
            dma_channel_disable(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL);
        
            dma_interrupt_flag_clear(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
            wTransferState = TRANSFER_COMPLETE;
            dma_channel_disable(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL);
        }

        // 传输错误
        if(dma_interrupt_flag_get(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL, DMA_INT_FLAG_TAE) != RESET)
        {
            dma_interrupt_flag_clear(HSPI0_DMA, HSPI0_RX_DMA_CHANNEL, DMA_INT_FLAG_TAE);
            wTransferState = TRANSFER_ERROR;
        }
    }

    void HSPI0_DMA_TX_IRQHandler(void)
    {
        // 清除发送标志位
        if(dma_interrupt_flag_get(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF) != RESET)
        {
            dma_interrupt_flag_clear(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
            // wTransferState = TRANSFER_COMPLETE;
            dma_channel_disable(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL); 
        }

        // 传输错误
        if(dma_interrupt_flag_get(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL, DMA_INT_FLAG_TAE) != RESET)
        {
            dma_interrupt_flag_clear(HSPI0_DMA, HSPI0_TX_DMA_CHANNEL, DMA_INT_FLAG_TAE);
            wTransferState = TRANSFER_ERROR;
        }
    }
#endif
    
/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
