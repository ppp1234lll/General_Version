#ifndef _SEND_H_
#define _SEND_H_

#include <stdint.h>

/* ============================================================
 * 发送消息模块 - 独立发送任务 + 优先级队列 + 消息池
 *
 * 每条消息自带独立 buffer，组包直接写入 msg->buf。
 * 生产者：alloc → 组包 → enqueue
 * 消费者：发送任务事件驱动，高优优先出队，消息级独立重试
 * ============================================================ */

#define SEND_MSG_BUF_SIZE   2048       /* 消息 buffer 大小 */
#define SEND_MSG_POOL_NUM   8          /* 消息池容量（可同时存在的消息数上限） */
#define SEND_HIGH_Q_DEPTH   4          /* 高优先队列深度 */
#define SEND_NORMAL_Q_DEPTH 8          /* 普通队列深度 */
#define SEND_RETRY_MAX      3          /* 默认最大重试次数 */
#define SEND_TIMEOUT_MS     5000       /* 单次发送超时时间(ms) */

/* -------------------------------------------------------
 * 优先级
 * ------------------------------------------------------- */
typedef enum {
    MSG_PRIO_HIGH   = 0,   /* ACK / 控制回复 / 立即上报 / 查询结果 */
    MSG_PRIO_NORMAL = 1,   /* 心跳 / 周期上报 */
} send_prio_t;

/* -------------------------------------------------------
 * 消息结构体
 * ------------------------------------------------------- */
/* dst 目标通道: 0=LWIP/TCP, 1=GPRS(与 com_channel_e/return_src 一致); 0xFF=自动探测在线网口 */
#define SEND_DST_AUTO       0xFF

typedef struct send_msg {
    uint8_t         buf[SEND_MSG_BUF_SIZE];   /* 消息数据缓冲区 */
    uint16_t        size;                     /* 有效数据长度 */
    uint8_t         cmd;                      /* 命令字 */
    send_prio_t     prio;                     /* 优先级 */
    uint8_t         retries_left;             /* 剩余重试次数 */
    uint32_t        send_tick;               /* 发送时刻(HAL_GetTick) */
    uint8_t         in_use;                   /* 消息池管理：1=占用 */
    uint8_t         dst;                      /* 目标通道: SEND_DST_AUTO/0/1, 支持ACK原路返回 */
} send_msg_t;

/* ============================================================
 * 公开 API
 * ============================================================ */

/* 初始化：创建消息池、优先级队列、发送任务（在 start_task 中调用） */
void send_task_init(void);

/* 从消息池分配一条消息
 *   prio    : 优先级
 *   cmd     : 命令字
 *   retries : 重试次数（0 = 不重试）
 * 返回 NULL 表示池满，调用者丢弃本消息。
 */
send_msg_t *send_msg_alloc(send_prio_t prio, uint8_t cmd,uint8_t retries);

/* 将已组包的消息推入发送队列（转移所有权给发送任务）
 * 返回 0=成功，-1=队列满
 */
int send_msg_enqueue(send_msg_t *msg);

/* 发送结果通知（由 app_set_send_result_function 调用）
 * 发送任务阻塞等待此通知，收到后继续处理下一条消息。
 */
void send_task_notify_result(uint8_t result);

#endif
