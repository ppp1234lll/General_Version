/*
*********************************************************************************************************
*
*	模块名称 : cpu内部falsh操作模块(for STM32H743 H750)
*	文件名称 : bsp_cpu_flash.c
*	版    本 : V1.1
*	说    明 : 提供读写CPU内部Flash的函数
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2019-09-20  armfly  正式发布
*		V1.1    2019-10-03  armfly  写flash函数，修正长度不是32字节整数倍的bug，末尾补0写入。
*									解决HAL库函数的 HAL_FLASH_Program（）的bug，
*									问题现象是大批量连续编程失败（报编程指令顺序错， PGSERR1、 PGSERR2）
*	Copyright (C), 2019-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp_cpu_flash.h"
#include "bsp.h"


/*
*********************************************************************************************************
*	函 数 名: bsp_GetSector
*	功能说明: 根据地址计算扇区首地址
*	形    参: address: a given address(0x08000000~0x080FFFFF)
*	返 回 值: 扇区号（0-7)
*********************************************************************************************************
*/
uint32_t bsp_GetSector(uint32_t Address)
{
	uint32_t sector = 0;

    if((Address < ADDR_FMC_SECTOR_1) && (Address >= ADDR_FMC_SECTOR_0)) {
        sector = CTL_SECTOR_NUMBER_0;
    } else if((Address < ADDR_FMC_SECTOR_2) && (Address >= ADDR_FMC_SECTOR_1)) {
        sector = CTL_SECTOR_NUMBER_1;
    } else if((Address < ADDR_FMC_SECTOR_3) && (Address >= ADDR_FMC_SECTOR_2)) {
        sector = CTL_SECTOR_NUMBER_2;
    } else if((Address < ADDR_FMC_SECTOR_4) && (Address >= ADDR_FMC_SECTOR_3)) {
        sector = CTL_SECTOR_NUMBER_3;
    } else if((Address < ADDR_FMC_SECTOR_5) && (Address >= ADDR_FMC_SECTOR_4)) {
        sector = CTL_SECTOR_NUMBER_4;
    } else if((Address < ADDR_FMC_SECTOR_6) && (Address >= ADDR_FMC_SECTOR_5)) {
        sector = CTL_SECTOR_NUMBER_5;
    } else if((Address < ADDR_FMC_SECTOR_7) && (Address >= ADDR_FMC_SECTOR_6)) {
        sector = CTL_SECTOR_NUMBER_6;
    } else if((Address < ADDR_FMC_SECTOR_8) && (Address >= ADDR_FMC_SECTOR_7)) {
        sector = CTL_SECTOR_NUMBER_7;
    } else if((Address < ADDR_FMC_SECTOR_9) && (Address >= ADDR_FMC_SECTOR_8)) {
        sector = CTL_SECTOR_NUMBER_8;
    } else if((Address < ADDR_FMC_SECTOR_10) && (Address >= ADDR_FMC_SECTOR_9)) {
        sector = CTL_SECTOR_NUMBER_9;
    } else if((Address < ADDR_FMC_SECTOR_11) && (Address >= ADDR_FMC_SECTOR_10)) {
        sector = CTL_SECTOR_NUMBER_10;
    } else {
        sector = CTL_SECTOR_NUMBER_11;
    }
    return sector;
}

/*
*********************************************************************************************************
*	函 数 名: bsp_ReadCpuFlash
*	功能说明: 读取CPU Flash的内容
*	形    参:  _ucpDst : 目标缓冲区
*			 _ulFlashAddr : 起始地址
*			 _ulSize : 数据大小（单位是字节）
*	返 回 值: 0=成功，1=失败
*********************************************************************************************************
*/
uint8_t bsp_ReadCpuFlash(uint32_t _ulFlashAddr, uint8_t *_ucpDst, uint32_t _ulSize)
{
	uint32_t i;

	if (_ulFlashAddr + _ulSize > CPU_FLASH_BASE_ADDR + CPU_FLASH_SIZE)
	{
		return 1;
	}

	/* 长度为0时不继续操作,否则起始地址为奇地址会出错 */
	if (_ulSize == 0)
	{
		return 1;
	}

	for (i = 0; i < _ulSize; i++)
	{
		*_ucpDst++ = *(uint8_t *)_ulFlashAddr++;
	}

	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: bsp_CmpCpuFlash
*	功能说明: 比较Flash指定地址的数据.
*	形    参: _ulFlashAddr : Flash地址
*			 _ucpBuf : 数据缓冲区
*			 _ulSize : 数据大小（单位是字节）
*	返 回 值:
*			FLASH_IS_EQU		0   Flash内容和待写入的数据相等，不需要擦除和写操作
*			FLASH_REQ_WRITE		1	Flash不需要擦除，直接写
*			FLASH_REQ_ERASE		2	Flash需要先擦除,再写
*			FLASH_PARAM_ERR		3	函数参数错误
*********************************************************************************************************
*/
uint8_t bsp_CmpCpuFlash(uint32_t _ulFlashAddr, uint8_t *_ucpBuf, uint32_t _ulSize)
{
	uint32_t i;
	uint8_t ucIsEqu;	/* 相等标志 */
	uint8_t ucByte;

	/* 如果偏移地址超过芯片容量，则不改写输出缓冲区 */
	if (_ulFlashAddr + _ulSize > CPU_FLASH_BASE_ADDR + CPU_FLASH_SIZE)
	{
		return FLASH_PARAM_ERR;		/*　函数参数错误　*/
	}

	/* 长度为0时返回正确 */
	if (_ulSize == 0)
	{
		return FLASH_IS_EQU;		/* Flash内容和待写入的数据相等 */
	}

	ucIsEqu = 1;			/* 先假设所有字节和待写入的数据相等，如果遇到任何一个不相等，则设置为 0 */
	for (i = 0; i < _ulSize; i++)
	{
		ucByte = *(uint8_t *)_ulFlashAddr;

		if (ucByte != *_ucpBuf)
		{
			if (ucByte != 0xFF)
			{
				return FLASH_REQ_ERASE;		/* 需要擦除后再写 */
			}
			else
			{
				ucIsEqu = 0;	/* 不相等，需要写 */
			}
		}

		_ulFlashAddr++;
		_ucpBuf++;
	}

	if (ucIsEqu == 1)
	{
		return FLASH_IS_EQU;	/* Flash内容和待写入的数据相等，不需要擦除和写操作 */
	}
	else
	{
		return FLASH_REQ_WRITE;	/* Flash不需要擦除，直接写 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: bsp_EraseCpuFlash
*	功能说明: 擦除CPU FLASH一个扇区 （128KB)
*	形    参: _ulFlashAddr : Flash地址
*	返 回 值: 0 成功， 1 失败
*			  HAL_OK       = 0x00,
*			  HAL_ERROR    = 0x01,
*			  HAL_BUSY     = 0x02,
*			  HAL_TIMEOUT  = 0x03
*
*********************************************************************************************************
*/
void bsp_EraseCpuFlash(uint32_t _ulFlashAddr)
{
	uint32_t FirstSector = 0;

	/* 解锁 */
	fmc_unlock();

	/* clear pending flags */
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
	
	/* 获取此地址所在的扇区 */
	FirstSector = bsp_GetSector(_ulFlashAddr);
	
	/* 扇区擦除 */	
    /* wait the erase operation complete*/
    if(FMC_READY != fmc_sector_erase(FirstSector)) {
        Error_Handler(__FILE__, __LINE__);
    } 
	
	/* 擦除完毕后，上锁 */
	fmc_lock();	

}

/*
*********************************************************************************************************
*	函 数 名: bsp_WriteCpuFlash
*	功能说明: 写数据到CPU 内部Flash。 必须按32字节整数倍写。不支持跨扇区。扇区大小128KB. \
*			  写之前需要擦除扇区. 长度不是32字节整数倍时，最后几个字节末尾补0写入.
*	形    参: _ulFlashAddr : Flash地址
*			 _ucpSrc : 数据缓冲区
*			 _ulSize : 数据大小（单位是字节, 必须是32字节整数倍）
*	返 回 值: 0-成功，1-数据长度或地址溢出，2-写Flash出错(估计Flash寿命到)
*********************************************************************************************************
*/
uint8_t bsp_WriteCpuFlash(uint32_t _ulFlashAddr, uint8_t *_ucpSrc, uint32_t _ulSize)
{
	uint32_t i;
	uint8_t ucRet;
	uint16_t StartSector, EndSector;

	/* 如果偏移地址超过芯片容量，则不改写输出缓冲区 */
	if (_ulFlashAddr + _ulSize > CPU_FLASH_BASE_ADDR + CPU_FLASH_SIZE)
	{
		return 1;
	}

	/* 长度为0时不继续操作  */
	if (_ulSize == 0)
	{
		return 0;
	}

	ucRet = bsp_CmpCpuFlash(_ulFlashAddr, _ucpSrc, _ulSize);

	if (ucRet == FLASH_IS_EQU)
	{
		return 0;
	}

	__set_PRIMASK(1);  		/* 关中断 */

	/* FLASH 解锁 */
	fmc_unlock();

	/* clear pending flags */
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);

    /* get the number of the start and end sectors */
    StartSector = bsp_GetSector(_ulFlashAddr);
    EndSector = bsp_GetSector(_ulFlashAddr + _ulSize);
    /* each time the sector number increased by 8, 参考扇区定义来理解步进值 */
    for(i = StartSector; i <= EndSector; i += 8) 
	{
        if(FMC_READY != fmc_sector_erase(i)) 
		{
            Error_Handler(__FILE__, __LINE__);
        }
    }

	for (i = 0; i < _ulSize / 4; i++)	
	{
		uint32_t FlashWord;
		
		memcpy((char *)&FlashWord, _ucpSrc, 4);
		_ucpSrc += 4;
		
		if(FMC_READY == fmc_word_program(_ulFlashAddr, FlashWord))
		{
			_ulFlashAddr = _ulFlashAddr + 4; /* 递增，操作下一个32bit */				
		}		
		else
		{
			goto err;
		}
	}
	
	/* 长度不是4字节整数倍 */
	if (_ulSize % 4)
	{
		uint32_t FlashWord = 0xFFFFFFFF; // 未写入的部分保持擦除状态(全1)
		
		memcpy((char *)&FlashWord, _ucpSrc, _ulSize % 4);
		if(FMC_READY == fmc_word_program(_ulFlashAddr, FlashWord))
		{
			; // _ulFlashAddr = _ulFlashAddr + 4;		
		}		
		else
		{
			goto err;
		}
	}
	
  	/* Flash 加锁，禁止写Flash控制寄存器 */
	fmc_lock();

  	__set_PRIMASK(0);  		/* 开中断 */

	return 0;
	
err:
  	/* Flash 加锁，禁止写Flash控制寄存器 */
	fmc_lock();

  	__set_PRIMASK(0);  		/* 开中断 */

	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: bsp_WriteCpuFlash_Save
*	功能说明: 写数据到CPU 内部Flash。 保存ReadAddr地址的2K字节数据
*	形    参: ReadAddr : 读取地址
*			 WriteAddr : 写入地址
*			 pBuffer : 数据缓冲区
*			 NumToWrite : 数据大小（单位是字节, 必须是32字节整数倍）
*	返 回 值: 0-成功，1-数据长度或地址溢出，2-写Flash出错(估计Flash寿命到)
*********************************************************************************************************
*/
#define FLASH_SAVE_SIZE	2048
uint8_t FLASH_SAVE_BUF[FLASH_SAVE_SIZE];//最多是2K字节
uint8_t bsp_WriteCpuFlash_Save(uint32_t _ulReadAddr,uint32_t _ulWriteAddr,uint8_t *_ucpBuf,uint32_t _ulSize)	
{
	uint32_t secoff;	   //扇区内偏移地址(32位字计算)
	uint8_t ret = 0; 
	secoff = _ulWriteAddr - _ulReadAddr;		//扇区内偏移地址

	bsp_ReadCpuFlash(_ulReadAddr,FLASH_SAVE_BUF,FLASH_SAVE_SIZE); //读出2K扇区的内容
	for(uint16_t i=0;i<_ulSize;i++)//复制
	{
		FLASH_SAVE_BUF[i+secoff]=_ucpBuf[i];	  
	}
	ret = bsp_WriteCpuFlash(_ulReadAddr, FLASH_SAVE_BUF,FLASH_SAVE_SIZE);
	return ret;
}

/*
*********************************************************************************************************
*	函 数 名: flash_test
*	功能说明: flash测试
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void flash_test(void)
{
	typedef struct
	{
		uint8_t   ParamVer;			
		uint16_t  ucBackLight;
		uint32_t  Baud485;
		float     ucRadioMode;		
	}
	PARAM_T;

	uint8_t  ucTest, *ptr8;
	uint16_t uiTest, *ptr16;
	uint32_t ulTest, *ptr32;
	PARAM_T tPara, *paraptr;
	
	/* 初始化数据 */
	tPara.Baud485 = 0x5555AAAA;
	tPara.ParamVer = 0x99;
	tPara.ucBackLight = 0x7788;
	tPara.ucRadioMode = 99.99f;

	/* 测试的FLASH地址 - PAGE SIZE = 2K字节 */
	uint32_t TEST_ADDR = ADDR_FMC_SECTOR_2;
	bsp_EraseCpuFlash(TEST_ADDR);
	printf("flash_test start\n");

	ucTest = 0xAA;
	uiTest = 0x55AA;
	ulTest = 0x11223344;
	
	/* 扇区写入数据 */
	bsp_WriteCpuFlash_Save(ADDR_FMC_SECTOR_2,(uint32_t)TEST_ADDR + 32*0,  (uint8_t *)&ucTest, sizeof(ucTest));
	bsp_WriteCpuFlash_Save(ADDR_FMC_SECTOR_2,(uint32_t)TEST_ADDR + 32*1,  (uint8_t *)&uiTest, sizeof(uiTest));
	bsp_WriteCpuFlash_Save(ADDR_FMC_SECTOR_2,(uint32_t)TEST_ADDR + 32*2,  (uint8_t *)&ulTest, sizeof(ulTest));				
	
	/* 读出数据并打印 */
	ptr8  = (uint8_t  *)(TEST_ADDR + 32*0);
	ptr16 = (uint16_t *)(TEST_ADDR + 32*1);
	ptr32 = (uint32_t *)(TEST_ADDR + 32*2);

	printf("写入数据：ucTest = %x, uiTest = %x, ulTest = %x\r\n", ucTest, uiTest, ulTest);
	printf("读取数据：ptr8 = %x, ptr16 = %x, ptr32 = %x\r\n", *ptr8, *ptr16, *ptr32);
	
	printf("flash_test done\n");
	while(1)
	{
		delay_ms(1000);
	}
}



/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
