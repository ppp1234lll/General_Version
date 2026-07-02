/********************************************************************************
* @File name  : 温湿度模块
* @Description: 模拟IIC通信
* @Author     : ZHLE
*  Version Date        Modification Description
    9、模拟IIC通信：引脚分配为：  
                SCL:    PD12
                SDA:    PD11

********************************************************************************/

#include "bsp_siic.h"
#include "./SYSTEM/delay/delay.h"
#include "bsp_core_dwt.h"

#define SIIC_SCL_GPIO_CLK        RCU_GPIOD
#define SIIC_SCL_GPIO            GPIOD
#define SIIC_SCL_PIN             GPIO_PIN_12

#define SIIC_SDA_GPIO_CLK        RCU_GPIOD
#define SIIC_SDA_GPIO            GPIOD
#define SIIC_SDA_PIN             GPIO_PIN_11

#define SIIC_SCL(x)     (x ? gpio_bit_set(SIIC_SCL_GPIO,SIIC_SCL_PIN) : gpio_bit_reset(SIIC_SCL_GPIO,SIIC_SCL_PIN))
#define SIIC_SDA(x)     (x ? gpio_bit_set(SIIC_SDA_GPIO,SIIC_SDA_PIN) : gpio_bit_reset(SIIC_SDA_GPIO,SIIC_SDA_PIN))

#define READ_SIIC_SDA   gpio_input_bit_get(SIIC_SDA_GPIO,SIIC_SDA_PIN)

/*
*********************************************************************************************************
*    函 数 名: siic_drive_sda_in
*    功能说明: 设置SDA为输入
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void siic_drive_sda_in(void)
{
    gpio_mode_set(SIIC_SDA_GPIO, GPIO_MODE_INPUT, GPIO_PUPD_NONE, SIIC_SDA_PIN);  
}
/*
*********************************************************************************************************
*    函 数 名: siic_drive_sda_out
*    功能说明: 设置SDA为输出
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void siic_drive_sda_out(void)
{
    gpio_mode_set(SIIC_SDA_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SIIC_SDA_PIN); 
    gpio_output_options_set(SIIC_SDA_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SIIC_SDA_PIN);
}
/*
*********************************************************************************************************
*    函 数 名: siic_delay
*    功能说明: 延时函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void siic_delay(void)
{
    dwt_delay_us(4);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_init_siic
*    功能说明: 初始化IIC的IO口
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_init_siic(void)
{
    /* enable GPIO clock */
    rcu_periph_clock_enable(SIIC_SCL_GPIO_CLK);
    rcu_periph_clock_enable(SIIC_SDA_GPIO_CLK);

    /* configure IIC_SCL as alternate function push-pull */
    gpio_mode_set(SIIC_SCL_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SIIC_SCL_PIN);
    gpio_output_options_set(SIIC_SCL_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SIIC_SCL_PIN);

    /* configure IIC_SDA as alternate function push-pull */
    gpio_mode_set(SIIC_SDA_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SIIC_SDA_PIN);
    gpio_output_options_set(SIIC_SDA_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SIIC_SDA_PIN);
    SIIC_SCL(1);
    SIIC_SDA(1);
}

/*
*********************************************************************************************************
*    函 数 名: siic_start
*    功能说明: 开始信号
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void siic_start(void)
{
    siic_drive_sda_out();
    SIIC_SDA(1);
    SIIC_SCL(1);
    siic_delay();
    SIIC_SDA(0);
    siic_delay();
    SIIC_SCL(0);
}

/*
*********************************************************************************************************
*    函 数 名: siic_stop
*    功能说明: 结束信号
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void siic_stop(void)
{
    siic_drive_sda_out();
    SIIC_SCL(0);
    SIIC_SDA(0);
    siic_delay();
    SIIC_SCL(1);
    siic_delay();
    SIIC_SDA(1);
}

/*
*********************************************************************************************************
*    函 数 名: siic_wait_ack
*    功能说明: 等待应答
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t siic_wait_ack(void)
{
    uint8_t ucErrTime=0;
    
    siic_drive_sda_in();
    SIIC_SDA(1);
    siic_delay();       
    SIIC_SCL(1);
    siic_delay();
    
    while(READ_SIIC_SDA)
    {
        ucErrTime++;
        if(ucErrTime > 250)
        {
            siic_stop();
            return 1;
        }
    }
    SIIC_SCL(0);//时钟输出0
    return 0;
} 

/*
*********************************************************************************************************
*    函 数 名: siic_ack
*    功能说明: 应答信号
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void siic_ack(void)
{
    SIIC_SCL(0);
    siic_drive_sda_out();
    SIIC_SDA(0);
    siic_delay();
    SIIC_SCL(1);
    siic_delay();
    SIIC_SCL(0);
}

/*
*********************************************************************************************************
*    函 数 名: siic_nack
*    功能说明: 无应答信号
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void siic_nack(void)
{
    SIIC_SCL(0);
    siic_drive_sda_out();
    SIIC_SDA(1);
    siic_delay();
    SIIC_SCL(1);
    siic_delay();
    SIIC_SCL(0);
}                                          

/*
*********************************************************************************************************
*    函 数 名: siic_write_byte
*    功能说明: 写字节
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void siic_write_byte(uint8_t dat)
{                        
    uint8_t t;   

    siic_drive_sda_out();
    SIIC_SCL(0);//拉低时钟开始数据传输
    
    for(t=0;t<8;t++)
    {              
        if(dat&0x80)
        {
            SIIC_SDA(1);
        }
        else
        {
            SIIC_SDA(0);
        }
        dat<<=1;       
        siic_delay(); 
        SIIC_SCL(1);
        siic_delay();
        SIIC_SCL(0);
        siic_delay(); 
    }
} 

/*
*********************************************************************************************************
*    函 数 名: siic_read_byte
*    功能说明: 读字节
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t siic_read_byte(uint8_t ack)
{
    uint8_t i = 0;
    uint8_t receive = 0;
    siic_drive_sda_in();

    for(i=0;i<8;i++ )
    {
        SIIC_SCL(0);
        siic_delay();
        SIIC_SCL(1);
        receive <<= 1;
        if(READ_SIIC_SDA)
        {
            receive++;
        }
        siic_delay();
    }   

    if(ack == 0)
        siic_nack();//发送nACK
    else if(ack == 1)
        siic_ack(); //发送ACK

    return receive;
}

