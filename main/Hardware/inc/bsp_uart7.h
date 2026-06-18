#ifndef _BSP_UART7_H_
#define _BSP_UART7_H_

#include "./SYSTEM/sys/sys.h"

/* 提供给其他C文件调用的函数 */
void bsp_InitUart7_GPIO(void);
void bsp_InitUart7_Config(uint32_t bound);
void bsp_InitUart7_DMA(void);

/* 提供给其他C文件调用的函数 */
void bsp_InitUart7(uint32_t bound);
void uart7_send_str(uint8_t *buff, uint16_t len);
void uart7_dma_rx_enable(void);

uint8_t *uart7_rx_get_frame(void);

void uart7_test(void);
#endif
