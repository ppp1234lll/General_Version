/*******************************************************************************
*
* @File name	: 
* @Description	: 
* @Author		: 
*	Version	Date		Modification Description
*	1.0		
*
*******************************************************************************/
//#include "includes.h"
#include "lwip_comm.h"
#include "netif/etharp.h"
#include "ethernetif.h" 
#include "lwip_ping.h"
#include "tcp_client.h"
#include "dns.h"
#include "appconfig.h"
#include "bsp.h"
#include "udp_multicast.h"
#include "print.h"
//#include "tcp_server.h"
#include "onvif.h"
#include <stdint.h>
#include "onvif_tcp.h"

struct ip4_addr DNS_Addr;

uint8_t eth_carema_start = 0;

static void dns_serverFound(const char *name, const ip_addr_t *ipaddr, void *arg);
/************************************************************
*
* Function name	: eth_network_line_status_detection_function
* Description	: 网线状态检测函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void eth_network_line_status_detection_function(void)
{
	if(lwip_comm_init() != 0)
		printf("lwip_comm_init error\r\n");
	
	while(1)
	{
		eth_network_line_status_check();		 /* 网口检测函数 */
		eth_udp_connect_control_function();  // 组播UDP连接		
		eth_carema_search_function();  // 摄像机搜索、查询函数
		//        eth_onvif_connect_control_function();
    eth_snmp_connect_control_function();
        
		#ifdef WIRELESS_PRIORITY_CONNECTION
		if(app_get_network_mode() == 1) {
			eth_set_tcp_cmd(1);
		}
		#endif
		eth_ping_detection_function();		   /* PING协议 */		
		if( app_get_carema_search_mode() == 1 ) // 判断摄像机搜索协议
		{
			rtsp_thread_stop();
		}
		else if( app_get_carema_search_mode() == 2 )
		{
			rtsp_thread_stop();
			port_scan_detection_function();
		}	
		else if( app_get_carema_search_mode() == 3 )
		{
			rtsp_thread_start();
		}	

		
		if( app_get_network_mode() != 2) {	/* 网络状态检测 */
			/* 网络状态检测 */
			eth_tcp_connect_control_function();
		} else {
			if (g_lwipdev.tcp_status != LWIP_TCP_NO_CONNECT) {
				eth_set_tcp_connect_reset();
				vTaskDelay(100);
			}
			#ifdef WIRED_PRIORITY_CONNECTION
			gsm_set_tcp_cmd(1);	 // 启动无线tcp连接
			#endif
		}
		
		/* 主网络状态1、2检测 */
		if(g_lwipdev.netif_state == 1) 
		{
			if( det_get_main_network_status() == 1 ||det_get_main_network_sub_status() == 1) 
				led_out_control_function(LD_LAN_O,LD_FLICKER);
			else 
				led_out_control_function(LD_LAN_O,LD_ON);
		} 
		else
			led_out_control_function(LD_LAN_O,LD_OFF);
	
		/* 更新检测 */
		if( g_lwipdev.netif_state == 1) {
			if( update_get_mode_function() == UPDATE_MODE_LWIP ) {
				/* 断开连接 */
				if (g_lwipdev.tcp_status != LWIP_TCP_NO_CONNECT) {
					eth_set_tcp_connect_reset();
					vTaskDelay(200);
				}
				/* 更新 */
				update_lwip_task_function();
			}
		}
		vTaskDelay(20);  // 延时20ms
	}
}

/************************************************************
*
* Function name	: eth_network_line_status_check
* Description	: 网口状态检测
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t eth_network_line_status_check(void)
{
	
				 int8_t  res			   		 = 0;
  static uint8_t count           = 0;
	static uint8_t eth_status_flag = 0;
	
	if((g_lwipdev.link_status == LWIP_LINK_ON) && (g_lwipdev.netif_state == 0))	// 网线连上 
	{
		lwip_start_function();
		eth_status_flag = 0;
	}
	if((g_lwipdev.link_status == LWIP_LINK_OFF)&& eth_status_flag == 0)		
	{
		eth_set_tcp_connect_reset();
		vTaskDelay(100);
		
		eth_status_flag = 1; // 只进入一次
		led_out_control_function(LD_LAN_O,LD_OFF);
		led_control_function(LD_LAN,LD_OFF);
		
			
#ifdef WIRED_PRIORITY_CONNECTION
		gsm_set_tcp_cmd(1);	 // 检测到网口无网线	
#endif
		/* 重启一次网口 */
//		if(g_lwipdev.init == 1) { 
//				eth_reset_function();
//		}
  }
	
	/* 网络连接状态检测 */
	if(g_lwipdev.link_status == LWIP_LINK_OFF)
	{
		if((++count) >= 100) // 50*20ms
		{ 
			count = 0;
			g_lwipdev.reset = 1; // 定时重启网络
		}
	} 
	else 
	{
    count = 0;
	}
    
	/* 网络重启 */
	if(g_lwipdev.reset == 1) 
	{
		g_lwipdev.reset = 0;	
		vTaskDelay(1000);  // 保证重启前数据及时发出		
		if (g_lwipdev.tcp_status != LWIP_TCP_NO_CONNECT) 
		{
			eth_set_tcp_connect_reset();
		}
		
		if (g_lwipdev.udp_multicast_status != LWIP_UDP_NO_CONNECT)   // 重启UDP连接
		{
			eth_set_udp_connect_reset();
		}
		
		if (g_lwipdev.udp_status != LWIP_UDP_NO_CONNECT)   // 重启ONVIF连接
		{
			eth_set_onvif_udp_connect_reset();
		}
		
		if (g_lwipdev.onvif_tcp_status != LWIP_TCP_NO_CONNECT)   // 重启ONVIF连接
		{
			eth_set_onvif_tcp_connect_reset();
		}
		
		if (g_lwipdev.snmp_status != LWIP_UDP_NO_CONNECT)   // 重启UDP连接
		{
			eth_snmp_connect_reset();
		}
		
		lwip_stop_function();
		g_lwipdev.netif_state = 0;
//		eth_reset_function();
		eth_status_flag = 0;
  }
	return res;
//		int8_t  res			   		 = 0;
//	static uint8_t count           = 0;
//	static uint8_t eth_now_status  = 0;
//	static uint8_t eth_status_flag = 0;
//	
//	/* 检测网线是否连接 */
//	eth_now_status = LAN8720_Get_Network_Cable_Status();
//	
//	/* 检测到网线变化或网络还未连接 */
//	if(eth_now_status != 0 && (g_lwipdev.netif_state == 0))	// 网线连上
//	{
//		/* 有网线接入且网络还未创建 */
//		lwip_start_function();
//		eth_status_flag = 0; // 清空
//	}
//	
//	if(eth_now_status == 0 && eth_status_flag == 0)			// 网线断开
//	{
//		eth_set_tcp_connect_reset();
//		OSTimeDlyHMSM(0,0,0,100);
//		
//		eth_status_flag = 1; // 只进入一次
//		led_out_control_function(LD_LAN_O,LD_OFF);
//		led_control_function(LD_LAN,LD_OFF);
//		
//		lwip_stop_function();		
//		#ifdef WIRED_PRIORITY_CONNECTION
//		gsm_set_tcp_cmd(1);	 // 检测到网口无网线	
//		#endif
//		/* 重启一次网口 */
//		if(g_lwipdev.init == 1) { 
//				eth_reset_function();
//		}
//  }
//	
//    /* 网络连接状态检测 */
//    if (eth_now_status == 0) 
//		{
//			if((++count) >= 50) // 50*20ms
//			{ 
//				count = 0;
//				g_lwipdev.reset = 1; // 定时重启网络
//			}
//    } 
//		else 
//		{
//        count = 0;
//    }
//    
//	/* 网络重启 */
//	if(g_lwipdev.reset == 1) 
//	{
//		g_lwipdev.reset = 0;	
//		OSTimeDlyHMSM(0,0,1,0);  // 保证重启前数据及时发出		
//		if (g_lwipdev.tcp_status != LWIP_TCP_NO_CONNECT) 
//		{
//			eth_set_tcp_connect_reset();
//		}
//		if (g_lwipdev.udp_multicast_status != LWIP_UDP_NO_CONNECT)   // 重启UDP连接
//		{
//			eth_set_udp_connect_reset();
//		}
//		if (g_lwipdev.udp_status != LWIP_UDP_NO_CONNECT)   // 重启ONVIF连接
//		{
//			eth_set_onvif_udp_connect_reset();
//		}
//		
//		/* 关闭网口 */
//		lwip_stop_function();
//		/* 复位网口 */
//		g_lwipdev.netif_state = 0;

//		eth_reset_function();
//		eth_status_flag = 0;
//  }
//	
//	return res;
}
   

struct eth_ping
{
	uint8_t  cnt;
	uint32_t count;
	uint8_t  ping_next;
	uint8_t  dev_next;
};

struct eth_ping sg_ethping_t = {0};

/************************************************************
*
* Function name	: eth_ping_timer_function
* Description	: ping相关函数- timer
* Parameter		: 
* Return		: 
*	
************************************************************/
void eth_ping_timer_function(void)
{
	if(sg_ethping_t.ping_next == 0)
	{
		sg_ethping_t.count++;
		if(sg_ethping_t.count > app_get_next_ping_time())
		{
			/* 开始一次ping */
			sg_ethping_t.cnt = 0;
			sg_ethping_t.ping_next = 1;
			sg_ethping_t.dev_next  = 1;
		}
	}
	else
	{
		if(sg_ethping_t.dev_next == 0)
		{
			sg_ethping_t.count++;
			if(sg_ethping_t.count > app_get_next_dev_ping_time())
			{
				sg_ethping_t.dev_next = 1;
			}
		}
	}
}
#define PING_IP_MAX_NUM 12

/************************************************************
*
* Function name	: eth_ping_detection_function
* Description	: ping检测函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void eth_ping_detection_function(void)
{
	static  uint8_t ping_cmd = 0;
  static  uint8_t ping_dev_num = 0;
	static uint8_t  ip[4] 	 = {0};
	int8_t  		ret   	 = 0;

	uint16_t times = 0;
	uint8_t delay_time = app_get_network_delay_time(); // 网络延时时间  20220308
	
	if( app_get_carema_search_mode() == 1 ) // 判断摄像机搜索协议
	   ping_dev_num = PING_IP_MAX_NUM;
	else
		ping_dev_num = 2;
	
	/* 检测是否可以开始一轮ping */
	if(sg_ethping_t.ping_next == 0 || sg_ethping_t.dev_next == 0)
	{
		return;
	}

	if ( ping_cmd == 0) {
		switch(sg_ethping_t.cnt)
		{
			case 0: 					// 主机
				app_get_main_network_ping_ip_addr(ip);
				if(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0)			/* 未主机ping地址 */
				{
					det_set_main_network_status(0);
					sg_ethping_t.cnt++;
					if(sg_ethping_t.cnt >= ping_dev_num)
					{
						det_set_ping_status(1);
						sg_ethping_t.cnt = 0;
						/* 开始新的一轮计时 */
						sg_ethping_t.ping_next = 0;
					}
					ping_cmd = 0;
				}
				else
				{
					lwip_ping_clear();					/* 检测到主机ip，开始ping */
					ping_cmd = 1;
				}
				break;
			case 1:
				app_get_main_network_sub_ping_ip_addr(ip);
				if(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0)					/* 未主机ping地址 */
				{
					det_set_main_network_sub_status(0);
					sg_ethping_t.cnt++;
					if(sg_ethping_t.cnt >= ping_dev_num)
					{
						det_set_ping_status(1);
						sg_ethping_t.cnt = 0;						/* 开始新的一轮计时 */
						sg_ethping_t.ping_next = 0;
					}
					ping_cmd = 0;
				}
				else					/* 检测到主机ip，开始ping */
				{
					lwip_ping_clear();
					ping_cmd = 1;
				}
				break;
			default: 					// 摄像头
				ret = app_get_camera_function(ip,sg_ethping_t.cnt-2);
				if(ret <0)					/* 未检测摄像头ip，直接标记 */
				{
					det_set_camera_status(sg_ethping_t.cnt-2,0);
					sg_ethping_t.cnt++;
					if(sg_ethping_t.cnt >= ping_dev_num)
					{
						det_set_ping_status(1);
						sg_ethping_t.cnt = 0;
						sg_ethping_t.ping_next = 0;						/* 开始新的一轮计时 */
					}
					ping_cmd = 0;
				}
				else					/* 检测到摄像头ip，开始ping */
				{
					lwip_ping_clear();
					ping_cmd = 1;
				}
				break;
		}
	}
	if(ping_cmd == 1)
	{
		ret = lwip_ping_the_specified_ip_function(ip);
		if(ret != LWIP_PING_WAIT)	// 获取到结果
		{
			ping_cmd = 0;			// 结束一次ping
			times = get_lwip_ping_times();
			if(ret == LWIP_PING_SUCCESS)
			{
				switch(sg_ethping_t.cnt)
				{
					case 0:
						if(times <= delay_time)               // 网络延时时间  20220308
							det_set_main_network_status(1);
						else
							det_set_main_network_status(2);
						break;
					case 1:
						if(times <= delay_time) 
							det_set_main_network_sub_status(1);							
						else
							det_set_main_network_sub_status(2);
						break;
					default:
						if(times <= delay_time) 
							det_set_camera_status(sg_ethping_t.cnt-2,1);
						else
							det_set_camera_status(sg_ethping_t.cnt-2,2);
						break;

				}
			}
			else
			{
				switch(sg_ethping_t.cnt)
				{
					case 0:
						det_set_main_network_status(0);
						break;
					case 1:
						det_set_main_network_sub_status(0);
						break;
					default:
						det_set_camera_status(sg_ethping_t.cnt-2,0);
						break;
				}
			}
			/* 开始下一个设备的ping计时 */
			sg_ethping_t.dev_next = 0;
			sg_ethping_t.cnt++;
			if(sg_ethping_t.cnt >= ping_dev_num)
			{
				det_set_ping_status(1);
				sg_ethping_t.cnt = 0;				/* 开始新的一轮计时 */
				sg_ethping_t.ping_next = 0;
			}
		}
	}
}

/************************************************************
*
* Function name	: dns_serverFound
* Description	: dns回调函数
* Parameter		: 
* Return		: 
*	
************************************************************/
//void dns_serverFound(const char *name, struct ip_addr_t *ipaddr, void *arg)
//{
//	uint32_t ip=0;
//	
//	if ((ipaddr) && (ipaddr->addr))
//	{
//		ip = ipaddr->addr;
//		if(ip != 0)
//		{
//			g_lwipdev.remoteip[3] = (ip>>24)&0xff;
//			g_lwipdev.remoteip[2] = (ip>>16)&0xff;
//			g_lwipdev.remoteip[1] = (ip>>8)&0xff;
//			g_lwipdev.remoteip[0] = (ip>>0)&0xff;
//			g_lwipdev.domename = 1;
//		}
//	}
//	else
//	{
//	}

//}

static void dns_serverFound(const char *name, const ip_addr_t *ipaddr, void *arg)
{
	uint32_t ip=0;
	
	if ((ipaddr) && (ipaddr->addr))
	{
		ip = ipaddr->addr;
		if(ip != 0)
		{
			g_lwipdev.remoteip[3] = (ip>>24)&0xff;
			g_lwipdev.remoteip[2] = (ip>>16)&0xff;
			g_lwipdev.remoteip[1] = (ip>>8)&0xff;
			g_lwipdev.remoteip[0] = (ip>>0)&0xff;
			g_lwipdev.domename = 1;
		}
	}
	else
	{
	}

}

/************************************************************
*
* Function name	: eth_tcp_connect_control_function
* Description	: tcp连接控制函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void eth_tcp_connect_control_function(void)
{
	struct remote_ip *remote = app_get_remote_network_function();
	
	/* 检测网口状态 */
	if(g_lwipdev.netif_state == 0)
	{
		return;
	}
	#ifdef WIRELESS_PRIORITY_CONNECTION
	/* 是否允许连接网络 */
	if(g_lwipdev.tcp_cmd == 0) {
		if (g_lwipdev.tcp_status != LWIP_TCP_NO_CONNECT) {
			eth_set_tcp_connect_reset();
			vTaskDelay(100);
		}
		return;
	}
	#endif
	
	/* 通过dns获取域名对应的IP */
	/* 测试域名：abc.fnwlw.net */
	if(g_lwipdev.netif_state == 1 && g_lwipdev.domename == 0 && g_lwipdev.iporname == 1)
	{
		IP4_ADDR(&DNS_Addr, g_lwipdev.dns[0],g_lwipdev.dns[1], g_lwipdev.dns[2],g_lwipdev.dns[3]);
		dns_gethostbyname((char *)remote->inside_iporname,&DNS_Addr,dns_serverFound,NULL);
	}	
	
	if(g_lwipdev.tcp_status == LWIP_TCP_NO_CONNECT) // 检测到tcp还未连接
	{
		if(g_lwipdev.iporname == 0)				  // 直接使用IP
		{
			/* 更新远端地址 */
			lwip_updata_remote_network_infor(&g_lwipdev);
			tcp_client_start_function();
			g_lwipdev.tcp_status = LWIP_TCP_INIT_CONNECT;
		}
		else									  // 需要通过域名获取IP
		{
			if(g_lwipdev.domename == 1)
			{
				/* 更新远端地址 */
				lwip_updata_remote_network_infor(&g_lwipdev);
				tcp_client_start_function();
				g_lwipdev.tcp_status = LWIP_TCP_INIT_CONNECT;
			}
		}
		
	}
}

/************************************************************
*
* Function name	: eth_udp_connect_control_function
* Description	: 组播udp连接控制函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void eth_udp_connect_control_function(void)
{
	if(g_lwipdev.netif_state == 0)	/* 检测网口状态 */
	{
		return;
	}

	if(g_lwipdev.udp_multicast_status == LWIP_UDP_NO_CONNECT) // 检测到UDP还未连接
	{
		udp_multicast_start_function();  // 创建UDP服务器
		g_lwipdev.udp_multicast_status = LWIP_UDP_INIT_CONNECT;
	}
	
}
/************************************************************
*
* Function name	: eth_carema_search_function
* Description	: 摄像机搜索、查询函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t eth_carema_search_function(void)
{
	static uint32_t carema_start_time = 0;
	static uint32_t carema_osd_time = 0;
	    static uint8_t  onvif_create_flag = 0;

	IPC_Info_t *ipc_t = onvif_get_ipc_param();
	/* 摄像机搜索、查询函数 */
	if(g_lwipdev.netif_state == 0)	/* 检测网口状态 */
	{
		eth_carema_start = ETH_ONVIF_INIT;
		return -1;
	}
	switch(eth_carema_start)
	{
		case ETH_ONVIF_START:
			if(carema_start_time == 0)
				carema_start_time = HAL_GetTick();
			
			            if(onvif_create_flag == 0)
            {
                onvif_create_flag = 1;
                if(g_lwipdev.udp_status == LWIP_UDP_INIT_CONNECT)
                    g_lwipdev.udp_status = LWIP_UDP_NO_CONNECT;
            } 
			
			if(g_lwipdev.udp_status == LWIP_UDP_NO_CONNECT) // 检测到UDP还未连接
			{
				onvif_udp_start();  // 创建UDP函数
				g_lwipdev.udp_status = LWIP_UDP_INIT_CONNECT;
			}

			if(HAL_GetTick() - carema_start_time >= 5000) // 5s后没有搜索到摄像机
			{
				eth_set_onvif_udp_connect_reset();
				eth_carema_start = ETH_ONVIF_OSD;
				carema_start_time = 0;
				return -2;
			}
			else 
			{
				if((ipc_t->search_flag & ONVIF_END) == ONVIF_END) // 搜索结束
				{
					eth_set_onvif_udp_connect_reset();
					eth_carema_start = ETH_ONVIF_OSD;
					carema_osd_time = 0;
					carema_start_time = 0;
				}			
			}
			break;

		case ETH_ONVIF_OSD: // 查询OSD标注
			if(ipc_t->ipc_num == 0)
			{
				carema_osd_time = 0;
				carema_start_time = 0;
				eth_carema_start = ETH_ONVIF_END;
				return -3;
			}
			
			if(carema_osd_time == 0)
				carema_osd_time = HAL_GetTick();
			
			if(g_lwipdev.onvif_tcp_status == LWIP_TCP_NO_CONNECT) // 检测到TCP还未连接
			{
				onvif_tcp_client_init();  // 创建TCP客户端
				g_lwipdev.onvif_tcp_status = LWIP_TCP_INIT_CONNECT;
			}

			if(HAL_GetTick() - carema_osd_time >= 5000) // 5s后没有搜索到摄像机
			{
				eth_set_onvif_tcp_connect_reset();
				eth_carema_start = ETH_ONVIF_END;
				carema_osd_time = 0;
				carema_start_time = 0;
				return -2;
			}
			else
			{
				if(onvif_tcp_get_ipc_read_status() == 2) // 搜索结束
				{
					eth_set_onvif_tcp_connect_reset();
					eth_carema_start = ETH_ONVIF_END;
					carema_osd_time = 0;
					carema_start_time = 0;
				}				
			}

			break;
		default:
			break;
	}
	return 0;
		
}
/************************************************************
*
* Function name	: eth_set_udp_connect_reset
* Description	: 重启组播
* Parameter		: 
* Return		: 
*	
************************************************************/
void eth_set_udp_connect_reset(void)
{
	g_lwipdev.udp_multicast_reset = 1;
}

/************************************************************
*
* Function name	: eth_set_onvif_udp_connect_reset
* Description	: 重启ONVIF组播
* Parameter		: 
* Return		: 
*	   20230810
************************************************************/
void eth_set_onvif_udp_connect_reset(void)
{
	g_lwipdev.udp_reset = 1;
}

/************************************************************
*
* Function name	: eth_set_onvif_tcp_connect_reset
* Description	: 重启ONVIF TCP连接
* Parameter		: 
* Return		: 
*	   20230810
************************************************************/
void eth_set_onvif_tcp_connect_reset(void)
{
	g_lwipdev.onvif_tcp_reset = 1;
}

/************************************************************
*
* Function name	: eth_set_tcp_connect_reset
* Description	: 重启TCP连接
* Parameter		: 
* Return		: 
*	
************************************************************/
void eth_set_tcp_connect_reset(void)
{
	app_set_send_result_function(SR_OK);
	
	lwip_updata_remote_network_infor(&g_lwipdev);
	
	g_lwipdev.tcp_reset = 1;
}

#ifdef WIRELESS_PRIORITY_CONNECTION
/************************************************************
*
* Function name	: eth_set_tcp_cmd
* Description	: 有线网络使能
* Parameter		: 
*	@cmd		: 0-不运行通信 1-允许通信
* Return		: 
*	
************************************************************/
void eth_set_tcp_cmd(uint8_t cmd)
{
	g_lwipdev.tcp_cmd = cmd;
}
#endif

/************************************************************
*
* Function name	: eth_set_network_reset
* Description	: 重启网络
* Parameter		: 
* Return		: 
*	
************************************************************/
void eth_set_network_reset(void)
{
	app_set_send_result_function(SR_OK);
//	g_lwipdev.domename = 0; // 重新获取IP
	g_lwipdev.reset = 1;
}

/************************************************************
*
* Function name	: eth_get_network_cable_status
* Description	: 获取网线状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t eth_get_network_cable_status(void)
{
	return g_lwipdev.netif_state;
}

/************************************************************
*
* Function name	: eth_get_tcp_status
* Description	: 获取tcp状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t eth_get_tcp_status(void)
{
	return g_lwipdev.tcp_status;
}
/************************************************************
*
* Function name	: eth_get_udp_status
* Description	: 获取udp状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t eth_get_udp_status(void)
{
	return g_lwipdev.udp_multicast_status;
}


/************************************************************
*
* Function name	: eth_set_udp_onvif_flag
* Description	: 设置ONVIF状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void eth_set_onvif_flag(uint8_t data)
{
	eth_carema_start = data;
}

uint8_t eth_get_onvif_flag(void)
{
	return eth_carema_start;
}

/************************************************************
*
* Function name	: eth_save_ipc_info
* Description	: 保存设备信息
* Parameter		: 
* Return		: 
*	
************************************************************/

// 保存 设备 IP 及 编号 
void eth_save_ipc_info(char *ip,uint8_t id)  // 保存设备信息
{
	uint8_t addr[4] = {0};

	sscanf(ip, "%d.%d.%d.%d",(int*)&addr[0],(int*)&addr[1],(int*)&addr[2],(int*)&addr[3]);

	app_set_camera_num_function(addr, id);
}
/*
*********************************************************************************************************
*	函 数 名: eth_snmp_connect_control_function
*	功能说明: SNMP连接控制函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void eth_snmp_connect_control_function(void)
{
	if(g_lwipdev.netif_state == 0)	/* 检测网口状态 */
	{
		return;
	}

	if(g_lwipdev.snmp_status == LWIP_UDP_NO_CONNECT) // 检测到UDP还未连接
	{
		snmp_start_function();  // 创建UDP服务器
		g_lwipdev.snmp_status = LWIP_UDP_INIT_CONNECT;
	}
}

/*
*********************************************************************************************************
*	函 数 名: eth_snmp_connect_reset
*	功能说明: 重启SNMP
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void eth_snmp_connect_reset(void)
{
	g_lwipdev.snmp_reset = 1;
}


/*
*********************************************************************************************************
*	函 数 名: eth_onvif_connect_control_function
*	功能说明: ONVIF连接控制函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void eth_onvif_connect_control_function(void)
{
	if(g_lwipdev.netif_state == 0)	/* 检测网口状态 */
	{
		return;
	}

    if(g_lwipdev.udp_status == LWIP_UDP_NO_CONNECT) // 检测到UDP还未连接
    {
        onvif_udp_start();  // 创建UDP函数
        g_lwipdev.udp_status = LWIP_UDP_INIT_CONNECT;
    }
}



