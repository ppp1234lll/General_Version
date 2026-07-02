#ifndef _GPRS_RX_H_
#define _GPRS_RX_H_

#include "./SYSTEM/sys/sys.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "semphr.h"

/* 流缓冲区容量:可容纳多个DMA帧,为HTTP大块数据预留余量 */
#define GPRS_RX_STREAMBUF_SIZE (4096)
/* 须高于gsm任务(优先级8),保证接收数据在gsm改cmdon前被处理 */
#define GPRS_RX_TASK_PRIO      (9)
/* 任务栈大小(字) */
#define GPRS_RX_STK_SIZE       (1024)

/* GPRS串口接收流缓冲区句柄:
 * 串口中断将DMA收到的字节流写入此处,由 gprs_rx 任务取出解析,避免中断内做耗时处理 */
extern StreamBufferHandle_t g_gprs_rx_streambuf;

/* 保护 gprs_rx_buff / gprs_rx_status / gprs_rx_take_point 的多任务互斥锁
 * gprs_rx任务(优先级9)写入, gsm任务(优先级8)读取, 必须互斥 */
extern SemaphoreHandle_t g_gprs_rx_mutex;

/* 创建流缓冲区与接收处理任务(须在使能串口接收前调用,仅创建一次) */
void gprs_rx_streambuf_init_function(void);

#endif
