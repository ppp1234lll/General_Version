#ifndef __SNMP_MIB_H__
#define __SNMP_MIB_H__

#include "sys.h"
void snmp_send_get(const char *oid_str);
void snmp_udp_init(void);
void snmp_demo(void);
void start_snmp_demo_timer(void);
void switch_port_control(int port, int status);
void switch_port_poe_control(int port, int status);
const char* get_device_model(void);
const char* get_device_sn(void);
int get_port_status(uint8_t port);
int get_port_tx_bytes(uint8_t port);
int get_port_rx_bytes(uint8_t port);
int get_mac_table_count(void);
int get_uptime_ticks(void);
void get_port_status_array(char *outbuf, int outbuf_size);
void report_switch_info(char *outbuf, int outbuf_size);
void report_switch_port_info(char *outbuf, int outbuf_size);
void report_switch_mac_table(char *outbuf, int outbuf_size);
#endif // __SNMP_MIB_H__
