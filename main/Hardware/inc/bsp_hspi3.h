#ifndef __BSP_HSPI3_H
#define __BSP_HSPI3_H

#include "./SYSTEM/sys/sys.h"
                                    
/* 提供给其他C文件调用的函数 */
void bsp_InitHSPI4(void);			 		   // 初始化SPI口
uint8_t HSPI4_ReadWriteByte(uint8_t TxData);
void HSPI4_Write_Multi_Byte(uint8_t *buff, uint16_t len);
uint8_t HSPI4_ReadByte(uint8_t data);
void HSPI4_test(void);

#endif


