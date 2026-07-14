/*
*********************************************************************************************************
*    函 数 名: 电能计量驱动
*    功能说明: UART通信
*    形    参: ZHLE
*    返 回 值: 串口1，波特率4800，数据必须是3位字节
*********************************************************************************************************
*/
#include "bsp.h"
#include "det.h"
#include "BL0910_UART.h"
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


struct bl0910_data_t {
	uint8_t auto_cmd;		// 自动采集功能
	uint8_t checksum;		// 和校验 - 主要是读使用
	uint8_t repeat;			// 重复计数
	uint8_t overtime;		// 超时倒计时 在等于1的状态下，计数清零，标识超时，result使用2
	uint8_t result;			// 结果：0-等待 1-获取到 2-超时
	uint8_t send; 			// 1-有发送数据
	uint8_t reg;				// 本次操作类型-寄存器地址			
	uint8_t mode;				// 0-读 0x80-写
	uint8_t flag;  			// 采集标志位  19:电压  10-18:电流1-9  1-9: 功率1-9
};

static uint16_t sg_bl0910_rec_sta = 0;
static uint8_t  sg_bl0910_buff[ELEC_RX_MAX] = {0};
struct bl0910_data_t sg_bl0910data_t = {0};

/* 接口与参数 */
#define BL0910_BAUDRATE (38400)  // 根据管脚进行调整
#define BL0910_USART_INIT(baudrate) elec_uart_init(baudrate)
#define BL0910_SEND_STR(buff,len) 	elec_send_str_function(buff,len)

/* 宏定义数据 */
#define BL0910_DET_NUM   			30  		// 采集次数 
#define BL0910_TIME_OUT  			20 		// 超时时间 20ms
#define BL0910_AUTO_TIME   		4000 	  // 2s (采集18次，每次100ms)
#define BL0910_SEND_TIME   		10 	    // 发送时间 10ms
/* 数据 */
#define BL0910_REC_STA  sg_bl0910_rec_sta
#define BL0910_REC_BUFF sg_bl0910_buff

/*
*********************************************************************************************************
*    函 数 名: bl0910_reset_function
*    功能说明: 复位函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_reset_function(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(ELEC_RX_GPIO_CLK,ENABLE);

	GPIO_InitStructure.GPIO_Pin   = ELEC_RX_PIN;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;   	// 上拉
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(ELEC_RX_GPIO_PORT,&GPIO_InitStructure);

	GPIO_WriteBit(ELEC_RX_GPIO_PORT,ELEC_RX_PIN,Bit_RESET);
	delay_ms(10);
	GPIO_WriteBit(ELEC_RX_GPIO_PORT,ELEC_RX_PIN,Bit_SET);
	delay_ms(10);
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_init_function
*    功能说明: bl0910初始化
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910c_init_function(void)
{
	bl0910_reset_function();
	BL0910_USART_INIT(BL0910_BAUDRATE);
	
	/* 开启自动采集功能-电压、电流 */
	sg_bl0910data_t.auto_cmd = 1;
		
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
*    函 数 名: bl0910_sending_data_function
*    功能说明: 发送数据函数
*    形    参: 
*    返 回 值: 寄存器值
*	@mode		: 发送模式
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_sending_data_function(uint8_t reg, uint8_t mode)
{
	sg_bl0910data_t.overtime 	= 1;  // 超时倒计时
	sg_bl0910data_t.result  	= 0;   
	sg_bl0910data_t.mode    	= mode;
	sg_bl0910data_t.send    	= 1;
	sg_bl0910data_t.reg     	= reg;
	sg_bl0910data_t.repeat  	= 0;
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_send_over_function
*    功能说明: 数据发送操作完成
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_send_over_function(void)
{
	sg_bl0910data_t.overtime = 0;
	sg_bl0910data_t.result  = 0;
	sg_bl0910data_t.send    = 0;
	sg_bl0910data_t.repeat  = 0;
}

/*
*********************************************************************************************************
*    函 数 名: Complement_2_Original
*    功能说明: 补码转换为原码
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint32_t Complement_2_Original(uint32_t data)
{
	uint32_t temp;
	if((data&0x00800000) == 0x00800000)  // 判断最高位是否为0，Bit[23]为符号位，Bit[23]=0为正
	{
		data &= 0x007FFFFF;  // 清除符号位 	
		temp =~data;         // 反码
		data = temp & 0x007FFFFF;  // 清除左边多余位
		data += 1;				
	}
	else  // 当前为负功
	{
		data &= 0x007FFFFF;  // 清除符号位
	}
	return data;
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_deal_read_data_function
*    功能说明: 处理读取到的数据
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
int8_t bl0910_deal_read_data_function(void)
{
	uint32_t data  = 0;
	int8_t   ret   = 0;
	uint8_t  index = 0;
	
	for(index=0; index<3; index++) 
		sg_bl0910data_t.checksum += BL0910_REC_BUFF[index];

	if(BL0910_REC_BUFF[3] != (0xff - sg_bl0910data_t.checksum)) 
	{
		ret = -1;
	}
	data = BL0910_REC_BUFF[2];
	data = (data<<8) | BL0910_REC_BUFF[1];
	data = (data<<8) | BL0910_REC_BUFF[0];

	switch(sg_bl0910data_t.reg) 
	{
		case V_RMS: 	  det_set_total_energy(0,data); break;
		case I9_RMS:    det_set_total_energy(1,data); break;
		case I8_RMS:    det_set_total_energy(2,data); break;
		case I7_RMS:    det_set_total_energy(3,data); break;
		case I6_RMS:    det_set_total_energy(4,data); break;
		case I5_RMS:    det_set_total_energy(5,data); break;
		case WATT5_AP:  det_set_total_energy(13,Complement_2_Original(data));	break;
		case WATT6_AP:  det_set_total_energy(12,Complement_2_Original(data));	break;
		case WATT7_AP:  det_set_total_energy(11,Complement_2_Original(data)); break;
		case WATT8_AP:  det_set_total_energy(10,Complement_2_Original(data));	break;
		case WATT9_AP:  det_set_total_energy(18,Complement_2_Original(data));	break;
		case CF9_CNT:   det_set_total_energy(27,data); break;
		case CF8_CNT: 	det_set_total_energy(19,data); break;
		case CF7_CNT:   det_set_total_energy(20,data); break;
		case CF6_CNT:   det_set_total_energy(21,data); break;
		case CF5_CNT:   det_set_total_energy(22,data); break;
		case I4_RMS:    det_set_total_energy(6,data); break;
		case I3_RMS:    det_set_total_energy(7,data); break;
		case I2_RMS:    det_set_total_energy(8,data); break;
		case I1_RMS:    det_set_total_energy(9,data); break;
		case WATT4_AP:  det_set_total_energy(14,Complement_2_Original(data));	break;
		case WATT3_AP:  det_set_total_energy(15,Complement_2_Original(data));	break;
		case WATT2_AP:  det_set_total_energy(16,Complement_2_Original(data)); break;
		case WATT1_AP:  det_set_total_energy(17,Complement_2_Original(data));	break;
		case CF4_CNT: 	det_set_total_energy(23,data); break;
		case CF3_CNT:   det_set_total_energy(24,data); break;
		case CF2_CNT:   det_set_total_energy(25,data); break;
		case CF1_CNT:   det_set_total_energy(26,data); break;
		default:			break;
	}
	return ret;
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_repeat_function
*    功能说明: 重复操作函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_repeat_function(void)
{
	/* 数据等待超时 */
	if( (++sg_bl0910data_t.repeat) <= 5) 
	{
		sg_bl0910data_t.overtime = 1;	/* 重复获取或写入 */
		bl0910_read_reg_function(sg_bl0910data_t.reg,0);
	} 
	else 
		bl0910_send_over_function();		/* 结束任务 */
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_analysis_data_function
*    功能说明: 数据读取函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_analysis_data_function(void)
{
	int8_t   ret = 0;
	/* 等待回传数据 */
	if(BL0910_REC_STA&0x8000) 
	{
		if(sg_bl0910data_t.mode == 0) 		/* 数据处理 */
		{
			ret = bl0910_deal_read_data_function();		/* 读取数据 */
			if(ret != 0) 
				bl0910_repeat_function();
			else 
				bl0910_send_over_function();
		}
		BL0910_REC_STA = 0;
	}
	
	/* 检测本次操作是否超时 */
	if(sg_bl0910data_t.result == 2) 
	{
		sg_bl0910data_t.result = 0;
		bl0910_repeat_function();
	}
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_write_reg_function
*    功能说明: 写寄存器
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_write_reg_function(uint8_t reg,uint8_t *data, uint8_t len,uint8_t mode)
{
	uint8_t buff[64] = {0};
	uint8_t index = 0;		
	
	buff[0] = CMD_WRITE;
	buff[1] = reg;
	
	for(index=0; index<len; index++) 
		buff[2+index] = data[index];
	
	/* 计数和校验 */
	sg_bl0910data_t.checksum = 0;
	for(index=1; index<(2+len); index++)
		sg_bl0910data_t.checksum+=buff[index];
	
	/* 填充和校验 */
	buff[2+len] = 0xff - sg_bl0910data_t.checksum;
	
	if( mode == 0) 	/* 更新标志 */
		bl0910_sending_data_function(reg,0x80);
	
	/* 数据发送 */
	BL0910_SEND_STR(buff,3+len);
}
/*
*********************************************************************************************************
*    函 数 名: bl0910_read_reg_function
*    功能说明: 寄存器读取命令
*    形    参: 
*    返 回 值: 寄存器值
*	@mode		: 0-更新标志 other-不更新
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_read_reg_function(uint8_t reg, uint8_t mode)
{
	uint8_t buff[2] = {0};

	buff[0] = CMD_READ;	/* 数据 */
	buff[1] = reg;	/* 数据 */

	sg_bl0910data_t.checksum = buff[1];   /* 计数和校验 */
	
	if( mode == 0) 	/* 更新标志 */
		bl0910_sending_data_function(reg,0);
	
	BL0910_SEND_STR(buff,sizeof(buff));	/* 数据发送 */
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_send_data_function
*    功能说明: 数据发送函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_send_data_function(void)
{
	/* 允许进行发送操作 */
	if(sg_bl0910data_t.send == 0) 
	{
		if(sg_bl0910data_t.flag > 0)
		{
			switch(sg_bl0910data_t.flag)
			{
				case 1: 	bl0910_read_reg_function(I5_RMS,0); break; // 电流 4
				case 2: 	bl0910_read_reg_function(I6_RMS,0); break; // 电流 3
				case 3: 	bl0910_read_reg_function(I7_RMS,0); break; // 电流 2
				case 4: 	bl0910_read_reg_function(I8_RMS,0); break; // 电流 1
				case 5: 	bl0910_read_reg_function(I9_RMS,0); break; // 总电流
				case 6: 	bl0910_read_reg_function(V_RMS,0); break;  // 电压
				case 7: 	bl0910_read_reg_function(WATT5_AP,0); break; // 功率 4
				case 8: 	bl0910_read_reg_function(WATT6_AP,0); break; // 功率 3
				case 9: 	bl0910_read_reg_function(WATT7_AP,0); break; // 功率 2
				case 10: 	bl0910_read_reg_function(WATT8_AP,0); break; // 功率 1
				case 11: 	bl0910_read_reg_function(WATT9_AP,0); break; // 总功率				
				case 12: 	bl0910_read_reg_function(CF5_CNT,0); break; // 功率 4
				case 13: 	bl0910_read_reg_function(CF6_CNT,0); break; // 功率 3
				case 14: 	bl0910_read_reg_function(CF7_CNT,0); break; // 功率 2
				case 15: 	bl0910_read_reg_function(CF8_CNT,0); break; // 功率 1
				case 16: 	bl0910_read_reg_function(CF9_CNT,0); break; // 总有功脉冲	
				case 17: 	bl0910_read_reg_function(I4_RMS,0); break; // 电流 4
				case 18: 	bl0910_read_reg_function(I3_RMS,0); break; // 电流 3
				case 19: 	bl0910_read_reg_function(I2_RMS,0); break; // 电流 2
				case 20: 	bl0910_read_reg_function(I1_RMS,0); break; // 电流 1
				case 21: 	bl0910_read_reg_function(WATT4_AP,0); break; // 功率 4
				case 22: 	bl0910_read_reg_function(WATT3_AP,0); break; // 功率 3
				case 23: 	bl0910_read_reg_function(WATT2_AP,0); break; // 功率 2
				case 24: 	bl0910_read_reg_function(WATT1_AP,0); break; // 功率 1
				case 25: 	bl0910_read_reg_function(CF4_CNT,0); break; // 功率 4
				case 26: 	bl0910_read_reg_function(CF3_CNT,0); break; // 功率 3
				case 27: 	bl0910_read_reg_function(CF2_CNT,0); break; // 功率 2
				case 28: 	bl0910_read_reg_function(CF1_CNT,0); break; // 功率 1				
				default: break;			
			}
			sg_bl0910data_t.flag--;
		}
	}
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_run_timer_function
*    功能说明: 运行计时相关函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_run_timer_function(void)
{
	static uint16_t  times = 0;
	static uint16_t  auto_time = 0;
	
	if(sg_bl0910data_t.overtime != 0) 	/* 计数更新 */
	{
		if(sg_bl0910data_t.overtime == 1) 
		{
			sg_bl0910data_t.overtime = 2;
			times = 0;
		}
		/* 计数值 */
		if((++times) >= BL0910_TIME_OUT)  // 超时时间
		{
			times = 0;
			sg_bl0910data_t.overtime = 0;
			sg_bl0910data_t.result = 2;
		}
	} 
	else 
		times = 0;
	
	/* 自动采集 */
	if(sg_bl0910data_t.auto_cmd == 1) 
	{
		if((++auto_time) >= BL0910_AUTO_TIME) 
		{
			auto_time = 0;
			sg_bl0910data_t.flag = BL0910_DET_NUM;
			sg_bl0910data_t.send = 0;
		}
	} 
	else 
		auto_time = 0;
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_work_process_function
*    功能说明: 工作进程函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_work_process_function(void)
{
	bl0910_analysis_data_function();
	bl0910_send_data_function();
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_get_rec_data_function
*    功能说明: 获取通信数据
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_get_rec_data_function(uint8_t *buff, uint16_t len)
{
	uint16_t index = 0;

	if(buff == NULL || len == 0) {
		return;
	}
	
	for(index=0; index<len; index++) {
		BL0910_REC_BUFF[index] = buff[index];
	}
	
	sg_bl0910_rec_sta = 0x8000|len;
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_write_enable_function
*    功能说明: 写使能控制函数
*    形    参: 
*    返 回 值: 0-失能 1-使能
*	返 回 值: 无
*********************************************************************************************************
*/
void bl0910_write_enable_function(uint8_t cmd)
{
	uint8_t buff[3] = {0};
	
	if(cmd == 1) 
	{
		buff[0] = 0x55;
		buff[1] = 0x55;
		buff[2] = 0x00;
		bl0910_write_reg_function(US_WRPROT_REG,buff,3,0);
	} 
	else 
	{
		buff[0] = 0x00;
		buff[1] = 0x00;
		buff[2] = 0x00;
		bl0910_write_reg_function(US_WRPROT_REG,buff,3,0);
	}
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_set_gain_function
*    功能说明: 设置增益寄存器
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_set_gain_function(void)
{
	uint8_t gain1_buff[3] = {0};
	uint8_t gain2_buff[3] = {0};
	
	gain1_buff[0] = GAIN_1<<4 | GAIN_1;  // 通道1 电压通道
	gain1_buff[1] = GAIN_1<<4 | GAIN_1;  // 通道3 通道2
	gain1_buff[2] = GAIN_1<<4 | GAIN_1;  // 通道5 通道4

	gain2_buff[0] = GAIN_1<<4 | GAIN_1;  // 通道7 通道6
	gain2_buff[1] = GAIN_1<<4 | GAIN_1;  // 通道9 通道8
	gain2_buff[2] = GAIN_1;  						 // 通道10
	
	bl0910_write_reg_function(GAIN1_REG,gain1_buff,3,0);
	delay_ms(50);
	bl0910_write_reg_function(GAIN2_REG,gain2_buff,3,0);
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_set_ch_function
*    功能说明: 设置通道寄存器
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_set_ch_function(void)
{
	uint8_t ch_buff[3] = {0};
	
	ch_buff[0] = 0x00;  // 全部使用
	ch_buff[1] = 0x04;  // 关闭通道10
	ch_buff[2] = 0x00;  // 关闭通道10	
	bl0910_write_reg_function(ADC_PD_CTRL,ch_buff,3,0);
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_set_mode_function
*    功能说明: 设置模式
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_set_mode_function(void)
{
	uint8_t mode_buff[3] = {0};
	
	mode_buff[0] = 0x00;  // 
	mode_buff[1] = 0x02;  // 打开cf
	mode_buff[2] = 0x00;  // 	
	bl0910_write_reg_function(MODE3_REG,mode_buff,3,0);
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_set_eng_rst_function
*    功能说明: 能量读后清零
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_set_eng_rst_function(void)
{
	uint8_t ch_buff[3] = {0};
	
	ch_buff[0] = 0x00;  // 全部使用
	ch_buff[1] = 0x00;  
	ch_buff[2] = 0x00; 
	bl0910_write_reg_function(RST_ENG,ch_buff,3,0);
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_reset_numreg_function
*    功能说明: 复位数字部分的状态机和寄存器  5A5A5A
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_reset_numreg_function(void)
{
	uint8_t ch_buff[3] = {0};
	
	ch_buff[0] = 0x5A;  // 全部使用
	ch_buff[1] = 0x5A;  
	ch_buff[2] = 0x5A; 
	bl0910_write_reg_function(SOFT_RESET_REG,ch_buff,3,0);
}

/*
*********************************************************************************************************
*    函 数 名: bl0910_test
*    功能说明: 电压、电流测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bl0910_test(void)
{
	bl0910_read_reg_function(TPS_CTRL,0);  // 默认值是0x07FF
	delay_ms(200);	
	bl0910_read_reg_function(SAGLVL_LINECYC,0);  // 默认值是0x100009
	delay_ms(200);	
	bl0910_read_reg_function(ADC_PD_CTRL,0);  // 默认值是0x100009
	delay_ms(200);
	bl0910_read_reg_function(GAIN1_REG,0);  // 默认值是0x100009
	delay_ms(200);	
	bl0910_read_reg_function(GAIN2_REG,0);  // 默认值是0x100009
	delay_ms(200);	
	bl0910_read_reg_function(RST_ENG,0);  // 默认值是0x100009
	delay_ms(200);		
	bl0910_read_reg_function(MODE3_REG,0);  // 默认值是0x100009
	delay_ms(200);
	while(1)
	{
		bl0910_work_process_function();	// 数据获取函数
		delay_ms(20);		
	}
}



