#ifndef _BL0942_H_
#define _BL0942_H_

#include "./SYSTEM/sys/sys.h"

/* 寄存器表单 通讯协议的数据字节为 24bit，高位无效位补 0。 */
#define BL0942_I_WAVE 	      	0x01 // 电流波形寄存器，有符号
#define BL0942_V_WAVE 	      	0x02 // 电压波形寄存器，有符号
#define BL0942_I_RMS 	        0x03 // 电流有效值寄存器，无符号
#define BL0942_V_RMS  	      	0x04 // 电压有效值寄存器，无符号
#define BL0942_I_FAST_RMS 	  	0x05 // 电流快速有效值寄存器，无符号
#define BL0942_WATT 	      	0x06 // 有功功率寄存器，有符号
#define BL0942_CF_CNT 	      	0x07 // 有功电能脉冲计数寄存器，无符号
#define BL0942_FREQ 	      	0x08 // 线电压频率寄存器
#define BL0942_STATUS 	      	0x09 // 状态寄存器

#define BL0942_I_RMSOS			0x12 // 电流有效值小信号校正寄存器
#define BL0942_WA_CREEP			0x14 // 有功功率防潜寄存器
#define BL0942_I_FAST_RMS_TH	0x15 // 电流快速有效值阈值寄存器
#define BL0942_I_FAST_RMS_CYC	0x16 // 电流快速有效值刷新周期寄存器
#define BL0942_FREQ_CYC      	0x17 // 线电压频率刷新周期寄存器
#define BL0942_OT_FUNX			0x18 // 输出配置寄存器
#define BL0942_MODE				0x19 // 用户模式选择寄存器
#define BL0942_GAIN_CR        	0x1A // 电流通道增益控制寄存器
#define BL0942_SOFT_RESET_REG 	0x1C // 写入 0x5A5A5A 时，用户区寄存器复位
#define BL0942_US_WRPROT_REG	0x1D // 用户写保护设置寄存器
										    	         // 写入 0x55 后，用户操作寄存器可以写入；写入其他值，用户操作寄存器区域不可写入

#define BL0942_VOLT_KP	      	15123.786f    // 电压系数 - v
#define BL0942_CURR_KP		    25121.346f    // 电流系数 - ma
#define BL0942_WATT_KP		    81.629f       // 功率系数 - W
#define BL0942_ELEC_Ke		    0.000078f    // 系数 - 度

/* 设置量 */
#define BL0942_GAIN_1 	 	  	0x00 // 增益1
#define BL0942_GAIN_2 	 	  	0x01 // 增益4
#define BL0942_GAIN_8 	 	  	0x02 // 增益16
#define BL0942_GAIN_16 	 	    0x03 // 增益24


//写字节 {1,0,1,0,1,0,A2,A1}为写操作的帧识别字节。假设{A2,A1}=10，器件地址 2，帧识别字节为 0xAA
//读字节 {0,1,0,1,1,0,A2,A1}为读操作的帧识别字节，假设{A2,A1}=10，器件地址 2，帧识别字节为 0x5A
#define BL0942_CMD_WRITE 	    0xA8
#define BL0942_CMD_READ       0x58

/* 提供给其他C文件调用的函数 */
void bl0942_init(void);

void bl0942_write_reg_function(uint8_t reg,uint8_t *data, uint8_t len);
uint8_t bl0942_read_reg_function(uint8_t reg, uint32_t *data);
void bl0942_run_timer_function(void);
void bl0942_work_process_function(void);
void bl0942_write_enable_function(uint8_t cmd);
void bl0942_set_gain_function(void);

void bl0942_test(void);

#endif
