#ifndef _UPDATE_H_
#define _UPDATE_H_

#include "./SYSTEM/sys/sys.h"

#define UPDATE_MODE_NULL 0
#define UPDATE_MODE_LWIP 1
#define UPDATE_MODE_GPRS 2

#define UPDATE_NETWORK_CNT (6) // 网络连接次数


// 升级状态
typedef enum
{
    UPDATE_NONE = 0,
    UPDATE_SUCCESS = 1,
    UPDATE_FAILED = 2,
} update_status_t;

// 开机升级参数
struct BOOT_UPDATE_PARAM
{
    unsigned int is_update;     // true:需要升级, false:无需升级
    unsigned int section_size;  // 每包的实际数据(去掉校验2字节)大小
    unsigned int section_count; // 总包数
    unsigned int update_status; // 升级状态
};

/* 20201103 */
typedef struct
{
    uint8_t mode;            // 0-不更新 1-通过LWIP更新 2-通过GPRS更新
    uint8_t ip[4];             // 更新地址
    uint32_t port;             // 更新端口
    struct {
        uint8_t state;         // 状态
        uint8_t connect;     // 连接
    } tcp_t;

    struct {
        uint8_t connect;
    } gprs_t;
} update_param_t;

/* 供外部调用 */
void update_status_detection(void); 
void update_set_update_addr(void);   /* 每次更新前从系统配置同步 IP、端口 */
void update_set_update_mode(uint8_t mode);
uint8_t update_get_mode_function(void);
void *update_get_infor_data_function(void);

/* 有线 OTA 后台任务 */
void update_lwip_task_create(void);
void update_lwip_delete(void);   /* 在 eth 任务上下文同步删除已完成的有线OTA任务 */
uint8_t update_lwip_is_running(void);

/* 无线 OTA 后台任务 */
void update_gsm_task_create(void);
void update_gsm_delete(void);    /* 在 gsm 任务上下文同步删除已完成的无线OTA任务 */
uint8_t update_gsm_is_running(void);


void update_read_boot_param(struct BOOT_UPDATE_PARAM *boot_update_param);
void update_write_boot_param(struct BOOT_UPDATE_PARAM *boot_update_param);

#endif


