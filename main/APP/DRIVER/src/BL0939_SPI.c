/********************************************************************************
* @File name  : BL0939电能计量驱动
* @Description: SPI通信
* @Author     : ZHLE
*  Version Date        Modification Description
********************************************************************************/

#include "./Driver/inc/BL0939.h"
#include "bsp.h"
#include "./Task/inc/det.h"
#include <stdint.h>

/*
实际电压值(V) = [电压有效值寄存器值*Vref*(R11+R10+R13+R14+R16+R9)]/[79931*R17*1000]

电压系数Kv   = [79931*R17*1000]/[Vref*(R11+R10+R13+R14+R16)]
(K 欧)      = [79931*0.0249*1000]/[1.218*(20+20+20+20+20)]
            = 1990281.9 / 121.8
            = 16340.574
    
实际电流值(A) = [电流有效值寄存器值*Vref] / [324004*(R5*1000)/Rt]
    Rt=1000

电流系数 Ki = [324004*R5*1000/Rt] / Vref
            = [324004*75*1000/1000] / 1.218
            = 19950985.221
            = 19950.985 (mA)

实际有功功率值(W) = [有功功率寄存器值*Vref*Vref*(R11+R10+R13+R14+R16+R9)]/
                                        [4046*(R5*1000/Rt*R17*1000]
功率系数Kp   = [4046*(R5*1000/Rt*R17*1000]/[Vref*Vref*(R11+R10+R13+R14+R16+R9)]
            = [4046*(75*1000/1000*0.0249*1000]/[1.218*1.218*(20+20+20+20+20+20)]  
            = [4046*(75*0.0249*1000]/[1.218*1.218*(20+20+20+20+20+20)]  
            = 7555905 / 178.023
            = 42443.42
                        
每个电能脉冲对应的电量 = [1638.4*256*Vref*Vref*(R11+R10+R13+R14+R16+R9)]/
                                                [3600000*4046*(R5*1000/Rt*R17*1000]
功率系数Ke   = [1638.4*256*Vref*Vref*(R11+R10+R13+R14+R16)]/[3600000*4046*(R5*1000/Rt*R17*1000]
            = [1638.4*256*1.218*1.218*(20*5)]/[3600000*4046*(75*1000/1000*0.0249*1000]
            = [1638.4*256*1.218*1.218*100]/[3600000*4046*(75*24.9)]
            = 62223506.47296    /    27201258000000
            = 0.00000228
*/
#define BL0939_USE_HSPI  // 使用硬件SPI
// #define BL0939_USE_SSPI  // 使用模拟SPI

struct bl0939_data_t {
    uint8_t flag;              // 采集标志位
    uint8_t send_cmd;		// 发送命令
};

static uint8_t  sg_bl0939_buff[16] = {0};
struct bl0939_data_t sg_bl0939data_t = {0};

/* 接口与参数 */
#ifdef BL0939_USE_SSPI
#define BL0939_Config()               bsp_InitSSPI1()
#define BL0939_SEND_STR(buff,len)     SSPI1_Write_Buffer(buff,len)
#define BL0939_ReadByte()             SSPI1_Read_Byte()
#else
#define BL0939_Config()               bsp_InitHSPI1()
#define BL0939_SEND_STR(buff,len)     HSPI1_Send_Buffer(buff,len)
#define BL0939_ReadByte(data)         HSPI1_Transmit_Byte(data)
#endif

/* 宏定义数据 */
#define BL0939_DET_NUM           5          // 采集次数 
#define BL0939_SEND_TIME         100        // 发送时间 100ms

/*
*********************************************************************************************************
*    函 数 名: bl0939_init
*    功能说明: 初始化
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0939_init(void)
{
    BL0939_Config();
    /* 写命令使能 */
    bl0939_write_enable_function(1);
    // bl0939_reset_numreg_function();
    // bl0939_set_mode_function();
    
    bl0939_set_WA_CREEP_function();
    /* 写命令失能 */
    bl0939_write_enable_function(0);
}

/*
*********************************************************************************************************

*    函 数 名: bl0942_write_reg_function
*    功能说明: 写寄存器
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0939_write_reg_function(uint8_t reg,uint8_t *data, uint8_t len)
{
    uint8_t buff[64] = {0};
    uint8_t index = 0;        
	uint8_t checksum = 0;
    
    buff[0] = BL0939_CMD_WRITE;
    buff[1] = reg;
    
    for(index=0; index<len; index++) 
        buff[2+index] = data[index];
    
    /* 计数和校验 */
    for(index=0; index<(2+len); index++)
        checksum+=buff[index];
    
    /* 填充和校验 */
	buff[2+len] = 0xff - checksum;

    /* 数据发送 */
    BL0939_SEND_STR(buff,3+len);
}
/*
*********************************************************************************************************
*    函 数 名: bl0942_read_reg_function
*    功能说明: 寄存器读取命令
*    形    参: 
*    @reg        : 寄存器值
*    @mode        : 0-更新标志 other-不更新
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t bl0939_read_reg_function(uint8_t reg, uint32_t *data)
{
    uint8_t buff[6] = {0};
	uint8_t checkSum;
    
    buff[0] = BL0939_CMD_READ;    /* 读取 */
    buff[1] = reg;    /* 数据 */

#ifdef BL0939_USE_SSPI
    BL0939_SEND_STR(buff,sizeof(buff));    /* 数据发送 */
    for(uint8_t index=0; index < 4; index++) {
        sg_bl0939_buff[index] = BL0939_ReadByte();
    }
	//校验和计算
	checkSum = (buff[0]+buff[1]+sg_bl0939_buff[0]+sg_bl0939_buff[1]+sg_bl0939_buff[2]) & 0xff;
	checkSum = ~checkSum;

	if(checkSum != sg_bl0939_buff[3])
		return 0;
	
	*data = (sg_bl0939_buff[0]<<16) + (sg_bl0939_buff[1]<<8) + sg_bl0939_buff[2];
	// printf("recv reg = 0x%02x, data = 0x%08x\n",reg,*data);
	return 1;
#else
    for(uint8_t index=0; index < 6; index++) {
        sg_bl0939_buff[index] = BL0939_ReadByte(buff[index]);
    }

	//校验和计算
	checkSum = (buff[0]+buff[1]+sg_bl0939_buff[2]+sg_bl0939_buff[3]+sg_bl0939_buff[4]) & 0xff;
	checkSum = ~checkSum;

	if(checkSum != sg_bl0939_buff[5])
		return 0;
	
	*data = (sg_bl0939_buff[2]<<16) + (sg_bl0939_buff[3]<<8) + sg_bl0939_buff[4];
	// printf("recv reg = 0x%02x, data = 0x%08x\n",reg,*data);
	return 1;
#endif
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_run_timer_function
*    功能说明: 运行计时相关函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0939_run_timer_function(void)
{
	static uint16_t  send_time = 0;
	
	if(sg_bl0939data_t.send_cmd == 0) 
	{
		if((++send_time) >= BL0939_SEND_TIME) 
		{
			sg_bl0939data_t.send_cmd = 1;
			sg_bl0939data_t.flag ++;
			if(sg_bl0939data_t.flag >= BL0939_DET_NUM)
				sg_bl0939data_t.flag = 0;
		}
	} 
	else 
		send_time = 0;
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_work_process_function
*    功能说明: 工作进程函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0939_work_process_function(void)
{
//	float voltage = 0.0f;
//	float current = 0.0f;
	uint32_t data = 0;
	if(sg_bl0939data_t.send_cmd == 1)
	{
		sg_bl0939data_t.send_cmd = 0;
		switch(sg_bl0939data_t.flag)
		{	
			case 1: 
				if(bl0939_read_reg_function(BL0939_V_RMS,&data))
					det_set_total_energy_bl0939(0,data);
//					voltage = (float)data / BL0939_VOLT_KP;
//					det_set_total_energy_bl0939(0,voltage);

				break;  // 电压
			case 2: 
				if(bl0939_read_reg_function(BL0939_IA_RMS,&data))
					det_set_total_energy_bl0939(1,data); 
//					current = (float)data / BL0939_CURR_KP;
//					det_set_total_energy_bl0939(1,current);
				break; // 电流 A
			default: break;			
		}
	}
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_write_enable_function
*    功能说明: 写使能控制函数
*    形    参: 
*    @cmd        : 0-失能 1-使能
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0939_write_enable_function(uint8_t cmd)
{
    uint8_t buff[3] = {0};
    
    if(cmd == 1) 
    {
        buff[0] = 0x00;
        buff[1] = 0x00;
        buff[2] = 0x55;
        bl0939_write_reg_function(BL0939_USR_WRPROT,buff,3);
    } 
    else 
    {
        buff[0] = 0x00;
        buff[1] = 0x00;
        buff[2] = 0x00;
        bl0939_write_reg_function(BL0939_USR_WRPROT,buff,3);
    }
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_set_mode_function
*    功能说明: 设置模式
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0939_set_mode_function(void)
{
    uint8_t mode_buff[3] = {0};
    
    mode_buff[0] = 0x00;  // 
    mode_buff[1] = 0x01;  // 打开cf
    mode_buff[2] = 0x00;  //     
    bl0939_write_reg_function(BL0939_MODE,mode_buff,3);
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_set_WA_CREEP_function
*    功能说明: 关闭防潜动
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0939_set_WA_CREEP_function(void)
{
    uint8_t mode_buff[3] = {0};
    
    mode_buff[0] = 0x00;  // 
    mode_buff[1] = 0x00;  //  
    mode_buff[2] = 0x00;  //     
    bl0939_write_reg_function(BL0939_WA_CREEP,mode_buff,3);
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_reset_numreg_function
*    功能说明: 用户区寄存器复位
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0939_reset_numreg_function(void)
{
    uint8_t ch_buff[3] = {0};
    
    ch_buff[0] = 0x5A;  // 全部使用
    ch_buff[1] = 0x5A;  
    ch_buff[2] = 0x5A; 
    bl0939_write_reg_function(BL0939_SOFT_RESET,ch_buff,3);
}
/*
*********************************************************************************************************
*    函 数 名: bl0939_test
*    功能说明: 电压、电流测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0939_test(void)
{ 
    bl0939_read_reg_function(BL0939_TPS_CTRL,0);  // 默认值是0x07FF
    delay_ms(200);    
    bl0939_read_reg_function(BL0939_WA_CREEP,0);  // 默认值是0x0B
    delay_ms(200);    
    bl0939_read_reg_function(BL0939_IA_RMSOS,0);  // 默认值是0x0B
    delay_ms(200);    
    bl0939_read_reg_function(BL0939_IB_RMSOS,0);  // 默认值是0x0B
    delay_ms(200);    
    while(1)
    {
        bl0939_work_process_function();    // 数据获取函数
//        bl0939_read_reg_function(BL0939_TPS_CTRL,0);
        
        delay_ms(20);        
    }
}






