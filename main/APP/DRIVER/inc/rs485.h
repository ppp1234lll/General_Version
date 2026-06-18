
#ifndef __RS485_H_
#define __RS485_H_

#include "./SYSTEM/sys/sys.h"

/* 提供给其他C文件调用的函数 */
void RS485_GPIO_Init(void);
void RS485_Init(uint32_t baudrate);
void rs485_send_char(uint8_t ch);
void rs485_send_str(uint8_t *data, uint16_t len);

void rs485_test(void);

#endif
