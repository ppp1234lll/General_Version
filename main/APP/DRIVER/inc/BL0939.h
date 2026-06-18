#ifndef _BL0939_H_
#define _BL0939_H_

#include "./SYSTEM/sys/sys.h"

/* 寄存器表单 */
#define BL0939_IA_FAST_RMS                  0x00 // A 通道快速有效值，无符号
#define BL0939_IA_WAVE                      0x01 // A 通道电流波形寄存器，有符号
#define BL0939_IB_WAVE                      0x02 // B 通道电流波形寄存器，有符号
#define BL0939_V_WAVE                       0x03 // 电压波形寄存器，有符号
#define BL0939_IA_RMS                       0x04 // A 通道电流有效值寄存器，无符号
#define BL0939_IB_RMS                       0x05 // B 通道电流有效值寄存器，无符号
#define BL0939_V_RMS                        0x06 // 电压有效值寄存器，无符号
#define BL0939_IB_FAST_RMS                  0x07 // B 通道快速有效值，无符号
#define BL0939_A_WATT                       0x08 // A 通道有功功率寄存器，有符号
#define BL0939_B_WATT                       0x09 // B 通道有功功率寄存器，有符号
#define BL0939_CFA_CNT                      0x0A // A 通道有功电能脉冲计数，无符号
#define BL0939_CFB_CNT                      0x0B // B 通道有功电能脉冲计数，无符号
#define BL0939_A_CORNER                     0x0C // A 通道电流电压波形相角寄存器
#define BL0939_B_CORNER                     0x0D // B 通道电流电压波形相角寄存器
#define BL0939_TPS1                         0x0E // 内部温度检测寄存器，无符号
#define BL0939_TPS2                         0x0F // 外部温度检测寄存器，无符号
#define BL0939_IA_FAST_RMS_CTRL             0x10 // A 通道快速有效值控制寄存器

#define BL0939_IA_RMSOS                     0x13 // 电流 A 通道有效值小信号校正寄存器
#define BL0939_IB_RMSOS                     0x14 // 电流 B 通道有效值小信号校正寄存器
#define BL0939_A_WATTOS                     0x15 // A 通道有功功率小信号校正寄存器
#define BL0939_B_WATTOS                     0x16 // B 通道有功功率小信号校正寄存器
#define BL0939_WA_CREEP                     0x17 // 有功功率防潜寄存器
#define BL0939_MODE                         0x18 // 用户模式选择寄存器
#define BL0939_SOFT_RESET                   0x19 // 写入 0x5A5A5A 时，用户区寄存器复位
#define BL0939_USR_WRPROT                   0x1A // 用户写保护设置寄存器。写入 0x55 后，用户操作寄存器可以写入；写入其他值，用户操作寄存器区域不可写入
#define BL0939_TPS_CTRL                     0x1B // 温度模式控制寄存器
#define BL0939_TPS2_A                       0x1C // 外部温度传感器增益系数校正寄存器
#define BL0939_TPS2_B                       0x1D // 外部温度传感器偏移系数校正寄存器
#define BL0939_IB_FAST_RMS_CTRL             0x1E // B 通道快速有效值控制寄存器

#define BL0939_VOLT_KP                      16340.574f          // 电压系数 - v
#define BL0939_CURR_KP                      19950.985f          // 电流系数 - ma
#define BL0939_POWER_KP                     42443.42f           // 功率系数 - W
#define BL0939_ELEC_Ke                      0.00000228f         // 系数 - 度

#define BL0939_CMD_WRITE                    0xA5
#define BL0939_CMD_READ                     0x55


/* 供外部调用的函数接口 */
void bl0939_init(void);

void bl0939_write_reg_function(uint8_t reg,uint8_t *data, uint8_t len);
uint8_t bl0939_read_reg_function(uint8_t reg, uint32_t *data);
void bl0939_run_timer_function(void);
void bl0939_work_process_function(void);
void bl0939_write_enable_function(uint8_t cmd);
void bl0939_set_mode_function(void);
void bl0939_set_WA_CREEP_function(void);
void bl0939_reset_numreg_function(void);

void bl0939_test(void);


#endif
