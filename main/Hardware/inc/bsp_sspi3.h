#ifndef __BSP_SSPI3_H
#define __BSP_SSPI3_H

#include "./SYSTEM/sys/sys.h"
                                    
/* 提供给其他C文件调用的函数 */
void bsp_InitSSPI3(void); 

void SSPI3_Write_Byte(uint8_t TxData);
void SSPI3_Write_Buffer(uint8_t *buff, uint16_t len);
uint8_t SSPI3_Read_Byte(void);

void SSPI3_test(void);

#endif

