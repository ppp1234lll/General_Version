/*
*********************************************************************************************************
*
*    模块名称 : 头文件汇总
*    文件名称 : BSP.h
*    版    本 : V1.0
*    说    明 : 当前使用头文件汇总
*
*    修改记录 :
*        版本号    日期        作者     说明
*        V1.0    2015-08-02  Eric2013   首次发布
*
*    Copyright (C), 2015-2020,
*
*********************************************************************************************************
*/
#ifndef _BSP_H_
#define _BSP_H_

#include  <stdio.h>
#include  <string.h>

//#define Enable_EventRecorder // 选择使用EVR
//#define Enable_RTTViewer   // 选择使用RTT
#define Enable_USART   // 选择使用UART

#ifdef Enable_EventRecorder
#include "EventRecorder.h"

#elif defined Enable_RTTViewer
#include "SEGGER_RTT.h"
#define printf(...) do { SEGGER_RTT_SetTerminal(0);   \
                        SEGGER_RTT_printf(0, __VA_ARGS__); \
                        }while(0)

#elif defined Enable_USART

#endif

#include "systick.h"

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"

// 内部模块
#include "bsp_core_dwt.h"
#include "bsp_cpu_flash.h"
#include "bsp_fwdgt.h"
#include "bsp_rtc.h"
#include "bsp_rng.h"
#include "bsp_timers.h"

// 外设模块
#include "bsp_led.h"
#include "bsp_relay.h"
#include "bsp_key.h"
#include "bsp_fan.h"

// 串口
#include "bsp_debug.h"
#include "bsp_usart0.h"
#include "bsp_usart1.h"
#include "bsp_usart2.h"
#include "bsp_uart3.h"
#include "bsp_uart4.h"
#include "bsp_usart5.h"
#include "bsp_uart6.h"
#include "bsp_uart7.h"


// SPI
#include "bsp_spi_bus.h"
#include "bsp_spi_flash.h"

#include "bsp_hspi1.h"
#include "bsp_hspi2.h"
#include "bsp_hspi3.h"

#include "bsp_sspi1.h"
#include "bsp_sspi2.h"
#include "bsp_sspi3.h"


#include "bsp_enet.h"

#include "bsp_siic.h"

#include "hal_lis3dh.h"
#include "lis3dh_driver.h" 
#include "lis3dh_iic.h"
#include "bsp_adc.h"
#include "bsp_hspi3.h"

#endif

