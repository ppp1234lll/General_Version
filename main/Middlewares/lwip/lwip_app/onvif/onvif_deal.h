#ifndef __ONVIF_DEAL_H
#define __ONVIF_DEAL_H

#include "./SYSTEM/sys/sys.h"
#include <lwip/sockets.h>

#define TCP_RX_BUFSIZE      4000    //接收缓冲区长度
#define RX_ONE_MAX_BUF    512

#define Get_ALL_FUNCTION            "GetCapabilities"     // 获取设备在ONVIF中所包含的所有功能
#define Get_DEVICE_INFO                "GetDeviceInformation"     // 获取设备信息：制造商、版本号
#define Get_HOST_NAME                    "GetHostname"                     // 获取设备名称
#define Get_NETWORK_INTER            "GetNetworkInterfaces"     // 获取网络接口信息
#define Get_NETWORK_DNS                "GetDNS"     // 获取DNS信息
#define Get_NETWORK_GATEWAY        "GetNetworkDefaultGateway"     // 获取默认网关
#define Get_NETWORK_PROTOCOL    "GetNetworkProtocols"     // 获取网络协议:HTTP/HTTPS/RTSP
#define Get_SCOPES                        "GetScopes"     // 获取范围参数
#define Get_SERVICE                        "GetServices"      
#define Get_DATE_TIME                    "GetSystemDateAndTime"     // 获取日期、时间
#define Get_USER_INFO                    "GetUsers"     // 获取用户名和权限
#define Get_OSD_INFO                    "GetOSDs"     // 获取OSD信息
#define Get_CONFIG_TOKEN            "GetVideoSourceConfigurations"     // 获取config token
#define Set_DATE_TIME                    "SetSystemDateAndTime"     // 设置日期、时间

#define DEVICE_REBOOT                    "SystemReboot"     // 系统重启

typedef struct
{
    char admin[10];
    char passwd[10];    
}Onvif_Login_t;

typedef struct
{
    char manufacturer[20];
    char models[20];
    char fire_version[40];
    char serial_num[40];
    char hardware_id[5];
}IPC_device_info_t;

typedef struct
{
    uint16_t ip6[5];    // IPV6
    uint8_t ip[4];      // IPV4
    uint8_t netmask[4]; // 子网掩码
    uint16_t port;      // 端口
    uint8_t gateway[4]; // 网关
    uint8_t mac[6];     // MAC地址
}IPC_network_info_t;

typedef struct
{
    uint32_t timess;
    int year;
    int mon;
    int day;
    int hour;
    int min;
    int sec;
}Onvif_Time_t;

typedef struct
{
    char    config_token[30];
    char    name[128];
}Onvif_OSD_Param_t;

typedef struct
{
    Onvif_Login_t login;
    IPC_device_info_t device;
    IPC_network_info_t network;
    Onvif_Time_t times;
    Onvif_OSD_Param_t OSD_info;
}IPC_Param_t;

extern IPC_Param_t sg_ipc_param;
extern Onvif_OSD_Param_t osd_params;

int onvif_send(int sock_t,char *onvif_cmd,uint8_t sort,char *ip,int port,uint8_t brand);
int onvif_tcp_client_rev(int sockfd,char *recv_buf);

int ONVIF_IPC_ChkData_FUN(char *data);
int Get_Str_Between(char *pcBuf, char *pcRes,char *begin,char *end);

int ONVIF_IPC_Token_API(int socket_t,char *ip,int port,uint8_t brand);      // 获取 configtion token
int Get_IPC_Config_Token(char *buf);

int ONVIF_IPC_OSD_API(int socket_t,char *ip,int port,uint8_t brand);       // 获取OSD标注
int Get_IPC_OSD_Info(char *buf);

void *onvif_get_osd_param_function(void);
void onvif_param_clear(void);
void onvif_get_token_function(char *buff);

uint32_t onvif_get_times(void);
void onvif_set_times(uint32_t timess);
#endif

