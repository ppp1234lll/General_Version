#include "appconfig.h"

update_param_t sg_updateparam_t =
{
	.ip={47, 104, 98, 214}, // 47.104.98.214:8989
	.port = 8989
};
////

/************************************************************
*
* Function name	: update_status_init
* Description	: 初始化
* Parameter		: 
* Return		: 
*	
************************************************************/
void update_status_init(void)
{
	app_set_update_status_function(0);
}

/************************************************************
*
* Function name	: update_set_update_mode
* Description	: 设置更新方式
* Parameter		: 
* Return		: 
*	
************************************************************/
void update_set_update_mode(uint8_t mode)
{
	sg_updateparam_t.mode = mode;
}

/************************************************************
*
* Function name	: update_detection_status_function
* Description	: 监测更新方式
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t update_get_mode_function(void)
{
	return sg_updateparam_t.mode;
}


/************************************************************
*
* Function name	: update_addr_ip
* Description	: 获取更新地址端口ip
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t *update_addr_ip(void)
{
	static uint8_t ip[20] = {0};
	
	sprintf((char*)ip,"%d.%d.%d.%d",sg_updateparam_t.ip[0],sg_updateparam_t.ip[1],sg_updateparam_t.ip[2],sg_updateparam_t.ip[3]);
	
	return ip;
}

/************************************************************
*
* Function name	: update_addr_port
* Description	: 获取更新地址端口
* Parameter		: 
* Return		: 
*	
************************************************************/
uint32_t update_addr_port(void)
{
	return sg_updateparam_t.port;
}
/************************************************************
*
* Function name	: update_get_infor_data_function
* Description	: 获取更新数据信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void *update_get_infor_data_function(void)
{
	return &sg_updateparam_t;
}
//////////////
