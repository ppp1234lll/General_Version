#include "main.h"

iapfun jump2app;
uint32_t iapbuf[128]={0};

/*
*********************************************************************************************************
*	函 数 名: iap_write_appbin
*	功能说明: 将BIN数据写入FLASH
*	形    参:  appxaddr : 应用程序的起始地址
*	           appbuf :   应用程序CODE
*		         appsize : 应用程序大小(字节)
*	返 回 值: 无
*********************************************************************************************************
*/
void iap_write_appbin(uint32_t appxaddr,uint8_t *appbuf,uint32_t appsize)
{
	uint16_t t;
	uint16_t i=0;
	uint32_t temp;
	uint32_t buff[4];
	uint32_t fwaddr=appxaddr;//当前写入的地址
	uint8_t *dfu=appbuf;
	for(t=0;t<appsize;t+=4)
	{		
		buff[0] = dfu[0];
		buff[1] = dfu[1];
		buff[2] = dfu[2];
		buff[3] = dfu[3];
		
		temp = buff[0]|(buff[1]<<8)|(buff[2]<<16)|(buff[3]<<24);
		
		dfu+=4;//偏移2个字节
		iapbuf[i++]=temp;	    
		if(i==128)
		{
			i=0;
			bsp_WriteCpuFlash(fwaddr,(uint8_t*)iapbuf,128);	
			fwaddr+=512;//偏移2048  16=2*8.所以要乘以2.
		}
	}
	if(i)
		bsp_WriteCpuFlash(fwaddr,(uint8_t*)iapbuf,i);//将最后的一些内容字节写进去.  
}

/*
*********************************************************************************************************
*	函 数 名: iap_load_app
*	功能说明: 跳转执行APP程序
*	形    参:  appxaddr : 用户代码起始地址
*	返 回 值: 无
*********************************************************************************************************
*/
void iap_load_app(uint32_t appxaddr)
{
	uint32_t stack = *(volatile uint32_t*)appxaddr;
	printf("栈顶地址 = 0x%08X\r\n", stack);    
    
	if(((*(volatile uint32_t*)appxaddr)&0x2FF00000)==0x20000000)	//检查栈顶地址是否合法.
	{ 
		jump2app=(iapfun)*(volatile uint32_t*)(appxaddr+4);		//用户代码区第二个字为程序开始地址(复位地址)		
		sys_msr_msp(*(volatile uint32_t*)appxaddr);					//初始化APP堆栈指针(用户代码区的第一个字用于存放栈顶地址)
		jump2app();									//跳转到APP.
	}
}		 
/****************************************** END OF FILE **********************************************/
