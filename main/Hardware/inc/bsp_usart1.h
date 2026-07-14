#ifndef _BSP_USART1_H_
#define _BSP_USART1_H_

#include "./SYSTEM/sys/sys.h"

/* 提供给其他C文件调用的函数 */
void bsp_InitUsart1_GPIO(void);
void bsp_InitUsart1_Config(uint32_t bound);
void bsp_InitUsart1_DMA(void);

/* 提供给其他C文件调用的函数 */
void bsp_InitUsart1(uint32_t bound);
void usart1_disable_dma(void);
void usart1_enable_dma(void);
void usart1_send_str(uint8_t *buff, uint32_t len);
void usart1_dma_rx_enable(void);

uint8_t *usart1_rx_get_frame(void);

void usart1_test(void);
#endif
