#ifndef __RTSP_TASK_
#define __RTSP_TASK_

#include <sys.h>

void rtsp_timer_function(void);
int8_t rtsp_get_ip_function(uint8_t ip[4]);
int8_t rtsp_connect_server(uint8_t ip[4],int port);
int rtsp_send_method(uint8_t ip[4]);
int rtsp_recv_method(void);
int rtsp_deal_recv_data(char *data);

void rtsp_thread_start(void);
void rtsp_thread_stop(void);


#endif // 
