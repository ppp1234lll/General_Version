#ifndef _UPLOAD_H_
#define _UPLOAD_H_

#include "./SYSTEM/sys/sys.h"

typedef enum
{
    UPLOAD_MODE_NULL = 0,
    UPLOAD_MODE_LWIP = 1,
    UPLOAD_MODE_GPRS = 2,
} upload_mode_t;

#define UPLOAD_CHUNK_SIZE                   1024U
#define UPLOAD_DEFAULT_URL                  "/fnwlw/oss/deviceLog"

typedef struct
{
    uint8_t mode;
    uint8_t ip[4];
    uint32_t port;
    char url[128];
} upload_param_t;

/* 供外部调用 */
void upload_set_upload_addr(void);   /* 每次上传前从系统配置同步 IP、端口 */
void upload_set_upload_mode(uint8_t mode);
uint8_t upload_get_mode_function(void);
void *upload_get_infor_function(void);

/* 后台上传任务创建(由 eth/gsm 主循环按上传模式调用) */
void upload_lwip_task_create(void);
void upload_gsm_task_create(void);
void upload_lwip_delete(void);   /* 在 eth 任务上下文同步删除已完成的有线上传任务 */
void upload_gsm_delete(void);    /* 在 gsm 任务上下文同步删除已完成的无线上传任务 */

#endif


