#ifndef __BSP_HSPI2_H
#define __BSP_HSPI2_H

#include "./SYSTEM/sys/sys.h"
                                    
/* 提供给其他C文件调用的函数 */
void bsp_InitHSPI2(void);                        // 初始化SPI口
uint8_t HSPI2_Transmit_Byte(uint8_t TxData);
void HSPI2_Send_Buffer(uint8_t *buff, uint16_t len);

#endif


