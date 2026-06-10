#include "lwip/debug.h"
#include "httpd.h"
#include "lwip/tcp.h"
#include "fs.h"
#include "lwip_comm.h"
#include "det.h"
#include "bsp.h"
#include "app.h"
#include "start.h"
#include "eth.h"
#include "appconfig.h"
#include "gsm.h"
//#include "includes.h"

#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include "update.h"

#include "httpd_cgi_ssi.h"

#define NUM_CONFIG_CGI_URIS	(sizeof(ppcURLs) / sizeof(tCGI))
#define NUM_CONFIG_SSI_TAGS	(sizeof(ppcTAGs) / sizeof(char *))
	
//控制LED的CGI handler
const char* INDEX_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
const char* SETTING_CGI_Handler(int iIndex,int iNumParams,char *pcParam[],char *pcValue[]);

uint8_t g_return_t[300] =  {0};
uint16_t g_return_flag  = 0;

#define PPCTAGS_SIZE (51)
static const char *ppcTAGs[PPCTAGS_SIZE]=  //SSI的Tag
{
	"a", // 同步时间
	"b", // 本机ID
	"c", // 设备名称
	"d", // 登陆密码
	"e", // IP
	"f", // 子网掩码
	"g", // 网关
	"h", // DNS
	"i", // 主网检测地址
	"m", // 摄像头1
	"n", // 摄像头2
	"o", // 摄像头3
	"p", // 摄像头4
	"q", // 摄像头5
	"r", // 摄像头6
	"s", // 每轮ping间隔时间
	"t", // 下次ping的时间
	"C", // 网络连接状态
	"D", // 内网IP
	"E", // 内网端口
	"F", // 外网IP
	"G", // 外网端口
	"H", // 升级地址
	"I", // 升级端口
	"J", // 上报时间 
	"L", // MAC
	"N", // 网络延时时间  20220308
	"O", // 组播IP
	"P", // 组播端口  
	"Q", // ONVIF时间 20230811 
	"aa", // 高压  20230720
	"ab", // 低压  
	"ac", // 电流  
	"ad", // 姿态 
	"ae", // 风扇启动温度
	"af", // 风扇停止温度  
	"ag", // 风扇启动湿度  
	"ah", // 风扇停止湿度 
	"ai", // 本机端口
	"aj", // 重启 	
	"ak", // 箱门开启时间	
	"al", // 补光灯开启时间
	"am", // 漏电
	"ba", // 升级url 
	"bb", // 升级端口 
	"bc", // 重启次数 
	"bd", // 重启时间
	"be", // OID
	"bf", // 交换机IP

	"Y",
	"Z",
};


static const tCGI ppcURLs[]= //cgi程序
{
	{"/parse.cgi",INDEX_CGI_Handler},
	{"/setting.cgi",SETTING_CGI_Handler},
};


/************************************************************
*
* Function name	: 
* Description	: 当web客户端请求浏览器的时候,使用此函数被CGI handler调用
* Parameter		: 
* Return		: 
*	
************************************************************/
static int FindCGIParameter(const char *pcToFind,char *pcParam[],int iNumParams)
{
	int iLoop;
	for(iLoop = 0;iLoop < iNumParams;iLoop ++ )
	{
		if(strcmp(pcToFind,pcParam[iLoop]) == 0)
		{
			return (iLoop); //返回iLOOP
		}
	}
	return (-1);
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
const char *INDEX_CGI_Handler(int iIndex,int iNumParams,char *pcParam[],char *pcValue[])
{
	uint8_t i=0;
	iIndex = FindCGIParameter("login",pcParam,iNumParams);
	if(iIndex != -1)
	{
		for (i=0; i<iNumParams; i++)
		{
			if (strcmp(pcParam[i] , "login")==0)
			{
				set_return_status_function(0,0);
			}
		}
	}
	return ("/parse.shtml");
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
const char *SETTING_CGI_Handler(int iIndex,int iNumParams,char *pcParam[],char *pcValue[])
{
	int8_t ret = 0;
	
	iIndex = FindCGIParameter("btntype",pcParam,iNumParams);
	
	if(iIndex != -1) {
		/* 网页登录 */
		ret = httpd_cgi_login_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
		goto EXIT;
		}
		/* 密码修改 */
		ret = httpd_cgi_login_mod_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
			goto EXIT;
		}
		/* 下拉框 */
		ret = httpd_cgi_select_function(pcValue);
		if(ret == 0) {
			goto EXIT;
		}
		/* 阈值 20230720*/
		ret = httpd_cgi_set_threshold_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
			goto EXIT;
		}
		/* 开关状态 */
		ret = httpd_cgi_switch_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
			goto EXIT;
		}
		/* 系统设置 */
		ret = httpd_cgi_set_system_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
			goto EXIT;
		}
		/* 网络设置 */
		ret = httpd_cgi_set_network_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
			goto EXIT;
		}
		/* 摄像机IP */
		ret = httpd_cgi_set_camera_ip_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
			goto EXIT;
		}
		/* 远端服务器 */
		ret = httpd_cgi_set_remote_ip_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
			goto EXIT;
		}
		
		/* 更新服务器 */
		ret = httpd_cgi_set_update_addr_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
			goto EXIT;
		}

		/* SNMP设置 */
		ret = httpd_cgi_snmp_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
			goto EXIT;
		}
		/* SNMP测试 */
		ret = httpd_cgi_snmp_test_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
			goto EXIT;
		}		
		
		/* 系统更新 */
		ret = httpd_cgi_update_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
			goto EXIT;
		}

		/* 系统设置 */
		ret = httpd_cgi_system_function(iNumParams,pcParam,pcValue);
		if(ret == 0) {
			goto EXIT;
		}
		/* 显示更新 */
		ret = httpd_cgi_show_function(pcValue,&g_return_flag,g_return_t);
		if(ret == 0) {
			goto EXIT;
		}
	}
EXIT:
	return ("/setting.shtml");
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
static void Return_status_Handler(char *pcInsert, uint8_t mode)
{
	if(mode == 0)
	{
		sprintf(pcInsert,"%d",g_return_flag);
	}
	else
	{
		sprintf(pcInsert,"%s",g_return_t);
	}
	
}

//SSI的Handler句柄
static u16_t SSIHandler(int iIndex,char *pcInsert,int iInsertLen)
{
	switch(iIndex)
	{
        /* 返回 */
		case (PPCTAGS_SIZE-2):
			Return_status_Handler(pcInsert,0);
			break;
		case (PPCTAGS_SIZE-1):
			Return_status_Handler(pcInsert,1);
			break;
		default:
			break;
	}
	return strlen(pcInsert);
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
void set_return_status_function(uint16_t flag,uint8_t *buff)
{
	g_return_flag = flag;
	memset(g_return_t,0,sizeof(g_return_t));
	memcpy(g_return_t,buff,strlen((char*)buff));
}


//SSI句柄初始化
void httpd_ssi_init(void)
{  
	//配置内部温度传感器的SSI句柄
	http_set_ssi_handler(SSIHandler,ppcTAGs,NUM_CONFIG_SSI_TAGS);
}

//CGI句柄初始化
void httpd_cgi_init(void)
{ 
  //配置CGI句柄LEDs control CGI) */
  http_set_cgi_handlers(ppcURLs, NUM_CONFIG_CGI_URIS);
}



