#ifndef _BL0972_H_
#define _BL0972_H_

#include "./SYSTEM/sys/sys.h"

/* 寄存器表单 */
#define BL0972_IA_WAVE  	      		0x05 // 电流波形寄存器，有符号
#define BL0972_V_WAVE 	      			0x0B // 电压波形寄存器，有符号
#define BL0972_IA_RMS 	      			0x10 // 电流有效值寄存器，无符号
#define BL0972_V_RMS	      				0x16 // 电压有效值寄存器，无符号
#define BL0972_A_WATT     					0x26 // 有功功率寄存器，有符号
#define BL0972_CFA_CNT 	      			0x33 // 有功电能脉冲计数，无符号
#define BL0972_A_CORNER 	      		0x40 // 电流电压波形相角寄存器

#define BL0972_TPS1	      					0x5E // 内部温度检测寄存器，无符号
#define BL0972_TPS2 	      				0x5F // 外部温度检测寄存器，无符号

#define BL0972_MODE1	  						0x96 // 用户模式选择寄存器
#define BL0972_MODE2	  						0x97 // 用户模式选择寄存器
#define BL0972_MODE3	  					  0x98 // 用户模式选择寄存器

#define BL0972_GAIN	  							0x60 // 增益设置
#define BL0972_GH	  						    0x93 // 增益设置

#define BL0972_SOFT_RESET 	  			0x9F // 写入 0x5A5A5A 时，用户区寄存器复位
#define BL0972_USR_WRPROT 	  			0x9E // 用户写保护设置寄存器。写入 0x55 后，用户操作寄存器可以写入；写入其他值，用户操作寄存器区域不可写入
#define BL0972_TPS_CTRL 	  				0x1B // 温度模式控制寄存器
#define BL0972_TPS2_A 	  					0x1C // 外部温度传感器增益系数校正寄存器
#define BL0972_TPS2_B 	 						0x1D // 外部温度传感器偏移系数校正寄存器

#define BL0972_PHASE_I	 						0x66 // [15： 8]：电流通道
#define BL0972_VAR_CREEP						0x88 // 23:12] 为无功防潜动功率阈值
#define BL0972_RMS_CREEP						0x8A // 有效值小信号阈值寄存器
#define BL0972_SAGLVL_LINECYC       0x8F // 跌落电压阈值寄存器



#define BL0972_VOLT_KP	     255620.3f     // 电压系数 - v
#define BL0972_CURR_KP		   1502.28f      // 电流系数 - a
#define BL0972_POWER_KP		   114472.8f     // 功率系数 - W
#define BL0972_ELEC_Ke		   0.000000334f  // 系数 - 度

#define BL0972_CMD_WRITE 		0x81
#define BL0972_CMD_READ    	0x82

/* 供外部调用的函数 */
void bl0972_init(void);

void bl0972_write_reg_function(uint8_t reg,uint8_t *data, uint8_t len);
uint8_t bl0972_read_reg_function(uint8_t reg, uint32_t *data);
void bl0972_run_timer_function(void);
void bl0972_work_process_function(void);
void bl0972_write_enable_function(uint8_t cmd);
void bl0972_set_mode1_function(void);
void bl0972_set_mode2_function(void);
void bl0972_set_gain_function(void);
void bl0972_set_gonghao_function(void);
void bl0972_reset_numreg_function(void);

void bl0972_test(void);

#endif
