#ifndef __TCP_CLIENT_H
#define __TCP_CLIENT_H

#include "./SYSTEM/sys/sys.h"
 
#define TCP_CLIENT_RX_BUFSIZE    2000    //接收缓冲区长度
#define REMOTE_PORT                    8087    //定义远端主机的IP地址
#define LWIP_SEND_DATA              0X8000    //定义有数据发送

int tcp_client_init(void);  //tcp客户端初始化(创建tcp客户端线程)

void tcp_client_start_function(void); // tcp客户端启动函数
void tcp_client_stop_function(void);  // tcp客户端停止函数

void tcp_set_send_buff(uint8_t *buff, uint16_t len);
uint8_t tcp_get_send_status(void);
int8_t tcp_send_data_immediately(uint8_t *str, uint16_t len);

#endif
