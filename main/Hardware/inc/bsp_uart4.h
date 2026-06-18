#ifndef _BSP_UART4_H_
#define _BSP_UART4_H_

#include "./SYSTEM/sys/sys.h"

/* 提供给其他C文件调用的函数 */
void bsp_InitUart4_GPIO(void);
void bsp_InitUart4_Config(uint32_t bound);
void bsp_InitUart4_DMA(void);

/* 提供给其他C文件调用的函数 */
void bsp_InitUart4(uint32_t bound);
void uart4_send_str(uint8_t *buff, uint16_t len);
void uart4_dma_rx_enable(void);

uint8_t *uart4_rx_get_frame(void);

void uart4_test(void);
#endif
