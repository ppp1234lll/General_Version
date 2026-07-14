/********************************************************************************
* @File name  : 温湿度模块
* @Description: 模拟IIC通信
* @Author     : ZHLE
*  Version Date        Modification Description
    9、AHT20温湿度传感器：(模拟IIC方式)，引脚分配为：  
            SCL:    PD12
            SDA:    PD11
********************************************************************************/
#include "./Driver/inc/aht20.h"
#include "bsp.h"
#include "appconfig.h"

#if (configUSE_AHT20 == 1)

/* 定义AHT20的从地址为0x70 */
#define ATH20_SLAVE_ADDRESS (0x70)

static const uint8_t crc_table[] =
{
    0x00,0x31,0x62,0x53,0xc4,0xf5,0xa6,0x97,0xb9,0x88,0xdb,0xea,0x7d,0x4c,0x1f,0x2e,
    0x43,0x72,0x21,0x10,0x87,0xb6,0xe5,0xd4,0xfa,0xcb,0x98,0xa9,0x3e,0x0f,0x5c,0x6d,
    0x86,0xb7,0xe4,0xd5,0x42,0x73,0x20,0x11,0x3f,0x0e,0x5d,0x6c,0xfb,0xca,0x99,0xa8,
    0xc5,0xf4,0xa7,0x96,0x01,0x30,0x63,0x52,0x7c,0x4d,0x1e,0x2f,0xb8,0x89,0xda,0xeb,
    0x3d,0x0c,0x5f,0x6e,0xf9,0xc8,0x9b,0xaa,0x84,0xb5,0xe6,0xd7,0x40,0x71,0x22,0x13,
    0x7e,0x4f,0x1c,0x2d,0xba,0x8b,0xd8,0xe9,0xc7,0xf6,0xa5,0x94,0x03,0x32,0x61,0x50,
    0xbb,0x8a,0xd9,0xe8,0x7f,0x4e,0x1d,0x2c,0x02,0x33,0x60,0x51,0xc6,0xf7,0xa4,0x95,
    0xf8,0xc9,0x9a,0xab,0x3c,0x0d,0x5e,0x6f,0x41,0x70,0x23,0x12,0x85,0xb4,0xe7,0xd6,
    0x7a,0x4b,0x18,0x29,0xbe,0x8f,0xdc,0xed,0xc3,0xf2,0xa1,0x90,0x07,0x36,0x65,0x54,
    0x39,0x08,0x5b,0x6a,0xfd,0xcc,0x9f,0xae,0x80,0xb1,0xe2,0xd3,0x44,0x75,0x26,0x17,
    0xfc,0xcd,0x9e,0xaf,0x38,0x09,0x5a,0x6b,0x45,0x74,0x27,0x16,0x81,0xb0,0xe3,0xd2,
    0xbf,0x8e,0xdd,0xec,0x7b,0x4a,0x19,0x28,0x06,0x37,0x64,0x55,0xc2,0xf3,0xa0,0x91,
    0x47,0x76,0x25,0x14,0x83,0xb2,0xe1,0xd0,0xfe,0xcf,0x9c,0xad,0x3a,0x0b,0x58,0x69,
    0x04,0x35,0x66,0x57,0xc0,0xf1,0xa2,0x93,0xbd,0x8c,0xdf,0xee,0x79,0x48,0x1b,0x2a,
    0xc1,0xf0,0xa3,0x92,0x05,0x34,0x67,0x56,0x78,0x49,0x1a,0x2b,0xbc,0x8d,0xde,0xef,
    0x82,0xb3,0xe0,0xd1,0x46,0x77,0x24,0x15,0x3b,0x0a,0x59,0x68,0xff,0xce,0x9d,0xac
};

/*
*********************************************************************************************************
*    函 数 名: cal_crc_table
*    功能说明: CRC校验计算
*    形    参: 
*    返 回 值:  无
*********************************************************************************************************
*/
uint8_t cal_crc_table(uint8_t *ptr, uint8_t len)
{
    uint8_t crc = 0xFF;

    while (len--)
    {
        crc = crc_table[crc ^ *ptr++];
    }
    return (crc);
}

/*
*********************************************************************************************************
*    函 数 名: aht20_i2c_read_bytes
*    功能说明: 读数据
*    形    参: 
*    @addr     : 寄存器地址
*    @buf      : 存储地址
*    @size     : 数据长度
*    返 回 值:  无
*********************************************************************************************************
*/
uint8_t aht20_i2c_read_bytes(uint8_t addr, uint8_t *buf, uint8_t size)
{
    uint8_t ret   = 0;
    uint8_t index = 0;

    siic_start();
    siic_write_byte(addr|0x01);
    ret = siic_wait_ack();
    for(index=0; index<size; index++)
    {
        if(index == (size-1))
        {
            buf[index] = siic_read_byte(0);
        }
        else
        {
            buf[index] = siic_read_byte(1);
        }
    }
    siic_stop();

    return ret;
}

/*
*********************************************************************************************************
*    函 数 名: aht20_i2c_write_bytes
*    功能说明: 写数据
*    形    参: 
*    @addr     : 寄存器地址
*    @buf      : 存储地址
*    @size     : 数据长度
*    返 回 值: 成功/失败
*********************************************************************************************************
*/
uint8_t aht20_i2c_write_bytes(uint8_t addr, uint8_t *buf, uint8_t size)
{
    uint8_t ret   = 0;
    uint8_t index = 0;

    siic_start();
    siic_write_byte(addr|0x00);
    ret = siic_wait_ack();
    for(index=0; index<size; index++)
    {
        siic_write_byte(buf[index]);
        ret = siic_wait_ack();
    }
    siic_stop();

    return ret;
}

/*
*********************************************************************************************************
*    函 数 名: aht20_init
*    功能说明: 初始化
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void aht20_init(void)
{
    uint8_t buff[1] = {0};

    bsp_init_siic();

    /* 激活采集 */
    delay_ms(100);
    aht20_i2c_read_bytes(ATH20_SLAVE_ADDRESS, buff, 1);
}

/*
*********************************************************************************************************
*    函 数 名: aht20_measure
*    功能说明: 测量温度和湿度
*    形    参: 
*    @humidity : 温湿度指针
*    @temperature : 温度指针
*    返 回 值: 成功/失败
*********************************************************************************************************
*/

int8_t aht20_measure(double *humidity, double *temperature)
{
    uint32_t datatemp = 0;
    double data = 0;
    uint8_t  ret      = 0;
    uint8_t  buff[7]  = {0};
    
    buff[0] = 0xAC;
    buff[1] = 0x33;
    buff[2] = 0x00;
    ret = aht20_i2c_write_bytes(ATH20_SLAVE_ADDRESS, buff, 3);    //触发测量
    if(ret != 0)
        return ret;
//    delay_ms(80);
    ret = aht20_i2c_read_bytes(ATH20_SLAVE_ADDRESS, buff, 7);    //读取状态字
    if(ret != 0)
        return ret;
    
    if(buff[6] != cal_crc_table(buff, 6))
        return 1;
    datatemp = buff[1];
    datatemp = (datatemp<<8) | buff[2];
    datatemp = (datatemp<<4) | (buff[3]>>4);
    data = datatemp;
    *humidity = (data / 1048576.0f) * 100.0f;
    
    datatemp = 0;
    datatemp = buff[3] & 0x0F;
    datatemp = (datatemp<<8) | buff[4];
    datatemp = (datatemp<<8) | buff[5];
    data = datatemp;
    *temperature = (data / 1048576.0f) * 200.0f - 50.0f;
    
    return ret;
}

/*
*********************************************************************************************************
*    函 数 名: aht20_test
*    功能说明: 测试温度和湿度
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void aht20_test(void)
{
    double det_temp = 0;
    double det_humi = 0;
    while(1)
    {
        aht20_measure(&det_humi,&det_temp);
        printf("temp=%03f...humi=%03f \n",det_temp,det_humi);
        delay_ms(1000);    
    }
}

#endif


