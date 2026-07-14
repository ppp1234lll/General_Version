#ifndef _COM_H_
#define _COM_H_

#include "./SYSTEM/sys/sys.h"

/* 配置指令 */
#define CONFIGURE_SERVER_DOMAIN_NAME        (0xF1) // 配置服务器域名与端口
#define CONFIGURE_PING_INTERVAL             (0xF2) // 配置PING的配置间隔时间
#define CONFIGURE_SERVER_IP                 (0xF3) // 配置服务器IP与端口     - 可被擦除 - 还原默认值
#define CONFIGURE_LOCAL_NETWORK             (0xF4) // 配置本地网络               - 可被擦除  
#define CONFIGURE_HEART_TIME                (0xF5) // 配置设备定时上报间隔时间 - 可被擦除 - 还原默认值
#define CONFIGURE_FAN_PARAMETER             (0xF6) // 配置风扇参数               - 可被擦除 - 还原默认值
#define CONFIGURE_CAMERA_CONFIG             (0xF7) // 配置摄像机ip                  - 可被擦除
#define CONFIGURE_MAIN_NETWORK_IP           (0xF8) // 配置主机检测IP        
#define CONFIGURE_FILL_LIGHT_TIME           (0xFB) // 配置补光灯时间段
#define CONFIGURE_FAN_HUMI                  (0xF9) // 配置风扇湿度启动参数
#define CONFIGURE_HEATING_PARAM             (0xFA) // 配置加热启动变量
#define CONFIGURE_SET_TEL                   (0xFC) // 配置电话号码
#define CONFIGURE_SERVER_MODE               (0xF3) // 设置传输模式
#define CONFIGURE_SET_MAC                   (0xFD) // 配置MAC地址
#define CONFIGURE_NETWORK_DELAY             (0xFE) // 配置网络延时时间          20220308
#define COM_HEART_UPDATA                    (0xFF) // 心跳上传

#define CONFIGURE_SNMP_OID                  (0xBE) // 配置SNMP OID
#define CONFIGURE_NETWOR_RELOAD             (0xBF) // 配置网络传输设备电源
#define CONFIGURE_DOOR_TIME                 (0xAC) // 配置箱门时间段
#define CONFIGURE_RELOAD_TIME               (0xAB) // 设备重启时间

#define CONFIGURE_ONVIF_CAREMA              (0xA9) // 搜索协议配置摄像机IP    20240201
#define CONFIGURE_DEVICE_ID                 (0xA8) // 配置设备ID    20231026
#define CONFIGURE_RS485_UOSTREAM          	(0xA7) // RS485透传
#define CONFIGURE_RS485_DATA_FORMAT         (0xA6) // RS485下游设备的数据格式
#define CONFIGURE_SEARCH_MODE               (0xA5) // 配置搜索方式        20230921
#define CONFIGURE_THRESHOLD_PARAMS          (0xA4) // 配置阈值               20230721
#define CONFIGURE_DEVICE_NAME               (0xA3) // 配置摄像机时间           20220416
#define CONFIGURE_IPC_LOGIN_INFO            (0xA2) // 配置摄像机的用户名、密码        20220329
#define CONFIGURE_IPC_TIME_SYNC             (0xA1) // 配置摄像机时间          20220329

/* 服务器查询指令 */
#define CR_QUERY_CONFIG                     (0xE1) // 查询设备当前参数设置 - 对应上传查询配置
#define CR_QUERY_INFO                       (0xE2) // 立即上报设备状态        - 正常上报
#define CR_QUERY_SOFTWARE_VERSION           (0xE3) // 查询设备软件版本号     
#define CR_QUERY_IPC_IP                     (0xE4) // 查询IPC的IP地址                20220329
#define CR_QUERY_IPC_INFO                   (0xE5) // 查询IPC的信息：设备、网络、时间、OSD  20220329
#define CR_QUERY_LBS_INFO                   (0xE6) // 查询LBS的信息：
#define CR_QUERY_SNMP_INFO                  (0xE7) // 查询SNMP参数
#define CR_QUERY_PING_INFO                  (0xE8) // 查询PING信息

/* 重启指令 */
#define CR_GPRS_NETWORK_RESET               (0xDD) // 复位GPRS网络
#define CR_LWIP_NETWORK_RESET               (0xDC) // 复位以太网卡
#define CR_IPC_REBOOT                       (0xDB) // 摄像机重启  20220329
#define CR_SINGLE_CAMERA_CONTROL            (0xDA) // 单路摄像头供电重启
#define CR_POWER_RESETART                   (0xD9) // 电源重启
#define CONFIGURE_ERASE_PARAMETER           (0xD1) // 擦除指定参数
#define CR_GPRS_NETWORK_V_RESET             (0xDE) // 断电重启4G模组


/* 控制命令 */
#define CONTROL_FAN                         (0xC1) // 风扇启停控制
#define CONTROL_FILL_LIGHT                  (0xC2) // 补光灯启停控制
#define CONTROL_HEATING                     (0xC3) // 加热器启停控制
#define CONTROL_OUT_PWR                     (0xC4) // 外设设备电源启停控制

/* 更新命令 */
#define CONFIGURE_UPDATE_SYSTEM             (0xB3) // 更新系统
#define CONFIGURE_NOW_TIME                  (0xB1) // 更新当前时间
/* 文件上传 */
#define CONFIGURE_UPLOAD_FILE               (0xB2) // 文件上传

/* 通用错误码 */
#define CR_DEVICE_NUMBER_ERROR              (0x70) // 设备编号错误
#define CR_CHECK_ERROR                      (0x71) // 校验错误
#define CR_HEAD_ERROR                       (0x72) // 数据头错误
#define CR_TAIL_ERROR                       (0x73) // 数据尾错误
#define CR_CONFIG_ERROR                     (0x74) // 配置错误

#define COM_SEND_MAX_NUM                    (3)       // 重复发生3次

#define COM_SEND_MAX_TIME                   (10*1000) // 10s超时

#define COM_MALLOC_SIZE                     (300)       // 内存数据申请

#define COM_FRAME_MAX                       (100)       // 单帧最大长度(原rec_buff长度)
#define COM_RX_Q_DEPTH                      (4)         // 帧队列深度
#define COM_FRAME_CUTDOWN_MS                (1000)      // 半截帧超时时间(ms)

/* 接收通道(支持TCP/GSM双平台) */
typedef enum
{
    COM_CH_TCP = 0,     // 以太网lwIP通道
    COM_CH_GSM,         // 4G/GSM通道
    COM_CH_MAX
} com_channel_e;

/* 组满的一帧指令, 按值投递到帧队列 */
typedef struct
{
    uint8_t  src;                 // 来源通道 com_channel_e
    uint16_t len;                 // 帧长度
    uint8_t  buf[COM_FRAME_MAX];  // 帧内容
} com_frame_t;

/* 每通道独立的组帧状态(静态分配, 一通道一份) */
typedef struct
{
    uint16_t pos;                 // 当前帧写入位置
    uint16_t head;                // 帧头滑动窗口
    uint16_t tail;                // 帧尾滑动窗口
    uint16_t body_size;           // 数据长度字段
    uint32_t last_tick;           // 上次收到字节的时刻(超时用)
    uint8_t  buf[COM_FRAME_MAX];  // 帧组装缓冲
} com_parser_t;


/*
QN
20210121233618008 我拆分成了 20210121 和 233618008
转成16进制就是  013461c9 和 0decba58
*/
struct com_qn_t {  // 请求标识码
    uint32_t qn1;  
    uint32_t qn2;  
    uint8_t  flag; // 1-需要回传标识码 
};


typedef struct
{
    uint32_t id;          // 设备ID
    uint8_t  version;     // 数据版本
    uint8_t  cmd;          // 命令
    uint8_t  size;          // 数据长度
    uint8_t  *buff;          // 数据内容
    uint8_t  src;          // 来源通道 com_channel_e(回复原路返回预留)
} com_rec_data_t;

/* 函数声明 */

int8_t com_deal_main_function(void);   // 通信接收处理函数

uint8_t com_report_get_adapter_status(uint8_t adapter);  // 获取适配器状态
uint8_t com_report_get_camera_status(uint8_t camera);    // 获取摄像机工作状态
uint8_t com_report_get_main_network_status(uint8_t main);   // 获取主网络状态
void com_report_normally_function(uint8_t *data, uint16_t *len, uint8_t cmd);  // 正常上报
void com_query_configuration_function(uint8_t *pdata, uint16_t *len);  // 查询配置
void com_heart_pack_function(uint8_t *data, uint16_t *len);  // 心跳包
void com_ack_function(uint8_t *data, uint16_t *len, uint8_t ack, uint8_t error);  // 回复数据
void com_version_information(uint8_t *pdata, uint16_t *size);  // 上传软件、硬件版本号
void com_rs485_up_param_function(uint8_t *rs485data, uint8_t *data, uint16_t *len);  // 上传参数

int8_t com_deal_configure_server_domain_name(com_rec_data_t *buff);  // 处理配置服务器域名函数
void com_deal_configure_server_mode(com_rec_data_t *buff);  // 设置服务器模式
void com_deal_update_system_function(com_rec_data_t *buff);  // 处理更新
void com_set_now_time_function(com_rec_data_t *buff);  //  设置当前时间
void com_deal_configure_server_ip_port(com_rec_data_t *buff); // 处理配置服务器IP端口
void com_deal_configure_local_network(com_rec_data_t *buff);  // 配置设备IP、子网掩码、网关
void com_deal_configure_mac(com_rec_data_t *buff);  // 配置设备mac
void com_deal_camera_config(com_rec_data_t *buff);  // 处理配置摄像头信息
void com_set_next_report_time(com_rec_data_t *buff);  // 设置上报间隔时间
void com_set_next_ping_time(com_rec_data_t *buff);   // 设置ping的时间间隔
void com_set_network_delay_time(com_rec_data_t *buff);  // 配置网络延时时间
void com_set_main_ping_ip(com_rec_data_t *buff);  // 设置主机pingip
void com_deal_ack_parameter(com_rec_data_t *buff);  // 处理回复数据
void com_query_processing_function(uint8_t cmd, uint8_t* data);

void com_set_threshold_params_function(com_rec_data_t *buff);  // 20230721
void com_set_carema_search_mode_function(com_rec_data_t *buff);
void com_set_device_password(com_rec_data_t *buff);
void com_gprs_lbs_information(uint8_t *pdata, uint16_t *size);
void com_deal_configure_snmp_oid(com_rec_data_t *buff,uint8_t len);
int com_device_snmp_information(uint8_t *pdata, uint16_t *size);
int com_device_ping_information(uint8_t *pdata, uint16_t *size);
void com_deal_upload_file_function(com_rec_data_t *buff);

// 接收(事件驱动, 双平台)
void com_recevie_function_init(void);                            // 通信接收初始化(建帧队列)
void com_rx_feed(com_channel_e ch, uint8_t *data, uint16_t len); // 生产者:喂入原始字节并组帧

void com_ipc_ip_information(uint8_t *pdata, uint16_t *size);  // IPC IP信息打包  20220329
void com_deal_camera_login(com_rec_data_t *buff);   // 配置摄像头用户名、密码  20220329
int  com_ipc_device_information(uint8_t *pdata, uint16_t *size);   // 上传摄像机设备信息
void com_set_ipc_time_function(com_rec_data_t *buff); // 设置摄像机时间  20220329
void com_set_device_name_function(com_rec_data_t *buff);  //设置设备名称  20220416

void com_deal_configure_device_id(com_rec_data_t *buff);  // 20231026
void com_deal_fan_temp_parmaeter(com_rec_data_t *buff);
void com_deal_fan_humi_param(com_rec_data_t *buff);
void com_set_carema_search_mode_function(com_rec_data_t *buff);

void com_set_work_time(com_rec_data_t *buff,uint8_t mode);  // 补光灯时间
void com_set_network_reload(com_rec_data_t *buff);

#endif
