#ifndef _GPRS_H_
#define _GPRS_H_

#include "./SYSTEM/sys/sys.h"
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

/* 延时函数 */
#define GPRS_DELAY_MS(ms) vTaskDelay(ms)
#define GPRS_DATA_BUFF_SIZE (2048)
#define GPRS_AT_BUFF_SIZE 	(256)

typedef struct
{
    uint16_t status;
    uint8_t  buff[GPRS_DATA_BUFF_SIZE];
    uint16_t take_point;  
} gprs_rx_t;


// SIM卡状态
typedef enum
{
    SIM_EXT  = 0,      // 外置SIM卡
    SIM_INT  = 1,      // 贴片SIM卡
} GPRS_SIM_STATUS_E;

// 链接
typedef enum 
{
    GPRS_LINK_DATA = 0, //数据平台
    GPRS_LINK_OTA , // OTA升级
    GPRS_LINK_FILE , // 文件上传
    GPRS_LINK_MAX , // 最大链路数
} GPRS_LINK_E;

// 发送(数据、命令)状态
enum GPRS_SEND_CODE_E
{
	GPRS_SEND_OK,      // 发送成功
	GPRS_SEND_TIMEOUT, // 发送超时
	GPRS_SEND_DISCONN, // 对端断开了连接
	GPRS_SEND_ERROR,   // 发送异常
	////

	GPRS_SEND_MAX
};
////

// 4G模块通信状态
typedef enum
{
	GPRS_BOOT = 0, // 开机
	GPRS_INIT , // 初始化
	GPRS_COMM_CHECK , // 通信检测
	GPRS_SIM , // SIM卡
	GPRS_CFUN , // 协议栈
	GPRS_CEREG , // 网络注册状态
	GPRS_CCLK , // 同步时间
	GPRS_MIPCCLK , // 查询拨号状态
	GPRS_PDP , // 激活网络
	GPRS_CGPADDR , // IP地址
	GPRS_CGMR , // 模块型号
	GPRS_IMEI , // IMEI
	GPRS_CSQ , // 信号状态
	GPRS_SUCCESS, 
} GPRS_COMM_STATUS_E;


struct gprs_status_t
{
    uint8_t sim_status; // SIM卡状态
	uint8_t mount;   // 挂载状态 	1-挂载成功
    uint8_t network[GPRS_LINK_MAX]; // TCP连接状态 [0]=DATA [1]=OTA [2]=FILE 1-连接成功
    uint8_t disconn[GPRS_LINK_MAX]; // 断链标记 [id]:1-收到disconn URC待上层消费
    uint8_t cmdon; 
	uint8_t step;    // 运行步骤计数
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
    uint8_t cereg; // 网络注册状态
    uint8_t csq; // 信号强度
    uint16_t errors; // 错误码
}gprs_log_t;


struct GPRS_FEEDBACK
{
	uint8_t *feedback;
	uint16_t feedback_len;
};
////

void gprs_gpio_init_function(void); // 引脚初始化函数
void gprs_init_function(void);  // 初始化函数
void gprs_boot_up_function(void); // 模块开机函数
void gprs_shutdown_function(void); // 模块关机函数
void gprs_reset_function(void); // 重启函数
void gprs_send_cmd_over_function(void); // 退出命令发送函数
void gprs_deinit_function(void);  // 初始化-清除
int8_t gprs_status_check_function(void); // 状态监测函数
void gprs_module_restart_function(void); // 模块重启函数
uint8_t gprs_network_data_send_function(uint8_t *data, uint16_t len); // 网络数据发送函数
int8_t gprs_network_disconnect_function(GPRS_LINK_E client_id); // 连接断开函数
int8_t gprs_network_status_monitoring_function(void); // 网络状态监测函数
void gprs_sim_status_monitoring_function(void);         // SIM卡状态监测函数
void gprs_csq_status_monitoring_function(void);       // CSQ信号监测函数
uint8_t gprs_get_module_status_function(void); // 获取模块状态
uint8_t gprs_get_module_init_state(void); // 获取模块初始化状态
uint8_t gprs_get_tcp_status(GPRS_LINK_E client_id); // 获取TCP连接状态
uint8_t gprs_get_csq_function(void); // 获取模块信号强度
void *gprs_get_ip_addr_function(void); // 获取ip地址信息
void gprs_get_receive_data_function(uint8_t *buff, uint16_t len); // 获取通信数据或命令数据
void* gprs_get_infor_data_function(void); // 获取模块数据指针
void *gprs_get_log_function(void); // 获取模块日志指针
uint8_t *gprs_get_ccid_function(void); // 获取卡号
uint8_t *gprs_get_model_soft_function(void);
uint8_t *gprs_get_imei_function(void);
char* my_strstr(const char* str1, const char* str2);

void gprs_v_reset_function(void);
////

int gprs_send_data(uint8_t *data, int len, GPRS_LINK_E client_id, int waittime);
int gprs_network_connect_server(uint8_t *host, uint16_t port, GPRS_LINK_E client_id);
int gprs_send_cmd(uint8_t *AT_cmd, int AT_cmd_len, struct GPRS_FEEDBACK *feedback_array, uint8_t feedback_count, int waittime);
int gprs_recv_data(GPRS_LINK_E client_id, const unsigned char **recv_data, int *recv_data_size);
////

#endif
