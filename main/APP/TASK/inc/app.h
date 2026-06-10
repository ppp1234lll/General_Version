#ifndef _APP_H_
#define _APP_H_

#include "./SYSTEM/sys/sys.h"

// 服务器连接模式
#define SERVER_MODE_LWIP (1) // 有线连接
#define SERVER_MODE_GPRS (2) // 无线连接
#define SERVER_MODE_ALL  (3) // 有线和无线同时连接
#define SERVER_MODE_AUTO (4) // 自动

struct local_ip_t				// 本地网络信息
{
	uint8_t ip[4];				// 本地IP
	uint8_t mac[6]; 			// MAC地址
	uint8_t netmask[4]; 		// 掩码
	uint8_t gateway[4]; 		// 网关
	uint8_t dns[4];				// 默认DNS
	
	uint8_t ping_ip[4];			// 主机需要测试的ping ip
	uint8_t ping_sub_ip[4];		// 主机需要测试的ip 2
	uint8_t server_mode;		// 服务器模式:1-有线 2-无线 4-自动
	uint8_t multicast_ip[4];	// 组播IP
	uint32_t multicast_port;	// 组播端口	
	uint8_t search_mode;			// 摄像机检测模式:1-PING 2-协议  20230810
};

struct remote_ip				
{
	uint8_t	 outside_iporname[64];	  // 外网IP-域名
	uint32_t outside_port;			  // 外网端口
	
	uint8_t  inside_iporname[64];	  // 内网IP-域名
	uint32_t inside_port;			  // 内网端口
};

struct update_addr			
{	
	uint8_t  ip[4];	  // 更新IP-域名
	uint32_t port;    // 更新端口
};

struct device_param
{
	union i_c  id;		    // id
	uint8_t  name[52];    // 设备名称
	uint8_t  password[20]; // 设备密码
	uint8_t  default_password; // 0-已修改过默认密码 1-未修改默认密码
};

typedef struct
{
	int8_t   JumpResult;
	int8_t   reset_num;    // 重启次数
	uint32_t jump_addr;    // 跳转地址
}run_result_t;


// 阈值检测 20230720
struct threshold_params {
	uint16_t volt_max;  // 高压
	uint16_t volt_min;  // 低压
	uint16_t current;
	uint8_t  angle;
	int8_t   temp_high;		// 
	int8_t   temp_low;	// 
	int8_t   humi_high;		// 
	int8_t   humi_low;  // 

	uint16_t door_open_time;  // 箱门时间
	uint16_t door_close_time;  // 箱门时间
	uint16_t light_open_time;  // 补光灯时间
	uint16_t light_close_time;  // 补光灯时间
	
	uint16_t miu;         //漏电阈值
	uint8_t net_reload; //重启次数
	uint8_t net_retime; //重启时间
};


/* 参数 */
typedef struct
{
	struct local_ip_t    local;  // 本机网络参数
	struct remote_ip     remote; // 远端网络参数
	struct device_param  device; // 设备参数
	struct threshold_params threshold; // 阈值 20230720
	struct update_addr ota;
	uint8_t mem;			   	 // 内存利用率
} sys_param_t;

/* 参数 */
typedef struct
{
	uint8_t config_flag;
	struct remote_ip     remote; // 远端网络参数
} sys_backups_t;


typedef struct
{
	uint8_t ip[6][4];		// 摄像头IP信息
	uint8_t mac[6][6];		// 摄像头mac信息
	
	char    name[6][20];	// 摄像头用户名
	char    pwd[6][20];		// 摄像头密码
	int     port[6];		// 摄像头端口
    uint8_t brand[6];       // 摄像机品牌
} carema_t;  // 摄像机


/*********************************************************/
/* SNMP命令 */
/* *********************************************************/
typedef enum
{
	IPC_BRAND = 0, // 品牌
	IPC_MODEL, // 型号
	IPC_SERIAL, // 序列号
	IPC_CPU, // CPU
	IPC_MEM, // 内存
	IPC_RATE, // 速率
	IPC_MAC, // MAC地址
	IPC_OID_MAX, // 最大值
} ipc_oid_e;

typedef enum
{
	ONV_BRAND = 0, // 品牌
	ONV_MODEL, // 型号
	ONV_POWER, // 光功率
	ONV_PORT, // 端口
	ONV_OID_MAX, // 最大值
} onv_oid_e;

typedef enum
{
	SW_BRAND = 0, // 品牌
	SW_MODEL, // 型号
	SW_PORT, // 端口
	SW_POE, // POE
	SW_POE_POWER, // POE功率
	SW_OID_MAX, // 最大值
} switch_oid_e;


typedef struct
{
	// IPC 
	char ipc_oid[3][IPC_OID_MAX][40];  // OID 字符串
	uint8_t ipc_oid_ber[3][IPC_OID_MAX][40];  // BER 编码
	uint8_t ipc_ber_len[3][IPC_OID_MAX];  // BER 编码长度
    
    // ONV 
	char onv_oid[1][ONV_OID_MAX][40];  // OID 字符串
	uint8_t onv_oid_ber[1][ONV_OID_MAX][40];  // BER 编码
	uint8_t onv_ber_len[1][ONV_OID_MAX];  // BER 编码长度
    
    // SWITCH 
	char switch_oid[1][SW_OID_MAX][40];  // OID 字符串
	uint8_t switch_oid_ber[1][SW_OID_MAX][40];  // BER 编码
	uint8_t switch_ber_len[1][SW_OID_MAX];  // BER 编码长度
    
	uint8_t onv_ip[4];
	uint8_t switch_ip[4];
} snmp_oid_t;  


typedef struct
{
	uint32_t heart;			   // 心跳包
	uint32_t report;		   // 上报时间
	uint32_t ping;			   // ping的间隔时间
	uint32_t dev_ping;		 // 设备间隔ping时间
	uint8_t  network_time; // 网络延时时间  20220308
	uint8_t  onvif_time;   // 搜索协议  20230811
} com_param_t;


/* 发送结果 */
typedef enum
{
	SR_WAIT       = 0, // 发送等待
	SR_OK         = 1, // 发送完成
	SR_TIMEOUT    = 2, // 发送超时
	SR_ERROR      = 3, // 发送结束到错误提示
	SR_SEND_ERROR = 4, // 发送错误
} send_result_e;

/* 函数声明 */

/** 执行函数 **/
void app_task_function(void);
void app_sw_control_function(void);			  // 开关控制
void app_detection_collection_param(void);	  // 检测采集数据: 市电电压、市电电流、适配器1-3 、 防雷模块 、 箱门、 箱体姿态
void app_task_save_function(void);			  // 存储任务
void app_com_send_function(void);			  // 通信发送函数
void app_com_time_function(void);  // 通信计时函数
void app_sys_operate_timer_function(void);  // 继电器处理函数


void app_set_switch_control_function(uint8_t mode, uint8_t cmd);			// 设置开关状态
void app_set_com_send_flag_function(uint8_t cmd, uint8_t data);				// 设置发送标志位
void app_set_reply_parameters_function(uint8_t cmd, uint8_t error);		// 设置回复参数
void app_set_send_result_function(send_result_e data);							// 设置发送结果函数
void app_set_peripheral_switch(uint8_t cmd, uint8_t data,uint8_t num);						// 设置外设开关状态
void app_set_sys_opeare_function(uint8_t cmd, uint8_t data);					// 设置操作任务 - 立即回发
void app_set_save_infor_function(uint8_t mode);									// 设置保存信息
void app_set_local_network_function(struct local_ip_t param);					// 设置本地网络参数
void app_set_local_network_function_two(struct local_ip_t param);				// 存储部分网络参数
void app_set_transfer_mode_function(uint8_t mode);								// 设置传输模式
void app_set_carema_search_mode_function(uint8_t mode,uint8_t config_mode) ;  // 20230810
void app_set_remote_network_function(struct remote_ip param);					// 设置远端网络参数
void app_set_reset_function(void);												// 设备重置
void app_set_mac_reset_function(void);
void app_set_camera_function(uint8_t *ip);										// 设置摄像头IP
void app_set_camera_num_function(uint8_t *ip, uint8_t num);						// 设置指定位数摄像头IP
void app_set_camera_num_brand_function(uint8_t brand, uint8_t num);					// 设置指定位数摄像头品牌

void app_set_device_param_function(struct device_param param);					// 设置设备参数
void app_set_next_report_time(uint16_t time);									  // 设置下一次上报时间
void app_set_next_report_time_other(uint16_t time,uint8_t sel);					// 设置上报间隔时间-可选择
void app_set_next_ping_time(uint16_t time, uint8_t time_dev);					// 设置下一次ping的时间
void app_set_network_delay_time(uint8_t time_dev);             // 设置网络延时时间  20220308
void app_set_current_time(int *time,uint8_t conv);								// 设置当前时间
void app_set_main_network_ping_ip(uint8_t *ip);									// 设置主PING地址
void app_set_com_interface_selection_function(uint8_t mode);					// 通信接口选择函数
void app_set_com_time_param_function(uint32_t *time,uint8_t mode) ;				// 设置通信相关时间参数:ping、上报

void app_set_code_function(struct device_param param);   // 设置密码
void app_set_onvif_reload_time(uint8_t time_dev,uint8_t mode);

void app_report_information_immediately(uint8_t if_save); // 立即上报数据
void app_deal_com_flag_function(void);   // 用来处理通信发送标志
void app_deal_com_send_wait_function(void);  // 发送等待处理任务
void app_sys_operate_timer_function(void);  // 继电器处理函数
void app_send_once_heart_infor(void);							// 立刻发送一次心跳信息
void app_send_query_configuration_infor(void);  // 立刻发送配置

uint8_t app_get_com_send_status_function(void);  // 获取当前通信状态
void app_get_storage_param_function(void);	// 用于获取存储的参数的函数
void *app_get_local_network_function(void);  					// 获取本机网络信息
void *app_get_remote_network_function(void); 					// 获取远端网络信息
void *app_get_backups_function(void);             // 20231022
int8_t app_get_camera_function(uint8_t *ip, uint8_t num);		// 获取指定摄像头ip
int8_t app_get_camera_mac_function(uint8_t *mac, uint8_t num);// 获取指定摄像头mac
void *app_get_device_param_function(void);						// 获取设备参数
uint16_t app_get_com_heart_time(void);							// 获取心跳
void app_get_current_time(char *time);							// 获取当前时间
void *app_get_current_times(void);									// 获取当前时间
uint8_t* app_get_report_current_time(uint8_t mode);
uint32_t app_get_next_ping_time(void);							// 下次ping的时间
uint32_t app_get_next_dev_ping_time(void);						// 获取下一个设备ping的时间
uint32_t app_get_report_time(void);								// 上报时间
uint8_t app_get_network_delay_time(void) ;                      // 获取网络延时时间 20220308
uint8_t app_get_onvif_time(void);                               // 获取ONVIF时间  20230811
void app_get_main_network_ping_ip_addr(uint8_t* ip);			// 获取主网ip1
void app_get_main_network_sub_ping_ip_addr(uint8_t* ip);		// 获取主网pingip地址 2
void *app_get_com_time_infor(void);								// 获取通信间隔时间
uint8_t app_get_network_mode(void);								// 获取网络模式
uint8_t app_get_carema_search_mode(void);                       // 获取搜索协议  20230810
void app_get_network_connect_status(char *buff);				// 获取网络连接状态
uint8_t *app_get_device_name(void);
uint8_t app_get_vlot_protec_status(void);
uint8_t app_get_current_status(void);							// 获取电流状态

uint8_t app_get_miu_protec_status(void);

void *app_get_carema_param_function(void);
uint8_t app_get_com_interface_selection_function(void);

void app_detect_function(void);
int8_t app_match_local_camera_ip(uint8_t *ip);					// 匹配摄像头地址
int8_t app_match_password_function(char *password);				// 密码校验函数

void app_set_lwip_reset_status(uint8_t sta); // 设置网络复位标志位
uint8_t app_get_lwip_reset_status(void);

// 20230720
void app_set_threshold_param_function(struct threshold_params param); 
void *app_get_threshold_param_function(void);        
uint16_t app_get_fault_code_function(void);

int8_t app_match_password_function(char *password); // 密码比较函数
int8_t app_match_set_code_function(void);  // 确认是否需要需要修改默认密码

void app_server_link_status_function(void); // 20231022
void app_save_backups_remote_param_function(void); // 20231024

void app_open_exec_task_function(void);
void app_power_fail_protection_function(void);
void app_power_open_protection_function(void);

// 20240903
void app_set_fan_humi_param_function(uint8_t *data);
void app_set_fan_param_function(int8_t *data);
void app_set_vol_current_param(uint16_t *data);
void app_send_data_task_function(void);
void my_app_run_param_init(void);
void app_sys_net_operate_relay(void);
uint32_t app_get_device_reload_time(void);

uint8_t app_get_update_status_function(void);
void app_set_update_status_function(uint8_t flag);

void app_set_fill_light_function(uint16_t *time);
void app_set_door_time_function(uint16_t *time);
void app_set_network_reload_param(uint16_t *data);

// 更新地址 20241101
void *app_get_http_ota_function(void);
void app_set_http_ota_function(struct update_addr param);

void app_system_softreset(void);

uint8_t app_get_mcb_status(void);
uint8_t app_get_ln_status(void);
uint8_t app_get_pe_status(void);
void app_set_net_operate_relay_id(uint8_t num);

void app_sys_net_relay_reload_num_times(void);
void app_set_net_reload_num(uint8_t num);

void app_set_camera_mac_function(uint8_t *mac, uint8_t num);

void app_set_camera_login_function(char *name_buf,char *pwd_buf,int port,uint8_t num);// 设置用户名、密码  20220329
int8_t app_get_camera_login_function(char *name_buf,char *pwd_buf,uint8_t num); //获取用户名、密码 20220329
int app_get_camera_port_function(uint8_t num);					//获取摄像机端口 20220329
int8_t app_get_camera_num_function(void); 					    // 获取摄像机ID 20220329

void *app_get_snmp_oid_function(void);
int8_t app_get_snmp_dev_type_function(void);
int8_t app_get_camera_param_function(char *ip, uint8_t *brand, uint8_t num);		// 获取指定摄像头参数
void app_set_camera_brand_function(uint8_t *brand);

void app_set_snmp_ip_function(uint8_t ip[4]);
int app_opeare_relay_function(uint8_t num);
void app_sys_operate_relay(void);

#endif
