#ifndef __BSP_HSPI1_H
#define __BSP_HSPI1_H

#include "./SYSTEM/sys/sys.h"
                                    
/* 提供给其他C文件调用的函数 */
void bsp_InitHSPI1(void);                        // 初始化SPI口
uint8_t HSPI1_Transmit_Byte(uint8_t TxData);
void HSPI1_Send_Buffer(uint8_t *buff, uint16_t len);

#endif


