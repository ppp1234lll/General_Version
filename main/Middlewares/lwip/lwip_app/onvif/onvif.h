#ifndef __ONVIF_H
#define __ONVIF_H
//#include "sys.h"
#include "./SYSTEM/sys/sys.h"
//#include "includes.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/netif.h"
#include "lwip/ip.h"
#include "lwip/tcp.h"
#include "lwip/init.h"
#include "netif/etharp.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/igmp.h"
#include "lwip/sockets.h"

#define LOCAL_MULTICAST_PROT    56000       // 本机组播端口号

#define MULTICAST_ADDR    "239.255.255.250"  // IPC组播IP地址
#define MULTICAST_PROT     3702                 // IPC组播端口号
#define HIKVISION_MULTICAST_PROT    37020       // IPC组播端口号

#define DAHUA_MULTICAST_ADDR1    "239.255.255.251"  // IPC组播IP地址
#define DAHUA_MULTICAST_ADDR2    "255.255.255.255"  // IPC组播IP地址
#define DAHUA_MULTICAST_PROT1     5050 
#define DAHUA_MULTICAST_PROT2     37810       // IPC组播端口号
#define DAHUA_MULTICAST_PROT3     37820
#define DAHUA_MULTICAST_PROT4     8087  

// 宇视
#define UNW_MULTICAST_ADDR    "255.255.255.255"  // IPC组播IP地址
#define UNW_MULTICAST_PROT     3702                 // IPC组播端口号


#define TEST_LOCAL_IP      "192.168.2.32"   // PC端测试IP
#define TEST_LOCAL_PROT     65000                // PC端测试端口号

#define ONVIF_TX_BUFSIZE    1500    //定义最大发送数据长度
#define ONVIF_RX_BUFSIZE    4000

#define IPC_NUM_MAX         10      // IPC搜索个数 

typedef enum 
{
    HIKVISION_P = 0,
    DAHUA_P,
    UNW_P,
    ONVIF_P ,
    IPC_PROTOCOL_END
}ipc_protocol_t;   // 通信协议


typedef enum 
{
    HIKVISION = 0,
    DAHUA,
    UNW,
    KEDACOM ,  // 科达
    JOVISION,     //中维世纪
}ipc_brand_t;   // 品牌


//typedef enum 
//{
//    ONVIF_INIT = 0,
//    ONVIF_START,
//    ONVIF_END,
//}onvif_ipc_e;   // 步骤

#define ONVIF_START     ((uint8_t)0x01)  // 开始搜索
#define ONVIF_INIT      ((uint8_t)0x02)  // 初始化

#define ONVIF_END          ((uint8_t)0x80)  // 搜索结束

typedef struct
{
    uint8_t brand;
    uint8_t ip[4];
    uint8_t mac[6];
    char     model[32];
    char    osd[128];
}ipc_t;

typedef struct
{
    uint8_t search_flag;// 查询标志位(第0位搜索：0禁止，1允许)
    uint8_t ipc_num;    // 数量
    ipc_t   ipc_param[IPC_NUM_MAX];
    uint16_t onvif_times;
    ipc_protocol_t ipc_protocol_status;
}IPC_Info_t;      // IPC摄像头参数


unsigned char onvif_init(void);
void onvif_udp_start(void);
int ONVIF_IPC_Send_API(int sockfd,char* data,int len,char* ip,int port);

int ONVIF_IPC_Search_API(int sockfd,char* ip,int port);
int HIKVISION_IPC_Search_API(int sockfd,char* ip,int port);
int DAHUA_IPC_Search_API(int sockfd,char* ip,int port);
int ONVIF_IPC_Recv_API(int sockfd,uint8_t cmd);

int ONVIF_IPC_Bind_API(int sockfd,int port);
int ONVIF_Search_API(int sockfd);
void ONVIF_IPC_NET_Detection_API(void);
void onvif_search_timer_function(void);
void onvif_udp_stop(void);
void onvif_save_ipc_info(char *ip,uint8_t id);  // 保存摄像机信息

void onvif_set_search_flag(uint8_t flag);
uint8_t onvif_get_search_flag(void);
uint8_t onvif_get_ipc_num(void);
int8_t onvif_match_camera_ip(uint8_t *ip);
int8_t onvif_match_ip(uint8_t *ip1,uint8_t *ip2);
void onvif_get_ipc_appoint_ip(char *buf,uint8_t id);    // 指定id的摄像头IP
void onvif_get_ipc_mac_info(char *buff,uint8_t *mac,uint8_t cmd)  ;

void onvif_get_ipc_model_info(char *buff,char *string,uint8_t cmd)  ;
void onvif_get_ipc_brand_info(char *buff,uint8_t *brand,uint8_t cmd) ;
void onvif_get_ipc_param_info(char *buff,uint8_t *mac,char *model,uint8_t *brand,uint8_t cmd)  ;
void onvif_search_start_function(void);

void *onvif_get_ipc_param(void);
#endif

