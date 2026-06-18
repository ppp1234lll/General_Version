/*
*********************************************************************************************************
*    函 数 名: SPI总线驱动
*    功能说明: bsp_hspi0.h
*    形    参: V1.3
*    返 回 值: 头文件
*********************************************************************************************************
*/

#ifndef __BSP_HSPI0_H
#define __BSP_HSPI0_H

#include "./SYSTEM/sys/sys.h"

/* 重定义下SPI0 SCK时钟，方便移植 */
// APB1 peripherals clock source: 60 MHz max
#define HSPI0_BAUDRATEPRESCALER_30M       SPI_PSC_2            /* 30MHz */
#define HSPI0_BAUDRATEPRESCALER_15M       SPI_PSC_4            /* 15M */
#define HSPI0_BAUDRATEPRESCALER_7_5M      SPI_PSC_8            /* 7.5M */
#define HSPI0_BAUDRATEPRESCALER_3_75M     SPI_PSC_16            /* 3.75M */
#define HSPI0_BAUDRATEPRESCALER_1_875M    SPI_PSC_32            /* 1.875M */
#define HSPI0_BAUDRATEPRESCALER_937_5K    SPI_PSC_64            /* 937.5K */
#define HSPI0_BAUDRATEPRESCALER_468_75K   SPI_PSC_128            /* 468.75K */
#define HSPI0_BAUDRATEPRESCALER_234_375K  SPI_PSC_256            /* 234.375K */

#define HSPI0_BUFFER_SIZE        (4 * 1024)                /* SPI0缓冲区大小 */


void bsp_InitHSPI0(void);
void bsp_InitHSPI0_GPIO(void);
void bsp_InitHSPI0_DMA(void);
void bsp_InitHSPI0_Param(uint32_t _BaudRatePrescaler, uint32_t _CLKPolarity_Phase);

uint8_t bsp_hspi0Transfer_Byte(uint8_t TxData);
void bsp_hspi0Transfer(void);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
