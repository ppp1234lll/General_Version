#ifndef _GPRS_RX_H_
#define _GPRS_RX_H_

#include "./SYSTEM/sys/sys.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

/* 流缓冲区容量:可容纳多个DMA帧,为HTTP大块数据预留余量 */
#define GPRS_RX_STREAMBUF_SIZE (4096)
/* 队列深度:DMA每帧一个通知,32帧足以覆盖最坏情况 */
#define GPRS_RX_QUEUE_SIZE     (32)
/* ISR→任务数据缓冲区:与 DMA 缓冲区等大 */
#define GPRS_RX_DATA_BUFF_SIZE (2048)

/* GPRS接收队列句柄: ISR投递数据长度通知, gprs_rx任务取出处理 */
extern QueueHandle_t g_gprs_rx_queue;

/* ISR → 任务通知接口: 拷贝DMA数据到内部缓冲区并通知任务(仅在ISR上下文调用) */
BaseType_t gprs_rx_notify_from_isr(const uint8_t *data, uint16_t len,
                                    BaseType_t *pxHigherPriorityTaskWoken);

/* 创建队列与接收处理任务(须在使能串口接收前调用,仅创建一次) */
void gprs_rx_queue_init_function(void);

#endif
