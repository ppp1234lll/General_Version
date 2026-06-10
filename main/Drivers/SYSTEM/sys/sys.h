/**
 ****************************************************************************************************
 * @file        sys.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-4-20
 * @brief       系统初始化代码(包括时钟配置/中断管理/GPIO设置等)
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 阿波罗 F429开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 第一次发布
 * V1.0 20220420

 ****************************************************************************************************
 */

#ifndef _SYS_H
#define _SYS_H

#include "gd32f4xx.h"
#include "core_cm4.h"

/**
 * SYS_SUPPORT_OS用于定义系统文件夹是否支持OS
 * 0,不支持OS
 * 1,支持OS
 */
#define SYS_SUPPORT_OS         1


//位带操作,实现51类似的GPIO控制功能
//具体实现思想,参考<<CM3权威指南>>第五章(87页~92页).M4同M3类似,只是寄存器地址变了.
//IO口操作宏定义
#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2)) 
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) 
//IO口地址映射
#define GPIOA_OCTL_Addr    (GPIOA_BASE+20) //0x40020014
#define GPIOB_OCTL_Addr    (GPIOB_BASE+20) //0x40020414 
#define GPIOC_OCTL_Addr    (GPIOC_BASE+20) //0x40020814 
#define GPIOD_OCTL_Addr    (GPIOD_BASE+20) //0x40020C14 
#define GPIOE_OCTL_Addr    (GPIOE_BASE+20) //0x40021014 
#define GPIOF_OCTL_Addr    (GPIOF_BASE+20) //0x40021414    
#define GPIOG_OCTL_Addr    (GPIOG_BASE+20) //0x40021814   
#define GPIOH_OCTL_Addr    (GPIOH_BASE+20) //0x40021C14    
#define GPIOI_OCTL_Addr    (GPIOI_BASE+20) //0x40022014     

#define GPIOA_ISTAT_Addr    (GPIOA_BASE+16) //0x40020010 
#define GPIOB_ISTAT_Addr    (GPIOB_BASE+16) //0x40020410 
#define GPIOC_ISTAT_Addr    (GPIOC_BASE+16) //0x40020810 
#define GPIOD_ISTAT_Addr    (GPIOD_BASE+16) //0x40020C10 
#define GPIOE_ISTAT_Addr    (GPIOE_BASE+16) //0x40021010 
#define GPIOF_ISTAT_Addr    (GPIOF_BASE+16) //0x40021410 
#define GPIOG_ISTAT_Addr    (GPIOG_BASE+16) //0x40021810 
#define GPIOH_ISTAT_Addr    (GPIOH_BASE+16) //0x40021C10 
#define GPIOI_ISTAT_Addr    (GPIOI_BASE+16) //0x40022010 
    
//IO口操作,只对单一的IO口!
//确保n的值小于16!
#define PAout(n)   BIT_ADDR(GPIOA_OCTL_Addr,n)  //输出 
#define PAin(n)    BIT_ADDR(GPIOA_ISTAT_Addr,n)  //输入 

#define PBout(n)   BIT_ADDR(GPIOB_OCTL_Addr,n)  //输出 
#define PBin(n)    BIT_ADDR(GPIOB_ISTAT_Addr,n)  //输入 

#define PCout(n)   BIT_ADDR(GPIOC_OCTL_Addr,n)  //输出 
#define PCin(n)    BIT_ADDR(GPIOC_ISTAT_Addr,n)  //输入 

#define PDout(n)   BIT_ADDR(GPIOD_OCTL_Addr,n)  //输出 
#define PDin(n)    BIT_ADDR(GPIOD_ISTAT_Addr,n)  //输入 

#define PEout(n)   BIT_ADDR(GPIOE_OCTL_Addr,n)  //输出 
#define PEin(n)    BIT_ADDR(GPIOE_ISTAT_Addr,n)  //输入

#define PFout(n)   BIT_ADDR(GPIOF_OCTL_Addr,n)  //输出 
#define PFin(n)    BIT_ADDR(GPIOF_ISTAT_Addr,n)  //输入

#define PGout(n)   BIT_ADDR(GPIOG_OCTL_Addr,n)  //输出 
#define PGin(n)    BIT_ADDR(GPIOG_ISTAT_Addr,n)  //输入

#define PHout(n)   BIT_ADDR(GPIOH_OCTL_Addr,n)  //输出 
#define PHin(n)    BIT_ADDR(GPIOH_ISTAT_Addr,n)  //输入

#define PIout(n)   BIT_ADDR(GPIOI_OCTL_Addr,n)  //输出 
#define PIin(n)    BIT_ADDR(GPIOI_ISTAT_Addr,n)  //输入

/* 常用参数 */

typedef float  fp32;
typedef double fp64;

union i_c{
    unsigned int  i;
    unsigned char c[4];
    };

#define FAILED  ((TestStatus)0)
#define PASSED  ((TestStatus)1)
typedef uint32_t TestStatus;

/*函数声明*******************************************************************************************/

void sys_nvic_set_vector_table(uint32_t baseaddr, uint32_t offset);                         /* 设置中断偏移量 */
void sys_standby(void);                                                                     /* 进入待机模式 */
void sys_soft_reset(void);                                                                  /* 系统软复位 */


/* 以下为汇编函数 */
void sys_wfi_set(void);             /* 执行WFI指令 */
void sys_intx_disable(void);        /* 关闭所有中断 */
void sys_intx_enable(void);         /* 开启所有中断 */
void sys_msr_msp(uint32_t addr);    /* 设置栈顶地址 */


void Error_Handler(char *file, uint32_t line);
#endif

