#ifndef _LOG_H_
#define _LOG_H_

#include "./SYSTEM/sys/sys.h"

/* 日志模块目录规划 */
#define LOG_ROOT_DIR                "/log"
#define LOG_CURRENT_DIR             "/log/current"
#define LOG_PENDING_DIR             "/log/pending"

/* 日志模块固定文件路径 */
#define LOG_CATALOG_FILE_NAME       "catalog.idx"
#define LOG_CURRENT_META_FILE       "/log/current/batch.meta"
#define LOG_PENDING_META_FILE       "/log/pending/batch.meta"
#define LOG_CURRENT_CATALOG_FILE    "/log/current/catalog.idx"
#define LOG_PENDING_CATALOG_FILE    "/log/pending/catalog.idx"
#define LOG_CURRENT_LOG_FILE_NAME   "device.txt"

/* 日志模块容量与字符串长度限制 */
#define LOG_MAX_COUNT               (5000U)
#define LOG_FILE_NAME_MAX           (64U)
#define LOG_PATH_NAME_MAX           (96U)
#define LOG_TIME_STR_LEN            (15U)
#define LOG_QUERY_PAYLOAD_MAX       (2048U)

/* 文件内容与元数据校验标识 */
#define LOG_META_MAGIC              (0x4C4F474DU)
#define LOG_RECORD_MAGIC            (0x4C4F4752U)
#define LOG_CATALOG_MAGIC           (0x4C4F4743U)
#define LOG_META_VERSION            (0x0001U)

/*
*********************************************************************************************************
*                                           错误码表
*  LOG_OK               : 操作成功
*  LOG_ERR_MUTEX        : 日志模块互斥锁创建或获取失败
*  LOG_ERR_INIT         : 日志模块初始化失败
*  LOG_ERR_INVALID_PARAM: 输入参数非法
*  LOG_ERR_NOT_READY    : 当前批次未处于可写或可操作状态
*  LOG_ERR_STORAGE_FULL : 已存在 pending 批次，当前存储空间已满
*  LOG_ERR_FS           : littlefs 文件系统操作失败
*  LOG_ERR_APPEND       : 日志记录追加写入失败
*  LOG_ERR_ROTATE       : current/pending 批次切换失败
*  LOG_ERR_NO_PENDING   : 当前不存在待上传批次
*  LOG_ERR_NO_DATA      : 当前没有可封存或可上传的数据
*  LOG_ERR_CREATE_BATCH : 新批次创建失败
*  LOG_ERR_RECOVER      : 启动时异常批次恢复失败
*  LOG_ERR_MEMORY       : 查询过程中内存申请失败
*  LOG_ERR_DATA_TOO_LARGE: 日志记录长度超过查询工作缓冲区
*********************************************************************************************************
*/
/* 返回值定义 */
#define LOG_OK                      ((int8_t)0)
#define LOG_ERR_MUTEX               ((int8_t)-1)
#define LOG_ERR_INIT                ((int8_t)-2)
#define LOG_ERR_INVALID_PARAM       ((int8_t)-3)
#define LOG_ERR_NOT_READY           ((int8_t)-4)
#define LOG_ERR_STORAGE_FULL        ((int8_t)-5)
#define LOG_ERR_FS                  ((int8_t)-6)
#define LOG_ERR_APPEND              ((int8_t)-7)
#define LOG_ERR_ROTATE              ((int8_t)-8)
#define LOG_ERR_NO_PENDING          ((int8_t)-9)
#define LOG_ERR_NO_DATA             ((int8_t)-10)
#define LOG_ERR_CREATE_BATCH        ((int8_t)-11)
#define LOG_ERR_RECOVER             ((int8_t)-12)
#define LOG_ERR_MEMORY              ((int8_t)-13)
#define LOG_ERR_DATA_TOO_LARGE      ((int8_t)-14)

/* 批次状态 */
typedef enum
{
    LOG_BATCH_STATUS_IDLE = 0,
    LOG_BATCH_STATUS_WRITING = 1,
    LOG_BATCH_STATUS_PENDING = 2
} log_batch_status_t;

/* 单条日志记录头，位于实际 payload 之前 */
typedef struct
{
    uint32_t magic;
    uint32_t index;
    uint32_t timestamp;
    uint16_t payload_len;
    uint8_t reserved;
    uint32_t checksum;
} log_record_header_t;

/* 目录索引项，用于按时间范围快速定位日志记录 */
typedef struct
{
    uint32_t magic;
    uint32_t index;
    uint32_t timestamp;
    uint32_t record_offset;
    uint16_t payload_len;
    uint8_t reserved;
    uint32_t checksum;
} log_catalog_item_t;

/* 单个日志批次的元数据，分别保存在 current/pending 的 batch.meta 中 */
typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t status;
    uint32_t total_count;
    uint32_t next_index;
    uint32_t create_timestamp;
    uint32_t last_timestamp;
    char create_time[LOG_TIME_STR_LEN];
    char log_file[LOG_FILE_NAME_MAX];
} log_batch_meta_t;

/* 上传层使用的待上传文件信息 */
typedef struct
{
    uint8_t has_pending;
    uint32_t total_count;
    char log_file[LOG_PATH_NAME_MAX];       /* LittleFS 完整路径 */
    char log_file_name[LOG_FILE_NAME_MAX];  /* 上传用日志文件名 */
} log_upload_bundle_t;

/* 查询命中记录后的回调接口 */
typedef void (*log_query_callback_t)(const log_record_header_t *header,
                                     const uint8_t *payload,
                                     void *user_arg);

/* 批次自动轮转后通知上层的回调接口 */
typedef void (*log_rotate_notify_callback_t)(void);

/* 模块初始化与配置接口 */
int8_t log_init_function(void);
int8_t log_set_rotate_notify_callback(log_rotate_notify_callback_t callback);

/* 日志写入接口 */
int8_t log_device_write(const uint8_t *data, uint16_t len);

/* 日志查询接口 */
int8_t log_query_by_time_function(  uint32_t start_timestamp,
                                    uint32_t end_timestamp,
                                    log_query_callback_t callback,
                                    void *user_arg);

/* 上传批次控制接口 */
int8_t log_prepare_upload_function(log_upload_bundle_t *bundle);
int8_t log_get_pending_upload_function(log_upload_bundle_t *bundle);
int8_t log_upload_success_function(void);
int8_t log_upload_fail_function(void);

/* pending 明文流接口(HTTP 分片上传) */
int8_t log_get_pending_text_size(uint32_t *text_size);
int8_t log_open_pending_text_stream(void);
int8_t log_read_pending_text_stream(uint8_t *buf, uint32_t buf_size, int *read_len);
void log_close_pending_text_stream(void);

/* 获取 current 批次元数据快照指针 */
const log_batch_meta_t *log_get_meta_function(void);

/* 调试打印接口 */
int8_t log_print_meta_function(void);
int8_t log_print_pending_function(void);
int8_t log_print_all_function(void);
int8_t log_print_by_time_function(uint32_t start_timestamp, uint32_t end_timestamp);
int8_t log_print_dir_function(void);
int8_t log_clear_current_function(void);

#endif
