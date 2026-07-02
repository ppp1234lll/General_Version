/**
 ****************************************************************************************************
 * @file        lwip_comm.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-08-01
 * @brief       lwIP配置驱动
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 实验平台:正点原子 阿波罗 F429开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 修改说明
 * V1.0 20211202
 * 第一次发布
 ****************************************************************************************************
 */
 
#ifndef _LWIP_COMM_H
#define _LWIP_COMM_H 

#include "bsp.h"
#include "netif/etharp.h"
#include "lwip/timeouts.h"
#include "lwip/snmp.h"


/* 链接状态 */
#define LWIP_LINK_OFF                   (uint8_t) 0     /* 链接关闭状态 */
#define LWIP_LINK_ON                    (uint8_t) 1     /* 链接开启状态 */
#define LWIP_LINK_AGAIN                 (uint8_t) 2     /* 重复开启 */

typedef void (*display_fn)(uint8_t index);

/* TCP客户端相关参数 */
#define LWIP_TCP_NO_CONNECT   (0)  
#define LWIP_TCP_INIT_CONNECT (1)  
#define LWIP_TCP_CONNECT          (2)  

#define LWIP_TCP_CONNECT_NUM  (5) // TCP连接尝试次数

/* UDP客户端相关参数 */
#define LWIP_UDP_NO_CONNECT    0  
#define LWIP_UDP_INIT_CONNECT  1  
#define LWIP_UDP_CONNECT           2  

/*lwip控制结构体*/
typedef struct  
{
    uint8_t init;               // 网络初始化：0：还未初始化 1：初始化过了
    uint8_t netif_state;   // 网口状态:0未创建网卡 1已经创建了网卡
    uint8_t tcp_status;       
    uint8_t udp_status;     
    uint8_t udp_multicast_status;      
    uint8_t snmp_status; 
    uint8_t iporname;        // ip或则域名 0-直接使用IP 1-需要通过域名获取ip
    uint8_t domename;        // 域名获取状态

    uint8_t mac[6];        /* MAC地址 */
    uint8_t remoteip[4];   /* 远端主机IP地址 */ 
    uint8_t ip[4];         /* 本机IP地址 */
    uint8_t netmask[4];    /* 子网掩码 */
    uint8_t gateway[4];    /* 默认网关的IP地址 */
    uint32_t remoteport;   // 远端主机端口
    uint8_t dns[4];             // dns
    
    uint8_t link_status;             /* 连接状态 */
    
    uint8_t reset;             // 网络复位：检测到1对网口进行复位
    uint8_t tcp_reset;       // tcp复位: 检测到1对tcp客户端进行复位
    uint8_t udp_reset;       // udp复位: 检测到1对udp客户端进行复位
    uint8_t udp_multicast_reset;
    uint8_t tcp_cmd;          // 网络连接命令
    uint8_t snmp_reset;
    
    uint8_t onvif_tcp_status;  
    uint8_t udp_onvif_flag;    // onvif主动搜索标志位，防止ping模式下开始onvif任务，随后被关闭
    uint8_t onvif_tcp_reset;
}__lwip_dev;

extern __lwip_dev g_lwipdev;                            /* lwip控制结构体 */
        
void    lwip_comm_default_ip_set(__lwip_dev *lwipx);    /* lwip 默认IP设置 */
uint8_t lwip_comm_init(void);                           /* LWIP初始化(LWIP启动的时候使用) */

uint8_t *lwip_get_mac_addr(void);
void lwip_stop_function(void);
void lwip_updata_remote_network_infor(__lwip_dev *lwipx);
int8_t lwip_start_function(void);
void lwip_stop_function(void);
#endif
