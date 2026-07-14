/*
*********************************************************************************************************
*    函 数 名: SPI总线驱动
*    功能说明: bsp_spi_bus.h
*    形    参: V1.3
*    返 回 值: 头文件
*********************************************************************************************************
*/

#ifndef __BSP_SPI_BUS_H
#define __BSP_SPI_BUS_H

#include "./SYSTEM/sys/sys.h"

/* 重定义下SPI SCK时钟，方便移植 */
// APB1 peripherals clock source: 60 MHz max
#define SPI_BAUDRATEPRESCALER_30M       SPI_PSC_2            /* 30MHz */
#define SPI_BAUDRATEPRESCALER_15M       SPI_PSC_4            /* 15M */
#define SPI_BAUDRATEPRESCALER_7_5M      SPI_PSC_8            /* 7.5M */
#define SPI_BAUDRATEPRESCALER_3_75M     SPI_PSC_16            /* 3.75M */
#define SPI_BAUDRATEPRESCALER_1_875M    SPI_PSC_32            /* 1.875M */
#define SPI_BAUDRATEPRESCALER_937_5K    SPI_PSC_64            /* 937.5K */
#define SPI_BAUDRATEPRESCALER_468_75K   SPI_PSC_128            /* 468.75K */
#define SPI_BAUDRATEPRESCALER_234_375K  SPI_PSC_256            /* 234.375K */

#define    SPI_BUFFER_SIZE        (4 * 1024)                /* SPI缓冲区大小 */

extern uint8_t g_spiTxBuf[SPI_BUFFER_SIZE];
extern uint8_t g_spiRxBuf[SPI_BUFFER_SIZE];
extern uint32_t g_spiLen;

extern uint8_t g_spi_busy;

//
void bsp_InitSPIBus(void);
void bsp_InitSPIGPIO(void);
void bsp_InitSPIDMA(void);
void bsp_InitSPIParam(uint32_t _BaudRatePrescaler, uint32_t _CLKPolarity_Phase);

uint8_t bsp_spiTransfer_Byte(uint8_t TxData);
void bsp_spiTransfer(void);

void bsp_SpiBusEnter(void);
void bsp_SpiBusExit(void);
uint8_t bsp_SpiBusBusy(void);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
