#ifndef _LIS3DH_SPI_H_
#define _LIS3DH_SPI_H_

#include "sys.h"
												  
/* 函数声明 */
void LIS3DH_SPI_INIT(void);			 			// 初始化SPI2口
uint8_t LIS3DH_SPI_ReadWriteByte(uint8_t TxData); // SPI2总线读写一个字节


#endif
