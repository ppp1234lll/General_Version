#ifndef __TCP_SERVER_H
#define __TCP_SERVER_H

#include "sys.h"
#include "includes.h"

#define TCP_SERVER_RX_BUFSIZE	2000	// 定义tcp server最大接收数据长度
#define TCP_SERVER_PORT				8088	// 定义tcp server的端口
#define LWIP_TCP_SERVER_DATA	0x8000	// 定义有数据发送

INT8U tcp_server_init(void);		//TCP服务器初始化(创建TCP服务器线程)

void tcp_server_start_function(void);
void tcp_server_stop_function(void);  // tcp客户端停止函数
void tcp_server_set_send_buff(uint8_t *buff, uint16_t len);
uint8_t tcp_server_get_link_status(void);

#endif

