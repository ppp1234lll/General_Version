/********************************************************************************
* @File name  : 电能计量驱动
* @Description: 硬件SPI4通信
* @Author     : ZHLE
*  Version Date        Modification Description
    7、硬件SPI1
        SPI1_SCLK：  PB10
        SPI1_MISO：  PB14       
        SPI1_MOSI：  PB15

********************************************************************************/

#include "bsp_hspi1.h"

#define HSPI1_TIMEOUT_COUNT             2000U

#define HSPI1_SCLK_GPIO_CLK             RCU_GPIOB   
#define HSPI1_SCLK_GPIO                 GPIOB
#define HSPI1_SCLK_PIN                  GPIO_PIN_10
#define HSPI1_SCLK_PIN_AF               GPIO_AF_5                      

#define HSPI1_MISO_GPIO_CLK             RCU_GPIOB
#define HSPI1_MISO_GPIO                 GPIOB
#define HSPI1_MISO_PIN                  GPIO_PIN_14
#define HSPI1_MISO_PIN_AF               GPIO_AF_5                      

#define HSPI1_MOSI_GPIO_CLK             RCU_GPIOB
#define HSPI1_MOSI_GPIO                 GPIOB
#define HSPI1_MOSI_PIN                  GPIO_PIN_15
#define HSPI1_MOSI_PIN_AF               GPIO_AF_5                      

/*
*********************************************************************************************************
*    函 数 名: bsp_InitHSPI1
*    功能说明: 硬件初始化。 
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitHSPI1(void)
{
    /* 1. 使能时钟 */
    rcu_periph_clock_enable(HSPI1_SCLK_GPIO_CLK); 
    rcu_periph_clock_enable(HSPI1_MISO_GPIO_CLK); 
    rcu_periph_clock_enable(HSPI1_MOSI_GPIO_CLK); 
    
    rcu_periph_clock_enable(RCU_SPI1);    

    /* 2. 配置GPIO复用功能 */
    gpio_af_set(HSPI1_SCLK_GPIO, HSPI1_SCLK_PIN_AF, HSPI1_SCLK_PIN);
    gpio_af_set(HSPI1_MOSI_GPIO, HSPI1_MOSI_PIN_AF, HSPI1_MOSI_PIN);
    gpio_af_set(HSPI1_MISO_GPIO, HSPI1_MISO_PIN_AF, HSPI1_MISO_PIN);

    /* 3. GPIO模式设置 */
    gpio_mode_set(HSPI1_SCLK_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, HSPI1_SCLK_PIN);   // SCK：复用推挽，无上下拉
    gpio_output_options_set(HSPI1_SCLK_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, HSPI1_SCLK_PIN);
    
    gpio_mode_set(HSPI1_MOSI_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, HSPI1_MOSI_PIN);   // MISO：复用推挽
    gpio_output_options_set(HSPI1_MOSI_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, HSPI1_MOSI_PIN);
    
    gpio_mode_set(HSPI1_MISO_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, HSPI1_MISO_PIN);   // MOSI：复用推挽
    gpio_output_options_set(HSPI1_MISO_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, HSPI1_MISO_PIN);

    /* 5. 复位SPI1并初始化参数 */
    spi_i2s_deinit(SPI1);
    spi_parameter_struct spi_init_struct;
    spi_struct_para_init(&spi_init_struct); // 结构体赋初值

    spi_init_struct.trans_mode     = SPI_TRANSMODE_FULLDUPLEX;  // 全双工模式
    spi_init_struct.device_mode    = SPI_MASTER;                // 主机模式
    spi_init_struct.frame_size     = SPI_FRAMESIZE_8BIT;        // 8位数据帧
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_2EDGE; // 模式0：空闲低电平，上升沿采样
    spi_init_struct.nss            = SPI_NSS_SOFT;              // 软件NSS管理
    spi_init_struct.prescale       = SPI_PSC_128;                // 时钟分频（16分频，频率 = PCLK2 / 16）
    spi_init_struct.endian         = SPI_ENDIAN_MSB;            // 高位在前
    spi_init(SPI1, &spi_init_struct);
    
    /* 6. 使能SPI1 */
    spi_enable(SPI1);
}

/*
*********************************************************************************************************
*    函 数 名: HSPI1_Transmit_Byte
*    功能说明: 读写字节函数
*    形    参: 
*    @TxData : 写入字节
*    返 回 值: 读取到的字节
*********************************************************************************************************
*/
uint8_t HSPI1_Transmit_Byte(uint8_t TxData)
{
    uint32_t retry = 0;

    // 等待发送缓冲区为空（TBE标志置1）
    while(RESET == spi_i2s_flag_get(SPI1, SPI_FLAG_TBE))
    {
        retry++;
        if(retry > HSPI1_TIMEOUT_COUNT) return 0;
    }

    spi_i2s_data_transmit(SPI1, TxData);
    retry = 0;

    // 等待接收缓冲区非空（RBNE标志置1，接收完成）
    while(RESET == spi_i2s_flag_get(SPI1, SPI_FLAG_RBNE))
    {
        retry++;
        if(retry > HSPI1_TIMEOUT_COUNT) return 0;
    }

    TxData = spi_i2s_data_receive(SPI1);
    retry = 0;

    // 等待本字节时钟完全发完，避免连续调用时后续字节因等待过短未真正发出
    while(SET == spi_i2s_flag_get(SPI1, SPI_FLAG_TRANS))
    {
        retry++;
        if(retry > HSPI1_TIMEOUT_COUNT) return TxData;
    }

    return TxData;
}

/*
*********************************************************************************************************
*    函 数 名: HSPI1_Send_Buffer
*    功能说明: SPI 发送数据缓冲区函数
*    形    参: 
*    @TxData        : 写入字节
*    返 回 值: 无
*********************************************************************************************************
*/
void HSPI1_Send_Buffer(uint8_t *buff, uint16_t len)
{
    while(len--) 
    {
        HSPI1_Transmit_Byte(buff[0]);
        buff++;
    }
}

