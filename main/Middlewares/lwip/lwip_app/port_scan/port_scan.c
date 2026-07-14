
#include "main.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include <lwip/sockets.h>

#define PORT_DEBUG  1// 调试

// 常见摄像头使用的端口列表
static const uint16_t camera_ports[] = {80, 554, 8000, 37777, 5000, 8080, 8899};
#define CAMERA_PORTS_NUM     7
#define MAX_CAMERAS          10
#define CAREMA_SCAN_TIME     30     // 每轮扫描时间间隔 30s
#define PORT_SCAN_TIME       10     // 端口扫描时间间隔 100ms


typedef enum
{
  PORT_HTTP = 0,
  PORT_RTSP,
  PORT_Camera_Web,
  PORT_Dahua,
  PORT_Camera0,
  PORT_HTTP_Alt,
  PORT_Camera1,
}PORT_SERVICE_E;  // 服务类型

typedef struct
{
    uint8_t  ipc_next;     // 下一轮摄像机
    uint16_t ipc_time;     // 计时
    uint8_t  ipc_idx;      // 当前正在扫描的摄像头 
    
    uint8_t  port_time;    // 计时
    uint8_t  port_idx;    // 当前正在扫描的端口 
    uint8_t  result;      // 扫描结果
}port_scan_t;  

port_scan_t sg_port_scan_t; 

/*
*********************************************************************************************************
*    函 数 名: port_scan_timer_function
*    功能说明: 端口扫描时间
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void port_scan_timer_function(void)
{
    if(sg_port_scan_t.ipc_next == 0)
    {
        sg_port_scan_t.ipc_time++;
        if(sg_port_scan_t.ipc_time > CAREMA_SCAN_TIME)
        {
            /* 开始一次扫描 */
            sg_port_scan_t.ipc_time  = 0;
            sg_port_scan_t.ipc_idx   = 0;
            sg_port_scan_t.ipc_next  = 1;
        }
    }
}

/*
*********************************************************************************************************
*    函 数 名: port_scan_detection_function
*    功能说明: 端口扫描 函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void port_scan_detection_function(void)
{
    static uint8_t ipc_scan_cmd = 0;
    static uint8_t  ip[4]  = {0};
           int8_t          ret  = 0;
    
    /* 检测是否可以开始一轮扫描  */
    if(sg_port_scan_t.ipc_next == 0)
    {
        return;
    }

    if ( ipc_scan_cmd == 0) 
    {
        ret = app_get_camera_function(ip,sg_port_scan_t.ipc_idx);
        if(ret <0)                    /* 未检测摄像头ip，直接标记 */
        {
            det_set_camera_status(sg_port_scan_t.ipc_idx,0);
            sg_port_scan_t.ipc_idx++;
            if(sg_port_scan_t.ipc_idx >= MAX_CAMERAS)
            {
                if(PORT_DEBUG) printf("port_scan start\n");
                sg_port_scan_t.ipc_idx  = 0;
                sg_port_scan_t.ipc_next = 0; /* 开始新的一轮计时 */
            }
            ipc_scan_cmd = 0;
        }
        else                    /* 检测到摄像头ip，开始ping */
        {
            sg_port_scan_t.result = 0; // 清除扫描结果
            sg_port_scan_t.port_idx = 0; 
            ipc_scan_cmd  = 1;
        }
    }
    if(ipc_scan_cmd == 1)
    {
        ret = port_scan_start(ip,camera_ports[sg_port_scan_t.port_idx]);
        if(ret == 0)    // 获取到结果
        {
            sg_port_scan_t.result |= (1<<sg_port_scan_t.port_idx);
            sg_port_scan_t.port_idx++;
        }
        else if(ret == -1)    
        {
            sg_port_scan_t.result &=~ (1<<sg_port_scan_t.port_idx);
            sg_port_scan_t.port_idx++;
        }
        /* 开始下一个端口 */
        if(sg_port_scan_t.port_idx >= CAMERA_PORTS_NUM)
        {
            ipc_scan_cmd = 0;   
            if(sg_port_scan_t.result != 0)
                det_set_camera_status(sg_port_scan_t.ipc_idx,NET_STATUS_NORMAL);  // 网络正常
            else
                det_set_camera_status(sg_port_scan_t.ipc_idx,NET_STATUS_FAULT);  // 网络故障
        
            sg_port_scan_t.ipc_idx++;
            if(sg_port_scan_t.ipc_idx >= MAX_CAMERAS)
            {
                sg_port_scan_t.ipc_idx  = 0;
                sg_port_scan_t.ipc_next = 0; /* 开始新的一轮计时 */
            }
        }
    }
}

/*
*********************************************************************************************************
*    函 数 名: port_scan_start
*    功能说明: 开始端口扫描
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
int8_t port_scan_start(uint8_t ip[4],int port)
{
    struct sockaddr_in server_addr;
    int g_sock = -1;
    char ip_param[20] = {0};
    int ret = -1;
    
    g_sock = socket(AF_INET, SOCK_STREAM, 0);            /* 可靠数据流交付服务既是TCP协议 */
    if (g_sock < 0)
    {
        if(PORT_DEBUG) printf("Socket error\n");
        close(g_sock);
        ret =  -2;  // 创建失败
    }    
    else
    {
        sprintf(ip_param,"%d.%d.%d.%d",ip[0],ip[1],ip[2],ip[3]);
        server_addr.sin_family = AF_INET;    /* 表示IPv4网络协议 */
        server_addr.sin_port = htons(port);  /* 端口号 */
        server_addr.sin_addr.s_addr = inet_addr(ip_param);   /* 远程IP地址 */
        memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));
        
    /*配置成非阻塞模式*/
        int val = 1;
        ioctlsocket(g_sock, FIONBIO, &val);
        
        /* 连接远程IP地址 */
        if (connect(g_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)))
        {
            if(PORT_DEBUG) printf("%s:%d error\n", ip_param, camera_ports[sg_port_scan_t.port_idx]);
//            ret = -1;  // 连接失败
        }
        else
        {
            if(PORT_DEBUG) printf("%s:%d open\n", ip_param, camera_ports[sg_port_scan_t.port_idx]);
            ret = 0; // 连接成功
        }
        
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(g_sock, &fdset);
    //可以利用tv_sec和tv_usec做更小精度的超时控制
    struct timeval timeout;
    timeout.tv_sec = 1;  
    timeout.tv_usec = 0;
    if (select(g_sock + 1, NULL, &fdset, NULL, &timeout) == 1)
    {
            ret = 0; // 连接成功
      if(PORT_DEBUG) printf("select %s:%d open\n", ip_param, camera_ports[sg_port_scan_t.port_idx]);
    } 
        else 
        {
            ret = -1;  // 连接失败
      if(PORT_DEBUG) printf("select %s:%d error\n", ip_param, camera_ports[sg_port_scan_t.port_idx]); 
    }
        
        closesocket(g_sock);
        g_sock = -1;
    }
    return ret;
}

