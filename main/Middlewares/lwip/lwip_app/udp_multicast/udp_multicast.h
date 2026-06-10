#ifndef __UDP_MULTICAST_H
#define __UDP_MULTICAST_H

#include "./SYSTEM/sys/sys.h"

#define UDP_MULTICAST_RX_LEN		800
#define UDP_MULTICAST_DATA			0x8000  //有数据发送
#define UDP_MULTICAST_PORT			12360		//udp服务器的本地端口号

//#define MULTICAST_ADDR    "239.255.255.251"  // IPC组播IP地址
//#define MULTICAST_PROT    65500            	 // IPC组播端口号

typedef struct
{
	uint8_t flag;
	char ip[20];
}udp_multicast_t;      // 参数


int  udp_multicast_init(void);
void udp_multicast_start_function(void);
void udp_multicast_stop_function(void);

int  udp_socket_send_api(int sockfd,char* data,int len,char* ip,int port);
void udp_multicast_send_buff(uint8_t *buff, uint16_t len);


#endif







