#include "BL0942.h"
#include "elec_uart.h"
#include "delay.h"
#include "det.h"

/*
	7、单相计量芯片: 串口3，波特率4800，
	   引脚分配为：  USART3_TX： PD9
                    USART3_RX： PD8

实际电压值(V) = [电压有效值寄存器值*Vref*(R8+R9+R10+R11+R12)]/[73978*R7*1000]

电压系数Kv  = [73978*R7*1000]/[Vref*(R25+R26+R35+R36+R37)]
					 = [73978*(24.9)*1000]/[1.218*(20000*5)]
					 = 1842077100 / 121800
					 = 15123.786


实际电流值(A) = [电流有效值寄存器值*Vref] / [305978*Gain_I*(R5)*1000/Rt]

电流系数 Ki  = [305978*(R5)*1000/Rt] / [Vref*Gain_I]
						= [305978*1*(2.2)*1000/1000] / [1.218*16]
						= 552669.62/16 (A)
						= 34.542   (mA)

实际有功功率值(W) = [有功功率寄存器值*Vref*Vref*(R8+R9+R10+R11+R12)]/
										[3537*(R5*1000/Rt)*R7*1000]
功率系数Kp = [3537*(R5*1000/Rt)*R7*1000]/[Vref*Vref*(R8+R9+R10+R11+R12)]
           = [3537*(2.2*1000/1000)*24.9*1000]/[1.218*1.218*(20000*5)]  
					 = [3537*2.2*24.9]/[1.218*1.218*(20*5)]
					 = 193756860 / 148352.4
					 = 1306.058 /16
					 = 81.629
*/


struct bl0942_data_t {
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

static uint16_t sg_bl0942_rec_sta = 0;
static uint8_t  sg_bl0942_buff[ELEC_RX_MAX] = {0};
struct bl0942_data_t sg_bl0942data_t = {0};

/* 接口与参数 */
#define BL0942_BAUDRATE (4800)
#define BL0942_USART_INIT(baudrate) elec_uart_init(baudrate)
#define BL0942_SEND_STR(buff,len) 	elec_send_str_function(buff,len)

/* 宏定义数据 */
#define BL0942_DET_NUM   			4  		  // 采集次数 
#define BL0942_TIME_OUT  			200 		// 超时时间 200ms
#define BL0942_AUTO_TIME   		1000 	  // 1s (采集9次，每次100ms)
#define BL0942_SEND_TIME   		100 	  // 发送时间 100ms
/* 数据 */
#define BL0942_REC_STA  sg_bl0942_rec_sta
#define BL0942_REC_BUFF sg_bl0942_buff

/*
*********************************************************************************************************
*    函 数 名: bl0942_reset_function
*    功能说明: 复位函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0942_reset_function(void)
{
//	GPIO_InitTypeDef GPIO_InitStructure;
//	RCC_APB2PeriphClockCmd(ELEC_RX_GPIO_CLK,ENABLE);

//	GPIO_InitStructure.GPIO_Pin   = ELEC_RX_PIN;
//	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
//	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

//	GPIO_Init(ELEC_RX_GPIO_PORT,&GPIO_InitStructure);

//	GPIO_WriteBit(ELEC_RX_GPIO_PORT,ELEC_RX_PIN,Bit_RESET);
//	delay_ms(30);
//	GPIO_WriteBit(ELEC_RX_GPIO_PORT,ELEC_RX_PIN,Bit_SET);
//	delay_ms(30);
}

/*
*********************************************************************************************************
*    函 数 名: bl0942c_init_function
*    功能说明: 初始化
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0942c_init_function(void)
{
	bl0942_reset_function();
	BL0942_USART_INIT(BL0942_BAUDRATE);
	
	/* 开启自动采集功能-电压、电流 */
	sg_bl0942data_t.auto_cmd = 1;
		
	/* 写命令使能 */
	bl0942_write_enable_function(1);
	delay_ms(50);	
	bl0942_set_gain_function();  // 设置增益
	delay_ms(50);	
	
	/* 写命令失能 */
	bl0942_write_enable_function(0);
	delay_ms(100);
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_sending_data_function
*    功能说明: 发送数据函数
*    形    参: @reg		: 寄存器值
*    返 回 值: 发送模式
*    ? ? ?: *	
*********************************************************************************************************
*/
void bl0942_sending_data_function(uint8_t reg, uint8_t mode)
{
	sg_bl0942data_t.overtime 	= 1;  // 超时倒计时
	sg_bl0942data_t.result  	= 0;   
	sg_bl0942data_t.mode    	= mode;
	sg_bl0942data_t.send    	= 1;
	sg_bl0942data_t.reg     	= reg;
	sg_bl0942data_t.repeat  	= 0;
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_send_over_function
*    功能说明: 数据发送操作完成
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0942_send_over_function(void)
{
	sg_bl0942data_t.overtime = 0;
	sg_bl0942data_t.result  = 0;
	sg_bl0942data_t.send    = 0;
	sg_bl0942data_t.repeat  = 0;
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_deal_read_data_function
*    功能说明: 处理读取到的数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t bl0942_deal_read_data_function(void)
{
	uint32_t temp  = 0;
	uint32_t data  = 0;
	int8_t   ret   = 0;
	uint8_t  index = 0;

	for(index=0; index<3; index++) 
		sg_bl0942data_t.checksum += BL0942_REC_BUFF[index];

	if(BL0942_REC_BUFF[3] != (0xff - sg_bl0942data_t.checksum)) 
	{
		ret = -1;
	}
	data = BL0942_REC_BUFF[2];
	data = (data<<8) | BL0942_REC_BUFF[1];
	data = (data<<8) | BL0942_REC_BUFF[0];

	switch(sg_bl0942data_t.reg) 
	{
		case V_RMS: 	det_set_total_energy(0,data); break;
		case I_RMS:   det_set_total_energy(1,data); break;
		case WATT :   // 有符号位  
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
			det_set_total_energy(2,data); 
			break;
			case CF_CNT:   det_set_total_energy(3,data); break;
		default:			break;
	}
	return ret;
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_repeat_function
*    功能说明: 重复操作函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0942_repeat_function(void)
{
	/* 数据等待超时 */
	if( (++sg_bl0942data_t.repeat) <= 5) 
	{
		sg_bl0942data_t.overtime = 1;	/* 重复获取或写入 */
		bl0942_read_reg_function(sg_bl0942data_t.reg,0);
	} 
	else 
		bl0942_send_over_function();		/* 结束任务 */
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_analysis_data_function
*    功能说明: 数据读取函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0942_analysis_data_function(void)
{
	int8_t   ret = 0;
	/* 等待回传数据 */
	if(BL0942_REC_STA&0x8000) 
	{
		if(sg_bl0942data_t.mode == 0) 		/* 数据处理 */
		{
			ret = bl0942_deal_read_data_function();		/* 读取数据 */
			if(ret != 0) 
				bl0942_repeat_function();
			else 
				bl0942_send_over_function();
		}
		BL0942_REC_STA = 0;
	}
	
	/* 检测本次操作是否超时 */
	if(sg_bl0942data_t.result == 2) 
	{
		sg_bl0942data_t.result = 0;
		bl0942_repeat_function();
	}
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_write_reg_function
*    功能说明: 写寄存器
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0942_write_reg_function(uint8_t reg,uint8_t *data, uint8_t len,uint8_t mode)
{
	uint8_t buff[64] = {0};
	uint8_t index = 0;		
	
	buff[0] = BL0942_CMD_WRITE;
	buff[1] = reg;
	
	for(index=0; index<len; index++) 
		buff[2+index] = data[index];
	
	/* 计数和校验 */
	sg_bl0942data_t.checksum = 0;
	for(index=0; index<(2+len); index++)
		sg_bl0942data_t.checksum+=buff[index];
	
	/* 填充和校验 */
	buff[2+len] = 0xff - sg_bl0942data_t.checksum;
	
	if( mode == 0) 	/* 更新标志 */
		bl0942_sending_data_function(reg,0x80);
	
	/* 数据发送 */
	BL0942_SEND_STR(buff,3+len);
}
/*
*********************************************************************************************************
*    函 数 名: bl0942_read_reg_function
*    功能说明: 寄存器读取命令
*    形    参: @reg		: 寄存器值
*    返 回 值: 0-更新标志 other-不更新
*    ? ? ?: *	
*********************************************************************************************************
*/
void bl0942_read_reg_function(uint8_t reg, uint8_t mode)
{
	uint8_t buff[2] = {0};

	buff[0] = BL0942_CMD_READ;	/* 数据 */
	buff[1] = reg;	/* 数据 */

	sg_bl0942data_t.checksum = buff[1]+buff[0];   /* 计数和校验 */
	
	if( mode == 0) 	/* 更新标志 */
		bl0942_sending_data_function(reg,0);
	
	BL0942_SEND_STR(buff,sizeof(buff));	/* 数据发送 */
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_send_data_function
*    功能说明: 数据发送函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0942_send_data_function(void)
{
	/* 允许进行发送操作 */
	if(sg_bl0942data_t.send == 0) 
	{
		if(sg_bl0942data_t.flag > 0)
		{
			switch(sg_bl0942data_t.flag)
			{
				case 1: 	bl0942_read_reg_function(I_RMS,0); break; // 电流
				case 2: 	bl0942_read_reg_function(V_RMS,0); break; // 电压
				case 3: 	bl0942_read_reg_function(WATT,0); break;  // 有功功率
				case 4: 	bl0942_read_reg_function(CF_CNT,0); break;  // 用电量
				default: break;			
			}
			sg_bl0942data_t.flag--;
		}
	}
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_run_timer_function
*    功能说明: 运行计时相关函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0942_run_timer_function(void)
{
	static uint16_t  times = 0;
	static uint16_t  auto_time = 0;
	
	if(sg_bl0942data_t.overtime != 0) 	/* 计数更新 */
	{
		if(sg_bl0942data_t.overtime == 1) 
		{
			sg_bl0942data_t.overtime = 2;
			times = 0;
		}
		/* 计数值 */
		if((++times) >= BL0942_TIME_OUT)  // 超时时间
		{
			times = 0;
			sg_bl0942data_t.overtime = 0;
			sg_bl0942data_t.result = 2;
		}
	} 
	else 
		times = 0;
	
	/* 自动采集 */
	if(sg_bl0942data_t.auto_cmd == 1) 
	{
		if((++auto_time) >= BL0942_AUTO_TIME) 
		{
			auto_time = 0;
			sg_bl0942data_t.flag = BL0942_DET_NUM;
			sg_bl0942data_t.send = 0;
		}
	} 
	else 
		auto_time = 0;
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_work_process_function
*    功能说明: 工作进程函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0942_work_process_function(void)
{
	bl0942_analysis_data_function();
	bl0942_send_data_function();
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_get_rec_data_function
*    功能说明: 获取通信数据
*    形    参: @
*    返 回 值: 
*********************************************************************************************************
*/
void bl0942_get_rec_data_function(uint8_t *buff, uint16_t len)
{
	uint16_t index = 0;

	if(buff == NULL || len == 0) {
		return;
	}
	
	for(index=0; index<len; index++) {
		BL0942_REC_BUFF[index] = buff[index];
	}
	
	sg_bl0942_rec_sta = 0x8000|len;
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_write_enable_function
*    功能说明: 写使能控制函数
*    形    参: @cmd		: 0-失能 1-使能
*    返 回 值: 
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
		bl0942_write_reg_function(US_WRPROT_REG,buff,3,0);
	} 
	else 
	{
		buff[0] = 0x00;
		buff[1] = 0x00;
		buff[2] = 0x00;
		bl0942_write_reg_function(US_WRPROT_REG,buff,3,0);
	}
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_set_gain_function
*    功能说明: 设置增益寄存器
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0942_set_gain_function(void)
{
	uint8_t gain_buff[3] = {0};
	
	gain_buff[0] = GAIN_1;  // 电流通道

	bl0942_write_reg_function(GAIN_CR,gain_buff,3,0);
	delay_ms(50);
}

/*
*********************************************************************************************************
*    函 数 名: bl0942_test
*    功能说明: 电压、电流测试
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0942_test(void)
{
	bl0942_read_reg_function(FREQ,0);  // 默认值是0x4E20
	delay_ms(200);	
	bl0942_read_reg_function(OT_FUNX,0);  // 默认值是0x24
	delay_ms(200);	
	bl0942_read_reg_function(MODE,0);  // 默认值是0x87
	delay_ms(200);
	bl0942_read_reg_function(GAIN_CR,0);  // 默认值是0x2
	delay_ms(200);		
	while(1)
	{
		bl0942_work_process_function();	// 数据获取函数
		delay_ms(100);		
	}
}






