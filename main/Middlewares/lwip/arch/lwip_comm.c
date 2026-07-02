/**
 ****************************************************************************************************
 * @file        lwip_comm.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-08-01
 * @brief       lwIP配置驱动
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 */

#include "lwip_comm.h"
#include "main.h"


extern xSemaphoreHandle g_rx_semaphore;
__lwip_dev g_lwipdev;                       /* lwip控制结构体 */
struct netif lwip_netif;                    // 定义一个全局的网络接口

#ifdef LWIP_PROVIDE_ERRNO
int errno;
#endif

struct netif g_lwip_netif;                                  /* 定义一个全局的网络接口 */

/* LINK线程配置 */
#define LWIP_LINK_TASK_PRIO             3                   /* 任务优先级 */
#define LWIP_LINK_STK_SIZE              (512)               /* 任务堆栈大小 */
void lwip_link_thread( void * argument );                   /* 链路线程 */


/**
 * @breif       lwip 默认IP设置
 * @param       lwipx: lwip控制结构体指针
 * @retval      无
 */
void lwip_comm_default_ip_set(__lwip_dev *lwipx)
{
    
    static uint8_t last_domename[128] = {0};
    struct local_ip_t *param;
    struct remote_ip *remote;
    int    ip[4] = {0};
    int    ret   = 0;
    
    remote = app_get_remote_network_function();
    param = app_get_local_network_function();    

    /* 获取本地IP */
    lwipx->ip[0] = param->ip[0];    
    lwipx->ip[1] = param->ip[1];
    lwipx->ip[2] = param->ip[2];
    lwipx->ip[3] = param->ip[3];
    
    /* 获取本地子网掩码 */
    lwipx->netmask[0] = param->netmask[0];    
    lwipx->netmask[1] = param->netmask[1];
    lwipx->netmask[2] = param->netmask[2];
    lwipx->netmask[3] = param->netmask[3];
    
    /* 获取默认网关 */
    lwipx->gateway[0] = param->gateway[0];    
    lwipx->gateway[1] = param->gateway[1];
    lwipx->gateway[2] = param->gateway[2];
    lwipx->gateway[3] = param->gateway[3];    
    
    /* 获取MAC */
    lwipx->mac[0] = param->mac[0];     // 高三字节(IEEE称之为组织唯一ID,OUI)地址固定为:2.0.0
    lwipx->mac[1] = param->mac[1];
    lwipx->mac[2] = param->mac[2];
    lwipx->mac[3] = param->mac[3];     // 低三字节用STM32的唯一ID
    lwipx->mac[4] = param->mac[4];
    lwipx->mac[5] = param->mac[5]; 
    
    lwipx->dns[0] = param->dns[0];
    lwipx->dns[1] = param->dns[1];
    lwipx->dns[2] = param->dns[2];
    lwipx->dns[3] = param->dns[3];
    
    /* 获取远端IP */
    ret = sscanf((char*)remote->inside_iporname,"%d.%d.%d.%d",&ip[0],&ip[1],&ip[2],&ip[3]);
    if(ret != 4)
    {
        if (memcmp(last_domename,remote->inside_iporname,128) != 0) {
            memcpy(last_domename,remote->inside_iporname,128);
            lwipx->domename = 0;         // 通过域名获取ip
        }
        /* 内外连接地址是域名 */
        lwipx->iporname = 1;
    }
    else
    {
        lwipx->iporname = 0;
        /* 内外连接地址是IP */
        lwipx->remoteip[0]=ip[0];    
        lwipx->remoteip[1]=ip[1];
        lwipx->remoteip[2]=ip[2];
        lwipx->remoteip[3]=ip[3];
    }
    
    /* 获取远端端口 */
    lwipx->remoteport = remote->inside_port;
//    /* 默认远端IP为:192.168.2.134 */
//    lwipx->remoteip[0] = 192;
//    lwipx->remoteip[1] = 168;
//    lwipx->remoteip[2] = 2;
//    lwipx->remoteip[3] = 134;
//    
//    /* MAC地址设置 */
//    lwipx->mac[0] = 0xB8;
//    lwipx->mac[1] = 0xAE;
//    lwipx->mac[2] = 0x1D;
//    lwipx->mac[3] = 0x00;
//    lwipx->mac[4] = 0x04;
//    lwipx->mac[5] = 0x00;
//    
//    /* 默认本地IP为:192.168.2.30 */
//    lwipx->ip[0] = 192;
//    lwipx->ip[1] = 168;
//    lwipx->ip[2] = 2;
//    lwipx->ip[3] = 30;
//    /* 默认子网掩码:255.255.255.0 */
//    lwipx->netmask[0] = 255;
//    lwipx->netmask[1] = 255;
//    lwipx->netmask[2] = 255;
//    lwipx->netmask[3] = 0;
//    
//    /* 默认网关:192.168.2.1 */
//    lwipx->gateway[0] = 192;
//    lwipx->gateway[1] = 168;
//    lwipx->gateway[2] = 2;
//    lwipx->gateway[3] = 1;
}

/**
 * @breif       LWIP初始化(LWIP启动的时候使用)
 * @param       无
 * @retval      0,成功
 *              1,内存错误
 *              2,以太网芯片初始化失败
 *              3,网卡添加失败.
 */
uint8_t lwip_comm_init(void)
{
    struct netif *netif_init_flag;                  /* 调用netif_add()函数时的返回值,用于判断网络初始化是否成功 */
    ip_addr_t ipaddr;                               /* ip地址 */
    ip_addr_t netmask;                              /* 子网掩码 */
    ip_addr_t gw;                                   /* 默认网关 */
    
    tcpip_init(NULL, NULL);

    lwip_comm_default_ip_set(&g_lwipdev);           /* 设置默认IP等信息 */

    IP4_ADDR(&ipaddr, g_lwipdev.ip[0], g_lwipdev.ip[1], g_lwipdev.ip[2], g_lwipdev.ip[3]);
    IP4_ADDR(&netmask, g_lwipdev.netmask[0], g_lwipdev.netmask[1], g_lwipdev.netmask[2], g_lwipdev.netmask[3]);
    IP4_ADDR(&gw, g_lwipdev.gateway[0], g_lwipdev.gateway[1], g_lwipdev.gateway[2], g_lwipdev.gateway[3]);
    printf("网卡en的MAC地址为:................%d.%d.%d.%d.%d.%d\r\n", g_lwipdev.mac[0], g_lwipdev.mac[1], g_lwipdev.mac[2], g_lwipdev.mac[3], g_lwipdev.mac[4], g_lwipdev.mac[5]);
    printf("静态IP地址........................%d.%d.%d.%d\r\n", g_lwipdev.ip[0], g_lwipdev.ip[1], g_lwipdev.ip[2], g_lwipdev.ip[3]);
    printf("子网掩码..........................%d.%d.%d.%d\r\n", g_lwipdev.netmask[0], g_lwipdev.netmask[1], g_lwipdev.netmask[2], g_lwipdev.netmask[3]);
    printf("默认网关..........................%d.%d.%d.%d\r\n", g_lwipdev.gateway[0], g_lwipdev.gateway[1], g_lwipdev.gateway[2], g_lwipdev.gateway[3]);

    /* 向网卡列表中添加一个网口 */
    netif_init_flag = netif_add(&g_lwip_netif, (const ip_addr_t *)&ipaddr, (const ip_addr_t *)&netmask, (const ip_addr_t *)&gw, NULL, &ethernetif_init, &tcpip_input);
    
    if (netif_init_flag == NULL)
    {
		g_lwipdev.init = 0;
        return 1;                                   /* 网卡添加失败 */
    }
    else                                            /* 网口添加成功后,设置netif为默认值,并且打开netif网口 */
    {
        g_lwipdev.init = 1;
        netif_set_default(&g_lwip_netif);           /* 设置netif为默认网口 */

#if LWIP_NETIF_LINK_CALLBACK
        // lwip_link_status_updated(&g_lwip_netif);    /* DHCP链接状态更新函数 */
        // netif_set_link_callback(&g_lwip_netif, lwip_link_status_updated);
        taskENTER_CRITICAL();           /* 进入临界区 */
        /* 查询PHY连接状态任务 */
        sys_thread_new("eth_link",
                       lwip_link_thread,            /* 任务入口函数 */
                       &g_lwip_netif,               /* 任务入口函数参数 */
                       LWIP_LINK_STK_SIZE,          /* 任务栈大小 */
                       LWIP_LINK_TASK_PRIO);        /* 任务的优先级 */
        taskEXIT_CRITICAL();           /* 退出临界区 */
#endif
    }
    g_lwipdev.link_status = LWIP_LINK_OFF;          /* 链接标记为0 */
        
    /* 启动网络功能 */
    igmp_init();
    httpd_init();
    lwip_ping_multi_init();
    lwip_ping_remote_init();
    dns_init();

    return 0;                                       /* 操作OK. */
}

/**
  * @brief       检查ETH链路状态，更新netif
  * @param       argument: netif
  * @retval      无
  */
void lwip_link_thread( void * argument )
{
    uint16_t regval = 0;
    struct netif *netif = (struct netif *) argument;
    int link_again_num = 0;

    while(1)
    {
        /* 读取PHY状态寄存器，获取链接信息 */
        enet_phy_write_read(ENET_PHY_READ, PHY_ADDRESS, PHY_REG_BSR, &regval);
        
        /* 判断链接状态 */
        if((regval & PHY_LINKED_STATUS) == 0)
        {
            g_lwipdev.link_status = LWIP_LINK_OFF;
            link_again_num ++ ;
            if (link_again_num >= 2)                    /* 网线一段时间没有插入 */
            {
                continue;
            }
            else                                        /* 关闭虚拟网卡及以太网中断 */
            {
                led_control_function(LD_LAN,LD_OFF);
                led_control_function(LD_LAN_EXT,LD_OFF);
				g_lwipdev.netif_state = 0;			  
                enet_disable();
                enet_interrupt_disable(ENET_DMA_INT_NIE);
                enet_interrupt_disable(ENET_DMA_INT_RIE);                
                netif_set_down(netif);
                netif_set_link_down(netif);
                eth_set_network_reset();  // 关闭网络链接
                printf("ETH链路断开\r\n");
            }
        }
        else                                            /* 网线插入检测 */
        {
            link_again_num = 0;

            if (g_lwipdev.link_status == LWIP_LINK_OFF)/* 开启以太网及虚拟网卡 */
            {
                led_control_function(LD_LAN,LD_ON);
                led_control_function(LD_LAN_EXT,LD_ON);
				g_lwipdev.netif_state = 1;
                g_lwipdev.link_status = LWIP_LINK_ON;
                enet_enable();
                enet_interrupt_enable(ENET_DMA_INT_NIE);
                enet_interrupt_enable(ENET_DMA_INT_RIE);
                netif_set_up(netif);
                netif_set_link_up(netif);

                printf("ETH链路插入\r\n");
            }

            if(g_lwipdev.netif_state == 0) // 重新插入网线后，需要重新初始化网络
            {
                printf("重新配置，需要重新初始化网络\r\n");
                lwip_start_function();
            }

        }
        vTaskDelay(100);
    }
}

/*
*********************************************************************************************************
*    函 数 名: ENET_IRQHandler
*    功能说明: 以太网中断服务函数.
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void ENET_IRQHandler(void)
{
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    /* 接收中断 */
    if(SET == enet_interrupt_flag_get(ENET_DMA_INT_FLAG_RS)){ 
        /* 发送信号量给lwip的底层接收 */
        xSemaphoreGiveFromISR(g_rx_semaphore, &xHigherPriorityTaskWoken);
    }

    /* clear the enet DMA Rx interrupt pending bits */
    enet_interrupt_flag_clear(ENET_DMA_INT_FLAG_RS_CLR);
    enet_interrupt_flag_clear(ENET_DMA_INT_FLAG_NI_CLR);

    /* switch tasks if necessary */
    if(pdFALSE != xHigherPriorityTaskWoken){
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    }
}


/*
*********************************************************************************************************
*    函 数 名: lwip_get_mac_addr
*    功能说明: 获取网卡MAC地址
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t *lwip_get_mac_addr(void)
{
    static uint8_t temp[20] = {0};
    
    sprintf((char*)temp,"%02x-%02x-%02x-%02x-%02x-%02x",g_lwipdev.mac[0],
                                                        g_lwipdev.mac[1],
                                                        g_lwipdev.mac[2],
                                                        g_lwipdev.mac[3],
                                                        g_lwipdev.mac[4],
                                                        g_lwipdev.mac[5]);
    
    return temp;
}

/*
*********************************************************************************************************
*    函 数 名: lwip_updata_remote_network_infor
*    功能说明: 更新远端参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void lwip_updata_remote_network_infor(__lwip_dev *lwipx)
{
    static uint8_t sg_last_domename[128] = {0};
    struct remote_ip *remote = NULL;
    int    ret     = 0;
    int    ip[4]   = {0};
    
    remote = app_get_remote_network_function();
    
    /* 获取远端IP */
    ret = sscanf((char*)remote->inside_iporname,"%d.%d.%d.%d",&ip[0],&ip[1],&ip[2],&ip[3]);
    if(ret != 4)
    {
        /* 内外连接地址是域名 */
        lwipx->iporname = 1;
        if (memcmp(sg_last_domename,remote->inside_iporname,128) != 0) {
            memcpy(sg_last_domename,remote->inside_iporname,128);
            
            lwipx->domename = 0;         // 通过域名获取ip
        }
    }
    else
    {
        lwipx->domename = 0;     
        lwipx->iporname = 0;
        /* 内外连接地址是IP */
        lwipx->remoteip[0]=ip[0];    
        lwipx->remoteip[1]=ip[1];
        lwipx->remoteip[2]=ip[2];
        lwipx->remoteip[3]=ip[3];
    }
    
    /* 获取远端端口 */
    lwipx->remoteport = remote->inside_port;
}

/*
*********************************************************************************************************
*    函 数 名: lwip_start_function
*    功能说明: 网络启动函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t lwip_start_function(void)
{
    if(g_lwipdev.init == 0) {
        lwip_comm_init();
    }
    
    if(g_lwipdev.init == 1) 
	{
//        enet_mac_dma_config();
        lwip_comm_default_ip_set(&g_lwipdev);        // 设置默认IP等信息

        printf("\n本机IP: %d.%d.%d.%d\n", g_lwipdev.ip[0],g_lwipdev.ip[1],g_lwipdev.ip[2],g_lwipdev.ip[3]);
        IP4_ADDR(&g_lwip_netif.ip_addr,g_lwipdev.ip[0],g_lwipdev.ip[1],g_lwipdev.ip[2],g_lwipdev.ip[3]);
        IP4_ADDR(&g_lwip_netif.netmask,g_lwipdev.netmask[0],g_lwipdev.netmask[1] ,g_lwipdev.netmask[2],g_lwipdev.netmask[3]);
        IP4_ADDR(&g_lwip_netif.gw,g_lwipdev.gateway[0],g_lwipdev.gateway[1],g_lwipdev.gateway[2],g_lwipdev.gateway[3]);
        
        g_lwipdev.netif_state = 1;
        enet_enable();
        enet_interrupt_enable(ENET_DMA_INT_NIE);
        enet_interrupt_enable(ENET_DMA_INT_RIE);
        netif_set_up(&g_lwip_netif);
        netif_set_link_up(&g_lwip_netif);
    }
    
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: lwip_stop_function
*    功能说明: 网络停止函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void lwip_stop_function(void)
{
	if(g_lwipdev.init == 1) 
	{
		if(g_lwipdev.netif_state == 1) 
		{
			g_lwipdev.netif_state = 0;
            enet_disable();
            enet_interrupt_disable(ENET_DMA_INT_NIE);
            enet_interrupt_disable(ENET_DMA_INT_RIE);                
            netif_set_down(&g_lwip_netif);
            netif_set_link_down(&g_lwip_netif);
		}
	}
}

