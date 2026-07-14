
#include "./onvif/onvif_tcp.h"
#include "main.h"

#define ONVIF_DEBUG                 1

#define ONVIF_TCP_PRIO              9       //    TCP客户端任务
#define ONVIF_TCP_STK_SIZE          1024    //    任务堆栈大小
TaskHandle_t OnvifTcp_Task_Handler;
static volatile uint8_t s_onvif_tcp_exit_req = 0;  /* 1: 任务已停到安全点, 待外部同步删除 */

int onvif_tcp_sockt = -1;    
onvif_tcp_t sg_onvif_tcp;  // TCP状态标志位

IPC_Param_t sg_ipc_param;
Onvif_OSD_Param_t osd_params;

__attribute__((section (".RAM_D1"))) Onvif_OSD_Param_t sg_osd_t[6] = {0};

#define ONVIF_TCP_PORT 80

//udp任务函数
static void onvif_tcp_thread(void *arg)
{
    char onvif_ipbuf[20] ={0};
    int  onvif_port  = 0;
    IPC_Info_t *sg_ipc_t = onvif_get_ipc_param();

    if(ONVIF_DEBUG) printf("create onvif_tcp_thread \r\n");
    memset(&sg_ipc_param,0,sizeof(IPC_Param_t));  // 清除上次信息    

    while(1)
    {
        if(g_lwipdev.onvif_tcp_status == LWIP_TCP_INIT_CONNECT) 
        {
            g_lwipdev.onvif_tcp_status = LWIP_TCP_CONNECT;
            sg_onvif_tcp.status = 1; // 开始查询
        }
        else if(g_lwipdev.onvif_tcp_status == LWIP_UDP_CONNECT) // 连接成功
        {
            if(sg_onvif_tcp.status == 1)
            {
                for(uint8_t c_id=0;c_id<sg_ipc_t->ipc_num;c_id++) // 循环检测
                {
                    sprintf(onvif_ipbuf,"%d.%d.%d.%d", sg_ipc_t->ipc_param[c_id].ip[0],sg_ipc_t->ipc_param[c_id].ip[1],sg_ipc_t->ipc_param[c_id].ip[2],sg_ipc_t->ipc_param[c_id].ip[3]);
                    onvif_port = ONVIF_TCP_PORT;
//                    if(onvif_tcp_link_carema(onvif_tcp_sockt,onvif_ipbuf,onvif_port) == 0)
//                    {                
//                        if(ONVIF_IPC_Token_API(onvif_tcp_sockt,onvif_ipbuf,onvif_port,sg_ipc_t->ipc_param[c_id].brand)<0)
//                        {
//                            printf("ONVIF_IPC_Token_API error\n");
//                        }
//                    }
//                    else
//                        if(ONVIF_DEBUG) printf("onvif tcp_link error\n");
//                    close(onvif_tcp_sockt);
                    
                    // 读取OSD标注
                    memset(&osd_params,0,sizeof(Onvif_OSD_Param_t));
                    if(onvif_tcp_link_carema(onvif_tcp_sockt,onvif_ipbuf,onvif_port) == 0)
                    {                
                        if(ONVIF_IPC_OSD_API(onvif_tcp_sockt,onvif_ipbuf,onvif_port,sg_ipc_t->ipc_param[c_id].brand)<0)
                        {
                            printf("ONVIF_IPC_OSD_API error\n");
                        }
                    }
                    sprintf(sg_ipc_t->ipc_param[c_id].osd,"%s",osd_params.name);
                    close(onvif_tcp_sockt);
                }
                // 打印osd_name
                for(uint8_t c_id=0;c_id<sg_ipc_t->ipc_num;c_id++)
                {
                    printf("osd_name:%s\n",sg_ipc_t->ipc_param[c_id].osd);
                }
                // 按照16进制打印osd_name
                for(uint8_t c_id=0;c_id<sg_ipc_t->ipc_num;c_id++)
                {
                    printf("osd_name hex: ");
                    for (uint16_t i = 0; i < strlen(sg_ipc_t->ipc_param[c_id].osd); i++)
                    {
                        printf("%02X ", (uint8_t)sg_ipc_t->ipc_param[c_id].osd[i]);    
                    }
                    printf("\n");
                }
                sg_onvif_tcp.status = 2;
            }
        }
    
        if(g_lwipdev.onvif_tcp_reset == 1)                      // 重启UDP连接
        {
            g_lwipdev.onvif_tcp_reset = 0;
            onvif_tcp_client_stop();   // 仅关闭socket并复位状态, 不删除任务
            /* 不自删除: 置退出请求并挂起, 由 eth 任务 onvif_tcp_delete() 同步删除 */
            s_onvif_tcp_exit_req = 1;
            for(;;)
            {
                vTaskSuspend(NULL);
            }
        }
        FeedFwdgt();
        vTaskDelay(200);  //延时5ms
    }
}

//创建UDP线程
//返回值:0 UDP创建成功
//        其他 UDP创建失败
unsigned char onvif_tcp_client_init(void)
{
    long res;
    
    if(s_onvif_tcp_exit_req){ return 0; }                /* 有任务待回收, 本轮先不创建 */
    if(OnvifTcp_Task_Handler != NULL){ return pdPASS; }  /* 已存在, 不重复创建 */
    
    res = xTaskCreate(  (TaskFunction_t )onvif_tcp_thread,
                        (const char *   )"onvif_tcp_thread",
                        (uint16_t       )ONVIF_TCP_STK_SIZE,
                        (void *         )NULL,
                        (UBaseType_t    )ONVIF_TCP_PRIO,
                        (TaskHandle_t * )&OnvifTcp_Task_Handler); //创建UDP线程
    if(res != pdPASS){ OnvifTcp_Task_Handler = NULL; }

    return res;
}

/*
*********************************************************************************************************
*    函 数 名: onvif_tcp_stop
*    功能说明: tcp客户端停止函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void onvif_tcp_client_stop(void)
{
    g_lwipdev.tcp_reset = 0;
    
    if(onvif_tcp_sockt >=0)
        close(onvif_tcp_sockt);
    
    sg_onvif_tcp.tcp_status = 0;  // 状态清零
    sg_onvif_tcp.status = 0;
    
    g_lwipdev.onvif_tcp_status = LWIP_TCP_NO_CONNECT;
    
    /* 不在此删除任务: 由 onvif_tcp_delete() 在 eth 任务上下文完成,
     * 避免自删除延迟回收 + 重建竞态 */
}

/*
*********************************************************************************************************
*    函 数 名: onvif_tcp_delete
*    功能说明: 回收已停到安全点的ONVIF TCP查询任务; 须由 eth 任务等其它任务上下文周期调用,
*              在外部上下文 vTaskDelete, 立即回收栈/TCB, 避免自删除延迟回收
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void onvif_tcp_delete(void)
{
    TaskHandle_t h;

    if(s_onvif_tcp_exit_req == 0){ return; }

    taskENTER_CRITICAL();    /*进入临界区*/
    h = OnvifTcp_Task_Handler;
    OnvifTcp_Task_Handler = NULL;
    s_onvif_tcp_exit_req = 0;
    taskEXIT_CRITICAL();    /*退出临界区*/

    if(h != NULL){ vTaskDelete(h); }
}

/*
*********************************************************************************************************
*    函 数 名: onvif_tcp_link_carema
*    功能说明: tcp连接
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int onvif_tcp_link_carema(int sockt,char *ip,int port)
{
    struct sockaddr_in server_addr;
//    struct timeval tv_out;
    
    onvif_tcp_sockt = socket(AF_INET, SOCK_STREAM, 0); // SOCK_STREAM:提供面向连接的稳定数据传输，即TCP协议 
    if (onvif_tcp_sockt < 0)
    {
        if(ONVIF_DEBUG) printf("Socket error\n");
        close(onvif_tcp_sockt);
        return -1;  // 失败
    }            
    else
    {
        /* 设置要访问的服务器的信息 */
        server_addr.sin_family = AF_INET;            // IPv4
        server_addr.sin_addr.s_addr = inet_addr(ip); // 服务器IP
        server_addr.sin_port = htons(port);          // 端口
        memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));
        
        /* 连接到服务端 */
        if (connect(onvif_tcp_sockt, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)))
        {
            if(ONVIF_DEBUG) printf("Unable to connect\n");
            close(onvif_tcp_sockt);
            return -1;  // 失败
        }            

        int value = 1; //1 开启端口复用 0关闭
                    setsockopt(onvif_tcp_sockt, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value));

                    unsigned long mode = 1;
                    ioctlsocket(onvif_tcp_sockt, FIONBIO, &mode); // 设置为非阻塞模式
        
        
        if(ONVIF_DEBUG) printf("connect...success... \n");
        return 0;  
    }
}

/*
*********************************************************************************************************
* 名    称: onvif_tcp_get_ipc_read_status
* 功    能：返回读取的状态：
* 入口参数：
* 返回参数：sg_ipc_flag.status
* 说    明：0正在读取，1读取成功，2失败
*********************************************************************************************************
*/
int8_t onvif_tcp_get_ipc_read_status(void)
{
    return sg_onvif_tcp.status;
}

/*
*********************************************************************************************************
* 名    称: onvif_get_ipc_ip_str
* 功    能：获取IPC IP地址：
* 入口参数：
* 返回参数： 
* 说    明： 
*********************************************************************************************************
*/
void onvif_get_ipc_ip_str(char *buff)
{
    uint8_t ip[4]    = {0};
    uint8_t num = app_get_camera_num_function();
    if(app_get_camera_function(ip,num) < 0)
    {}
    else
    {
        sprintf(buff,"%d.%d.%d.%d", ip[0],ip[1],ip[2],ip[3]);
    }
}
/*
*********************************************************************************************************
* 名    称: onvif_get_ipc_port_str
* 功    能：获取IPC IP端口：
* 入口参数：
* 返回参数： 
* 说    明： 
*********************************************************************************************************
*/
int onvif_get_ipc_port_str(void)
{
    uint8_t num = app_get_camera_num_function();
    return app_get_camera_port_function(num);
}

/*
*********************************************************************************************************
* 名    称: onvif_get_ipc_net_status
* 功    能：获取IPC 网络状态：
* 入口参数：
* 返回参数： 
* 说    明： 
*********************************************************************************************************
*/
uint8_t onvif_get_ipc_net_status(void)
{
    uint8_t num = app_get_camera_num_function();
    return com_report_get_camera_status(num);
}

/*
*********************************************************************************************************
*函数名:Hex2Ascii
*功能描述:把16进制转Ascii字符
*参数：16进制
*返回：Ascii字符
*********************************************************************************************************
*/
unsigned char Ascii2Hex( unsigned char bAscii )        
{
    unsigned char bHex = 0;
    if( ( bAscii >= '0' ) && ( bAscii <= '9' ) )
        bHex =  bAscii - '0';
    else if( ( bAscii >= 'A' ) && ( bAscii <= 'F' ) )
        bHex = bAscii - '7';
    else if( ( bAscii >= 'a' ) && ( bAscii <= 'f' ) )
        bHex = bAscii - 0x57;
    else
        bHex = 0xff;
    return bHex;
}

int tcp_http_test(int sockfd) // 百度网页测试
{
    int len=0;
    char *send_data = "GET / HTTP/1.1\r\n\r\n";
    char rcvData[512];

    send(sockfd, send_data, 18, 0); 
    len = recv(sockfd, rcvData, TCP_RX_BUFSIZE, 0);    /* 接收并打印响应的数据，使用加密数据传输 */
    if(len > 0)
    {
        if(ONVIF_DEBUG) printf("%s \r\n", rcvData);
    }
    return len;
}
