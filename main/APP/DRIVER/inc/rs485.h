
#ifndef __RS485_H_
#define __RS485_H_

#include "./SYSTEM/sys/sys.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"

// RS485配置结构体
typedef struct {
    uint32_t baudrate;        // 波特率
    uint8_t data_bits;        // 数据位长度
    uint8_t parity;           // 奇偶校验位 0-None, 1-Odd, 2-Even
    float stop_bits;          // 停止位长度
} rs485_config_t;

/* 提供给其他C文件调用的函数 */
void RS485_GPIO_Init(void);
void RS485_Init(uint32_t baudrate);
void RS485_ReConfig(rs485_config_t *config);
void rs485_send_str(uint8_t *data, uint16_t len);
void rs485_get_receive_data(uint8_t *data, uint16_t len);

void rs485_test(void);

#endif
