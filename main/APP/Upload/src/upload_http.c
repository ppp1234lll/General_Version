#include "./Upload/inc/upload_http.h"
#include "main.h"

/* multipart/form-data 分片上传边界 */
#define UPLOAD_HTTP_BOUNDARY              "----FNLOG_UPLOAD_BOUNDARY"
#define UPLOAD_HTTP_HEADER_MAX            512   /* HTTP 请求头缓冲 */
#define UPLOAD_HTTP_FORM_HEAD_MAX         512   /* multipart 表单头缓冲 */
#define UPLOAD_HTTP_FORM_TAIL_MAX         64    /* multipart 结束边界缓冲 */
/* chunk 完整 body: form_head + file_data + form_tail,与 Python build_chunk_body 一致 */
#define UPLOAD_HTTP_CHUNK_BODY_MAX        (UPLOAD_HTTP_FORM_HEAD_MAX + UPLOAD_HTTP_CHUNK_SIZE + UPLOAD_HTTP_FORM_TAIL_MAX)
#define UPLOAD_HTTP_CHUNK_REQUEST_MAX     (UPLOAD_HTTP_HEADER_MAX + UPLOAD_HTTP_CHUNK_BODY_MAX)
#define UPLOAD_HTTP_RESP_MAX              1024  /* HTTP 应答缓冲 */
#define UPLOAD_HTTP_JSON_BODY_MAX         512   /* 解码后的 JSON 正文缓冲 */
#define UPLOAD_HTTP_RECV_TIMEOUT_MS       10000U /* 等待应答超时(ms) */
#define UPLOAD_HTTP_GPRS_RECV_BUF         512   /* GPRS 单次读取缓冲 */
#define UPLOAD_HTTP_CHUNK_RETRY_MAX       3U    /* 分片上传失败重试次数 */
#define UPLOAD_HTTP_API_CODE_OK           200   /* 接口 JSON 成功 code 值 */
#define UPLOAD_HTTP_MD5_HEX_MAX           33U   /* MD5 十六进制字符串长度(32+1) */

/* 有线 TCP 连接句柄 */
static struct netconn *s_upload_tcp = NULL;
static uint8_t s_upload_http_chunk_request[UPLOAD_HTTP_CHUNK_REQUEST_MAX];
static uint8_t s_upload_http_chunk_data[UPLOAD_HTTP_CHUNK_SIZE];
////

static const char *upload_http_safe_str(const char *str, const char *default_str);
static upload_http_link_t upload_http_select_link(upload_http_link_t preferred_link);
static void upload_http_dns_cb_server_ip(const char *name, const ip_addr_t *ipaddr, void *arg);
static int upload_http_resolve_host_lwip(const char *host, ip_addr_t *out_ip);
static int upload_http_connect_lwip(const char *host, uint16_t port);
static int upload_http_connect_gprs(const char *host, uint16_t port);
static void upload_http_close_link(upload_http_link_t link_type);
static int upload_http_send_raw(upload_http_link_t link_type, const uint8_t *data, uint32_t len);
static int upload_http_send_lwip_all(const uint8_t *data, uint32_t len);
static int upload_http_lwip_drain_tx(void);
static int upload_http_send_buffer(upload_http_link_t link_type, const uint8_t *data, uint32_t len);
static int upload_http_get_file_size(const char *file_path, uint32_t *file_size);
static void upload_http_get_device_id(const char *device_sn, char *device_id, uint32_t size);
static int upload_http_response_is_chunked(const char *response);
static int upload_http_response_chunked_complete(const char *body);
static int upload_http_decode_chunked_body(const char *chunked_body, char *out, int out_size);
static int upload_http_extract_json_body(const char *response, char *json_body, int json_body_size);
static int upload_http_response_complete(const char *response);
static int upload_http_parse_status_code(const char *response, int *http_status_code);
static int upload_http_parse_json_int(const char *body, const char *key, int *out_value);
static int upload_http_parse_json_string(const char *body, const char *key, char *out_str, uint32_t out_size);
static int upload_http_parse_start_response(const char *response, char *upload_key, uint32_t upload_key_size);
static int upload_http_parse_chunk_response(const char *response);
static void upload_http_md5_to_hex(const unsigned char *digest, char *hex_out, uint32_t hex_size);
static const char *upload_http_get_file_name(const char *file_path, const char *upload_file_name);
static int upload_http_append_recv(char *response, int response_size, const uint8_t *data, int data_len);
static int upload_http_recv_response_lwip(char *response, int response_size, int *http_status_code);
static int upload_http_recv_response_gprs(char *response, int response_size, int *http_status_code);
static int upload_http_recv_response(upload_http_link_t link_type, char *response, int response_size, int *http_status_code);
static int upload_http_post_query(upload_http_link_t link_type,
                                  const char *host,
                                  uint16_t port,
                                  const char *path_with_query,
                                  uint8_t keep_alive,
                                  char *response,
                                  int response_size,
                                  int *http_status_code);
static int upload_http_post_chunk(upload_http_link_t link_type,
                                  const char *host,
                                  uint16_t port,
                                  const char *upload_id,
                                  const char *file_name,
                                  uint32_t chunk_index,
                                  const uint8_t *data,
                                  uint16_t data_len,
                                  uint8_t keep_alive,
                                  char *response,
                                  int response_size,
                                  int *http_status_code);
static int upload_http_post_finish(upload_http_link_t link_type,
                                   const char *host,
                                   uint16_t port,
                                   const char *upload_id,
                                   uint8_t finish_md5_enable,
                                   const char *file_md5,
                                   char *response,
                                   int response_size,
                                   int *http_status_code);
static int upload_http_path_is_log(const char *path);

/*
*********************************************************************************************************
*    函 数 名: upload_http_safe_str
*    功能说明: 取有效字符串,空指针或空串时返回默认值。
*    形    参: str / default_str
*    返 回 值: 有效字符串指针。
*********************************************************************************************************
*/
static const char *upload_http_safe_str(const char *str, const char *default_str)
{
    if ((str != NULL) && (str[0] != '\0'))
    {
        return str;
    }

    return default_str;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_select_link
*    功能说明: 根据配置和网络状态选择 lwIP 或 GPRS 链路。
*    形    参: preferred_link 优先链路(AUTO/LWIP/GPRS)
*    返 回 值: 实际使用的链路类型。
*********************************************************************************************************
*/
static upload_http_link_t upload_http_select_link(upload_http_link_t preferred_link)
{
    uint8_t net_mode = app_get_network_mode();

    if (preferred_link == UPLOAD_HTTP_LINK_LWIP)
    {
        return UPLOAD_HTTP_LINK_LWIP;
    }

    if (preferred_link == UPLOAD_HTTP_LINK_GPRS)
    {
        return UPLOAD_HTTP_LINK_GPRS;
    }

    if ((net_mode == SERVER_MODE_GPRS) || (g_lwipdev.netif_state == 0U))
    {
        return UPLOAD_HTTP_LINK_GPRS;
    }

    return UPLOAD_HTTP_LINK_LWIP;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_path_is_log
*    功能说明: 判断路径是否属于 log 模块管理的日志文件。
*    形    参: path LittleFS 路径
*    返 回 值: 1 是日志路径,0 否。
*********************************************************************************************************
*/
static int upload_http_path_is_log(const char *path)
{
    size_t root_len = 0U;

    if (path == NULL)
    {
        return 0;
    }

    root_len = strlen(LOG_ROOT_DIR);
    if (strncmp(path, LOG_ROOT_DIR, root_len) != 0)
    {
        return 0;
    }

    return (path[root_len] == '\0') || (path[root_len] == '/');
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_dns_cb_server_ip
*    功能说明: DNS 解析完成回调,写入解析结果。
*    形    参: name / ipaddr / arg
*    返 回 值: 无。
*********************************************************************************************************
*/
static void upload_http_dns_cb_server_ip(const char *name, const ip_addr_t *ipaddr, void *arg)
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
*    函 数 名: upload_http_resolve_host_lwip
*    功能说明: 将主机名或域名解析为 IP 地址(lwIP)。
*    形    参: host 主机名或 IP 字符串; out_ip 输出 IP
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_resolve_host_lwip(const char *host, ip_addr_t *out_ip)
{
    err_t err = ERR_OK;
    uint32_t begin_tick = 0U;

    if ((host == NULL) || (out_ip == NULL))
    {
        return -1;
    }

    memset(out_ip, 0, sizeof(ip_addr_t));
    if ((host[0] >= '0') && (host[0] <= '9'))
    {
        return (ipaddr_aton(host, out_ip) == 1) ? 0 : -1;
    }

    err = dns_gethostbyname(host, out_ip, upload_http_dns_cb_server_ip, out_ip);
    if (err == ERR_OK)
    {
        return 0;
    }

    if (err != ERR_INPROGRESS)
    {
        return -1;
    }

    begin_tick = HAL_GetTick();
    while ((HAL_GetTick() - begin_tick) < 5000U)
    {
        if (out_ip->addr != 0U)
        {
            return 0;
        }
        vTaskDelay(10);
    }

    return -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_connect_lwip
*    功能说明: 通过有线网络建立 HTTP 上传 TCP 连接。
*    形    参: host 服务器地址; port 端口
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_connect_lwip(const char *host, uint16_t port)
{
    ip_addr_t server_ip = {0};
    err_t err = ERR_OK;
    uint8_t retry = 0U;

    if (upload_http_resolve_host_lwip(host, &server_ip) != 0)
    {
        return -1;
    }

    for (retry = 0U; retry < 3U; retry++)
    {
        s_upload_tcp = netconn_new(NETCONN_TCP);
        if (s_upload_tcp == NULL)
        {
            continue;
        }

        err = netconn_connect(s_upload_tcp, &server_ip, port);
        if (err == ERR_OK)
        {
            s_upload_tcp->recv_timeout = 10;
            if (s_upload_tcp->pcb.tcp != NULL)
            {
                tcp_nagle_disable(s_upload_tcp->pcb.tcp);
            }
            return 0;
        }

        netconn_delete(s_upload_tcp);
        s_upload_tcp = NULL;
        vTaskDelay(50);
    }

    return -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_connect_gprs
*    功能说明: 通过 GPRS 模块建立 HTTP 上传 TCP 连接(FILE 链路)。
*    形    参: host 服务器地址; port 端口
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_connect_gprs(const char *host, uint16_t port)
{
    uint8_t retry = 0U;

    for (retry = 0U; retry < 3U; retry++)
    {
        if (gprs_network_connect_function(host, port, GPRS_LINK_FILE) == GPRS_SEND_OK)
        {
            return 0;
        }
        vTaskDelay(50);
    }

    return -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_close_link
*    功能说明: 关闭上传链路连接。
*    形    参: link_type 链路类型
*    返 回 值: 无。
*********************************************************************************************************
*/
static void upload_http_close_link(upload_http_link_t link_type)
{
    if (link_type == UPLOAD_HTTP_LINK_LWIP)
    {
        if (s_upload_tcp != NULL)
        {
            netconn_close(s_upload_tcp);
            netconn_delete(s_upload_tcp);
            s_upload_tcp = NULL;
        }
    }
    else if (link_type == UPLOAD_HTTP_LINK_GPRS)
    {
        gprs_network_disconnect_function(GPRS_LINK_FILE);
    }
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_send_raw
*    功能说明: 按链路类型发送一段原始数据。
*    形    参: link_type / data / len
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_send_raw(upload_http_link_t link_type, const uint8_t *data, uint32_t len)
{
    if ((data == NULL) || (len == 0U))
    {
        return 0;
    }

    if (link_type == UPLOAD_HTTP_LINK_LWIP)
    {
        if ((s_upload_tcp == NULL) || (netconn_write(s_upload_tcp, data, len, NETCONN_COPY) != ERR_OK))
        {
            return -1;
        }
        return 0;
    }

    if (link_type == UPLOAD_HTTP_LINK_GPRS)
    {
        return (gprs_send_data(data, (int)len, 5000, GPRS_LINK_FILE) == GPRS_SEND_OK) ? 0 : -1;
    }

    return -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_send_lwip_all
*    功能说明: 有线链路循环 netconn_write_partly 直至全部写入 TCP 发送缓冲。
*********************************************************************************************************
*/
static int upload_http_send_lwip_all(const uint8_t *data, uint32_t len)
{
    uint32_t offset = 0U;
    uint32_t stall = 0U;

    if ((s_upload_tcp == NULL) || (data == NULL) || (len == 0U))
    {
        return -1;
    }

    while (offset < len)
    {
        size_t written = 0U;
        err_t err = netconn_write_partly(s_upload_tcp,
                                         data + offset,
                                         (size_t)(len - offset),
                                         NETCONN_COPY,
                                         &written);

        if (err != ERR_OK)
        {
            return -1;
        }

        if (written == 0U)
        {
            stall++;
            if (stall > 500U)
            {
                return -1;
            }
            vTaskDelay(1);
            continue;
        }

        stall = 0U;
        offset += (uint32_t)written;
    }

    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_lwip_drain_tx
*    功能说明: 等待 unsent 队列清空后再 recv,避免只发出首段 TCP 包。
*********************************************************************************************************
*/
static int upload_http_lwip_drain_tx(void)
{
    struct tcp_pcb *pcb = NULL;
    uint32_t stall = 0U;

    if (s_upload_tcp == NULL)
    {
        return -1;
    }

    while (stall < 500U)
    {
        pcb = s_upload_tcp->pcb.tcp;
        if (pcb == NULL)
        {
            return -1;
        }

        if (pcb->unsent == NULL)
        {
            return 0;
        }

        stall++;
        vTaskDelay(1);
    }

    return -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_send_buffer
*    功能说明: 发送任意长度缓冲,内部分片不超过 1KB。
*    形    参: link_type / data / len
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_send_buffer(upload_http_link_t link_type, const uint8_t *data, uint32_t len)
{
    return upload_http_send_raw(link_type, data, len);
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_get_file_size
*    功能说明: 获取 LittleFS 待上传文件大小。
*    形    参: file_path 文件路径; file_size 输出大小
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_get_file_size(const char *file_path, uint32_t *file_size)
{
    lfs_file_t lfs_fp;
    lfs_soff_t pos = 0;
    int err = 0;

    if ((file_path == NULL) || (file_size == NULL))
    {
        return -1;
    }

    err = lfs_file_open(&g_lfs_t, &lfs_fp, file_path, LFS_O_RDONLY);
    if (err != 0)
    {
        return -1;
    }

    pos = lfs_file_seek(&g_lfs_t, &lfs_fp, 0, LFS_SEEK_END);
    (void)lfs_file_close(&g_lfs_t, &lfs_fp);
    if (pos < 0)
    {
        return -1;
    }

    *file_size = (uint32_t)pos;
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_get_device_id
*    功能说明: 获取设备编号,优先使用传入值,否则从 Flash 读取。
*    形    参: device_sn 外部编号; device_id 输出缓冲; size 缓冲长度
*    返 回 值: 无。
*********************************************************************************************************
*/
static void upload_http_get_device_id(const char *device_sn, char *device_id, uint32_t size)
{
    union
    {
        uint32_t value;
        uint8_t raw[4];
    } dev_id = {0};

    if ((device_id == NULL) || (size == 0U))
    {
        return;
    }

    if ((device_sn != NULL) && (device_sn[0] != '\0'))
    {
        snprintf(device_id, size, "%s", device_sn);
        return;
    }

    bsp_ReadCpuFlash(DEVICE_ID_ADDR, dev_id.raw, sizeof(dev_id.raw));
    dev_id.value &= 0x00FFFFFFUL;
    if ((dev_id.value == 0U) || (dev_id.value == 0x00FFFFFFUL))
    {
        dev_id.value = 3U;
    }

    snprintf(device_id, size, "%lX", (unsigned long)dev_id.value);
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_response_is_chunked
*    功能说明: 判断 HTTP 应答头是否使用 Transfer-Encoding: chunked。
*********************************************************************************************************
*/
static int upload_http_response_is_chunked(const char *response)
{
    const char *head_end = NULL;
    const char *pt = NULL;

    if (response == NULL)
    {
        return 0;
    }

    head_end = strstr(response, "\r\n\r\n");
    if (head_end == NULL)
    {
        return 0;
    }

    pt = strstr(response, "Transfer-Encoding:");
    if ((pt == NULL) || (pt >= head_end))
    {
        return 0;
    }

    return (strstr(pt, "chunked") != NULL) ? 1 : 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_response_chunked_complete
*    功能说明: 判断 chunked 应答体是否已收到结束块(0)。
*********************************************************************************************************
*/
static int upload_http_response_chunked_complete(const char *body)
{
    if (body == NULL)
    {
        return 0;
    }

    return (strstr(body, "\r\n0\r\n\r\n") != NULL) ? 1 : 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_decode_chunked_body
*    功能说明: 将 chunked 编码的 HTTP 正文解码为纯 JSON 字符串。
*              例: 39\r\n{"code":500,...}\r\n0\r\n\r\n -> {"code":500,...}
*********************************************************************************************************
*/
static int upload_http_decode_chunked_body(const char *chunked_body, char *out, int out_size)
{
    const char *pt = chunked_body;
    char *endptr = NULL;
    unsigned long chunk_size = 0UL;
    int out_len = 0;

    if ((chunked_body == NULL) || (out == NULL) || (out_size <= 0))
    {
        return -1;
    }

    out[0] = '\0';

    while (*pt != '\0')
    {
        while ((*pt == '\r') || (*pt == '\n'))
        {
            pt++;
        }

        if (*pt == '\0')
        {
            break;
        }

        chunk_size = strtoul(pt, &endptr, 16);
        if (endptr == pt)
        {
            return -1;
        }

        pt = endptr;
        if (*pt == '\r')
        {
            pt++;
        }
        if (*pt == '\n')
        {
            pt++;
        }

        if (chunk_size == 0UL)
        {
            break;
        }

        if ((out_len + (int)chunk_size) >= out_size)
        {
            return -1;
        }

        memcpy(out + out_len, pt, (size_t)chunk_size);
        out_len += (int)chunk_size;
        pt += chunk_size;

        if (*pt == '\r')
        {
            pt++;
        }
        if (*pt == '\n')
        {
            pt++;
        }
    }

    if (out_len <= 0)
    {
        return -1;
    }

    out[out_len] = '\0';
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_extract_json_body
*    功能说明: 从开始上传等 HTTP 应答中提取 JSON 正文(自动处理 chunked)。
*********************************************************************************************************
*/
static int upload_http_extract_json_body(const char *response, char *json_body, int json_body_size)
{
    const char *raw_body = NULL;

    if ((response == NULL) || (json_body == NULL) || (json_body_size <= 0))
    {
        return -1;
    }

    raw_body = strstr(response, "\r\n\r\n");
    if (raw_body == NULL)
    {
        return -1;
    }
    raw_body += 4;

    if (upload_http_response_is_chunked(response))
    {
        return upload_http_decode_chunked_body(raw_body, json_body, json_body_size);
    }

    snprintf(json_body, (size_t)json_body_size, "%s", raw_body);
    return (json_body[0] != '\0') ? 0 : -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_response_complete
*    功能说明: 判断 HTTP 应答是否接收完整(Content-Length 或 chunked 结束块)。
*    形    参: response 应答缓冲
*    返 回 值: 1 完整,0 未完整。
*********************************************************************************************************
*/
static int upload_http_response_complete(const char *response)
{
    const char *head_end = NULL;
    const char *pt = NULL;
    int body_size = 0;
    int head_size = 0;
    int recv_body = 0;

    if ((response == NULL) || (response[0] == '\0'))
    {
        return 0;
    }

    head_end = strstr(response, "\r\n\r\n");
    if (head_end == NULL)
    {
        return 0;
    }

    head_size = (int)(head_end - response + 4);

    if (upload_http_response_is_chunked(response))
    {
        return upload_http_response_chunked_complete(head_end + 4);
    }

    pt = strstr(response, "Content-Length:");
    if (pt == NULL)
    {
        return 1;
    }

    if (sscanf(pt, "Content-Length: %d", &body_size) != 1)
    {
        return 1;
    }

    recv_body = (int)strlen(response) - head_size;
    return (recv_body >= body_size) ? 1 : 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_parse_status_code
*    功能说明: 从 HTTP 应答中解析状态码。
*    形    参: response 应答缓冲; http_status_code 输出状态码
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_parse_status_code(const char *response, int *http_status_code)
{
    const char *pt = NULL;
    int status = 0;

    if ((response == NULL) || (http_status_code == NULL))
    {
        return -1;
    }

    pt = strstr(response, "HTTP/");
    if (pt == NULL)
    {
        return -1;
    }

    if (sscanf(pt, "HTTP/%*d.%*d %d", &status) != 1)
    {
        return -1;
    }

    *http_status_code = status;
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_get_file_name
*    功能说明: 获取上传文件名,优先使用 upload_file_name,否则从路径提取。
*    形    参: file_path / upload_file_name
*    返 回 值: 文件名字符串指针。
*********************************************************************************************************
*/
static const char *upload_http_get_file_name(const char *file_path, const char *upload_file_name)
{
    const char *name = NULL;

    if ((upload_file_name != NULL) && (upload_file_name[0] != '\0'))
    {
        return upload_file_name;
    }

    if (file_path == NULL)
    {
        return "";
    }

    name = strrchr(file_path, '/');
    if (name == NULL)
    {
        name = strrchr(file_path, '\\');
    }

    return (name != NULL) ? (name + 1) : file_path;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_parse_json_int
*    功能说明: 从 JSON 文本中解析整型字段。
*    形    参: body JSON 正文; key 字段名; out_value 输出值
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_parse_json_int(const char *body, const char *key, int *out_value)
{
    char key_pattern[32] = {0};
    const char *key_pos = NULL;
    const char *start = NULL;
    int value = 0;

    if ((body == NULL) || (key == NULL) || (out_value == NULL))
    {
        return -1;
    }

    snprintf(key_pattern, sizeof(key_pattern), "\"%s\"", key);
    key_pos = strstr(body, key_pattern);
    if (key_pos == NULL)
    {
        return -1;
    }

    start = strchr(key_pos, ':');
    if (start == NULL)
    {
        return -1;
    }
    start++;
    while ((*start == ' ') || (*start == '\t'))
    {
        start++;
    }

    if (sscanf(start, "%d", &value) != 1)
    {
        return -1;
    }

    *out_value = value;
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_parse_json_string
*    功能说明: 从 JSON 文本中解析字符串字段。
*    形    参: body JSON 正文; key 字段名; out_str 输出缓冲; out_size 缓冲长度
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_parse_json_string(const char *body, const char *key, char *out_str, uint32_t out_size)
{
    char key_pattern[32] = {0};
    const char *key_pos = NULL;
    const char *start = NULL;
    const char *end = NULL;
    int len = 0;

    if ((body == NULL) || (key == NULL) || (out_str == NULL) || (out_size == 0U))
    {
        return -1;
    }

    snprintf(key_pattern, sizeof(key_pattern), "\"%s\"", key);
    key_pos = strstr(body, key_pattern);
    if (key_pos == NULL)
    {
        return -1;
    }

    start = strchr(key_pos, ':');
    if (start == NULL)
    {
        return -1;
    }
    start++;
    while ((*start == ' ') || (*start == '\t'))
    {
        start++;
    }
    if (*start == '\"')
    {
        start++;
    }

    end = start;
    while ((*end != '\0') && (*end != '\"') && (*end != ',') && (*end != '}'))
    {
        end++;
    }

    len = (int)(end - start);
    if ((len < 0) || (len >= (int)out_size))
    {
        return -1;
    }

    memcpy(out_str, start, (size_t)len);
    out_str[len] = '\0';
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_parse_start_response
*    功能说明: 解析 start 接口 JSON 应答,校验 code==200 并提取 data.uploadId。
*    形    参: response 应答缓冲; upload_key 输出 uploadId; upload_key_size 缓冲长度
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_parse_start_response(const char *response, char *upload_key, uint32_t upload_key_size)
{
    char json_body[UPLOAD_HTTP_JSON_BODY_MAX] = {0};
    char msg_buf[128] = {0};
    int api_code = 0;

    if ((response == NULL) || (upload_key == NULL) || (upload_key_size == 0U))
    {
        return -1;
    }

    if (upload_http_extract_json_body(response, json_body, (int)sizeof(json_body)) != 0)
    {
        return -1;
    }

    if (upload_http_parse_json_int(json_body, "code", &api_code) != 0)
    {
        return -1;
    }

    (void)upload_http_parse_json_string(json_body, "msg", msg_buf, sizeof(msg_buf));

    if (api_code != UPLOAD_HTTP_API_CODE_OK)
    {
        return -1;
    }

    if (upload_http_parse_json_string(json_body, "uploadId", upload_key, upload_key_size) != 0)
    {
        return -1;
    }
    if (upload_key[0] == '\0')
    {
        return -1;
    }

    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_parse_chunk_response
*    功能说明: 解析 chunk/finish 接口 JSON 应答,校验 code==200。
*    形    参: response HTTP 应答缓冲
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_parse_chunk_response(const char *response)
{
    char json_body[UPLOAD_HTTP_JSON_BODY_MAX] = {0};
    int api_code = 0;

    if (response == NULL)
    {
        return -1;
    }

    if (upload_http_extract_json_body(response, json_body, (int)sizeof(json_body)) != 0)
    {
        return -1;
    }

    if (upload_http_parse_json_int(json_body, "code", &api_code) != 0)
    {
        return -1;
    }

    return (api_code == UPLOAD_HTTP_API_CODE_OK) ? 0 : -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_md5_to_hex
*    功能说明: 将 16 字节 MD5 摘要转为 32 位小写十六进制字符串。
*    形    参: digest MD5 摘要; hex_out 输出缓冲; hex_size 缓冲长度(至少 33)
*    返 回 值: 无。
*********************************************************************************************************
*/
static void upload_http_md5_to_hex(const unsigned char *digest, char *hex_out, uint32_t hex_size)
{
    static const char hex_tab[] = "0123456789abcdef";
    uint32_t i = 0U;

    if ((digest == NULL) || (hex_out == NULL) || (hex_size < UPLOAD_HTTP_MD5_HEX_MAX))
    {
        return;
    }

    for (i = 0U; i < 16U; i++)
    {
        hex_out[i * 2U] = hex_tab[(digest[i] >> 4) & 0x0FU];
        hex_out[(i * 2U) + 1U] = hex_tab[digest[i] & 0x0FU];
    }
    hex_out[32] = '\0';
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_append_recv
*    功能说明: 将新收到的数据追加到应答缓冲末尾。
*    形    参: response / response_size / data / data_len
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_append_recv(char *response, int response_size, const uint8_t *data, int data_len)
{
    int recv_size = 0;
    int copy_len = 0;

    if ((response == NULL) || (response_size <= 0) || (data == NULL) || (data_len <= 0))
    {
        return -1;
    }

    recv_size = (int)strlen(response);
    copy_len = data_len;
    if ((recv_size + copy_len) >= (response_size - 1))
    {
        copy_len = (response_size - 1) - recv_size;
    }

    if (copy_len <= 0)
    {
        return -1;
    }

    memcpy(response + recv_size, data, (size_t)copy_len);
    response[recv_size + copy_len] = '\0';
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_recv_response_lwip
*    功能说明: 通过有线链路接收 HTTP 应答并解析状态码。
*    形    参: response / response_size / http_status_code
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_recv_response_lwip(char *response, int response_size, int *http_status_code)
{
    struct netbuf *recvbuf = NULL;
    struct pbuf *q = NULL;
    err_t recv_err = ERR_OK;
    uint32_t begin_tick = HAL_GetTick();

    if ((response == NULL) || (response_size <= 0) || (http_status_code == NULL))
    {
        return -1;
    }

    memset(response, 0, (size_t)response_size);
    *http_status_code = 0;

    while ((HAL_GetTick() - begin_tick) < UPLOAD_HTTP_RECV_TIMEOUT_MS)
    {
        recv_err = netconn_recv(s_upload_tcp, &recvbuf);
        if (recv_err == ERR_OK)
        {
            for (q = recvbuf->p; q != NULL; q = q->next)
            {
                if (upload_http_append_recv(response, response_size,
                                             (const uint8_t *)q->payload, q->len) != 0)
                {
                    netbuf_delete(recvbuf);
                    return -1;
                }
            }

            netbuf_delete(recvbuf);
            recvbuf = NULL;
            begin_tick = HAL_GetTick();

            if (upload_http_response_complete(response))
            {
                break;
            }
            continue;
        }

        if (recv_err == ERR_TIMEOUT)
        {
            if (upload_http_response_complete(response))
            {
                break;
            }
            vTaskDelay(10);
            continue;
        }

        break;
    }

    if (recvbuf != NULL)
    {
        netbuf_delete(recvbuf);
    }

    return upload_http_parse_status_code(response, http_status_code);
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_recv_response_gprs
*    功能说明: 通过 GPRS FILE 链路接收 HTTP 应答并解析状态码。
*    形    参: response / response_size / http_status_code
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_recv_response_gprs(char *response, int response_size, int *http_status_code)
{
    uint8_t recv_buf[UPLOAD_HTTP_GPRS_RECV_BUF];
    int recv_data_size = 0;
    int ret = 0;
    uint32_t begin_tick = HAL_GetTick();

    if ((response == NULL) || (response_size <= 0) || (http_status_code == NULL))
    {
        return -1;
    }

    memset(response, 0, (size_t)response_size);
    *http_status_code = 0;

    while ((HAL_GetTick() - begin_tick) < UPLOAD_HTTP_RECV_TIMEOUT_MS)
    {
        recv_data_size = 0;
        ret = gprs_recv_data_file(recv_buf, (int)sizeof(recv_buf), &recv_data_size);
        if (ret != GPRS_SEND_OK)
        {
            if (upload_http_response_complete(response))
            {
                break;
            }
            break;
        }

        if (recv_data_size > 0)
        {
            if (upload_http_append_recv(response, response_size, recv_buf, recv_data_size) != 0)
            {
                return -1;
            }
            begin_tick = HAL_GetTick();

            if (upload_http_response_complete(response))
            {
                break;
            }
        }
        else
        {
            if (upload_http_response_complete(response))
            {
                break;
            }
            vTaskDelay(10);
        }
    }

    return upload_http_parse_status_code(response, http_status_code);
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_recv_response
*    功能说明: 按链路类型分发 HTTP 应答接收。
*    形    参: link_type / response / response_size / http_status_code
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_recv_response(upload_http_link_t link_type,
                                     char *response,
                                     int response_size,
                                     int *http_status_code)
{
    if (link_type == UPLOAD_HTTP_LINK_LWIP)
    {
        return upload_http_recv_response_lwip(response, response_size, http_status_code);
    }

    if (link_type == UPLOAD_HTTP_LINK_GPRS)
    {
        return upload_http_recv_response_gprs(response, response_size, http_status_code);
    }

    return -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_post_query
*    功能说明: 发送带 query 参数、无请求体的 POST 请求(start 接口)。
*    形    参: link_type / host / port / path_with_query /
*              keep_alive / response / response_size / http_status_code
*    返 回 值: 0 成功(2xx),-1 失败。
*********************************************************************************************************
*/
static int upload_http_post_query(upload_http_link_t link_type,
                                  const char *host,
                                  uint16_t port,
                                  const char *path_with_query,
                                  uint8_t keep_alive,
                                  char *response,
                                  int response_size,
                                  int *http_status_code)
{
    char request_header[UPLOAD_HTTP_HEADER_MAX] = {0};
    int header_len = 0;
    int status_code = 0;

    if ((host == NULL) || (path_with_query == NULL))
    {
        return -1;
    }

    header_len = snprintf(request_header, sizeof(request_header),
                          "POST %s HTTP/1.1\r\n"
                          "Host: %s:%u\r\n"
                          "Content-Length: 0\r\n"
                          "Connection: %s\r\n\r\n",
                          path_with_query,
                          host,
                          (unsigned int)port,
                          (keep_alive != 0U) ? "keep-alive" : "close");
    if ((header_len < 0) || (header_len >= (int)sizeof(request_header)))
    {
        return -1;
    }

    if (upload_http_send_buffer(link_type, (const uint8_t *)request_header, (uint32_t)header_len) != 0)
    {
        return -1;
    }

    if (upload_http_recv_response(link_type, response, response_size, &status_code) != 0)
    {
        return -1;
    }

    if (http_status_code != NULL)
    {
        *http_status_code = status_code;
    }

    return ((status_code >= 200) && (status_code < 300)) ? 0 : -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_post_chunk
*    功能说明: 发送分片上传请求。data 必须是已从 Flash 读取出的本片文件数据。
*              本函数在同一个缓冲区内一次性组好完整 HTTP 包后再发送。
*    形    参: link_type / host / port / upload_id / file_name / chunk_index /
*              data / data_len / keep_alive / response / response_size / http_status_code
*    返 回 值: 0 成功(HTTP 2xx 且 code==200),-1 失败。
*********************************************************************************************************
*/
static int upload_http_post_chunk(upload_http_link_t link_type,
                                  const char *host,
                                  uint16_t port,
                                  const char *upload_id,
                                  const char *file_name,
                                  uint32_t chunk_index,
                                  const uint8_t *data,
                                  uint16_t data_len,
                                  uint8_t keep_alive,
                                  char *response,
                                  int response_size,
                                  int *http_status_code)
{
    uint8_t *packet = s_upload_http_chunk_request;
    uint32_t pos = 0U;
    int n = 0;
    int content_length = 0;
    int status_code = 0;

    if ((host == NULL) || (upload_id == NULL) || (file_name == NULL) || (file_name[0] == '\0') ||
        (data == NULL) || (data_len == 0U))
    {
        return -1;
    }

    memset(packet, 0, UPLOAD_HTTP_CHUNK_REQUEST_MAX);

    n = snprintf((char *)packet, UPLOAD_HTTP_CHUNK_REQUEST_MAX,
                    "--%s\r\n"
                    "Content-Disposition: form-data; name=\"uploadId\"\r\n\r\n"
                    "%s\r\n"
                    "--%s\r\n"
                    "Content-Disposition: form-data; name=\"chunkIndex\"\r\n\r\n"
                    "%lu\r\n"
                    "--%s\r\n"
                    "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
                    "Content-Type: application/octet-stream\r\n\r\n",
                    UPLOAD_HTTP_BOUNDARY,
                    upload_id,
                    UPLOAD_HTTP_BOUNDARY,
                    (unsigned long)chunk_index,
                    UPLOAD_HTTP_BOUNDARY,
                    file_name);
    if ((n < 0) || ((uint32_t)n >= UPLOAD_HTTP_CHUNK_REQUEST_MAX))
    {
        return -1;
    }
    pos = (uint32_t)n;

    if ((pos + (uint32_t)data_len) >= UPLOAD_HTTP_CHUNK_REQUEST_MAX)
    {
        return -1;
    }
    memcpy(packet + pos, data, (size_t)data_len);
    pos += (uint32_t)data_len;

    n = snprintf((char *)packet + pos, UPLOAD_HTTP_CHUNK_REQUEST_MAX - pos,
                 "\r\n--%s--\r\n",
                 UPLOAD_HTTP_BOUNDARY);
    if ((n < 0) || ((pos + (uint32_t)n) >= UPLOAD_HTTP_CHUNK_REQUEST_MAX))
    {
        return -1;
    }
    pos += (uint32_t)n;
    content_length = (int)pos;

    memmove(packet + UPLOAD_HTTP_HEADER_MAX, packet, (size_t)content_length);

    n = snprintf((char *)packet, UPLOAD_HTTP_HEADER_MAX,
                 "POST %s HTTP/1.1\r\n"
                 "Host: %s:%u\r\n"
                 "Content-Type: multipart/form-data; boundary=%s\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: %s\r\n\r\n",
                 UPLOAD_HTTP_PATH_CHUNK,
                 host,
                 (unsigned int)port,
                 UPLOAD_HTTP_BOUNDARY,
                 content_length,
                 (keep_alive != 0U) ? "keep-alive" : "close");
    if ((n < 0) || (n >= UPLOAD_HTTP_HEADER_MAX))
    {
        return -1;
    }

    pos = (uint32_t)n;
    if ((pos + (uint32_t)content_length) > UPLOAD_HTTP_CHUNK_REQUEST_MAX)
    {
        return -1;
    }
    memmove(packet + pos, packet + UPLOAD_HTTP_HEADER_MAX, (size_t)content_length);
    pos += (uint32_t)content_length;

    if (link_type == UPLOAD_HTTP_LINK_LWIP)
    {
        if (upload_http_send_lwip_all(packet, pos) != 0)
        {
            return -1;
        }
        if (upload_http_lwip_drain_tx() != 0)
        {
            return -1;
        }
    }
    else if (upload_http_send_buffer(link_type, packet, pos) != 0)
    {
        return -1;
    }

    if (upload_http_recv_response(link_type, response, response_size, &status_code) != 0)
    {
        return -1;
    }

    if (http_status_code != NULL)
    {
        *http_status_code = status_code;
    }

    if ((status_code < 200) || (status_code >= 300))
    {
        return -1;
    }

    return upload_http_parse_chunk_response(response);
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_post_finish
*    功能说明: 发送 finish 接口 POST 请求,query 参数 uploadId,可选 md5,校验应答 code==200。
*    形    参: link_type / host / port / upload_id / finish_md5_enable /
*              file_md5 / response / response_size / http_status_code
*    返 回 值: 0 成功(HTTP 2xx 且 code==200),-1 失败。
*********************************************************************************************************
*/
static int upload_http_post_finish(upload_http_link_t link_type,
                                   const char *host,
                                   uint16_t port,
                                   const char *upload_id,
                                   uint8_t finish_md5_enable,
                                   const char *file_md5,
                                   char *response,
                                   int response_size,
                                   int *http_status_code)
{
    char query_path[UPLOAD_HTTP_QUERY_PATH_MAX] = {0};
    int query_len = 0;

    if (host == NULL || upload_id == NULL)
    {
        return -1;
    }

    if (finish_md5_enable != UPLOAD_HTTP_FINISH_MD5_DISABLE)
    {
        if ((file_md5 == NULL) || (file_md5[0] == '\0'))
        {
            return -1;
        }

        query_len = snprintf(query_path, sizeof(query_path),
                             "%s?uploadId=%s&md5=%s",
                             UPLOAD_HTTP_PATH_FINISH,
                             upload_id,
                             file_md5);
    }
    else
    {
        query_len = snprintf(query_path, sizeof(query_path),
                             "%s?uploadId=%s",
                             UPLOAD_HTTP_PATH_FINISH,
                             upload_id);
    }
    if ((query_len < 0) || (query_len >= (int)sizeof(query_path)))
    {
        return -1;
    }

    if (upload_http_post_query(link_type, host, port, query_path,
                               0U, response, response_size, http_status_code) != 0)
    {
        return -1;
    }

    return upload_http_parse_chunk_response(response);
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_file_function
*    功能说明: HTTP 三步分片上传入口,支持 lwIP 与 GPRS 链路。
*              1. POST /fnwlw/oss/log/upload/start  上报设备编号、文件长度、总分片数,获取 uploadId
*              2. POST /fnwlw/oss/log/upload/chunk  按片上传数据(/log 路径仅上传 payload 明文)
*              3. POST /fnwlw/oss/log/upload/finish 通知服务端合并完成
*    形    参: request 上传请求参数; http_status_code 输出最后一次 HTTP 状态码(可为 NULL)
*    返 回 值: UPLOAD_HTTP_OK 成功,其他为错误码。
*********************************************************************************************************
*/
int8_t upload_http_file_function(const upload_http_request_t *request, int *http_status_code)
{
    upload_http_link_t link_type;
    const char *host = NULL;
    const char *upload_file_name = NULL;
    uint16_t port = 0U;
    char device_id[16] = {0};
    char upload_id[UPLOAD_HTTP_UPLOAD_ID_MAX] = {0};
    char query_path[UPLOAD_HTTP_QUERY_PATH_MAX] = {0};
    char response[UPLOAD_HTTP_RESP_MAX] = {0};
    char file_md5_hex[UPLOAD_HTTP_MD5_HEX_MAX] = {0};
    lfs_file_t lfs_fp;
    MD5_CTX md5_ctx;
    unsigned char md5_digest[16] = {0};
    uint8_t finish_md5_enable = UPLOAD_HTTP_FINISH_MD5_DISABLE;
    uint8_t *chunk_buf = s_upload_http_chunk_data;
    uint32_t file_size = 0U;
    uint32_t total_chunks = 0U;
    uint32_t chunk_index = 0U;
    uint32_t uploaded_chunks = 0U;
    uint8_t chunk_retry = 0U;
    int use_log_text = 0;
    int read_len = 0;
    int once_len = 0;
    int err = 0;
    int status_code = 0;
    int query_len = 0;

    if (http_status_code != NULL)
    {
        *http_status_code = 0;
    }

    if ((request == NULL) || (request->file_path == NULL) || (request->file_path[0] == '\0'))
    {
        return UPLOAD_HTTP_ERR_PARAM;
    }

    host = upload_http_safe_str(request->host, UPLOAD_HTTP_DEFAULT_HOST);
    port = (request->port != 0U) ? request->port : UPLOAD_HTTP_DEFAULT_PORT;
    upload_file_name = upload_http_get_file_name(request->file_path, request->upload_file_name);
    if ((upload_file_name == NULL) || (upload_file_name[0] == '\0'))
    {
        return UPLOAD_HTTP_ERR_PARAM;
    }
    link_type = upload_http_select_link(request->preferred_link);
    finish_md5_enable = (request->finish_md5_enable != UPLOAD_HTTP_FINISH_MD5_DISABLE) ?
                        UPLOAD_HTTP_FINISH_MD5_ENABLE : UPLOAD_HTTP_FINISH_MD5_DISABLE;
    use_log_text = upload_http_path_is_log(request->file_path);

    if (use_log_text != 0)
    {
        if (log_get_pending_text_size(&file_size) != LOG_OK)
        {
            return UPLOAD_HTTP_ERR_FILE;
        }
    }
    else if (upload_http_get_file_size(request->file_path, &file_size) != 0)
    {
        return UPLOAD_HTTP_ERR_FILE;
    }

    total_chunks = (file_size + UPLOAD_HTTP_CHUNK_SIZE - 1U) / UPLOAD_HTTP_CHUNK_SIZE;
    if (total_chunks == 0U)
    {
        total_chunks = 1U;
    }

    upload_http_get_device_id(request->device_sn, device_id, sizeof(device_id));

    if (link_type == UPLOAD_HTTP_LINK_LWIP)
    {
        if (upload_http_connect_lwip(host, port) != 0)
        {
            return UPLOAD_HTTP_ERR_CONNECT;
        }
    }
    else if (link_type == UPLOAD_HTTP_LINK_GPRS)
    {
        if (upload_http_connect_gprs(host, port) != 0)
        {
            return UPLOAD_HTTP_ERR_CONNECT;
        }
    }
    else
    {
        return UPLOAD_HTTP_ERR_LINK;
    }

    /* 1. 开始上传: POST query 参数,无请求体 */
    query_len = snprintf(query_path, sizeof(query_path),
                         "%s?deviceSn=%s&totalSize=%lu&totalChunks=%lu&fileName=%s&logType=%s",
                         UPLOAD_HTTP_PATH_START,
                         device_id,
                         (unsigned long)file_size,
                         (unsigned long)total_chunks,
                         upload_file_name,
                         upload_http_safe_str(request->log_type, "SYSTEM"));
    if ((query_len < 0) || (query_len >= (int)sizeof(query_path)))
    {
        upload_http_close_link(link_type);
        return UPLOAD_HTTP_ERR_PARAM;
    }

    memset(response, 0, sizeof(response));
    if (upload_http_post_query(link_type, host, port, query_path,
                               1U, response, (int)sizeof(response), &status_code) != 0)
    {
        upload_http_close_link(link_type);
        if (http_status_code != NULL)
        {
            *http_status_code = status_code;
        }
        return (status_code == 0) ? UPLOAD_HTTP_ERR_RESPONSE : UPLOAD_HTTP_ERR_STATUS;
    }

    if (upload_http_parse_start_response(response, upload_id, sizeof(upload_id)) != 0)
    {
        upload_http_close_link(link_type);
        if (http_status_code != NULL)
        {
            *http_status_code = status_code;
        }
        return UPLOAD_HTTP_ERR_RESPONSE;
    }

    /* 2. 上传分片,按需累计 MD5 */
    if (use_log_text != 0)
    {
        if (log_open_pending_text_stream() != LOG_OK)
        {
            upload_http_close_link(link_type);
            return UPLOAD_HTTP_ERR_FILE;
        }
    }
    else
    {
        err = lfs_file_open(&g_lfs_t, &lfs_fp, request->file_path, LFS_O_RDONLY);
        if (err != 0)
        {
            upload_http_close_link(link_type);
            return UPLOAD_HTTP_ERR_FILE;
        }
    }

    if (finish_md5_enable != UPLOAD_HTTP_FINISH_MD5_DISABLE)
    {
        MD5Init(&md5_ctx);
    }

    for (chunk_index = 0U; chunk_index < total_chunks; chunk_index++)
    {
        read_len = 0;

        if (use_log_text != 0)
        {
            if (log_read_pending_text_stream(chunk_buf, UPLOAD_HTTP_CHUNK_SIZE, &read_len) != LOG_OK)
            {
                log_close_pending_text_stream();
                upload_http_close_link(link_type);
                return UPLOAD_HTTP_ERR_FILE;
            }
        }
        else
        {
            while (read_len < (int)UPLOAD_HTTP_CHUNK_SIZE)
            {
                once_len = lfs_file_read(&g_lfs_t, &lfs_fp,
                                         chunk_buf + read_len,
                                         UPLOAD_HTTP_CHUNK_SIZE - (uint32_t)read_len);
                if (once_len < 0)
                {
                    (void)lfs_file_close(&g_lfs_t, &lfs_fp);
                    upload_http_close_link(link_type);
                    return UPLOAD_HTTP_ERR_FILE;
                }

                if (once_len == 0)
                {
                    break;
                }

                read_len += once_len;
            }
        }
        printf("lfs_file_read: read_len = %d\n, chunk_buf = %s\n", read_len, chunk_buf);
        if (read_len == 0)
        {
            break;
        }

        if (finish_md5_enable != UPLOAD_HTTP_FINISH_MD5_DISABLE)
        {
            MD5Update(&md5_ctx, chunk_buf, (unsigned int)read_len);
        }

        memset(response, 0, sizeof(response));
        for (chunk_retry = 0U; chunk_retry < UPLOAD_HTTP_CHUNK_RETRY_MAX; chunk_retry++)
        {
            if (upload_http_post_chunk(link_type, host, port, upload_id, upload_file_name,
                                       chunk_index, chunk_buf, (uint16_t)read_len, 1U,
                                       response, (int)sizeof(response), &status_code) == 0)
            {
                break;
            }

            if ((chunk_retry + 1U) < UPLOAD_HTTP_CHUNK_RETRY_MAX)
            {
                vTaskDelay(50);
            }
        }

        if (chunk_retry >= UPLOAD_HTTP_CHUNK_RETRY_MAX)
        {
            if (use_log_text != 0)
            {
                log_close_pending_text_stream();
            }
            else
            {
                (void)lfs_file_close(&g_lfs_t, &lfs_fp);
            }
            upload_http_close_link(link_type);
            if (http_status_code != NULL)
            {
                *http_status_code = status_code;
            }
            return (status_code == 0) ? UPLOAD_HTTP_ERR_SEND : UPLOAD_HTTP_ERR_STATUS;
        }

        uploaded_chunks++;
    }

    if (use_log_text != 0)
    {
        log_close_pending_text_stream();
    }
    else
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
    }
    if (uploaded_chunks != total_chunks)
    {
        upload_http_close_link(link_type);
        return UPLOAD_HTTP_ERR_FILE;
    }

    if (finish_md5_enable != UPLOAD_HTTP_FINISH_MD5_DISABLE)
    {
        MD5Final(&md5_ctx, md5_digest);
        upload_http_md5_to_hex(md5_digest, file_md5_hex, sizeof(file_md5_hex));
    }

    /* 3. 结束上传: POST query 参数 uploadId,可选 md5 */
    memset(response, 0, sizeof(response));
    if (upload_http_post_finish(link_type, host, port, upload_id, finish_md5_enable,
                                (finish_md5_enable != UPLOAD_HTTP_FINISH_MD5_DISABLE) ? file_md5_hex : NULL,
                                response, (int)sizeof(response), &status_code) != 0)
    {
        upload_http_close_link(link_type);
        if (http_status_code != NULL)
        {
            *http_status_code = status_code;
        }
        return (status_code == 0) ? UPLOAD_HTTP_ERR_RESPONSE : UPLOAD_HTTP_ERR_STATUS;
    }

    upload_http_close_link(link_type);

    if (http_status_code != NULL)
    {
        *http_status_code = status_code;
    }

    return UPLOAD_HTTP_OK;
}
