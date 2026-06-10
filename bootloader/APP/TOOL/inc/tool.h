#ifndef _TOOL_H_
#define _TOOL_H_

#include "./SYSTEM/sys/sys.h"

/* 参数 */

/* 函数声明 */
uint32_t complement_to_original(uint32_t data);
int8_t hex_to_dec(char c);
uint8_t str_to_hex(uint8_t *buff);

void PrecisionHandle(float *value,unsigned char Wrange);
signed char FloatToString(float value,uint8_t int_width,uint8_t Wrange,uint8_t * data,uint8_t len);



#endif
