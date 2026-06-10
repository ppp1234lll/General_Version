#include "lis3dh_iic.h"
#include "./SYSTEM/delay/delay.h"
#include "bsp.h"

/*
	8、3轴加速度计LIS3DH: (模拟IIC)，引脚分配为：  
		SCL:   PE4   SCL
		SDA:   PE3   SDA
*/

#define IIC_SCL_GPIO_CLK			RCU_GPIOE
#define IIC_SCL_GPIO_PORT           GPIOE
#define IIC_SCL_PIN                 GPIO_PIN_4

#define IIC_SDA_GPIO_CLK			RCU_GPIOE
#define IIC_SDA_GPIO_PORT           GPIOE
#define IIC_SDA_PIN                 GPIO_PIN_3

///* IO操作 */
//#define IIC_SCL   PEout(4) // SCL
//#define IIC_SDA   PEout(3) // SDA
//#define READ_SDA  PEin(3)  // 输入SDA

#define IIC_SCL(x) (x ? gpio_bit_set(IIC_SCL_GPIO_PORT,IIC_SCL_PIN) : gpio_bit_reset(IIC_SCL_GPIO_PORT,IIC_SCL_PIN))

#define IIC_SDA(x) (x ? gpio_bit_set(IIC_SDA_GPIO_PORT,IIC_SDA_PIN) : gpio_bit_reset(IIC_SDA_GPIO_PORT,IIC_SDA_PIN))
#define RD_IIC_SDA  gpio_input_bit_get(IIC_SDA_GPIO_PORT,IIC_SDA_PIN)


/************************************************************
*
* Function name	: iic_sda_in_function
* Description	: SDA配置为输入模式
* Parameter		: 
* Return		: 
*	
************************************************************/
void iic_sda_in_function(void)
{
	gpio_mode_set(IIC_SDA_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, IIC_SDA_PIN);
}

/************************************************************
*
* Function name	: iic_sda_out_function
* Description	: SDA配置为输出模式
* Parameter		: 
* Return		: 
*	
************************************************************/
void iic_sda_out_function(void)
{
	gpio_mode_set(IIC_SDA_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, IIC_SDA_PIN);
	gpio_output_options_set(IIC_SDA_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, IIC_SDA_PIN);
}

/* IO方向设置 */
#define SDA_IN()  iic_sda_in_function()
#define SDA_OUT() iic_sda_out_function()


/************************************************************
*
* Function name	: HAL_IIC_Init
* Description	: 初始化函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void LIS3DH_IIC_Init(void)
{
	/* enable GPIO clock */
	rcu_periph_clock_enable(IIC_SCL_GPIO_CLK);
	rcu_periph_clock_enable(IIC_SDA_GPIO_CLK);

	/* configure USART5 TX as alternate function push-pull */
	gpio_mode_set(IIC_SCL_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, IIC_SCL_PIN);
	gpio_output_options_set(IIC_SCL_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, IIC_SCL_PIN);

	/* configure USART5 RX as alternate function push-pull */
	gpio_mode_set(IIC_SDA_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, IIC_SDA_PIN);
	gpio_output_options_set(IIC_SDA_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, IIC_SDA_PIN);
	IIC_SCL(1);
	IIC_SDA(1); 
}

/**
 * @brief       IIC延时函数,用于控制IIC读写速度
 * @param       无
 * @retval      无
 */
static void iic_delay(void)
{
    dwt_delay_us(5);    /* 2us的延时, 读写速度在250Khz以内 */
}

/************************************************************
*
* Function name	: IIC_Start
* Description	: 起始信号
* Parameter		: 
* Return		: 
*	
************************************************************/
static void IIC_Start(void)
{
	SDA_OUT(); // sda线输出
	IIC_SDA(1);
	delay_us(6);
	IIC_SCL(1);
	delay_us(10);
 	IIC_SDA(0); // START:when CLK is high,DATA change form high to low 
	delay_us(10);
	IIC_SCL(0); // 钳住I2C总线，准备发送或接收数据 
}

/************************************************************
*
* Function name	: IIC_Stop
* Description	: 停止信号
* Parameter		: 
* Return		: 
*	
************************************************************/
static void IIC_Stop(void)
{
	SDA_OUT(); // sda线输出
	IIC_SCL(0);
	delay_us(6);
	IIC_SDA(0); // STOP:when CLK is high DATA change form low to high
	delay_us(10);
	IIC_SCL(1); 
	IIC_SDA(1); // 发送I2C总线结束信号
	delay_us(10);							   	
}
		
/************************************************************
*
* Function name	: IIC_Wait_Ack
* Description	: 应答信号
* Parameter		: 
* Return		: 1，接收应答失败 0，接收应答成功
*	
************************************************************/
static uint8_t IIC_Wait_Ack(void)
{
	uint8_t ucErrTime=0;
	SDA_IN();      // SDA设置为输入  
	IIC_SDA(1);delay_us(5);	   
	IIC_SCL(1);delay_us(5);	 
	while(RD_IIC_SDA)
	{
		ucErrTime++;
		if(ucErrTime>250)
		{
			IIC_Stop();
			return 1;
		}
	}
	IIC_SCL(0); // 时钟输出0 	   
	return 0;  
} 

/************************************************************
*
* Function name	: IIC_Ack
* Description	: 产生应答信号
* Parameter		: 
* Return		: 
*	
************************************************************/
static void IIC_Ack(void)
{
	IIC_SCL(0);
	SDA_OUT();
	IIC_SDA(0);
	delay_us(6);
	IIC_SCL(1);
	delay_us(6);
	IIC_SCL(0);
}

/************************************************************
*
* Function name	: IIC_NAck
* Description	: 不参数应答信号
* Parameter		: 
* Return		: 
*	
************************************************************/
static void IIC_NAck(void)
{
	IIC_SCL(0);
	SDA_OUT();
	IIC_SDA(1);
	delay_us(6);
	IIC_SCL(1);
	delay_us(6);
	IIC_SCL(0);
}					 				     

/************************************************************
*
* Function name	: IIC_Send_Byte
* Description	: 发送一个字节
* Parameter		: 
*	@txd		: 字节
* Return		: 1，有应答 0，无应答	
*	
************************************************************/
static void IIC_Send_Byte(uint8_t txd)
{                        
	uint8_t t;  

	SDA_OUT(); 	    
	IIC_SCL(0);	// 拉低时钟开始数据传输
	for(t=0;t<8;t++)
	{              
		IIC_SDA((txd&0x80)>>7);
		txd<<=1; 	  
		delay_us(6);   // 对TEA5767这三个延时都是必须的
		IIC_SCL(1);
		delay_us(6); 
		IIC_SCL(0);	
		delay_us(6);
	}	 
} 	    

/************************************************************
*
* Function name	: IIC_Read_Byte
* Description	: 读取一个字节
* Parameter		: 
*	@ack		: 1:发送ACK 0:发送nACK
* Return		: 字节
*	
************************************************************/
static uint8_t IIC_Read_Byte(unsigned char ack)
{
	unsigned char i,receive=0;
	
	SDA_IN();//SDA设置为输入
	for(i=0;i<8;i++ )
	{
		IIC_SCL(0); 
			delay_us(6);
		receive<<=1;
			if(RD_IIC_SDA)receive++;   
		delay_us(6);
		IIC_SCL(1);
		delay_us(6);
		delay_us(6);
	}					 
	if (!ack)
		IIC_NAck();//发送nACK
	else
		IIC_Ack(); //发送ACK   

	return receive;
}

static uint8_t m_DeviceIDs[1] = {0x30}; //默认地址
/************************************************************
*
* Function name	: HAL_IIC_EMU_Read
* Description	: 读取数据
* Parameter		: 
*	@Addr		: 地址
*	@pBuf		: 数据指针
*	@Len		: 数据长度
* Return		: 
*	
************************************************************/
bool HAL_IIC_EMU_Read(uint8_t Addr,uint8_t *pBuf,uint32_t Len)
{
	bool 	 Vale = true;
	uint32_t i;

	IIC_Start();
	IIC_Send_Byte(m_DeviceIDs[0]);
	if(IIC_Wait_Ack())
	{
		Vale = false;
		goto Exit;
	}
	IIC_Send_Byte(Addr);
	if(IIC_Wait_Ack())
	{
		Vale = false;
		goto Exit;
	}

//	delay_ms(2);

	IIC_Start();
	IIC_Send_Byte((1+ m_DeviceIDs[0]));
	if(IIC_Wait_Ack())
	{
		Vale = false;
		goto Exit;
	}
	for( i = 0;i< Len;i ++ )
	{
		if( i == (Len-1) )
		{
			pBuf[i]= IIC_Read_Byte(false);
		}
		else
		{
			pBuf[i] = IIC_Read_Byte(true);
		}
	}
	Exit:
	IIC_Stop();
	return Vale;
}

/************************************************************
*
* Function name	: HAL_IIC_EMU_Write
* Description	: 写数据
* Parameter		: 
*	@Addr		: 地址
*	@pBuff		: 数据指针
*	@Len		: 数据长度
* Return		: 
*	
************************************************************/
bool HAL_IIC_EMU_Write(uint8_t Addr,uint8_t *pBuf,uint32_t Len)
{
	uint8_t Vale = true;
	uint32_t i = 0;

	IIC_Start();
	IIC_Send_Byte(m_DeviceIDs[0]);
	if(IIC_Wait_Ack())
	{
		Vale = false;
		goto Exit;
	}
	IIC_Send_Byte(Addr);
	if(IIC_Wait_Ack())
	{
		Vale = false;
		goto Exit;
	}
	for( i = 0;i < Len;i ++ )
	{
		IIC_Send_Byte(pBuf[i]);
		if(IIC_Wait_Ack())
		{
			Vale = false;
			goto Exit;
		}
	}
	Exit:
	IIC_Stop();
	return Vale;
}
