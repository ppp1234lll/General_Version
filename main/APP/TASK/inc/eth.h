#ifndef _ETH_H_
#define _ETH_H_

typedef enum
{
    ETH_ONVIF_INIT = 0,
    ETH_ONVIF_START,
    ETH_ONVIF_OSD,
    ETH_ONVIF_END,
}eth_onvif_flag_t;

#include "./SYSTEM/sys/sys.h"
#include "lwip/dhcp.h"
#include "lwip/timeouts.h"
//#include "lwip/tcp_impl.h"
#include "lwip/ip4_frag.h"
#include "lwip/tcpip.h" 

/* º¯ÊýÉùÃ÷ */
void eth_task_function(void);
void eth_network_reset_function(void);
void eth_ping_timer_function(void);
void eth_ping_detection_function(void);

void eth_tcp_connect_control_function(void);
void eth_set_tcp_connect_reset(void);
void eth_set_tcp_cmd(uint8_t cmd);

uint8_t eth_get_network_cable_status(void);
uint8_t eth_get_tcp_status(void);

void eth_udp_connect_control_function(void);

uint8_t eth_get_udp_status(void);
void eth_set_network_reset(void);
void eth_set_udp_connect_reset(void);
void eth_set_onvif_udp_connect_reset(void);
void eth_set_onvif_tcp_connect_reset(void);

void eth_save_ipc_info(char *ip,uint8_t id);
void eth_set_onvif_flag(uint8_t data);
uint8_t eth_get_onvif_flag(void);

void eth_rtsp_connect_reset(void);
void eth_rtsp_connect_control_function(void);
int8_t eth_carema_search_function(void);

void eth_snmp_connect_control_function(void);
void eth_snmp_connect_reset(void);

#endif


