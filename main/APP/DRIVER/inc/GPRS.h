#ifndef _GPRS_H_
#define _GPRS_H_

#include "./SYSTEM/sys/sys.h"
#include <stdio.h>
#include <assert.h>

/* 延时函数 */
#define GPRS_DELAY_MS(ms) vTaskDelay(ms)

#define GSM_RX_BUFF_SIZE (2048)
////

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
	uint8_t mount;   // 挂载状态 	1-挂载成功
	uint8_t network; // TCP连接状态 1-连接成功
	
	uint8_t cmdon;   // 1-发送命令 0-未发送命令
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

struct GPRS_FEEDBACK
{
	const unsigned char *feedback;
	unsigned int feedback_len;
};
////

void gprs_gpio_init_function(void); // 引脚初始化函数
void gprs_init_function(void);  // 初始化函数
void gprs_boot_up_function(void); // 模块开机函数
void gprs_shutdown_function(void); // 模块关机函数
void gprs_reset_function(void); // 重启函数
uint8_t* gprs_check_cmd_function(uint8_t *str) ; // 验证响应数据
uint8_t gprs_send_cmd_function(uint8_t *cmd, uint8_t *ack, uint16_t waittime);// 数据发送函数
void gprs_send_cmd_over_function(void); // 退出命令发送函数
void gprs_deinit_function(void);  // 初始化-清除
int8_t gprs_status_check_function(void); // 状态监测函数
void gprs_network_connection_restart_function(void); // 网络连接重启函数
void gprs_module_restart_function(void); // 模块重启函数
uint8_t gprs_network_data_send_function(uint8_t *data, uint16_t len); // 网络数据发送函数
void gprs_network_disconnect_function(uint8_t data); // 连接断开函数
int8_t gprs_network_connect_function(uint8_t *ip, uint8_t *port) ; // 网络连接函数
uint8_t gprs_mult_network_connect_function(uint8_t *ip, uint8_t *port, uint8_t mult); // 多链路网络连接函数
int8_t gprs_network_status_monitoring_function(void); // 网络状态监测函数
uint8_t gprs_get_module_status_function(void); // 获取模块状态
uint8_t gprs_get_module_init_state(void); // 获取模块初始化状态
uint8_t gprs_get_tcp_status(void); // 获取TCP连接状态
uint8_t gprs_get_csq_function(void); // 获取模块信号强度
void *gprs_get_ip_addr_function(void); // 获取ip地址信息
void gprs_get_receive_data_function(uint8_t *buff, uint16_t len); // 获取通信数据或命令数据
void* gprs_get_infor_data_function(void); // 获取模块数据指针
uint8_t* gprs_get_rec_buff_function(uint16_t *len); // 获取接收数据
uint8_t *gprs_get_ccid_function(void); // 获取卡号
uint8_t *gprs_get_model_soft_function(void);
uint8_t *gprs_get_imei_function(void);
int8_t gprs_tcplink_status_function(void);
char* my_strstr(const char* str1, const char* str2);

void gprs_v_reset_function(void);
////

extern int gprs_send_data(const uint8_t *data, int len, int waittime);
extern int gprs_network_connect_server(const char *host, unsigned short port);
extern int gprs_send_cmd
(
	const uint8_t *AT_cmd,
	int AT_cmd_len,
	const struct GPRS_FEEDBACK *feedback_array,
	unsigned int feedback_count,
	int waittime
);
extern void gprs_disconnect(void);
extern int gprs_recv_data(const unsigned char **recv_data, int *recv_data_size);
extern void trace_gprs_recv_buff(const unsigned char *send_buff, int send_size);
////

#endif
