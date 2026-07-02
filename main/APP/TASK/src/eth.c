/*
*********************************************************************************************************
*    函 数 名: 
*    功能说明: 
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
#include "./Task/inc/eth.h"
#include "main.h"
#include "./ping/lwip_ping_remote.h"

struct ip4_addr DNS_Addr;

uint8_t eth_carema_start = 0;
eth_ping_t sg_ping_info_t = {0};

static void dns_serverFound(const char *name, const ip_addr_t *ipaddr, void *arg);

typedef enum
{
    ETH_CONNECT_TCP = 0,
    ETH_CONNECT_UDP_MULTICAST,
    ETH_CONNECT_ONVIF_UDP,
    ETH_CONNECT_ONVIF_TCP,
    ETH_CONNECT_SNMP,
} eth_connect_t;

static void eth_connect_control_function(eth_connect_t connect);
static void eth_connect_reset(eth_connect_t connect);
/*
*********************************************************************************************************
*    函 数 名: eth_network_line_status_detection_function
*    功能说明: 网线状态检测函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void eth_task_function(void)
{
    while(1)
    {
        eth_network_reset_function();     // 网络复位函数
        eth_connect_control_function(ETH_CONNECT_UDP_MULTICAST); // 组播UDP连接  
        eth_connect_control_function(ETH_CONNECT_SNMP); // SNMP连接  
        eth_carema_search_function();  // 摄像机搜索、查询函数
        
        #ifdef WIRELESS_PRIORITY_CONNECTION
        if(app_get_network_mode() == SERVER_MODE_LWIP) {
            eth_set_tcp_cmd(1);
        }
        #endif

        lwip_ping_multi_poll();                  /* PING协议 */
        eth_ping_remote_ip_function();           /* 远程指定IP ping */
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
        
        if( app_get_network_mode() != SERVER_MODE_GPRS) 
        { 
            eth_tcp_connect_control_function();
        } 
        else 
        {
            if (g_lwipdev.tcp_status != LWIP_TCP_NO_CONNECT) {
                eth_connect_reset(ETH_CONNECT_TCP);
                vTaskDelay(100);
            }
            #ifdef WIRED_PRIORITY_CONNECTION
            gsm_set_tcp_cmd(1);     // 启动无线tcp连接
            #endif
        }
        
        /* 外部指示灯 */
        if(g_lwipdev.netif_state == 1) 
        {
            if( det_get_main_network_status() == 1 ||det_get_main_network_sub_status() == 1) 
                led_control_function(LD_LAN_EXT,LD_FLICKER);
        }
            
        /* OTA / 日志上传: 由后台任务执行,不阻塞 eth 主循环 */
        if( g_lwipdev.netif_state == 1) 
        {
            if( update_get_mode_function() == UPDATE_MODE_LWIP ) 
            {
                update_lwip_poll();
            }
            else if( upload_get_mode_function() == UPLOAD_MODE_LWIP )
            {
                upload_lwip_poll();
            }
        }
        vTaskDelay(20);  // 延时20ms
    }
}

/*
*********************************************************************************************************
*    函 数 名: eth_network_reset_function
*    功能说明: 网络复位函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void eth_network_reset_function(void)
{
    /* 网络重启 */
    if(g_lwipdev.reset == 1) 
    {
        g_lwipdev.reset = 0;    
        vTaskDelay(1000);  // 保证重启前数据及时发出 
        
#ifdef WIRED_PRIORITY_CONNECTION
        gsm_set_tcp_cmd(1);     // 检测到网口无网线    
#endif 
        if (g_lwipdev.tcp_status != LWIP_TCP_NO_CONNECT) 
        {
            eth_connect_reset(ETH_CONNECT_TCP);
        }
        
        if (g_lwipdev.udp_multicast_status != LWIP_UDP_NO_CONNECT)   // 重启UDP连接
        {
            eth_connect_reset(ETH_CONNECT_UDP_MULTICAST);
        }
        
        if (g_lwipdev.udp_status != LWIP_UDP_NO_CONNECT)   // 重启ONVIF连接
        {
            eth_connect_reset(ETH_CONNECT_ONVIF_UDP);
        }
        
        if (g_lwipdev.onvif_tcp_status != LWIP_TCP_NO_CONNECT)   // 重启ONVIF连接
        {
            eth_connect_reset(ETH_CONNECT_ONVIF_TCP);
        }
        
        if (g_lwipdev.snmp_status != LWIP_UDP_NO_CONNECT)   // 重启UDP连接
        {
            eth_connect_reset(ETH_CONNECT_SNMP);
        }

        lwip_stop_function();
        vTaskDelay(100);
        /* 网线仍连接时主动恢复网口,避免仅依赖 link 线程导致长时间不可达 */
        if (g_lwipdev.link_status == LWIP_LINK_ON)
        {
            lwip_start_function();
        }
    }
}


struct eth_ping
{
    uint8_t  cnt;
    uint32_t count;
    uint8_t  ping_next;
    uint8_t  dev_next;
};

struct eth_ping sg_ethping_t = {0};

/*
*********************************************************************************************************
*    函 数 名: eth_ping_timer_function
*    功能说明: ping相关函数- timer
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
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

/*
*********************************************************************************************************
*    函 数 名: eth_ping_detection_function
*    功能说明: ping检测函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void eth_ping_detection_function(void)
{
    static  uint8_t ping_cmd = 0;
    static  uint8_t ping_dev_num = 0;
    static uint8_t  ip[4]      = {0};
    int8_t          ret        = 0;

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

    if ( ping_cmd == 0) 
    {
        switch(sg_ethping_t.cnt)
        {
            case 0:                     // 主机
                app_get_main_network_ping_ip_addr(ip);
                if(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0)            /* 未主机ping地址 */
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
                    lwip_ping_clear();                    /* 检测到主机ip，开始ping */
                    ping_cmd = 1;
                }
                break;
            case 1:
                app_get_main_network_sub_ping_ip_addr(ip);
                if(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0)                    /* 未主机ping地址 */
                {
                    det_set_main_network_sub_status(0);
                    sg_ethping_t.cnt++;
                    if(sg_ethping_t.cnt >= ping_dev_num)
                    {
                        det_set_ping_status(1);
                        sg_ethping_t.cnt = 0;                        /* 开始新的一轮计时 */
                        sg_ethping_t.ping_next = 0;
                    }
                    ping_cmd = 0;
                }
                else                    /* 检测到主机ip，开始ping */
                {
                    lwip_ping_clear();
                    ping_cmd = 1;
                }
                break;
            default:                     // 摄像头
                ret = app_get_camera_function(ip,sg_ethping_t.cnt-2);
                if(ret <0)                    /* 未检测摄像头ip，直接标记 */
                {
                    det_set_camera_status(sg_ethping_t.cnt-2,0);
                    sg_ethping_t.cnt++;
                    if(sg_ethping_t.cnt >= ping_dev_num)
                    {
                        det_set_ping_status(1);
                        sg_ethping_t.cnt = 0;
                        sg_ethping_t.ping_next = 0;                        /* 开始新的一轮计时 */
                    }
                    ping_cmd = 0;
                }
                else                    /* 检测到摄像头ip，开始ping */
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
        if(ret != LWIP_PING_WAIT)    // 获取到结果
        {
            ping_cmd = 0;            // 结束一次ping
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
                sg_ethping_t.cnt = 0;                /* 开始新的一轮计时 */
                sg_ethping_t.ping_next = 0;
            }
        }
    }
}

/*
*********************************************************************************************************
*    函 数 名: dns_serverFound
*    功能说明: dns回调函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
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

/*
*********************************************************************************************************
*    函 数 名: eth_tcp_connect_control_function
*    功能说明: tcp连接控制函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
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
			vTaskDelay(pdMS_TO_TICKS(100));
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
/*
*********************************************************************************************************
*    函 数 名: eth_connect_control_function
*    功能说明: 连接控制函数
*    形    参: connect 连接类型
*    返 回 值: 无
*********************************************************************************************************
*/
static void eth_connect_control_function(eth_connect_t connect)
{
    if(g_lwipdev.netif_state == 0)    /* 检测网口状态 */
    {
        return;
    }

    switch(connect)
    {
        case ETH_CONNECT_UDP_MULTICAST:
            if(g_lwipdev.udp_multicast_status == LWIP_UDP_NO_CONNECT) // 检测到UDP还未连接
            {
                udp_multicast_start_function();  // 创建UDP服务器
                g_lwipdev.udp_multicast_status = LWIP_UDP_INIT_CONNECT;
            }
            break;

        case ETH_CONNECT_ONVIF_UDP:
            if(g_lwipdev.udp_status == LWIP_UDP_NO_CONNECT) // 检测到UDP还未连接
            {
                onvif_udp_start();  // 创建UDP函数
                g_lwipdev.udp_status = LWIP_UDP_INIT_CONNECT;
            }
            break;

        case ETH_CONNECT_ONVIF_TCP:
            if(g_lwipdev.onvif_tcp_status == LWIP_TCP_NO_CONNECT) // 检测到TCP还未连接
            {
                onvif_tcp_client_init();  // 创建TCP客户端
                g_lwipdev.onvif_tcp_status = LWIP_TCP_INIT_CONNECT;
            }
            break;

        case ETH_CONNECT_SNMP:
            if(g_lwipdev.snmp_status == LWIP_UDP_NO_CONNECT) // 检测到UDP还未连接
            {
                snmp_start_function();  // 创建UDP服务器
                g_lwipdev.snmp_status = LWIP_UDP_INIT_CONNECT;
            }
            break;

        default:
            break;
    }
}

/*
*********************************************************************************************************
*    函 数 名: eth_carema_search_function
*    功能说明: 摄像机搜索、查询函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t eth_carema_search_function(void)
{
    static uint32_t carema_start_time = 0;
    static uint32_t carema_osd_time = 0;
    static uint8_t  onvif_create_flag = 0;

    IPC_Info_t *ipc_t = onvif_get_ipc_param();
    /* 摄像机搜索、查询函数 */
    if(g_lwipdev.netif_state == 0)    /* 检测网口状态 */
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
            
            eth_connect_control_function(ETH_CONNECT_ONVIF_UDP);

            if(HAL_GetTick() - carema_start_time >= 5000) // 5s后没有搜索到摄像机
            {
                eth_connect_reset(ETH_CONNECT_ONVIF_UDP);
                eth_carema_start = ETH_ONVIF_OSD;
                carema_start_time = 0;
                return -2;
            }
            else 
            {
                if((ipc_t->search_flag & ONVIF_END) == ONVIF_END) // 搜索结束
                {
                    eth_connect_reset(ETH_CONNECT_ONVIF_UDP);
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
            
            eth_connect_control_function(ETH_CONNECT_ONVIF_TCP);

            if(HAL_GetTick() - carema_osd_time >= 5000) // 5s后没有搜索到摄像机
            {
                eth_connect_reset(ETH_CONNECT_ONVIF_TCP);
                eth_carema_start = ETH_ONVIF_END;
                carema_osd_time = 0;
                carema_start_time = 0;
                return -2;
            }
            else
            {
                if(onvif_tcp_get_ipc_read_status() == 2) // 搜索结束
                {
                    eth_connect_reset(ETH_CONNECT_ONVIF_TCP);
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
/*
*********************************************************************************************************
*    函 数 名: eth_ping_remote_ip_function
*    功能说明: 远程ping指定IP，结果写入sg_ping_info_t
*    形    参: 无
*    返 回 值: -1-无网络 0-进行中/空闲 1-完成
*********************************************************************************************************
*/
int8_t eth_ping_remote_ip_function(void)
{
    int8_t ret = 0;
    uint8_t i = 0;
    lwip_remote_ping_report_t report = {0};

    if(g_lwipdev.netif_state == 0)    /* 检测网口状态 */
    {
        sg_ping_info_t.start = 0;
        lwip_ping_remote_cancel();
        return -1;
    }

    if(sg_ping_info_t.start == 0)
    {
        lwip_ping_remote_cancel();
        return 0;
    }

    ret = lwip_ping_remote_task(sg_ping_info_t.ip, 1U, &report);
    if(ret == 0)
    {
        sg_ping_info_t.start = 0;
        return 0;
    }

    if(ret < 0)
    {
        sg_ping_info_t.start = 0;
        return -1;
    }

    for(i = 0; i < ETH_REMOTE_PING_NUM; i++)
    {
        sg_ping_info_t.status[i] = report.status[i];
        sg_ping_info_t.times[i] = report.times[i];
    }
    sg_ping_info_t.start = 0;
    return 1;
}

/*
*********************************************************************************************************
*    函 数 名: eth_connect_reset
*    功能说明: 重启连接
*    形    参: connect 连接类型
*    返 回 值: 无
*********************************************************************************************************
*/
static void eth_connect_reset(eth_connect_t connect)
{
    switch(connect)
    {
        case ETH_CONNECT_TCP:
            app_set_send_result_function(SR_OK);
            lwip_updata_remote_network_infor(&g_lwipdev);
            g_lwipdev.tcp_reset = 1;
            break;

        case ETH_CONNECT_UDP_MULTICAST:
            g_lwipdev.udp_multicast_reset = 1;
            break;

        case ETH_CONNECT_ONVIF_UDP:
            g_lwipdev.udp_reset = 1;
            break;

        case ETH_CONNECT_ONVIF_TCP:
            g_lwipdev.onvif_tcp_reset = 1;
            break;

        case ETH_CONNECT_SNMP:
            g_lwipdev.snmp_reset = 1;
            break;

        default:
            break;
    }
}
/*
*********************************************************************************************************
*    函 数 名: eth_set_tcp_connect_reset
*    功能说明: 重启 TCP 连接
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void eth_set_tcp_connect_reset(void)
{
    eth_connect_reset(ETH_CONNECT_TCP);
}

#ifdef WIRELESS_PRIORITY_CONNECTION
/*
*********************************************************************************************************
*    函 数 名: eth_set_tcp_cmd
*    功能说明: 有线网络使能
*    形    参: @cmd        : 0-不运行通信 1-允许通信
*    返 回 值: 
*********************************************************************************************************
*/
void eth_set_tcp_cmd(uint8_t cmd)
{
    g_lwipdev.tcp_cmd = cmd;
}
#endif

/*
*********************************************************************************************************
*    函 数 名: eth_set_network_reset
*    功能说明: 重启网络
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void eth_set_network_reset(void)
{
    g_lwipdev.reset = 1;
}

/*
*********************************************************************************************************
*    函 数 名: eth_get_network_cable_status
*    功能说明: 获取网线状态
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t eth_get_network_cable_status(void)
{
    return g_lwipdev.netif_state;
}

/*
*********************************************************************************************************
*    函 数 名: eth_get_tcp_status
*    功能说明: 获取tcp状态
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t eth_get_tcp_status(void)
{
    return g_lwipdev.tcp_status;
}
/*
*********************************************************************************************************
*    函 数 名: eth_get_udp_status
*    功能说明: 获取udp状态
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t eth_get_udp_status(void)
{
    return g_lwipdev.udp_multicast_status;
}

/*
*********************************************************************************************************
*    函 数 名: eth_set_udp_onvif_flag
*    功能说明: 设置ONVIF状态
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void eth_set_onvif_flag(uint8_t data)
{
    eth_carema_start = data;
}
/*
*********************************************************************************************************
*    函 数 名: eth_get_onvif_flag
*    功能说明: 获取ONVIF状态
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t eth_get_onvif_flag(void)
{
    return eth_carema_start;
}

/*
*********************************************************************************************************
*    函 数 名: eth_save_ipc_info
*    功能说明: 保存设备信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void eth_save_ipc_info(char *ip,uint8_t id)  // 保存设备信息
{
    uint8_t addr[4] = {0};

    sscanf(ip, "%d.%d.%d.%d",(int*)&addr[0],(int*)&addr[1],(int*)&addr[2],(int*)&addr[3]);

    app_set_camera_num_function(addr, id);
}

/*
*********************************************************************************************************
*    函 数 名: eth_get_ping_status
*    功能说明: 获取远程 ping 检测状态
*    形    参: 无
*    返 回 值: 0-空闲/完成  1-进行中
*********************************************************************************************************
*/
uint8_t eth_get_ping_status(void)
{
    return sg_ping_info_t.start;
}

/*
*********************************************************************************************************
*    函 数 名: eth_set_ping_status
*    功能说明: 设置远程 ping 检测状态
*    形    参: data 0-停止/完成  1-开始检测
*    返 回 值: 无
*********************************************************************************************************
*/
void eth_set_ping_status(uint8_t data)
{
    sg_ping_info_t.start = data;
}

/*
*********************************************************************************************************
*    函 数 名: eth_set_ping_ip_address
*    功能说明: 设置Ping IP地址
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void eth_set_ping_ip_address(uint8_t *ip)
{
    sg_ping_info_t.ip[0] = ip[0];
    sg_ping_info_t.ip[1] = ip[1];
    sg_ping_info_t.ip[2] = ip[2];
    sg_ping_info_t.ip[3] = ip[3];
}
/*
*********************************************************************************************************
*    函 数 名: eth_get_ping_info
*    功能说明: 获取远程 ping 结果信息
*    形    参: 无
*    返 回 值: 指向 sg_ping_info_t 的指针
*********************************************************************************************************
*/
void *eth_get_ping_info(void)
{
    return &sg_ping_info_t;
}
