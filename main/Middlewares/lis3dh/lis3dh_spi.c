#include "lis3dh_spi.h"
#include "bsp.h"

/*
    8、3轴加速度计LIS3DH: (SPI方式)，引脚分配为：
        SCL:   PE7
        SDA:   PB1
        SDO:   PE8
*/

#define LIS3DH_SPI_SCLK_GPIO_CLK            RCU_GPIOE
#define LIS3DH_SPI_SCLK_GPIO_PORT           GPIOE
#define LIS3DH_SPI_SCLK_PIN                 GPIO_PIN_7

#define LIS3DH_SPI_MOSI_GPIO_CLK            RCU_GPIOB
#define LIS3DH_SPI_MOSI_GPIO_PORT           GPIOB
#define LIS3DH_SPI_MOSI_PIN                 GPIO_PIN_1

#define LIS3DH_SPI_MISO_GPIO_CLK            RCU_GPIOE   
#define LIS3DH_SPI_MISO_GPIO_PORT           GPIOE
#define LIS3DH_SPI_MISO_PIN                 GPIO_PIN_8

#define LIS3DH_SPI_SCK(x)           (x ? gpio_bit_set(LIS3DH_SPI_SCLK_GPIO_PORT,LIS3DH_SPI_SCLK_PIN) : gpio_bit_reset(LIS3DH_SPI_SCLK_GPIO_PORT,LIS3DH_SPI_SCLK_PIN))
#define LIS3DH_SPI_MOSI(x)          (x ? gpio_bit_set(LIS3DH_SPI_MOSI_GPIO_PORT,LIS3DH_SPI_MOSI_PIN) : gpio_bit_reset(LIS3DH_SPI_MOSI_GPIO_PORT,LIS3DH_SPI_MOSI_PIN))
#define READ_LIS3DH_SPI_MISO        gpio_input_bit_get(LIS3DH_SPI_MISO_GPIO_PORT, LIS3DH_SPI_MISO_PIN)
/*
*********************************************************************************************************
*    函 数 名: LIS3DH_SPI_INIT
*    功能说明: 初始化SPI
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void LIS3DH_SPI_INIT(void)
{    
    rcu_periph_clock_enable(LIS3DH_SPI_SCLK_GPIO_CLK);
    rcu_periph_clock_enable(LIS3DH_SPI_MOSI_GPIO_CLK);
    rcu_periph_clock_enable(LIS3DH_SPI_MISO_GPIO_CLK);

    /* configure IIC_SCL as alternate function push-pull */
    gpio_mode_set(LIS3DH_SPI_SCLK_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LIS3DH_SPI_SCLK_PIN);
    gpio_output_options_set(LIS3DH_SPI_SCLK_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, LIS3DH_SPI_SCLK_PIN);

    /* configure IIC_SDA as alternate function push-pull */
    gpio_mode_set(LIS3DH_SPI_MOSI_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LIS3DH_SPI_MOSI_PIN);
    gpio_output_options_set(LIS3DH_SPI_MOSI_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, LIS3DH_SPI_MOSI_PIN);
    
    /* configure IIC_SDO as alternate function push-pull */
    gpio_mode_set(LIS3DH_SPI_MISO_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, LIS3DH_SPI_MISO_PIN);
}  

/*
*********************************************************************************************************
*    函 数 名: LIS3DH_SPI_ReadWriteByte
*    功能说明: 读写一个字节
*    形    参: 无
*    返 回 值: 字节
*********************************************************************************************************
*/
uint8_t LIS3DH_SPI_ReadWriteByte(uint8_t TxData)
{        
    uint8_t RecevieData=0;
    uint8_t i;

    for(i=0;i<8;i++)  
    {  
        LIS3DH_SPI_SCK(0);  
        if(TxData&0x80) 
            LIS3DH_SPI_MOSI(1); 
        else 
            LIS3DH_SPI_MOSI(0);
        TxData<<=1;  
        LIS3DH_SPI_SCK(1);  //上升沿采样  
        RecevieData<<=1;  
        if(READ_LIS3DH_SPI_MISO) 
            RecevieData |= 0x01;
        else 
            RecevieData &= ~0x01;   //下降沿接收数据  
    }  
    LIS3DH_SPI_SCK(0);  //idle情况下SCK为电平  
    return RecevieData;                    
}








