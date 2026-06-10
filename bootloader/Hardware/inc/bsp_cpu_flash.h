/*
*********************************************************************************************************
*
*	模块名称 : cpu内部falsh操作模块
*	文件名称 : bsp_cpu_flash.h
*	版    本 : V1.0
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_CPU_FLASH_H_
#define _BSP_CPU_FLASH_H_

#include "./SYSTEM/sys/sys.h"

#define CPU_FLASH_BASE_ADDR      (uint32_t)(FLASH_BASE)		/* 0x08000000 */
#define CPU_FLASH_END_ADDR       (uint32_t)(0x080FFFFF)

#define CPU_FLASH_SIZE       	(1 * 1024 * 1024)	/* FLASH总容量 */
#define CPU_FLASH_SECTOR_SIZE	(128 * 1024)		/* 扇区大小，字节 */

/* base address of the FMC sectors */
#define ADDR_FMC_SECTOR_0     ((uint32_t)0x08000000) /*!< base address of sector 0, 16 kbytes */
#define ADDR_FMC_SECTOR_1     ((uint32_t)0x08004000) /*!< base address of sector 1, 16 kbytes */
#define ADDR_FMC_SECTOR_2     ((uint32_t)0x08008000) /*!< base address of sector 2, 16 kbytes */
#define ADDR_FMC_SECTOR_3     ((uint32_t)0x0800C000) /*!< base address of sector 3, 16 kbytes */
#define ADDR_FMC_SECTOR_4     ((uint32_t)0x08010000) /*!< base address of sector 4, 64 kbytes */
#define ADDR_FMC_SECTOR_5     ((uint32_t)0x08020000) /*!< base address of sector 5, 128 kbytes */
#define ADDR_FMC_SECTOR_6     ((uint32_t)0x08040000) /*!< base address of sector 6, 128 kbytes */
#define ADDR_FMC_SECTOR_7     ((uint32_t)0x08060000) /*!< base address of sector 7, 128 kbytes */
#define ADDR_FMC_SECTOR_8     ((uint32_t)0x08080000) /*!< base address of sector 8, 128 kbytes */
#define ADDR_FMC_SECTOR_9     ((uint32_t)0x080A0000) /*!< base address of sector 9, 128 kbytes */
#define ADDR_FMC_SECTOR_10    ((uint32_t)0x080C0000) /*!< base address of sector 10, 128 kbytes */
#define ADDR_FMC_SECTOR_11    ((uint32_t)0x080E0000) /*!< base address of sector 11, 128 kbytes */


#define FLASH_IS_EQU		0   /* Flash内容和待写入的数据相等，不需要擦除和写操作 */
#define FLASH_REQ_WRITE		1	/* Flash不需要擦除，直接写 */
#define FLASH_REQ_ERASE		2	/* Flash需要先擦除,再写 */
#define FLASH_PARAM_ERR		3	/* 函数参数错误 */

uint8_t bsp_ReadCpuFlash(uint32_t _ulFlashAddr, uint8_t *_ucpDst, uint32_t _ulSize);
uint8_t bsp_WriteCpuFlash(uint32_t _ulFlashAddr, uint8_t *_ucpSrc, uint32_t _ulSize);
uint8_t bsp_CmpCpuFlash(uint32_t _ulFlashAddr, uint8_t *_ucpBuf, uint32_t _ulSize);
uint8_t bsp_WriteCpuFlash_Save(uint32_t _ulReadAddr, uint32_t _ulWriteAddr, uint8_t *_ucpBuf, uint32_t _ulSize);
void bsp_EraseCpuFlash(uint32_t _ulFlashAddr);

void flash_test(void);
#endif

/****************************************** END OF FILE **********************************************/

