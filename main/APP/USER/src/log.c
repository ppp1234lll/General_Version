#include "main.h"
#include "./User/inc/log.h"

typedef union
{
    uint32_t value;
    uint8_t raw[4];
} log_device_id_u;

typedef struct
{
    uint32_t timestamp;
    char time_text[LOG_TIME_STR_LEN];
} log_time_info_t;

static log_batch_meta_t s_current_meta = {0};
static log_batch_meta_t s_pending_meta = {0};
static uint8_t s_log_inited = 0;
static uint8_t s_pending_valid = 0;
static SemaphoreHandle_t s_log_mutex = NULL;
static uint8_t s_query_payload_buf[LOG_QUERY_PAYLOAD_MAX] = {0};
static log_rotate_notify_callback_t s_rotate_notify_cb = NULL;

/*
*********************************************************************************************************
*    函 数 名: log_bcd_to_dec
*    功能说明: 将单个 BCD 值转换为十进制。
*    形    参: 见函数声明
*    返 回 值: 返回转换后的十进制值。
*********************************************************************************************************
*/
static uint8_t log_bcd_to_dec(uint8_t value)
{
    return (uint8_t)(((value >> 4) * 10U) + (value & 0x0FU));
}

/*
*********************************************************************************************************
*    函 数 名: log_mutex_init
*    功能说明: 按需创建日志模块互斥锁。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static int log_mutex_init(void)
{
    if (s_log_mutex == NULL)
    {
        s_log_mutex = xSemaphoreCreateMutex();
        if (s_log_mutex == NULL)
        {
            return LOG_ERR_MUTEX;
        }
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_mutex_lock
*    功能说明: 获取日志模块互斥锁。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static int log_mutex_lock(void)
{
    if (log_mutex_init() != 0)
    {
        return LOG_ERR_MUTEX;
    }

    if (xSemaphoreTake(s_log_mutex, portMAX_DELAY) != pdTRUE)
    {
        return LOG_ERR_MUTEX;
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_mutex_unlock
*    功能说明: 释放日志模块互斥锁。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static void log_mutex_unlock(void)
{
    if (s_log_mutex != NULL)
    {
        (void)xSemaphoreGive(s_log_mutex);
    }
}

/*
*********************************************************************************************************
*    函 数 名: log_simple_checksum_update
*    功能说明: 基于已有结果继续累加校验值。
*    形    参: 见函数声明
*    返 回 值: 返回计算得到的校验值。
*********************************************************************************************************
*/
static uint32_t log_simple_checksum_update(uint32_t checksum, const uint8_t *data, uint16_t len)
{
    uint16_t i = 0;

    if ((data == NULL) || (len == 0U))
    {
        return checksum;
    }

    for (i = 0; i < len; i++)
    {
        checksum = (checksum * 131U) + data[i];
    }

    return checksum;
}

/*
*********************************************************************************************************
*    函 数 名: log_simple_checksum
*    功能说明: 计算一段缓冲区的轻量校验值。
*    形    参: 见函数声明
*    返 回 值: 返回计算得到的校验值。
*********************************************************************************************************
*/
static uint32_t log_simple_checksum(const uint8_t *data, uint16_t len)
{
    return log_simple_checksum_update(0U, data, len);
}

/*
*********************************************************************************************************
*    函 数 名: log_record_checksum_calc
*    功能说明: 计算单条日志记录的校验值。
*    形    参: 见函数声明
*    返 回 值: 返回计算得到的校验值。
*********************************************************************************************************
*/
static uint32_t log_record_checksum_calc(const log_record_header_t *header, const uint8_t *payload)
{
    uint32_t checksum = 0;

    if (header == NULL)
    {
        return 0U;
    }

    checksum = log_simple_checksum( (const uint8_t *)header,
                                    (uint16_t)offsetof(log_record_header_t, checksum));
    checksum ^= log_simple_checksum(payload, header->payload_len);

    return checksum;
}

/*
*********************************************************************************************************
*    函 数 名: log_catalog_checksum_calc
*    功能说明: 计算单条目录项的校验值。
*    形    参: 见函数声明
*    返 回 值: 返回计算得到的校验值。
*********************************************************************************************************
*/
static uint32_t log_catalog_checksum_calc(const log_catalog_item_t *item)
{
    if (item == NULL)
    {
        return 0U;
    }

    return log_simple_checksum((const uint8_t *)item,
                               (uint16_t)offsetof(log_catalog_item_t, checksum));
}

/*
*********************************************************************************************************
*    函 数 名: log_catalog_item_valid
*    功能说明: 检查目录项内容是否有效。
*    形    参: 见函数声明
*    返 回 值: 1 表示有效，0 表示无效。
*********************************************************************************************************
*/
static int log_catalog_item_valid(const log_catalog_item_t *item)
{
    if (item == NULL)
    {
        return 0;
    }

    if (item->magic != LOG_CATALOG_MAGIC)
    {
        return 0;
    }

    if (item->checksum != log_catalog_checksum_calc(item))
    {
        return 0;
    }

    return 1;
}

/*
*********************************************************************************************************
*    函 数 名: log_record_header_valid
*    功能说明: 检查日志记录头内容是否有效。
*    形    参: 见函数声明
*    返 回 值: 1 表示有效，0 表示无效。
*********************************************************************************************************
*/
static int log_record_header_valid(const log_record_header_t *header)
{
    if (header == NULL)
    {
        return 0;
    }

    if (header->magic != LOG_RECORD_MAGIC)
    {
        return 0;
    }

    return 1;
}

/*
*********************************************************************************************************
*    函 数 名: log_get_time
*    功能说明: 读取 RTC 时间并生成时间字符串与时间戳。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static void log_get_time(log_time_info_t *time_info)
{
    rtc_parameter_struct rtc_now = {0};
    struct tm tm_now;
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    time_t epoch = 0;

    memset(&tm_now, 0, sizeof(tm_now));
    memset(time_info, 0, sizeof(log_time_info_t));

    rtc_current_time_get(&rtc_now);

    year = (uint16_t)(2000U + log_bcd_to_dec(rtc_now.year));
    month = log_bcd_to_dec(rtc_now.month);
    day = log_bcd_to_dec(rtc_now.date);
    hour = log_bcd_to_dec(rtc_now.hour);
    minute = log_bcd_to_dec(rtc_now.minute);
    second = log_bcd_to_dec(rtc_now.second);

    snprintf(time_info->time_text,
            sizeof(time_info->time_text),
            "%04u%02u%02u%02u%02u%02u",
            year,
            month,
            day,
            hour,
            minute,
            second);

    tm_now.tm_year = (int)year - 1900;
    tm_now.tm_mon = (int)month - 1;
    tm_now.tm_mday = day;
    tm_now.tm_hour = hour;
    tm_now.tm_min = minute;
    tm_now.tm_sec = second;
    tm_now.tm_isdst = -1;

    epoch = mktime(&tm_now);

    time_info->timestamp = (uint32_t)epoch;
}

/*
*********************************************************************************************************
*    函 数 名: log_get_device_id
*    功能说明: 读取用于文件命名的设备 ID。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static void log_get_device_id(char *device_id, uint32_t size)
{
    log_device_id_u dev_id = {0};

    if ((device_id == NULL) || (size == 0U))
    {
        return;
    }

    bsp_ReadCpuFlash(DEVICE_ID_ADDR, dev_id.raw, sizeof(dev_id.raw));
    dev_id.value &= 0x00FFFFFFUL;
    if ((dev_id.value == 0U) || (dev_id.value == 0x00FFFFFFUL))
    {
        dev_id.value = 3U;
    }

    snprintf(device_id, size, "%06lX", (unsigned long)dev_id.value);
}

/*
*********************************************************************************************************
*    函 数 名: log_build_file_name
*    功能说明: 根据设备 ID、类型和时间生成日志文件名。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static void log_build_file_name(char *name, uint32_t size, const char *type_name, const char *time_text)
{
    char device_id[16] = {0};

    log_get_device_id(device_id, sizeof(device_id));
    snprintf(name, size, "%s_%s_%s.txt", device_id, type_name, time_text);
}

/*
*********************************************************************************************************
*    函 数 名: log_find_upload_timestamp
*    功能说明: 在上传日志文件名中查找时间戳起始位置。
*    形    参: file_name : 日志文件名
*    返 回 值: 时间戳起始指针，失败返回 NULL。
*********************************************************************************************************
*/
static const char *log_find_upload_timestamp(const char *file_name)
{
    const char *markers[] = {"_log_", "_fault_", "_net4g_", NULL};
    const char *found = NULL;
    uint8_t i = 0U;

    if (file_name == NULL)
    {
        return NULL;
    }

    for (i = 0U; markers[i] != NULL; i++)
    {
        found = strstr(file_name, markers[i]);
        if (found != NULL)
        {
            return found + strlen(markers[i]);
        }
    }

    return NULL;
}

/*
*********************************************************************************************************
*    函 数 名: log_is_upload_log_name
*    功能说明: 判断文件名是否为上传命名格式。
*    形    参: 见函数声明
*    返 回 值: 1 表示是，0 表示否。
*********************************************************************************************************
*/
static int log_is_upload_log_name(const char *name)
{
    return (log_find_upload_timestamp(name) != NULL);
}

/*
*********************************************************************************************************
*    函 数 名: log_is_current_log_name
*    功能说明: 判断文件名是否为 current 批次固定文件名。
*    形    参: 见函数声明
*    返 回 值: 1 表示是，0 表示否。
*********************************************************************************************************
*/
static int log_is_current_log_name(const char *name)
{
    if (name == NULL)
    {
        return 0;
    }

    return (strcmp(name, LOG_CURRENT_LOG_FILE_NAME) == 0);
}

/*
*********************************************************************************************************
*    函 数 名: log_build_path
*    功能说明: 根据目录和文件名拼接完整路径。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static void log_build_path(char *path, uint32_t size, const char *dir, const char *name)
{
    snprintf(path, size, "%s/%s", dir, name);
}

/*
*********************************************************************************************************
*    函 数 名: log_path_exists
*    功能说明: 检查 littlefs 中的路径是否存在。
*    形    参: 见函数声明
*    返 回 值: 1 表示存在，0 表示不存在。
*********************************************************************************************************
*/
static int log_path_exists(const char *path)
{
    struct lfs_info info;
    int err = 0;

    memset(&info, 0, sizeof(info));
    err = lfs_stat(&g_lfs_t, path, &info);

    return (err == 0);
}

/*
*********************************************************************************************************
*    函 数 名: log_dir_has_files
*    功能说明: 检查目录下是否存在有效文件。
*    形    参: 见函数声明
*    返 回 值: 1 表示存在文件，0 表示不存在。
*********************************************************************************************************
*/
static int log_dir_has_files(const char *dir_path)
{
    lfs_dir_t dir;
    struct lfs_info info;
    int err = 0;

    memset(&dir, 0, sizeof(dir));
    memset(&info, 0, sizeof(info));

    err = lfs_dir_open(&g_lfs_t, &dir, dir_path);
    if (err != 0)
    {
        return 0;
    }

    while (1)
    {
        err = lfs_dir_read(&g_lfs_t, &dir, &info);
        if (err <= 0)
        {
            break;
        }

        if ((strcmp(info.name, ".") != 0) && (strcmp(info.name, "..") != 0))
        {
            (void)lfs_dir_close(&g_lfs_t, &dir);
            return 1;
        }
    }

    (void)lfs_dir_close(&g_lfs_t, &dir);
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: log_create_dir
*    功能说明: 在目录不存在时创建目录。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static void log_create_dir(const char *path)
{
    int err = 0;

    err = lfs_mkdir(&g_lfs_t, path);
    if ((err != 0) && (err != LFS_ERR_EXIST))
    {
        printf("log mkdir fail: %s, err=%d\r\n", path, err);
    }
}

/*
*********************************************************************************************************
*    函 数 名: log_remove_file_if_exists
*    功能说明: 在文件存在时将其删除。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static void log_remove_file_if_exists(const char *path)
{
    if (log_path_exists(path))
    {
        (void)lfs_remove(&g_lfs_t, path);
    }
}

/*
*********************************************************************************************************
*    函 数 名: log_clear_dir_files
*    功能说明: 删除指定目录下的全部文件。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static void log_clear_dir_files(const char *dir_path)
{
    lfs_dir_t dir;
    struct lfs_info info;
    char child_path[LOG_PATH_NAME_MAX] = {0};
    int err = 0;

    memset(&dir, 0, sizeof(dir));
    memset(&info, 0, sizeof(info));

    err = lfs_dir_open(&g_lfs_t, &dir, dir_path);
    if (err != 0)
    {
        return;
    }

    while (1)
    {
        err = lfs_dir_read(&g_lfs_t, &dir, &info);
        if (err <= 0)
        {
            break;
        }

        if ((strcmp(info.name, ".") == 0) || (strcmp(info.name, "..") == 0))
        {
            continue;
        }

        memset(child_path, 0, sizeof(child_path));
        snprintf(child_path, sizeof(child_path), "%s/%s", dir_path, info.name);
        (void)lfs_remove(&g_lfs_t, child_path);
    }

    (void)lfs_dir_close(&g_lfs_t, &dir);
}

/*
*********************************************************************************************************
*    函 数 名: log_create_empty_file
*    功能说明: 创建空文件或将已有文件清空。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int log_create_empty_file(const char *path)
{
    lfs_file_t lfs_fp = {0};
    int err = 0;

    err = lfs_file_open(&g_lfs_t, &lfs_fp, path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (err == 0)
    {
        err = lfs_file_close(&g_lfs_t, &lfs_fp);
    }

    return err;
}

/*
*********************************************************************************************************
*    函 数 名: log_meta_valid
*    功能说明: 检查批次元数据是否有效。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int log_meta_valid(const log_batch_meta_t *meta)
{
    if (meta == NULL)
    {
        return 0;
    }

    if ((meta->magic != LOG_META_MAGIC) || (meta->version != LOG_META_VERSION))
    {
        return 0;
    }

    if ((meta->status != LOG_BATCH_STATUS_WRITING) && (meta->status != LOG_BATCH_STATUS_PENDING))
    {
        return 0;
    }

    if (meta->log_file[0] == '\0')
    {
        return 0;
    }

    return 1;
}

/*
*********************************************************************************************************
*    函 数 名: log_current_meta_ready
*    功能说明: 检查 current 批次元数据是否可用于写入。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static int log_current_meta_ready(void)
{
    if (log_meta_valid(&s_current_meta) == 0)
    {
        return 0;
    }

    return (s_current_meta.status == LOG_BATCH_STATUS_WRITING);
}

/*
*********************************************************************************************************
*    函 数 名: log_meta_save
*    功能说明: 将批次元数据保存到指定文件。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int log_meta_save(const char *path, const log_batch_meta_t *meta)
{
    lfs_file_t lfs_fp = {0};
    int err = 0;

    err = lfs_file_open(&g_lfs_t, &lfs_fp, path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    err = lfs_file_write(&g_lfs_t, &lfs_fp, meta, sizeof(log_batch_meta_t));
    if (err == (int)sizeof(log_batch_meta_t))
    {
        err = lfs_file_close(&g_lfs_t, &lfs_fp);
        return err;
    }

    (void)lfs_file_close(&g_lfs_t, &lfs_fp);
    return LOG_ERR_FS;
}

/*
*********************************************************************************************************
*    函 数 名: log_meta_load
*    功能说明: 从指定文件加载批次元数据。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int log_meta_load(const char *path, log_batch_meta_t *meta)
{
    lfs_file_t lfs_fp = {0};
    int err = 0;

    memset(meta, 0, sizeof(log_batch_meta_t));

    err = lfs_file_open(&g_lfs_t, &lfs_fp, path, LFS_O_RDONLY);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    err = lfs_file_read(&g_lfs_t, &lfs_fp, meta, sizeof(log_batch_meta_t));
    (void)lfs_file_close(&g_lfs_t, &lfs_fp);
    if ((err != (int)sizeof(log_batch_meta_t)) || !log_meta_valid(meta))
    {
        memset(meta, 0, sizeof(log_batch_meta_t));
        return LOG_ERR_FS;
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_meta_update_current
*    功能说明: 持久化保存 current 批次元数据。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static int log_meta_update_current(void)
{
    return log_meta_save(LOG_CURRENT_META_FILE, &s_current_meta);
}

/*
*********************************************************************************************************
*    函 数 名: log_meta_update_pending
*    功能说明: 持久化保存 pending 批次元数据。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static int log_meta_update_pending(void)
{
    return log_meta_save(LOG_PENDING_META_FILE, &s_pending_meta);
}

/*
*********************************************************************************************************
*    函 数 名: log_prepare_pending_upload_names
*    功能说明: 将 pending 批次固定文件名改为上传所需命名格式。
*    形    参: 无
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int log_prepare_pending_upload_names(void)
{
    log_time_info_t time_info;
    char old_path[LOG_PATH_NAME_MAX] = {0};
    char new_path[LOG_PATH_NAME_MAX] = {0};
    char new_name[LOG_FILE_NAME_MAX] = {0};
    int err = 0;

    if (s_pending_valid == 0U)
    {
        return LOG_ERR_NO_PENDING;
    }

    if (log_is_current_log_name(s_pending_meta.log_file) == 0)
    {
        return LOG_OK;
    }

    log_get_time(&time_info);
    log_build_file_name(new_name, sizeof(new_name), "log", time_info.time_text);
    log_build_path(old_path, sizeof(old_path), LOG_PENDING_DIR, s_pending_meta.log_file);
    log_build_path(new_path, sizeof(new_path), LOG_PENDING_DIR, new_name);

    err = lfs_rename(&g_lfs_t, old_path, new_path);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    snprintf(s_pending_meta.log_file, sizeof(s_pending_meta.log_file), "%s", new_name);

    err = log_meta_update_pending();
    if (err != 0)
    {
        (void)lfs_rename(&g_lfs_t, new_path, old_path);
        snprintf(s_pending_meta.log_file, sizeof(s_pending_meta.log_file), "%s", LOG_CURRENT_LOG_FILE_NAME);
        return LOG_ERR_FS;
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_append_catalog_item
*    功能说明: 向 catalog.idx 追加一条目录项。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int log_append_catalog_item(const char *file_path, const log_catalog_item_t *item)
{
    lfs_file_t lfs_fp = {0};
    int err = 0;

    if ((file_path == NULL) || (item == NULL))
    {
        return LOG_ERR_INVALID_PARAM;
    }

    err = lfs_file_open(&g_lfs_t, &lfs_fp, file_path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    err = lfs_file_write(&g_lfs_t, &lfs_fp, item, sizeof(log_catalog_item_t));
    if (err != (int)sizeof(log_catalog_item_t))
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_APPEND;
    }

    err = lfs_file_close(&g_lfs_t, &lfs_fp);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_truncate_file_to_offset
*    功能说明: 将文件截断回指定偏移。
*    形    参: 见函数声明
*    返 回 值: 返回计算得到的校验值。
*********************************************************************************************************
*/
static int log_truncate_file_to_offset(const char *file_path, uint32_t offset)
{
    lfs_file_t lfs_fp = {0};
    lfs_soff_t pos = 0;
    int err = 0;

    if (file_path == NULL)
    {
        return LOG_ERR_INVALID_PARAM;
    }

    err = lfs_file_open(&g_lfs_t, &lfs_fp, file_path, LFS_O_WRONLY);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    pos = lfs_file_seek(&g_lfs_t, &lfs_fp, (lfs_soff_t)offset, LFS_SEEK_SET);
    if (pos < 0)
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_FS;
    }

    err = lfs_file_truncate(&g_lfs_t, &lfs_fp, (lfs_off_t)offset);
    if (err != 0)
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_FS;
    }

    err = lfs_file_close(&g_lfs_t, &lfs_fp);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_get_file_size
*    功能说明: 获取文件当前大小。
*    形    参: 见函数声明
*    返 回 值: 返回计算得到的校验值。
*********************************************************************************************************
*/
static int log_get_file_size(const char *file_path, uint32_t *file_size)
{
    lfs_file_t lfs_fp = {0};
    lfs_soff_t pos = 0;
    int err = 0;

    if ((file_path == NULL) || (file_size == NULL))
    {
        return LOG_ERR_INVALID_PARAM;
    }

    *file_size = 0U;
    if (log_path_exists(file_path) == 0)
    {
        return LOG_OK;
    }

    err = lfs_file_open(&g_lfs_t, &lfs_fp, file_path, LFS_O_RDONLY);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    pos = lfs_file_seek(&g_lfs_t, &lfs_fp, 0, LFS_SEEK_END);
    (void)lfs_file_close(&g_lfs_t, &lfs_fp);
    if (pos < 0)
    {
        return LOG_ERR_FS;
    }

    *file_size = (uint32_t)pos;
    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_validate_record_item
*    功能说明: 校验目录项引用的日志记录是否完整。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int log_validate_record_item(const char *file_path,
                                    const log_catalog_item_t *item,
                                    uint32_t *record_end)
{
    lfs_file_t lfs_fp = {0};
    log_record_header_t header;
    lfs_soff_t pos = 0;
    uint32_t payload_checksum = 0;
    uint16_t remain_len = 0;
    uint16_t read_len = 0;
    int err = 0;

    if ((file_path == NULL) || (item == NULL) || (record_end == NULL))
    {
        return LOG_ERR_INVALID_PARAM;
    }

    err = lfs_file_open(&g_lfs_t, &lfs_fp, file_path, LFS_O_RDONLY);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    pos = lfs_file_seek(&g_lfs_t, &lfs_fp, (lfs_soff_t)item->record_offset, LFS_SEEK_SET);
    if (pos < 0)
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_FS;
    }

    memset(&header, 0, sizeof(header));
    err = lfs_file_read(&g_lfs_t, &lfs_fp, &header, sizeof(header));
    if (err != (int)sizeof(header))
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_FS;
    }

    if ((log_record_header_valid(&header) == 0) ||
        (header.index != item->index) ||
        (header.timestamp != item->timestamp) ||
        (header.payload_len != item->payload_len))
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_FS;
    }

    remain_len = header.payload_len;
    while (remain_len > 0U)
    {
        read_len = (remain_len > LOG_QUERY_PAYLOAD_MAX) ? LOG_QUERY_PAYLOAD_MAX : remain_len;
        err = lfs_file_read(&g_lfs_t, &lfs_fp, s_query_payload_buf, read_len);
        if (err != read_len)
        {
            (void)lfs_file_close(&g_lfs_t, &lfs_fp);
            return LOG_ERR_FS;
        }

        payload_checksum = log_simple_checksum_update(payload_checksum, s_query_payload_buf, read_len);
        remain_len = (uint16_t)(remain_len - read_len);
    }

    if ((log_simple_checksum((const uint8_t *)&header,
                             (uint16_t)offsetof(log_record_header_t, checksum)) ^ payload_checksum) != header.checksum)
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_FS;
    }

    *record_end = item->record_offset + (uint32_t)sizeof(log_record_header_t) + (uint32_t)header.payload_len;
    (void)lfs_file_close(&g_lfs_t, &lfs_fp);
    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_time_text_to_timestamp
*    功能说明: 将 YYYYMMDDHHMMSS 格式时间字符串转换为时间戳。
*    形    参: 见函数声明
*    返 回 值: 转换后的时间戳，失败返回 0。
*********************************************************************************************************
*/
static uint32_t log_time_text_to_timestamp(const char *time_text)
{
    struct tm tm_val = {0};
    time_t epoch = 0;
    uint32_t year = 0U;
    uint32_t month = 0U;
    uint32_t day = 0U;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;

    if ((time_text == NULL) || (strlen(time_text) < 14U))
    {
        return 0U;
    }

    year = (uint32_t)((time_text[0] - '0') * 1000U + (time_text[1] - '0') * 100U +
                      (time_text[2] - '0') * 10U + (time_text[3] - '0'));
    month = (uint32_t)((time_text[4] - '0') * 10U + (time_text[5] - '0'));
    day = (uint32_t)((time_text[6] - '0') * 10U + (time_text[7] - '0'));
    hour = (uint32_t)((time_text[8] - '0') * 10U + (time_text[9] - '0'));
    minute = (uint32_t)((time_text[10] - '0') * 10U + (time_text[11] - '0'));
    second = (uint32_t)((time_text[12] - '0') * 10U + (time_text[13] - '0'));

    tm_val.tm_year = (int)year - 1900;
    tm_val.tm_mon = (int)month - 1;
    tm_val.tm_mday = (int)day;
    tm_val.tm_hour = (int)hour;
    tm_val.tm_min = (int)minute;
    tm_val.tm_sec = (int)second;
    tm_val.tm_isdst = -1;

    epoch = mktime(&tm_val);

    return (uint32_t)epoch;
}

/*
*********************************************************************************************************
*    函 数 名: log_restore_create_info_from_meta_file
*    功能说明: 从损坏的 batch.meta 中尽力恢复批次创建时间信息。
*    形    参: 见函数声明
*    返 回 值: 1 表示恢复成功，0 表示失败。
*********************************************************************************************************
*/
static int log_restore_create_info_from_meta_file(const char *path,
                                                  uint32_t *create_timestamp,
                                                  char *create_time,
                                                  uint32_t create_time_size)
{
    lfs_file_t lfs_fp = {0};
    log_batch_meta_t meta;
    int err = 0;

    if ((path == NULL) || (create_timestamp == NULL) || (create_time == NULL) || (create_time_size == 0U))
    {
        return 0;
    }

    memset(&meta, 0, sizeof(meta));
    err = lfs_file_open(&g_lfs_t, &lfs_fp, path, LFS_O_RDONLY);
    if (err != 0)
    {
        return 0;
    }

    err = lfs_file_read(&g_lfs_t, &lfs_fp, &meta, sizeof(meta));
    (void)lfs_file_close(&g_lfs_t, &lfs_fp);
    if (err != (int)sizeof(meta))
    {
        return 0;
    }

    if ((meta.magic != LOG_META_MAGIC) || (meta.version != LOG_META_VERSION))
    {
        return 0;
    }

    if (meta.create_timestamp != 0U)
    {
        *create_timestamp = meta.create_timestamp;
    }

    if (meta.create_time[0] != '\0')
    {
        memcpy(create_time, meta.create_time, create_time_size - 1U);
        create_time[create_time_size - 1U] = '\0';
    }

    return (*create_timestamp != 0U) ? 1 : 0;
}

/*
*********************************************************************************************************
*    函 数 名: log_restore_create_info_from_filename
*    功能说明: 从日志文件名中解析批次创建时间信息。
*    形    参: 见函数声明
*    返 回 值: 1 表示恢复成功，0 表示失败。
*********************************************************************************************************
*/
static int log_restore_create_info_from_filename(const char *file_name,
                                                 uint32_t *create_timestamp,
                                                 char *create_time,
                                                 uint32_t create_time_size)
{
    const char *time_start = NULL;
    uint32_t i = 0U;

    if ((file_name == NULL) || (create_timestamp == NULL) || (create_time == NULL) || (create_time_size < 15U))
    {
        return 0;
    }

    time_start = log_find_upload_timestamp(file_name);

    if (time_start == NULL)
    {
        return 0;
    }

    if (strlen(time_start) < 14U)
    {
        return 0;
    }

    for (i = 0U; i < 14U; i++)
    {
        if ((time_start[i] < '0') || (time_start[i] > '9'))
        {
            return 0;
        }
    }

    memcpy(create_time, time_start, 14U);
    create_time[14U] = '\0';
    *create_timestamp = log_time_text_to_timestamp(create_time);
    return (*create_timestamp != 0U) ? 1 : 0;
}

/*
*********************************************************************************************************
*    函 数 名: log_rebuild_meta_from_catalog
*    功能说明: 根据 catalog 和文件尾重建批次计数及时间元数据。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int log_rebuild_meta_from_catalog(log_batch_meta_t *meta, const char *dir_path)
{
    lfs_file_t lfs_fp = {0};
    log_catalog_item_t item;
    char log_path[LOG_PATH_NAME_MAX] = {0};
    char catalog_path[LOG_PATH_NAME_MAX] = {0};
    uint32_t expect_size = 0U;
    uint32_t file_size = 0U;
    uint32_t valid_catalog_size = 0U;
    uint32_t record_end = 0U;
    uint32_t first_timestamp = 0U;
    int err = 0;

    if ((meta == NULL) || (dir_path == NULL) || (meta->log_file[0] == '\0'))
    {
        return LOG_ERR_INVALID_PARAM;
    }

    meta->total_count = 0U;
    meta->next_index = 1U;
    meta->last_timestamp = meta->create_timestamp;

    log_build_path(log_path, sizeof(log_path), dir_path, meta->log_file);
    log_build_path(catalog_path, sizeof(catalog_path), dir_path, LOG_CATALOG_FILE_NAME);

    if (log_path_exists(catalog_path) != 0)
    {
        err = lfs_file_open(&g_lfs_t, &lfs_fp, catalog_path, LFS_O_RDONLY);
        if (err != 0)
        {
            return LOG_ERR_FS;
        }

        while (1)
        {
            memset(&item, 0, sizeof(item));
            err = lfs_file_read(&g_lfs_t, &lfs_fp, &item, sizeof(item));
            if (err == 0)
            {
                err = LOG_OK;
                break;
            }

            if ((err != (int)sizeof(item)) || (log_catalog_item_valid(&item) == 0))
            {
                err = LOG_OK;
                break;
            }

            err = log_validate_record_item(log_path, &item, &record_end);
            if (err != 0)
            {
                err = LOG_OK;
                break;
            }

            valid_catalog_size += (uint32_t)sizeof(log_catalog_item_t);
            if (meta->total_count == 0U)
            {
                first_timestamp = item.timestamp;
            }

            meta->total_count++;
            meta->next_index = item.index + 1U;
            meta->last_timestamp = item.timestamp;
            expect_size = record_end;
        }

        (void)lfs_file_close(&g_lfs_t, &lfs_fp);

        err = log_truncate_file_to_offset(catalog_path, valid_catalog_size);
        if (err != 0)
        {
            return err;
        }
    }
    else
    {
        err = log_create_empty_file(catalog_path);
        if (err != 0)
        {
            return LOG_ERR_RECOVER;
        }
    }

    if ((meta->create_timestamp == 0U) && (first_timestamp != 0U))
    {
        meta->create_timestamp = first_timestamp;
    }

    err = log_get_file_size(log_path, &file_size);
    if (err != 0)
    {
        return err;
    }

    if (file_size > expect_size)
    {
        err = log_truncate_file_to_offset(log_path, expect_size);
        if (err != 0)
        {
            return err;
        }
    }
    else if ((file_size == 0U) && (log_path_exists(log_path) == 0))
    {
        err = log_create_empty_file(log_path);
        if (err != 0)
        {
            return LOG_ERR_RECOVER;
        }
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_recover_current_consistency
*    功能说明: 根据 catalog 和文件尾恢复 current 批次一致性。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static int log_recover_current_consistency(void)
{
    log_batch_meta_t recover_meta;
    int err = 0;

    if (log_meta_valid(&s_current_meta) == 0)
    {
        return LOG_ERR_INVALID_PARAM;
    }

    recover_meta = s_current_meta;
    recover_meta.status = LOG_BATCH_STATUS_WRITING;

    err = log_rebuild_meta_from_catalog(&recover_meta, LOG_CURRENT_DIR);
    if (err != 0)
    {
        return err;
    }

    memcpy(&s_current_meta, &recover_meta, sizeof(s_current_meta));
    err = log_meta_update_current();
    if (err != 0)
    {
        return LOG_ERR_RECOVER;
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_fill_upload_bundle
*    功能说明: 根据批次元数据填充上传文件信息。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static void log_fill_upload_bundle(const log_batch_meta_t *meta, const char *dir_path, log_upload_bundle_t *bundle)
{
    if ((meta == NULL) || (bundle == NULL))
    {
        return;
    }

    memset(bundle, 0, sizeof(log_upload_bundle_t));
    bundle->has_pending = 1U;
    bundle->total_count = meta->total_count;

    log_build_path(bundle->log_file, sizeof(bundle->log_file), dir_path, meta->log_file);
    snprintf(bundle->log_file_name, sizeof(bundle->log_file_name), "%s", meta->log_file);
}

/*
*********************************************************************************************************
*    函 数 名: log_create_current_batch
*    功能说明: 创建新的 current 批次及相关文件。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static int log_create_current_batch(void)
{
    log_time_info_t time_info;
    char log_path[LOG_PATH_NAME_MAX] = {0};
    char catalog_path[LOG_PATH_NAME_MAX] = {0};

    memset(&s_current_meta, 0, sizeof(s_current_meta));
    log_get_time(&time_info);

    s_current_meta.magic = LOG_META_MAGIC;
    s_current_meta.version = LOG_META_VERSION;
    s_current_meta.status = LOG_BATCH_STATUS_WRITING;
    s_current_meta.next_index = 1U;
    s_current_meta.create_timestamp = time_info.timestamp;
    s_current_meta.last_timestamp = time_info.timestamp;

    memcpy(s_current_meta.create_time, time_info.time_text, sizeof(s_current_meta.create_time) - 1U);
    snprintf(s_current_meta.log_file, sizeof(s_current_meta.log_file), "%s", LOG_CURRENT_LOG_FILE_NAME);

    log_build_path(log_path, sizeof(log_path), LOG_CURRENT_DIR, s_current_meta.log_file);
    log_build_path(catalog_path, sizeof(catalog_path), LOG_CURRENT_DIR, LOG_CATALOG_FILE_NAME);

    if (log_create_empty_file(log_path) != 0)
    {
        return LOG_ERR_CREATE_BATCH;
    }

    if (log_create_empty_file(catalog_path) != 0)
    {
        return LOG_ERR_CREATE_BATCH;
    }

    if (log_meta_update_current() != 0)
    {
        return LOG_ERR_CREATE_BATCH;
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_recover_current_to_pending
*    功能说明: 将损坏的 current 批次转移到 pending。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static int log_recover_current_to_pending(void)
{
    lfs_dir_t dir;
    struct lfs_info info;
    char current_path[LOG_PATH_NAME_MAX] = {0};
    char pending_path[LOG_PATH_NAME_MAX] = {0};
    log_time_info_t time_info;
    int err = 0;

    if (s_pending_valid != 0U)
    {
        return LOG_ERR_RECOVER;
    }

    if (log_dir_has_files(LOG_CURRENT_DIR) == 0)
    {
        return LOG_ERR_NO_DATA;
    }

    log_create_dir(LOG_PENDING_DIR);
    log_clear_dir_files(LOG_PENDING_DIR);
    memset(&s_pending_meta, 0, sizeof(s_pending_meta));

    memset(&dir, 0, sizeof(dir));
    memset(&info, 0, sizeof(info));
    err = lfs_dir_open(&g_lfs_t, &dir, LOG_CURRENT_DIR);
    if (err != 0)
    {
        return LOG_ERR_RECOVER;
    }

    while (1)
    {
        err = lfs_dir_read(&g_lfs_t, &dir, &info);
        if (err <= 0)
        {
            break;
        }

        if ((strcmp(info.name, ".") == 0) || (strcmp(info.name, "..") == 0) ||
            (strcmp(info.name, "batch.meta") == 0) ||
            (strcmp(info.name, LOG_CATALOG_FILE_NAME) == 0))
        {
            continue;
        }

        if (log_is_current_log_name(info.name) || log_is_upload_log_name(info.name))
        {
            snprintf(s_pending_meta.log_file, sizeof(s_pending_meta.log_file), "%s", info.name);
        }
    }
    (void)lfs_dir_close(&g_lfs_t, &dir);

    if (s_pending_meta.log_file[0] == '\0')
    {
        return LOG_ERR_RECOVER;
    }

    s_pending_meta.magic = LOG_META_MAGIC;
    s_pending_meta.version = LOG_META_VERSION;
    s_pending_meta.status = LOG_BATCH_STATUS_PENDING;

    if (log_restore_create_info_from_meta_file(LOG_CURRENT_META_FILE,
                                               &s_pending_meta.create_timestamp,
                                               s_pending_meta.create_time,
                                               sizeof(s_pending_meta.create_time)) == 0)
    {
        if (log_restore_create_info_from_filename(s_pending_meta.log_file,
                                                  &s_pending_meta.create_timestamp,
                                                  s_pending_meta.create_time,
                                                  sizeof(s_pending_meta.create_time)) == 0)
        {
            log_get_time(&time_info);
            s_pending_meta.create_timestamp = time_info.timestamp;
            memcpy(s_pending_meta.create_time, time_info.time_text, sizeof(s_pending_meta.create_time) - 1U);
        }
    }

    if (s_pending_meta.log_file[0] != '\0')
    {
        log_build_path(current_path, sizeof(current_path), LOG_CURRENT_DIR, s_pending_meta.log_file);
        log_build_path(pending_path, sizeof(pending_path), LOG_PENDING_DIR, s_pending_meta.log_file);
        if (lfs_rename(&g_lfs_t, current_path, pending_path) != 0)
        {
            return LOG_ERR_RECOVER;
        }
    }

    log_build_path(current_path, sizeof(current_path), LOG_CURRENT_DIR, LOG_CATALOG_FILE_NAME);
    log_build_path(pending_path, sizeof(pending_path), LOG_PENDING_DIR, LOG_CATALOG_FILE_NAME);
    if (log_path_exists(current_path) != 0)
    {
        if (lfs_rename(&g_lfs_t, current_path, pending_path) != 0)
        {
            return LOG_ERR_RECOVER;
        }
    }

    log_remove_file_if_exists(LOG_CURRENT_META_FILE);

    err = log_rebuild_meta_from_catalog(&s_pending_meta, LOG_PENDING_DIR);
    if (err != 0)
    {
        return LOG_ERR_RECOVER;
    }

    if (log_meta_update_pending() != 0)
    {
        return LOG_ERR_RECOVER;
    }

    s_pending_valid = 1U;
    memset(&s_current_meta, 0, sizeof(s_current_meta));

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_append_record
*    功能说明: 向日志文件和目录索引追加一条记录。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int log_append_record(const char *file_path,
                             const uint8_t *payload,
                             uint16_t payload_len)
{
    log_time_info_t time_info;
    log_record_header_t header;
    log_catalog_item_t catalog_item;
    lfs_file_t lfs_fp = {0};
    char catalog_path[LOG_PATH_NAME_MAX] = {0};
    lfs_soff_t record_offset = 0;
    int err = 0;

    if ((payload_len > 0U) && (payload == NULL))
    {
        return LOG_ERR_INVALID_PARAM;
    }

    memset(&header, 0, sizeof(header));
    log_get_time(&time_info);

    header.magic = LOG_RECORD_MAGIC;
    header.index = s_current_meta.next_index;
    header.timestamp = time_info.timestamp;
    header.payload_len = payload_len;
    header.checksum = log_record_checksum_calc(&header, payload);

    err = lfs_file_open(&g_lfs_t, &lfs_fp, file_path, LFS_O_WRONLY | LFS_O_CREAT);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    record_offset = lfs_file_seek(&g_lfs_t, &lfs_fp, 0, LFS_SEEK_END);
    if (record_offset < 0)
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_FS;
    }

    err = lfs_file_write(&g_lfs_t, &lfs_fp, &header, sizeof(header));
    if (err != (int)sizeof(header))
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_APPEND;
    }

    if (payload_len > 0U)
    {
        err = lfs_file_write(&g_lfs_t, &lfs_fp, payload, payload_len);
        if (err != payload_len)
        {
            (void)lfs_file_close(&g_lfs_t, &lfs_fp);
            return LOG_ERR_APPEND;
        }
    }

    err = lfs_file_close(&g_lfs_t, &lfs_fp);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    memset(&catalog_item, 0, sizeof(catalog_item));
    catalog_item.magic = LOG_CATALOG_MAGIC;
    catalog_item.index = header.index;
    catalog_item.timestamp = header.timestamp;
    catalog_item.record_offset = (uint32_t)record_offset;
    catalog_item.payload_len = header.payload_len;
    catalog_item.checksum = log_catalog_checksum_calc(&catalog_item);

    log_build_path(catalog_path, sizeof(catalog_path), LOG_CURRENT_DIR, LOG_CATALOG_FILE_NAME);
    err = log_append_catalog_item(catalog_path, &catalog_item);
    if (err != 0)
    {
        (void)log_truncate_file_to_offset(file_path, (uint32_t)record_offset);
        return err;
    }

    s_current_meta.last_timestamp = time_info.timestamp;
    s_current_meta.total_count++;
    s_current_meta.next_index++;

    if (log_meta_update_current() != 0)
    {
        return LOG_ERR_FS;
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_batch_time_match
*    功能说明: 判断批次时间范围是否命中查询区间。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int log_batch_time_match(const log_batch_meta_t *meta,
                                uint32_t start_timestamp,
                                uint32_t end_timestamp)
{
    if (meta == NULL)
    {
        return 0;
    }

    if ((meta->total_count == 0U) || (meta->last_timestamp < start_timestamp) ||
        (meta->create_timestamp > end_timestamp))
    {
        return 0;
    }

    return 1;
}

/*
*********************************************************************************************************
*    函 数 名: log_query_record_by_catalog
*    功能说明: 根据目录项定位并读取一条日志记录。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int log_query_record_by_catalog(const log_batch_meta_t *meta,
                                       const char *dir_path,
                                       const log_catalog_item_t *item,
                                       log_query_callback_t callback,
                                       void *user_arg)
{
    lfs_file_t lfs_fp = {0};
    log_record_header_t header;
    char file_path[LOG_PATH_NAME_MAX] = {0};
    lfs_soff_t pos = 0;
    int err = 0;

    if ((meta == NULL) || (dir_path == NULL) || (item == NULL) || (callback == NULL))
    {
        return LOG_ERR_INVALID_PARAM;
    }

    if (item->payload_len > LOG_QUERY_PAYLOAD_MAX)
    {
        return LOG_ERR_DATA_TOO_LARGE;
    }

    log_build_path(file_path, sizeof(file_path), dir_path, meta->log_file);

    err = lfs_file_open(&g_lfs_t, &lfs_fp, file_path, LFS_O_RDONLY);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    pos = lfs_file_seek(&g_lfs_t, &lfs_fp, (lfs_soff_t)item->record_offset, LFS_SEEK_SET);
    if (pos < 0)
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_FS;
    }

    memset(&header, 0, sizeof(header));
    err = lfs_file_read(&g_lfs_t, &lfs_fp, &header, sizeof(header));
    if (err != (int)sizeof(header))
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_FS;
    }

    if ((log_record_header_valid(&header) == 0) ||
        (header.index != item->index) ||
        (header.timestamp != item->timestamp) ||
        (header.payload_len != item->payload_len))
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_FS;
    }

    if (header.payload_len > 0U)
    {
        err = lfs_file_read(&g_lfs_t, &lfs_fp, s_query_payload_buf, header.payload_len);
        if (err != header.payload_len)
        {
            (void)lfs_file_close(&g_lfs_t, &lfs_fp);
            return LOG_ERR_FS;
        }
    }

    if (header.checksum != log_record_checksum_calc(&header,
                                                    (header.payload_len > 0U) ? s_query_payload_buf : NULL))
    {
        (void)lfs_file_close(&g_lfs_t, &lfs_fp);
        return LOG_ERR_FS;
    }

    callback(&header, (header.payload_len > 0U) ? s_query_payload_buf : NULL, user_arg);
    (void)lfs_file_close(&g_lfs_t, &lfs_fp);

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_query_batch_by_time
*    功能说明: 按时间范围查询单个批次。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int log_query_batch_by_time(const log_batch_meta_t *meta,
                                   const char *dir_path,
                                   uint32_t start_timestamp,
                                   uint32_t end_timestamp,
                                   log_query_callback_t callback,
                                   void *user_arg)
{
    lfs_file_t lfs_fp = {0};
    log_catalog_item_t item;
    char file_path[LOG_PATH_NAME_MAX] = {0};
    int err = 0;

    if ((meta == NULL) || (dir_path == NULL) || (callback == NULL))
    {
        return LOG_ERR_INVALID_PARAM;
    }

    if (log_batch_time_match(meta, start_timestamp, end_timestamp) == 0)
    {
        return LOG_OK;
    }

    log_build_path(file_path, sizeof(file_path), dir_path, LOG_CATALOG_FILE_NAME);
    if (log_path_exists(file_path) == 0)
    {
        return LOG_OK;
    }

    err = lfs_file_open(&g_lfs_t, &lfs_fp, file_path, LFS_O_RDONLY);
    if (err != 0)
    {
        return LOG_ERR_FS;
    }

    while (1)
    {
        memset(&item, 0, sizeof(item));
        err = lfs_file_read(&g_lfs_t, &lfs_fp, &item, sizeof(item));
        if (err == 0)
        {
            err = LOG_OK;
            break;
        }

        if (err != (int)sizeof(item))
        {
            err = LOG_ERR_FS;
            break;
        }

        if (log_catalog_item_valid(&item) == 0)
        {
            continue;
        }

        if ((item.timestamp < start_timestamp) || (item.timestamp > end_timestamp))
        {
            continue;
        }

        err = log_query_record_by_catalog(meta, dir_path, &item, callback, user_arg);
        if (err != 0)
        {
            break;
        }
    }

    (void)lfs_file_close(&g_lfs_t, &lfs_fp);
    return err;
}

/*
*********************************************************************************************************
*    函 数 名: log_switch_current_to_pending
*    功能说明: 将 current 批次切换到 pending 并创建新批次。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static int log_switch_current_to_pending(void)
{
    char current_log[LOG_PATH_NAME_MAX] = {0};
    char current_catalog[LOG_PATH_NAME_MAX] = {0};
    char pending_log[LOG_PATH_NAME_MAX] = {0};
    char pending_catalog[LOG_PATH_NAME_MAX] = {0};
    int err = 0;

    if (s_pending_valid != 0U)
    {
        return LOG_ERR_STORAGE_FULL;
    }

    if (log_current_meta_ready() == 0)
    {
        return LOG_ERR_NOT_READY;
    }

    if (s_current_meta.total_count == 0U)
    {
        return LOG_ERR_NO_DATA;
    }

    log_create_dir(LOG_PENDING_DIR);
    if (log_dir_has_files(LOG_PENDING_DIR) != 0)
    {
        return LOG_ERR_RECOVER;
    }
    log_clear_dir_files(LOG_PENDING_DIR);

    s_current_meta.status = LOG_BATCH_STATUS_PENDING;
    if (log_meta_update_current() != 0)
    {
        s_current_meta.status = LOG_BATCH_STATUS_WRITING;
        return LOG_ERR_ROTATE;
    }

    log_build_path(current_log, sizeof(current_log), LOG_CURRENT_DIR, s_current_meta.log_file);
    log_build_path(current_catalog, sizeof(current_catalog), LOG_CURRENT_DIR, LOG_CATALOG_FILE_NAME);
    log_build_path(pending_log, sizeof(pending_log), LOG_PENDING_DIR, s_current_meta.log_file);
    log_build_path(pending_catalog, sizeof(pending_catalog), LOG_PENDING_DIR, LOG_CATALOG_FILE_NAME);

    err = lfs_rename(&g_lfs_t, current_log, pending_log);
    if (err != 0)
    {
        s_current_meta.status = LOG_BATCH_STATUS_WRITING;
        (void)log_meta_update_current();
        return LOG_ERR_ROTATE;
    }

    err = lfs_rename(&g_lfs_t, current_catalog, pending_catalog);
    if (err != 0)
    {
        (void)lfs_rename(&g_lfs_t, pending_log, current_log);
        s_current_meta.status = LOG_BATCH_STATUS_WRITING;
        (void)log_meta_update_current();
        return LOG_ERR_ROTATE;
    }

    err = lfs_rename(&g_lfs_t, LOG_CURRENT_META_FILE, LOG_PENDING_META_FILE);
    if (err != 0)
    {
        (void)lfs_rename(&g_lfs_t, pending_catalog, current_catalog);
        (void)lfs_rename(&g_lfs_t, pending_log, current_log);
        s_current_meta.status = LOG_BATCH_STATUS_WRITING;
        (void)log_meta_update_current();
        return LOG_ERR_ROTATE;
    }

    memcpy(&s_pending_meta, &s_current_meta, sizeof(s_pending_meta));
    s_pending_valid = 1U;

    err = log_create_current_batch();
    if (err != 0)
    {
        memset(&s_current_meta, 0, sizeof(s_current_meta));
        return LOG_ERR_CREATE_BATCH;
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_auto_rotate_if_needed
*    功能说明: 在 current 批次达到上限时自动轮转。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static int log_auto_rotate_if_needed(void)
{
    if (log_current_meta_ready() == 0)
    {
        return LOG_ERR_NOT_READY;
    }

    if (s_current_meta.total_count < LOG_MAX_COUNT)
    {
        return LOG_OK;
    }

    if (s_pending_valid != 0U)
    {
        return LOG_OK;
    }

    return log_switch_current_to_pending();
}

/*
*********************************************************************************************************
*    函 数 名: log_do_init
*    功能说明: 初始化日志模块并恢复已有批次。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static int log_do_init(void)
{
    int err = 0;

    if (log_mutex_init() != 0)
    {
        return LOG_ERR_MUTEX;
    }

    log_create_dir(LOG_ROOT_DIR);
    log_create_dir(LOG_CURRENT_DIR);
    log_create_dir(LOG_PENDING_DIR);

    memset(&s_current_meta, 0, sizeof(s_current_meta));
    memset(&s_pending_meta, 0, sizeof(s_pending_meta));
    s_pending_valid = 0U;

    if (log_meta_load(LOG_PENDING_META_FILE, &s_pending_meta) == 0)
    {
        s_pending_valid = 1U;
    }

    err = log_meta_load(LOG_CURRENT_META_FILE, &s_current_meta);
    if (err != 0)
    {
        if (log_dir_has_files(LOG_CURRENT_DIR) != 0)
        {
            if (s_pending_valid != 0U)
            {
                log_clear_dir_files(LOG_CURRENT_DIR);
                log_remove_file_if_exists(LOG_CURRENT_META_FILE);
                err = log_create_current_batch();
                if (err != 0)
                {
                    return LOG_ERR_CREATE_BATCH;
                }
            }
            else
            {
                err = log_recover_current_to_pending();
                if (err != 0)
                {
                    return LOG_ERR_RECOVER;
                }

                err = log_create_current_batch();
                if (err != 0)
                {
                    return LOG_ERR_CREATE_BATCH;
                }
            }
        }
        else
        {
            err = log_create_current_batch();
            if (err != 0)
            {
                return LOG_ERR_CREATE_BATCH;
            }
        }
    }
    else
    {
        err = log_recover_current_consistency();
        if (err != 0)
        {
            return LOG_ERR_RECOVER;
        }
        if (log_is_upload_log_name(s_current_meta.log_file))
        {
            log_clear_dir_files(LOG_CURRENT_DIR);
            log_remove_file_if_exists(LOG_CURRENT_META_FILE);
            memset(&s_current_meta, 0, sizeof(s_current_meta));
            err = log_create_current_batch();
            if (err != 0)
            {
                return LOG_ERR_CREATE_BATCH;
            }
        }
    }

    s_log_inited = 1U;
    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_init_function
*    功能说明: 日志模块初始化对外入口。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
int8_t log_init_function(void)
{
    int err = 0;

    err = log_mutex_lock();
    if (err != 0)
    {
        return LOG_ERR_MUTEX;
    }

    err = log_do_init();
    log_mutex_unlock();

    return (int8_t)err;
}

/*
*********************************************************************************************************
*    函 数 名: log_set_rotate_notify_callback
*    功能说明: 注册批次轮转通知回调，当自动轮转生成 pending 批次时调用。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功。
*********************************************************************************************************
*/
int8_t log_set_rotate_notify_callback(log_rotate_notify_callback_t callback)
{
    s_rotate_notify_cb = callback;
    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_write_common
*    功能说明: 各类日志共用的写入入口。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int8_t log_write_common(const uint8_t *payload, uint16_t len)
{
    char file_path[LOG_PATH_NAME_MAX] = {0};
    uint8_t was_pending = 0U;
    uint8_t rotated = 0U;
    int err = 0;

    err = log_mutex_lock();
    if (err != 0)
    {
        return LOG_ERR_MUTEX;
    }

    if (s_log_inited == 0U)
    {
        err = log_do_init();
        if (err != 0)
        {
            log_mutex_unlock();
            return LOG_ERR_INIT;
        }
    }

    if (log_current_meta_ready() == 0)
    {
        log_mutex_unlock();
        return LOG_ERR_STORAGE_FULL;
    }

    was_pending = s_pending_valid;

    if (s_current_meta.total_count >= LOG_MAX_COUNT)
    {
        if (s_pending_valid != 0U)
        {
            log_mutex_unlock();
            return LOG_ERR_STORAGE_FULL;
        }

        err = log_switch_current_to_pending();
        if (err != 0)
        {
            log_mutex_unlock();
            return (int8_t)err;
        }
    }

    log_build_path(file_path, sizeof(file_path), LOG_CURRENT_DIR, s_current_meta.log_file);

    err = log_append_record(file_path, payload, len);
    if (err != 0)
    {
        log_mutex_unlock();
        return (int8_t)err;
    }

    err = log_auto_rotate_if_needed();
    if (err != 0)
    {
        log_mutex_unlock();
        return (int8_t)err;
    }

    if ((was_pending == 0U) && (s_pending_valid != 0U))
    {
        rotated = 1U;
    }

    log_mutex_unlock();

    if ((rotated != 0U) && (s_rotate_notify_cb != NULL))
    {
        s_rotate_notify_cb();
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_device_write
*    功能说明: 追加一条设备日志记录。
*    形    参: data : 日志数据
*              len  : 数据长度
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
int8_t log_device_write(const uint8_t *data, uint16_t len)
{
    return log_write_common(data, len);
}

/*
*********************************************************************************************************
*    函 数 名: log_query_by_time_function
*    功能说明: 按时间范围查询日志。
*    形    参: 见函数声明
*    返 回 值: 返回计算得到的校验值。
*********************************************************************************************************
*/
int8_t log_query_by_time_function(  uint32_t start_timestamp,
                                    uint32_t end_timestamp,
                                    log_query_callback_t callback,
                                    void *user_arg)
{
    int err = 0;
    uint8_t has_batch = 0U;

    if ((callback == NULL) || (start_timestamp > end_timestamp))
    {
        return LOG_ERR_INVALID_PARAM;
    }

    err = log_mutex_lock();
    if (err != 0)
    {
        return LOG_ERR_MUTEX;
    }

    if (s_log_inited == 0U)
    {
        err = log_do_init();
        if (err != 0)
        {
            log_mutex_unlock();
            return LOG_ERR_INIT;
        }
    }

    if (s_pending_valid != 0U)
    {
        has_batch = 1U;
        err = log_query_batch_by_time(&s_pending_meta,
                                      LOG_PENDING_DIR,
                                      start_timestamp,
                                      end_timestamp,
                                      callback,
                                      user_arg);
        if (err != 0)
        {
            log_mutex_unlock();
            return (int8_t)err;
        }
    }

    if (log_meta_valid(&s_current_meta) != 0)
    {
        has_batch = 1U;
        err = log_query_batch_by_time(&s_current_meta,
                                      LOG_CURRENT_DIR,
                                      start_timestamp,
                                      end_timestamp,
                                      callback,
                                      user_arg);
        if (err != 0)
        {
            log_mutex_unlock();
            return (int8_t)err;
        }
    }

    log_mutex_unlock();

    if (has_batch == 0U)
    {
        return LOG_ERR_NO_DATA;
    }

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_prepare_upload_function
*    功能说明: 准备待上传批次文件信息。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
int8_t log_prepare_upload_function(log_upload_bundle_t *bundle)
{
    int err = 0;

    err = log_mutex_lock();
    if (err != 0)
    {
        return LOG_ERR_MUTEX;
    }

    if (s_log_inited == 0U)
    {
        err = log_do_init();
        if (err != 0)
        {
            log_mutex_unlock();
            return LOG_ERR_INIT;
        }
    }

    if (s_pending_valid == 0U)
    {
        if (log_current_meta_ready() == 0)
        {
            log_mutex_unlock();
            return LOG_ERR_NO_DATA;
        }

        err = log_switch_current_to_pending();
        if (err != 0)
        {
            log_mutex_unlock();
            return err;
        }
    }

    if (bundle != NULL)
    {
        err = log_prepare_pending_upload_names();
        if (err != 0)
        {
            log_mutex_unlock();
            return (int8_t)err;
        }
        log_fill_upload_bundle(&s_pending_meta, LOG_PENDING_DIR, bundle);
    }

    log_mutex_unlock();
    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_get_pending_upload_function
*    功能说明: 获取当前 pending 批次的上传信息。
*    形    参: 见函数声明
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
int8_t log_get_pending_upload_function(log_upload_bundle_t *bundle)
{
    int err = 0;

    err = log_mutex_lock();
    if (err != 0)
    {
        return LOG_ERR_MUTEX;
    }

    if ((s_pending_valid == 0U) || (bundle == NULL))
    {
        log_mutex_unlock();
        return (bundle == NULL) ? LOG_ERR_INVALID_PARAM : LOG_ERR_NO_PENDING;
    }

    err = log_prepare_pending_upload_names();
    if (err != 0)
    {
        log_mutex_unlock();
        return (int8_t)err;
    }

    log_fill_upload_bundle(&s_pending_meta, LOG_PENDING_DIR, bundle);
    log_mutex_unlock();
    return LOG_OK;
}

typedef struct
{
    uint32_t total_size;
} log_text_size_ctx_t;

typedef struct
{
    uint8_t opened;
    lfs_file_t catalog_fp;
    lfs_file_t log_fp;
    uint32_t payload_remain;
    uint32_t payload_off;
    uint8_t payload_buf[LOG_QUERY_PAYLOAD_MAX];
} log_text_stream_t;

static log_text_stream_t s_pending_text_stream = {0};

static void log_text_size_callback(const log_record_header_t *header,
                                   const uint8_t *payload,
                                   void *user_arg)
{
    log_text_size_ctx_t *ctx = (log_text_size_ctx_t *)user_arg;

    (void)payload;

    if ((header == NULL) || (ctx == NULL))
    {
        return;
    }

    ctx->total_size += header->payload_len;
}

static int log_text_stream_load_next_payload(void)
{
    log_catalog_item_t item;
    log_record_header_t header;
    int err = 0;

    s_pending_text_stream.payload_remain = 0U;
    s_pending_text_stream.payload_off = 0U;

    while (1)
    {
        memset(&item, 0, sizeof(item));
        err = lfs_file_read(&g_lfs_t, &s_pending_text_stream.catalog_fp, &item, sizeof(item));
        if (err == 0)
        {
            return 0;
        }

        if (err != (int)sizeof(item))
        {
            return LOG_ERR_FS;
        }

        if (log_catalog_item_valid(&item) == 0)
        {
            continue;
        }

        if (item.payload_len > LOG_QUERY_PAYLOAD_MAX)
        {
            return LOG_ERR_DATA_TOO_LARGE;
        }

        err = lfs_file_seek(&g_lfs_t, &s_pending_text_stream.log_fp, (lfs_soff_t)item.record_offset, LFS_SEEK_SET);
        if (err < 0)
        {
            return LOG_ERR_FS;
        }

        memset(&header, 0, sizeof(header));
        err = lfs_file_read(&g_lfs_t, &s_pending_text_stream.log_fp, &header, sizeof(header));
        if (err != (int)sizeof(header))
        {
            return LOG_ERR_FS;
        }

        if ((log_record_header_valid(&header) == 0) ||
            (header.index != item.index) ||
            (header.timestamp != item.timestamp) ||
            (header.payload_len != item.payload_len))
        {
            return LOG_ERR_FS;
        }

        if (header.payload_len > 0U)
        {
            err = lfs_file_read(&g_lfs_t, &s_pending_text_stream.log_fp,
                                s_pending_text_stream.payload_buf, header.payload_len);
            if (err != header.payload_len)
            {
                return LOG_ERR_FS;
            }
        }

        if (header.checksum != log_record_checksum_calc(&header,
                                                        (header.payload_len > 0U) ?
                                                        s_pending_text_stream.payload_buf : NULL))
        {
            return LOG_ERR_FS;
        }

        s_pending_text_stream.payload_remain = header.payload_len;
        return 1;
    }
}

static void log_text_stream_reset(void)
{
    if (s_pending_text_stream.opened != 0U)
    {
        (void)lfs_file_close(&g_lfs_t, &s_pending_text_stream.catalog_fp);
        (void)lfs_file_close(&g_lfs_t, &s_pending_text_stream.log_fp);
    }

    memset(&s_pending_text_stream, 0, sizeof(s_pending_text_stream));
}

/*
*********************************************************************************************************
*    函 数 名: log_get_pending_text_size
*    功能说明: 计算 pending 批次全部 payload 明文总长度(供 HTTP 上传用)。
*    形    参: text_size 输出总字节数
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
int8_t log_get_pending_text_size(uint32_t *text_size)
{
    log_text_size_ctx_t ctx = {0};
    int err = 0;

    if (text_size == NULL)
    {
        return LOG_ERR_INVALID_PARAM;
    }

    *text_size = 0U;

    err = log_mutex_lock();
    if (err != 0)
    {
        return LOG_ERR_MUTEX;
    }

    if (s_pending_valid == 0U)
    {
        log_mutex_unlock();
        return LOG_ERR_NO_PENDING;
    }

    err = log_query_batch_by_time(&s_pending_meta,
                                  LOG_PENDING_DIR,
                                  0U,
                                  0xFFFFFFFFU,
                                  log_text_size_callback,
                                  &ctx);
    if (err != 0)
    {
        log_mutex_unlock();
        return (int8_t)err;
    }

    *text_size = ctx.total_size;
    log_mutex_unlock();
    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_open_pending_text_stream
*    功能说明: 打开 pending 批次 payload 明文流(供 HTTP 分片顺序读取)。
*    形    参: 无
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
int8_t log_open_pending_text_stream(void)
{
    char catalog_path[LOG_PATH_NAME_MAX] = {0};
    char log_path[LOG_PATH_NAME_MAX] = {0};
    int err = 0;

    err = log_mutex_lock();
    if (err != 0)
    {
        return LOG_ERR_MUTEX;
    }

    log_text_stream_reset();

    if (s_pending_valid == 0U)
    {
        log_mutex_unlock();
        return LOG_ERR_NO_PENDING;
    }

    log_build_path(catalog_path, sizeof(catalog_path), LOG_PENDING_DIR, LOG_CATALOG_FILE_NAME);
    log_build_path(log_path, sizeof(log_path), LOG_PENDING_DIR, s_pending_meta.log_file);

    err = lfs_file_open(&g_lfs_t, &s_pending_text_stream.catalog_fp, catalog_path, LFS_O_RDONLY);
    if (err != 0)
    {
        log_mutex_unlock();
        return LOG_ERR_FS;
    }

    err = lfs_file_open(&g_lfs_t, &s_pending_text_stream.log_fp, log_path, LFS_O_RDONLY);
    if (err != 0)
    {
        (void)lfs_file_close(&g_lfs_t, &s_pending_text_stream.catalog_fp);
        memset(&s_pending_text_stream, 0, sizeof(s_pending_text_stream));
        log_mutex_unlock();
        return LOG_ERR_FS;
    }

    s_pending_text_stream.opened = 1U;
    log_mutex_unlock();
    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_read_pending_text_stream
*    功能说明: 从 pending 明文流顺序读取 payload 数据。
*    形    参: buf 输出缓冲; buf_size 缓冲容量; read_len 实际读取字节数
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
int8_t log_read_pending_text_stream(uint8_t *buf, uint32_t buf_size, int *read_len)
{
    int err = 0;
    int filled = 0;
    uint32_t copy_len = 0U;
    int load_ret = 0;

    if ((buf == NULL) || (read_len == NULL) || (buf_size == 0U))
    {
        return LOG_ERR_INVALID_PARAM;
    }

    *read_len = 0;

    err = log_mutex_lock();
    if (err != 0)
    {
        return LOG_ERR_MUTEX;
    }

    if (s_pending_text_stream.opened == 0U)
    {
        log_mutex_unlock();
        return LOG_ERR_NOT_READY;
    }

    while ((filled < (int)buf_size) && (s_pending_text_stream.opened != 0U))
    {
        if (s_pending_text_stream.payload_remain == 0U)
        {
            load_ret = log_text_stream_load_next_payload();
            if (load_ret == 0)
            {
                break;
            }

            if (load_ret < 0)
            {
                log_text_stream_reset();
                log_mutex_unlock();
                return (int8_t)load_ret;
            }
        }

        copy_len = s_pending_text_stream.payload_remain;
        if (copy_len > (uint32_t)((int)buf_size - filled))
        {
            copy_len = (uint32_t)((int)buf_size - filled);
        }

        if (copy_len > 0U)
        {
            memcpy(buf + filled,
                   s_pending_text_stream.payload_buf + s_pending_text_stream.payload_off,
                   copy_len);
            filled += (int)copy_len;
            s_pending_text_stream.payload_off += copy_len;
            s_pending_text_stream.payload_remain -= copy_len;
        }
    }

    *read_len = filled;
    log_mutex_unlock();
    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_close_pending_text_stream
*    功能说明: 关闭 pending payload 明文流。
*    形    参: 无
*    返 回 值: 无。
*********************************************************************************************************
*/
void log_close_pending_text_stream(void)
{
    if (log_mutex_lock() != 0)
    {
        return;
    }

    log_text_stream_reset();
    log_mutex_unlock();
}

/*
*********************************************************************************************************
*    函 数 名: log_upload_success_function
*    功能说明: 上传成功后清理 pending 文件。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
int8_t log_upload_success_function(void)
{
    char log_path[LOG_PATH_NAME_MAX] = {0};
    char catalog_path[LOG_PATH_NAME_MAX] = {0};
    int err = 0;

    err = log_mutex_lock();
    if (err != 0)
    {
        return LOG_ERR_MUTEX;
    }

    if (s_pending_valid == 0U)
    {
        log_mutex_unlock();
        return LOG_ERR_NO_PENDING;
    }

    log_build_path(log_path, sizeof(log_path), LOG_PENDING_DIR, s_pending_meta.log_file);
    log_build_path(catalog_path, sizeof(catalog_path), LOG_PENDING_DIR, LOG_CATALOG_FILE_NAME);

    log_remove_file_if_exists(log_path);
    log_remove_file_if_exists(catalog_path);
    log_remove_file_if_exists(LOG_PENDING_META_FILE);

    memset(&s_pending_meta, 0, sizeof(s_pending_meta));
    s_pending_valid = 0U;

    log_mutex_unlock();
    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_upload_fail_function
*    功能说明: 上传失败后保留 pending 文件。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
int8_t log_upload_fail_function(void)
{
    int8_t ret = LOG_ERR_NO_PENDING;

    if (log_mutex_lock() != 0)
    {
        return LOG_ERR_MUTEX;
    }

    ret = (s_pending_valid != 0U) ? LOG_OK : LOG_ERR_NO_PENDING;
    log_mutex_unlock();

    return ret;
}

/*
*********************************************************************************************************
*    函 数 名: log_get_meta_function
*    功能说明: 返回 current 批次元数据指针。
*    形    参: 见函数声明
*    返 回 值: 返回 current 批次元数据指针。
*********************************************************************************************************
*/
const log_batch_meta_t *log_get_meta_function(void)
{
    return &s_current_meta;
}

typedef struct
{
    uint32_t count;
} log_print_ctx_t;

/*
*********************************************************************************************************
*    函 数 名: log_print_record_callback
*    功能说明: 按条打印日志记录内容。
*    形    参: 见函数声明
*    返 回 值: 无。
*********************************************************************************************************
*/
static void log_print_record_callback(const log_record_header_t *header,
                                      const uint8_t *payload,
                                      void *user_arg)
{
    log_print_ctx_t *ctx = (log_print_ctx_t *)user_arg;
    uint16_t i = 0;

    if (header == NULL)
    {
        return;
    }

    if (ctx != NULL)
    {
        ctx->count++;
    }

    printf("[log] #%lu idx=%lu ts=%lu len=%u\r\n",
            (unsigned long)((ctx != NULL) ? ctx->count : 0U),
            (unsigned long)header->index,
            (unsigned long)header->timestamp,
            header->payload_len);

    if ((payload == NULL) || (header->payload_len == 0U))
    {
        return;
    }

    printf("[log] hex:");
    for (i = 0; i < header->payload_len; i++)
    {
        if ((i % 16U) == 0U)
        {
            printf("\r\n[log]      ");
        }

        printf("%02X ", payload[i]);
    }
    printf("\r\n");

    printf("[log] txt: %.*s\r\n", (int)header->payload_len, (const char *)payload);
}

/*
*********************************************************************************************************
*    函 数 名: log_print_meta_function
*    功能说明: 打印 current 批次元数据。
*    形    参: 无
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
int8_t log_print_meta_function(void)
{
    const log_batch_meta_t *meta = NULL;
    int err = 0;

    err = log_mutex_lock();
    if (err != 0)
    {
        return LOG_ERR_MUTEX;
    }

    if (s_log_inited == 0U)
    {
        err = log_do_init();
        if (err != 0)
        {
            log_mutex_unlock();
            return LOG_ERR_INIT;
        }
    }

    meta = &s_current_meta;
    if (log_meta_valid(meta) == 0)
    {
        printf("[log] current meta: invalid or empty\r\n");
    }
    else
    {
        printf("[log] current meta:\r\n");
        printf("  status=%u total=%lu next=%lu\r\n",
               meta->status,
               (unsigned long)meta->total_count,
               (unsigned long)meta->next_index);
        printf("  create_ts=%lu last_ts=%lu create_time=%s\r\n",
               (unsigned long)meta->create_timestamp,
               (unsigned long)meta->last_timestamp,
               meta->create_time);
        printf("  log_file=%s\r\n", meta->log_file);
    }

    log_mutex_unlock();
    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_print_pending_function
*    功能说明: 打印 pending 待上传批次信息。
*    形    参: 无
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
int8_t log_print_pending_function(void)
{
    log_upload_bundle_t bundle;
    int8_t ret = 0;

    ret = log_get_pending_upload_function(&bundle);
    if (ret != LOG_OK)
    {
        printf("[log] pending: none (ret=%d)\r\n", ret);
        return ret;
    }

    printf("[log] pending upload:\r\n");
    printf("  total=%lu\r\n",
            (unsigned long)bundle.total_count);
    printf("  log_file=%s\r\n", bundle.log_file);
    printf("  log_file_name=%s\r\n", bundle.log_file_name);

    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_print_by_time_function
*    功能说明: 按时间范围查询并打印日志记录。
*    形    参: start_timestamp 起始时间戳
*              end_timestamp   结束时间戳
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
int8_t log_print_by_time_function(uint32_t start_timestamp, uint32_t end_timestamp)
{
    log_print_ctx_t ctx = {0};
    int8_t ret = 0;

    if (start_timestamp > end_timestamp)
    {
        return LOG_ERR_INVALID_PARAM;
    }

    printf("[log] query range: %lu - %lu\r\n",
           (unsigned long)start_timestamp,
           (unsigned long)end_timestamp);

    ret = log_query_by_time_function(start_timestamp,
                                     end_timestamp,
                                     log_print_record_callback,
                                     &ctx);
    if (ret != LOG_OK)
    {
        printf("[log] query fail, ret=%d\r\n", ret);
        return ret;
    }

    printf("[log] query done, count=%lu\r\n", (unsigned long)ctx.count);
    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_print_all_function
*    功能说明: 打印 current 与 pending 批次中的全部日志。
*    形    参: 无
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
int8_t log_print_all_function(void)
{
    return log_print_by_time_function(0U, 0xFFFFFFFFU);
}

/*
*********************************************************************************************************
*    函 数 名: log_print_dir_indent
*    功能说明: 打印目录层级缩进，便于查看树形结构。
*    形    参: depth 当前目录深度
*    返 回 值: 无。
*********************************************************************************************************
*/
static void log_print_dir_indent(uint8_t depth)
{
    uint8_t i = 0;

    for (i = 0; i < depth; i++)
    {
        printf("  ");
    }
}

/*
*********************************************************************************************************
*    函 数 名: log_print_dir_recursive
*    功能说明: 递归遍历 littlefs 目录并打印文件信息。
*    形    参: dir_path 目录路径
*              depth    当前目录深度
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
static int8_t log_print_dir_recursive(const char *dir_path, uint8_t depth)
{
    lfs_dir_t dir;
    struct lfs_info info;
    char child_path[LOG_PATH_NAME_MAX] = {0};
    int err = 0;

    memset(&dir, 0, sizeof(dir));
    memset(&info, 0, sizeof(info));

    err = lfs_dir_open(&g_lfs_t, &dir, dir_path);
    if (err != 0)
    {
        log_print_dir_indent(depth);
        printf("[log] open dir fail: %s, err=%d\r\n", dir_path, err);
        return LOG_ERR_FS;
    }

    while (1)
    {
        memset(&info, 0, sizeof(info));
        err = lfs_dir_read(&g_lfs_t, &dir, &info);
        if (err < 0)
        {
            (void)lfs_dir_close(&g_lfs_t, &dir);
            log_print_dir_indent(depth);
            printf("[log] read dir fail: %s, err=%d\r\n", dir_path, err);
            return LOG_ERR_FS;
        }
        if (err == 0)
        {
            break;
        }

        if ((strcmp(info.name, ".") == 0) || (strcmp(info.name, "..") == 0))
        {
            continue;
        }

        memset(child_path, 0, sizeof(child_path));
        snprintf(child_path, sizeof(child_path), "%s/%s", dir_path, info.name);

        log_print_dir_indent(depth);
        if (info.type == LFS_TYPE_DIR)
        {
            printf("[DIR ] %s\r\n", child_path);
            if (log_print_dir_recursive(child_path, (uint8_t)(depth + 1U)) != LOG_OK)
            {
                (void)lfs_dir_close(&g_lfs_t, &dir);
                return LOG_ERR_FS;
            }
        }
        else
        {
            printf("[FILE] %s size=%lu\r\n", child_path, (unsigned long)info.size);
        }
    }

    (void)lfs_dir_close(&g_lfs_t, &dir);
    return LOG_OK;
}

/*
*********************************************************************************************************
*    函 数 名: log_print_dir_function
*    功能说明: 遍历目录，打印文件名和文件信息。
*    形    参: 无
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
int8_t log_print_dir_function(void)
{
    int err = 0;

    err = log_mutex_lock();
    if (err != 0)
    {
        return LOG_ERR_MUTEX;
    }

    if (s_log_inited == 0U)
    {
        err = log_do_init();
        if (err != 0)
        {
            log_mutex_unlock();
            return LOG_ERR_INIT;
        }
    }

    printf("[log] dir tree:\r\n");
    printf("[DIR ] %s\r\n", LOG_ROOT_DIR);
    err = log_print_dir_recursive(LOG_ROOT_DIR, 1U);

    log_mutex_unlock();
    return (int8_t)err;
}

/*
*********************************************************************************************************
*    函 数 名: log_clear_current_function
*    功能说明: 删除 current/pending 目录下全部文件,并重建 device.txt 批次。
*    形    参: 无
*    返 回 值: 0 表示成功，其他表示错误码。
*********************************************************************************************************
*/
int8_t log_clear_current_function(void)
{
    int err = 0;

    err = log_mutex_lock();
    if (err != 0)
    {
        return LOG_ERR_MUTEX;
    }

    if (s_log_inited == 0U)
    {
        err = log_do_init();
        if (err != 0)
        {
            log_mutex_unlock();
            return LOG_ERR_INIT;
        }
    }

    log_clear_dir_files(LOG_CURRENT_DIR);
    log_clear_dir_files(LOG_PENDING_DIR);
    memset(&s_current_meta, 0, sizeof(s_current_meta));
    memset(&s_pending_meta, 0, sizeof(s_pending_meta));
    s_pending_valid = 0U;

    err = log_create_current_batch();
    if (err != 0)
    {
        log_mutex_unlock();
        return (int8_t)err;
    }

    log_mutex_unlock();
    return LOG_OK;
}



