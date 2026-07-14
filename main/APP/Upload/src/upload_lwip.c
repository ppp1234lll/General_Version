#include "./Upload/inc/upload.h"
#include "./Upload/inc/upload_http.h"
#include "main.h"

#define UPLOAD_LWIP_TASK_PRIO           (9U)
#define UPLOAD_LWIP_TASK_STK            (3072U)
static TaskHandle_t s_upload_lwip_task = NULL;
static volatile uint8_t s_upload_lwip_exit_req = 0;  /* 1: 任务已停到安全点, 待外部同步删除 */

typedef upload_http_request_t upload_lwip_request_t;

/* 有线上传 TCP 连接句柄 */
static struct netconn *s_upload_lwip_tcp = NULL;

/* 前向声明: 由本文件后台任务在定义前调用 */
int8_t upload_lwip_file_function(void);

/* 传输层回调(注入 upload_http 内核) */
static int upload_lwip_transport_connect(const char *host, uint16_t port);
static int upload_lwip_transport_send(const uint8_t *data, uint32_t len);
static int upload_lwip_transport_recv(int *out_recv_size);
static void upload_lwip_transport_close(void);

static const upload_http_transport_t s_upload_lwip_transport =
{
    upload_lwip_transport_connect,
    upload_lwip_transport_send,
    upload_lwip_transport_recv,
    upload_lwip_transport_close,
};

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_transport_connect
*    功能说明: 按 IP 新建 TCP 连接(最多重试 3 次)。有线链路地址固定为 IP,不做域名解析。
*    形    参: host IP 字符串; port 端口
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_lwip_transport_connect(const char *host, uint16_t port)
{
    ip_addr_t server_ip = {0};
    err_t err = ERR_OK;
    uint8_t retry = 0U;

    if ((host == NULL) || (ipaddr_aton(host, &server_ip) != 1))
    {
        return -1;
    }

    for (retry = 0U; retry < 3U; retry++)
    {
        s_upload_lwip_tcp = netconn_new(NETCONN_TCP);
        if (s_upload_lwip_tcp == NULL)
        {
            vTaskDelay(50);
            continue;
        }

        err = netconn_connect(s_upload_lwip_tcp, &server_ip, port);
        if (err == ERR_OK)
        {
            s_upload_lwip_tcp->recv_timeout = 10;
            if (s_upload_lwip_tcp->pcb.tcp != NULL)
            {
                tcp_nagle_disable(s_upload_lwip_tcp->pcb.tcp);
            }
            return 0;
        }

        netconn_delete(s_upload_lwip_tcp);
        s_upload_lwip_tcp = NULL;
        vTaskDelay(50);
    }

    return -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_transport_send
*    功能说明: 阻塞写入整段数据(netconn_write 内部自动分段),并等待 unsent 队列清空,
*              避免大分片包只发出首段就去收应答。
*    形    参: data 数据; len 长度
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_lwip_transport_send(const uint8_t *data, uint32_t len)
{
    uint32_t wait = 0U;
    struct tcp_pcb *pcb = NULL;

    if ((s_upload_lwip_tcp == NULL) || (data == NULL) || (len == 0U))
    {
        return -1;
    }

    /* 阻塞写入,netconn_write 内部循环直至整段写入发送缓冲 */
    if (netconn_write(s_upload_lwip_tcp, data, (size_t)len, NETCONN_COPY) != ERR_OK)
    {
        return -1;
    }

    /* 等待 unsent 清空,确保整包已发出再收应答(最多约 500ms) */
    for (wait = 0U; wait < 100U; wait++)
    {
        pcb = s_upload_lwip_tcp->pcb.tcp;
        if (pcb == NULL)
        {
            return -1;
        }
        if (pcb->unsent == NULL)
        {
            return 0;
        }
        vTaskDelay(5);
    }

    return -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_transport_recv
*    功能说明: 接收一包 TCP 数据并经 upload_http_save_response 追加到应答缓冲。
*    形    参: out_recv_size 输出本次接收字节数
*    返 回 值: 0 成功(含暂无数据),-1 保存溢出/未连接,-3 链路断开。
*********************************************************************************************************
*/
static int upload_lwip_transport_recv(int *out_recv_size)
{
    struct netbuf *recvbuf = NULL;
    struct pbuf *q = NULL;
    err_t recv_err = ERR_OK;
    int total = 0;

    if (out_recv_size != NULL)
    {
        *out_recv_size = 0;
    }
    if (s_upload_lwip_tcp == NULL)
    {
        return -1;
    }

    recv_err = netconn_recv(s_upload_lwip_tcp, &recvbuf);
    if (recv_err == ERR_OK)
    {
        for (q = recvbuf->p; q != NULL; q = q->next)
        {
            if (upload_http_save_response((const uint8_t *)q->payload, (int)q->len) != 0)
            {
                netbuf_delete(recvbuf);
                return -1;
            }
            total += (int)q->len;
        }
        netbuf_delete(recvbuf);
        if (out_recv_size != NULL)
        {
            *out_recv_size = total;
        }
        return 0;
    }

    if (recv_err == ERR_TIMEOUT)
    {
        return 0; /* 暂无数据 */
    }

    return -3; /* ERR_CLSD 等,链路断开 */
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_transport_close
*    功能说明: 关闭有线上传 TCP 连接。
*    形    参: 无
*    返 回 值: 无。
*********************************************************************************************************
*/
static void upload_lwip_transport_close(void)
{
    if (s_upload_lwip_tcp != NULL)
    {
        netconn_close(s_upload_lwip_tcp);
        netconn_delete(s_upload_lwip_tcp);
        s_upload_lwip_tcp = NULL;
    }
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_task
*    功能说明: LwIP 上传后台任务入口,执行后自动删除任务。
*    形    参: pvParameters FreeRTOS 任务参数(未使用)
*    返 回 值: 无。
*********************************************************************************************************
*/
static void upload_lwip_task(void *pvParameters)
{
    (void)pvParameters;

    FeedFwdgt();
    (void)upload_lwip_file_function();
    if (upload_get_mode_function() != UPLOAD_MODE_NULL)
    {
        upload_set_upload_mode(UPLOAD_MODE_NULL);
    }
    /* 不自删除: 改为置退出请求并挂起, 由 eth 任务调用 upload_lwip_delete() 在其它任务
     * 上下文同步删除, 立即回收栈/TCB, 避免自删除延迟回收导致重建时内存不足 */
    s_upload_lwip_exit_req = 1;
    for(;;)
    {
        vTaskSuspend(NULL);
    }
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_task_create
*    功能说明: 创建 LwIP 上传后台任务。
*    形    参: 无
*    返 回 值: 无。
*********************************************************************************************************
*/
void upload_lwip_task_create(void)
{
    BaseType_t ret;

    if (upload_get_mode_function() != UPLOAD_MODE_LWIP)
    {
        return;
    }

    if (s_upload_lwip_exit_req)   /* 有任务待回收, 本轮先不创建 */
    {
        return;
    }

    if (s_upload_lwip_task != NULL)
    {
        return;
    }

    ret = xTaskCreate( upload_lwip_task,
                        "upload_lwip",
                        UPLOAD_LWIP_TASK_STK,
                        NULL,
                        UPLOAD_LWIP_TASK_PRIO,
                        &s_upload_lwip_task);
    if (ret != pdPASS)
    {
        s_upload_lwip_task = NULL;
    }
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_delete
*    功能说明: 回收已完成并停到安全点的有线上传任务; 须由 eth 任务等其它任务上下文周期调用,
*              以在外部上下文 vTaskDelete, 立即回收栈/TCB, 避免自删除延迟回收导致内存不足
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void upload_lwip_delete(void)
{
    TaskHandle_t h;

    if (s_upload_lwip_exit_req == 0)
    {
        return;
    }

    taskENTER_CRITICAL();
    h = s_upload_lwip_task;
    s_upload_lwip_task = NULL;
    s_upload_lwip_exit_req = 0;
    taskEXIT_CRITICAL();

    if (h != NULL)
    {
        vTaskDelete(h);
    }
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_file_function
*    功能说明: 有线上传入口: 取服务器配置、准备 pending 日志,经 lwIP 三步分片上传。
*              流程: get_server_config -> log_prepare_upload_function -> HTTP start/chunk/finish
*    形    参: 无
*    返 回 值: UPLOAD_HTTP_OK 或错误码。
*********************************************************************************************************
*/
int8_t upload_lwip_file_function(void)
{
    log_upload_bundle_t bundle;
    upload_lwip_request_t request = {0};
    int8_t ret;

    /* 前置检查: 网卡未就绪或当前处于 4G 模式,不走有线上传 */
    if ((g_lwipdev.netif_state == 0U) || (app_get_network_mode() == SERVER_MODE_GPRS))
    {
        upload_set_upload_mode(UPLOAD_MODE_NULL);
        return UPLOAD_HTTP_ERR_NET;
    }

    /* 准备待上传的 pending 日志批次(文件路径、文件名、分片数等) */
    ret = log_prepare_upload_function(&bundle);
    if (ret != LOG_OK)
    {
        /* 准备失败(如无 pending 日志/文件系统异常): 直接退出,不标记失败 */
        upload_set_upload_mode(UPLOAD_MODE_NULL);
        return UPLOAD_HTTP_ERR_FILE;
    }

    /* 批次为空: 无内容可传,标记本次失败后退出 */
    if (bundle.total_count == 0U)
    {
        upload_set_upload_mode(UPLOAD_MODE_NULL);
        (void)log_upload_fail_function();
        return UPLOAD_HTTP_ERR_FILE;
    }

    /* 填充上传请求: 上传文件名(服务器地址/大小由 upload_http 读取全局参数与日志流) */
    request.upload_file_name = bundle.log_file_name;

    /* 经 lwIP 传输层执行三步分片上传(start -> chunk -> finish) */
    ret = upload_http_file_function(&request, &s_upload_lwip_transport, NULL);

    /* 上传结束,清除上传模式 */
    upload_set_upload_mode(UPLOAD_MODE_NULL);

    /* 依据结果更新日志上传状态(成功/失败) */
    if (ret == UPLOAD_HTTP_OK)
    {
        (void)log_upload_success_function();
    }
    else
    {
        (void)log_upload_fail_function();
    }
    return ret;
}
