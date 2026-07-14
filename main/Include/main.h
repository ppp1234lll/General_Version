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
#ifndef _MAIN_H_
#define _MAIN_H_

/*
*********************************************************************************************************
*                                        程序版本号
*********************************************************************************************************
*/
#define SOFT_NO_STR    "1.0.1.20260501"  // 软件版本号
#define HARD_NO_STR    "FN-ZJTY-PY0"     // 硬件名称

/*
*********************************************************************************************************
*                                      设备版本号
*********************************************************************************************************
*/
#define COM_DATA_VERSION        0x11
/*
*********************************************************************************************************
*                                      设备类型
*********************************************************************************************************
*/
// 老筑基版（8157）      0x0100
// 简约版               0x0200
// 新筑基版(新8157)      0x0300
// YX市电               0x0400
// YX太阳能             0x0500
// 红绿灯               0x0600
// 箱门监测             0x0700

#define DEVICE_TYPE        0x0400

/*
*********************************************************************************************************
*                                         标准库
*********************************************************************************************************
*/
#include  <stdarg.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <math.h>
#include  <time.h>
#include  <stddef.h>
#include  <string.h>

#include "appconfig.h"
/*
*********************************************************************************************************
*                                         中间库
*********************************************************************************************************
*/
//#include "SEGGER_RTT.h"
#include "./cjson/cJSON.h"
#include "./cjson/mycJSON.h"
#include "./tool/inc/convert.h"
#include "./tool/inc/crc.h"
#include "./tool/inc/md5.h"
#include "./tool/inc/SHA1.h"

//#include "cmox_crypto.h"
//#include <cm_backtrace.h>

// 按键库
#include "./easybutton/ebtn_APP.h"
#include "./easybutton/ebtn_APP_Keys.h"

// 环形缓冲区库
#include "./lwrb/lwrb.h"

/*
*********************************************************************************************************
*                                         驱动库
*********************************************************************************************************
*/
#include  "bsp.h"
#include "./Driver/inc/rs485.h"
#include "./Driver/inc/BL0939.h"
#include "./Driver/inc/BL0972.h"
#include "./Driver/inc/BL0906.h"
#include "./Driver/inc/BL0910.h"

#include "./Driver/inc/GPRS.h"
#include "./Driver/inc/aht20.h"

#include "./Driver/inc/ATGM336H.h"

/*
*********************************************************************************************************
*                                     FreeRTOS 库
*********************************************************************************************************
*/
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

/*
*********************************************************************************************************
*                                        内存管理
*********************************************************************************************************
*/
#include "./malloc/malloc.h"

/*
*********************************************************************************************************
*                                         LWIP网络
*********************************************************************************************************
*/
// ping相关
#include "./ping/lwip_ping.h"
#include "./ping/lwip_ping_multi.h"
#include "./ping/lwip_ping_remote.h"
// tcp相关
#include "./tcp_client/tcp_client.h"
// web服务器相关
#include "./web_server/httpd.h"
#include "./web_server/httpd_cgi_ssi.h"
// onvif相关
#include "./onvif/onvif.h"
#include "./onvif/onvif_tcp.h"
#include "./onvif/onvif_agree.h"
#include "./onvif/onvif_deal.h"
#include "./onvif/onvif_digest.h"
#include "./onvif/onvif_prefixlen.h"
// 端口扫描相关
#include "./port_scan/port_scan.h"
// rtsp相关
#include "./rtsp_client/rtsp_task.h"
// snmp相关
#include "./snmp_udp/snmp_udp.h"
// udp多播相关
#include "./udp_multicast/udp_multicast.h"

// 通用头文件
#include "lwip_comm.h"
#include "ethernetif.h"
#include "lwipopts.h"

#include <lwip/sockets.h>
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/tcpip.h"
#include "lwip/icmp.h"
#include "lwip/igmp.h"
#include "lwip/raw.h"
#include "lwip/dns.h"
/*
*********************************************************************************************************
*                                         OTA升级
*********************************************************************************************************
*/
#include "./Update/inc/update.h"
#include "./Update/inc/update_http.h"
/*
*********************************************************************************************************
*                                         文件上传
*********************************************************************************************************
*/
#include "./Upload/inc/upload.h"
#include "./Upload/inc/upload_http.h"

/*
*********************************************************************************************************
*                                          APP
*********************************************************************************************************
*/
//线程
#include "./Task/inc/alarm.h"
#include "./Task/inc/app.h"
#include "./Task/inc/det.h"
#include "./Task/inc/eth.h"
#include "./Task/inc/gsm.h"
#include "./Task/inc/print.h"
#include "./Task/inc/dtu.h"
#include "./Task/inc/gprs_rx.h"
#include "./Task/inc/send.h"

// 用户相关
#include "./User/inc/start.h"
#include "./User/inc/com.h"
#include "./User/inc/save.h"
#include "./User/inc/error.h"
#include "./User/inc/log.h"

/*
*********************************************************************************************************
*                                          文件系统
*********************************************************************************************************
*/
#include "./littlefs/lfs_port.h"
#include "./littlefs/lfs.h"

/*
*********************************************************************************************************
*                                           宏定义
*********************************************************************************************************
*/
#define PRIORITY_CONNECTION_MODE 1 // 0-有线优先连接 1-无线优先连接

#if (PRIORITY_CONNECTION_MODE == 1) 
#define WIRELESS_PRIORITY_CONNECTION // 无线优先连接
#else
#define WIRED_PRIORITY_CONNECTION    // 有线优先连接
#endif

/*
*********************************************************************************************************
*                                          FLASH存储位置
*********************************************************************************************************
*/
#define DEVICE_FLASH_STORE        ADDR_FMC_SECTOR_2
#define DEVICE_ID_ADDR            DEVICE_FLASH_STORE
#define DEVICE_MAC_ADDR           DEVICE_FLASH_STORE + 16
#define DEVICE_ELECTRICITY_ADDR   DEVICE_FLASH_STORE + 32


/*
*********************************************************************************************************
*                                           OTA升级参数
*********************************************************************************************************
*/

#define EXTERNAL_FLASH_SIZE        (g_tSF.TotalSize)
#define UPDATE_CHUNK_SIZE          (1024 + 2)
#define UPDATA_SPIFLASH_ADDR       (EXTERNAL_FLASH_SIZE - (1 * 1024 * 1024))  // 芯片最后1M空间作为升级文件存储地址
#define UPDATA_PARAM_ADDR          (EXTERNAL_FLASH_SIZE - (1 * 1024))          // 芯片最后1K空间作为升级参数存储地址


#endif
/*****************************************  (END OF FILE) *********************************************/
