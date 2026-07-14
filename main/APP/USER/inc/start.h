#ifndef _START_H_
#define _START_H_

#include "./SYSTEM/sys/sys.h"

/* 参数 */
typedef struct
{
    uint32_t id[3];
} ChipID_t;

/* 提供给其他C文件调用的函数 */
void PVD_Init(void);  // 初始化PVD 
void start_bsp_init(void);  // 硬件初始化函数
void start_get_device_id(void);  // 获取本机ID
void start_get_device_id_hex(uint32_t *id);
void start_get_device_id_str(uint8_t *str); // 获取本机ID

void start_task_create(void);  // 创建所有的任务

#endif
/******************************************  (END OF FILE) **********************************************/
