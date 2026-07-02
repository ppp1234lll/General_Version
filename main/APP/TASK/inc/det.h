#ifndef _DET_H_
#define _DET_H_

#include "./SYSTEM/sys/sys.h"
#include "ebtn_APP_Keys.h"
#include "bsp_relay.h"

/* 按键事件 */
typedef enum
{
    KEY_NONE  = 0,     // 无事件
    KEY_EVNT,          // 事件触发
    KEY_ERASE,         // 擦除
}KEY_VALUE_E;

/* 电量 */
typedef enum
{
    ENERGY_VOLTAGE  = 0,     // 电压
    ENERGY_CURRENT,          // 电流
    ENERGY_POWER,            // 功率
    ENERGY_ENERGY,           // 用电量
}ENERGY_VALUE_E;

// 电量参数
typedef struct {
    float voltage;        // 220V电压检测
    float current;         // 总电流检测
    float power;          // 总功率1
} energy_t;

typedef struct {
    float total; // 总用电量
    float port[RELAY_NUM];
} electricity_t;

typedef struct
{
    float temp_inside;            // 内部温度值
    float humi_inside;            // 内部湿度值
    uint16_t attitude_acc;        // 加速度
    uint8_t key_s[KEYS_COUNT];    // 按键数量定义
    
    uint8_t camera[6];       // 摄像机状态x3：0；离线 1：在线，2：延时严重
    uint8_t main_ip;         // 主网络状态：0：离线 1：在线，2：延时严重
    uint8_t main_sub_ip;     // 主网络状态sub: 0：离线 1：在线，2：延时严重
    uint8_t ping_status;     // ping结束标志位
    
    uint8_t residual_c;    // 剩余电流
    energy_t total_energy; // 总用电量
    energy_t acport_energy[RELAY_NUM]; // 输出端口用电量
    
    electricity_t electricity_current; // 当前用电量
    electricity_t electricity_all; // 总用电量（保存+当前）
} data_collection_t;

extern data_collection_t sg_datacollec_t;

/* 供外部调用 */
void det_task_function(void);

void det_get_key_status_function(void);
uint8_t det_main_network_and_camera_network(void);
void det_get_temphumi_function(void);

void det_get_attitude_state_value(void);

void det_set_camera_status(uint8_t num,uint8_t status);     // 设置摄像机状态
uint8_t det_get_camera_status(uint8_t num);                 // 获取摄像机状态
void det_set_main_network_status(uint8_t status);           // 设置主网络状态
uint8_t det_get_main_network_status(void);                  // 获取主网络状态
void det_set_main_network_sub_status(uint8_t status);       // 设置主网络状态 - 2
uint8_t det_get_main_network_sub_status(void);              // 获取主网络状态 - 2

// 电能计量芯片
void det_set_total_energy_bl0910(uint8_t num,uint32_t data);
void det_set_total_energy_bl0906(uint8_t num,float data);
void det_set_total_energy_bl0942(uint8_t num,float data);
void det_set_total_energy_bl0939(uint8_t num,float data);
void det_set_total_energy_bl0972(uint8_t num,float data);
void det_set_ping_status(uint8_t status);

void *det_get_collect_data(void);
float det_get_total_energy_handler(uint8_t num);            // 获取总电量参数
float det_get_output_energy_handler(uint8_t channel,uint8_t num); // 获取通道电量参数
float det_get_inside_temp(void);                            // 获取内部温度
float det_get_inside_humi(void);                            // 获取内部湿度

void det_set_key_value(uint8_t key_id,uint8_t key_value);
uint8_t det_get_key_value(uint8_t key_id);
uint8_t det_get_miu_value(void);
uint16_t det_get_cabinet_posture(void);                      // 获取箱体姿态

#endif
