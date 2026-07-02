#include "./Upload/inc/upload.h"
#include "./Upload/inc/upload_http.h"
#include "main.h"

#define UPLOAD_LWIP_TASK_PRIO           (7U)
#define UPLOAD_LWIP_TASK_STK            (3072U)
static TaskHandle_t s_upload_lwip_task = NULL;

/* 有线上传分片大小,与 upload_http 保持一致(1KB) */
#define UPLOAD_LWIP_CHUNK_SIZE            UPLOAD_HTTP_CHUNK_SIZE

extern upload_param_t sg_uploadparam_t;
typedef upload_http_request_t upload_lwip_request_t;

static int upload_lwip_server_ip_valid(const uint8_t *ip);
static void upload_lwip_dns_cb_server_ip(const char *name, const ip_addr_t *ipaddr, void *arg);
static int upload_lwip_resolve_host(const char *host, uint8_t *ip);
static void upload_lwip_sync_server_from_remote(void);
static int upload_lwip_get_server_config(char *host, uint32_t host_size, uint16_t *port);
static int8_t upload_lwip_do_http_upload(const upload_lwip_request_t *request, int *http_status_code);

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_task_done
*    功能说明: 标记 LwIP 后台上传任务结束
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void upload_lwip_task_done(void)
{
    s_upload_lwip_task = NULL;
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_bg_task
*    功能说明: LwIP 上传后台任务入口，执行后自动删除任务
*    形    参: pvParameters FreeRTOS 任务参数(未使用)
*    返 回 值: 无
*********************************************************************************************************
*/
static void upload_lwip_bg_task(void *pvParameters)
{
    (void)pvParameters;

    FeedFwdgt();
    (void)upload_lwip_file_function();
    if (upload_get_mode_function() != UPLOAD_MODE_NULL)
    {
        upload_set_upload_mode(UPLOAD_MODE_NULL);
    }
    upload_lwip_task_done();
    vTaskDelete(NULL);
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_poll
*    功能说明: 有线上传后台任务轮询
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void upload_lwip_poll(void)
{
    BaseType_t ret;

    if (upload_get_mode_function() != UPLOAD_MODE_LWIP)
    {
        return;
    }

    if (s_upload_lwip_task != NULL)
    {
        return;
    }

    ret = xTaskCreate(upload_lwip_bg_task,
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
*    函 数 名: upload_lwip_is_running
*    功能说明: 有线上传后台任务是否运行中
*    形    参: 无
*    返 回 值: 1-运行中 0-空闲
*********************************************************************************************************
*/
uint8_t upload_lwip_is_running(void)
{
    return (uint8_t)(s_upload_lwip_task != NULL);
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_server_ip_valid
*    功能说明: 判断 IP 地址是否有效(非 0.0.0.0)。
*    形    参: ip IP 地址
*    返 回 值: 1 有效,0 无效。
*********************************************************************************************************
*/
static int upload_lwip_server_ip_valid(const uint8_t *ip)
{
    return  (ip != NULL) &&
            !((ip[0] == 0U) && (ip[1] == 0U) && (ip[2] == 0U) && (ip[3] == 0U));
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_dns_cb_server_ip
*    功能说明: DNS 解析完成回调,写入解析结果。
*    形    参: name / ipaddr / arg
*    返 回 值: 无。
*********************************************************************************************************
*/
static void upload_lwip_dns_cb_server_ip(const char *name, const ip_addr_t *ipaddr, void *arg)
{
    ip_addr_t *out_addr = (ip_addr_t *)arg;

    (void)name;

    if ((out_addr != NULL) && (ipaddr != NULL) && (ipaddr->addr != 0U))
    {
        memcpy(out_addr, ipaddr, sizeof(ip_addr_t));
    }
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_resolve_host
*    功能说明: 将主机名或域名解析为 IPv4 地址。
*    形    参: host 主机名或 IP; ip 输出 4 字节地址
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_lwip_resolve_host(const char *host, uint8_t *ip)
{
    ip_addr_t server_ip = {0};
    err_t err;
    uint32_t tick;

    if ((host == NULL) || (host[0] == '\0') || (ip == NULL))
    {
        return -1;
    }

    if ((host[0] >= '0') && (host[0] <= '9'))
    {
        if (ipaddr_aton(host, &server_ip) != 1)
        {
            return -1;
        }
    }
    else
    {
        err = dns_gethostbyname(host, &server_ip, upload_lwip_dns_cb_server_ip, &server_ip);
        if (err == ERR_OK)
        {
            /* fall through */
        }
        else if (err != ERR_INPROGRESS)
        {
            return -1;
        }
        else
        {
            tick = HAL_GetTick();
            while ((HAL_GetTick() - tick) < 5000U)
            {
                if (server_ip.addr != 0U)
                {
                    break;
                }
                vTaskDelay(10);
            }

            if (server_ip.addr == 0U)
            {
                return -1;
            }
        }
    }

    ip[0] = ip4_addr1(&server_ip);
    ip[1] = ip4_addr2(&server_ip);
    ip[2] = ip4_addr3(&server_ip);
    ip[3] = ip4_addr4(&server_ip);
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_sync_server_from_remote
*    功能说明: 上传参数未配置时,从远程网络信息同步服务器地址。
*    形    参: 无
*    返 回 值: 无。
*********************************************************************************************************
*/
static void upload_lwip_sync_server_from_remote(void)
{
    struct remote_ip *remote = app_get_remote_network_function();
    int ip[4] = {0};

    if ((sg_uploadparam_t.port != 0U) &&
        (upload_lwip_server_ip_valid(sg_uploadparam_t.ip) || (sg_uploadparam_t.host[0] != '\0')))
    {
        return;
    }

    if ((remote == NULL) || (remote->inside_port == 0U) || (remote->inside_iporname[0] == '\0'))
    {
        return;
    }

    snprintf(sg_uploadparam_t.host, sizeof(sg_uploadparam_t.host), "%s", remote->inside_iporname);
    sg_uploadparam_t.port = remote->inside_port;

    if (sscanf((char *)remote->inside_iporname, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]) == 4)
    {
        sg_uploadparam_t.ip[0] = (uint8_t)ip[0];
        sg_uploadparam_t.ip[1] = (uint8_t)ip[1];
        sg_uploadparam_t.ip[2] = (uint8_t)ip[2];
        sg_uploadparam_t.ip[3] = (uint8_t)ip[3];
    }
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_get_server_config
*    功能说明: 获取有线上传服务器主机名与端口。
*    形    参: host 输出缓冲; host_size 缓冲长度; port 输出端口
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_lwip_get_server_config(char *host, uint32_t host_size, uint16_t *port)
{
    uint8_t server_ip[4] = {0};

    if ((host == NULL) || (host_size == 0U) || (port == NULL))
    {
        return -1;
    }

    upload_lwip_sync_server_from_remote();

    if (sg_uploadparam_t.port == 0U)
    {
        *port = UPLOAD_HTTP_DEFAULT_PORT;
    }
    else
    {
        *port = (uint16_t)sg_uploadparam_t.port;
    }

    if (sg_uploadparam_t.host[0] != '\0')
    {
        snprintf(host, host_size, "%s", sg_uploadparam_t.host);
        return 0;
    }

    if (upload_lwip_server_ip_valid(sg_uploadparam_t.ip))
    {
        snprintf(host, host_size, "%u.%u.%u.%u",
                 (unsigned int)sg_uploadparam_t.ip[0],
                 (unsigned int)sg_uploadparam_t.ip[1],
                 (unsigned int)sg_uploadparam_t.ip[2],
                 (unsigned int)sg_uploadparam_t.ip[3]);
        return 0;
    }

    if (upload_lwip_server_ip_valid(g_lwipdev.remoteip))
    {
        snprintf(host, host_size, "%u.%u.%u.%u",
                 (unsigned int)g_lwipdev.remoteip[0],
                 (unsigned int)g_lwipdev.remoteip[1],
                 (unsigned int)g_lwipdev.remoteip[2],
                 (unsigned int)g_lwipdev.remoteip[3]);
        return 0;
    }

    if (upload_lwip_resolve_host(UPLOAD_HTTP_DEFAULT_HOST, server_ip) == 0)
    {
        snprintf(host, host_size, "%u.%u.%u.%u",
                 (unsigned int)server_ip[0],
                 (unsigned int)server_ip[1],
                 (unsigned int)server_ip[2],
                 (unsigned int)server_ip[3]);
        return 0;
    }

    snprintf(host, host_size, "%s", UPLOAD_HTTP_DEFAULT_HOST);
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_do_http_upload
*    功能说明: 新建 TCP 连接,按 UPLOAD_LWIP_CHUNK_SIZE(1KB) 读取日志并三步分片上传。
*              start -> chunk(1KB/包) -> finish
*    形    参: request 上传请求; http_status_code 输出 HTTP 状态码(可为 NULL)
*    返 回 值: UPLOAD_LWIP_OK 或错误码。
*********************************************************************************************************
*/
static int8_t upload_lwip_do_http_upload(const upload_lwip_request_t *request, int *http_status_code)
{
    upload_http_request_t http_request = {0};

    if ((request == NULL) || (request->file_path == NULL) || (request->file_path[0] == '\0'))
    {
        return UPLOAD_HTTP_ERR_PARAM;
    }

    http_request.host = request->host;
    http_request.port = request->port;
    http_request.url = request->url;
    http_request.file_path = request->file_path;
    http_request.upload_file_name = request->upload_file_name;
    http_request.device_sn = request->device_sn;
    http_request.log_type = request->log_type;
    http_request.finish_md5_enable = request->finish_md5_enable;
    http_request.preferred_link = UPLOAD_HTTP_LINK_LWIP;

    return upload_http_file_function(&http_request, http_status_code);
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_file_with_request
*    功能说明: 按外部请求参数执行有线日志上传(后台任务可调用)。
*    形    参: request 上传请求; http_status_code 输出 HTTP 状态码(可为 NULL)
*    返 回 值: UPLOAD_LWIP_OK 或错误码。
*********************************************************************************************************
*/
int8_t upload_lwip_file_with_request(const upload_lwip_request_t *request, int *http_status_code)
{
    if (request == NULL)
    {
        return UPLOAD_HTTP_ERR_PARAM;
    }

    if (g_lwipdev.netif_state == 0U)
    {
        return UPLOAD_HTTP_ERR_NET;
    }

    if (app_get_network_mode() == SERVER_MODE_GPRS)
    {
        return UPLOAD_HTTP_ERR_NET;
    }

    return upload_lwip_do_http_upload(request, http_status_code);
}

/*
*********************************************************************************************************
*    函 数 名: upload_lwip_file_function
*    功能说明: 有线上传入口: 收到上传指令后新建 TCP,读取 pending 日志并按 1KB 分包上传。
*              流程: log_prepare_upload_function -> TCP 连接 -> HTTP start/chunk/finish
*    形    参: 无
*    返 回 值: UPLOAD_LWIP_OK 或错误码。
*********************************************************************************************************
*/
int8_t upload_lwip_file_function(void)
{
    log_upload_bundle_t bundle;
    upload_lwip_request_t request = {0};
    char host_buf[64] = {0};
    int8_t ret;

    if (g_lwipdev.netif_state == 0U)
    {
        upload_set_upload_mode(UPLOAD_MODE_NULL);
        return UPLOAD_HTTP_ERR_NET;
    }

    if (app_get_network_mode() == SERVER_MODE_GPRS)
    {
        upload_set_upload_mode(UPLOAD_MODE_NULL);
        return UPLOAD_HTTP_ERR_NET;
    }

    if (upload_lwip_get_server_config(host_buf, sizeof(host_buf), &request.port) != 0)
    {
        upload_set_upload_mode(UPLOAD_MODE_NULL);
        return UPLOAD_HTTP_ERR_PARAM;
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

    request.host = host_buf;
    request.file_path = bundle.log_file;
    request.upload_file_name = bundle.log_file_name;

    ret = upload_lwip_do_http_upload(&request, NULL);

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
