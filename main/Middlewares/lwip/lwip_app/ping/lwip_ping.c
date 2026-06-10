/*******************************************************************************
*
* @File name	: 
* @Description	: 
* @Author		: 
*	Version	Date		Modification Description
*	1.0		
*
*******************************************************************************/
#include "lwip_ping.h"
#include "lwip/api.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet.h"
#include "lwip/inet_chksum.h"
#include "stdio.h"
#include "app.h"
#include "det.h"
#include "./MALLOC/malloc.h"
#include "lwip/sockets.h"
#include "./SYSTEM/delay/delay.h"

/* 参数 */
typedef struct
{
	uint8_t  state;
	uint8_t  cnt;
	uint8_t  error_cnt;
	uint8_t  time_flag;
	uint16_t times;
	uint16_t ping_times_report;  // ping上报时间 
	uint16_t ping_times;    //ping延时时间

} lwip_ping_operate_t;

lwip_ping_operate_t sg_pingoperate_t = {0};
struct raw_pcb 		*ping_pcb		 = {0};
uint8_t 			g_pingechoreplay = 0;

/* 函数声明 */
static int8_t lwip_ping_send(uint8_t local_ip[4], uint8_t remote_ip[4]);

static long int ping_counter=0;

uint8_t raw_callback(void *arg,struct raw_pcb *pcb,struct pbuf *p,const ip_addr_t *addr)
{
  uint8_t buf[40],ct[20];
  //struct ip_hdr  *iphdr;
  if(p->tot_len>=(PBUF_IP_HLEN + 8))
  {
     //iphdr=(struct ip_hdr  *)((u8_t*)p->payload);
     ping_counter++;
     sprintf((char *)buf,"IP:%d.%d.%d.%d",*(char *)addr,*((char *)addr+1),*((char *)addr+2),*((char *)addr+3));

     sprintf((char *)ct,"Ping times:%ld",ping_counter);

  }
  return 0;
}

/************************************************************
*
* Function name	: icmp_pcb_init
* Description	: 初始化
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t icmp_pcb_init(void)
{
	ping_pcb = raw_new(IP_PROTO_ICMP);
	if(!ping_pcb)
	{
		return 1;
	}
	raw_recv(ping_pcb,raw_callback,NULL);
	raw_bind(ping_pcb,IP_ADDR_ANY);
	
	return 0;
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
void icmp_pcb_deinit(void)
{
	raw_remove(ping_pcb);
}

/************************************************************
*
* Function name	: lwip_ping_the_specified_ip_function
* Description	: 对指定ip进行ping操作
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t lwip_ping_the_specified_ip_function(uint8_t ip[4])
{
	int8_t ret = LWIP_PING_WAIT;
	if(g_lwipdev.netif_state == 1)
	{
		/* 开始ping操作 */
		switch(sg_pingoperate_t.state)
		{
			case 0:
				lwip_ping_send(g_lwipdev.ip,ip);
				sg_pingoperate_t.state 	   = 1;
				sg_pingoperate_t.times 	   = PING_TIME_OUT;
				sg_pingoperate_t.time_flag = 0;
				break;
			case 1:
				if(sg_pingoperate_t.time_flag == 1)	// PING_TIME_OUT超时未接收到应答信号
				{
					sg_pingoperate_t.times 	   	= PING_TIME_DELAY;
					sg_pingoperate_t.time_flag 	= 0;
					sg_pingoperate_t.state 	   	= 2;
					sg_pingoperate_t.cnt 	   		= 0;
					sg_pingoperate_t.error_cnt++;  // 未ping通
					sg_pingoperate_t.ping_times_report =0;  // 清除ping计时
					sg_pingoperate_t.ping_times =0;
				}
				if(g_pingechoreplay == 1) 			// 获取到接收到应答信号
				{
					// ping时间获取放在lwip_ping_echo_reply中
					if(sg_pingoperate_t.cnt == 0)      //第一次ping通
						sg_pingoperate_t.ping_times_report = sg_pingoperate_t.ping_times;
					else
					{
						if(sg_pingoperate_t.ping_times >= sg_pingoperate_t.ping_times_report)
							sg_pingoperate_t.ping_times_report = sg_pingoperate_t.ping_times;
					}
					sg_pingoperate_t.times 	   = PING_TIME_DELAY;
					sg_pingoperate_t.time_flag = 0;
					sg_pingoperate_t.state 	   = 2;
					sg_pingoperate_t.error_cnt = 0;
					g_pingechoreplay 		   = 0;
					sg_pingoperate_t.cnt++;				
				}
				break;
			case 2:
				if(sg_pingoperate_t.time_flag == 1)  // 超时
				{
					sg_pingoperate_t.time_flag = 0;
					sg_pingoperate_t.state 	   = 0;
				}
				break;
		}
		if(sg_pingoperate_t.error_cnt >= 4)		/* 连续4次未ping通 */
		{
			sg_pingoperate_t.error_cnt = 0;
			sg_pingoperate_t.cnt 	   = 0;			/* 等待下一次查找 */
			ret = LWIP_PING_FAIL;
		}
		if(sg_pingoperate_t.cnt >= 4)		/* 连续4次ping通 */
		{
			sg_pingoperate_t.error_cnt = 0;
			sg_pingoperate_t.cnt 	   = 0;			/* 等待下一次查找 */
			ret = LWIP_PING_SUCCESS;
			
		}
	}
	else
	{
		ret = LWIP_PING_NO_NETWORK;
	}
	
	return ret;
}

/************************************************************
*
* Function name	: lwip_ping_clear
* Description	: 清空一次ping参数区
* Parameter		: 
* Return		: 
*	
************************************************************/
void lwip_ping_clear(void)
{
	sg_pingoperate_t.state     = 0;
	sg_pingoperate_t.cnt       = 0;
	sg_pingoperate_t.error_cnt = 0;
	sg_pingoperate_t.times     = 0;
	sg_pingoperate_t.time_flag = 0;
}

/************************************************************
*
* Function name	: lwip_ping_timer_function
* Description	: 记时相关函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void lwip_ping_timer_function(void)
{
	if(sg_pingoperate_t.times != 0)
	{
		sg_pingoperate_t.times--;
		if(sg_pingoperate_t.times == 0)
		{
			sg_pingoperate_t.time_flag = 1;
		}
	}
	
}

void ping_prepare_echo(struct icmp_echo_hdr *iecho,uint16_t ping_size)
{
  size_t i;
  size_t data_len=ping_size-sizeof(struct icmp_echo_hdr);
	
  ICMPH_TYPE_SET(iecho,ICMP_ECHO);
  ICMPH_CODE_SET(iecho,0);
  iecho->chksum=0;
  iecho->id=0x01;
  iecho->seqno=0x8418;
  for(i=0;i<data_len;i++)
  {
   ((char*)iecho)[sizeof(struct icmp_echo_hdr)+i]=1;
  }
  iecho->chksum=inet_chksum(iecho,ping_size);
}
/************************************************************
*
* Function name	: lwip_ping_send
* Description	: ping发送函数
* Parameter		: 
*	@local_ip	: 本地IP
*	@remote_ip  : 目标IP
* Return		: 
*	
************************************************************/
static int8_t lwip_ping_send(uint8_t local_ip[4], uint8_t remote_ip[4])
{
	struct pbuf 		 *p     = NULL;
	struct icmp_echo_hdr *iecho = NULL;
	err_t 				 err    = 0;
//	uint8_t 			 index  = 0;
	int8_t				 ret    = 0;
	struct ip4_addr		 ipaddr = {0};
	
	/* 处理IP地址 */
	IP4_ADDR(&ipaddr, local_ip[0], local_ip[1],  local_ip[2], local_ip[3]);
	/* 设置本地地址 */
	ip_addr_set(&ping_pcb->local_ip, &ipaddr);
	IP4_ADDR(&ipaddr, remote_ip[0], remote_ip[1], remote_ip[2], remote_ip[3]);
	/* 设置需要ping的目标地址 */
	ip_addr_set(&ping_pcb->remote_ip, &ipaddr);
	
	/*申请内存  */
	//p = pbuf_alloc(PBUF_IP, sizeof(struct icmp_echo_hdr),PBUF_RAM);
	p = pbuf_alloc(PBUF_IP,32,PBUF_RAM);
	if(!p)
	{
		ret = -1;
		goto ERROR;
	}
	
	if(p->len == p->tot_len && p->next == NULL) {
		iecho = (struct icmp_echo_hdr *)p->payload;
		ping_prepare_echo(iecho,32);
		err = raw_sendto(ping_pcb, p, &ping_pcb->remote_ip);
		if(err != ERR_OK)
		{
			ret = -2;
			goto ERROR;
		}
	}	
	pbuf_free(p);
	
	return ret;
	
ERROR:
	pbuf_free(p);
	
	return ret;
}

/************************************************************
*
* Function name	: lwip_ping_echo_reply
* Description	: 接收到回传数据
* Parameter		: 
* Return		: 
*	      在收到回复后计算时间，不然时间计算不准确
*       当程序跑到lwip_ping_the_specified_ip_function函数，时间已经过去一段时间
************************************************************/
void lwip_ping_echo_reply(void)
{
	sg_pingoperate_t.ping_times = PING_TIME_OUT - sg_pingoperate_t.times;  // 网络延时时间  20220308
	g_pingechoreplay = 1;
}

/************************************************************
*
* Function name	: get_lwip_ping_times
* Description	: 获取ping延时时间
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t get_lwip_ping_times(void)  // 网络延时时间  20220308
{
	return sg_pingoperate_t.ping_times_report;
}


