#ifndef _LWIP_PING_
#define _LWIP_PING_
#include "bsp.h"


/* 参数 */
#define PING_NETWORK_DELAY	 100  // ping网络延时判定时间

#define PING_DEV_MAX_NUM (3)
#define PING_TIME_OUT	 (4000)
#define PING_TIME_DELAY	 (2000)


#define LWIP_PING_WAIT 		 (1)	// 等待ping
#define LWIP_PING_SUCCESS 	 (0)	// 成功
#define LWIP_PING_FAIL	   	 (-1)	// 失败
#define LWIP_PING_NO_NETWORK (-2)	// 无网络


/* 函数声明 */

/** 执行函数 **/
void lwip_ping_timer_function(void);
int8_t lwip_ping_function(void);
uint8_t icmp_pcb_init(void);
void icmp_pcb_deinit(void);
int8_t lwip_mainnet_ping_function(void);


void lwip_ping_clear(void);
int8_t lwip_ping_the_specified_ip_function(uint8_t ip[4]);

/** 数据输入函数 **/
void lwip_ping_echo_reply(void);
uint16_t get_lwip_ping_times(void);

#endif
