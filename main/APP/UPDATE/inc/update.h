#ifndef _UPDATE_H_
#define _UPDATE_H_

#include "./SYSTEM/sys/sys.h"

#define UPDATE_MODE_NULL 0
#define UPDATE_MODE_LWIP 1
#define UPDATE_MODE_GPRS 2

#define UPDATE_NETWORK_CNT (6) // 网络连接次数
////

/* 20201103 */
typedef struct
{
	uint8_t mode;			// 0-不更新 1-通过LWIP更新 2-通过GPRS更新
	
	uint8_t ip[4]; 			// 更新地址
	uint32_t port; 			// 更新端口
	struct {
		uint8_t state; 		// 状态
		uint8_t connect; 	// 连接
	} tcp_t;
	struct {
		uint8_t connect;
	} gprs_t;
} update_param_t;
////

void update_status_init(void);
void update_set_update_mode(uint8_t mode);
uint8_t update_get_mode_function(void);
void *update_get_infor_data_function(void);

// 无线更新
int8_t update_mobile_task_function(void);
int8_t update_gsm_recevie_data_function(uint8_t *buff, uint16_t len);

// 有线更新
int8_t update_lwip_task_function(void);
int8_t update_tcp_send_function(uint8_t *buff, uint16_t len);

uint8_t *update_addr_ip(void);
uint32_t update_addr_port(void);

#endif
