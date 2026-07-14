/********************************************************************************
* @File name  : BL0910电能计量驱动
* @Description: SPI通信
* @Author     : ZHLE
*  Version Date        Modification Description
********************************************************************************/

#include "./Driver/inc/BL0910.h"
#include "bsp.h"
#include "./Task/inc/det.h"
/*

实际电压值(V) = [电压有效值寄存器值*Vref*(R25+R26+R35+R36+R37)]/[13162*Gain_V*R46*1000]

电压系数Kv  = [13162*Gain_V*R46*1000]/[Vref*(R25+R26+R35+R36+R37)]
(去除1000) = [13162*1*(51+51)*1000]/[1.097*(20+20+20+20+20)]
			= 1342524 / 109.7
			= 12238.14


实际电流值(A) = [电流有效值寄存器值*Vref] / [12875*Gain_I*(R1+R2)*1000/Rt]

电流系数 Ki = [12875*Gain_I*(R1+R2)*1000/Rt] / Vref
						= [12875*1*(51+51)*1000/1000] / 1.097
						= 1197128.53
						= 1197.128 (mA)

实际有功功率值(W) = [有功功率寄存器值*Vref*Vref*(R25+R26+R35+R36+R37)]/
										[40.4125*((R1+R2)*1000/Rt*Gain_I*R46*Gain_V*1000]
功率系数Kp = [40.4125*((R1+R2)*1000/Rt*Gain_I*R46*Gain_V*1000]/[Vref*Vref*(R25+R26+R35+R36+R37)]
           = [40.4125*((51+51)*1000/1000*1*(51+51)*1*1000]/[1.097*1.097*(20+20+20+20+20)]     
					 = 420451.65 / 120.3409
					 = 3493.84
					 
每个电能脉冲对应的电量 = [4194304*0.032768*16]/
												 [3600000*CFDIV*Kp]
功率系数Ke = [4194304*0.032768*16]/[3600000*16*Kp]
           = [4194304*0.032768*16]/[3600000*16*3493.84]  
					 = 0.00001093	 
*/
#define BL0910_USE_HSPI  // 使用硬件SPI
//#define BL0910_USE_SSPI  // 使用模拟SPI

struct bl0910_data_t {
	uint8_t flag;       // 采集标志位  19:电压  10-18:电流1-9  1-9: 功率1-9
    uint8_t send_cmd;		// 发送命令
};

static uint8_t  sg_bl0910_buff[16] = {0};
struct bl0910_data_t sg_bl0910data_t = {0};

/* 接口与参数 */
#ifdef BL0910_USE_SSPI
#define BL0910_Config()               bsp_InitSSPI1()
#define BL0910_SEND_STR(buff,len)     SSPI1_Write_Buffer(buff,len)
#define BL0910_ReadByte()             SSPI1_Read_Byte()
#else
#define BL0910_Config()               bsp_InitHSPI2()
#define BL0910_SEND_STR(buff,len)     HSPI2_Send_Buffer(buff,len)
#define BL0910_ReadByte(data)         HSPI2_Transmit_Byte(data)
#endif

/* 宏定义数据 */
#define BL0910_DET_NUM   		30  		// 采集次数 
#define BL0910_SEND_TIME   		10 	    // 发送时间 10ms

/*
*********************************************************************************************************
*	函 数 名: BL0910_INIT
*	功能说明: bl0910初始化
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_init(void)
{
	BL0910_Config();
		
	/* 写命令使能 */
	bl0910_write_enable_function(1);
	delay_ms(10);	
	bl0910_set_gain_function();  // 设置增益
	delay_ms(10);	
	bl0910_set_ch_function();    // 设置通道
	delay_ms(10);	
	bl0910_set_mode_function();
	bl0910_set_eng_rst_function();
	delay_ms(10);
	/* 写命令失能 */
	bl0910_write_enable_function(0);
	delay_ms(100);
}

/*
*********************************************************************************************************
*	函 数 名: bl0910_write_reg_function
*	功能说明: 写寄存器
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_write_reg_function(uint8_t reg,uint8_t *data, uint8_t len)
{
	uint8_t buff[64] = {0};
	uint8_t index = 0;		
	uint8_t checksum = 0;
	
	buff[0] = BL0910_CMD_WRITE;
	buff[1] = reg;
	
	for(index=0; index<len; index++) 
		buff[2+index] = data[index];
	
	/* 计数和校验 */
	for(index=0; index<(2+len); index++)
		checksum+=buff[index];
	
	/* 填充和校验 */
	buff[2+len] = 0xff - checksum;
	
	/* 数据发送 */
	BL0910_SEND_STR(buff,3+len);
}

/*
*********************************************************************************************************
*	函 数 名: bl0910_read_reg_function
*	功能说明: 寄存器读取命令
*	形    参: 
*	@reg		: 寄存器值
*	@mode		: 0-更新标志 other-不更新
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t bl0910_read_reg_function(uint8_t reg, uint32_t *data)
{
	uint8_t buff[2] = {0};
	uint8_t checkSum;
	
	buff[0] = BL0910_CMD_READ;	/* 数据 */
	buff[1] = reg;	/* 数据 */

#ifdef BL0910_USE_SSPI
    BL0910_SEND_STR(buff,sizeof(buff));    /* 数据发送 */
    for(uint8_t index=0; index < 4; index++) {
        sg_bl0910_buff[index] = BL0910_ReadByte();
    }
	//校验和计算
	checkSum = (buff[0]+buff[1]+sg_bl0910_buff[0]+sg_bl0910_buff[1]+sg_bl0910_buff[2]) & 0xff;
	checkSum = ~checkSum;

	if(checkSum != sg_bl0910_buff[3])
		return 0;
	
	*data = (sg_bl0910_buff[0]<<16) + (sg_bl0910_buff[1]<<8) + sg_bl0910_buff[2];
	// printf("recv reg = 0x%02x, data = 0x%08x\n",reg,*data);
	return 1;
#else
    for(uint8_t index=0; index < 6; index++) {
        sg_bl0910_buff[index] = BL0910_ReadByte(buff[index]);
    }

	//校验和计算
	checkSum = (buff[0]+buff[1]+sg_bl0910_buff[2]+sg_bl0910_buff[3]+sg_bl0910_buff[4]) & 0xff;	
	checkSum = ~checkSum;

	if(checkSum != sg_bl0910_buff[5])
		return 0;
	
	*data = (sg_bl0910_buff[2]<<16) + (sg_bl0910_buff[3]<<8) + sg_bl0910_buff[4];
	// printf("recv reg = 0x%02x, data = 0x%08x\n",reg,*data);
	return 1;
#endif
}

/*
*********************************************************************************************************
*	函 数 名: bl0910_run_timer_function
*	功能说明: 运行计时相关函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_run_timer_function(void)
{
	static uint16_t  send_time = 0;
		
	if(sg_bl0910data_t.send_cmd == 0) 
	{
		if((++send_time) >= BL0910_SEND_TIME) 
		{
			sg_bl0910data_t.send_cmd = 1;
			sg_bl0910data_t.flag ++;
			if(sg_bl0910data_t.flag >= BL0910_DET_NUM)
				sg_bl0910data_t.flag = 0;
		}
	} 
	else 
		send_time = 0;
}

/*
*********************************************************************************************************
*	函 数 名: bl0910_work_process_function
*	功能说明: 工作进程函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_work_process_function(void)
{
//	float voltage = 0.0f;
//	float current = 0.0f;
	uint32_t data = 0;
	if(sg_bl0910data_t.send_cmd == 1)
	{
		sg_bl0910data_t.send_cmd = 0;
		switch(sg_bl0910data_t.flag)
		{	
			case 1: 
				if(bl0910_read_reg_function(BL0910_V_RMS,&data))
					det_set_total_energy_bl0910(0,data);
				break;  // 电压
			case 2: 
				if(bl0910_read_reg_function(BL0910_I1_RMS,&data))
					det_set_total_energy_bl0910(1,data); 
				break; // 电流 1

			case 3:
				if(bl0910_read_reg_function(BL0910_I2_RMS,&data))
					det_set_total_energy_bl0910(2,data);
				break; // 电流 2
			case 4:
				if(bl0910_read_reg_function(BL0910_I3_RMS,&data))
					det_set_total_energy_bl0910(3,data);
				break; // 电流 3
			case 5:
				if(bl0910_read_reg_function(BL0910_I4_RMS,&data))
					det_set_total_energy_bl0910(4,data);
				break; // 电流 4
			case 6:
				if(bl0910_read_reg_function(BL0910_I5_RMS,&data))
					det_set_total_energy_bl0910(5,data);
				break; // 电流 5
			case 7:
				if(bl0910_read_reg_function(BL0910_I6_RMS,&data))
					det_set_total_energy_bl0910(6,data);
				break; // 电流 6
			case 8:
				if(bl0910_read_reg_function(BL0910_I7_RMS,&data))
					det_set_total_energy_bl0910(7,data);
				break; // 电流 7
			case 9:
				if(bl0910_read_reg_function(BL0910_I8_RMS,&data))
					det_set_total_energy_bl0910(8,data);
				break; // 电流 8
			case 10:
				if(bl0910_read_reg_function(BL0910_I9_RMS,&data))
					det_set_total_energy_bl0910(9,data);
				break; // 总电流
			case 11:
				if(bl0910_read_reg_function(BL0910_WATT1_AP,&data))
					det_set_total_energy_bl0910(10,data);
				break; // 功率 1
			case 12:
				if(bl0910_read_reg_function(BL0910_WATT2_AP,&data))
					det_set_total_energy_bl0910(11,data);
				break; // 功率 2
			case 13:
				if(bl0910_read_reg_function(BL0910_WATT3_AP,&data))
					det_set_total_energy_bl0910(12,data);
				break; // 功率 3
			case 14:
				if(bl0910_read_reg_function(BL0910_WATT4_AP,&data))
					det_set_total_energy_bl0910(13,data);
				break; // 功率 4
			case 15:
				if(bl0910_read_reg_function(BL0910_WATT5_AP,&data))
					det_set_total_energy_bl0910(14,data);
				break; // 功率 5
			case 16:
				if(bl0910_read_reg_function(BL0910_WATT6_AP,&data))
					det_set_total_energy_bl0910(15,data);
				break; // 功率 6
			case 17:
				if(bl0910_read_reg_function(BL0910_WATT7_AP,&data))
					det_set_total_energy_bl0910(16,data);
				break; // 功率 7
			case 18:
				if(bl0910_read_reg_function(BL0910_WATT8_AP,&data))
					det_set_total_energy_bl0910(17,data);
				break; // 功率 8
			case 19:
				if(bl0910_read_reg_function(BL0910_WATT9_AP,&data))
					det_set_total_energy_bl0910(18,data);
				break; // 总功率
			case 20:
				if(bl0910_read_reg_function(BL0910_CF1_CNT,&data))
					det_set_total_energy_bl0910(19,data);
				break; // 有功脉冲 1
			case 21:
				if(bl0910_read_reg_function(BL0910_CF2_CNT,&data))
					det_set_total_energy_bl0910(20,data);
				break; // 有功脉冲 2
			case 22:
				if(bl0910_read_reg_function(BL0910_CF3_CNT,&data))
					det_set_total_energy_bl0910(21,data);
				break; // 有功脉冲 3
			case 23:
				if(bl0910_read_reg_function(BL0910_CF4_CNT,&data))
					det_set_total_energy_bl0910(22,data);
				break; // 有功脉冲 4
			case 24:
				if(bl0910_read_reg_function(BL0910_CF5_CNT,&data))
					det_set_total_energy_bl0910(23,data);
				break; // 有功脉冲 5
			case 25:
				if(bl0910_read_reg_function(BL0910_CF6_CNT,&data))
					det_set_total_energy_bl0910(24,data);
				break; // 有功脉冲 6
			case 26:
				if(bl0910_read_reg_function(BL0910_CF7_CNT,&data))
					det_set_total_energy_bl0910(25,data);
				break; // 有功脉冲 7
			case 27:
				if(bl0910_read_reg_function(BL0910_CF8_CNT,&data))
					det_set_total_energy_bl0910(26,data);
				break; // 有功脉冲 8
			case 28:
				if(bl0910_read_reg_function(BL0910_CF9_CNT,&data))
					det_set_total_energy_bl0910(27,data);
				break; // 总有功脉冲
			default: break;			
		}
	}

}

/*
*********************************************************************************************************
*	函 数 名: bl0910_write_enable_function
*	功能说明: 写使能控制函数
*	形    参: 
*	@cmd		: 0-失能 1-使能
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_write_enable_function(uint8_t cmd)
{
	uint8_t buff[3] = {0};
	
	if(cmd == 1) 
	{
		buff[0] = 0x00;
		buff[1] = 0x55;
		buff[2] = 0x55;
		bl0910_write_reg_function(BL0910_US_WRPROT_REG,buff,3);
	} 
	else 
	{
		buff[0] = 0x00;
		buff[1] = 0x00;
		buff[2] = 0x00;
		bl0910_write_reg_function(BL0910_US_WRPROT_REG,buff,3);
	}
}

/*
*********************************************************************************************************
*	函 数 名: bl0910_set_gain_function
*	功能说明: 设置增益寄存器
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_set_gain_function(void)
{
	uint8_t gain1_buff[3] = {0};
	uint8_t gain2_buff[3] = {0};
	
	gain1_buff[0] = BL0910_GAIN_1<<4 | BL0910_GAIN_1;  // 通道1 电压通道
	gain1_buff[1] = BL0910_GAIN_1<<4 | BL0910_GAIN_1;  // 通道3 通道2
	gain1_buff[2] = BL0910_GAIN_1<<4 | BL0910_GAIN_1;  // 通道5 通道4

	gain2_buff[0] = BL0910_GAIN_1<<4 | BL0910_GAIN_1;  // 通道7 通道6
	gain2_buff[1] = BL0910_GAIN_1<<4 | BL0910_GAIN_1;  // 通道9 通道8
	gain2_buff[2] = BL0910_GAIN_1;  						 // 通道10
	
	bl0910_write_reg_function(BL0910_GAIN1_REG,gain1_buff,3);
	delay_ms(50);
	bl0910_write_reg_function(BL0910_GAIN2_REG,gain2_buff,3);

}

/*
*********************************************************************************************************
*	函 数 名: bl0910_set_ch_function
*	功能说明: 设置通道寄存器
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_set_ch_function(void)
{
	uint8_t ch_buff[3] = {0};
	
	ch_buff[0] = 0x00;  // 全部使用
	ch_buff[1] = 0x00;  // 关闭通道10
	ch_buff[2] = 0x00;  // 关闭通道10	
	bl0910_write_reg_function(BL0910_ADC_PD_CTRL,ch_buff,3);
}

/*
*********************************************************************************************************
*	函 数 名: bl0910_set_mode_function
*	功能说明: 设置模式
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_set_mode_function(void)
{
	uint8_t mode_buff[3] = {0};
	
	mode_buff[0] = 0x00;  // 
	mode_buff[1] = 0x02;  // 打开cf
	mode_buff[2] = 0x00;  // 	
	bl0910_write_reg_function(BL0910_MODE3_REG,mode_buff,3);
}

/*
*********************************************************************************************************
*	函 数 名: bl0910_set_eng_rst_function
*	功能说明: 能量读后清零
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_set_eng_rst_function(void)
{
	uint8_t ch_buff[3] = {0};
	
	ch_buff[0] = 0x00;  // 全部使用
	ch_buff[1] = 0x00;  
	ch_buff[2] = 0x00; 
	bl0910_write_reg_function(BL0910_RST_ENG,ch_buff,3);
}

/*
*********************************************************************************************************
*	函 数 名: bl0910_reset_numreg_function
*	功能说明: 复位数字部分的状态机和寄存器
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_reset_numreg_function(void)
{
	uint8_t ch_buff[3] = {0};
	
	ch_buff[0] = 0x5A;  // 全部使用
	ch_buff[1] = 0x5A;  
	ch_buff[2] = 0x5A; 
	bl0910_write_reg_function(BL0910_SOFT_RESET_REG,ch_buff,3);
}

/*
*********************************************************************************************************
*	函 数 名: bl0910_test
*	功能说明: 电压、电流测试
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_test(void)
{
	bl0910_read_reg_function(BL0910_TPS_CTRL,0);  // 默认值是0x07FF
	delay_ms(200);	
	bl0910_read_reg_function(BL0910_SAGLVL_LINECYC,0);  // 默认值是0x100009
	delay_ms(200);	
	bl0910_read_reg_function(BL0910_ADC_PD_CTRL,0);  // 默认值是0x000000
	delay_ms(200);
	bl0910_read_reg_function(BL0910_GAIN1_REG,0);  // 默认值是 0x000000
	delay_ms(200);	
	bl0910_read_reg_function(BL0910_GAIN2_REG,0);  // 默认值是 0x000000
	delay_ms(200);	
	bl0910_read_reg_function(BL0910_RST_ENG,0);  // 默认值是0x100009
	delay_ms(200);		
	bl0910_read_reg_function(BL0910_MODE3_REG,0);  // 默认值是0x100009
	delay_ms(200);
	while(1)
	{
//		bl0910_read_reg_function(0x94,0);
//		bl0910_analysis_data_function();
		bl0910_work_process_function();	// 数据获取函数
		delay_ms(20);		
	}

}








