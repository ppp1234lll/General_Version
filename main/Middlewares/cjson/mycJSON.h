#ifndef _MYCJSON_H_
#define _MYCJSON_H_

#include "./SYSTEM/sys/sys.h"

int8_t my_cjson_create_function(uint8_t *buff, uint8_t mode);
int8_t my_cjson_info_create_function(uint8_t *buff, uint8_t mode);
int8_t my_cjson_data_create_function(uint8_t *buff, uint8_t mode);
int8_t my_cjson_join_string_function(uint8_t *buff,uint8_t *join_t,uint8_t *join_d, uint8_t next);
int8_t my_cjson_join_int_function(uint8_t *buff,uint8_t *join_t,int32_t number, uint8_t next);

#endif
