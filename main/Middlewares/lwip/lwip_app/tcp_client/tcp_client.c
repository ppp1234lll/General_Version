#include "./tcp_client/tcp_client.h"
#include "appconfig.h"

struct netconn *tcp_clientconn = NULL;	// TCP CLIENT网络连接结构体
uint16_t tcp_client_flag;				// TCP客户端数据发送标志位
uint8_t *tcp_client_send_buff;
struct netbuf *sg_recvbuf = NULL;

/* START_TASK 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define TCPCLIENT_TASK_PRIO         	8        
#define TCPCLIENT_STK_SIZE          	1024      
TaskHandle_t TcpClient_Task_Handler;          
void tcp_client_task(void *pvParameters);    


//tcp客户端任务函数
static void tcp_client_task(void *pvParameters)
{
	struct pbuf *q;
	err_t err,recv_err;
	
	static ip_addr_t server_ipaddr,loca_ipaddr;
	static u16_t 		 server_port,loca_port;
	uint8_t tcp_connect_cnt = 0;

	while (1) 
	{
		if(g_lwipdev.tcp_status == LWIP_TCP_INIT_CONNECT) // TCP连接中
		{
			/* 获取IP地址 */
			server_port = g_lwipdev.remoteport;
			IP4_ADDR(&server_ipaddr, g_lwipdev.remoteip[0],g_lwipdev.remoteip[1], g_lwipdev.remoteip[2],g_lwipdev.remoteip[3]);
			/* 尝试连接TCP */
			tcp_clientconn = netconn_new(NETCONN_TCP);  						// 创建一个TCP链接
			if(tcp_clientconn == NULL)
			{
				eth_set_network_reset();
				break;
			}
			err = netconn_connect(tcp_clientconn,&server_ipaddr,server_port);	// 连接服务器
			if(err != ERR_OK)  													// 连接失败
			{
				netconn_delete(tcp_clientconn);  // 返回值不等于ERR_OK,删除tcp_clientconn连接
				tcp_clientconn = NULL;

				tcp_connect_cnt++;
				if(tcp_connect_cnt > LWIP_TCP_CONNECT_NUM)
				{
					/* 一定次数过后还未成功连接上服务器 */
					g_lwipdev.tcp_status = 0;
					tcp_connect_cnt = 0;
					/* 关闭TCP连接 */
					#ifdef WIRED_PRIORITY_CONNECTION
						/* 开启GPRS */
						gsm_set_tcp_cmd(1);
					#endif
					vTaskDelay(10); // 重启系统也很快,还能消除未知的隐患
					break;
				}
			}
			else																// 连接成功
			{
				g_lwipdev.tcp_status = LWIP_TCP_CONNECT;							// 服务器连接成功
				
				#ifdef WIRED_PRIORITY_CONNECTION
				gsm_set_tcp_cmd(0);
				#endif
				app_set_com_interface_selection_function(0);
				led_control_function(LD_LAN,LD_FLICKER);
		
				tcp_clientconn->recv_timeout = 10;
				netconn_getaddr(tcp_clientconn,&loca_ipaddr,&loca_port,1); 		// 获取本地IP主机IP地址和端口号
				app_send_once_heart_infor();  									// 发送一次心跳
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
				taskENTER_CRITICAL(); //关中断

				for(q=sg_recvbuf->p;q!=NULL;q=q->next)  //遍历完整个pbuf链表
				{
					//判断要拷贝到TCP_CLIENT_RX_BUFSIZE中的数据是否大于TCP_CLIENT_RX_BUFSIZE的剩余空间，如果大于
					//的话就只拷贝TCP_CLIENT_RX_BUFSIZE中剩余长度的数据，否则的话就拷贝所有的数
					if(q->len > 0) {
						com_stroage_cache_data(q->payload,q->len);
					}					
				}
				taskEXIT_CRITICAL();  //开中断				
				
				netbuf_delete(sg_recvbuf);
				sg_recvbuf = NULL;
			}
			else if(recv_err == ERR_CLSD)  //关闭连接
			{
				netconn_close(tcp_clientconn);
				netconn_delete(tcp_clientconn);
				g_lwipdev.tcp_status = LWIP_TCP_NO_CONNECT;		// 服务器断开
				tcp_connect_cnt = 0;
			}
		}
		
		
		/* 停止tcp */
		if(g_lwipdev.tcp_reset == 1)					  // 重启tcp连接
		{
			g_lwipdev.tcp_reset = 0;
			tcp_client_stop_function();
		}
		
		vTaskDelay(10);
	}
	
}

//创建TCP客户端线程
//返回值:0 TCP客户端创建成功
//		其他 TCP客户端创建失败
int tcp_client_init(void)
{
	BaseType_t res;
	
	taskENTER_CRITICAL();	/*进入临界区*/
	
	xTaskCreate((TaskFunction_t )tcp_client_task,
							(const char *   )"tcp_client_task",
							(uint16_t       )TCPCLIENT_STK_SIZE,
							(void *         )NULL,
							(UBaseType_t    )TCPCLIENT_TASK_PRIO,
							(TaskHandle_t * )&TcpClient_Task_Handler);

	taskEXIT_CRITICAL();	/*退出临界区*/
	
	return res;
}

/************************************************************
*
* Function name	: tcp_client_start_function
* Description	: tcp客户端启动函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_client_start_function(void)
{
	/* 创建TCP客户端 */
	g_lwipdev.tcp_reset = 0;
	tcp_client_init();
}

/************************************************************
*
* Function name	: tcp_client_stop_function
* Description	: tcp客户端停止函数
* Parameter		: 
* Return		: 
*	
************************************************************/
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
	
	taskENTER_CRITICAL();	/*进入临界区*/
	vTaskDelete(TcpClient_Task_Handler);
	taskEXIT_CRITICAL();	/*退出临界区*/
}


/************************************************************
*
* Function name	: tcp_send_data_immediately
* Description	: 立刻发送tcp数据
* Parameter		: 
* Return		: 
*	
************************************************************/
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

/************************************************************
*
* Function name	: tcp_set_send_buff
* Description	: tcp发送数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_set_send_buff(uint8_t *buff, uint16_t len)
{
	tcp_client_send_buff = buff;
	tcp_client_flag      = len + LWIP_SEND_DATA;
}

/************************************************************
*
* Function name	: tcp_get_send_status
* Description	: tcp发送状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t tcp_get_send_status(void)
{
	return tcp_client_flag;
}


