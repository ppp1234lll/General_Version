#include "tcp_server.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "app.h"
#include "lwip/lwip_sys.h"
#include "lwip/api.h"
#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "led.h"
#include "tcp_client.h"

struct netconn *tcp_server_conn, *tcp_server_newconn;

//TCP客户端任务
#define TCPSERVER_PRIO			10
//任务堆栈大小
#define TCPSERVER_STK_SIZE	256
//任务堆栈
__align(8) static OS_STK TCPSERVER_TASK_STK[TCPSERVER_STK_SIZE];

//tcp服务器任务
static void tcp_server_thread(void *arg)
{
	err_t err;
	struct local_ip_t  *local = app_get_local_network_function();
	INT8U osres;	
	LWIP_UNUSED_ARG(arg);
	
	if(TCP_SERVER_DEBUG) printf("create tcp_server_thread \r\n");
	
	while (1) 
	{
		if(lwipdev.tcp_server_status == LWIP_TCP_INIT_CONNECT) // TCP创建
		{	
			tcp_server_conn = netconn_new(NETCONN_TCP);//创建一个TCP服务器连接
			if(tcp_server_conn == NULL)
			{
				if(TCP_SERVER_DEBUG) printf("tcp_server_conn error \r\n");
				lwipdev.tcp_server_reset = 1;
			}
			else
			{
				err = netconn_bind(tcp_server_conn,IP_ADDR_ANY,local->port);//绑定端口号TCP_SERVER_PORT
				if(err == ERR_OK)
				{
					err = netconn_listen(tcp_server_conn);	//进入监听模式
					if(err == ERR_OK)
					{
						if(TCP_SERVER_DEBUG) printf("TCP Server ready to accept");
						tcp_server_conn->recv_timeout = 10;		//禁止阻塞线程 等待20ms
						lwipdev.tcp_server_status = LWIP_TCP_SERVER_LINK;	// 服务器等待连接				
					}
					else
						if(TCP_SERVER_DEBUG) printf("TCP Server listen failed");
				}
				else
					if(TCP_SERVER_DEBUG) printf("netconn bind failed");			
			}
		}
		else if(lwipdev.tcp_server_status == LWIP_TCP_SERVER_LINK) // TCP创建成功
		{
			err = netconn_accept(tcp_server_conn,&tcp_server_newconn);  //接收连接请求
			if (err == ERR_OK)    //处理新连接的数据
			{ 		
				if(TCP_SERVER_DEBUG) printf("netconn_accept\n");			
				tcp_server_newconn->recv_timeout = 10;
				// 创建客户端任务
				osres = tcp_client_init((void *)tcp_server_newconn);
				if(TCP_SERVER_DEBUG) printf("tcp_server:%d\n",osres);		

				led_control_function(LD_LAN,LD_FLICKER);
				lwipdev.tcp_server_link = 1; // 标志位置1
			}
		}
		/* 停止tcp */
		if(lwipdev.tcp_server_reset == 1)					  // 重启tcp连接
		{
			lwipdev.tcp_server_reset = 0;
			tcp_server_stop_function();
		}	
		IWDG_Feed();
		OSTimeDlyHMSM(0,0,0,10);
	}
}


//创建TCP服务器线程
//返回值:0 TCP服务器创建成功
//		其他 TCP服务器创建失败
INT8U tcp_server_init(void)
{
	INT8U res, err;
	OS_CPU_SR cpu_sr;
	
	OS_ENTER_CRITICAL();	//关中断
	OSTaskCreateExt(	 tcp_server_thread, 																					//建立扩展任务(任务代码指针) 
										(void *)0,																					//传递参数指针 
										(OS_STK*)&TCPSERVER_TASK_STK[TCPSERVER_STK_SIZE-1], 					//分配任务堆栈栈顶指针 
										(INT8U)TCPSERVER_PRIO, 															//分配任务优先级 
										(INT16U)TCPSERVER_PRIO,															//(未来的)优先级标识(与优先级相同) 
										(OS_STK *)&TCPSERVER_TASK_STK[0], 											//分配任务堆栈栈底指针 
										(INT32U)TCPSERVER_STK_SIZE, 															//指定堆栈的容量(检验用) 
										(void *)0,																					//指向用户附加的数据域的指针 
										(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 
	OSTaskNameSet(TCPSERVER_PRIO, (INT8U *)(void *)"tcp_server", &err);
//	res = OSTaskCreate(tcp_server_thread,(void*)0,(OS_STK*)&TCPSERVER_TASK_STK[TCPSERVER_STK_SIZE-1],TCPSERVER_PRIO); //创建TCP服务器线程
	OS_EXIT_CRITICAL();		//开中断
	
	return res;
}

/************************************************************
*
* Function name	: tcp_server_start_function
* Description	: tcp服务器启动函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_server_start_function(void)
{
	/* 创建TCP服务器 */
	lwipdev.tcp_server_status = 0;
	tcp_server_init();
}

/************************************************************
*
* Function name	: tcp_server_stop_function
* Description	: tcp服务器停止函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_server_stop_function(void)
{
	lwipdev.tcp_server_reset = 0;
	
	OS_CPU_SR cpu_sr;
	led_control_function(LD_LAN,LD_ON);
	tcp_client_stop_function();
	/* 关闭tcp服务器 */
	if(lwipdev.tcp_server_status != LWIP_TCP_NO_CONNECT)
	{
		if(tcp_server_newconn!= NULL)
		{
			netconn_close(tcp_server_newconn);
			netconn_delete(tcp_server_newconn);
		}
		if(tcp_server_conn!= NULL)
		{
			netconn_close(tcp_server_conn);
			netconn_delete(tcp_server_conn);
		}
	}
	tcp_server_newconn = NULL;
	tcp_server_conn = NULL;
	lwipdev.tcp_server_link = 0;
	lwipdev.tcp_server_status = LWIP_TCP_NO_CONNECT;
	
	OS_ENTER_CRITICAL();		// 关中断
	OSTaskDel(TCPSERVER_PRIO);	// 删除TCP任务
	OS_EXIT_CRITICAL();			// 开中断
}


/************************************************************
*
* Function name	: tcp_server_get_link_status
* Description	: 获取TCP连接状态
* Parameter		: 
* Return		: 
*	只有在TCP连接时才可以发送数据
************************************************************/
uint8_t tcp_server_get_link_status(void)
{
	return lwipdev.tcp_server_link;
}



