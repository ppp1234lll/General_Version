#ifndef _BSP_USART1_H_
#define _BSP_USART1_H_

#include "./SYSTEM/sys/sys.h"

/* 提供给其他C文件调用的函数 */
void bsp_InitUsart1(uint32_t baudrate);
void usart1_send_str(uint8_t *buff, uint16_t len);

void bsp_InitUsart1_GPIO(void);
void bsp_InitUsart1_Config(uint32_t baudrate);
void bsp_InitUsart1_DMA(void);

#endif
