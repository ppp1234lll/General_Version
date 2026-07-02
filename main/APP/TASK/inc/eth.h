#ifndef _ETH_H_
#define _ETH_H_

#include "./SYSTEM/sys/sys.h"

// 摄像机检测方式
typedef enum
{
    IPC_PING = 1,
    IPC_PORT_SACN,
    ETH_RTSP,
}ipc_det_type_t;

// ONVIF 状态
typedef enum
{
    ETH_ONVIF_INIT = 0,
    ETH_ONVIF_START,
    ETH_ONVIF_OSD,
    ETH_ONVIF_END,
}eth_onvif_flag_t;

#define ETH_REMOTE_PING_NUM    (4U)

typedef struct
{
    uint8_t  start;
    uint8_t  ip[4];
    uint8_t  status[ETH_REMOTE_PING_NUM];
    uint16_t times[ETH_REMOTE_PING_NUM];
}eth_ping_t;

extern eth_ping_t sg_ping_info_t;


/* 函数声明 */
void eth_task_function(void);                   /* 以太网任务主循环 */
void eth_network_reset_function(void);           /* 网络复位处理 */
void eth_ping_timer_function(void);              /* ping 定时计数处理 */
void eth_ping_detection_function(void);          /* 本地网络 ping 检测处理 */
void eth_tcp_connect_control_function(void);     /* TCP 连接控制 */
int8_t eth_carema_search_function(void);         /* 摄像机搜索、查询处理 */
int8_t eth_ping_remote_ip_function(void);        /* 远程指定 IP ping 检测 */
void eth_set_tcp_cmd(uint8_t cmd);               /* 有线网络 TCP 通信使能 */
void eth_set_tcp_connect_reset(void);            /* 重启 TCP 连接 */
void eth_set_network_reset(void);                /* 触发网络重启 */
uint8_t eth_get_network_cable_status(void);      /* 获取网线连接状态 */
uint8_t eth_get_tcp_status(void);                /* 获取 TCP 连接状态 */
uint8_t eth_get_udp_status(void);                /* 获取 UDP 组播连接状态 */
void eth_set_onvif_flag(uint8_t data);           /* 设置 ONVIF 状态 */
uint8_t eth_get_onvif_flag(void);                /* 获取 ONVIF 状态 */
void eth_save_ipc_info(char *ip,uint8_t id);     /* 保存摄像机设备信息 */
uint8_t eth_get_ping_status(void);               /* 获取远程 ping 检测状态 */
void eth_set_ping_status(uint8_t data);          /* 设置远程 ping 检测状态 */
void eth_set_ping_ip_address(uint8_t *ip);       /* 设置远程 ping 目标 IP 地址 */
void *eth_get_ping_info(void);                   /* 获取远程 ping 结果信息 */

#endif


