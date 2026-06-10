#ifndef __IAP_H__
#define __IAP_H__

#include "./SYSTEM/sys/sys.h"

typedef  void (*iapfun)(void);				//定义一个函数类型的参数.

/* 供外部调用的函数声明 */
void iap_load_app(uint32_t appxaddr);			// 执行flash里面的app程序
void iap_write_appbin(uint32_t appxaddr,uint8_t *appbuf,uint32_t applen);	//在指定地址开始,写入bin
#endif

/****************************************** END OF FILE **********************************************/

