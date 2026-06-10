/********************************************************************************
*
* @File name  ：udp.c
* @Description：udp操作
* @Author     ：编号9527
* Version Date       Modification Description
* 1.0     2019-08-16 1.udp初始化并接收广播包
*
********************************************************************************/
#include "udp_multicast.h"
//#include "includes.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/igmp.h"
#include "lwip/opt.h"
#include <lwip/sockets.h>
#include "lwip_comm.h"
#include "./MALLOC/malloc.h"
#include "app.h"
#include "eth.h"
#include "appconfig.h"
#include "print.h"


// UDP 参数设置
#define UDP_MULTICAST_PRIO		3
//任务堆栈大小
#define UDP_MULTICAST_STK_SIZE	256
//任务堆栈
TaskHandle_t UdpMulticast_Task_Handler;

udp_multicast_t udp_multicast_param;

#define UDP_RX_LEN		500	 					// udp最大接收数据长度
uint8_t udp_recv_buf[UDP_RX_LEN];		// UDP接收
uint8_t *udp_send_buf = NULL;				// UDP发送
uint16_t udp_flag;

int udp_sock = -1;  // SOCKET

//udp任务函数
static void udp_multicast_thread(void *arg)
{
	int ret = 0;
	int len	=	0;
	struct sockaddr_in local_addr;
	struct sockaddr_in recv_addr;
	int 	 sender_len = sizeof(recv_addr);
	struct ip_mreq mreq; 			// 多播地址结构体
	struct timeval tv_out;	
	struct local_ip_t   	*local = app_get_local_network_function();
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

				ret = bind(udp_sock, (struct sockaddr*)&local_addr, sizeof(local_addr));		// 将 Socket 与本地某网络接口绑定 
				if(ret != 0 )				
				{	}
				else
				{				
					mreq.imr_multiaddr.s_addr	=	inet_addr(multicast_addr); /*<! 多播组 IP 地址设置 */
					mreq.imr_interface.s_addr = inet_addr(ip_param); /*<! 待加入多播组的 IP 地址 */   

					// 添加多播组成员（该语句之前，socket 只与 某单播IP地址相关联 执行该语句后 将与多播地址相关联）
					ret = setsockopt(udp_sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq));

					tv_out.tv_sec = 10;
					tv_out.tv_usec = 0;
					setsockopt(udp_sock, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out));			//recv延时时间设置
					
					g_lwipdev.udp_multicast_status = LWIP_UDP_CONNECT;
				}						
			}	
		}
		else if(g_lwipdev.udp_multicast_status == LWIP_UDP_CONNECT) // 连接成功
		{				
//			udp_socket_send_api(udp_sock,udp_send_buf,len,multicast_addr,multicast_port);		
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
		if(g_lwipdev.udp_multicast_reset == 1)					  // 重启UDP连接
		{
			g_lwipdev.udp_multicast_reset = 0;
			udp_multicast_stop_function();
		}
		FeedFwdgt();
		vTaskDelay(50);  //延时100		
	}			
}



//创建UDP线程
//返回值:0 UDP创建成功
//		其他 UDP创建失败
unsigned char udp_multicast_init(void)
{
//	unsigned char res,err;
////	res = OSTaskCreate(udp_multicast_thread,(void*)0,(OS_STK*)&UDP_MULTICAST_TASK_STK[UDP_MULTICAST_STK_SIZE-1],UDP_MULTICAST_PRIO); //创建UDP线程
//	
//	res = OSTaskCreateExt(udp_multicast_thread, 															//建立扩展任务(任务代码指针) 
//										(void *)0,																					//传递参数指针 
//										(OS_STK*)&UDP_MULTICAST_TASK_STK[UDP_MULTICAST_STK_SIZE-1], 					//分配任务堆栈栈顶指针 
//										(INT8U)UDP_MULTICAST_PRIO, 															//分配任务优先级 
//										(INT16U)UDP_MULTICAST_PRIO,															//(未来的)优先级标识(与优先级相同) 
//										(OS_STK *)&UDP_MULTICAST_TASK_STK[0], 											//分配任务堆栈栈底指针 
//										(INT32U)UDP_MULTICAST_STK_SIZE, 															//指定堆栈的容量(检验用) 
//										(void *)0,																					//指向用户附加的数据域的指针 
//										(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 	
//	
//	OSTaskNameSet(UDP_MULTICAST_PRIO, (INT8U *)(void *)"udp_multicast", &err);
//	
//	OS_EXIT_CRITICAL();		//开中断
//	
//	return res;
										
										
	BaseType_t res;
	
	taskENTER_CRITICAL();	/*进入临界区*/
	
	xTaskCreate((TaskFunction_t )udp_multicast_thread,
							(const char *   )"udp_multicast_thread",
							(uint16_t       )UDP_MULTICAST_STK_SIZE,
							(void *         )NULL,
							(UBaseType_t    )UDP_MULTICAST_PRIO,
							(TaskHandle_t * )&UdpMulticast_Task_Handler);

	taskEXIT_CRITICAL();	/*退出临界区*/
	
	return res;
}
/************************************************************
*
* Function name	: udp_multicast_start_function
* Description	: udp服务器启动函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void udp_multicast_start_function(void)
{
	/* 创建TCP客户端 */
	g_lwipdev.udp_multicast_reset = 0;
	udp_multicast_init();
	
}

/************************************************************
*
* Function name	: udp_multicast_stop_function
* Description	: udp停止函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void udp_multicast_stop_function(void)
{
//	lwipdev.udp_multicast_reset = 0;
//	
//	OS_CPU_SR cpu_sr;
//	
//	if(lwipdev.udp_multicast_status == LWIP_UDP_CONNECT)
//	{
//		closesocket(udp_sock);
//	}

//	lwipdev.udp_multicast_status = LWIP_UDP_NO_CONNECT;
//	
//	OS_ENTER_CRITICAL();		// 关中断
//	OSTaskDel(UDP_MULTICAST_PRIO);	// 删除UDP任务
//	OS_EXIT_CRITICAL();		// 开中断
	
	g_lwipdev.udp_multicast_reset = 0;
	
	/* 关闭tcp服务器 */
	if(g_lwipdev.udp_multicast_status == LWIP_UDP_CONNECT)
	{
		closesocket(udp_sock);
	}
	
	g_lwipdev.udp_multicast_status = LWIP_UDP_NO_CONNECT;
	
	taskENTER_CRITICAL();	/*进入临界区*/
	vTaskDelete(UdpMulticast_Task_Handler);
	taskEXIT_CRITICAL();	/*退出临界区*/
}


/************************************************************
*
* Function name	: udp_socket_send_api
* Description	: udp发送函数
* Parameter		: 
* Return		: 
*	
************************************************************/
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

/************************************************************
*
* Function name	: udp_multicast_send_buff
* Description	: 发送数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void udp_multicast_send_buff(uint8_t *buff, uint16_t len)
{
	udp_send_buf = buff;
	udp_flag     = len | UDP_MULTICAST_DATA;
}
