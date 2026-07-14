#include "main.h"
#include "./Task/inc/gprs_rx.h"

#define GPRS_RX_TASK_PRIO      (15)
#define GPRS_RX_STK_SIZE       (1024)
TaskHandle_t s_gprs_rx_task_handler = NULL;   

/* 队列句柄 */
QueueHandle_t g_gprs_rx_queue = NULL;

/* ISR → 任务数据缓冲区 */
uint8_t  g_gprs_rx_data_buff[GPRS_RX_DATA_BUFF_SIZE] = {0};
uint16_t g_gprs_rx_data_len = 0;

/* 声明 */
static void gprs_rx_task_function(void *pvParameters);

/*
*********************************************************************************************************
*    函数名称: gprs_rx_queue_init_function
*    功能说明: 初始化GPRS接收队列及处理任务
*             仅创建一次;在串口接收使能之前调用,确保各句柄已准备好
*    参数    : 无
*    返回值  : 无
*********************************************************************************************************
*/
void gprs_rx_queue_init_function(void)
{
    g_gprs_rx_queue = xQueueCreate(GPRS_RX_QUEUE_SIZE, sizeof(uint16_t));
    configASSERT(g_gprs_rx_queue);

    xTaskCreate((TaskFunction_t  )gprs_rx_task_function,
                (const char *    )"gprs_rx_task",
                (uint16_t        )GPRS_RX_STK_SIZE,
                (void *          )NULL,
                (UBaseType_t     )GPRS_RX_TASK_PRIO,
                (TaskHandle_t *  )&s_gprs_rx_task_handler);
}

/*
*********************************************************************************************************
*    函数名称: gprs_rx_notify_from_isr
*    功能说明: ISR 将 DMA 数据拷贝到内部缓冲区并通过队列通知任务
*             封装内部变量, 调用方无需直接访问 g_gprs_rx_data_buff / g_gprs_rx_data_len
*    参数    : data : 数据指针, len : 长度, pxHigherPriorityTaskWoken : 上下文切换标志
*    返回值  : pdTRUE 成功, pdFALSE 参数无效
*********************************************************************************************************
*/
BaseType_t gprs_rx_notify_from_isr(const uint8_t *data, uint16_t len,
                                    BaseType_t *pxHigherPriorityTaskWoken)
{
    if (g_gprs_rx_queue == NULL || data == NULL
        || len == 0U || len > GPRS_RX_DATA_BUFF_SIZE)
    {
        return pdFALSE;
    }

    memcpy(g_gprs_rx_data_buff, data, len);
    g_gprs_rx_data_len = len;

    return xQueueSendFromISR(g_gprs_rx_queue, &g_gprs_rx_data_len,
                            pxHigherPriorityTaskWoken);
}

/*
*********************************************************************************************************
*    函数名称: gprs_rx_task_function
*    功能说明: GPRS接收帧解析任务
*    参数    : pvParameters : 未使用
*    返回值  : 无
*********************************************************************************************************
*/
static void gprs_rx_task_function(void *pvParameters)
{
    uint16_t rx_len = 0;

    for(;;)
    {
        /* 阻塞等待 ISR 投递的数据长度通知 */
        if (xQueueReceive(g_gprs_rx_queue, &rx_len, portMAX_DELAY) == pdTRUE)
        {
            if (rx_len > 0 && rx_len <= GPRS_RX_DATA_BUFF_SIZE)
            {
                gprs_get_receive_data_function(g_gprs_rx_data_buff, rx_len);
            }
        }
    }
}

