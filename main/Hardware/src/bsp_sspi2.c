/*
*********************************************************************************************************
*    函 数 名: 电能计量驱动
*    功能说明: 模拟SPI通信
*    形    参: ZHLE
*    返 回 值: 
*********************************************************************************************************
*/
#include "bsp_sspi2.h"
#include "bsp.h"

#define SSPI2_SCLK_GPIO_CLK             RCU_GPIOC   
#define SSPI2_SCLK_GPIO                 GPIOC
#define SSPI2_SCLK_PIN                  GPIO_PIN_10                  

#define SSPI2_MISO_GPIO_CLK             RCU_GPIOC
#define SSPI2_MISO_GPIO                 GPIOC
#define SSPI2_MISO_PIN                  GPIO_PIN_11                  

#define SSPI2_MOSI_GPIO_CLK             RCU_GPIOC
#define SSPI2_MOSI_GPIO                 GPIOC
#define SSPI2_MOSI_PIN                  GPIO_PIN_12

#define SSPI2_SCLK(x)     (x ? gpio_bit_set(SSPI2_SCLK_GPIO,SSPI2_SCLK_PIN) : gpio_bit_reset(SSPI2_SCLK_GPIO,SSPI2_SCLK_PIN))
#define SSPI2_MOSI(x)     (x ? gpio_bit_set(SSPI2_MOSI_GPIO,SSPI2_MOSI_PIN) : gpio_bit_reset(SSPI2_MOSI_GPIO,SSPI2_MOSI_PIN))

#define READ_SSPI2_MISO   gpio_input_bit_get(SSPI2_MISO_GPIO,SSPI2_MISO_PIN)
/*
*********************************************************************************************************
*    函 数 名: bsp_InitSSPI2
*    功能说明: 配置模拟SPI GPIO。
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitSSPI2(void)
{
    /* 1. 使能时钟 */
    rcu_periph_clock_enable(SSPI2_SCLK_GPIO_CLK); 
    rcu_periph_clock_enable(SSPI2_MISO_GPIO_CLK); 
    rcu_periph_clock_enable(SSPI2_MOSI_GPIO_CLK); 
    
    /* 3. GPIO模式设置 */
    gpio_mode_set(SSPI2_SCLK_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SSPI2_SCLK_PIN); 
    gpio_output_options_set(SSPI2_SCLK_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SSPI2_SCLK_PIN);
    
    gpio_mode_set(SSPI2_MOSI_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SSPI2_MOSI_PIN); 
    gpio_output_options_set(SSPI2_MOSI_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SSPI2_MOSI_PIN);	
    
    gpio_mode_set(SSPI2_MISO_GPIO, GPIO_MODE_INPUT, GPIO_PUPD_NONE, SSPI2_MISO_PIN);  
    gpio_output_options_set(SSPI2_MISO_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SSPI2_MISO_PIN);	
}

/*
*********************************************************************************************************
*    函 数 名: sspi2_delay
*    功能说明: 软件SPI延时
*    形    参: time 时间
*    返 回 值: 无
*********************************************************************************************************
*/
void sspi2_delay(uint16_t time)	
{
	do
	{
	} while (--time);
}

/*
*********************************************************************************************************
*    函 数 名: SSPI2_Transmit_Byte
*    功能说明: 读写字节函数
*    形    参: 
*    返 回 值: 写入字节
*	返 回 值: 读取到的字节
*********************************************************************************************************
*/
uint8_t SSPI2_Transmit_Byte(uint8_t TxData)
{
	uint8_t RecevieData=0;
	uint8_t i = 0;

	for(i=0; i<8; i++)
	{
		SSPI2_SCLK(0);
		sspi2_delay(20);
		if(TxData&0x80) SSPI2_MOSI(1);
		else SSPI2_MOSI(0);
		TxData<<=1;
		sspi2_delay(20);
		SSPI2_SCLK(1);  // 上升沿采样
		sspi2_delay(20);
		RecevieData<<=1;
		if(READ_SSPI2_MISO) RecevieData |= 0x01;
		else RecevieData &= ~0x01;   // 下降沿接收数据
		sspi2_delay(20);
	}
	SSPI2_SCLK(0);  // idle情况下SCK为电平
	sspi2_delay(20);
	return RecevieData;
}

/*
*********************************************************************************************************
*    函 数 名: SSPI2_Write_Byte
*    功能说明: 模拟 SPI 写一个字节
*    形    参: 
*    返 回 值: 写入字节
*	返 回 值:  
*********************************************************************************************************
*/
void SSPI2_Write_Byte(uint8_t TxData)  
{
	uint8_t i = 0;  
	for(i=0; i<8; i++) 
	{
		SSPI2_SCLK(0); //CPOL=0        //拉低时钟，即空闲时钟为低电平， CPOL=0；
		if(TxData&0x80) SSPI2_MOSI(1);
		else SSPI2_MOSI(0);
		TxData<<=1;
		sspi2_delay(20); 
		SSPI2_SCLK(1);                   // 上升沿采样 //CPHA=0  
		sspi2_delay(20); 
	}
	SSPI2_SCLK(0);                   // 最后SPI发送完后，拉低时钟，进入空闲状态；
}
/*
*********************************************************************************************************
*    函 数 名: SSPI2_Write_Buffer
*    功能说明: 模拟 SPI 写多个字节
*    形    参: 
*    返 回 值: 写入的缓冲区地址
*	@len		: 写入的字节数
*	返 回 值:  
*********************************************************************************************************
*/
void SSPI2_Write_Buffer(uint8_t *buff, uint16_t len)
{
	while(len--) {
		SSPI2_Write_Byte(buff[0]);
		buff++;
	}
}

/*
*********************************************************************************************************
*    函 数 名: SSPI2_Read_Byte
*    功能说明: 模拟 SPI 读一个字节
*    形    参: 
*    返 回 值: 读取到的字节
*********************************************************************************************************
*/
uint8_t SSPI2_Read_Byte(void)
{
	uint8_t i = 0;
	uint8_t RecevieData=0;
	for(i=0; i<8; i++) 
	{
		SSPI2_SCLK(1);           //拉低时钟，即空闲时钟为低电平；  
		sspi2_delay(20); 
		SSPI2_SCLK(0);   
		RecevieData<<=1;
		if(READ_SSPI2_MISO) RecevieData |= 0x01;
		else RecevieData &= ~0x01;   // 下降沿接收数据
		sspi2_delay(20); 
	}
	SSPI2_SCLK(0);  // idle情况下SCK为电平
	return RecevieData;
}  


/*
*********************************************************************************************************
*    函 数 名: SSPI2_test
*    功能说明: SPI测试
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void SSPI2_test(void)
{
//	while(1)
//	{
//		SSPI2_Read_Byte();
//	}
}






