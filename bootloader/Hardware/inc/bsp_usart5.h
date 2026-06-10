#ifndef _BSP_USART5_H_
#define _BSP_USART5_H_

#include "./SYSTEM/sys/sys.h"

void bsp_InitUsart5(uint32_t baudrate);
void Usart5_Send_Str(uint8_t *buff, uint16_t len);
void usart5_test(void);

#endif
