/*
*********************************************************************************************************
*    函 数 名: 串行Flash读写演示程序。
*    功能说明: demo_spi_flash.c
*    形    参: V1.0
*    返 回 值: 安富莱STM32-V7开发板标配的串行Flash型号为W25Q64, 8M字节
*    修改记录 :
*        版本号  日期        作者     说明
*        V1.0    2020-03-14 armfly  正式发布
*    Copyright (C), 2020-2030, 安富莱电子 www.armfly.com
*********************************************************************************************************
*/
#include "demo_spi_flash.h"
#include "bsp.h"

#define TEST_ADDR        0            /* 读写测试地址 */
#define TEST_SIZE        4096        /* 读写测试数据大小 */

/* 仅允许本文件内调用的函数声明 */
static void sfDispMenu(void);
static void sfReadTest(void);
static void sfWriteTest(void);
static void sfErase(void);
static void sfViewData(uint32_t _uiAddr);
static void sfWriteAll(uint8_t _ch);
static void sfTestReadSpeed(void);

static uint8_t buf[TEST_SIZE];
extern uint8_t g_U1RxBuffer[2048];
/*
*********************************************************************************************************
*    函 数 名: DemoSpiFlash
*    功能说明: 串行EEPROM读写例程
*    形    参: 无
*    返 回 值: 
*********************************************************************************************************
*/
void DemoSpiFlash(void)
{
    char ch  ;
    uint32_t uiReadPageNo = 0;
    
    /* 检测串行Flash OK */
    printf("Detected serial Flash, ID = %08X, Model: %s \r\n", g_tSF.ChipID , g_tSF.ChipName);
    printf("    Capacity: %dMB, Sector size: %d bytes\r\n", g_tSF.TotalSize/(1024*1024), g_tSF.SectorSize);

    sfDispMenu();        /* 打印命令提示 */

    while(1)
    {
        ch=getchar();
        printf("Received char: %c\n",ch);
//        if (comGetChar(COM1, &cmd))    /* 从串口读入一个字符(非阻塞方式) */
        {
            switch (ch)
            {
                case '1':
                    printf("\r\n[1 - Read serial Flash, Addr:0x%X, Length:%d bytes]\r\n", TEST_ADDR, TEST_SIZE);
                    sfReadTest();    /* 读串行Flash数据，并打印出来数据内容 */
                    break;

                case '2':
                    printf("\r\n[2 - Write serial Flash, Addr:0x%X, Length:%d bytes]\r\n", TEST_ADDR, TEST_SIZE);
                    sfWriteTest();    /* 写串行Flash数据，并打印写入速度 */
                    break;

                case '3':
                    printf("\r\n[3 - Erase whole serial Flash]\r\n");
                    printf("Erasing whole Flash takes about 20s, please wait");
                    sfErase();        /* 擦除串行Flash数据，实际上就是写入全0xFF */
                    break;

                case '4':
                    printf("\r\n[4 - Write whole serial Flash, all 0x55]\r\n");
                    printf("Writing whole Flash takes about 20s, please wait");
                    sfWriteAll(0x55);/* 擦除串行Flash数据，实际上就是写入全0xFF */
                    break;

                case '5':
                    printf("\r\n[5 - Read whole serial Flash, %dMB]\r\n", g_tSF.TotalSize/(1024*1024));
                    sfTestReadSpeed(); /* 读整个串行Flash数据，测试速度 */
                    break;

                case 'z':
                case 'Z': /* 读取前1K */
                    if (uiReadPageNo > 0)
                    {
                        uiReadPageNo--;
                    }
                    else
                    {
                        printf("Already at the beginning\r\n");
                    }
                    sfViewData(uiReadPageNo * 1024);
                    break;

                case 'x':
                case 'X': /* 读取后1K */
                    if (uiReadPageNo < g_tSF.TotalSize / 1024 - 1)
                    {
                        uiReadPageNo++;
                    }
                    else
                    {
                        printf("Already at the end\r\n");
                    }
                    sfViewData(uiReadPageNo * 1024);
                    break;

                default:
                    sfDispMenu();    /* 无效命令，重新打印命令提示 */
                    break;

            }
        }
    }
    
}

/*
*********************************************************************************************************
*    函 数 名: sfReadTest
*    功能说明: 读串行Flash测试
*    形    参: 无
*    返 回 值: 
*********************************************************************************************************
*/
static void sfReadTest(void)
{
    uint16_t i;
    int32_t iTime1, iTime2;
    
    /* 起始地址 = 0， 数据长度为 256 */
    iTime1 = HAL_GetTick();    /* 记下开始时间 */
    sf_ReadBuffer(buf, TEST_ADDR, TEST_SIZE);
    iTime2 = HAL_GetTick();    /* 记下结束时间 */
    printf("Read serial Flash success, data as below:\r\n");

    /* 打印数据 */
    for (i = 0; i < TEST_SIZE; i++)
    {
        printf(" %02X", buf[i]);

        if ((i & 31) == 31)
        {
            printf("\r\n");    /* 每行显示16字节数据 */
        }
        else if ((i & 31) == 15)
        {
            printf(" - ");
        }
    }

    /* 打印读速度 */
    printf("Data length: %d bytes, Read time: %dms, Read speed: %d Bytes/s\r\n", TEST_SIZE, iTime2 - iTime1, (TEST_SIZE * 1000) / (iTime2 - iTime1));
}


/*
*********************************************************************************************************
*    函 数 名: sfTestReadSpeed
*    功能说明: 测试串行Flash读速度。读取整个串行Flash的数据，最后打印结果
*    形    参: 无
*    返 回 值: 
*********************************************************************************************************
*/
static void sfTestReadSpeed(void)
{
    uint16_t i;
    int32_t iTime1, iTime2;
    uint32_t uiAddr;

    /* 起始地址 = 0， 数据长度为 256 */
    iTime1 = HAL_GetTick();    /* 记下开始时间 */
    uiAddr = 0;
    for (i = 0; i < g_tSF.TotalSize / TEST_SIZE; i++, uiAddr += TEST_SIZE)
    {
        sf_ReadBuffer(buf, uiAddr, TEST_SIZE);
    }
    iTime2 = HAL_GetTick();    /* 记下结束时间 */

    /* 打印读速度 */
    printf("Data length: %d bytes, Read time: %dms, Read speed: %lld Bytes/s\r\n", g_tSF.TotalSize, iTime2 - iTime1, (uint64_t)g_tSF.TotalSize * 1000 / (iTime2 - iTime1));
}

/*
*********************************************************************************************************
*    函 数 名: sfWriteTest
*    功能说明: 写串行Flash测试
*    形    参: 无
*    返 回 值: 
*********************************************************************************************************
*/
static void sfWriteTest(void)
{
    uint16_t i;
    int32_t iTime1, iTime2;

    /* 填充测试缓冲区 */
    for (i = 0; i < TEST_SIZE; i++)
    {
        buf[i] = i;
    }

    /* 写EEPROM, 起始地址 = 0，数据长度为 256 */
    iTime1 = HAL_GetTick();    /* 记下开始时间 */
    if (sf_WriteBuffer(buf, TEST_ADDR, TEST_SIZE) == 0)
    {
        printf("Write serial Flash error!\r\n");
        return;
    }
    else
    {
        iTime2 = HAL_GetTick();    /* 记下结束时间 */
        printf("Write serial Flash success!\r\n");
    }


    /* 打印读速度 */
    printf("Data length: %d bytes, Write time: %dms, Write speed: %dB/s\r\n", TEST_SIZE, iTime2 - iTime1, (TEST_SIZE * 1000) / (iTime2 - iTime1));
}

/*
*********************************************************************************************************
*    函 数 名: sfWriteAll
*    功能说明: 写串行EEPROM全部数据
*    形    参: 写入的数据
*    返 回 值: 无
*********************************************************************************************************
*/
static void sfWriteAll(uint8_t _ch)
{
    uint16_t i;
    int32_t iTime1, iTime2;

    /* 填充测试缓冲区 */
    for (i = 0; i < TEST_SIZE; i++)
    {
        buf[i] = _ch;
    }

    /* 写EEPROM, 起始地址 = 0，数据长度为 256 */
    iTime1 = HAL_GetTick();    /* 记下开始时间 */
    for (i = 0; i < g_tSF.TotalSize / g_tSF.SectorSize; i++)
    {
        if (sf_WriteBuffer(buf, i * g_tSF.SectorSize, g_tSF.SectorSize) == 0)
        {
            printf("Write serial Flash error!\r\n");
            return;
        }
        printf(".");
        if (((i + 1) % 128) == 0)
        {
            printf("\r\n");
        }
    }
    iTime2 = HAL_GetTick();    /* 记下结束时间 */

    /* 打印读速度 */
    printf("Data length: %dKB, Write time: %dms, Write speed: %dB/s\r\n", g_tSF.TotalSize / 1024, iTime2 - iTime1, (g_tSF.TotalSize * 1000) / (iTime2 - iTime1));
}

/*
*********************************************************************************************************
*    函 数 名: sfErase
*    功能说明: 擦除串行Flash
*    形    参: 无
*    返 回 值: 
*********************************************************************************************************
*/
static void sfErase(void)
{
    int32_t iTime1, iTime2;

    iTime1 = HAL_GetTick();    /* 记下开始时间 */
    sf_EraseChip();
    iTime2 = HAL_GetTick();    /* 记下结束时间 */

    /* 打印读速度 */
    printf("Erase serial Flash done!, Time: %dms\r\n", iTime2 - iTime1);
    return;
}


/*
*********************************************************************************************************
*    函 数 名: sfViewData
*    功能说明: 读串行Flash并显示，每次显示1K的内容
*    形    参: 无
*    返 回 值: 
*********************************************************************************************************
*/
static void sfViewData(uint32_t _uiAddr)
{
    uint16_t i;

    sf_ReadBuffer(buf, _uiAddr,  1024);        /* 读数据 */
    printf("Addr: 0x%08X; Data length = 1024\r\n", _uiAddr);

    /* 打印数据 */
    for (i = 0; i < 1024; i++)
    {
        printf(" %02X", buf[i]);

        if ((i & 31) == 31)
        {
            printf("\r\n");    /* 每行显示16字节数据 */
        }
        else if ((i & 31) == 15)
        {
            printf(" - ");
        }
    }
}

/*
*********************************************************************************************************
*    函 数 名: sfDispMenu
*    功能说明: 显示操作提示菜单
*    形    参: 无
*    返 回 值: 
*********************************************************************************************************
*/
static void sfDispMenu(void)
{
    printf("\r\n*******************************************\r\n");
    printf("Please select command:\r\n");
    printf("[1 - Read serial Flash, Addr:0x%X, Length:%d bytes]\r\n", TEST_ADDR, TEST_SIZE);
    printf("[2 - Write serial Flash, Addr:0x%X, Length:%d bytes]\r\n", TEST_ADDR, TEST_SIZE);
    printf("[3 - Erase whole serial Flash]\r\n");
    printf("[4 - Write whole serial Flash, all 0x55]\r\n");
    printf("[5 - Read whole serial Flash, test read speed]\r\n");
    printf("[Z - Read previous 1K, addr auto decrease]\r\n");
    printf("[X - Read next 1K, addr auto increase]\r\n");
    printf("Any other key - show command menu\r\n");
    printf("\r\n");
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
