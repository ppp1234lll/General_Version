#ifndef _SAVE_H_
#define _SAVE_H_

#include "./SYSTEM/sys/sys.h"
#include "./Task/inc/app.h"

/* 登录密码 */
#define DEFALUT_DEVICE_ID           (3)
#define DEFALUT_PASSWORD            ("88888888")

/* 有线网络 */

#define DEFALUT_LOCAL_IP0           (192)   // IP地址
#define DEFALUT_LOCAL_IP1           (168)
#define DEFALUT_LOCAL_IP2           (1)
#define DEFALUT_LOCAL_IP3           (30)

#define DEFALUT_NETMASK0            (255)   // 子网掩码
#define DEFALUT_NETMASK1            (255)
#define DEFALUT_NETMASK2            (255)
#define DEFALUT_NETMASK3            (0)

#define DEFALUT_GATEWAY0            (192)   // 网关
#define DEFALUT_GATEWAY1            (168)
#define DEFALUT_GATEWAY2            (1)
#define DEFALUT_GATEWAY3            (1)

#define DEFALUT_MULTICAST_IP0       (239)   // 多播地址
#define DEFALUT_MULTICAST_IP1       (255)
#define DEFALUT_MULTICAST_IP2       (255)
#define DEFALUT_MULTICAST_IP3       (249)
#define DEFALUT_MULTICAST_PORT      (65000)

#define DEFALUT_DNS0                (114)   // DNS服务器
#define DEFALUT_DNS1                (114)
#define DEFALUT_DNS2                (114)
#define DEFALUT_DNS3                (114)

#define DEFALUT_SERVERMODE          (4)       // 服务器模式

/* 阈值参数 */
#define DEFALUT_VOLT_MAX            (0)       // 过压
#define DEFALUT_VOLT_MIN            (0)       // 欠压
#define DEFALUT_CURRENT_MAX         (0)       // 过流
#define DEFAULT_ANGLE               (20)      // 角度
#define DEFAULT_MIU                 (25)      // 漏电阈值
#define DEFALUT_TEMP_HIGH           (60)      // 过温
#define DEFALUT_TEMP_LOW            (1)       // 欠温
#define DEFALUT_HUMI_HIGH           (80)      // 过湿
#define DEFAULT_HUMI_LOW            (10)
// 时间
#define DEFALUT_LIGHT_OPEN_TIME     (720) // 12:00 = 12*60
#define DEFALUT_LIGHT_CLOSE_TIME    (720) // 12:00 = 12*60

#define DEFALUT_DOOR_OPEN_TIME      (720) // 12:00 = 12*60
#define DEFALUT_DOOR_CLOSE_TIME     (720) // 12:00 = 12*60

#define DEFALUT_NETWORK_DELAY       (200)      // 网络延时时间

// 通信时间
#define DEFALUT_HEART               (90*1000)   // 心跳时间
#define DEFALUT_REPORT              (180*1000)  // 上报时间
#define DEFALUT_PING                (20*1000)   // 每轮ping的间隔时间
#define DEFALUT_DEV_PING            (10*1000)   // 下一次ping的时间
#define DEFALUT_ONVIF_TIME          (120)       // ONVIF每轮搜索时间

// 摄像机检测方式
#define DEFALUT_IPC_DET_TYPE        (1)       

// 平台地址
#define DEFALUT_LWIP_PLATFORM_URL   ("47.104.250.225")   // 内网地址
#define DEFALUT_LWIP_PLATFORM_PORT  (6012)  
#define DEFALUT_GSM_PLATFORM_URL    ("47.104.250.225")   // 外网地址
#define DEFALUT_GSM_PLATFORM_PORT   (6012)

// OTA地址
#define DEFALUT_OTA_IP0             (47)  
#define DEFALUT_OTA_IP1             (104)  
#define DEFALUT_OTA_IP2             (98)  
#define DEFALUT_OTA_IP3             (214)  
#define DEFALUT_OTA_PORT            (8989)

// 文件上传地址
#define DEFALUT_UPLOAD_IP0          (47)  
#define DEFALUT_UPLOAD_IP1          (104)  
#define DEFALUT_UPLOAD_IP2          (250)  
#define DEFALUT_UPLOAD_IP3          (225)  
#define DEFALUT_UPLOAD_PORT         (8080)


/* 存储相关 */
#define SAVE_OTHER_PARAM            (0) // 其余数据
#define SAVE_LOCAL_NETWORK          (1) // 本地网络信息
#define SAVE_REMOTE_IP              (2) // 远端网络信息
#define SAVE_COMPARISION            (3) // 外设相关数据
#define SAVE_DEVICE_PARAM           (4) // 系统数据
#define SAVE_COM_PARAMETER          (5) // 通信数据
#define SAVE_UPDATE                 (6) // 更新参数
#define SAVE_REPORT_SW              (7) // 上报开关参数
#define SAVE_ONLY_SEND_IP           (8) // 只发送服务器
#define SAVE_CAREMA                 (9) // 摄像机参数
#define SAVE_THRESHOLD              (10) // 阈值
#define SAVE_HTTP_OTA               (11) // 更新地址
#define SAVE_SNMP_OID               (12) // SNMP OID参数
#define SAVE_UPLOAD                 (13) // 文件上传地址


/* 函数声明 */
void save_init_function(void);
void save_clear_file_function(uint8_t mode);

// 存储
int8_t save_stroage_local_network(struct local_ip_t *local);
int8_t save_stroage_remote_ip_function(struct remote_ip *remote);
int8_t save_storage_device_parameter_function(struct device_param *param);
int8_t save_stroage_com_param_function(com_param_t *param);
int8_t save_stroage_update_addr(uint8_t *ip,uint32_t port) ;

// 读取
int8_t save_read_local_network(struct local_ip_t *local);
int8_t save_read_remote_ip_function(struct remote_ip *remote);
int8_t save_read_device_paramter_function(struct device_param *param);
int8_t save_read_com_param_function(com_param_t *param);
int8_t save_read_update_addr(uint8_t *ip,uint32_t *port);
// 默认参数
void save_read_default_local_network(struct local_ip_t *local);
void save_read_default_device_paramter_function(struct device_param *param);
void save_read_default_remote_ip(struct remote_ip *remote);
void save_read_default_com_param_function(com_param_t *param);

// 20230712 摄像机相关参数
int8_t save_read_carema_parameter(carema_t *param);
void save_read_default_carema_parameter(carema_t *param);
int8_t save_stroage_carema_parameter(carema_t *param);

// 20230723 阈值
int8_t save_stroage_threshold_parameter(struct threshold_params *param);
void save_read_default_threshold_parameter(struct threshold_params *param);
int8_t save_read_threshold_parameter(struct threshold_params *param);    

// 20231022 备份信息
int8_t save_stroage_backups_function(sys_backups_t *param);
int8_t save_read_backups_function(sys_backups_t *param);
void save_read_default_backups(sys_backups_t *param);

// 20241101 HTTP升级
int8_t save_stroage_http_ota_function(struct update_addr *param);
int8_t save_read_http_ota_function(struct update_addr *param);
void save_read_default_http_ota(struct update_addr *param);

// 文件上传地址
int8_t save_stroage_http_upload_function(struct upload_addr *param);
int8_t save_read_http_upload_function(struct upload_addr *param);
void save_read_default_http_upload(struct upload_addr *param);

// 用电量
int8_t save_stroage_electricity_function(electricity_t *param);
int8_t save_read_electricity_function(electricity_t *param);
void save_read_default_electricity(electricity_t *param);

// snmp_oid_t 相关存储函数
int8_t save_stroage_snmp_oid_parameter(snmp_oid_t *param);
void save_read_default_snmp_oid_parameter(snmp_oid_t *param);
int8_t save_read_snmp_oid_parameter(snmp_oid_t *param);


#endif
