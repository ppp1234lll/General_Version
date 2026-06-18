/*
*********************************************************************************************************
*    函 数 名: SPI总线驱动
*    功能说明: bsp_spi_bus.c
*    形    参: V1.3
*    返 回 值: SPI总线底层驱动。提供SPI配置、收发数据、多设备共享SPI功能.
*    修改记录 :
*        版本号  日期        作者    说明
*       v1.0    2014-10-24 armfly   首版。将串行FLASH、TSC2046、VS1053、AD7705、ADS1256等SPI设备的配置
*                                    和收发数据的函数进行汇总分类。并解决不同速度的设备间的共享问题。
*        V1.1    2015-02-25 armfly   硬件SPI时，没有开启GPIOB时钟，已解决。
*        V1.2    2015-07-23 armfly   修改 bsp_SPI_Init() 函数，增加开关SPI时钟的语句。规范硬件SPI和软件SPI的宏定义。
*        V1.3    2020-03-14 Eric2013 适配STM32H7。
*    Copyright (C), 2020-2030, 安富莱电子 www.armfly.com
*********************************************************************************************************
*/

#include "bsp_spi_bus.h"
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
#define USE_SPI_DMA      /* DMA方式  */
//#define USE_SPI_INT    /* 中断方式 */
//#define USE_SPI_POLL   /* 查询方式 */


/*
*********************************************************************************************************
*                                时钟，引脚，DMA，中断等宏定义
*********************************************************************************************************
*/
#define SPIx                            SPI0
#define SPIx_CLK                        RCU_SPI0
#define DMAx_CLK                        RCU_DMA1

#define SPIx_SCK_CLK                    RCU_GPIOB
#define SPIx_SCK_GPIO                   GPIOB
#define SPIx_SCK_PIN                    GPIO_PIN_3
#define SPIx_SCK_AF                     GPIO_AF_5

#define SPIx_MISO_CLK                    RCU_GPIOB
#define SPIx_MISO_GPIO                   GPIOB
#define SPIx_MISO_PIN                    GPIO_PIN_4
#define SPIx_MISO_AF                     GPIO_AF_5

#define SPIx_MOSI_CLK                    RCU_GPIOB
#define SPIx_MOSI_GPIO                   GPIOB
#define SPIx_MOSI_PIN                    GPIO_PIN_5
#define SPIx_MOSI_AF                     GPIO_AF_5

#define SPIx_DMAx                        DMA1
#define SPIx_TX_DMA_CHANNEL              DMA_CH3
#define SPIx_RX_DMA_CHANNEL              DMA_CH2

#define SPIx_TX_DMA_PERIEN               DMA_SUBPERI3
#define SPIx_RX_DMA_PERIEN               DMA_SUBPERI3

#define SPIx_DMA_TX_IRQn                 DMA1_Channel3_IRQn
#define SPIx_DMA_RX_IRQn                 DMA1_Channel2_IRQn

#define SPIx_DMA_TX_IRQHandler           DMA1_Channel3_IRQHandler
#define SPIx_DMA_RX_IRQHandler           DMA1_Channel2_IRQHandler

#define SPIx_IRQn                        SPI0_IRQn
#define SPIx_IRQHandler                  SPI0_IRQHandler

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

uint32_t g_spiLen;    
uint8_t  g_spi_busy; /* SPI忙状态，0表示不忙，1表示忙 */
__IO uint32_t wTransferState = TRANSFER_WAIT;

uint8_t g_spiTxBuf[SPI_BUFFER_SIZE];  
uint8_t g_spiRxBuf[SPI_BUFFER_SIZE];

/*
*********************************************************************************************************
*    函 数 名: bsp_InitSPIBus
*    功能说明: 配置SPI总线。
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitSPIBus(void)
{    
    g_spi_busy = 0;
    bsp_InitSPIGPIO();
    bsp_InitSPIDMA();
    bsp_InitSPIParam(SPI_BAUDRATEPRESCALER_15M, SPI_CK_PL_HIGH_PH_2EDGE);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitSPIParam
*    功能说明: 配置SPI总线参数，时钟分频，时钟相位和时钟极性。
*    形    参: _BaudRatePrescaler  SPI总线时钟分频设置，支持的参数如下：
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitSPIParam(uint32_t _BaudRatePrescaler, uint32_t _CLKPolarity_Phase)
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
    spi_init(SPIx, &spi_init_struct);

    spi_enable(SPIx);    

//    spi_i2s_data_transmit(SPIx, 0xFF);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitSPIGPIO
*    功能说明: 配置SPI总线时钟，GPIO
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitSPIGPIO(void)
{
    /* SPI和GPIP时钟 */
    rcu_periph_clock_enable(SPIx_SCK_CLK);
    rcu_periph_clock_enable(SPIx_MISO_CLK);
    rcu_periph_clock_enable(SPIx_MOSI_CLK);
    rcu_periph_clock_enable(SPIx_CLK);

    /* SPI SCK */
    gpio_af_set(SPIx_SCK_GPIO, SPIx_SCK_AF, SPIx_SCK_PIN);
    gpio_mode_set(SPIx_SCK_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, SPIx_SCK_PIN);
    gpio_output_options_set(SPIx_SCK_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPIx_SCK_PIN);

    /* SPI MISO */
    gpio_af_set(SPIx_MISO_GPIO, SPIx_MISO_AF, SPIx_MISO_PIN);
    gpio_mode_set(SPIx_MISO_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, SPIx_MISO_PIN);
    gpio_output_options_set(SPIx_MISO_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPIx_MISO_PIN);

    /* SPI MOSI */
    gpio_af_set(SPIx_MOSI_GPIO, SPIx_MOSI_AF, SPIx_MOSI_PIN);
    gpio_mode_set(SPIx_MOSI_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, SPIx_MOSI_PIN);
    gpio_output_options_set(SPIx_MOSI_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPIx_MOSI_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitSPIDMA
*    功能说明: 配置SPI DMA
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitSPIDMA(void)
{
    /* 配置DMA和NVIC */
    #ifdef USE_SPI_DMA
    {
        dma_single_data_parameter_struct dma_tx_struct;
        dma_single_data_parameter_struct dma_rx_struct;

        /* 使能DMA时钟 */
        rcu_periph_clock_enable(DMAx_CLK);      

        /* SPI DMA发送配置 */    
        dma_deinit(SPIx_DMAx, SPIx_TX_DMA_CHANNEL);    
        dma_tx_struct.periph_addr         = (uint32_t)&SPI_DATA(SPIx);
        dma_tx_struct.memory0_addr        = (uint32_t)g_spiTxBuf;
        dma_tx_struct.direction           = DMA_MEMORY_TO_PERIPH;
        dma_tx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
        dma_tx_struct.priority            = DMA_PRIORITY_LOW;
        dma_tx_struct.number              = SPI_BUFFER_SIZE;
        dma_tx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
        dma_tx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
        dma_tx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
        dma_single_data_mode_init(SPIx_DMAx, SPIx_TX_DMA_CHANNEL, &dma_tx_struct);    
        dma_channel_subperipheral_select(SPIx_DMAx, SPIx_TX_DMA_CHANNEL, SPIx_TX_DMA_PERIEN);
        
        /* enable SPI DMA */
        spi_dma_enable(SPIx, SPI_DMA_TRANSMIT);
    
        /* SPI DMA接收配置 */    
        dma_deinit(SPIx_DMAx, SPIx_RX_DMA_CHANNEL);
        dma_rx_struct.periph_addr         = (uint32_t)&SPI_DATA(SPIx);
        dma_rx_struct.memory0_addr        = (uint32_t)g_spiRxBuf;
        dma_rx_struct.direction           = DMA_PERIPH_TO_MEMORY;
        dma_rx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
        dma_rx_struct.priority            = DMA_PRIORITY_HIGH;
        dma_rx_struct.number              = SPI_BUFFER_SIZE;
        dma_rx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
        dma_rx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
        dma_rx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
        dma_single_data_mode_init(SPIx_DMAx, SPIx_RX_DMA_CHANNEL, &dma_rx_struct);
        dma_channel_subperipheral_select(SPIx_DMAx, SPIx_RX_DMA_CHANNEL, SPIx_RX_DMA_PERIEN);

        /* enable SPI DMA */
        spi_dma_enable(SPIx, SPI_DMA_RECEIVE);

        /* 配置DMA发送中断 */
        nvic_irq_enable(SPIx_DMA_TX_IRQn, 5, 0);
        dma_interrupt_enable(SPIx_DMAx, SPIx_TX_DMA_CHANNEL, DMA_INT_FTF);
        
        /* 配置DMA接收中断 */
        nvic_irq_enable(SPIx_DMA_RX_IRQn, 5, 0);
        dma_interrupt_enable(SPIx_DMAx, SPIx_RX_DMA_CHANNEL, DMA_INT_FTF);
        
    }
    #endif
    
    #ifdef USE_SPI_INT
        /* 配置SPI中断优先级并使能中断 */
        nvic_irq_enable(SPIx_IRQn, 5, 0);
        // 注意：不要在这里全局开启 RBNE，否则只要有数据就会频繁进中断
        // 应该在需要传输的时候再开启，传完关闭
        // spi_i2s_interrupt_enable(SPIx, SPI_I2S_INT_RBNE); 
    #endif
}

/*
*********************************************************************************************************
*    函 数 名: bsp_spiTransfer_Byte
*    功能说明: 发送一个字节并接收一个字节
*    形    参: TxData - 发送的字节
*    返 回 值: 接收的字节
*********************************************************************************************************
*/
uint8_t bsp_spiTransfer_Byte(uint8_t TxData)
{
    uint8_t retry = 0;                     
    // 等待发送缓冲区为空（TXE标志置1）
    while(RESET == spi_i2s_flag_get(SPIx, SPI_FLAG_TBE))
    {
        retry++;
        if(retry > 200) return 0; // 超时返回0（发送失败）
    }              
    spi_i2s_data_transmit(SPIx, TxData);         // 将数据写入SPI数据寄存器（开始发送）
    retry = 0;

    // 等待接收缓冲区非空（RXNE标志置1，接收完成）
    while(RESET == spi_i2s_flag_get(SPIx, SPI_FLAG_RBNE))
    {
        retry++;
        if(retry > 200) return 0; // 超时返回0（接收失败）
    }                                  
    return spi_i2s_data_receive(SPIx); // 返回接收的数据（从SPI数据寄存器读取）
}

/*
*********************************************************************************************************
*    函 数 名: bsp_spiTransfer
*    功能说明: 启动数据传输
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_spiTransfer(void)
{
    if (g_spiLen > SPI_BUFFER_SIZE)
    {
        return;
    }
    
    /* DMA方式传输 */
#ifdef USE_SPI_DMA
    wTransferState = TRANSFER_WAIT;

    // 关闭SPI DMA
    dma_channel_disable(SPIx_DMAx, SPIx_TX_DMA_CHANNEL);
    dma_channel_disable(SPIx_DMAx, SPIx_RX_DMA_CHANNEL);

    // 设置发送数据量
    dma_transfer_number_config(SPIx_DMAx, SPIx_TX_DMA_CHANNEL, g_spiLen);
    // 设置接收数据量
    dma_transfer_number_config(SPIx_DMAx, SPIx_RX_DMA_CHANNEL, g_spiLen);
    spi_i2s_data_receive(SPIx);
    // 清除DMA标志位
    dma_interrupt_flag_clear(SPIx_DMAx, SPIx_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
    dma_interrupt_flag_clear(SPIx_DMAx, SPIx_RX_DMA_CHANNEL, DMA_INT_FLAG_FTF);

    // 使能 DMA
    dma_channel_enable(SPIx_DMAx, SPIx_TX_DMA_CHANNEL);
    dma_channel_enable(SPIx_DMAx, SPIx_RX_DMA_CHANNEL);

    while (wTransferState == TRANSFER_WAIT)
    {
        ;
    }
#endif

    /* 中断方式传输 */    
#ifdef USE_SPI_INT
    wTransferState = TRANSFER_WAIT;

    // 在这里使能接收和发送中断
    spi_i2s_interrupt_enable(SPIx, SPI_I2S_INT_RBNE); 
    spi_i2s_interrupt_enable(SPIx, SPI_I2S_INT_TBE);  

    while (wTransferState == TRANSFER_WAIT)
    {
        ;
    }

#endif

    /* 查询方式传输 */    
#ifdef USE_SPI_POLL
    for(uint16_t i = 0; i < g_spiLen; i++)
    {
        g_spiRxBuf[i] = bsp_spiTransfer_Byte(g_spiTxBuf[i]);
    }
#endif
}

/*
*********************************************************************************************************
*    函 数 名: bsp_SpiBusEnter
*    功能说明: 占用SPI总线
*    形    参: 无
*    返 回 值: 0 表示不忙  1表示忙
*********************************************************************************************************
*/
void bsp_SpiBusEnter(void)
{
    g_spi_busy = 1;
}

/*
*********************************************************************************************************
*    函 数 名: bsp_SpiBusExit
*    功能说明: 释放占用的SPI总线
*    形    参: 无
*    返 回 值: 0 表示不忙  1表示忙
*********************************************************************************************************
*/
void bsp_SpiBusExit(void)
{
    g_spi_busy = 0;
}

/*
*********************************************************************************************************
*    函 数 名: bsp_SpiBusBusy
*    功能说明: 判断SPI总线忙，方法是检测其他SPI芯片的片选信号是否为1
*    形    参: 无
*    返 回 值: 0 表示不忙  1表示忙
*********************************************************************************************************
*/
uint8_t bsp_SpiBusBusy(void)
{
    return g_spi_busy;
}

/*
*********************************************************************************************************
*    函 数 名: SPIx_IRQHandler，SPIx_DMA_RX_IRQHandler，SPIx_DMA_TX_IRQHandler
*    功能说明: 中断服务程序
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
#ifdef USE_SPI_INT
    void SPIx_IRQHandler(void)
    {
        static uint16_t tx_index = 0;
        static uint16_t rx_index = 0;
        // 接收中断
        if (spi_i2s_interrupt_flag_get(SPIx, SPI_I2S_INT_FLAG_RBNE) != RESET) 
        {
            g_spiRxBuf[rx_index++] = spi_i2s_data_receive(SPIx);
            if (rx_index >= g_spiLen) 
            {
                // 传输完成后关闭接收中断
                spi_i2s_interrupt_disable(SPIx, SPI_I2S_INT_RBNE);
                wTransferState = TRANSFER_COMPLETE;
                rx_index = 0;
            }
        }
        
        // 发送中断
        if (spi_i2s_interrupt_flag_get(SPIx, SPI_I2S_INT_FLAG_TBE) != RESET)
        {
            if (tx_index < g_spiLen) 
            {
                spi_i2s_data_transmit(SPIx, g_spiTxBuf[tx_index++]);
            } 
            else 
            {
                // 发送完成，关闭发送中断，防止一直进中断
                spi_i2s_interrupt_disable(SPIx, SPI_I2S_INT_TBE);
                // wTransferState = TRANSFER_COMPLETE;
                tx_index = 0;
            }
        }
        
        // 错误中断
        if (spi_i2s_interrupt_flag_get(SPIx, SPI_I2S_INT_ERR) != RESET)
        {
            wTransferState = TRANSFER_ERROR;
            spi_i2s_data_receive(SPIx);
            SPI_STAT(SPIx);
        }
    }    
#endif

#ifdef USE_SPI_DMA
    void SPIx_DMA_RX_IRQHandler(void)
    {
        // 清除接收标志位
        if(dma_interrupt_flag_get(SPIx_DMAx, SPIx_RX_DMA_CHANNEL, DMA_INT_FLAG_FTF) != RESET)
        {
            dma_interrupt_flag_clear(SPIx_DMAx, SPIx_RX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
            wTransferState = TRANSFER_COMPLETE;
            dma_channel_disable(SPIx_DMAx, SPIx_RX_DMA_CHANNEL);
        }

        // 传输错误
        if(dma_interrupt_flag_get(SPIx_DMAx, SPIx_RX_DMA_CHANNEL, DMA_INT_FLAG_TAE) != RESET)
        {
            dma_interrupt_flag_clear(SPIx_DMAx, SPIx_RX_DMA_CHANNEL, DMA_INT_FLAG_TAE);
            wTransferState = TRANSFER_ERROR;
        }
    }

    void SPIx_DMA_TX_IRQHandler(void)
    {
        // 清除发送标志位
        if(dma_interrupt_flag_get(SPIx_DMAx, SPIx_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF) != RESET)
        {
            dma_interrupt_flag_clear(SPIx_DMAx, SPIx_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
            // wTransferState = TRANSFER_COMPLETE;
            dma_channel_disable(SPIx_DMAx, SPIx_TX_DMA_CHANNEL);
        }

        // 传输错误
        if(dma_interrupt_flag_get(SPIx_DMAx, SPIx_TX_DMA_CHANNEL, DMA_INT_FLAG_TAE) != RESET)
        {
            dma_interrupt_flag_clear(SPIx_DMAx, SPIx_TX_DMA_CHANNEL, DMA_INT_FLAG_TAE);
            wTransferState = TRANSFER_ERROR;
        }
    }
#endif
    
/******************************************  (END OF FILE) **********************************************/

