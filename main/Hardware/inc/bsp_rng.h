#ifndef __BSP_RNG_H
#define __BSP_RNG_H     

#include "./SYSTEM/sys/sys.h"

/* 提供给其他C文件调用的函数 */
uint8_t  bsp_InitRNG(void);            //RNG初始化 
uint32_t RNG_Get_RandomNum(void);//得到随机数
int RNG_Get_RandomRange(int min,int max);//生成[min,max]范围的随机数

void rng_test(void);

#endif

/******************************************  (END OF FILE) **********************************************/




