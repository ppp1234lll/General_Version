/*
*********************************************************************************************************
* @File name  ：udp.c
* @Description：udp操作
* @Author     ：编号9527
* Version Date       Modification Description
* 1.0     2019-08-16 1.udp初始化并接收广播包
*********************************************************************************************************
*/
#include "./udp_multicast/udp_multicast.h"
#include "main.h"



// UDP 参数设置
#define UDP_MULTICAST_PRIO        3
//任务堆栈大小
#define UDP_MULTICAST_STK_SIZE    256
//任务堆栈
TaskHandle_t UdpMulticast_Task_Handler;

udp_multicast_t udp_multicast_param;

#define UDP_RX_LEN        500                         // udp最大接收数据长度
uint8_t udp_recv_buf[UDP_RX_LEN];        // UDP接收
uint8_t *udp_send_buf = NULL;                // UDP发送
uint16_t udp_flag;

int udp_sock = -1;  // SOCKET
static volatile uint8_t s_udp_multicast_exit_req = 0;  /* 1: 任务已停到安全点, 待外部同步删除 */

//udp任务函数
static void udp_multicast_thread(void *arg)
{
    int ret = 0;
    int len    =    0;
    struct sockaddr_in local_addr;
    struct sockaddr_in recv_addr;
    int      sender_len = sizeof(recv_addr);
    struct ip_mreq mreq;             // 多播地址结构体
    struct timeval tv_out;    
    struct local_ip_t       *local = app_get_local_network_function();
    char ip_param[20] = {0};
    char multicast_addr[20] = {0};
    uint32_t multicast_port = {0};
    
    LWIP_UNUSED_ARG(arg);

    while (1) 
    {
        if(g_lwipdev.udp_multicast_status == LWIP_UDP_INIT_CONNECT) // UDP连接中
        {    
            udp_sock = socket(AF_INET, SOCK_DGRAM, 0);  //创建一个UDP链接
            if(udp_sock < 0) 
                g_lwipdev.udp_multicast_reset = 1;
            else
            {                    
                sprintf(ip_param,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
                sprintf(multicast_addr,"%d.%d.%d.%d",local->multicast_ip[0],local->multicast_ip[1],
                                                                                         local->multicast_ip[2],local->multicast_ip[3]);
                multicast_port = local->multicast_port;
                local_addr.sin_family = AF_INET;
                local_addr.sin_addr.s_addr = htonl(INADDR_ANY); /*<! 待与 socket 绑定的本地网络接口 IP */   
                local_addr.sin_port = htons(multicast_port); /*<! 待与 socket 绑定的本地端口号 */   

                ret = bind(udp_sock, (struct sockaddr*)&local_addr, sizeof(local_addr));        // 将 Socket 与本地某网络接口绑定 
                if(ret != 0 )                
                {    }
                else
                {                
                    mreq.imr_multiaddr.s_addr    =    inet_addr(multicast_addr); /*<! 多播组 IP 地址设置 */
                    mreq.imr_interface.s_addr = inet_addr(ip_param); /*<! 待加入多播组的 IP 地址 */   

                    // 添加多播组成员（该语句之前，socket 只与 某单播IP地址相关联 执行该语句后 将与多播地址相关联）
                    ret = setsockopt(udp_sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq));

                    tv_out.tv_sec = 10;
                    tv_out.tv_usec = 0;
                    setsockopt(udp_sock, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out));            //recv延时时间设置
                    
                    g_lwipdev.udp_multicast_status = LWIP_UDP_CONNECT;
                }                        
            }    
        }
        else if(g_lwipdev.udp_multicast_status == LWIP_UDP_CONNECT) // 连接成功
        {                
//            udp_socket_send_api(udp_sock,udp_send_buf,len,multicast_addr,multicast_port);        
            if((udp_flag & UDP_MULTICAST_DATA) == UDP_MULTICAST_DATA) //有数据要发送
            {
                if(udp_socket_send_api(udp_sock,(char*)udp_send_buf,(udp_flag & 0x3fff),multicast_addr,multicast_port) < 0)
                {

                }
                udp_flag &= ~UDP_MULTICAST_DATA;
            }
                
            len = recvfrom(udp_sock,udp_recv_buf,UDP_RX_LEN,0,(struct sockaddr*)&recv_addr,(socklen_t *)&sender_len);
            if(len > 0)
            {
                print_stroage_queue_data((uint8_t *)udp_recv_buf,len);  // 数据存到队列
            }        
        }
        if(g_lwipdev.udp_multicast_reset == 1)                      // 重启UDP连接
        {
            g_lwipdev.udp_multicast_reset = 0;
            udp_multicast_stop_function();   // 仅关闭socket, 不删除任务
            /* 不自删除: 置退出请求并挂起, 由 eth 任务 udp_multicast_delete() 同步删除并复位状态 */
            s_udp_multicast_exit_req = 1;
            for(;;)
            {
                vTaskSuspend(NULL);
            }
        }
        FeedFwdgt();
        vTaskDelay(50);  //延时100        
    }            
}



//创建UDP线程
//返回值:0 UDP创建成功
//        其他 UDP创建失败
unsigned char udp_multicast_init(void)
{
                                        
    BaseType_t res;
    
    if(s_udp_multicast_exit_req){ return 0; }               /* 有任务待回收, 本轮先不创建 */
    if(UdpMulticast_Task_Handler != NULL){ return pdPASS; } /* 已存在, 不重复创建 */
    
    taskENTER_CRITICAL();    /*进入临界区*/
    
    xTaskCreate((TaskFunction_t )udp_multicast_thread,
                            (const char *   )"udp_multicast_thread",
                            (uint16_t       )UDP_MULTICAST_STK_SIZE,
                            (void *         )NULL,
                            (UBaseType_t    )UDP_MULTICAST_PRIO,
                            (TaskHandle_t * )&UdpMulticast_Task_Handler);

    taskEXIT_CRITICAL();    /*退出临界区*/
    
    return res;
}
/*
*********************************************************************************************************
*    函 数 名: udp_multicast_start_function
*    功能说明: udp服务器启动函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void udp_multicast_start_function(void)
{
    /* 创建TCP客户端 */
    g_lwipdev.udp_multicast_reset = 0;
    udp_multicast_init();
    
}

/*
*********************************************************************************************************
*    函 数 名: udp_multicast_stop_function
*    功能说明: udp停止函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void udp_multicast_stop_function(void)
{
    g_lwipdev.udp_multicast_reset = 0;
    
    /* 关闭udp服务器 */
    if(g_lwipdev.udp_multicast_status == LWIP_UDP_CONNECT)
    {
        closesocket(udp_sock);
    }
    /* 不在此删除任务/复位状态: 由 udp_multicast_delete() 在 eth 任务上下文完成,
     * 避免自删除延迟回收 + 重建竞态 */
}

/*
*********************************************************************************************************
*    函 数 名: udp_multicast_delete
*    功能说明: 回收已停到安全点的UDP组播任务; 须由 eth 任务等其它任务上下文周期调用,
*              在外部上下文 vTaskDelete 并复位状态, 立即回收栈/TCB, 避免自删除延迟回收
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void udp_multicast_delete(void)
{
    TaskHandle_t h;

    if(s_udp_multicast_exit_req == 0){ return; }

    taskENTER_CRITICAL();    /*进入临界区*/
    h = UdpMulticast_Task_Handler;
    UdpMulticast_Task_Handler = NULL;
    s_udp_multicast_exit_req = 0;
    taskEXIT_CRITICAL();    /*退出临界区*/

    g_lwipdev.udp_multicast_status = LWIP_UDP_NO_CONNECT;

    if(h != NULL){ vTaskDelete(h); }
}


/*
*********************************************************************************************************
*    函 数 名: udp_socket_send_api
*    功能说明: udp发送函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int udp_socket_send_api(int sockfd,char* data,int len,char* ip,int port)
{
    int ret = 0;
    struct sockaddr_in addr;      // internet环境下套接字的地址形式
    
    memset((void *)&addr,0,sizeof(struct sockaddr_in));
    addr.sin_family      = AF_INET;  
    addr.sin_port        = htons(port);    // 转换成网络
    addr.sin_addr.s_addr = inet_addr(ip);  // 将一个字符串格式的ip地址转换成一个uint32_t数字格式

    ret = sendto(sockfd,data,len,0,(struct sockaddr*)&addr,sizeof(addr)); // sendto主要在UDP连接中使用，作用是向另一端发送UDP报文   
    return ret;
}

/*
*********************************************************************************************************
*    函 数 名: udp_multicast_send_buff
*    功能说明: 发送数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void udp_multicast_send_buff(uint8_t *buff, uint16_t len)
{
    udp_send_buf = buff;
    udp_flag     = len | UDP_MULTICAST_DATA;
}
