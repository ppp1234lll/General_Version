#ifndef _ERROR_H_
#define _ERROR_H_

#include "./SYSTEM/sys/sys.h"

typedef struct
{
    uint32_t fault_index[32];
    uint8_t fault_count;
} ErrorFault_t;

/* 错误码 */
typedef enum {  
    ERR_TYPE_ELEC,  
    ERR_TYPE_NET,      
    ERR_TYPE_SENSOR,    
    ERR_MAX            
} ErrorType_e;         

// 错误类型基础值定义
#define ERR_TYPE_ELEC_BASE    0x10000000
#define ERR_TYPE_NET_BASE     0x20000000
#define ERR_TYPE_SENSOR_BASE  0x30000000       

// 电量故障类型定义
#define  ELEC_NORMAL				0
#define  ELEC_MAIN_AC				ERR_TYPE_ELEC_BASE | 1 << 20  // 断电
#define  ELEC_ACDC_MODULE			ERR_TYPE_ELEC_BASE | 2 << 20  // ACDC模块故障
#define  ELEC_AC_OVER_V				ERR_TYPE_ELEC_BASE | 3 << 20  // 过压
#define  ELEC_AC_LOW_V				ERR_TYPE_ELEC_BASE | 4 << 20  // 低压
#define  ELEC_AC_OVER_C				ERR_TYPE_ELEC_BASE | 5 << 20  // 过流
#define  ELEC_AC_LEAKAGE			ERR_TYPE_ELEC_BASE | 6 << 20  // 漏电
#define  ELEC_AC_MCB				ERR_TYPE_ELEC_BASE | 7 << 20  // 空开故障
#define  ELEC_GROUND_FAULT			ERR_TYPE_ELEC_BASE | 8 << 20  // 地线缺失
#define  ELEC_AC_LN_FAULT			ERR_TYPE_ELEC_BASE | 9 << 20  // 零火反接

// 网络故障类型定义
#define NET_NORMAL					0
#define NET_LAN_PORT				ERR_TYPE_NET_BASE | 1 << 20  // LAN端口故障
#define NET_MAIN_IP					ERR_TYPE_NET_BASE | 2 << 20  // 主IP故障
#define NET_SINGLE_IP				ERR_TYPE_NET_BASE | 3 << 20  // 单IP故障
#define NET_MAIN_IP_DELAY			ERR_TYPE_NET_BASE | 4 << 20  // 主IP延时
#define NET_SINGLE_IP_DELAY			ERR_TYPE_NET_BASE | 5 << 20  // 单IP延时
#define NET_CAREMA1_FAULT			ERR_TYPE_NET_BASE | 6 << 20  // 摄像机1故障
#define NET_CAREMA2_FAULT			ERR_TYPE_NET_BASE | 7 << 20  // 摄像机2故障
#define NET_CAREMA3_FAULT			ERR_TYPE_NET_BASE | 8 << 20  // 摄像机3故障
#define NET_CAREMA4_FAULT			ERR_TYPE_NET_BASE | 9 << 20  // 摄像机4故障
#define NET_CAREMA5_FAULT			ERR_TYPE_NET_BASE | 10 << 20  // 摄像机5故障
#define NET_CAREMA6_FAULT			ERR_TYPE_NET_BASE | 11 << 20  // 摄像机6故障
#define NET_MAIN2_FAULT			    ERR_TYPE_NET_BASE | 12 << 20  // 主IP2故障

// 传感器故障类型定义
#define SENSOR_NORMAL				0
#define SENSOR_TEMP_HIGH		ERR_TYPE_SENSOR_BASE | 1 << 20	// 温度高
#define SENSOR_TEMP_LOW			ERR_TYPE_SENSOR_BASE | 2 << 20	// 温度低
#define SENSOR_HUMI_HIGH		ERR_TYPE_SENSOR_BASE | 3 << 20	// 湿度高
#define SENSOR_BOX_TILT     	ERR_TYPE_SENSOR_BASE | 4 << 20	// 倾斜
#define SENSOR_DOOR_OPEN    	ERR_TYPE_SENSOR_BASE | 5 << 20	// 门打开
#define SENSOR_WATER_LEAK   	ERR_TYPE_SENSOR_BASE | 6 << 20	// 漏水


/* 函数声明 */
void Error_Set(uint32_t item_idx);
void Error_Clear(uint32_t item_idx);
int8_t Error_GetAllCodes(uint32_t* codes);
int8_t Error_Get_Codesbuf(uint8_t* codes, uint16_t max_len);	
uint8_t Error_GetCount(void);

#endif
