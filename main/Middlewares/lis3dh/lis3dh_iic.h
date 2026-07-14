#ifndef _LIS3DHIIC_H
#define _LIS3DHIIC_H

#include "./SYSTEM/sys/sys.h"
#include <stdbool.h> 

//IIC所有操作函数
void LIS3DH_IIC_Init(void);                //初始化IIC的IO口                 

bool HAL_IIC_EMU_Read(uint8_t Addr,uint8_t *pBuf,uint32_t Len);
bool HAL_IIC_EMU_Write(uint8_t Addr,uint8_t *pBuf,uint32_t Len);

#endif


