/********************************************************************************
* @File name  : BL0972电能计量驱动
* @Description: SPI通信
* @Author     : ZHLE
*  Version Date        Modification Description
********************************************************************************/
#include "./Driver/inc/BL0972.h"
#include "bsp.h"
#include "./Task/inc/det.h"

/*									 
实际电压值(V) = [电压有效值寄存器值*Vref*(R11+R12+R13+R14+R15)]/[79931*R17*1000]

电压系数Kv  = [79931*R17*1000]/[Vref*(R11+R12+R13+R14+R15)]
(K 欧)     = [79931*0.0249*1000]/[1.218*(20+20+20+20+20)]
					 = 1990281.9 / 121.8
					 = 16340.534


实际电流值(A) = [电流有效值寄存器值*Vref] / [324004*(R5*1000)/Rt]
               Rt=1000

电流系数 Ki = [324004*R5*1000/Rt] / Vref
						= [324004*2.2*1000/1000] / 1.218
						= 585228.900
						= 585.229 (mA)

实际有功功率值(W) = [有功功率寄存器值*Vref*Vref*(R11+R12+R13+R14+R15)]/
										[4046*(R5*1000/Rt*R17*1000]
功率系数Kp = [4046*(R5*1000/Rt*R17*1000]/[Vref*Vref*(R25+R26+R35+R36+R37)]
           = [4046*(2.2*1000/1000*24.9*1000]/[1.218*1.218*(20+20+20+20+20)]  
					 = [4046*(2.2*0.0249*1000]/[1.218*1.218*(20+20+20+20+20)]  
					 = 221639.88 / 148.3524
					 = 1494.009
					 
每个电能脉冲对应的电量 = [1638.4*256*Vref*Vref*(R11+R12+R13+R14+R15)]/
												[3600000*4046*(R5*1000/Rt*R17*1000]
功率系数Ke = [1638.4*256*Vref*Vref*(R11+R12+R13+R14+R15)]/[3600000*4046*(R5*1000/Rt*R17*1000]
           = [1638.4*256*1.218*1.218*(20*5)]/[3600000*4046*(2.2*1000/1000*0.0249*1000]
           = [1638.4*256*1.218*1.218*100]/[3600000*4046*(2.2*24.9)]
           = 62223506.47296	/	797903568000
					 = 0.000078
*/
#define BL0972_USE_HSPI  // 使用硬件SPI
//#define BL0972_USE_SSPI  // 使用模拟SPI

struct bl0972_data_t {
	uint8_t flag;  			// 采集标志位  19:电压  10-18:电流1-9  1-9: 功率1-9
    uint8_t send_cmd;		// 发送命令
};

static uint8_t  sg_bl0972_buff[8] = {0};
struct bl0972_data_t sg_bl0972data_t = {0};

/* 接口与参数 */
#ifdef BL0972_USE_SSPI
#define BL0972_Config()               bsp_InitSSPI1()
#define BL0972_SEND_STR(buff,len)     SSPI1_Write_Buffer(buff,len)
#define BL0972_ReadByte()             SSPI1_Read_Byte()
#else
#define BL0972_Config()               bsp_InitHSPI1()
#define BL0972_SEND_STR(buff,len)     HSPI1_Send_Buffer(buff,len)
#define BL0972_ReadByte(data)         HSPI1_Transmit_Byte(data)
#endif

/* 宏定义数据 */
#define bl0972_DET_NUM   			4  		  // 采集次数 
#define bl0972_SEND_TIME   		100 	  // 发送时间 100ms

/*
*********************************************************************************************************
*	函 数 名: bl0972_init
*	功能说明: 初始化
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0972_init(void)
{
	BL0972_Config();
	
	/* 写命令使能 */
	bl0972_write_enable_function(1);
	bl0972_reset_numreg_function();
	bl0972_set_mode1_function();
	
	bl0972_set_mode2_function();
	bl0972_set_gain_function();
	bl0972_set_gonghao_function();
	/* 写命令失能 */
	bl0972_write_enable_function(0);
}

/*
*********************************************************************************************************
*	函 数 名: bl0972_write_reg_function
*	功能说明: 写寄存器
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0972_write_reg_function(uint8_t reg,uint8_t *data, uint8_t len)
{
	uint8_t buff[64] = {0};
	uint8_t index = 0;		
	uint8_t checksum = 0;
	
	buff[0] = BL0972_CMD_WRITE;
	buff[1] = reg;
	
	for(index=0; index<len; index++) 
		buff[2+index] = data[index];
	
	/* 计数和校验 */
	for(index=0; index<(2+len); index++)
		checksum+=buff[index];
	
	/* 填充和校验 */
	buff[2+len] = 0xff - checksum;
	
	/* 数据发送 */
	BL0972_SEND_STR(buff,3+len);
}

/*
*********************************************************************************************************
*	函 数 名: bl0972_read_reg_function
*	功能说明: 寄存器读取命令
*	形    参: 
*	@reg		: 寄存器值
*	@mode		: 0-更新标志 other-不更新
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t bl0972_read_reg_function(uint8_t reg, uint32_t *data)
{
	uint8_t buff[2] = {0};
	uint8_t checkSum;

	buff[0] = BL0972_CMD_READ;	/* 读取 */
	buff[1] = reg;	/* 数据 */

#ifdef BL0972_USE_SSPI
    BL0972_SEND_STR(buff,sizeof(buff));    /* 数据发送 */
    for(uint8_t index=0; index < 4; index++) {
        sg_bl0972_buff[index] = BL0972_ReadByte();
    }
	//校验和计算
	checkSum = (buff[0]+buff[1]+sg_bl0972_buff[0]+sg_bl0972_buff[1]+sg_bl0972_buff[2]) & 0xff;
	checkSum = ~checkSum;

	if(checkSum != sg_bl0972_buff[3])
		return 0;
	
	*data = (sg_bl0972_buff[0]<<16) + (sg_bl0972_buff[1]<<8) + sg_bl0972_buff[2];
	// printf("recv reg = 0x%02x, data = 0x%08x\n",reg,*data);
	return 1;
#else
    for(uint8_t index=0; index < 6; index++) {
        sg_bl0972_buff[index] = BL0972_ReadByte(buff[index]);
    }

	//校验和计算
	checkSum = (buff[0]+buff[1]+sg_bl0972_buff[2]+sg_bl0972_buff[3]+sg_bl0972_buff[4]) & 0xff;
	checkSum = ~checkSum;

	if(checkSum != sg_bl0972_buff[5])
		return 0;
	
	*data = (sg_bl0972_buff[2]<<16) + (sg_bl0972_buff[3]<<8) + sg_bl0972_buff[4];
	// printf("recv reg = 0x%02x, data = 0x%08x\n",reg,*data);
	return 1;
#endif
}

/*
*********************************************************************************************************
*	函 数 名: bl0972_run_timer_function
*	功能说明: 运行计时相关函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0972_run_timer_function(void)
{
	static uint16_t  send_time = 0;
	
	if(sg_bl0972data_t.send_cmd == 0) 
	{
		if((++send_time) >= bl0972_SEND_TIME) 
		{
			sg_bl0972data_t.send_cmd = 1;
			sg_bl0972data_t.flag ++;
			if(sg_bl0972data_t.flag >= bl0972_DET_NUM)
				sg_bl0972data_t.flag = 0;
		}
	} 
	else 
		send_time = 0;

}

/*
*********************************************************************************************************
*	函 数 名: bl0972_work_process_function
*	功能说明: 工作进程函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0972_work_process_function(void)
{
//	float voltage = 0.0f;
//	float current = 0.0f;
//	uint32_t data = 0;
	if(sg_bl0972data_t.send_cmd == 1)
	{
		sg_bl0972data_t.send_cmd = 0;
		switch(sg_bl0972data_t.flag)
		{	

			default: break;			
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: bl0972_write_enable_function
*	功能说明: 写使能控制函数
*	形    参: 
*	@cmd		: 0-失能 1-使能
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0972_write_enable_function(uint8_t cmd)
{
	uint8_t buff[3] = {0};
	
	if(cmd == 1) 
	{
		buff[0] = 0x00;
		buff[1] = 0x55;
		buff[2] = 0x55;
		bl0972_write_reg_function(BL0972_USR_WRPROT,buff,3);
	} 
	else 
	{
		buff[0] = 0x00;
		buff[1] = 0x00;
		buff[2] = 0x00;
		bl0972_write_reg_function(BL0972_USR_WRPROT,buff,3);
	}
}

/*
*********************************************************************************************************
*	函 数 名: bl0972_set_mode1_function
*	功能说明: 设置模式1
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0972_set_mode1_function(void)
{
	uint8_t mode1_buff[3] = {0};
	
	mode1_buff[0] = 0x00;  // 
	mode1_buff[1] = 0x07;  // 打开cf
	mode1_buff[2] = 0xFF;  // 	
	bl0972_write_reg_function(BL0972_MODE1,mode1_buff,3);
}

/*
*********************************************************************************************************
*	函 数 名: bl0972_set_mode2_function
*	功能说明: 设置模式2
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0972_set_mode2_function(void)
{
	uint8_t mode2_buff[3] = {0};
	
	mode2_buff[0] = 0x2A;  // 
	mode2_buff[1] = 0xAA;  // 打开cf
	mode2_buff[2] = 0xAA;  // 	
	bl0972_write_reg_function(BL0972_MODE2,mode2_buff,3);
}

/*
*********************************************************************************************************
*	函 数 名: bl0972_set_gain_function
*	功能说明: 设置增益寄存器
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0972_set_gain_function(void)
{
	uint8_t gain_buff[3] = {0};
	
	gain_buff[0] = 0x30;  // 电流通道
	gain_buff[1] = 0x00;
	gain_buff[2] = 0x03;

	bl0972_write_reg_function(BL0972_GAIN,gain_buff,3);
	delay_ms(50);
}

/*
*********************************************************************************************************
*	函 数 名: bl0972_set_gonghao_function
*	功能说明: 设置功耗寄存器
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0972_set_gonghao_function(void)
{
	uint8_t gh_buff[3] = {0};
	
	gh_buff[0] = 0x00;  // 电流通道
	gh_buff[1] = 0x07;
	gh_buff[2] = 0xDE;

	bl0972_write_reg_function(BL0972_GH,gh_buff,3);
//	delay_ms(50);
}

/*
*********************************************************************************************************
*	函 数 名: bl0972_reset_numreg_function
*	功能说明: 用户区寄存器复位
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0972_reset_numreg_function(void)
{
	uint8_t ch_buff[3] = {0};
	
	ch_buff[0] = 0x5A;  // 全部使用
	ch_buff[1] = 0x5A;  
	ch_buff[2] = 0x5A; 
	bl0972_write_reg_function(BL0972_SOFT_RESET,ch_buff,3);
}

/*
*********************************************************************************************************
*	函 数 名: bl0972_test
*	功能说明: 电压、电流测试
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0972_test(void)
{
	bl0972_read_reg_function(BL0972_PHASE_I,0);  // 0x1010
	delay_ms(200);
	bl0972_read_reg_function(BL0972_VAR_CREEP,0);  // 0x04C04C
	delay_ms(200);	
	bl0972_read_reg_function(BL0972_RMS_CREEP,0);  // 0x200
	delay_ms(200);	
	bl0972_read_reg_function(BL0972_SAGLVL_LINECYC,0);  // 0x100009
	delay_ms(200);	
	while(1)
	{
		bl0972_work_process_function();	// 数据获取函数
		delay_ms(100);	
	}
}






