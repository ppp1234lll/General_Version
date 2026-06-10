#ifndef _GSM_H_
#define _GSM_H_
#include "bsp.h"

/* 参数 */
#define GSM_TCP_CONNECT_TIME (3)


/* 函数声明 */
void gsm_task_function(void);
void gsm_task_timer_function(void);

void gsm_tcp_control_function(void);
void gsm_reset_task_function(void);
void gsm_send_tcp_data(uint8_t *data, uint16_t size);
uint8_t  gsm_gst_init_status_function(uint8_t sel);
void gsm_gst_run_status_function(char *buff, uint8_t sel);
uint8_t gsm_data_send_function(uint8_t *buff, uint16_t len);

uint8_t *gsm_get_sim_ccid_function(void);
uint8_t gsm_get_network_connect_status_function(void);

void gsm_set_tcp_cmd(uint8_t cmd);
void gsm_set_network_reset_function(void);
void gsm_set_module_reset_function(void);

int8_t gsm_gps_task_function(void);
double gsm_get_location_information_function(uint8_t mode);
void gsm_run_gps_task_function(void);
void *gsm_get_gprs_information_function(void);

int8_t gsm_network_status_check(void);
#endif

