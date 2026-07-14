#ifndef _UPLOAD_HTTP_H_
#define _UPLOAD_HTTP_H_

#include "./SYSTEM/sys/sys.h"

/*
*********************************************************************************************************
*    HTTP 分片文件上传模块
*    协议流程: /fnwlw/oss/log/upload/start -> /fnwlw/oss/log/upload/chunk -> /fnwlw/oss/log/upload/finish
*    服务器: http://47.104.250.225:8080
*********************************************************************************************************
*/

/* HTTP 文件上传返回值 */
#define UPLOAD_HTTP_OK                    ((int8_t)0)   /* 成功 */
#define UPLOAD_HTTP_ERR_PARAM             ((int8_t)-21) /* 参数错误 */
#define UPLOAD_HTTP_ERR_LINK              ((int8_t)-22) /* 链路类型无效 */
#define UPLOAD_HTTP_ERR_RESOLVE           ((int8_t)-23) /* 域名解析失败 */
#define UPLOAD_HTTP_ERR_CONNECT           ((int8_t)-24) /* 连接失败 */
#define UPLOAD_HTTP_ERR_FILE              ((int8_t)-25) /* 文件操作失败 */
#define UPLOAD_HTTP_ERR_SEND              ((int8_t)-26) /* 发送失败 */
#define UPLOAD_HTTP_ERR_RESPONSE          ((int8_t)-27) /* 应答解析失败 */
#define UPLOAD_HTTP_ERR_STATUS            ((int8_t)-28) /* HTTP 状态码非 2xx */
#define UPLOAD_HTTP_ERR_NET               ((int8_t)-29) /* 网络不可用 */

/* 分片上传 API 路径 */
#define UPLOAD_HTTP_PATH_START            "/fnwlw/oss/log/upload/start"
#define UPLOAD_HTTP_PATH_CHUNK            "/fnwlw/oss/log/upload/chunk"
#define UPLOAD_HTTP_PATH_FINISH           "/fnwlw/oss/log/upload/finish"

#define UPLOAD_HTTP_CHUNK_SIZE            1024U /* 单分片最大字节数 */
#define UPLOAD_HTTP_UPLOAD_ID_MAX         64U   /* 开始上传返回 uploadId 最大长度 */
#define UPLOAD_HTTP_QUERY_PATH_MAX        384U  /* start 接口 query 路径缓冲 */

#define UPLOAD_HTTP_FINISH_MD5_DISABLE    0U    /* finish 不带 md5(默认) */
#define UPLOAD_HTTP_FINISH_MD5_ENABLE     1U    /* finish 携带 md5 校验 */

/* HTTP 上传请求参数 */
typedef struct
{
    const char *upload_file_name;  /* 上传日志文件名(必填,来自 log.c) */
    const char *log_type;          /* 日志类型,NULL 时默认 SYSTEM */
    uint8_t finish_md5_enable;     /* finish 是否携带 md5,默认 UPLOAD_HTTP_FINISH_MD5_DISABLE */
} upload_http_request_t;

/*
 * HTTP 传输层回调接口
 * 由 upload_lwip.c / upload_gsm.c 分别实现并注入,使 upload_http.c 传输无关。
 * recv 回调收到数据后须调用 upload_http_save_response() 追加到当前应答缓冲。
 */
typedef struct
{
    int  (*connect_fn)(const char *host, uint16_t port); /* 建立连接: 0 成功, -1 失败 */
    int  (*send_fn)(const uint8_t *data, uint32_t len);  /* 发送全部数据: 0 成功, -1 失败 */
    int  (*recv_fn)(int *out_recv_size);                 /* 收一包: 0 成功(含无数据), -1 错误, -3 断开 */
    void (*close_fn)(void);                              /* 关闭连接 */
} upload_http_transport_t;

/* 供传输层 recv 回调调用: 将收到的数据追加到当前应答缓冲, 0 成功 -1 溢出 */
int upload_http_save_response(const uint8_t *data, int data_len);

/*
*********************************************************************************************************
*    函 数 名: upload_http_file_function
*    功能说明: 执行三步分片上传(传输无关),详见 upload_http.c。
*    形    参: request 上传请求; transport 传输层回调; http_status_code 输出 HTTP 状态码(可为 NULL)
*    返 回 值: UPLOAD_HTTP_OK 或错误码。
*********************************************************************************************************
*/
int8_t upload_http_file_function(const upload_http_request_t *request,
                                 const upload_http_transport_t *transport,
                                 int *http_status_code);

#endif
