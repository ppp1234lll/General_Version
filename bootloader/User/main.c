/*
*********************************************************************************************************
*    函 数 名: 主程序模块。
*    功能说明: main.c
*    形    参: V1.0
*    返 回 值: bootloader 主程序模块
*    修改记录 :
*        版本号   日期         作者      说明
*        v1.0    2026-05-01    zb/zh     首发
*    
*********************************************************************************************************
*/
#include "main.h"


/* 本.c文件调用的函数 */
void system_setup_function(void);                                                   // 系统启动初始化
static void update_check_function(void);                                            // 更新检测
static void PrintfLogo(void);                                                       // 打印logo
static void DeviceRstReason(void);                                                  // 打印复位原因
static void led_show_control(uint8_t mode);                                         // led显示控制
static void read_boot_update_param(struct BOOT_UPDATE_PARAM *boot_update_param);    // 读取引导更新参数
static void write_boot_update_param(struct BOOT_UPDATE_PARAM *boot_update_param);   // 写入引导更新参数

/*
*********************************************************************************************************
*    函 数 名: main
*    功能说明: c程序入口
*    形    参: 无
*    返 回 值: 错误代码(无需处理)
*********************************************************************************************************
*/
int main(void)
{
    system_setup_function();
    
    bsp_InitFwdgt(FWDGT_PSC_DIV64,1000);
    bsp_Init_DWT();
    bsp_InitLed();
    bsp_InitDebug(115200);
    bsp_InitTimers(TIMER1, 1000, 6, 0);
    
    bsp_InitSPIBus();    /* 配置SPI总线 */        
    bsp_InitSFlash();    /* 初始化SPI 串行Flash */
    
    PrintfLogo();
    DeviceRstReason();
    update_check_function(); // 更新检测
//    flash_test();

    while(1)
    {
        printf("\nbootloader run error...\n");
        delay_ms(1000);
    }

}

/*
*********************************************************************************************************
*    函 数 名: system_setup_function
*    功能说明: 系统启动初始化
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void system_setup_function(void)
{
    sys_nvic_set_vector_table(FLASH_BASE, 0x0);
    systick_config();
    delay_init(240);
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);//设置系统中断优先级分组4
}


/*
*********************************************************************************************************
*    函 数 名: update_check_function
*    功能说明: 更新检测
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void update_check_function(void)
{
    struct BOOT_UPDATE_PARAM boot_update_param = {0};
    unsigned int ii;
    unsigned int read_addr = 0, write_addr = 0;
    unsigned char *app_buff = NULL;
    uint8_t count = 30;
    ////

    while(count--)
    {
        FeedFwdgt();
        delay_ms(100);
        if(PWR_TST_READ == 0) 
            break; 
    }
    count = 200;
    while(count--)
    {
        delay_ms(10);
        FeedFwdgt();
    }
    
    mymem_init(SRAMIN);    // 内存初始化

    // 读取升级参数
    read_boot_update_param(&boot_update_param);

    // 判断升级标志,直接跳转
    if( boot_update_param.is_update != 1 )
    {
        printf("\n无需升级，直接执行 main 模块 ...\n");
        FeedFwdgt();
        iap_load_app(MAIN_APP_ADDR); // 执行FLASH APP代码
        return;
    }

    // 执行更新
    printf("\n执行升级程序 ..... \n");
    boot_update_param.is_update = false; // 关闭标志
    write_boot_update_param(&boot_update_param); // 保存升级参数

    app_buff = (unsigned char *)mymalloc(SRAMIN, (boot_update_param.section_size + 64));

    printf("\n执行升级参数,section_count: %u, section_size: %u\n", boot_update_param.section_count, boot_update_param.section_size);
    
    // 写入BIN文件
    for(ii = 0; ii < boot_update_param.section_count; ii++)
    {
        FeedFwdgt();
        led_show_control(ii % 256); // led灯效果

        // 读一块
        read_addr = UPDATA_SPIFLASH_ADDR + (ii * boot_update_param.section_size);
        sf_ReadBuffer(app_buff, read_addr, boot_update_param.section_size);

        // 写入一块
        write_addr = MAIN_APP_ADDR + (ii * boot_update_param.section_size);
        iap_write_appbin(write_addr, app_buff, boot_update_param.section_size);
    }  
    
    myfree(SRAMIN, (void *)app_buff);

    FeedFwdgt();

    printf("\n升级完毕！跳转 ...\n");
    iap_load_app(MAIN_APP_ADDR); // 执行FLASH APP代码
}

/*
*********************************************************************************************************
*    函 数 名: PrintfLogo
*    功能说明: 打印例程名称和例程发布日期, 接上串口线后，打开PC机的超级终端软件可以观察结果
*    形    参: 无
*    返 回 值: 
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
    printf("* \r\n");    /* 打印一行空格 */
}

/*
*********************************************************************************************************
*    函 数 名: DeviceRstReason
*    功能说明: 判断硬件重启原因
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void DeviceRstReason(void)
{
    if( SET == rcu_flag_get( RCU_FLAG_PORRST) )
    {
        printf("PORRST:Power reset flag\n");
    }
    if( SET == rcu_flag_get( RCU_FLAG_BORRST) )
    {
        printf("BORRST:BOR reset flags\n");
    }
    if( SET == rcu_flag_get(RCU_FLAG_SWRST) )
    {
        printf("SWRST:Software reset flag\n");
    }
    if( SET == rcu_flag_get(RCU_FLAG_FWDGTRST) )
    {
        printf("FWDGTRST:Forward watchdog reset flag\n");
    }
    if( SET == rcu_flag_get(RCU_FLAG_WWDGTRST) )
    {
        printf("WWDGTRST:Window watchdog reset flag\n");
    }
    if( SET == rcu_flag_get(RCU_FLAG_EPRST))
    {
        printf("EPRST:external PIN reset flag\n");
    }
    rcu_all_reset_flag_clear() ;
}

/*
*********************************************************************************************************
*    函 数 名: led_show_control
*    功能说明: LED灯光控制，显示更新进度
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void led_show_control(uint8_t mode)
{
    static uint8_t flag = 0;
    uint8_t num = mode % 100;
    if(num<50 && flag == 0) 
    {
        flag = 1;
        led_all_on();
    } 
    else if(num >=50 && flag == 1)
    {
        flag = 0;
        led_all_off();
    }
}

/*
*********************************************************************************************************
*    函 数 名: read_boot_update_param
*    功能说明: 读取更新信息
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void read_boot_update_param(struct BOOT_UPDATE_PARAM *boot_update_param)
{
    sf_ReadBuffer((uint8_t*)boot_update_param, UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM));
}

/*
*********************************************************************************************************
*    函 数 名: write_boot_update_param
*    功能说明: 保存升级参数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void write_boot_update_param(struct BOOT_UPDATE_PARAM *boot_update_param)
{
    if(sf_WriteBuffer((uint8_t *)boot_update_param, UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM)) == 0)
    {
        printf("写串行Flash出错！\r\n");
    }
}

/****************************************** END OF FILE **********************************************/
