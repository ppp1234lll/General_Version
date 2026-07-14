#include "./Upload/inc/upload.h"
#include "./Upload/inc/upload_http.h"
#include "main.h"

#define UPLOAD_GSM_TASK_PRIO            (9U)
#define UPLOAD_GSM_TASK_STK             (3072U)
static TaskHandle_t s_upload_gsm_task = NULL;
static volatile uint8_t s_upload_gsm_exit_req = 0;  /* 1: 任务已停到安全点, 待外部同步删除 */

/* 无线上传分片大小,与 upload_http 保持一致(1KB) */
#define UPLOAD_GSM_CHUNK_SIZE             UPLOAD_HTTP_CHUNK_SIZE

typedef upload_http_request_t upload_gprs_request_t;

/* 前向声明: 由本文件后台任务在定义前调用 */
int8_t upload_gsm_file_function(void);

/* GPRS 单次 MIPSEND 最大分段(超过则拆分多次发送) */
#define UPLOAD_GSM_SEND_SEG              1024U
/* GPRS 单次 MIPSEND 等待应答超时(ms) */
#define UPLOAD_GSM_SEND_TIMEOUT_MS       5000


/* 传输层回调(注入 upload_http 内核) */
static int upload_gsm_transport_connect(const char *host, uint16_t port);
static int upload_gsm_transport_send(const uint8_t *data, uint32_t len);
static int upload_gsm_transport_recv(int *out_recv_size);
static void upload_gsm_transport_close(void);

static const upload_http_transport_t s_upload_gsm_transport =
{
    upload_gsm_transport_connect,
    upload_gsm_transport_send,
    upload_gsm_transport_recv,
    upload_gsm_transport_close,
};

/*
*********************************************************************************************************
*    函 数 名: upload_gsm_task
*    功能说明: GPRS 上传后台任务入口，执行后自动删除任务
*    形    参: pvParameters FreeRTOS 任务参数(未使用)
*    返 回 值: 无
*********************************************************************************************************
*/
static void upload_gsm_task(void *pvParameters)
{
    (void)pvParameters;

    FeedFwdgt();
    (void)upload_gsm_file_function();
    if (upload_get_mode_function() != UPLOAD_MODE_NULL)
    {
        upload_set_upload_mode(UPLOAD_MODE_NULL);
    }
    /* 不自删除: 改为置退出请求并挂起, 由 gsm 任务调用 upload_gsm_delete() 在其它任务
     * 上下文同步删除, 立即回收栈/TCB, 避免自删除延迟回收导致重建时内存不足 */
    s_upload_gsm_exit_req = 1;
    for(;;)
    {
        vTaskSuspend(NULL);
    }
}

/*
*********************************************************************************************************
*    函 数 名: upload_gsm_task_create
*    功能说明: 无线上传后台任务创建
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void upload_gsm_task_create(void)
{
    BaseType_t ret;

    if (upload_get_mode_function() != UPLOAD_MODE_GPRS)
    {
        return;
    }

    if (s_upload_gsm_exit_req)   /* 有任务待回收, 本轮先不创建 */
    {
        return;
    }

    if (s_upload_gsm_task != NULL)
    {
        return;
    }

    ret = xTaskCreate(upload_gsm_task,
                      "upload_gprs",
                      UPLOAD_GSM_TASK_STK,
                      NULL,
                      UPLOAD_GSM_TASK_PRIO,
                      &s_upload_gsm_task);
    if (ret != pdPASS)
    {
        s_upload_gsm_task = NULL;
    }
}

/*
*********************************************************************************************************
*    函 数 名: upload_gsm_delete
*    功能说明: 回收已完成并停到安全点的无线上传任务; 须由 gsm 任务等其它任务上下文周期调用,
*              以在外部上下文 vTaskDelete, 立即回收栈/TCB, 避免自删除延迟回收导致内存不足
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void upload_gsm_delete(void)
{
    TaskHandle_t h;

    if (s_upload_gsm_exit_req == 0)
    {
        return;
    }

    taskENTER_CRITICAL();
    h = s_upload_gsm_task;
    s_upload_gsm_task = NULL;
    s_upload_gsm_exit_req = 0;
    taskEXIT_CRITICAL();

    if (h != NULL)
    {
        vTaskDelete(h);
    }
}


/*
*********************************************************************************************************
*    函 数 名: upload_gsm_transport_connect
*    功能说明: 通过 GPRS FILE 链路连接服务器(先断旧连接,最多重试 3 次)。
*    形    参: host 主机名或 IP; port 端口
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_gsm_transport_connect(const char *host, uint16_t port)
{
    uint8_t retry = 0U;

    if (host == NULL)
        return -1;

    gprs_network_disconnect_function(GPRS_LINK_FILE);
    for (retry = 0U; retry < 3U; retry++)
    {
        if (gprs_network_connect_server((uint8_t *)host, port, GPRS_LINK_FILE) == GPRS_SEND_OK)
        {
            return 0;
        }
        vTaskDelay(50);
    }
    return -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_gsm_transport_send
*    功能说明: 通过 GPRS FILE 链路发送整段数据,超过单次上限时拆分为多次 MIPSEND。
*    形    参: data 数据; len 长度
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_gsm_transport_send(const uint8_t *data, uint32_t len)
{
    uint32_t offset = 0U;
    uint32_t seg = 0U;

    if (data == NULL)
        return -1;

    if (len == 0U)
        return 0;

    while (offset < len)
    {
        seg = len - offset;
        if (seg > UPLOAD_GSM_SEND_SEG)
        {
            seg = UPLOAD_GSM_SEND_SEG;
        }

        if (gprs_send_data((uint8_t *)(data + offset), (int)seg,
                            GPRS_LINK_FILE, UPLOAD_GSM_SEND_TIMEOUT_MS) != GPRS_SEND_OK)
        {
            return -1;
        }
        offset += seg;
    }
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_gsm_transport_recv
*    功能说明: 从 GPRS FILE 链路取一包数据并经 upload_http_save_response 追加到应答缓冲。
*    形    参: out_recv_size 输出本次接收字节数
*    返 回 值: 0 成功(含暂无数据),-1 保存溢出,-3 链路断开。
*********************************************************************************************************
*/
static int upload_gsm_transport_recv(int *out_recv_size)
{
    int ret = 0;
    const unsigned char *recv_data = NULL;
    int recv_data_size = 0;

    if (out_recv_size != NULL)
    {
        *out_recv_size = 0;
    }

    ret = gprs_recv_data(GPRS_LINK_FILE, &recv_data, &recv_data_size);
    if (ret == GPRS_SEND_DISCONN)
    {
        return -3;
    }
    if (ret != GPRS_SEND_OK)
    {
        return 0; /* 暂无数据 */
    }

    if ((recv_data == NULL) || (recv_data_size <= 0))
    {
        return 0;
    }

    if (upload_http_save_response(recv_data, recv_data_size) != 0)
    {
        return -1;
    }

    if (out_recv_size != NULL)
    {
        *out_recv_size = recv_data_size;
    }

    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_gsm_transport_close
*    功能说明: 断开 GPRS FILE 链路连接。
*    形    参: 无
*    返 回 值: 无。
*********************************************************************************************************
*/
static void upload_gsm_transport_close(void)
{
    gprs_network_disconnect_function(GPRS_LINK_FILE);
}

/*
*********************************************************************************************************
*    函 数 名: upload_gsm_file_function
*    功能说明: 无线上传入口: 准备 pending 日志批次,读取日志文件,按 1KB 分包上传。
*              流程: log_prepare_upload_function -> HTTP start/chunk/finish
*    形    参: 无
*    返 回 值: UPLOAD_GPRS_OK 或错误码。
*********************************************************************************************************
*/
int8_t upload_gsm_file_function(void)
{
    log_upload_bundle_t bundle;
    upload_gprs_request_t request = {0};
    int8_t ret;

    if (gprs_get_module_status_function() != 1U)
    {
        upload_set_upload_mode(UPLOAD_MODE_NULL);
        return UPLOAD_HTTP_ERR_NET;
    }

    if (app_get_network_mode() == SERVER_MODE_LWIP)
    {
        upload_set_upload_mode(UPLOAD_MODE_NULL);
        return UPLOAD_HTTP_ERR_NET;
    }

    ret = log_prepare_upload_function(&bundle);
    if (ret != LOG_OK)
    {
        upload_set_upload_mode(UPLOAD_MODE_NULL);
        return UPLOAD_HTTP_ERR_FILE;
    }

    if (bundle.total_count == 0U)
    {
        upload_set_upload_mode(UPLOAD_MODE_NULL);
        (void)log_upload_fail_function();
        return UPLOAD_HTTP_ERR_FILE;
    }
    /* 填充上传请求: 上传文件名(服务器地址/大小由 upload_http 读取全局参数与日志流) */
    request.upload_file_name = bundle.log_file_name;

    ret = upload_http_file_function(&request, &s_upload_gsm_transport, NULL);
    upload_set_upload_mode(UPLOAD_MODE_NULL);

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
