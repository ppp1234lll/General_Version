#ifndef _BSP_USART5_H_
#define _BSP_USART5_H_

#include "./SYSTEM/sys/sys.h"

/* 本文件内部函数声明 */
void bsp_InitUsart5_GPIO(void);
void bsp_InitUsart5_Config(uint32_t baudrate);
void bsp_InitUsart5_DMA(void);

/* 提供给其他C文件调用的函数 */
void bsp_InitUsart5(uint32_t baudrate);
void usart5_send_str(uint8_t *buff, uint16_t len);
void usart5_send_char(uint8_t ch);
void usart5_test(void);

#endif
