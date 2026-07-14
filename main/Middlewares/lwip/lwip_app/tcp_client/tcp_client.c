#include "./tcp_client/tcp_client.h"
#include "main.h"

struct netconn *tcp_clientconn = NULL;    // TCP CLIENT网络连接结构体
uint16_t tcp_client_flag;                 // TCP客户端数据发送标志位
uint8_t *tcp_client_send_buff;
struct netbuf *sg_recvbuf = NULL;

/* START_TASK 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define TCPCLIENT_TASK_PRIO            11        
#define TCPCLIENT_STK_SIZE             1024      
TaskHandle_t TcpClient_Task_Handler;          
void tcp_client_task(void *pvParameters);    

/* 任务退出请求标志: 任务不再自删除, 而是停到安全点置1,
 * 由 eth 任务在其上下文中调用 tcp_client_reap() 同步删除, 立即回收栈/TCB */
static volatile uint8_t sg_tcp_client_exit_req = 0;


//tcp客户端任务函数
static void tcp_client_task(void *pvParameters)
{
    struct pbuf *q;
    err_t err,recv_err;
    
    static ip_addr_t server_ipaddr,loca_ipaddr;
    static uint16_t  server_port,loca_port;
    uint8_t tcp_connect_cnt = 0;

    while (1) 
    {
        if(g_lwipdev.tcp_status == LWIP_TCP_INIT_CONNECT) // TCP连接中
        {
            /* 获取IP地址 */
            server_port = g_lwipdev.remoteport;
            IP4_ADDR(&server_ipaddr, g_lwipdev.remoteip[0],g_lwipdev.remoteip[1], g_lwipdev.remoteip[2],g_lwipdev.remoteip[3]);
            /* 尝试连接TCP */
            tcp_clientconn = netconn_new(NETCONN_TCP);                          // 创建一个TCP链接
            if(tcp_clientconn == NULL)
            {
                g_lwipdev.tcp_status = LWIP_TCP_NO_CONNECT;
                vTaskDelay(100);
                continue;
            }
            err = netconn_connect(tcp_clientconn,&server_ipaddr,server_port);    // 连接服务器
            if(err != ERR_OK)                                                    // 连接失败
            {
                netconn_delete(tcp_clientconn);  // 返回值不等于ERR_OK,删除tcp_clientconn连接
                tcp_clientconn = NULL;

                tcp_connect_cnt++;
                if(tcp_connect_cnt > LWIP_TCP_CONNECT_NUM)
                {
                    /* 一定次数过后还未成功连接上服务器 */
                    tcp_connect_cnt = 0;
                    /* 关闭TCP连接 */
                    #ifdef WIRED_PRIORITY_CONNECTION
                        /* 开启GPRS */
                        gsm_set_tcp_cmd(1);
                    #endif
                    /* 达重试上限: 复位状态并跳出主循环到安全点, 交由 eth 任务同步删除。
                     * 不用 vTaskDelete(NULL) 自删除: 自删除的栈/TCB 要推迟到空闲任务才
                     * 回收, 频繁重连时来不及回收会堆积/碎片, 最终 xTaskCreate 返回
                     * 0xFFFFFFFF(内存不足) */
                    g_lwipdev.tcp_status = LWIP_TCP_NO_CONNECT;
                    break;
                }
            }
            else                                                                // 连接成功
            {
                g_lwipdev.tcp_status = LWIP_TCP_CONNECT;                            // 服务器连接成功
                
                #ifdef WIRED_PRIORITY_CONNECTION
                gsm_set_tcp_cmd(0);
                #endif
                led_control_function(LD_LAN,LD_FLICKER);
        
                tcp_clientconn->recv_timeout = 10;
                netconn_getaddr(tcp_clientconn,&loca_ipaddr,&loca_port,1);         // 获取本地IP主机IP地址和端口号
                app_send_once_heart_infor();                                      // 发送一次心跳
                update_status_detection();  // 更新状态检测
            }
        }
        else if(g_lwipdev.tcp_status == LWIP_TCP_CONNECT) // TCP连接成功
        {
            if((tcp_client_flag & LWIP_SEND_DATA) == LWIP_SEND_DATA) // 有数据要发送
            {
                err = netconn_write( tcp_clientconn ,tcp_client_send_buff,(tcp_client_flag & 0x3fff),NETCONN_COPY); 
                if(err != ERR_OK)
                {
                    /* 发送失败 */
                    app_set_send_result_function(SR_SEND_ERROR);
                }
                tcp_client_flag &= ~LWIP_SEND_DATA;
            }
                
            if((recv_err = netconn_recv(tcp_clientconn,&sg_recvbuf)) == ERR_OK)  //接收到数据
            {    
                for(q=sg_recvbuf->p;q!=NULL;q=q->next)  //遍历完整个pbuf链表
                {
                    if(q->len > 0) {
                        com_rx_feed(COM_CH_TCP, q->payload, q->len);
                    }                    
                }

                netbuf_delete(sg_recvbuf);
                sg_recvbuf = NULL;
            }
            else if(recv_err == ERR_CLSD)  //关闭连接
            {
                netconn_close(tcp_clientconn);
                netconn_delete(tcp_clientconn);
                g_lwipdev.tcp_status = LWIP_TCP_NO_CONNECT;        // 服务器断开
                tcp_connect_cnt = 0;
            }
        }
        
        /* 停止tcp: 复位连接后跳出到安全点, 由 eth 任务同步删除本任务 */
        if(g_lwipdev.tcp_reset == 1)                      // 重启tcp连接
        {
            g_lwipdev.tcp_reset = 0;
            tcp_client_stop_function();   // 仅关闭连接并复位状态, 不删除任务
            break;
        }
        
        vTaskDelay(10);
    }

    /* 到达安全点(重试超限或收到停止请求): 置退出请求并挂起自身,
     * 由 eth 任务调用 tcp_client_delete() 在其上下文中同步删除本任务,
     * 立即回收栈/TCB, 从根本上避免重建时 xTaskCreate 返回 0xFFFFFFFF */
    sg_tcp_client_exit_req = 1;
    for(;;)
    {
        vTaskSuspend(NULL);
    }
}

//创建TCP客户端线程
//返回值:pdPASS TCP客户端创建成功
//        其他 TCP客户端创建失败
int tcp_client_init(void)
{
    BaseType_t res;

    /* 有任务正等待删除(已停在安全点): 本轮先不创建, 待 tcp_client_reap() 回收后再建,
     * 避免与待删除任务并存或被误判为"已在运行" */
    if(sg_tcp_client_exit_req != 0)
    {
        return pdFAIL;
    }

    /* 任务已存在则不再重复创建, 避免产生重复任务和句柄泄漏 */
    if(TcpClient_Task_Handler != NULL)
    {
        return pdPASS;
    }

    taskENTER_CRITICAL();    /*进入临界区*/

    res = xTaskCreate((TaskFunction_t )tcp_client_task,
                    (const char *   )"tcp_client_task",
                    (uint16_t   )TCPCLIENT_STK_SIZE,
                    (void *   )NULL,
                    (UBaseType_t    )TCPCLIENT_TASK_PRIO,
                    (TaskHandle_t * )&TcpClient_Task_Handler);

    taskEXIT_CRITICAL();    /*退出临界区*/

    if(res != pdPASS)
    {
        /* 创建失败(通常为堆不足), 复位句柄, 避免残留无效句柄 */
        TcpClient_Task_Handler = NULL;
    }

    return res;
}

/*
*********************************************************************************************************
*    函 数 名: tcp_client_start_function
*    功能说明: tcp客户端启动函数
*    形    参: 
*    返 回 值: pdPASS 创建成功, 其他 创建失败
*********************************************************************************************************
*/
int tcp_client_start_function(void)
{
    /* 创建TCP客户端 */
    g_lwipdev.tcp_reset = 0;
    return tcp_client_init();
}

/*
*********************************************************************************************************
*    函 数 名: tcp_client_task_is_running
*    功能说明: 查询TCP客户端任务是否存在(供重连看门狗判断任务是否已被删除)
*    形    参: 无
*    返 回 值: 1-任务存在  0-任务不存在
*********************************************************************************************************
*/
uint8_t tcp_client_task_is_running(void)
{
    /* 停在安全点等待删除的任务视为"不在运行" */
    return (TcpClient_Task_Handler != NULL && sg_tcp_client_exit_req == 0) ? 1 : 0;
}

/*
*********************************************************************************************************
*    函 数 名: tcp_client_stop_function
*    功能说明: tcp客户端停止函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void tcp_client_stop_function(void)
{
    g_lwipdev.tcp_reset = 0;
    led_control_function(LD_LAN,LD_ON);
    /* 关闭tcp服务器 */
    if(g_lwipdev.tcp_status == LWIP_TCP_CONNECT)
    {
        netconn_close(tcp_clientconn);
    }
    netconn_delete(tcp_clientconn);
    tcp_clientconn = NULL;
    g_lwipdev.tcp_status = LWIP_TCP_NO_CONNECT;
    /* 不在此删除任务: 本函数在 tcp_client_task 自身上下文中被调用(tcp_reset),
     * 自删除会推迟回收; 调用方随后 break 到安全点, 由 eth 任务同步删除 */
}

/*
*********************************************************************************************************
*    函 数 名: tcp_client_delete
*    功能说明: 回收已停到安全点的TCP客户端任务; 必须由 eth 任务等"其它任务"上下文周期调用。
*              在其它任务上下文调用 vTaskDelete 会立即释放栈/TCB, 避免自删除延迟回收
*              造成的堆积/碎片, 从而杜绝重建时 xTaskCreate 返回 0xFFFFFFFF(内存不足)。
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void tcp_client_delete(void)
{
    TaskHandle_t handle;

    if(sg_tcp_client_exit_req == 0)
    {
        return;
    }

    taskENTER_CRITICAL();    /*进入临界区*/
    handle = TcpClient_Task_Handler;
    TcpClient_Task_Handler = NULL;      // 先置空, 防止重复删除或期间被重建
    sg_tcp_client_exit_req = 0;
    taskEXIT_CRITICAL();    /*退出临界区*/

    if(handle != NULL)
    {
        vTaskDelete(handle);            // 其它任务上下文删除 -> 立即回收栈/TCB
    }
}


/*
*********************************************************************************************************
*    函 数 名: tcp_send_data_immediately
*    功能说明: 立刻发送tcp数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t tcp_send_data_immediately(uint8_t *str, uint16_t len)
{
    err_t err;
    if(g_lwipdev.tcp_status != LWIP_TCP_CONNECT) {
        return 0;
    }    
    err = netconn_write(tcp_clientconn ,(const void *)str,len,NETCONN_COPY);
    if(err != ERR_OK)
    {
        return -1;
    }
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: tcp_set_send_buff
*    功能说明: tcp发送数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void tcp_set_send_buff(uint8_t *buff, uint16_t len)
{
    tcp_client_send_buff = buff;
    tcp_client_flag      = len + LWIP_SEND_DATA;
}

/*
*********************************************************************************************************
*    函 数 名: tcp_get_send_status
*    功能说明: tcp发送状态
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t tcp_get_send_status(void)
{
    return tcp_client_flag;
}


