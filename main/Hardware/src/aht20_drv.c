/********************************************************************************
* @File name  : 温湿度模块
* @Description: 模拟IIC通信
* @Author     : ZHLE
*  Version Date        Modification Description
	9、AHT20温湿度传感器：(模拟IIC方式)，引脚分配为：  
		      SCL:	PD12
		      SDA:  PD11

********************************************************************************/

#include "aht20_drv.h"
#include "./SYSTEM/delay/delay.h"

#define AHT20_SCL_GPIO_CLK        RCU_GPIOB
#define AHT20_SCL_GPIO            GPIOB
#define AHT20_SCL_PIN             GPIO_PIN_15

#define AHT20_SDA_GPIO_CLK        RCU_GPIOB
#define AHT20_SDA_GPIO            GPIOB
#define AHT20_SDA_PIN             GPIO_PIN_14

#define AHT20_SCL(x) (x ? gpio_bit_set(AHT20_SCL_GPIO,AHT20_SCL_PIN) : gpio_bit_reset(AHT20_SCL_GPIO,AHT20_SCL_PIN))

#define AHT20_SDA(x) (x ? gpio_bit_set(AHT20_SDA_GPIO,AHT20_SDA_PIN) : gpio_bit_reset(AHT20_SDA_GPIO,AHT20_SDA_PIN))
#define RD_AHT20_SDA  gpio_input_bit_get(AHT20_SDA_GPIO,AHT20_SDA_PIN)


/*
*********************************************************************************************************
*	函 数 名: aht20_drive_sda_in
*	功能说明: 设置SDA为输入
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void aht20_drive_sda_in(void)
{
   gpio_mode_set(AHT20_SDA_GPIO, GPIO_MODE_INPUT, GPIO_PUPD_NONE, AHT20_SDA_PIN);  
}
/*
*********************************************************************************************************
*	函 数 名: aht20_drive_sda_out
*	功能说明: 设置SDA为输出
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void aht20_drive_sda_out(void)
{
   gpio_mode_set(AHT20_SDA_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, AHT20_SDA_PIN); 
   gpio_output_options_set(AHT20_SDA_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, AHT20_SDA_PIN);
}


/* 重新宏定义 */
#define HTU21D_SDA_OUT() aht20_drive_sda_out()
#define HTU21D_SDA_IN()  aht20_drive_sda_in()

/*
*********************************************************************************************************
*	函 数 名: aht20_i2c_init
*	功能说明: aht20初始化
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void aht20_i2c_init(void)
{
	/* enable GPIO clock */
	rcu_periph_clock_enable(AHT20_SCL_GPIO_CLK);
	rcu_periph_clock_enable(AHT20_SDA_GPIO_CLK);

	/* configure USART5 TX as alternate function push-pull */
	gpio_mode_set(AHT20_SCL_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, AHT20_SCL_PIN);
	gpio_output_options_set(AHT20_SCL_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, AHT20_SCL_PIN);

	/* configure USART5 RX as alternate function push-pull */
	gpio_mode_set(AHT20_SDA_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, AHT20_SDA_PIN);
	gpio_output_options_set(AHT20_SDA_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, AHT20_SDA_PIN);
	AHT20_SCL(1);
	AHT20_SDA(1);
}

/*
*********************************************************************************************************
*	函 数 名: aht20_i2c_start
*	功能说明: 开始信号
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void aht20_i2c_start(void)
{
	HTU21D_SDA_OUT();
	AHT20_SDA(1);
	AHT20_SCL(1);
	delay_us(4);
 	AHT20_SDA(0);
	delay_us(4);
	AHT20_SCL(0);
}

/*
*********************************************************************************************************
*	函 数 名: aht20_i2c_stop
*	功能说明: 结束信号
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void aht20_i2c_stop(void)
{
	HTU21D_SDA_OUT();
	AHT20_SCL(0);
	AHT20_SDA(0);
 	delay_us(4);
	AHT20_SCL(1);
	delay_us(4);
	AHT20_SDA(1);
}

/*
*********************************************************************************************************
*	函 数 名: aht20_i2c_wait_ack
*	功能说明: 等待应答
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t aht20_i2c_wait_ack(void)
{
	uint8_t ucErrTime=0;
    
	HTU21D_SDA_IN();
	AHT20_SDA(1);
	delay_us(1);	   
	AHT20_SCL(1);
	delay_us(1);
	
	while(RD_AHT20_SDA)
	{
		ucErrTime++;
		if(ucErrTime > 250)
		{
			aht20_i2c_stop();
			return 1;
		}
	}
	AHT20_SCL(0);//时钟输出0
	return 0;
} 

/*
*********************************************************************************************************
*	函 数 名: aht20_i2c_ack
*	功能说明: 应答信号
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void aht20_i2c_ack(void)
{
	AHT20_SCL(0);
	HTU21D_SDA_OUT();
	AHT20_SDA(0);
	delay_us(2);
	AHT20_SCL(1);
	delay_us(2);
	AHT20_SCL(0);
}

/*
*********************************************************************************************************
*	函 数 名: aht20_i2c_nack
*	功能说明: 无应答信号
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void aht20_i2c_nack(void)
{
	AHT20_SCL(0);
	HTU21D_SDA_OUT();
	AHT20_SDA(1);
	delay_us(2);
	AHT20_SCL(1);
	delay_us(2);
	AHT20_SCL(0);
}					 				     

/*
*********************************************************************************************************
*	函 数 名: aht20_drive_write_byte
*	功能说明: 写字节
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void aht20_drive_write_byte(uint8_t dat)
{                        
	uint8_t t;   

	HTU21D_SDA_OUT();
	AHT20_SCL(0);//拉低时钟开始数据传输
	
	for(t=0;t<8;t++)
	{              
		if(dat&0x80)
		{
			AHT20_SDA(1);
		}
		else
		{
			AHT20_SDA(0);
		}
		dat<<=1; 	  
		delay_us(2); 
		AHT20_SCL(1);
		delay_us(2);
		AHT20_SCL(0);
		delay_us(2); 
	}
} 

/*
*********************************************************************************************************
*	函 数 名: aht20_drive_read_byte
*	功能说明: 读字节
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t aht20_drive_read_byte(uint8_t ack)
{
	uint8_t i = 0;
	uint8_t receive = 0;
	
	HTU21D_SDA_IN();

	for(i=0;i<8;i++ )
	{
		AHT20_SCL(0);
		delay_us(2);
		AHT20_SCL(1);
		receive <<= 1;
		if(RD_AHT20_SDA)
		{
			receive++;
		}
		delay_us(1);
  }

	if(ack == 0)
		aht20_i2c_nack();//发送nACK
	else if(ack == 1)
		aht20_i2c_ack(); //发送ACK

	return receive;
}
