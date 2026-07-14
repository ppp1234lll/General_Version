#ifndef __BSP_SSPI2_H
#define __BSP_SSPI2_H

#include "./SYSTEM/sys/sys.h"
                                    
/* 提供给其他C文件调用的函数 */
void bsp_InitSSPI2(void);			 		   // 初始化SPI口

void SSPI2_Write_Byte(uint8_t TxData);
void SSPI2_Write_Buffer(uint8_t *buff, uint16_t len);
uint8_t SSPI2_Read_Byte(void);

void SSPI2_test(void);

#endif

