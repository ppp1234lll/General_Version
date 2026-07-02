#ifndef _GPRS_H_
#define _GPRS_H_

#include "./SYSTEM/sys/sys.h"
#include <stdio.h>
#include <assert.h>

/* 延时函数 */
#define GPRS_DELAY_MS(ms) vTaskDelay(ms)

#define GSM_RX_BUFF_SIZE (2048)

/* 调用方私有应答快照缓冲容量: send_cmd/send_at 在持锁时把本次应答拷到此大小
 * 的调用方 buffer, 解析私有副本, 不再读共享 gprs_rx_buff, 解除多链路并发解析竞态。
 * 足以容纳 +CEREG/+CSQ/+CCLK/+MIPCALL/+CGPADDR/+CGSN/+ICCID/+CME ERROR/+CGMR 模型串。 */
#define GPRS_RESP_SNAPSHOT_MAX (256)
////

// SIM卡状态
typedef enum
{
    SIM_EXT  = 0,      // 外部SIM卡
    SIM_INT  = 1,      // 贴片SIM卡
} GPRS_SIM_STATUS_E;

// 链路
typedef enum 
{
    GPRS_LINK_DATA = 0, // 数据平台
    GPRS_LINK_OTA = 1,  // OTA升级
    GPRS_LINK_FILE = 2, // 文件上传
} GPRS_LINK_E;

// 发送(数据、命令)状态
typedef enum
{
    GPRS_SEND_OK,      // 发送成功
    GPRS_SEND_TIMEOUT, // 发送超时
    GPRS_SEND_DISCONN, // 对端断开连接
    GPRS_SEND_ERROR,   // 发送异常
    GPRS_SEND_MAX
} GPRS_SEND_CODE_E;
////

// 4G模块通信状态
typedef enum
{
    GPRS_BOOT = 0,      // 开机
    GPRS_INIT ,         // 初始化
    GPRS_COMM_CHECK ,   // 通信检测
    GPRS_SIM ,          // SIM卡
    GPRS_CFUN ,         // 协议栈
    GPRS_CEREG ,        // 网络注册状态
    GPRS_CCLK ,         // 同步时间
    GPRS_MIPCCLK ,      // 查询链路状态
    GPRS_PDP ,          // 拨号激活
    GPRS_CGPADDR ,      // IP地址
    GPRS_CGMR ,         // 模块型号
    GPRS_IMEI ,         // IMEI
    GPRS_CSQ ,          // 信号状态
    GPRS_SUCCESS, 
} GPRS_COMM_STATUS_E;


struct gprs_status_t
{
    uint8_t sim_status; // SIM卡状态
    uint8_t mount;      // 挂载状态     1-挂载成功
    uint8_t network[3]; // TCP连接状态 [0]=DATA [1]=OTA [2]=FILE 1-连接成功
    
    uint8_t cmdon[3]; // 当前链路记录: 1-正在执行 0-未执行 [0]=DATA [1]=OTA [2]=FILE
    uint8_t disconn_pending[3]; // 1-disconn URC pending [0]=DATA [1]=OTA [2]=FILE
    uint8_t at_generic_cmd; // 1-通用AT命令(disconn走异步) 0-链路型命令
    uint8_t step;    // 串行操作步骤
    struct {
        uint8_t com; // 模块通信状态
        uint8_t sim; // SIM卡状态
        uint8_t csq; // 信号状态
        uint8_t net; // 网络注册状态
        uint8_t gps; // gps状态
        uint8_t ip[16]; // IP
    } status;
    uint8_t ccid[21];
    
    uint8_t imei[16];
    uint8_t model[40];
};
////
/* 4G模块状态日志 */
typedef struct 
{
    uint8_t init_step; // 初始化步骤
    uint8_t cereg;     // 网络注册状态
    uint8_t csq;       // 信号强度
    uint16_t errors;   // 错误数
}gprs_log_t;


struct GPRS_FEEDBACK
{
    const unsigned char *feedback;
    unsigned int feedback_len;
};
////

void gprs_gpio_init_function(void);                     // 引脚初始化函数
void gprs_init_function(void);                          // 初始化函数
void gprs_rx_streambuf_init_function(void);             // 创建GPRS接收流缓冲与任务(定义于gprs_rx.c)
void gprs_boot_up_function(void);                       // 模块开机函数
void gprs_shutdown_function(void);                      // 模块关机函数
void gprs_reset_function(void);                         // 复位重启函数
void gprs_v_reset_function(void);                       // 断电重启函数
void gprs_deinit_function(void);                        // 初始化-清除
int8_t gprs_status_check_function(void);                // 状态检测函数

void gprs_module_restart_function(void);                // 模块重启函数
uint8_t gprs_network_data_send_function(uint8_t *data, uint16_t len); // 网络数据发送函数
int gprs_check_data_disconn(void);                                  // 检查 DATA 链路异步断开

int8_t gprs_network_status_monitoring_function(void);   // 网络状态监测函数
void gprs_sim_status_monitoring_function(void);         // SIM卡状态监测函数
void gprs_csq_status_monitoring_function(void);         // CSQ信号监测函数
uint8_t gprs_get_module_status_function(void);          // 获取模块状态
uint8_t gprs_get_module_init_state(void);               // 获取模块初始化状态

uint8_t gprs_get_csq_function(void);                    // 获取模块信号强度
void *gprs_get_ip_addr_function(void);                  // 获取ip地址信息
void gprs_get_receive_data_function(uint8_t *buff, uint16_t len); // 获取通信数据缓冲区数据
uint8_t gprs_at_cmdon_active(void);                               // 任一路 AT 握手(cmdon)进行中
int gprs_rtcp_urc_frame_size(const uint8_t *buff, uint16_t avail); // rtcp URC 完整帧字节数(0=无效或未到齐)
void* gprs_get_infor_data_function(void);               // 获取模块信息指针
uint8_t *gprs_get_ccid_function(void);                  // 获取卡号
uint8_t *gprs_get_model_soft_function(void);            // 获取模块软件版本
uint8_t *gprs_get_imei_function(void);                  // 获取IMEI
void *gprs_get_log_function(void);                      // 获取模块日志
char* my_strstr(const char* str1, const char* str2);
////

extern int gprs_send_data(const uint8_t *data, int len, int waittime, GPRS_LINK_E client_id);
extern int gprs_network_connect_function(const char *host, unsigned short port, GPRS_LINK_E client_id);
extern int gprs_send_cmd
(
    const uint8_t *AT_cmd,
    int AT_cmd_len,
    const struct GPRS_FEEDBACK *feedback_array,
    unsigned int feedback_count,
    int waittime,
    int client_id,
    uint8_t *resp_out,   /* 调用方私有应答快照缓冲:非NULL时持锁拷出本次应答,解析私有副本,避免共享 gprs_rx_buff 并发竞态 */
    int resp_cap         /* resp_out 容量;<=0 或 resp_out=NULL 表示不快照 */
);
extern void gprs_network_disconnect_function(GPRS_LINK_E client_id);
extern int gprs_recv_data_ota(uint8_t *out_buf, int out_cap, int *out_size);
extern void gprs_reset_ota_rx_stream(void);
extern int gprs_recv_data_file(uint8_t *out_buf, int out_cap, int *out_size);
extern void trace_gprs_recv_buff(const unsigned char *send_buff, int send_size);

#include "FreeRTOS.h"
#include "semphr.h"
/* GPRS 接收互斥锁, 定义于 gprs_rx.c, 保护 gprs_rx_buff/ota/file 流缓冲 */
extern SemaphoreHandle_t g_gprs_rx_mutex;

/* AT 指令互斥锁:串行化 gprs_send_data / gprs_send_cmd / connect / disconnect,
 * 因为 ML307 串口无法同时处理多条 AT 指令。在 gprs_init_function 创建 */
//extern SemaphoreHandle_t g_gprs_at_mutex;
////

#endif
