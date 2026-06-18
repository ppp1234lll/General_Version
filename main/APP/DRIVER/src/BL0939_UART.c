#include "BL0939.h"
#include "bsp_sspi_energy.h"
#include "delay.h"
#include "det.h"
#include <stdio.h>

/*
	7、BL0939单相计量芯片: 模拟SPI
	   引脚分配为： SCLK： PD4  PA4
									MOSI： PD3  PA5
									MISO： PD2  PC6
									 
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


struct bl0939_data_t {
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

static uint16_t sg_bl0939_rec_sta = 0;
static uint8_t  sg_bl0939_buff[100] = {0};
struct bl0939_data_t sg_bl0939data_t = {0};

float ld_current;


/* 接口与参数 */
#define bl0939_BAUDRATE (4800)
#define bl0939_INIT() 							bsp_InitSSPI()
#define bl0939_SEND_STR(buff,len) 	SSPI_Write_Multi_Byte(buff,len)

/* 宏定义数据 */
#define bl0939_DET_NUM   			1  		// 采集次数 
#define bl0939_TIME_OUT  			200 		// 超时时间 200ms
#define bl0939_AUTO_TIME   		1000 	  // 2s (采集18次，每次100ms)
#define bl0939_SEND_TIME   		100 	  // 发送时间 100ms
/* 数据 */
#define bl0939_REC_STA  sg_bl0939_rec_sta
#define bl0939_REC_BUFF sg_bl0939_buff

/*
*********************************************************************************************************
*    函 数 名: bl0939_init_function
*    功能说明: 初始化
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0939_init_function(void)
{
	bl0939_INIT();
	
	/* 开启自动采集功能-电压、电流 */
	sg_bl0939data_t.auto_cmd = 1;
		
	/* 写命令使能 */
	bl0939_write_enable_function(1);
	bl0939_reset_numreg_function();
	bl0939_set_mode_function();
	/* 写命令失能 */
	bl0939_write_enable_function(0);
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_sending_data_function
*    功能说明: 发送数据函数
*    形    参: @reg		: 寄存器值
*    返 回 值: 发送模式
*    ? ? ?: *	
*********************************************************************************************************
*/
void bl0939_sending_data_function(uint8_t reg, uint8_t mode)
{
	sg_bl0939data_t.overtime 	= 1;  // 超时倒计时
	sg_bl0939data_t.result  	= 0;   
	sg_bl0939data_t.mode    	= mode;
	sg_bl0939data_t.send    	= 1;
	sg_bl0939data_t.reg     	= reg;
	sg_bl0939data_t.repeat  	= 0;
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_send_over_function
*    功能说明: 数据发送操作完成
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0939_send_over_function(void)
{
	sg_bl0939data_t.overtime = 0;
	sg_bl0939data_t.result  = 0;
	sg_bl0939data_t.send    = 0;
	sg_bl0939data_t.repeat  = 0;
}

/*
*********************************************************************************************************
*    函 数 名: Complement_2_Original
*    功能说明: 补码转换为原码
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint32_t Complement_3_Original(uint32_t data)
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
*    函 数 名: bl0939_deal_read_data_function
*    功能说明: 处理读取到的数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t bl0939_deal_read_data_function(void)
{
	uint32_t data  = 0;
	int8_t   ret   = 0;
	uint8_t  index = 0;
	
	for(index=0; index<3; index++) 
		sg_bl0939data_t.checksum += bl0939_REC_BUFF[index];

	if(bl0939_REC_BUFF[3] != (0xff - sg_bl0939data_t.checksum)) 
	{
		ret = -1;
	}
	data = bl0939_REC_BUFF[0];
	data = (data<<8) | bl0939_REC_BUFF[1];
	data = (data<<8) | bl0939_REC_BUFF[2];
	
//	printf("%d\n",data);

	switch(sg_bl0939data_t.reg) 
	{
		case BL0939_V_WAVE: 	 BL0942_set_total_energy(1,data); break;
//		case BL0939_USR_WRPROT: BL0942_set_total_energy(0,data); break;
//		case BL0939_V_RMS: 	 det_set_total_energy(0,data); break;
//		case BL0939_IA_RMS:  det_set_total_energy(1,data); break;
//		case BL0939_IB_RMS:  det_set_total_energy(2,data); break;
//		case BL0939_A_WATT:  det_set_total_energy(3,Complement_3_Original(data));	break;
//		case BL0939_B_WATT:  det_set_total_energy(4,Complement_3_Original(data));	break;
//		case BL0939_CFA_CNT: det_set_total_energy(5,data); break;
//		case BL0939_CFB_CNT: det_set_total_energy(6,data); break;
		default:			break;
	}
	return ret;
}

 void BL0942_set_total_energy(uint8_t num , uint32_t data)
 {
	 
	//switch(num)
	//{
	//	case 1: sg_datacollec_t.total_current = data / BL0942_CURR_KP; 	break;
	//	case 2: sg_datacollec_t.power = data / BL0942_WATT_KP;		break;
		//printf("%d\n",data);
		ld_current = data / 25121.00f;
		//case 0: ld_current = data / 25121; 			break;
		//case 1: ld_current = data / 25121; 			break;
		//default:			break;
    
	//}
	//printf("%f\n",ld_current);
 }



 
 float BL0942_get_ld_current_handler(void)
 {
	 return ld_current;
 }



/*
*********************************************************************************************************
*    函 数 名: bl0939_repeat_function
*    功能说明: 重复操作函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0939_repeat_function(void)
{
	/* 数据等待超时 */
	if( (++sg_bl0939data_t.repeat) <= 5) 
	{
		sg_bl0939data_t.overtime = 1;	/* 重复获取或写入 */
		bl0939_read_reg_function(sg_bl0939data_t.reg,0);
	} 
	else 
		bl0939_send_over_function();		/* 结束任务 */
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_analysis_data_function
*    功能说明: 数据读取函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0939_analysis_data_function(void)
{
	int8_t   ret = 0;
	/* 等待回传数据 */
	if(bl0939_REC_STA&0x8000) 
	{
		
		if(sg_bl0939data_t.mode == 0) 		/* 数据处理 */
		{
			ret = bl0939_deal_read_data_function();		/* 读取数据 */
			
			//printf("%d \r\n",ret);
			//printf("123/n");
			
			if(ret != 0) 
				bl0939_repeat_function();
			else 
				bl0939_send_over_function();
		}
		bl0939_REC_STA = 0;
	}
	
	/* 检测本次操作是否超时 */
	if(sg_bl0939data_t.result == 2) 
	{
		sg_bl0939data_t.result = 0;
		bl0939_repeat_function();
	}
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_write_reg_function
*    功能说明: 写寄存器
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0939_write_reg_function(uint8_t reg,uint8_t *data, uint8_t len,uint8_t mode)
{
	uint8_t buff[64] = {0};
	uint8_t index = 0;		
	
	buff[0] = BL0939_CMD_WRITE;
	buff[1] = reg;
	
	for(index=0; index<len; index++) 
		buff[2+index] = data[index];
	
	/* 计数和校验 */
	sg_bl0939data_t.checksum = 0;
	for(index=0; index<(2+len); index++)
		sg_bl0939data_t.checksum+=buff[index];
	
	/* 填充和校验 */
	buff[2+len] = 0xff - sg_bl0939data_t.checksum;
	
	if( mode == 0) 	/* 更新标志 */
		bl0939_sending_data_function(reg,0x80);
	
	/* 数据发送 */
	bl0939_SEND_STR(buff,3+len);
}
/*
*********************************************************************************************************
*    函 数 名: bl0939_read_reg_function
*    功能说明: 寄存器读取命令
*    形    参: @reg		: 寄存器值
*    返 回 值: 0-更新标志 other-不更新
*    ? ? ?: *	
*********************************************************************************************************
*/
void bl0939_read_reg_function(uint8_t reg, uint8_t mode)
{
	uint8_t buff[2] = {0};
	uint16_t index = 0;
	
	buff[0] = BL0939_CMD_READ;	/* 读取 */
	buff[1] = reg;	/* 数据 */

	sg_bl0939data_t.checksum = buff[0]+buff[1];   /* 计数和校验 */
	
	if( mode == 0) 	/* 更新标志 */
		bl0939_sending_data_function(reg,0);
	
	bl0939_SEND_STR(buff,sizeof(buff));	/* 数据发送 */
	
	for(index=0; index < 4; index++) {
		bl0939_REC_BUFF[index] = SSPI_ReadByte();
	}
	bl0939_REC_STA = 0x8000+4;	
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_send_data_function
*    功能说明: 数据发送函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0939_send_data_function(void)
{
	/* 允许进行发送操作 */
	if(sg_bl0939data_t.send == 0) 
	{
		//bl0939_read_reg_function(0x03,0);
		
		
		if(sg_bl0939data_t.flag > 0)
		{
			//printf("456\n");
			switch(sg_bl0939data_t.flag)
			{	
				case 1: 	bl0939_read_reg_function(BL0939_V_WAVE,0); break; // 电流 A
//				case 2: 	bl0939_read_reg_function(BL0939_IB_RMS,0); break; // 电流 B
//				case 3: 	bl0939_read_reg_function(BL0939_V_RMS,0); break;  // 电压
//				case 4: 	bl0939_read_reg_function(BL0939_A_WATT,0); break; // 功率 A
//				case 5: 	bl0939_read_reg_function(BL0939_B_WATT,0); break; // 功率 B			
//				case 6: 	bl0939_read_reg_function(BL0939_CFA_CNT,0); break; // 总有功脉冲	A	
//				case 7: 	bl0939_read_reg_function(BL0939_CFB_CNT,0); break; // 总有功脉冲	B	
				default: break;			
			}
			sg_bl0939data_t.flag--;
		}
	}
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_run_timer_function
*    功能说明: 运行计时相关函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0939_run_timer_function(void)
{
	static uint16_t  times = 0;
	static uint16_t  auto_time = 0;
	
	if(sg_bl0939data_t.overtime != 0) 	/* 计数更新 */
	{
		if(sg_bl0939data_t.overtime == 1) 
		{
			sg_bl0939data_t.overtime = 2;
			times = 0;
		}
		/* 计数值 */
		if((++times) >= bl0939_TIME_OUT)  // 超时时间
		{
			times = 0;
			sg_bl0939data_t.overtime = 0;
			sg_bl0939data_t.result = 2;
		}
	} 
	else 
		times = 0;
	
	/* 自动采集 */
	if(sg_bl0939data_t.auto_cmd == 1) 
	{
		if((++auto_time) >= bl0939_AUTO_TIME) 
		{
			//printf("123\n");
			auto_time = 0;
			sg_bl0939data_t.flag = bl0939_DET_NUM;
			sg_bl0939data_t.send = 0;
		}
	} 
	else 
		auto_time = 0;
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_work_process_function
*    功能说明: 工作进程函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0939_work_process_function(void)
{
	//uint32_t data;
	
	//bl0939_read_reg_function(0x03,0);
	bl0939_analysis_data_function();
	
	bl0939_send_data_function();
	
//	data = bl0939_REC_BUFF[0];
//	data = (data<<8) | bl0939_REC_BUFF[1];
//	data = (data<<8) | bl0939_REC_BUFF[2];
	
	//printf("%d %d %d",bl0939_REC_BUFF[0],bl0939_REC_BUFF[1],bl0939_REC_BUFF[2]);
	//printf("%d ",data);
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_get_rec_data_function
*    功能说明: 获取通信数据
*    形    参: @
*    返 回 值: 
*********************************************************************************************************
*/
void bl0939_get_rec_data_function(uint8_t *buff, uint16_t len)
{
	uint16_t index = 0;

	if(buff == NULL || len == 0) {
		return;
	}
	
	for(index=0; index<len; index++) {
		bl0939_REC_BUFF[index] = buff[index];
	}
	
	sg_bl0939_rec_sta = 0x8000|len;
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_write_enable_function
*    功能说明: 写使能控制函数
*    形    参: @cmd		: 0-失能 1-使能
*    返 回 值: 
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
		bl0939_write_reg_function(BL0939_USR_WRPROT,buff,3,0);
	} 
	else 
	{
		buff[0] = 0x00;
		buff[1] = 0x00;
		buff[2] = 0x00;
		bl0939_write_reg_function(BL0939_USR_WRPROT,buff,3,0);
	}
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_set_mode_function
*    功能说明: 设置模式
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0939_set_mode_function(void)
{
	uint8_t mode_buff[3] = {0};
	
	mode_buff[0] = 0x00;  // 
	mode_buff[1] = 0x01;  // 打开cf
	mode_buff[2] = 0x00;  // 	
	bl0939_write_reg_function(BL0939_MODE,mode_buff,3,0);
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_reset_numreg_function
*    功能说明: 用户区寄存器复位
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0939_reset_numreg_function(void)
{
	uint8_t ch_buff[3] = {0};
	
	ch_buff[0] = 0x5A;  // 全部使用
	ch_buff[1] = 0x5A;  
	ch_buff[2] = 0x5A; 
	bl0939_write_reg_function(BL0939_SOFT_RESET,ch_buff,3,0);
}

/*
*********************************************************************************************************
*    函 数 名: bl0939_test
*    功能说明: 电压、电流测试
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void bl0939_test(void)
{
	//bl0939_read_reg_function(BL0939_TPS_CTRL,0);  // 默认值是0x07FF
	
	//bl0939_read_reg_function(0x03,0);  
	
	delay_ms(200);	
	while(1)
	{
		bl0939_read_reg_function(0x14,0);
//		softSPI_WriteByte(0x58);
//		softSPI_WriteByte(0x08);
		delay_ms(1);
		bl0939_analysis_data_function();
//		//bl0939_work_process_function();	// 数据获取函数
		delay_ms(200);		
	}
}






