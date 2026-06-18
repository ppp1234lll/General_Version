#ifndef __SNMP_UDP_H_
#define __SNMP_UDP_H_

#include "./SYSTEM/sys/sys.h"

#define SNMP_NONE       0
#define SNMP_START      1
#define SNMP_LINK       2
#define SNMP_SEND       3
#define SNMP_RECV       4
#define SNMP_END          ((uint8_t)0x80)  // 搜索结束

typedef enum
{
    SNMP_IPC = 0,
    SNMP_ONV  ,
    SNMP_SWITCH ,
} snmp_dev_type_t;

// 摄像机【品牌】
typedef enum
{
    IPC_HIKVISION = 0,
    IPC_DAHUA,
    IPC_UNV,
    IPC_MAX,
} snmp_ipc_brand_t;

typedef struct 
{
    char         brand[32];                      // 品牌
    char         device_model[32];                  // 型号
    uint8_t     port_status[10];  // 端口状态（1=up/2=down）
    uint8_t     port_poe[10];     // PoE状态（1=开/2=关）    
    uint8_t     port_poe_power[10]; // PoE功率（1=100W/2=200W）
}switch_t;


typedef struct
{
    uint8_t ipc_param[3][10][32]; 
    uint8_t onv_param[1][10][32]; 
    switch_t switch_param[1]; 
} snmp_param_t;


void snmp_start_function(void);
void snmp_stop_function(void);

void snmp_set_enable_flag(uint8_t flag);
uint8_t snmp_get_status(void);

int snmp_link_function(char *ip,int port);
int8_t snmp_send_function(char* ip,int port,char* data,int len);
int snmp_build_get_packet(uint8_t *packet, int max_len, uint8_t oid,uint8_t brand_type,uint8_t port_id) ;
int snmp_oid_str_to_ber(const char *oid_str, uint8_t *ber, int max_len);

int snmp_recv_function(uint8_t oid);

int8_t snmp_deal_hikvision_response(uint8_t *buf, int len,uint8_t oid);
int8_t snmp_deal_dahua_response(uint8_t *buf, int len,uint8_t oid);
int8_t snmp_deal_switch_response(uint8_t *buf, int len,uint8_t oid);


void *snmp_get_snmp_param(void);
uint8_t snmp_get_snmp_ipc_brand(void);
#endif // __SNMP_UDP_H__
