#ifndef _BSP_RELAY_H_
#define _BSP_RELAY_H_

#include "./SYSTEM/sys/sys.h"

typedef enum
{
    RELAY_1 = 0 , // 继电器 1
    RELAY_2     , // 继电器 2
    RELAY_3     , // 继电器 3
    RELAY_4     , // 继电器 4
    RELAY_5     , // 继电器 5
    RELAY_6     , // 继电器 6
    RELAY_7     , // 继电器 7
    RELAY_8     , // 继电器 8
    RELAY_NUM
} RELAY_DEV;

typedef enum
{
    RELAY_OFF = 0, // 关闭
    RELAY_ON  = 1, // 打开
} RELAY_STATUS;

/* 供外部调用的函数声明 */
void bsp_InitRelay(void); // 初始化函数
void relay_control(RELAY_DEV dev, RELAY_STATUS state);
uint8_t relay_get_status(RELAY_DEV dev);
void relay_test(void);
    
#endif
/******************************************  (END OF FILE) **********************************************/

