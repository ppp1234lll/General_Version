#include "rtsp.h"
#include "rtsp_task.h"
#include "main.h"
#include <lwip/sockets.h>


#define RTSP_PRIO           6                   // TCP客户端任务
#define RTSP_STK_SIZE       512                  // 任务堆栈大小
TaskHandle_t RTSP_Task_Handler;

#define RTSP_DEBUG  1         // 调试

#define RTSP_MAX_CAMERAS     10
#define RTSP_SCAN_TIME       30     // 每轮扫描时间间隔 30s

typedef struct
{
    uint8_t  rtsp_falg;  // RTSP线程状态，为创建则新建任务
    uint8_t  ipc_next; // 下一轮摄像机
    uint16_t ipc_time; // 计时
    uint8_t  ipc_idx;  // 当前正在扫描的摄像头 
    uint8_t  rtsp_cmd;
    uint8_t  rtsp_steps;  // 
    char     recv[512];        
    char     send[128];            
    uint8_t  result;   // 扫描结果
}rtsp_t;  

rtsp_t sg_rtsp_t; 
int sg_socket = -1;

//udp任务函数
static void rtsp_thread(void *arg)
{
    int ret = 0;
    static uint8_t rtsp_ip[4]={0};
    
    LWIP_UNUSED_ARG(arg);
    
    while (1) 
    {
        switch(sg_rtsp_t.rtsp_steps)
        {
            case 0:  // 获取IP地址
                ret = rtsp_get_ip_function(rtsp_ip);
                if(ret == 0)
                    sg_rtsp_t.rtsp_steps++;
                else if(ret == -1) 
                {
                    sg_rtsp_t.result = 0;
                    sg_rtsp_t.rtsp_steps = 9;
                }
            break;
            
            case 1:   // 连接IP地址
                if(rtsp_connect_server(rtsp_ip,554) == 0)
                    sg_rtsp_t.rtsp_steps++;
                else
                {
                    sg_rtsp_t.result = 0;
                    sg_rtsp_t.rtsp_steps = 9;
                }
            break;            
            
            case 2:   // 发送RTSP
                if(rtsp_send_method(rtsp_ip) == 0)
                    sg_rtsp_t.rtsp_steps++;
            break;                

            case 3:   // 接收
                ret = rtsp_recv_method() ;
                if(ret > 0 )  // 接收到数据
                    sg_rtsp_t.rtsp_steps++;
                else if(ret < 0 ) // 超时
                {
                    sg_rtsp_t.result = 0;
                    sg_rtsp_t.rtsp_steps = 9;
                }
            break;    

            case 4:   // 处理接收数据
//                ret = rtsp_deal_recv_data(sg_rtsp_t.recv);
//                if(ret < 0 )  
//                {
//                    sg_rtsp_t.result = 0;
//                    sg_rtsp_t.rtsp_steps = 9;
//                }
//                else
//                {
                    sg_rtsp_t.result = 1;
                    sg_rtsp_t.rtsp_steps++;
//                }
            break;    
                
            default:
                if(sg_socket >= 0)
                {
                    closesocket(sg_socket); // 关闭连接
                    sg_socket = -1;
                }
            
                if(sg_rtsp_t.result == 1)
                    det_set_camera_status(sg_rtsp_t.ipc_idx,NET_STATUS_NORMAL);  // 网络正常
                else
                    det_set_camera_status(sg_rtsp_t.ipc_idx,NET_STATUS_FAULT);  // 网络故障
                
                sg_rtsp_t.rtsp_steps = 0;
                sg_rtsp_t.ipc_idx++;
                if(sg_rtsp_t.ipc_idx >= RTSP_MAX_CAMERAS)
                {
                    sg_rtsp_t.ipc_idx  = 0;
                    sg_rtsp_t.ipc_next = 0; /* 开始新的一轮计时 */
                }
            break;
        }
        vTaskDelay(50);  //延时5s
    }
}

/*
*********************************************************************************************************
*    函 数 名: rtsp_timer_function
*    功能说明: 扫描时间
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void rtsp_timer_function(void)
{
    if(sg_rtsp_t.ipc_next == 0)
    {
        sg_rtsp_t.ipc_time++;
        if(sg_rtsp_t.ipc_time > RTSP_SCAN_TIME)
        {
            /* 开始一次扫描 */
            sg_rtsp_t.ipc_time  = 0;
            sg_rtsp_t.ipc_idx   = 0;
            sg_rtsp_t.ipc_next  = 1;
            sg_rtsp_t.rtsp_steps = 0;
        }
    }
}


/*
*********************************************************************************************************
*    函 数 名: rtsp_get_ip_function
*    功能说明: 获取IP
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
int8_t rtsp_get_ip_function(uint8_t ip[4])
{
    int8_t  ret = 0;
    
    /* 检测是否可以开始一轮扫描  */
    if(sg_rtsp_t.ipc_next == 0)
    {
        return -2;
    }
    else
    {
        if(sg_rtsp_t.rtsp_cmd == 0) 
        {
            if(app_get_camera_function(ip,sg_rtsp_t.ipc_idx) <0) /* 未检测摄像头ip，直接标记 */    
                ret = -1;
            else                    /* 检测到摄像头ip，开始ping */
                ret = 0;
        }
    }
    return ret;
}
/*
*********************************************************************************************************
*    函 数 名: rtsp_connect_server
*    功能说明: 连接RTSP服务器
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
int8_t rtsp_connect_server(uint8_t ip[4],int port)
{
    struct sockaddr_in server_addr;
    char ip_param[20] = {0};
    int ret = -1;
    
    sg_socket = socket(AF_INET, SOCK_STREAM, 0);  /* 可靠数据流交付服务既是TCP协议 */
    if (sg_socket < 0)
    {
        if(RTSP_DEBUG) printf("Socket error\n");
        close(sg_socket);
        sg_socket = -1;
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
        ioctlsocket(sg_socket, FIONBIO, &val);
        
        /* 连接远程IP地址 */
        if (connect(sg_socket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)))
        {
//            if(RTSP_DEBUG) printf("%s:%d error\n", ip_param, port);
//            ret = -1;  // 连接失败
//            closesocket(sock);
//            sock = -1;
        }
        else
        {
            if(RTSP_DEBUG) printf("%s:%d open\n", ip_param, port);
            ret = 0; // 连接成功
        }
        
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sg_socket, &fdset);
    //可以利用tv_sec和tv_usec做更小精度的超时控制
    struct timeval timeout;
    timeout.tv_sec = 1;  
    timeout.tv_usec = 0;
    if (select(sg_socket + 1, NULL, &fdset, NULL, &timeout) == 1)
    {
            ret = 0; // 连接成功
      if(RTSP_DEBUG) printf("select %s:%d open\n", ip_param,port);
    } 
        else 
        {
            ret = -1;  // 连接失败
            closesocket(sg_socket);
            sg_socket = -1;
      if(RTSP_DEBUG) printf("select %s:%d error\n", ip_param,port); 
    }
    }
    return ret;
}


/*
*********************************************************************************************************
*    函 数 名: rtsp_send_method
*    功能说明: 发送RTSP数据
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
int rtsp_send_method(uint8_t ip[4])
{
    int  data_len = 0;    
    memset(sg_rtsp_t.send,0,128);    
    memset(sg_rtsp_t.recv,0,512);    
    data_len  = sprintf(sg_rtsp_t.send,"OPTIONS rtsp://%d.%d.%d.%d:554/ RTSP/1.0\r\n",ip[0],ip[1],ip[2],ip[3]); // 方法
    data_len += sprintf(sg_rtsp_t.send+data_len,"%s","CSeq: 2\r\n");
    data_len += sprintf(sg_rtsp_t.send+data_len,"%s","User-Agent: LibVLC/3.0.19 (LIVE555 Streaming Media v2016.11.28)\r\n\r\n");

    send(sg_socket, sg_rtsp_t.send, data_len, 0); // socket数据发送
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: rtsp_recv_method
*    功能说明: 发送RTSP数据
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
int rtsp_recv_method(void)
{
    static uint8_t recv_times = 0;
    int recv_len = 0;
    recv_len = recv(sg_socket,sg_rtsp_t.recv,512, 0);    
    if(recv_len > 0)
        return recv_len;
    else
    {
        recv_times++;
        if(recv_times >= 20)
        {
            recv_times = 0;
            return -1;
        }
    }
    return 0;
}
/*
*********************************************************************************************************
*    函 数 名: rtsp_deal_recv_data
*    功能说明: 处理RTSP数据
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
int rtsp_deal_recv_data(char *data)
{
    char *str = NULL;

    str = strstr(data,"200 OK");    /*SOAP-ENV:Sender*/
    if(str == NULL)      // 未找到指定字符串     
    {
        return -1;
    }
  return 0;
}

//创建UDP线程
//返回值:0 UDP创建成功
//        其他 UDP创建失败
unsigned char rtsp_thread_init(void)
{                     
    BaseType_t res;                                        
    taskENTER_CRITICAL();    /*进入临界区*/
    
    xTaskCreate((TaskFunction_t )rtsp_thread,
                            (const char *   )"rtsp_thread",
                            (uint16_t       )RTSP_STK_SIZE,
                            (void *         )NULL,
                            (UBaseType_t    )RTSP_PRIO,
                            (TaskHandle_t * )&RTSP_Task_Handler);

    taskEXIT_CRITICAL();    /*退出临界区*/
    
    return res;
}

/*
*********************************************************************************************************
*    函 数 名: onvif_udp_start
*    功能说明: udp启动函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void rtsp_thread_start(void)
{
    if(sg_rtsp_t.rtsp_falg == 0)
    {
        if(rtsp_thread_init()==0)
            sg_rtsp_t.rtsp_falg = 1;
    }
}

/*
*********************************************************************************************************
*    函 数 名: rtsp_thread_stop
*    功能说明: tcp客户端停止函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void rtsp_thread_stop(void)
{
    if(sg_rtsp_t.rtsp_falg == 1)
    {
        sg_rtsp_t.rtsp_falg = 0;
        taskENTER_CRITICAL();    /*进入临界区*/
        vTaskDelete(RTSP_Task_Handler);        // 删除TCP任务
        taskEXIT_CRITICAL();    /*退出临界区*/
    }
}

