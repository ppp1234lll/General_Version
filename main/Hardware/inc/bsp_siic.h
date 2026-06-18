#ifndef __BSP_SIIC_H_
#define __BSP_SIIC_H_

#include "./SYSTEM/sys/sys.h"

//IIC所有操作函数
void    bsp_init_siic(void);  // 初始化IIC的IO口        
void    siic_start(void); // 发送IIC开始信号
void    siic_stop(void);  // 发送IIC停止信号

uint8_t siic_wait_ack(void);
void siic_write_byte(uint8_t dat);
uint8_t siic_read_byte(uint8_t ack);

#endif
