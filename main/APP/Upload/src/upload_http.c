#include "./Upload/inc/upload_http.h"
#include "./Upload/inc/upload.h"
#include "main.h"

/* multipart/form-data 分片上传边界 */
#define UPLOAD_HTTP_BOUNDARY              "----FNLOG_UPLOAD_BOUNDARY"
#define UPLOAD_HTTP_HEADER_MAX            512    /* HTTP 请求头缓冲 */
#define UPLOAD_HTTP_FORM_MAX              512    /* multipart 表单头缓冲 */
#define UPLOAD_HTTP_TAIL_MAX              64     /* multipart 结束边界缓冲 */
#define UPLOAD_HTTP_BODY_MAX              (UPLOAD_HTTP_FORM_MAX + UPLOAD_HTTP_CHUNK_SIZE + UPLOAD_HTTP_TAIL_MAX)
#define UPLOAD_HTTP_RESP_MAX             1024    /* HTTP 应答缓冲 */
#define UPLOAD_HTTP_RECV_TIMEOUT_MS      10000U  /* 等待应答超时(ms) */
#define UPLOAD_HTTP_CHUNK_RETRY_MAX      3       /* 分片上传失败重试次数 */
#define UPLOAD_HTTP_API_CODE_OK          200     /* 接口 JSON 成功 code 值 */
#define UPLOAD_HTTP_MD5_HEX_MAX          33U     /* MD5 十六进制字符串长度(32+1) */

/*
 * 上传会话上下文: 集中保存一次上传的所有共享状态,
 * 使各函数无需层层传参(参照 update_http.c 的 sg_http_update_param)。
 */
typedef struct
{
    const upload_http_transport_t *tp; /* 传输层回调 */
    char        host[16];              /* 服务器 IP(来自 sg_uploadparam_t) */
    uint16_t    port;
    const char *file_name;             /* 上传日志文件名(来自 log.c) */
    uint8_t     md5_enable;            /* finish 是否携带 md5 */
    uint32_t    total_chunks;          /* 总分片数 */
    char        upload_id[UPLOAD_HTTP_UPLOAD_ID_MAX];
    char        md5_hex[UPLOAD_HTTP_MD5_HEX_MAX];
    char        resp[UPLOAD_HTTP_RESP_MAX]; /* HTTP 应答缓冲 */
    int         resp_len;                   /* 已接收字节数 */
    int         status;                     /* 最近一次 HTTP 状态码 */
} upload_http_ctx_t;

static upload_http_ctx_t s_ctx;
static uint8_t s_body[UPLOAD_HTTP_BODY_MAX]; /* chunk multipart body 组包缓冲 */
static uint8_t s_chunk[UPLOAD_HTTP_CHUNK_SIZE]; /* 分片文件读缓冲 */

/*
*********************************************************************************************************
*    函 数 名: upload_http_safe_str
*    功能说明: 取有效字符串,空指针或空串时返回默认值。
*********************************************************************************************************
*/
static const char *upload_http_safe_str(const char *str, const char *def)
{
    return ((str != NULL) && (str[0] != '\0')) ? str : def;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_get_device_id
*    功能说明: 从系统参数(sg_sysparam_t.device)获取设备编号并格式化为十六进制字符串。
*********************************************************************************************************
*/
static void upload_http_get_device_id(char *out, uint32_t size)
{
    struct device_param *dev = (struct device_param *)app_get_device_param_function();
    uint32_t id = (dev != NULL) ? (dev->id.i & 0x00FFFFFFUL) : 0U;

    if ((id == 0U) || (id == 0x00FFFFFFUL))
    {
        id = 3U;
    }
    snprintf(out, size, "%lX", (unsigned long)id);
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_md5_to_hex
*    功能说明: 将 16 字节 MD5 摘要转为 32 位小写十六进制字符串。
*********************************************************************************************************
*/
static void upload_http_md5_to_hex(const unsigned char *digest, char *out)
{
    static const char tab[] = "0123456789abcdef";
    uint32_t i;

    for (i = 0U; i < 16U; i++)
    {
        out[i * 2U]      = tab[(digest[i] >> 4) & 0x0FU];
        out[i * 2U + 1U] = tab[digest[i] & 0x0FU];
    }
    out[32] = '\0';
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_save_response
*    功能说明: 供传输层 recv 回调调用,将收到的数据追加到应答缓冲。
*    返 回 值: 0 成功,-1 溢出/无效。
*********************************************************************************************************
*/
int upload_http_save_response(const uint8_t *data, int data_len)
{
    if ((data == NULL) || (data_len <= 0))
    {
        return -1;
    }
    if ((s_ctx.resp_len + data_len) >= (int)sizeof(s_ctx.resp))
    {
        data_len = (int)sizeof(s_ctx.resp) - 1 - s_ctx.resp_len;
    }
    if (data_len <= 0)
    {
        return -1;
    }
    memcpy(s_ctx.resp + s_ctx.resp_len, data, (size_t)data_len);
    s_ctx.resp_len += data_len;
    s_ctx.resp[s_ctx.resp_len] = '\0';
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_response_complete
*    功能说明: 判断应答是否收完(chunked 结束块 或 Content-Length 满足)。
*********************************************************************************************************
*/
static int upload_http_response_complete(void)
{
    const char *head_end;
    const char *pt;
    int body_size = 0;

    head_end = strstr(s_ctx.resp, "\r\n\r\n");
    if (head_end == NULL)
    {
        return 0;
    }

    pt = strstr(s_ctx.resp, "Transfer-Encoding:");
    if ((pt != NULL) && (pt < head_end))
    {
        return (strstr(head_end, "\r\n0\r\n\r\n") != NULL) ? 1 : 0;
    }

    pt = strstr(s_ctx.resp, "Content-Length:");
    if ((pt == NULL) || (sscanf(pt, "Content-Length: %d", &body_size) != 1))
    {
        return 1; /* 无长度信息, 视为已收完 */
    }
    return ((s_ctx.resp_len - (int)(head_end - s_ctx.resp) - 4) >= body_size) ? 1 : 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_recv
*    功能说明: 循环调用 transport->recv 收取应答直至完整或超时,并解析 HTTP 状态码到 s_ctx.status。
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_recv(void)
{
    uint32_t begin = HAL_GetTick();
    const char *pt;
    int got;
    int r;

    s_ctx.resp_len = 0;
    s_ctx.resp[0] = '\0';
    s_ctx.status = 0;

    while ((HAL_GetTick() - begin) < UPLOAD_HTTP_RECV_TIMEOUT_MS)
    {
        got = 0;
        r = s_ctx.tp->recv_fn(&got);
        if (r == -3)
        {
            break; /* 链路断开,尝试解析已收内容 */
        }
        if (r != 0)
        {
            return -1;
        }

        if (got > 0)
        {
            begin = HAL_GetTick();
            if (upload_http_response_complete())
            {
                break;
            }
        }
        else
        {
            if (upload_http_response_complete())
            {
                break;
            }
            vTaskDelay(10);
        }
    }

    pt = strstr(s_ctx.resp, "HTTP/");
    if ((pt == NULL) || (sscanf(pt, "HTTP/%*d.%*d %d", &s_ctx.status) != 1))
    {
        return -1;
    }
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_json_int / upload_http_json_str
*    功能说明: 从应答中按 "key" 提取整型/字符串字段(直接在原始应答里搜索,无需先解码)。
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_json_int(const char *key, int *out)
{
    char pat[24];
    const char *p;

    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(s_ctx.resp, pat);
    if (p == NULL)
    {
        return -1;
    }
    p = strchr(p, ':');
    if (p == NULL)
    {
        return -1;
    }
    return (sscanf(p + 1, "%d", out) == 1) ? 0 : -1;
}

static int upload_http_json_str(const char *key, char *out, uint32_t size)
{
    char pat[24];
    const char *p;
    const char *e;
    int len;

    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(s_ctx.resp, pat);
    if (p == NULL)
    {
        return -1;
    }
    p = strchr(p, ':');
    if (p == NULL)
    {
        return -1;
    }
    p++;
    while ((*p == ' ') || (*p == '\t') || (*p == '\"'))
    {
        p++;
    }
    e = p;
    while ((*e != '\0') && (*e != '\"') && (*e != ',') && (*e != '}'))
    {
        e++;
    }
    len = (int)(e - p);
    if ((len <= 0) || (len >= (int)size))
    {
        return -1;
    }
    memcpy(out, p, (size_t)len);
    out[len] = '\0';
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_api_ok
*    功能说明: 校验应答 JSON 的 code 字段是否为 200。
*    返 回 值: 0 是,-1 否。
*********************************************************************************************************
*/
static int upload_http_api_ok(void)
{
    int code = 0;
    return ((upload_http_json_int("code", &code) == 0) && (code == UPLOAD_HTTP_API_CODE_OK)) ? 0 : -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_post_query
*    功能说明: 发送无请求体的 POST(start/finish),收应答。
*    返 回 值: 0 成功(HTTP 2xx),-1 失败。
*********************************************************************************************************
*/
static int upload_http_post_query(const char *path, int keep_alive)
{
    char hdr[UPLOAD_HTTP_HEADER_MAX];
    int n;

    n = snprintf(hdr, sizeof(hdr),
                 "POST %s HTTP/1.1\r\n"
                 "Host: %s:%u\r\n"
                 "Content-Length: 0\r\n"
                 "Connection: %s\r\n\r\n",
                 path, s_ctx.host, (unsigned int)s_ctx.port,
                 keep_alive ? "keep-alive" : "close");
    if ((n < 0) || (n >= (int)sizeof(hdr)))
    {
        return -1;
    }
    if (s_ctx.tp->send_fn((const uint8_t *)hdr, (uint32_t)n) != 0)
    {
        return -1;
    }
    if (upload_http_recv() != 0)
    {
        return -1;
    }
    return ((s_ctx.status >= 200) && (s_ctx.status < 300)) ? 0 : -1;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_post_chunk
*    功能说明: 组 multipart 分片请求(header 与 body 分两次发送),收应答并校验 code==200。
*    返 回 值: 0 成功,-1 失败。
*********************************************************************************************************
*/
static int upload_http_post_chunk(uint32_t index, const uint8_t *data, uint16_t len)
{
    char hdr[UPLOAD_HTTP_HEADER_MAX];
    uint32_t pos;
    int n;

    /* 1) multipart body */
    n = snprintf((char *)s_body, UPLOAD_HTTP_BODY_MAX,
                 "--%s\r\nContent-Disposition: form-data; name=\"uploadId\"\r\n\r\n%s\r\n"
                 "--%s\r\nContent-Disposition: form-data; name=\"chunkIndex\"\r\n\r\n%lu\r\n"
                 "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
                 "Content-Type: application/octet-stream\r\n\r\n",
                 UPLOAD_HTTP_BOUNDARY, s_ctx.upload_id,
                 UPLOAD_HTTP_BOUNDARY, (unsigned long)index,
                 UPLOAD_HTTP_BOUNDARY, s_ctx.file_name);
    if ((n < 0) || (((uint32_t)n + len + UPLOAD_HTTP_TAIL_MAX) >= UPLOAD_HTTP_BODY_MAX))
    {
        return -1;
    }
    pos = (uint32_t)n;
    memcpy(s_body + pos, data, len);
    pos += len;
    n = snprintf((char *)s_body + pos, UPLOAD_HTTP_BODY_MAX - pos, "\r\n--%s--\r\n", UPLOAD_HTTP_BOUNDARY);
    if (n < 0)
    {
        return -1;
    }
    pos += (uint32_t)n;

    /* 2) 请求头(Content-Length = body 长度) */
    n = snprintf(hdr, sizeof(hdr),
                 "POST %s HTTP/1.1\r\n"
                 "Host: %s:%u\r\n"
                 "Content-Type: multipart/form-data; boundary=%s\r\n"
                 "Content-Length: %lu\r\n"
                 "Connection: keep-alive\r\n\r\n",
                 UPLOAD_HTTP_PATH_CHUNK, s_ctx.host, (unsigned int)s_ctx.port,
                 UPLOAD_HTTP_BOUNDARY, (unsigned long)pos);
    if ((n < 0) || (n >= (int)sizeof(hdr)))
    {
        return -1;
    }

    /* 3) 先发 header 再发 body,然后收应答 */
    if (s_ctx.tp->send_fn((const uint8_t *)hdr, (uint32_t)n) != 0)
    {
        return -1;
    }
    if (s_ctx.tp->send_fn(s_body, pos) != 0)
    {
        return -1;
    }
    if (upload_http_recv() != 0)
    {
        return -1;
    }
    if ((s_ctx.status < 200) || (s_ctx.status >= 300))
    {
        return -1;
    }
    return upload_http_api_ok();
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_do_start
*    功能说明: 第 1 步——POST start 上报设备/文件信息,解析获取 uploadId。
*    返 回 值: UPLOAD_HTTP_OK 或错误码。
*********************************************************************************************************
*/
static int8_t upload_http_do_start(const upload_http_request_t *request, uint32_t file_size)
{
    char device_id[16] = {0};
    char query[UPLOAD_HTTP_QUERY_PATH_MAX];
    int n;

    upload_http_get_device_id(device_id, sizeof(device_id));

    n = snprintf(query, sizeof(query),
                 "%s?deviceSn=%s&totalSize=%lu&totalChunks=%lu&fileName=%s&logType=%s",
                 UPLOAD_HTTP_PATH_START, device_id,
                 (unsigned long)file_size, (unsigned long)s_ctx.total_chunks,
                 s_ctx.file_name, upload_http_safe_str(request->log_type, "SYSTEM"));
    if ((n < 0) || (n >= (int)sizeof(query)))
    {
        return UPLOAD_HTTP_ERR_PARAM;
    }

    if (upload_http_post_query(query, 1) != 0)
    {
        return (s_ctx.status == 0) ? UPLOAD_HTTP_ERR_RESPONSE : UPLOAD_HTTP_ERR_STATUS;
    }
    if ((upload_http_api_ok() != 0) ||
        (upload_http_json_str("uploadId", s_ctx.upload_id, sizeof(s_ctx.upload_id)) != 0) ||
        (s_ctx.upload_id[0] == '\0'))
    {
        return UPLOAD_HTTP_ERR_RESPONSE;
    }
    return UPLOAD_HTTP_OK;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_upload_chunks
*    功能说明: 第 2 步——从日志明文流按 1KB 分片循环读取并上传(带重试),按需累计并生成 MD5。
*    返 回 值: UPLOAD_HTTP_OK 或错误码。
*********************************************************************************************************
*/
static int8_t upload_http_upload_chunks(void)
{
    MD5_CTX md5;
    unsigned char digest[16];
    uint32_t idx;
    uint32_t done = 0U;
    int read_len;
    int retry;

    if (log_open_pending_text_stream() != LOG_OK)
    {
        return UPLOAD_HTTP_ERR_FILE;
    }

    if (s_ctx.md5_enable)
    {
        MD5Init(&md5);
    }

    for (idx = 0U; idx < s_ctx.total_chunks; idx++)
    {
        read_len = 0;
        if (log_read_pending_text_stream(s_chunk, UPLOAD_HTTP_CHUNK_SIZE, &read_len) != LOG_OK)
        {
            log_close_pending_text_stream();
            return UPLOAD_HTTP_ERR_FILE;
        }
        if (read_len == 0)
        {
            break;
        }

        if (s_ctx.md5_enable)
        {
            MD5Update(&md5, s_chunk, (unsigned int)read_len);
        }

        for (retry = 0; retry < UPLOAD_HTTP_CHUNK_RETRY_MAX; retry++)
        {
            if (upload_http_post_chunk(idx, s_chunk, (uint16_t)read_len) == 0)
            {
                break;
            }
            if ((retry + 1) < UPLOAD_HTTP_CHUNK_RETRY_MAX)
            {
                vTaskDelay(50);
            }
        }
        if (retry >= UPLOAD_HTTP_CHUNK_RETRY_MAX)
        {
            log_close_pending_text_stream();
            return (s_ctx.status == 0) ? UPLOAD_HTTP_ERR_SEND : UPLOAD_HTTP_ERR_STATUS;
        }

        done++;
    }

    log_close_pending_text_stream();

    if (done != s_ctx.total_chunks)
    {
        return UPLOAD_HTTP_ERR_FILE;
    }

    if (s_ctx.md5_enable)
    {
        MD5Final(&md5, digest);
        upload_http_md5_to_hex(digest, s_ctx.md5_hex);
    }
    return UPLOAD_HTTP_OK;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_do_finish
*    功能说明: 第 3 步——POST finish(可选 md5),校验 code==200。
*    返 回 值: UPLOAD_HTTP_OK 或错误码。
*********************************************************************************************************
*/
static int8_t upload_http_do_finish(void)
{
    char query[UPLOAD_HTTP_QUERY_PATH_MAX];
    int n;

    if (s_ctx.md5_enable)
    {
        n = snprintf(query, sizeof(query), "%s?uploadId=%s&md5=%s",
                     UPLOAD_HTTP_PATH_FINISH, s_ctx.upload_id, s_ctx.md5_hex);
    }
    else
    {
        n = snprintf(query, sizeof(query), "%s?uploadId=%s",
                     UPLOAD_HTTP_PATH_FINISH, s_ctx.upload_id);
    }
    if ((n < 0) || (n >= (int)sizeof(query)))
    {
        return UPLOAD_HTTP_ERR_PARAM;
    }

    if (upload_http_post_query(query, 0) != 0)
    {
        return (s_ctx.status == 0) ? UPLOAD_HTTP_ERR_RESPONSE : UPLOAD_HTTP_ERR_STATUS;
    }
    return (upload_http_api_ok() == 0) ? UPLOAD_HTTP_OK : UPLOAD_HTTP_ERR_RESPONSE;
}

/*
*********************************************************************************************************
*    函 数 名: upload_http_file_function
*    功能说明: HTTP 三步分片上传入口(传输无关): start -> chunk -> finish。
*    形    参: request 上传请求; transport 传输回调; http_status_code 输出状态码(可为 NULL)
*    返 回 值: UPLOAD_HTTP_OK 成功,其他为错误码。
*********************************************************************************************************
*/
int8_t upload_http_file_function(const upload_http_request_t *request,
                                 const upload_http_transport_t *transport,
                                 int *http_status_code)
{
    uint32_t file_size = 0U;
    upload_param_t *param;
    int8_t ret;

    /* 每次上传前从系统配置同步最新 IP、端口 */
    upload_set_upload_addr();
    param = (upload_param_t *)upload_get_infor_function();

    if (http_status_code != NULL)
    {
        *http_status_code = 0;
    }

    if ((request == NULL) ||
        (request->upload_file_name == NULL) || (request->upload_file_name[0] == '\0') ||
        (transport == NULL) || (transport->connect_fn == NULL) || (transport->send_fn == NULL) ||
        (transport->recv_fn == NULL) || (transport->close_fn == NULL))
    {
        return UPLOAD_HTTP_ERR_PARAM;
    }

    /* 初始化会话上下文(只上传日志明文流) */
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.tp = transport;
    snprintf(s_ctx.host, sizeof(s_ctx.host), "%u.%u.%u.%u",
             (unsigned int)param->ip[0], (unsigned int)param->ip[1],
             (unsigned int)param->ip[2], (unsigned int)param->ip[3]);
    s_ctx.port = (uint16_t)param->port;
    s_ctx.file_name = request->upload_file_name;
    s_ctx.md5_enable = (request->finish_md5_enable != UPLOAD_HTTP_FINISH_MD5_DISABLE) ? 1U : 0U;

    /* 日志明文总长度 -> 总分片数(至少 1 片) */
    if (log_get_pending_text_size(&file_size) != LOG_OK)
    {
        return UPLOAD_HTTP_ERR_FILE;
    }
    s_ctx.total_chunks = (file_size + UPLOAD_HTTP_CHUNK_SIZE - 1U) / UPLOAD_HTTP_CHUNK_SIZE;
    if (s_ctx.total_chunks == 0U)
    {
        s_ctx.total_chunks = 1U;
    }

    if (transport->connect_fn(s_ctx.host, s_ctx.port) != 0)
    {
        return UPLOAD_HTTP_ERR_CONNECT;
    }

    ret = upload_http_do_start(request, file_size);
    if (ret == UPLOAD_HTTP_OK)
    {
        ret = upload_http_upload_chunks();
    }
    if (ret == UPLOAD_HTTP_OK)
    {
        ret = upload_http_do_finish();
    }

    transport->close_fn();

    if (http_status_code != NULL)
    {
        *http_status_code = s_ctx.status;
    }
    return ret;
}
