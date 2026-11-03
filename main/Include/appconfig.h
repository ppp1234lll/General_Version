/*
*********************************************************************************************************
*    函 数 名: 头文件汇总
*    功能说明: appconfig.h
*    形    参: V1.0
*    返 回 值: 当前使用头文件汇总
*    修改记录 :
*        版本号    日期        作者     说明
*        V1.0    2025-10-14  fengniao   首次发布
*    Copyright (C), 2015-2020,
*********************************************************************************************************
*/
#ifndef _APPCONFIG_H_
#define _APPCONFIG_H_

/*
*********************************************************************************************************
*                                       功能选择
*********************************************************************************************************
*/
#define configUSE_DEVICE_TYPE           1 // 设备类型
#define configUSE_DEVIDE_TYPE_DEC       0 // 设备类型2(TID为10进制)

#define configUSE_EXT_LED               1 // 外接指示灯
#define configUSE_EXT_FAN               1 // 外接风扇
#define configUSE_EXT_GPS               1 // 北斗定位
#define configUSE_KEY_WATER             1 // 水浸检测
#define configUSE_KEY_SPD               1 // 防雷检测
#define configUSE_KEY_MCB               1 // 空气开关检测
#define configUSE_LN_PGND               1 // 零火反接 缺地检测

#define configUSE_RS485                 1 // 485通信

#define configUSE_AHT20                 1 // 温湿度检测
#define configUSE_TILT                  1 // 倾斜度检测
#define configUSE_CHIP_ORIENTATION      0 // 芯片放置方向(0：水平，1：垂直)

#define configUSE_EXT_SIM               0 // 使用SIM卡检测

#define configUSE_RS485_TRANSPARENT     0 // 使用485透传
#define configUSE_SUN_POWER             1 // 使用太阳能板

#if (configUSE_KEY_MCB == 1) // 只能选择一种检测方式
    #define configUSE_KEY_MCB_COUPLING    0  // 使用光耦检测
    #define configUSE_KEY_MCB_METRIC_CHIP 1  // 使用计量芯片
#endif

/*  
*********************************************************************************************************
*                                       DEBUG调试
*********************************************************************************************************
*/



#endif
/*****************************************  (END OF FILE) *********************************************/

