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
#define ERR_TYPE_ELEC_BASE    (0x10000000UL)
#define ERR_TYPE_NET_BASE     (0x20000000UL)
#define ERR_TYPE_SENSOR_BASE  (0x30000000UL)       

// 电量故障类型定义
#define  ELEC_NORMAL                (0U)
#define  ELEC_MAIN_AC               (ERR_TYPE_ELEC_BASE | (1UL << 20))  // 断电
#define  ELEC_ACDC_MODULE           (ERR_TYPE_ELEC_BASE | (2UL << 20))  // ACDC模块故障
#define  ELEC_AC_OVER_V             (ERR_TYPE_ELEC_BASE | (3UL << 20))  // 过压
#define  ELEC_AC_LOW_V              (ERR_TYPE_ELEC_BASE | (4UL << 20))  // 低压
#define  ELEC_AC_OVER_C             (ERR_TYPE_ELEC_BASE | (5UL << 20))  // 过流
#define  ELEC_AC_LEAKAGE            (ERR_TYPE_ELEC_BASE | (6UL << 20))  // 漏电
#define  ELEC_AC_MCB                (ERR_TYPE_ELEC_BASE | (7UL << 20))  // 空开故障
#define  ELEC_GROUND_FAULT          (ERR_TYPE_ELEC_BASE | (8UL << 20))  // 地线缺失
#define  ELEC_AC_LN_FAULT           (ERR_TYPE_ELEC_BASE | (9UL << 20))  // 零火反接

// 网络故障类型定义
#define NET_NORMAL                  (0U)
#define NET_LAN_PORT                (ERR_TYPE_NET_BASE | (1UL << 20))                  // LAN端口故障 0x20100000
#define NET_MAIN_FAULT              (ERR_TYPE_NET_BASE | (2UL << 20))                  // 主网1故障 0x20200000
#define NET_MAIN2_FAULT             (ERR_TYPE_NET_BASE | (3UL << 20))                  // 主网2故障 0x20300000

#define NET_CAREMA1_FAULT           (ERR_TYPE_NET_BASE | (4UL << 20) | (1UL << 16))    // 摄像机1故障 0x20410000
#define NET_CAREMA2_FAULT           (ERR_TYPE_NET_BASE | (4UL << 20) | (2UL << 16))    // 摄像机2故障 0x20420000
#define NET_CAREMA3_FAULT           (ERR_TYPE_NET_BASE | (4UL << 20) | (3UL << 16))    // 摄像机3故障 0x20430000
#define NET_CAREMA4_FAULT           (ERR_TYPE_NET_BASE | (4UL << 20) | (4UL << 16))    // 摄像机4故障 0x20440000
#define NET_CAREMA5_FAULT           (ERR_TYPE_NET_BASE | (4UL << 20) | (5UL << 16))    // 摄像机5故障 0x20450000
#define NET_CAREMA6_FAULT           (ERR_TYPE_NET_BASE | (4UL << 20) | (6UL << 16))    // 摄像机6故障 0x20460000

#define NET_MAIN_DELAY              (ERR_TYPE_NET_BASE | (5UL << 20))                  // 主网1延时告警 0x20500000
#define NET_MAIN2_DELAY             (ERR_TYPE_NET_BASE | (6UL << 20))                  // 主网2延时告警 0x20600000

#define NET_CAREMA1_DELAY           (ERR_TYPE_NET_BASE | (7UL << 20) | (1UL << 16))    // 摄像机1延时告警 0x20710000
#define NET_CAREMA2_DELAY           (ERR_TYPE_NET_BASE | (7UL << 20) | (2UL << 16))    // 摄像机2延时告警 0x20720000
#define NET_CAREMA3_DELAY           (ERR_TYPE_NET_BASE | (7UL << 20) | (3UL << 16))    // 摄像机3延时告警 0x20730000
#define NET_CAREMA4_DELAY           (ERR_TYPE_NET_BASE | (7UL << 20) | (4UL << 16))    // 摄像机4延时告警 0x20740000
#define NET_CAREMA5_DELAY           (ERR_TYPE_NET_BASE | (7UL << 20) | (5UL << 16))    // 摄像机5延时告警 0x20750000
#define NET_CAREMA6_DELAY           (ERR_TYPE_NET_BASE | (7UL << 20) | (6UL << 16))    // 摄像机6延时告警 0x20760000

#define NET_MAIN_LOSS               (ERR_TYPE_NET_BASE | (8UL << 20))                  // 主网1丢包告警 0x20800000
#define NET_MAIN2_LOSS              (ERR_TYPE_NET_BASE | (9UL << 20))                  // 主网2丢包告警 0x20900000

#define NET_CAREMA1_LOSS            (ERR_TYPE_NET_BASE | (10UL << 20) | (1UL << 16))   // 摄像机1丢包告警 0x20A10000
#define NET_CAREMA2_LOSS            (ERR_TYPE_NET_BASE | (10UL << 20) | (2UL << 16))   // 摄像机2丢包告警 0x20A20000
#define NET_CAREMA3_LOSS            (ERR_TYPE_NET_BASE | (10UL << 20) | (3UL << 16))   // 摄像机3丢包告警 0x20A30000
#define NET_CAREMA4_LOSS            (ERR_TYPE_NET_BASE | (10UL << 20) | (4UL << 16))   // 摄像机4丢包告警 0x20A40000
#define NET_CAREMA5_LOSS            (ERR_TYPE_NET_BASE | (10UL << 20) | (5UL << 16))   // 摄像机5丢包告警 0x20A50000
#define NET_CAREMA6_LOSS            (ERR_TYPE_NET_BASE | (10UL << 20) | (6UL << 16))   // 摄像机6丢包告警 0x20A60000

#define NET_MAIN_IP_UNCONFIG        (ERR_TYPE_NET_BASE | (11UL << 20))                 // 主网1 IP未配置 0x20B00000
#define NET_MAIN2_IP_UNCONFIG       (ERR_TYPE_NET_BASE | (12UL << 20))                 // 主网2 IP未配置 0x20C00000


// 传感器故障类型定义
#define SENSOR_NORMAL               (0U)
#define SENSOR_TEMP_HIGH            (ERR_TYPE_SENSOR_BASE | (1UL << 20))  // 温度高
#define SENSOR_TEMP_LOW             (ERR_TYPE_SENSOR_BASE | (2UL << 20))  // 温度低
#define SENSOR_HUMI_HIGH            (ERR_TYPE_SENSOR_BASE | (3UL << 20))  // 湿度高
#define SENSOR_BOX_TILT             (ERR_TYPE_SENSOR_BASE | (4UL << 20))  // 倾斜
#define SENSOR_DOOR_OPEN            (ERR_TYPE_SENSOR_BASE | (5UL << 20))  // 门打开
#define SENSOR_WATER_LEAK           (ERR_TYPE_SENSOR_BASE | (6UL << 20))  // 漏水
#define SENSOR_SPD_FAULT            (ERR_TYPE_SENSOR_BASE | (7UL << 20))  // 防雷失效

/* 函数声明 */
void Error_Set(uint32_t item_idx);
void Error_Clear(uint32_t item_idx);
int8_t Error_GetAllCodes(uint32_t* codes);
int8_t Error_Get_Codesbuf(uint8_t* codes, uint16_t max_len);    
uint8_t Error_GetCount(void);

#endif
