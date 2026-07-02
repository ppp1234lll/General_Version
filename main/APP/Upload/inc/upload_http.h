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

/* 日志上传服务器默认地址 */
#define UPLOAD_HTTP_DEFAULT_HOST          "47.104.250.225"
#define UPLOAD_HTTP_DEFAULT_PORT          8080U

/* 分片上传 API 路径 */
#define UPLOAD_HTTP_PATH_START            "/fnwlw/oss/log/upload/start"
#define UPLOAD_HTTP_PATH_CHUNK            "/fnwlw/oss/log/upload/chunk"
#define UPLOAD_HTTP_PATH_FINISH           "/fnwlw/oss/log/upload/finish"

#define UPLOAD_HTTP_CHUNK_SIZE            1024U /* 单分片最大字节数 */
#define UPLOAD_HTTP_UPLOAD_ID_MAX         64U   /* 开始上传返回 uploadId 最大长度 */
#define UPLOAD_HTTP_QUERY_PATH_MAX        384U  /* start 接口 query 路径缓冲 */

#define UPLOAD_HTTP_FINISH_MD5_DISABLE    0U    /* finish 不带 md5(默认) */
#define UPLOAD_HTTP_FINISH_MD5_ENABLE     1U    /* finish 携带 md5 校验 */

/* 上传链路类型 */
typedef enum
{
    UPLOAD_HTTP_LINK_AUTO = 0, /* 自动选择 */
    UPLOAD_HTTP_LINK_LWIP = 1, /* 有线网络 */
    UPLOAD_HTTP_LINK_GPRS = 2  /* 4G 网络 */
} upload_http_link_t;

/* HTTP 上传请求参数 */
typedef struct
{
    const char *host;              /* 服务器地址,支持IP或域名,NULL时使用 UPLOAD_HTTP_DEFAULT_HOST */
    uint16_t port;                 /* 服务器端口,0时使用 UPLOAD_HTTP_DEFAULT_PORT */
    const char *url;               /* 保留字段,新协议不再使用 */
    const char *file_path;         /* LittleFS 文件路径 */
    const char *upload_file_name;  /* 上传日志文件名(如 000003_log_20260101120000.txt),必填 */
    const char *device_sn;         /* 设备编号,NULL时从 Flash 读取 */
    const char *log_type;          /* 日志类型,NULL时默认 SYSTEM */
    uint8_t finish_md5_enable;     /* finish 是否携带 md5,默认 UPLOAD_HTTP_FINISH_MD5_DISABLE */
    upload_http_link_t preferred_link; /* AUTO/LWIP/GPRS */
} upload_http_request_t;

/* 有线上传: 新建 TCP,读取 pending 日志,按 1KB 分包上传 */
int8_t upload_lwip_file_function(void);
void upload_lwip_poll(void);
uint8_t upload_lwip_is_running(void);


/* 无线上传: 自动读取 pending 日志,按 1KB 分包上传 */
int8_t upload_gsm_file_function(void);
void upload_gsm_poll(void);
uint8_t upload_gsm_is_running(void);
/*
*********************************************************************************************************
*    函 数 名: upload_http_file_function
*    功能说明: 执行三步分片上传,详见 upload_http.c。
*    形    参: request 上传请求; http_status_code 输出 HTTP 状态码(可为 NULL)
*    返 回 值: UPLOAD_HTTP_OK 或错误码。
*********************************************************************************************************
*/
int8_t upload_http_file_function(const upload_http_request_t *request, int *http_status_code);

#endif
