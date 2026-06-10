#ifndef AHT20_DRV_H_
#define AHT20_DRV_H_

#include "./SYSTEM/sys/sys.h"

//IIC所有操作函数
void    aht20_i2c_init(void);  // 初始化IIC的IO口		
void    aht20_i2c_start(void); // 发送IIC开始信号
void    aht20_i2c_stop(void);  // 发送IIC停止信号

uint8_t aht20_i2c_wait_ack(void);
void aht20_drive_write_byte(uint8_t dat);
uint8_t aht20_drive_read_byte(uint8_t ack);


#endif
