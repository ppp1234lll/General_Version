/********************************************************************************
* @File name  : 电能计量驱动
* @Description: 硬件SPI3通信
* @Author     : ZHLE
*  Version Date        Modification Description
    7、硬件SPI3
        SPI3_SCLK：  PE2
        SPI3_MISO：  PE5
        SPI3_MOSI：  PE6
********************************************************************************/

#include "bsp_hspi3.h"

#define HSPI3_SCLK_GPIO_CLK             RCU_GPIOE   
#define HSPI3_SCLK_GPIO                 GPIOE
#define HSPI3_SCLK_PIN                  GPIO_PIN_2
#define HSPI3_SCLK_PIN_AF               GPIO_AF_5                      

#define HSPI3_MISO_GPIO_CLK             RCU_GPIOE   
#define HSPI3_MISO_GPIO                 GPIOE
#define HSPI3_MISO_PIN                  GPIO_PIN_5
#define HSPI3_MISO_PIN_AF               GPIO_AF_5                      

#define HSPI3_MOSI_GPIO_CLK             RCU_GPIOE   
#define HSPI3_MOSI_GPIO                 GPIOE
#define HSPI3_MOSI_PIN                  GPIO_PIN_6
#define HSPI3_MOSI_PIN_AF               GPIO_AF_5                      

/*
*********************************************************************************************************
*    函 数 名: bsp_InitHSPI3
*    功能说明: 硬件初始化。 
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitHSPI3(void)
{
    /* 1. 使能时钟 */
    rcu_periph_clock_enable(HSPI3_SCLK_GPIO_CLK); 
    rcu_periph_clock_enable(HSPI3_MISO_GPIO_CLK); 
    rcu_periph_clock_enable(HSPI3_MOSI_GPIO_CLK);       
    
    rcu_periph_clock_enable(RCU_SPI3);    

    /* 2. 配置GPIO复用功能 */
    gpio_af_set(HSPI3_SCLK_GPIO, HSPI3_SCLK_PIN_AF, HSPI3_SCLK_PIN);
    gpio_af_set(HSPI3_MOSI_GPIO, HSPI3_MOSI_PIN_AF, HSPI3_MOSI_PIN);
    gpio_af_set(HSPI3_MISO_GPIO, HSPI3_MISO_PIN_AF, HSPI3_MISO_PIN);

    /* 3. GPIO模式设置 */
    gpio_mode_set(HSPI3_SCLK_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, HSPI3_SCLK_PIN);   // SCK：复用推挽，无上下拉
    gpio_output_options_set(HSPI3_SCLK_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, HSPI3_SCLK_PIN); 
    
    gpio_mode_set(HSPI3_MOSI_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, HSPI3_MOSI_PIN);   // MISO：复用推挽
    gpio_output_options_set(HSPI3_MOSI_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, HSPI3_MOSI_PIN);
    
    gpio_mode_set(HSPI3_MISO_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, HSPI3_MISO_PIN);   // MOSI：复用推挽
    gpio_output_options_set(HSPI3_MISO_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, HSPI3_MISO_PIN);

    /* 5. 复位SPI3并初始化参数 */
    spi_i2s_deinit(SPI3);
    spi_parameter_struct spi_init_struct;
    spi_struct_para_init(&spi_init_struct); // 结构体赋初值

    spi_init_struct.trans_mode     = SPI_TRANSMODE_FULLDUPLEX;  // 全双工模式
    spi_init_struct.device_mode    = SPI_MASTER;                // 主机模式
    spi_init_struct.frame_size     = SPI_FRAMESIZE_8BIT;        // 8位数据帧
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_2EDGE; // 模式0：空闲低电平，上升沿采样
    spi_init_struct.nss            = SPI_NSS_SOFT;              // 软件NSS管理
    spi_init_struct.prescale       = SPI_PSC_256;                // 时钟分频（16分频，频率 = PCLK2 / 16）
    spi_init_struct.endian         = SPI_ENDIAN_MSB;            // 高位在前
    spi_init(SPI3, &spi_init_struct);
    
    /* 6. 使能SPI3 */
    spi_enable(SPI3);
}

/*
*********************************************************************************************************
*    函 数 名: HSPI3_Transmit_Byte
*    功能说明: 读写字节函数
*    形    参: 
*    @TxData : 写入字节
*    返 回 值: 读取到的字节
*********************************************************************************************************
*/
uint8_t HSPI3_Transmit_Byte(uint8_t TxData)
{
    uint8_t retry = 0;                     
    // 等待发送缓冲区为空（TXE标志置1）
    while(RESET == spi_i2s_flag_get(SPI3, SPI_FLAG_TBE))
    {
        retry++;
        if(retry > 200) return 0; // 超时返回0（发送失败）
    }              
    spi_i2s_data_transmit(SPI3, TxData);         // 将数据写入SPI数据寄存器（开始发送）
    retry = 0;

    // 等待接收缓冲区非空（RXNE标志置1，接收完成）
    while(RESET == spi_i2s_flag_get(SPI3, SPI_FLAG_RBNE))
    {
        retry++;
        if(retry > 200) return 0; // 超时返回0（接收失败）
    }                                  
    return spi_i2s_data_receive(SPI3); // 返回接收的数据（从SPI数据寄存器读取）
}

/*
*********************************************************************************************************
*    函 数 名: HSPI3_Send_Buffer
*    功能说明: SPI 发送数据缓冲区函数
*    形    参: 
*    @TxData        : 写入字节
*    返 回 值: 无
*********************************************************************************************************
*/
void HSPI3_Send_Buffer(uint8_t *buff, uint16_t len)
{
    while(len--) 
    {
        HSPI3_Transmit_Byte(buff[0]);
        buff++;
    }
}

