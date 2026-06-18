/*
*********************************************************************************************************
*    函 数 名: BL0942电能计量驱动
*    功能说明: 模拟SPI通信
*    形    参: ZHLE
*    返 回 值: 
*********************************************************************************************************
*/

#include "./Driver/inc/BL0942.h"
#include "bsp.h"
#include "./Task/inc/det.h"

/*

实际电压值(V) = [电压有效值寄存器值*Vref*(R8+R9+R10+R11+R12)]/[73978*R7*1000]

电压系数Kv   = [73978*R7*1000]/[Vref*(R25+R26+R35+R36+R37)]
			= [73978*(24.9)*1000]/[1.218*(20000*5)]
			= 1842077100 / 121800
			= 15123.786

实际电流值(A) = [电流有效值寄存器值*Vref] / [305978*Gain_I*(R5)*1000/Rt]

电流系数 Ki  = [305978*(R5)*1000/Rt]   /  [Vref]
						= [305978*(100)*1000/1000] / [1.218]
						= 30597.8/1.218 (A)
						= 25121.346   (mA)

实际有功功率值(W) = [有功功率寄存器值*Vref*Vref*(R8+R9+R10+R11+R12)]/
										[3537*(R5*1000/Rt)*R7*1000]
功率系数Kp = [3537*(R5*1000/Rt)*R7*1000]/[Vref*Vref*(R8+R9+R10+R11+R12)]
           = [3537*(2.2*1000/1000)*24.9*1000]/[1.218*1.218*(20000*5)]  
					 = [3537*2.2*24.9]/[1.218*1.218*(20*5)]
					 = 193756860 / 148352.4
					 = 1306.058 /16
					 = 81.629
*/
#define BL0942_USE_HSPI  // 使用硬件SPI
//#define BL0942_USE_SSPI  // 使用模拟SPI

struct bl0942_data_t {
	uint8_t flag;  			// 采集标志位  19:电压  10-18:电流1-9  1-9: 功率1-9
    uint8_t send_cmd;		// 发送命令
};

static uint8_t  sg_bl0942_buff[16] = {0};
struct bl0942_data_t sg_bl0942data_t = {0};

/* 接口与参数 */
#ifdef BL0942_USE_SSPI
#define BL0942_Config()               bsp_InitSSPI1()
#define BL0942_SEND_STR(buff,len)     SSPI1_Write_Buffer(buff,len)
#define BL0942_ReadByte()         	  SSPI1_Read_Byte()
#else
#define BL0942_Config()               bsp_InitHSPI1()
#define BL0942_SEND_STR(buff,len)     HSPI1_Send_Buffer(buff,len)
#define BL0942_ReadByte(data)         HSPI1_Transmit_Byte(data)
#endif

/* 宏定义数据 */
#define BL0942_DET_NUM   		2  		  // 采集次数 
#define BL0942_SEND_TIME   		100 	  // 发送时间 100ms

/*
*********************************************************************************************************
*    函 数 名: bl0942_init
*    功能说明: bl0942初始化
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0942_init(void)
{
	BL0942_Config();
		
	/* 写命令使能 */
	bl0942_write_enable_function(1);
	bl0942_set_gain_function();  // 设置增益
	/* 写命令失能 */
	bl0942_write_enable_function(0);
	delay_ms(100);
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_write_reg_function
*    功能说明: 写寄存器
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0942_write_reg_function(uint8_t reg,uint8_t *data, uint8_t len)
{
	uint8_t buff[64] = {0};
	uint8_t index = 0;		
	uint8_t checksum = 0;
	
	buff[0] = BL0942_CMD_WRITE;
	buff[1] = reg;
	
	for(index=0; index<len; index++) 
		buff[2+index] = data[index];
	
	/* 计数和校验 */
	for(index=0; index<(2+len); index++)
		checksum+=buff[index];
	
	/* 填充和校验 */
	buff[2+len] = 0xff - checksum;

	/* 数据发送 */
	BL0942_SEND_STR(buff,3+len);
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_read_reg_function
*    功能说明: 寄存器读取命令
*    形    参: 
*    返 回 值: 寄存器值
*	@mode		: 0-更新标志 other-不更新
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t bl0942_read_reg_function(uint8_t reg, uint32_t *data)
{
	uint8_t buff[6] = {0};
	uint8_t checkSum;
	
	buff[0] = BL0942_CMD_READ;	/* 数据 */
	buff[1] = reg;	/* 数据 */

#ifdef BL0942_USE_SSPI
    BL0942_SEND_STR(buff,sizeof(buff));    /* 数据发送 */
    for(uint8_t index=0; index < 4; index++) {
        sg_bl0942_buff[index] = BL0942_ReadByte();
    }
	//校验和计算
	checkSum = (buff[0]+buff[1]+sg_bl0942_buff[0]+sg_bl0942_buff[1]+sg_bl0942_buff[2]) & 0xff;
	checkSum = ~checkSum;

	if(checkSum != sg_bl0942_buff[3])
		return 0;
	
	*data = (sg_bl0942_buff[0]<<16) + (sg_bl0942_buff[1]<<8) + sg_bl0942_buff[2];
	// printf("recv reg = 0x%02x, data = 0x%08x\n",reg,*data);
	return 1;
#else
    for(uint8_t index=0; index < 6; index++) {
        sg_bl0942_buff[index] = BL0942_ReadByte(buff[index]);
    }

	//校验和计算
	checkSum = (buff[0]+buff[1]+sg_bl0942_buff[2]+sg_bl0942_buff[3]) & 0xff;
	checkSum = ~checkSum;

	if(checkSum != sg_bl0942_buff[4])
		return 0;
	
	*data = (sg_bl0942_buff[2]<<16) + (sg_bl0942_buff[3]<<8) + sg_bl0942_buff[4];
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
void bl0942_run_timer_function(void)
{
	static uint16_t  send_time = 0;
		
	if(sg_bl0942data_t.send_cmd == 0) 
	{
		if((++send_time) >= BL0942_SEND_TIME) 
		{
			sg_bl0942data_t.send_cmd = 1;
			sg_bl0942data_t.flag ++;
			if(sg_bl0942data_t.flag >= BL0942_DET_NUM)
				sg_bl0942data_t.flag = 0;
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
void bl0942_work_process_function(void)
{
//	float voltage = 0.0f;
//	float current = 0.0f;
	uint32_t data = 0;
	if(sg_bl0942data_t.send_cmd == 1)
	{
		sg_bl0942data_t.send_cmd = 0;
		switch(sg_bl0942data_t.flag)
		{	
			case 1: 
				if(bl0942_read_reg_function(BL0942_V_RMS,&data))
					det_set_total_energy_bl0942(0,data);
//					voltage = (float)data / BL0942_VOLT_KP;
//					det_set_total_energy_bl0942(0,voltage);

				break;  // 电压
			case 2: 
				if(bl0942_read_reg_function(BL0942_I_RMS,&data))
					det_set_total_energy_bl0942(1,data); 
//					current = (float)data / BL0942_CURR_KP;
//					det_set_total_energy_bl0942(1,current);
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
*    返 回 值: 0-失能 1-使能
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0942_write_enable_function(uint8_t cmd)
{
	uint8_t buff[3] = {0};
	
	if(cmd == 1) 
	{
		buff[0] = 0x55;
		buff[1] = 0x00;
		buff[2] = 0x00;
		bl0942_write_reg_function(BL0942_US_WRPROT_REG,buff,3);
	} 
	else 
	{
		buff[0] = 0x00;
		buff[1] = 0x00;
		buff[2] = 0x00;
		bl0942_write_reg_function(BL0942_US_WRPROT_REG,buff,3);
	}
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_set_gain_function
*    功能说明: 设置增益寄存器
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0942_set_gain_function(void)
{
	uint8_t gain_buff[3] = {0};
	
	gain_buff[0] = BL0942_GAIN_1;  // 电流通道

	bl0942_write_reg_function(BL0942_GAIN_CR,gain_buff,3);
	delay_ms(50);
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_test
*    功能说明: 电压、电流测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0942_test(void)
{
	bl0942_read_reg_function(BL0942_FREQ,0);  // 默认值是0x4E20
	delay_ms(200);	
	bl0942_read_reg_function(BL0942_OT_FUNX,0);  // 默认值是0x24
	delay_ms(200);	
	bl0942_read_reg_function(BL0942_MODE,0);  // 默认值是0x87
	delay_ms(200);
	bl0942_read_reg_function(BL0942_GAIN_CR,0);  // 默认值是0x2
	delay_ms(200);		
	while(1)
	{
		bl0942_work_process_function();	// 数据获取函数
		delay_ms(100);		
	}
}


