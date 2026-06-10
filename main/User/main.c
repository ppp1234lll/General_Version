/**
 ****************************************************************************************************
 * @file        main.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-08-01
 * @brief       lwIP HTTPS 实验
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
**/

#include "appconfig.h"
#include "freertos_demo.h"

/* 本.c文件调用的函数 */
static void system_setup_function(void);
static void PrintfLogo(void);

/*
*********************************************************************************************************
*	函 数 名: main
*	功能说明: c程序入口
*	形    参: 无
*	返 回 值: 错误代码(无需处理)
*********************************************************************************************************
*/
int main(void)
{
    system_setup_function();                    /* 初始化HAL库 */
    bsp_InitUsart5(115200);
//    PrintfLogo();

//	usart5_test();
	start_bsp_init();
	start_task_create();

//    while(1)
//    {
//        printf("lwIP HTTPS Test\n");
//        delay_ms(1000);
//    }
//    freertos_demo();
}

/*
*********************************************************************************************************
*	函 数 名: system_setup_function
*	功能说明: 系统启动初始化
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void system_setup_function(void)
{
    sys_nvic_set_vector_table(FLASH_BASE, 0x10000);
    systick_config();
    delay_init(240);
	nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);//设置系统中断优先级分组4
}

/*
*********************************************************************************************************
*	函 数 名: PrintfLogo
*	功能说明: 打印例程名称和例程发布日期, 接上串口线后，打开PC机的超级终端软件可以观察结果
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void PrintfLogo(void)
{
	/* 检测CPU ID */
	{
		/* 参考手册：
			32.6.1 MCU device ID code
			33.1 Unique device ID register (96 bits)
		*/
		uint32_t CPU_Sn0, CPU_Sn1, CPU_Sn2;

		CPU_Sn0 = *(__IO uint32_t*)(0x1FFF7A10);
		CPU_Sn1 = *(__IO uint32_t*)(0x1FFF7A10 + 4);
		CPU_Sn2 = *(__IO uint32_t*)(0x1FFF7A10 + 8);

		printf("\r\nCPU : GD32F450VG, LQFP100, 240MHz, 主频: %dMHz\r\n", SystemCoreClock / 1000000);
		printf("UID = %08X %08X %08X\n\r", CPU_Sn2, CPU_Sn1, CPU_Sn0);
	}

	printf("\n\r");
	/* 检测CPU 配置时钟 */
	{
		uint32_t sysclk, hclk, pclk1, pclk2;

        sysclk = rcu_clock_freq_get(CK_SYS);
        hclk   = rcu_clock_freq_get(CK_AHB);
        pclk1  = rcu_clock_freq_get(CK_APB1);
        pclk2  = rcu_clock_freq_get(CK_APB2);

        printf("SYSCLK (系统时钟)   : %d Hz\r\n", sysclk);
        printf("SYSCLK (系统时钟)   : %d MHz\r\n", sysclk/1000000);
        printf("AHB    (HCLK)       : %d Hz\r\n", hclk);
        printf("APB1   (PCLK1)      : %d Hz\r\n", pclk1);
        printf("APB2   (PCLK2)      : %d Hz\r\n", pclk2);
        printf("===================================\r\n\r\n");
	}

	/* 打印GD固件库版本，这定义宏在system_gd32f4xx.h文件中 */
#ifdef __FIRMWARE_VERSION_DEFINE
    uint32_t fw_ver = 0;
    fw_ver = gd32f4xx_firmware_version_get();
	printf("\r\nGD32F4xx series firmware version: V%d.%d.%d", (uint8_t)(fw_ver >> 24), (uint8_t)(fw_ver >> 16), (uint8_t)(fw_ver >> 8));
#endif
    printf("* \r\n");	/* 打印一行空格 */
}



