#ifndef __ONVIF_TCP_H
#define __ONVIF_TCP_H

#include "./SYSTEM/sys/sys.h"
//#include "includes.h" 
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "lwip/memp.h"
#include "lwip/mem.h"
#include "lwip_comm.h"
#include "onvif.h"
#include <stdint.h>

#define ONVIF_SUCCESS         0x00
#define DEVICE_ERROR         0x01
#define NETWORK_ERROR         0x02
#define GATEWAY_ERROR          0x04
#define OSD_ERROR                0x08
#define GET_TIME_ERROR      0x10
#define REBOOT_ERROR          0x20
#define SET_TIME_ERROR      0x40
#define TOKEN_ERROR          0x80

// 命令
#define READ_DEVICE_INFO  1
#define IPC_REBOOT        2
#define IPC_TIME_SYNC     3

#define ERROR_MAX_NUM         3

#define TCP_TEST_HOST    "192.168.2.245"  /* 输入你的 TCP server 域名或者 ip 地址 */
#define TCP_TEST_PORT    80                 /* 输入你的 TCP Server 断口号 */

typedef enum
{
    ONVIF_DEVICE  = 0,
    ONVIF_NETWORK = 1,
    ONVIF_GATEWAY = 2,
    ONVIF_OSD     = 3,
    ONVIF_TIMEGET = 4,
    ONVIF_REBOOT  = 5,
    ONVIF_TIMESET = 6,
    ONVIF_TOKEN   = 7,
}ONVIF_ID_T;


typedef struct
{
    uint8_t error_flag;
    uint8_t error_count[8];
    int8_t status;
}ipc_flag_t;

typedef struct
{
    uint8_t cmd;     // 命令
    uint8_t tcp_status;
    int8_t status;
}onvif_tcp_t;

extern onvif_tcp_t sg_onvif_tcp;

unsigned char onvif_tcp_client_init(void);  //tcp客户端初始化(创建tcp客户端线程)
void onvif_tcp_client_stop(void);

int onvif_tcp_link_carema(int sockt,char *ip,int port);

int tcp_http_test(int sockfd);

void onvif_get_ipc_ip_str(char *buff);

void set_ipc_device_cmd(uint8_t cmd);
void set_ipc_read_status(uint8_t dat);

int8_t get_ipc_read_status(void);
int8_t onvif_tcp_get_ipc_read_status(void);
int onvif_get_ipc_port_str(void);
uint8_t onvif_get_ipc_net_status(void);

unsigned char Ascii2Hex( unsigned char bAscii )    ;

void onvif_get_ipc_info_end(void);
void onvif_set_ipc_info_start(void);
void onvif_tcp_suspend(void);
void onvif_tcp_resum(void);
int connectTimeOut(int sock_t, struct sockaddr *sock_addr, int timeout);
#endif


