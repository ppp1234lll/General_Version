#ifndef _APP_H_
#define _APP_H_

#include "./SYSTEM/sys/sys.h"

// 服务器连接模式
typedef enum
{
    SERVER_MODE_LWIP = 1, // 有线连接
    SERVER_MODE_GPRS = 2, // 无线连接
    SERVER_MODE_ALL  = 3, // 有线和无线同时连接
    SERVER_MODE_AUTO = 4, // 自动
}server_mode_t;

// 本地网络信息
struct local_ip_t                   
{
    uint8_t ip[4];                  // 本地IP
    uint8_t mac[6];                 // MAC地址
    uint8_t netmask[4];             // 掩码
    uint8_t gateway[4];             // 网关
    uint8_t dns[4];                 // 默认DNS
    
    uint8_t ping_ip[4];             // 主机需要测试的ping ip
    uint8_t ping_sub_ip[4];         // 主机需要测试的ip 2
    uint8_t server_mode;            // 服务器模式: SERVER_MODE_LWIP/SERVER_MODE_GPRS/SERVER_MODE_ALL/SERVER_MODE_AUTO
    uint8_t multicast_ip[4];        // 组播IP
    uint32_t multicast_port;        // 组播端口    
    uint8_t search_mode;            // 摄像机检测模式:1-PING 2-协议  20230810
};

struct remote_ip                
{
    uint8_t     outside_iporname[64];      // 外网IP-域名
    uint32_t    outside_port;              // 外网端口
    
    uint8_t     inside_iporname[64];      // 内网IP-域名
    uint32_t    inside_port;              // 内网端口
};

// OTA 更新地址
struct update_addr            
{    
    uint8_t     ip[4];     
    uint32_t    port;    
};

// 上传地址
struct upload_addr            
{    
    uint8_t     ip[4];  
    uint32_t    port;   
};

// 设备参数
struct device_param
{
    union i_c  id;              // ID
    uint8_t  name[52];          // 设备名称
    uint8_t  password[20];      // 设备密码
    uint8_t  default_password;  // 0-已修改过默认密码 1-未修改默认密码
};

// 阈值检测 20230720
struct threshold_params {
    uint16_t volt_max;  // 高压
    uint16_t volt_min;  // 低压
    uint16_t current;
    uint8_t  angle;
    int8_t   temp_high;        // 
    int8_t   temp_low;    // 
    int8_t   humi_high;        // 
    int8_t   humi_low;  // 

    uint16_t door_open_time;  // 箱门时间
    uint16_t door_close_time;  // 箱门时间
    uint16_t light_open_time;  // 补光灯时间
    uint16_t light_close_time;  // 补光灯时间
    
    uint16_t miu;         //漏电阈值
    uint8_t  net_reload; //重启次数
    uint8_t  net_retime; //重启时间
    uint8_t  net_delay_time; // 网络延时时间
};

/* 参数 */
typedef struct
{
    struct local_ip_t       local;  // 本机网络参数
    struct remote_ip        remote; // 远端网络参数
    struct device_param     device; // 设备参数
    struct threshold_params threshold; // 阈值 20230720
    struct update_addr      ota;
    struct upload_addr      upload;
} sys_param_t;

/* 备份 */
typedef struct
{
    uint8_t config_flag;
    struct  remote_ip   remote; // 远端网络参数 
} sys_backups_t;

// 摄像机参数
typedef struct
{
    uint8_t ip[6][4];       // 摄像头IP信息
    uint8_t mac[6][6];      // 摄像头mac信息
    
    char    name[6][20];    // 摄像头用户名
    char    pwd[6][20];     // 摄像头密码
    int     port[6];        // 摄像头端口
    uint8_t brand[6];       // 摄像机品牌
} carema_t;  // 摄像机


/*********************************************************/
/* SNMP命令 */
/* *********************************************************/
typedef enum
{
    IPC_BRAND = 0,      // 品牌
    IPC_MODEL,          // 型号
    IPC_SERIAL,         // 序列号
    IPC_CPU,            // CPU
    IPC_MEM,            // 内存
    IPC_RATE,           // 速率
    IPC_MAC,            // MAC地址
    IPC_OID_MAX,        // 最大值
} ipc_oid_e;

typedef enum
{
    ONV_BRAND = 0,      // 品牌
    ONV_MODEL,          // 型号
    ONV_POWER,          // 光功率
    ONV_PORT,           // 端口
    ONV_OID_MAX,        // 最大值
} onv_oid_e;

typedef enum
{
    SW_BRAND = 0,       // 品牌
    SW_MODEL,           // 型号
    SW_PORT,            // 端口
    SW_POE,             // POE
    SW_POE_POWER,       // POE功率
    SW_OID_MAX,         // 最大值
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

// 通信参数
typedef struct
{
    uint32_t heart;            // 心跳包
    uint32_t report;           // 上报时间
    uint32_t ping;             // ping的间隔时间
    uint32_t dev_ping;         // 设备间隔ping时间
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

/* === 任务函数 === */
void app_task_function(void);                                    /* 应用程序主任务循环 */
void app_com_send_function(void);                                /* 通信发送函数 */
void app_com_time_function(void);                                /* 通信计时函数 */
void app_task_save_function(void);                               /* 存储任务函数 */
void app_sw_control_function(void);                              /* 开关控制 */
void app_detection_collection_param(void);                       /* 检测采集数据: 市电电压/电流/适配器/防雷/箱门/姿态 */
void app_open_exec_task_function(void);                          /* 开关执行任务 */
void app_send_data_task_function(void);                          /* 发送数据任务 */
void app_sys_net_operate_relay(void);                            /* 系统网络控制继电器 */
void app_sys_net_relay_reload_num_times(void);                   /* 系统网络继电器重连次数处理 */
void app_sys_operate_relay(void);                                /* 系统操作继电器 */
void app_server_link_status_function(void);                      /* 服务器连接状态检测 */
void app_power_fail_protection_function(void);                   /* 掉电保护 */
void app_power_open_protection_function(void);                   /* 上电保护 */
void app_detect_function(void);                                  /* 检测函数 */

/* === 设置函数 === */
void app_set_switch_control_function(uint8_t mode, uint8_t cmd); /* 设置开关状态 */
void app_set_com_send_flag_function(uint8_t cmd, uint8_t data);  /* 设置通信发送标志位 */
void app_set_reply_parameters_function(uint8_t cmd, uint8_t error); /* 设置回复参数 */
void app_set_send_result_function(send_result_e data);           /* 设置发送结果 */
void app_set_peripheral_switch(uint8_t cmd, uint8_t data, uint8_t num); /* 设置外设开关状态 */
int  app_opeare_relay_function(uint8_t num);                     /* 继电器操作 */
void app_set_sys_opeare_function(uint8_t cmd, uint8_t data);     /* 设置系统操作任务 - 立即回发 */
void app_set_net_operate_relay_id(uint8_t num);                  /* 设置网络操作继电器ID */
void app_set_net_reload_num(uint8_t num);                        /* 设置网络重连次数 */
void app_set_save_infor_function(uint8_t mode);                  /* 设置保存信息标志位 */
void app_set_local_network_function(struct local_ip_t param);    /* 设置本机网络参数 */
int8_t app_set_transfer_mode_function(uint8_t mode);             /* 设置传输模式, 返回1-改变 0-未变 */
void app_set_carema_search_mode_function(uint8_t mode, uint8_t config_mode); /* 设置摄像机搜索模式 */
void app_set_remote_network_function(struct remote_ip param);    /* 设置远端网络参数 */
void app_set_reset_function(void);                               /* 恢复出厂化 */
void app_set_mac_reset_function(void);                           /* 重置MAC地址 */
void app_set_camera_function(uint8_t *ip);                       /* 设置摄像头IP */
void app_set_camera_brand_function(uint8_t *brand);              /* 设置摄像头品牌 */
void app_set_camera_login_function(char *name_buf, char *pwd_buf, int port, uint8_t num); /* 设置摄像机用户名/密码/端口 */
void app_set_camera_num_function(uint8_t *ip, uint8_t num);      /* 设置指定位数摄像头IP */
void app_set_camera_num_brand_function(uint8_t brand, uint8_t num); /* 设置指定位数摄像头品牌 */
void app_set_camera_mac_function(uint8_t *mac, uint8_t num);     /* 设置摄像机MAC地址 */
void app_set_camera_id_num_function(uint8_t data);               /* 设置摄像机编号 */
void app_set_fan_humi_param_function(uint8_t *data);             /* 设置风扇湿度参数 */
void app_set_fan_param_function(int8_t *data);                   /* 设置风扇参数 */
void app_set_fill_light_function(uint16_t *time);                /* 设置补光灯时间 */
void app_set_door_time_function(uint16_t *time);                 /* 设置箱门时间 */
void app_set_vol_current_param(uint16_t *data);                  /* 设置电压电流参数 */
void app_set_network_reload_param(uint16_t *data);               /* 设置网络重连参数 */
void app_set_device_param_function(struct device_param param);   /* 设置设备参数 */
void app_set_code_function(struct device_param param);           /* 设置密码 */
void app_set_next_report_time(uint16_t time);                    /* 设置下次上报时间 */
void app_set_next_report_time_other(uint16_t time, uint8_t sel); /* 设置上报间隔时间(可选) */
void app_set_next_ping_time(uint16_t time, uint8_t time_dev);    /* 设置下次PING时间 */
void app_set_network_delay_time(uint8_t time_dev);               /* 设置网络延时时间 */
void app_set_current_time(int *time, uint8_t conv);              /* 设置当前时间 */
void app_set_main_network_ping_ip(uint8_t *ip);                  /* 设置主PING地址 */
void app_set_com_interface_selection_function(uint8_t mode);     /* 通信接口选择函数 */
void app_set_com_time_param_function(uint32_t *time, uint8_t mode); /* 设置通信相关时间参数 */
void app_set_report_time_function(uint32_t time);                /* 设置上报时间 */
void app_set_threshold_param_function(struct threshold_params param); /* 设置阈值参数 */
void app_set_http_ota_function(struct update_addr param);        /* 设置OTA升级地址 */
void app_set_http_upload_function(struct upload_addr param);     /* 设置文件上传地址 */
void app_set_snmp_ip_function(uint8_t ip[4]);                    /* 设置SNMP IP地址 */

/* === 获取函数 === */
void app_get_storage_param_function(void);                       /* 获取并恢复存储参数 */
void *app_get_local_network_function(void);                      /* 获取本机网络信息 */
void *app_get_remote_network_function(void);                     /* 获取远端网络信息 */
void *app_get_backups_function(void);                            /* 获取备份信息 */
int8_t app_get_camera_param_function(char *ip, uint8_t *brand, uint8_t num); /* 获取指定摄像头参数 */
int8_t app_get_camera_function(uint8_t *ip, uint8_t num);        /* 获取指定摄像头IP */
int8_t app_get_camera_mac_function(uint8_t *mac, uint8_t num);   /* 获取指定摄像头MAC */
int8_t app_get_camera_login_function(char *name_buf, char *pwd_buf, uint8_t num); /* 获取摄像机用户名密码 */
int  app_get_camera_port_function(uint8_t num);                  /* 获取摄像机端口 */
int8_t app_get_camera_num_function(void);                        /* 获取摄像机数量 */
void *app_get_carema_param_function(void);                       /* 获取摄像头参数结构体 */
uint16_t app_get_com_heart_time(void);                           /* 获取心跳时间 */
void  app_get_current_time(char *time);                          /* 获取当前时间字符串 */
void *app_get_current_times(void);                               /* 获取当前时间结构体指针 */
uint8_t *app_get_report_current_time(uint8_t mode);              /* 获取上报用当前时间 */
uint32_t app_get_next_ping_time(void);                           /* 获取下次PING间隔时间 */
uint32_t app_get_next_dev_ping_time(void);                       /* 获取下次设备PING间隔时间 */
uint32_t app_get_report_time(void);                              /* 获取上报间隔时间 */
uint8_t app_get_network_delay_time(void);                        /* 获取网络延时时间 */
uint8_t app_get_onvif_time(void);                                /* 获取ONVIF每轮搜索时间 */
void  app_get_main_network_ping_ip_addr(uint8_t *ip);            /* 获取主网PING IP */
void  app_get_main_network_sub_ping_ip_addr(uint8_t *ip);        /* 获取主网PING IP(备份) */
uint8_t app_get_com_interface_selection_function(void);          /* 获取通信接口选择 */
void *app_get_com_time_infor(void);                              /* 获取通信间隔时间 */
uint8_t app_get_network_mode(void);                              /* 获取网络模式 */
uint8_t app_get_carema_search_mode(void);                        /* 获取摄像机搜索模式 */
uint8_t *app_get_device_name(void);                              /* 获取设备名称 */
void *app_get_device_param_function(void);                      /* 获取设备参数 */
void *app_get_backups_param_function(void);                     /* 获取备份参数 */
void *app_get_threshold_param_function(void);                    /* 获取阈值参数 */
void *app_get_http_ota_function(void);                           /* 获取OTA升级地址 */
void *app_get_http_upload_function(void);                        /* 获取文件上传地址 */
void *app_get_snmp_oid_function(void);                           /* 获取SNMP OID参数 */
int8_t app_get_snmp_dev_type_function(void);                     /* 获取SNMP设备类型 */
uint8_t app_get_vlot_protec_status(void);                        /* 获取电压保护状态 */
uint8_t app_get_current_status(void);                            /* 获取电流状态 */
uint8_t app_get_miu_protec_status(void);                         /* 获取漏电保护状态 */

/* === 通信/上报 === */
void app_report_information_immediately(uint8_t if_save);        /* 立即上报数据 */
void app_deal_com_flag_function(void);                           /* 通信标志处理 */
uint8_t app_get_com_send_status_function(void);                  /* 获取通信发送状态 */
void app_deal_com_send_wait_function(void);                      /* 发送等待处理 */
void app_send_once_heart_infor(void);                            /* 立即发送一次心跳 */
void app_send_query_configuration_infor(void);                   /* 立即发送配置查询 */
void app_get_network_connect_status(char *buff);                 /* 获取网络连接状态 */
void app_save_backups_remote_param_function(void);               /* 保存备份远端参数 */

/* === 校验/匹配 === */
int8_t app_match_local_camera_ip(uint8_t *ip);                   /* 匹配本地摄像头IP */
int8_t app_match_password_function(char *password);              /* 密码校验 */
int8_t app_match_set_code_function(void);                        /* 确认是否需要修改默认密码 */

/* === 系统/SW === */
void app_set_lwip_reset_status(uint8_t sta);                     /* 设置网络复位标志位 */
uint8_t app_get_lwip_reset_status(void);                         /* 获取网络复位标志位 */
uint16_t app_get_fault_code_function(void);                      /* 获取故障码 */
void app_system_softreset(void);                                 /* 系统软复位 */
uint32_t app_get_device_reload_time(void);                       /* 获取设备重连时间 */
void my_app_run_param_init(void);                                /* 运行参数初始化 */

/* === 状态获取 === */
uint8_t app_get_update_status_function(void);                    /* 获取更新状态 */
void    app_set_update_status_function(uint8_t flag);             /* 设置更新状态 */
uint8_t app_get_mcb_status(void);                                /* 获取MCB状态 */
uint8_t app_get_ln_status(void);                                 /* 获取LN状态 */
uint8_t app_get_pe_status(void);                                 /* 获取PE状态 */

#endif

