#ifndef _BL0910_H_
#define _BL0910_H_

#include "./SYSTEM/sys/sys.h"

/* 寄存器表单 */
#define BL0910_I1_WAVE         0x01 // 通道 1 波形寄存器(正常电流和快速电流可选)
#define BL0910_I2_WAVE         0x02 // 通道 2 波形寄存器(正常电流和快速电流可选)
#define BL0910_I3_WAVE         0x03 // 通道 3 波形寄存器(正常电流和快速电流可选)
#define BL0910_I4_WAVE         0x04 // 通道 4 波形寄存器(正常电流和快速电流可选)
#define BL0910_I5_WAVE         0x05 // 通道 5 波形寄存器(正常电流和快速电流可选)
#define BL0910_I6_WAVE         0x06 // 通道 6 波形寄存器(正常电流和快速电流可选)
#define BL0910_I7_WAVE         0x07 // 通道 7 波形寄存器(正常电流和快速电流可选)
#define BL0910_I8_WAVE         0x08 // 通道 8 波形寄存器(正常电流和快速电流可选)
#define BL0910_I9_WAVE         0x09 // 通道 9 波形寄存器(正常电流和快速电流可选)
#define BL0910_I10_WAVE        0x0A // 通道 10 波形寄存器(正常电流和快速电流可选)
#define BL0910_V_WAVE          0x0B // 通道 11 波形寄存器
#define BL0910_I1_RMS          0x0C // 通道 1 有效值寄存器,无符号
#define BL0910_I2_RMS          0x0D // 通道 2 有效值寄存器,无符号
#define BL0910_I3_RMS          0x0E // 通道 3 有效值寄存器,无符号
#define BL0910_I4_RMS          0x0F // 通道 4 有效值寄存器,无符号
#define BL0910_I5_RMS          0x10 // 通道 5 有效值寄存器,无符号
#define BL0910_I6_RMS          0x11 // 通道 6 有效值寄存器,无符号
#define BL0910_I7_RMS          0x12 // 通道 7 有效值寄存器,无符号
#define BL0910_I8_RMS          0x13 // 通道 8 有效值寄存器,无符号
#define BL0910_I9_RMS          0x14 // 通道 9 有效值寄存器,无符号
#define BL0910_I10_RMS         0x15 // 通道 10 有效值寄存器,无符号
#define BL0910_V_RMS           0x16 // 通道 11 有效值寄存器,无符号
#define BL0910_I1_FAST_RMS     0x17 // 通道 1 快速有效值寄存器,无符号
#define BL0910_I2_FAST_RMS     0x18 // 通道 2 快速有效值寄存器,无符号
#define BL0910_I3_FAST_RMS     0x19 // 通道 3 快速有效值寄存器,无符号
#define BL0910_I4_FAST_RMS     0x1A // 通道 4 快速有效值寄存器,无符号
#define BL0910_I5_FAST_RMS     0x1B // 通道 5 快速有效值寄存器,无符号
#define BL0910_I6_FAST_RMS     0x1C // 通道 6 快速有效值寄存器,无符号
#define BL0910_I7_FAST_RMS     0x1D // 通道 7 快速有效值寄存器,无符号
#define BL0910_I8_FAST_RMS     0x1E // 通道 8 快速有效值寄存器,无符号
#define BL0910_I9_FAST_RMS     0x1F // 通道 9 快速有效值寄存器,无符号
#define BL0910_I10_FAST_RMS    0x20 // 通道 10 快速有效值寄存器,无符号
#define BL0910_V_FAST_RMS      0x21 // 通道 11 快速有效值寄存器,无符号
#define BL0910_WATT1_AP        0x22 // 通道 1 有功功率寄存器
#define BL0910_WATT2_AP        0x23 // 通道 2 有功功率寄存器
#define BL0910_WATT3_AP        0x24 // 通道 3 有功功率寄存器
#define BL0910_WATT4_AP        0x25 // 通道 4 有功功率寄存器
#define BL0910_WATT5_AP        0x26 // 通道 5 有功功率寄存器
#define BL0910_WATT6_AP        0x27 // 通道 6 有功功率寄存器
#define BL0910_WATT7_AP        0x28 // 通道 7 有功功率寄存器
#define BL0910_WATT8_AP        0x29 // 通道 8 有功功率寄存器
#define BL0910_WATT9_AP        0x2A // 通道 9 有功功率寄存器
#define BL0910_WATT10_AP       0x2B // 通道 10 有功功率寄存器
#define BL0910_WATT_ALL_AP     0x2C // 总有功功率寄存器
#define BL0910_VAR_SEL         0x2D // 可选通道无功功率寄存器（基波）
#define BL0910_VA              0x2E // 可选通道视在功率寄存器
#define BL0910_CF1_CNT         0x2F // 通道 1 有功脉冲计数，无符号
#define BL0910_CF2_CNT         0x30 // 通道 2 有功脉冲计数，无符号
#define BL0910_CF3_CNT         0x31 // 通道 3 有功脉冲计数，无符号
#define BL0910_CF4_CNT         0x32 // 通道 4 有功脉冲计数，无符号
#define BL0910_CF5_CNT         0x33 // 通道 5 有功脉冲计数，无符号
#define BL0910_CF6_CNT         0x34 // 通道 6 有功脉冲计数，无符号
#define BL0910_CF7_CNT         0x35 // 通道 7 有功脉冲计数，无符号
#define BL0910_CF8_CNT         0x36 // 通道 8 有功脉冲计数，无符号
#define BL0910_CF9_CNT         0x37 // 通道 9 有功脉冲计数，无符号
#define BL0910_CF10_CNT        0x38 // 通道 10 有功脉冲计数，无符号
#define BL0910_CF_CNT          0x39 // 总有功脉冲计数，无符号

#define BL0910_INT_STATUS1     0x54 // 中断状态寄存器 1
#define BL0910_M_STATUS3       0x56 // M状态寄存器 
#define BL0910_TPS1_IN         0x5E // 内部温度值寄存器
#define BL0910_TPS2_OUT        0x5F // 外部温度值寄存器
#define BL0910_GAIN1_REG       0x60 // 通道 PGA 增益调整寄存器0000=1 0001=2 0010=8 0011=16
			 								              // [3:0]->通道 11(电压) [7:4]->通道1 [8:11]->通道 2......
#define BL0910_GAIN2_REG       0x61 // 通道 PGA 增益调整寄存器0000=1 0001=2 0010=8 0011=16
				 									          // [3:0]->通道 11(电压) [7:4]->通道1 [8:11]->通道 2......
#define BL0910_SAGLVL_LINECYC  0x8F //  
#define BL0910_ADC_PD_CTRL     0x93 // 11 个通道 ADC 的使能控制
			 				// ADC_PD<0>控制电压通道; ADC_PD<10:1>控制对应的电流通道 10 到电流通道 1
#define BL0910_TPS_CTRL        0x94 // 温度选择
#define BL0910_MODE1_REG       0x96 // 用户模式选择寄存器 1
#define BL0910_MODE2_REG       0x97 // 用户模式选择寄存器 2
#define BL0910_MODE3_REG       0x98 // 用户模式选择寄存器 3
#define BL0910_MASK1_REG       0x9A // 中断屏蔽寄存器
#define BL0910_RST_ENG         0x9D // 能量读后清零设置寄存器
#define BL0910_US_WRPROT_REG   0x9E // 用户写保护设置寄存器
														        // 写入5555H时,表示可操作用户寄存器对 reg60 到 reg9d, rega0到d0
#define BL0910_SOFT_RESET_REG  0x9F // 当输入为 5A5A5A 时，系统复位
														        // 当输入为 55AA55 时，用户读写寄存器复位Reset: reg60 到reg9f, rega0 到 regd0
#define BL0910_VOLT_KP         12238.14f   		// 电压系数 - v
#define BL0910_CURR_KP         1197.128f     	// 电流系数 - ma
#define BL0910_POWER_KP        3493.84f     	// 功率系数 - W
#define BL0910_ELEC_Ke         0.00001093f    // 系数 - 度

/* 设置量 */
#define BL0910_GAIN_1          0x00 // 增益1
#define BL0910_GAIN_2          0x01 // 增益2
#define BL0910_GAIN_8          0x02 // 增益8
#define BL0910_GAIN_16         0x03 // 增益16

#define BL0910_CMD_WRITE       0x81
#define BL0910_CMD_READ        0x82

/* 提供给其他C文件调用的函数 */
void bl0910_init(void);

void bl0910_write_reg_function(uint8_t reg,uint8_t *data, uint8_t len);
uint8_t bl0910_read_reg_function(uint8_t reg, uint32_t *data);
void bl0910_run_timer_function(void);
void bl0910_work_process_function(void);
void bl0910_write_enable_function(uint8_t cmd);
void bl0910_set_gain_function(void);
void bl0910_set_ch_function(void);
void bl0910_set_mode_function(void);
void bl0910_set_eng_rst_function(void);
void bl0910_reset_numreg_function(void);

void bl0910_test(void);

#endif
