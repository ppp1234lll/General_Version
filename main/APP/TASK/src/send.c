#include "main.h"
#include "./Task/inc/send.h"

/* ============================================================
 * 内部类型与常量
 * ============================================================ */

/* 发送任务通知值 */
#define SEND_NOTIFY_RESULT_MASK  0x0F  /* 结果通知（低4位存 send_result_e） */

/* 发送任务优先级（介于 APP=16 和 ETH=13 之间） */
#define SEND_TASK_PRIO  15
#define SEND_STK_SIZE   1024

/* ============================================================
 * 模块内部变量
 * ============================================================ */

static TaskHandle_t       g_send_task_handle = NULL;
static QueueHandle_t      g_high_q   = NULL;
static QueueHandle_t      g_normal_q = NULL;
static QueueSetHandle_t   g_qset     = NULL;
static send_msg_t         g_msg_pool[SEND_MSG_POOL_NUM];
static send_msg_t        *g_sending  = NULL;    /* 当前正在发送的消息 */

/* ============================================================
 * 内部函数声明
 * ============================================================ */
static void send_task(void *pv);
static void send_msg_free(send_msg_t *msg);
static int  send_network_available(send_msg_t *msg);
static void send_handle_fail(send_msg_t *msg);
static void send_data_task_function(send_msg_t *msg);
/*
*********************************************************************************************************
*    函 数 名: send_task
*    功能说明: 发送任务主循环
*
*    工作流程：
*      1. 从队列集合中取出下一条消息（高优先出队）
*      2. 检查网络状态
*         - 网络不通：消息放回队首，等待 500ms 后重试
*      3. 调用底层发送（直接传递 msg->buf，零拷贝）
*      4. 阻塞等待发送结果通知（带超时）
*         - 成功：释放消息，处理下一条
*         - 失败/超时：handle_send_fail 决定重试或丢弃
*    形    参: pv       未使用
*    返 回 值: 无
*********************************************************************************************************
*/
static void send_task(void *pv)
{
    send_msg_t *msg = NULL;
    QueueSetMemberHandle_t active_q;
    uint32_t elapsed;
    uint32_t remain;
    TickType_t wait;
    uint32_t notify_val;

    (void)pv;

    for (;;) 
    {
        /* ---- 阶段1：取出下一条待发送消息 ---- */
        if (g_sending == NULL) 
        {
            msg = NULL;

            /* 通过队列集合选择：高优队列优先 */
            active_q = xQueueSelectFromSet(g_qset, pdMS_TO_TICKS(200));
            if (active_q != NULL) {
                xQueueReceive((QueueHandle_t)active_q, &msg, 0);
            }

            if (msg == NULL) continue;

            /* ---- 阶段2：检查网络 ---- */
            if (!send_network_available(msg)) 
            {
                /* 网络不通，放回队首 */
                QueueHandle_t q = (msg->prio == MSG_PRIO_HIGH) ? g_high_q : g_normal_q;
                xQueueSendToFront(q, &msg, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }

            /* ---- 阶段3：发送 ---- */
            g_sending       = msg;
            msg->send_tick  = HAL_GetTick();
            send_data_task_function(msg);
        }

        /* ---- 阶段4：等待发送结果 ---- */
        elapsed    = HAL_GetTick() - g_sending->send_tick;
        remain     = (elapsed < SEND_TIMEOUT_MS) ? (SEND_TIMEOUT_MS - elapsed) : 1;
        wait       = pdMS_TO_TICKS(remain);
        notify_val = 0;

        if (xTaskNotifyWait(0, 0xFFFFFFFF, &notify_val, wait) == pdTRUE) {
            /* 收到结果通知 */
            if ((send_result_e)notify_val == SR_OK) {
                send_msg_free(g_sending);
            } else {
                send_handle_fail(g_sending);
            }
        } else {
            /* 超时 */
            send_handle_fail(g_sending);
        }
    }
}

/* ============================================================
 * 公开 API
 * ============================================================ */

/*
*********************************************************************************************************
*    函 数 名: send_task_init
*    功能说明: 初始化发送模块（消息池、队列、发送任务）
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void send_task_init(void)
{
    /* 初始化消息池 */
    memset(g_msg_pool, 0, sizeof(g_msg_pool));

    /* 创建高优队列 */
    g_high_q = xQueueCreate(SEND_HIGH_Q_DEPTH, sizeof(send_msg_t *));
    configASSERT(g_high_q != NULL);

    /* 创建普通队列 */
    g_normal_q = xQueueCreate(SEND_NORMAL_Q_DEPTH, sizeof(send_msg_t *));
    configASSERT(g_normal_q != NULL);

    /* 创建队列集合（高优先被 select） */
    g_qset = xQueueCreateSet(SEND_HIGH_Q_DEPTH + SEND_NORMAL_Q_DEPTH);
    configASSERT(g_qset != NULL);

    xQueueAddToSet(g_high_q,   g_qset);
    xQueueAddToSet(g_normal_q, g_qset);

    /* 创建发送任务 */
    xTaskCreate((TaskFunction_t)send_task,
                (const char *)"send_task",
                (uint16_t)SEND_STK_SIZE,
                (void *)NULL,
                (UBaseType_t)SEND_TASK_PRIO,
                (TaskHandle_t *)&g_send_task_handle);

    printf("send task init ok, pool=%d high_q=%d normal_q=%d\n",
            SEND_MSG_POOL_NUM, SEND_HIGH_Q_DEPTH, SEND_NORMAL_Q_DEPTH);
}

/*
*********************************************************************************************************
*    函 数 名: send_msg_alloc
*    功能说明: 从消息池分配一条消息
*    形    参: prio          优先级
*              cmd          命令字
*              retries      重试次数
*    返 回 值: 消息指针，NULL 表示池满
*********************************************************************************************************
*/
send_msg_t *send_msg_alloc(send_prio_t prio, uint8_t cmd,uint8_t retries)
{
    for (int i = 0; i < SEND_MSG_POOL_NUM; i++) {
        if (!g_msg_pool[i].in_use) {
            send_msg_t *m = &g_msg_pool[i];
            memset(m->buf, 0, SEND_MSG_BUF_SIZE);
            m->size         = 0;
            m->cmd          = cmd;
            m->prio         = prio;
            m->retries_left = retries;
            m->send_tick    = 0;
            m->dst          = SEND_DST_AUTO;
            m->in_use       = 1;
            return m;
        }
    }
    return NULL;
}

/*
*********************************************************************************************************
*    函 数 名: send_msg_enqueue
*    功能说明: 将已组包的消息推入发送队列
*    形    参: msg      消息指针（所有权转移给发送任务）
*    返 回 值: 0=成功  -1=队列满
*********************************************************************************************************
*/
int send_msg_enqueue(send_msg_t *msg)
{
    QueueHandle_t q;
    BaseType_t    ret;

    if (msg == NULL) return -1;

    q = (msg->prio == MSG_PRIO_HIGH) ? g_high_q : g_normal_q;
    ret = xQueueSend(q, &msg, 0);

    return (ret == pdPASS) ? 0 : -1;
}

/*
*********************************************************************************************************
*    函 数 名: send_task_notify_result
*    功能说明: 发送结果通知（由 app_set_send_result_function 调用）
*    形    参: result    发送结果（send_result_e 的值）
*    返 回 值: 无
*********************************************************************************************************
*/
void send_task_notify_result(uint8_t result)
{
    if (g_send_task_handle != NULL) {
        xTaskNotify(g_send_task_handle,
                    (uint32_t)(result & SEND_NOTIFY_RESULT_MASK),
                    eSetValueWithOverwrite);
    }
}

/* ============================================================
 * 内部函数
 * ============================================================ */

/*
*********************************************************************************************************
*    函 数 名: send_network_available
*    功能说明: 检测当前通信通道的网络是否可用
*    形    参: 无
*    返 回 值: 1=可用  0=不可用
*********************************************************************************************************
*/
static uint8_t send_msg_channel(const send_msg_t *msg)
{
    /* dst 有效则原路返回(按收到指令的通道), 否则自动探测当前在线网口 */
    if (msg != NULL && msg->dst != SEND_DST_AUTO)
        return msg->dst;

    /* 优先有线, 无线在线时降级判断 */
#ifdef WIRED_PRIORITY_CONNECTION
    if (eth_get_tcp_status() == 2)
        return 0;
    if (gsm_get_network_connect_status_function() == 1)
        return 1;
#else
    if (gsm_get_network_connect_status_function() == 1)
        return 1;
    if (eth_get_tcp_status() == 2)
        return 0;
#endif
    return 0;   /* 都不在线, 默认有线(让下层去处理超时/重连) */
}

static int send_network_available(send_msg_t *msg)
{
    /* 通道: 0=LWIP/TCP, 非0=GPRS */
    if (send_msg_channel(msg) == 0) {
        return (eth_get_tcp_status() == 2);
    } else {
        return (gsm_get_network_connect_status_function() == 1);
    }
}

/*
*********************************************************************************************************
*    函 数 名: send_msg_free
*    功能说明: 释放消息回到消息池
*    形    参: msg      消息指针
*    返 回 值: 无
*********************************************************************************************************
*/
static void send_msg_free(send_msg_t *msg)
{
    if (msg != NULL) {
        msg->in_use = 0;
    }
    g_sending = NULL;
}

/*
*********************************************************************************************************
*    函 数 名: handle_send_fail
*    功能说明: 处理发送失败（错误或超时）
*    形    参: msg      失败的消息
*    返 回 值: 无
*********************************************************************************************************
*/
static void send_handle_fail(send_msg_t *msg)
{
    if (msg == NULL) return;

    if (msg->retries_left > 0) {
        /* 还有重试次数：重新入队 */
        msg->retries_left--;
        send_msg_enqueue(msg);
    } else {
        /* 重试耗尽 */
        send_msg_free(msg);
    }
}
/*
*********************************************************************************************************
*    函 数 名: send_data_task_function
*    功能说明: 发送数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
static void send_data_task_function(send_msg_t *msg)
{
    if(send_msg_channel(msg) == 0)
        tcp_set_send_buff(msg->buf, msg->size);
    else
        gsm_send_tcp_data(msg->buf, msg->size);
}
