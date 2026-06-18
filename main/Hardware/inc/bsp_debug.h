#ifndef _BSP_DEBUG_H_
#define _BSP_DEBUG_H_

#include "./SYSTEM/sys/sys.h"

/* 本文件内部函数声明 */
void bsp_InitDebug_GPIO(void);
void bsp_InitDebug_Config(uint32_t baudrate);

/* 提供给其他C文件调用的函数 */
void bsp_InitDebug(uint32_t baudrate);
void debug_send_str(uint8_t *buff, uint16_t len);
void debug_send_char(uint8_t ch);
void debug_test(void);

#endif
