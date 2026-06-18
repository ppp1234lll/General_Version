#ifndef _BSP_USART2_H_
#define _BSP_USART2_H_

#include "./SYSTEM/sys/sys.h"

/* 提供给其他C文件调用的函数 */
void bsp_InitUsart2_GPIO(void);
void bsp_InitUsart2_Config(uint32_t bound);
void bsp_InitUsart2_DMA(void);

/* 提供给其他C文件调用的函数 */
void bsp_InitUsart2(uint32_t bound);
void usart2_send_str(uint8_t *buff, uint16_t len);
void usart2_dma_rx_enable(void);

uint8_t *usart2_rx_get_frame(void);

void usart2_test(void);
#endif
