#ifndef _BL0906_H_
#define _BL0906_H_

#include "./SYSTEM/sys/sys.h"

/* 寄存器表单 */
#define BL0906_I1_WAVE 	      0x02 // 通道 1 波形寄存器(正常电流和快速电流可选)
#define BL0906_I2_WAVE 	      0x03 // 通道 2 波形寄存器(正常电流和快速电流可选)
#define BL0906_I3_WAVE 	      0x04 // 通道 3 波形寄存器(正常电流和快速电流可选)
#define BL0906_I4_WAVE 	      0x05 // 通道 4 波形寄存器(正常电流和快速电流可选)
#define BL0906_I5_WAVE 	      0x08 // 通道 5 波形寄存器(正常电流和快速电流可选)
#define BL0906_I6_WAVE 	      0x09 // 通道 6 波形寄存器(正常电流和快速电流可选)
#define BL0906_V_WAVE 	      0x0B // 电压通道波形寄存器
#define BL0906_I1_RMS 	      0x0D // 通道 1 有效值寄存器,无符号
#define BL0906_I2_RMS 	      0x0E // 通道 2 有效值寄存器,无符号
#define BL0906_I3_RMS 	      0x0F // 通道 3 有效值寄存器,无符号
#define BL0906_I4_RMS 	      0x10 // 通道 4 有效值寄存器,无符号
#define BL0906_I5_RMS 	      0x13 // 通道 5 有效值寄存器,无符号
#define BL0906_I6_RMS 	      0x14 // 通道 6 有效值寄存器,无符号
#define BL0906_V_RMS 	      	0x16 // 电压通道有效值寄存器
#define BL0906_I1_FAST_RMS 	  0x18 // 通道 1 快速有效值寄存器,无符号
#define BL0906_I2_FAST_RMS 	  0x19 // 通道 2 快速有效值寄存器,无符号
#define BL0906_I3_FAST_RMS 	  0x1A // 通道 3 快速有效值寄存器,无符号
#define BL0906_I4_FAST_RMS 	  0x1B // 通道 4 快速有效值寄存器,无符号
#define BL0906_I5_FAST_RMS 	  0x1E // 通道 5 快速有效值寄存器,无符号
#define BL0906_I6_FAST_RMS 	  0x1F // 通道 6 快速有效值寄存器,无符号
#define BL0906_V_FAST_RMS 	  0x21 // 电压通道快速有效值寄存器
#define BL0906_WATT1_AP 	  	0x23 // 通道 1 有功功率寄存器
#define BL0906_WATT2_AP 	  	0x24 // 通道 2 有功功率寄存器
#define BL0906_WATT3_AP 	  	0x25 // 通道 3 有功功率寄存器
#define BL0906_WATT4_AP 	  	0x26 // 通道 4 有功功率寄存器
#define BL0906_WATT5_AP 	  	0x29 // 通道 5 有功功率寄存器
#define BL0906_WATT6_AP 	  	0x2A // 通道 6 有功功率寄存器
#define BL0906_WATT_ALL_AP 	  0x2C // 总有功功率寄存器
#define BL0906_VAR		 	  		0x2D // 可选通道无功功率寄存器（基波）
#define BL0906_VA 	  				0x2E // 可选通道视在功率寄存器
#define BL0906_CF1_CNT 	  		0x30 // 通道 1 有功脉冲计数，无符号
#define BL0906_CF2_CNT 	  		0x31 // 通道 2 有功脉冲计数，无符号
#define BL0906_CF3_CNT 	  		0x32 // 通道 3 有功脉冲计数，无符号
#define BL0906_CF4_CNT 	  		0x33 // 通道 4 有功脉冲计数，无符号
#define BL0906_CF5_CNT 	  		0x36 // 通道 5 有功脉冲计数，无符号
#define BL0906_CF6_CNT 	  		0x37 // 通道 6 有功脉冲计数，无符号
#define BL0906_CF_CNT 	  		0x39 // 总有功脉冲计数，无符号

#define BL0906_STATUS1 	  		0x54 // 中断状态寄存器 1
#define BL0906_STATUS3 	  		0x56 // M状态寄存器 
#define BL0906_TPS 	  				0x5E // 内部温度值寄存器

#define BL0906_GAIN1_REG 	  	0x60 // 通道 PGA 增益调整寄存器0000=1 0001=2 0010=8 0011=16
																		// [3:0]->(电压) [8:11]->通道 1......
#define BL0906_GAIN2_REG 	  	0x61 // 通道 PGA 增益调整寄存器0000=1 0001=2 0010=8 0011=16
																		// [8:11]->通道 5......

#define BL0906_PHASE1 	  	  0x64
#define BL0906_PHASEV 	  	  0x69	
#define BL0906_VAR_CREEP      0x88
#define BL0906_RMS_CREEP      0x8A
#define BL0906_FAST_RMS_CTRL  0x8B
#define BL0906_SAGLVL         0x8F

#define BL0906_ADC_PD 	  		0x93 // 7 个通道 ADC 的使能控制
																	 // ADC_PD<0>控制电压通道; ADC_PD<10:1>控制对应的电流通道 10 到电流通道 1
#define BL0906_MODE1 	 	 			0x96 // 用户模式选择寄存器 1
#define BL0906_MODE2	 	 			0x97 // 用户模式选择寄存器 2
#define BL0906_MODE3 	 	 			0x98 // 用户模式选择寄存器 3

#define BL0906_RST_ENG 	 	 	  0x9D // 能量读后清零设置寄存器

#define BL0906_US_WRPROT 			0x9E // 用户写保护设置寄存器
															// 写入5555H时,表示可操作用户寄存器对 reg60 到 reg9d, rega0到d0

#define BL0906_SOFT_RESET 		0x9F // 当输入为 5A5A5A 时，系统复位
															// 当输入为 55AA55 时，用户读写寄存器复位Reset: reg60 到reg9f, rega0 到 regd0

#define BL0906_VOLT_KP	     	12238.14f   		// 电压系数 - v
#define BL0906_CURR_KP		   	1197.128f     	// 电流系数 - ma
#define BL0906_POWER_KP		 		3493.84f     	  // 功率系数 - W
#define BL0906_ELEC_Ke		   	0.00001093f     // 系数 - 度

/* 设置量 */
#define BL0906_GAIN_1 	 	 	0x00 // 增益1
#define BL0906_GAIN_2 	 	 	0x01 // 增益2
#define BL0906_GAIN_8 	 	 	0x02 // 增益8
#define BL0906_GAIN_16 	 		0x03 // 增益16

#define BL0906_CMD_WRITE 		0x81
#define BL0906_CMD_READ    	0x82

/* 供其他C函数调用的函数 */
void bl0906_init(void);

void bl0906_write_reg_function(uint8_t reg,uint8_t *data, uint8_t len);
uint8_t bl0906_read_reg_function(uint8_t reg, uint32_t *data);
void bl0906_run_timer_function(void);
void bl0906_work_process_function(void);
void bl0906_write_enable_function(uint8_t cmd);
void bl0906_set_gain_function(void);
void bl0906_set_ch_function(void);
void bl0906_set_mode_function(void);
void bl0906_set_eng_rst_function(void);
void bl0906_reset_numreg_function(void);
void bl0906_test(void);

#endif
