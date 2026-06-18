#ifndef __CONVERT_H__
#define __CONVERT_H__

#include "./SYSTEM/sys/sys.h"


/* º¯ÊıÉùÃ÷ */
uint32_t complement_to_original(uint32_t data);
int8_t hex_to_dec(char c);
uint8_t str_to_hex(uint8_t *buff);
int8_t float_to_str(float value,uint8_t precision,uint8_t *data,uint16_t len);

#endif
