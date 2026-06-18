#ifndef _BSP_FAN_H_
#define _BSP_FAN_H_

#include "./SYSTEM/sys/sys.h"

typedef enum
{
    FAN_OFF = 0, // 关闭
    FAN_ON  = 1 // 打开
} FAN_STATUS;


/* 提供给其他C文件调用的函数 */
void bsp_InitFan(void); // 初始化函数
void fan_control( FAN_STATUS state);
void fan_test(void);
    
#endif

/******************************************  (END OF FILE) **********************************************/

