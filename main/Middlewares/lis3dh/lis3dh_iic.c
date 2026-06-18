#include "lis3dh_iic.h"
#include "bsp.h"

/*
	8、3轴加速度计LIS3DH: (模拟IIC)，引脚分配为：  
		SCL:   PE7
		SDA:   PB1
*/

#define IIC_SCL_GPIO_CLK            RCU_GPIOE
#define IIC_SCL_GPIO_PORT           GPIOE
#define IIC_SCL_PIN                 GPIO_PIN_7

#define IIC_SDA_GPIO_CLK            RCU_GPIOB
#define IIC_SDA_GPIO_PORT           GPIOB
#define IIC_SDA_PIN                 GPIO_PIN_1

/* IO操作 */
#define IIC_SCL(x) (x ? gpio_bit_set(IIC_SCL_GPIO_PORT,IIC_SCL_PIN) : gpio_bit_reset(IIC_SCL_GPIO_PORT,IIC_SCL_PIN))
#define IIC_SDA(x) (x ? gpio_bit_set(IIC_SDA_GPIO_PORT,IIC_SDA_PIN) : gpio_bit_reset(IIC_SDA_GPIO_PORT,IIC_SDA_PIN))

#define READ_IIC_SDA  gpio_input_bit_get(IIC_SDA_GPIO_PORT,IIC_SDA_PIN)

/*
*********************************************************************************************************
*    函 数 名: iic_sda_in
*    功能说明: SDA配置为输入模式
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void iic_sda_in(void)
{
    gpio_mode_set(IIC_SDA_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, IIC_SDA_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: iic_sda_out
*    功能说明: SDA配置为输出模式
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void iic_sda_out(void)
{
    gpio_mode_set(IIC_SDA_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, IIC_SDA_PIN);
    gpio_output_options_set(IIC_SDA_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, IIC_SDA_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: LIS3DH_IIC_Init
*    功能说明: 初始化IIC接口
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void LIS3DH_IIC_Init(void)
{
    /* enable GPIO clock */
    rcu_periph_clock_enable(IIC_SCL_GPIO_CLK);
    rcu_periph_clock_enable(IIC_SDA_GPIO_CLK);

    /* configure SCL as alternate function push-pull */
    gpio_mode_set(IIC_SCL_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, IIC_SCL_PIN);
    gpio_output_options_set(IIC_SCL_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, IIC_SCL_PIN);

    /* configure SDA as alternate function push-pull */
    gpio_mode_set(IIC_SDA_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, IIC_SDA_PIN);
    gpio_output_options_set(IIC_SDA_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, IIC_SDA_PIN);
    IIC_SCL(1);
    IIC_SDA(1); 
}

/*
*********************************************************************************************************
*    函 数 名: iic_delay
*    功能说明: 延时函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void iic_delay(void)
{
    dwt_delay_us(5);    /* 2us的延时, 读写速度在250Khz以内 */
}

/*
*********************************************************************************************************
*    函 数 名: IIC_Start
*    功能说明: 发送起始信号
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void IIC_Start(void)
{
    iic_sda_out(); // sda线输出
    IIC_SDA(1);
    iic_delay();
    IIC_SCL(1);
    iic_delay();
    IIC_SDA(0); // START:when CLK is high,DATA change form high to low 
    iic_delay();
    IIC_SCL(0); // 钳住I2C总线，准备发送或接收数据 
}

/*
*********************************************************************************************************
*    函 数 名: IIC_Stop
*    功能说明: 发送停止信号
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void IIC_Stop(void)
{
    iic_sda_out(); // sda线输出
    IIC_SCL(0);
    iic_delay();
    IIC_SDA(0); // STOP:when CLK is high DATA change form low to high
    iic_delay();
    IIC_SCL(1); 
    IIC_SDA(1); // 发送I2C总线结束信号
    iic_delay();                                   
}
        
/*
*********************************************************************************************************
*    函 数 名: IIC_Wait_Ack
*    功能说明: 等待应答信号
*    形    参: 无
*    返 回 值: 1，接收应答失败 0，接收应答成功
*********************************************************************************************************
*/
static uint8_t IIC_Wait_Ack(void)
{
    uint8_t ucErrTime=0;
    iic_sda_in();      // SDA设置为输入  
    IIC_SDA(1);
    iic_delay();       
    IIC_SCL(1);
    iic_delay();     
    while(READ_IIC_SDA)
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

/*
*********************************************************************************************************
*    函 数 名: IIC_Ack
*    功能说明: 产生应答信号
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void IIC_Ack(void)
{
    IIC_SCL(0);
    iic_sda_out();
    IIC_SDA(0);
    iic_delay();
    IIC_SCL(1);
    iic_delay();
    IIC_SCL(0);
}

/*
*********************************************************************************************************
*    函 数 名: IIC_NAck
*    功能说明: 不参数应答信号
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void IIC_NAck(void)
{
    IIC_SCL(0);
    iic_sda_out();
    IIC_SDA(1);
    iic_delay();
    IIC_SCL(1);
    iic_delay();
    IIC_SCL(0);
}                                          

/*
*********************************************************************************************************
*    函 数 名: IIC_Send_Byte
*    功能说明: 发送一个字节
*    形    参: txd 字节数据
*    返 回 值: 字节
*********************************************************************************************************
*/
static void IIC_Send_Byte(uint8_t txd)
{                        
    uint8_t t;  

    iic_sda_out();         
    IIC_SCL(0);    // 拉低时钟开始数据传输
    for(t=0;t<8;t++)
    {              
        IIC_SDA((txd&0x80)>>7);
        txd<<=1;       
        iic_delay();   // 对TEA5767这三个延时都是必须的
        IIC_SCL(1);
        iic_delay(); 
        IIC_SCL(0);    
        iic_delay();
    }     
}         
/*
*********************************************************************************************************
*    函 数 名: IIC_Read_Byte
*    功能说明: 读取一个字节
*    形    参: ack 1:发送ACK 0:发送nACK
*    返 回 值: 字节
*********************************************************************************************************
*/
static uint8_t IIC_Read_Byte(unsigned char ack)
{
    unsigned char i,receive=0;
    
    iic_sda_in();//SDA设置为输入
    for(i=0;i<8;i++ )
    {
        IIC_SCL(0); 
            iic_delay();
        receive<<=1;
            if(READ_IIC_SDA)receive++;   
        iic_delay();
        IIC_SCL(1);
        iic_delay();
    }                     
    if (!ack)
        IIC_NAck();//发送nACK
    else
        IIC_Ack(); //发送ACK   

    return receive;
}

static uint8_t m_DeviceIDs[1] = {0x30}; //默认地址
/*
*********************************************************************************************************
*    函 数 名: HAL_IIC_EMU_Read
*    功能说明: 读取数据
*    形    参: Addr 地址 pBuf 数据指针 Len 数据长度
*    返 回 值: 读取成功 1，读取失败 0
*********************************************************************************************************
*/
bool HAL_IIC_EMU_Read(uint8_t Addr,uint8_t *pBuf,uint32_t Len)
{
    bool    Vale = true;
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

/*
*********************************************************************************************************
*    函 数 名: HAL_IIC_EMU_Write
*    功能说明: 写数据
*    形    参: Addr 地址 pBuf 数据指针 Len 数据长度
*    返 回 值: 写入成功 1，写入失败 0
*********************************************************************************************************
*/
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

