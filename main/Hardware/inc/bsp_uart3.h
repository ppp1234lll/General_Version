#ifndef _BSP_UART3_H_
#define _BSP_UART3_H_

#include "./SYSTEM/sys/sys.h"

/* 本文件内部函数 */
void bsp_InitUart3_GPIO(void);
void bsp_InitUart3_Config(uint32_t bound);
void bsp_InitUart3_DMA(void);

/* 提供给其他C文件调用的函数 */
void bsp_InitUart3(uint32_t bound);
void uart3_disable_dma(void);
void uart3_enable_dma(void);
void uart3_send_str(uint8_t *buff, uint16_t len);
void uart3_dma_rx_enable(void);

uint8_t *uart3_rx_get_frame(void);

void uart3_test(void);
#endif
