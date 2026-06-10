#ifndef _BSP_USART0_H_
#define _BSP_USART0_H_

#include "./SYSTEM/sys/sys.h"

/* 提供给其他C文件调用的函数 */
void bsp_InitUsart0_GPIO(void);
void bsp_InitUsart0_Config(uint32_t bound);
void bsp_InitUsart0_DMA(void);

/* 提供给其他C文件调用的函数 */
void bsp_InitUsart0(uint32_t bound);
void usart0_send_str(uint8_t *buff, uint16_t len);

#endif
