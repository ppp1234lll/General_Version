#ifndef _BSP_UART6_H_
#define _BSP_UART6_H_

#include "./SYSTEM/sys/sys.h"

/* 提供给其他C文件调用的函数 */
void bsp_InitUart6_GPIO(void);
void bsp_InitUart6_Config(uint32_t bound);
void bsp_InitUart6_DMA(void);

/* 提供给其他C文件调用的函数 */
void bsp_InitUart6(uint32_t bound);
void uart6_send_str(uint8_t *buff, uint16_t len);
void uart6_dma_rx_enable(void);

uint8_t *uart6_rx_get_frame(void);

void uart6_test(void);
#endif
