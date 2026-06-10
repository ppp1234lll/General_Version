#include "lis3dh_spi.h"
#include "./SYSTEM/delay/delay.h"

/*
	8、3轴加速度计LIS3DH: (模拟IIC方式)，引脚分配为：
		SCL:   PE7
		SDA:   PB1
*/
/* 参数 */

#define LIS3DH_SPI_SCLK_GPIO_CLK				    RCU_GPIOE
#define LIS3DH_SPI_SCLK_GPIO_PORT           GPIOE
#define LIS3DH_SPI_SCLK_PIN                 GPIO_PIN_5

#define LIS3DH_SPI_MOSI_GPIO_CLK				    RCU_GPIOE
#define LIS3DH_SPI_MOSI_GPIO_PORT           GPIOE
#define LIS3DH_SPI_MOSI_PIN                 GPIO_PIN_4

#define LIS3DH_SPI_MISO_GPIO_CLK				    RCU_GPIOE
#define LIS3DH_SPI_MISO_GPIO_PORT           GPIOE
#define LIS3DH_SPI_MISO_PIN                 GPIO_PIN_3

//#define LIS3DH_SPI_SCK	PEout(5)  	// SCL
//#define LIS3DH_SPI_MOSI	PEout(4) 	  // SDA
//#define LIS3DH_SPI_MISO	PEin(3)   	// SDO
 	    		
/************************************************************
*
* Function name	: LIS3DH_SPI_INIT
* Description	: 初始化SPI
* Parameter		: 
* Return		: 
*	
************************************************************/
void LIS3DH_SPI_INIT(void)
{	
//	GPIO_InitTypeDef GPIO_InitStructure;

//	RCC_AHB1PeriphClockCmd(LIS3DH_SPI_SCLK_GPIO_CLK|LIS3DH_SPI_MOSI_GPIO_CLK|
//												 LIS3DH_SPI_MISO_GPIO_CLK,ENABLE);

//	GPIO_InitStructure.GPIO_Pin   = LIS3DH_SPI_SCLK_PIN;
//	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_OUT;			// 输出
//	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;  		// 推挽输出
//	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;   	// 上拉
//	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_100MHz; 	// 高速GPIO
//	GPIO_Init(LIS3DH_SPI_SCLK_GPIO_PORT,&GPIO_InitStructure);

//	GPIO_InitStructure.GPIO_Pin   = LIS3DH_SPI_MOSI_PIN;
//	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_OUT;			// 输出
//	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;  		// 推挽输出
//	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;   	// 上拉
//	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_100MHz; 	// 高速GPIO
//	GPIO_Init(LIS3DH_SPI_MOSI_GPIO_PORT,&GPIO_InitStructure);

//	GPIO_InitStructure.GPIO_Pin   = LIS3DH_SPI_MISO_PIN;
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;  // 输入
//	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  // 上拉输入
//	GPIO_Init(LIS3DH_SPI_MISO_GPIO_PORT,&GPIO_InitStructure);
}  

/************************************************************
*
* Function name	: SPI2_ReadWriteByte
* Description	: 读写一个字节
* Parameter		: 
*	@TxData		: 写入字节
* Return		: 字节
*	
************************************************************/
uint8_t LIS3DH_SPI_ReadWriteByte(uint8_t TxData)
{		
	uint8_t RecevieData=0;
	uint8_t i;

//	for(i=0;i<8;i++)  
//	{  
//		LIS3DH_SPI_SCK=0;  
//		if(TxData&0x80) 
//			LIS3DH_SPI_MOSI=1; 
//		else 
//			LIS3DH_SPI_MOSI=0;
//		TxData<<=1;  
//		LIS3DH_SPI_SCK=1;  //上升沿采样  
//		RecevieData<<=1;  
//		if(LIS3DH_SPI_MISO) 
//			RecevieData |= 0x01;
//		else 
//			RecevieData &= ~0x01;   //下降沿接收数据  
//	}  
//	LIS3DH_SPI_SCK=0;  //idle情况下SCK为电平  
//	return RecevieData;				    
}








