#ifndef __PRINT_H
#define __PRINT_H

#include "./SYSTEM/sys/sys.h"

//#define  KEY_ENABLE   // 使用秘钥加密

typedef struct
{
    uint8_t  size;        // 数据长度
    uint8_t  cmd;          // 命令
    union i_c id;          // 设备ID
    uint8_t  *buff;        // 数据内容
} print_data_t;

#define CMD_SEARCH                  "search"
#define CMD_INQUIRY                  "inquiry"
#define CMD_UPDATE_MAC          "update_mac"
#define CMD_UPDATE_RELAY      "update_relay"
#define CMD_UPDATE_IP              "update_ip"
#define CMD_UPDATE_ADDR          "update_runaddr"
#define CMD_UPDATE_SERVER      "update_server"
#define CMD_UPDATE_NP         "update_np"



#define DEVICE_ID              "tid"
#define DEVICE_TYPES        "types"
#define DEVICE_PWD          "password"
#define DEVICE_MAC          "mac"
#define DEVICE_IP              "\"ip\""
#define DEVICE_NM              "nm"
#define DEVICE_GW              "gw"
#define DEVICE_MAINIP1  "mip1"
#define DEVICE_MAINIP2  "mip2"
#define DEVICE_CAM1          "c1"
#define DEVICE_CAM2          "c2"
#define DEVICE_CAM3          "c3"
#define DEVICE_CAM4          "c4"
#define DEVICE_CAM5          "c5"
#define DEVICE_CAM6          "c6"
#define DEVICE_RELAY1      "pa1"
#define DEVICE_RELAY2      "pa2"
#define DEVICE_RELAY3      "pa3"
#define DEVICE_RELAY4      "pa4"
#define DEVICE_RELAY5      "pa5"
#define DEVICE_RELAY6      "pa6"

#define DEVICE_INIP        "inip"
#define DEVICE_OUTIP      "outip"
#define DEVICE_PORT1      "po1"
#define DEVICE_PORT2      "po2"
#define DEVICE_NAME          "username"

typedef enum
{
    IP = 0,
    NM = 1,
    GW = 2,
    MIP1 = 3,
    MIP2 = 4,
    CAM1 = 5,
    CAM2 = 6,    
    CAM3 = 7,
    CAM4 = 8,
    CAM5 = 9,
    CAM6 = 10,
    IP_ALL,
}net_list_t;

typedef enum
{
    RELAY1 = 0,
    RELAY2 = 1,
    RELAY3 = 2,
    RELAY4 = 3,
    RELAY5 = 4,
    RELAY6 = 5,
    RELAY_ALL,
}relay_list_t;

typedef enum
{
    NO_ACK         =    0,
    MAC_ACK     = 1,
    RELAY_ACK = 2,
    IP_ACK         = 3,
    SERVER_ACK = 4,
    NAMEPWD_ACK = 5,
    ACK_ALL,
}config_ack_t; // 回复信息



void print_task_function(void);
void print_stroage_queue_data(uint8_t *buff,uint16_t len); // 将数据存储到缓存区
int print_getparam_fun(char *data,char *sStr); // 根据str 取出xml中相应的参数
int print_queue_find_msg(void); // 获取缓存区中的数据
int8_t print_buff_deal_function(void); // 缓存接收处理函数
void print_reply_parameters_function(uint8_t cmd, uint8_t error); // 设置回复参数
void print_udp_send_function(void);  // 通信发送函数
void print_deal_com_flag_function(void); // 用来处理通信回复信息

void print_search_ack_function(uint8_t *pdata, uint16_t *len,uint8_t ack,uint8_t result); // 生成查询回传包
void print_relay_ack_function(uint8_t *pdata, uint16_t *len, uint8_t result); // 继电器回复数据
void print_inquiry_ack_function(uint8_t *pdata, uint16_t *len); // 查询配置 
 
void print_tcp_configure_mac(void); // 配置设备mac
void print_tcp_configure_rlelay(void); // 配置继电器
void print_configure_local_network(void); // 配置设备IP、子网掩码、网关
int8_t print_configure_camera_ip(uint8_t id); // 处理配置摄像头信息

void print_report_time_function(void);// 上报计时
void print_configure_run_addr(void);

void printf_tcp_configure_server_network(void);
void printf_tcp_configure_pwd_name(void);


#endif
