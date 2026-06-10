#include "httpd_cgi_ssi.h"
#include "lwip/debug.h"
#include "httpd.h"
#include "lwip/tcp.h"
#include "bsp.h"
#include "appconfig.h"

const char cg_ssi_open[]   = {0xe5,0xbc,0x80,0x00};				  // 开启
const char cg_ssi_close[]  = {0xe5,0x85,0xb3,0x00};				  // 关闭
const char cg_ssi_normal[] = {0xe6,0xad,0xa3,0xe5,0xb8,0xb8,0x00}; // 正常
const char cg_ssi_error[]  = {0xe6,0x95,0x85,0xe9,0x9a,0x9c,0x00}; // 故障

const char spd_ssi_none[]  = {0xE4,0xB8,0x8D,0xE6,0xA3,0x80,0xE6,0xB5,0x8B,0x00}; // 不检测
const char spd_ssi_error[]  = {0xE5,0xB7,0xB2,0xE5,0xA4,0xB1,0xE6,0x95,0x88,0x00}; // 已失效
const char spd_ssi_ok[]  = {0xe6,0xad,0xa3,0xe5,0xb8,0xb8,0x00}; // 正常

const char water_ssi_none[]  = {0xE4,0xB8,0x8D,0xE6,0xA3,0x80,0xE6,0xB5,0x8B,0x00}; // 不检测
const char water_ssi_error[] = {0xe6,0xbc,0x8f,0xe6,0xb0,0xb4,0x00}; // 漏水
const char water_ssi_ok[]    = {0xe6,0xad,0xa3,0xe5,0xb8,0xb8,0x00}; // 正常



/************************************************************
*
* Function name	: Get_Total_Energy_Handler
* Description	: 获取总能量参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void Get_Total_Energy_Handler(char *pcInsert, uint8_t num)
{
	float temp =  det_get_total_energy_handler(num);
	uint32_t data[2] = {0};
	
	data[0] = (uint32_t)temp;
	temp	  = temp - data[0];  
	data[1] = temp*100;
	
	sprintf(pcInsert,"%d.%02d",data[0],data[1]);
}

/************************************************************
*
* Function name	: open_door_status_Handler
* Description	: 箱门状态
* Parameter		: 
* Return		: 
*	
************************************************************/
static void open_door_status_Handler(char *pcInsert)
{
	uint8_t data = det_get_door_status();
	
	if(data == 1)
	{
		sprintf(pcInsert,"%s",cg_ssi_open);
	}
	else
	{
		sprintf(pcInsert,"%s",cg_ssi_close);
	}
}

/************************************************************
*
* Function name	: cabinet_posture_Handler
* Description	: 箱体姿态
* Parameter		: 
* Return		: 
*	
************************************************************/
static void cabinet_posture_Handler(char *pcInsert)
{
	uint8_t data = det_get_cabinet_posture();
	sprintf(pcInsert,"%d",data);
}


/************************************************************
*
* Function name	: spd_status_Handler
* Description	: SPD状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void spd_status_Handler(char *pcInsert)
{
	uint8_t data = 0;//det_get_spd_status();
	switch(data)
	{
		case 0: sprintf(pcInsert,"%s",spd_ssi_none);  break;
		case 1: sprintf(pcInsert,"%s",spd_ssi_ok); 	  break;
		case 2: sprintf(pcInsert,"%s",spd_ssi_error); break;
		default:break;
	}
}

/************************************************************
*
* Function name	: water_status_Handler
* Description	: 浸水状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void water_status_Handler(char *pcInsert)
{
	uint8_t data = det_get_water_status();
	switch(data)
	{
		case 0: sprintf(pcInsert,"%s",water_ssi_none);  break;
		case 1: sprintf(pcInsert,"%s",water_ssi_ok); 	  break;
		case 2: sprintf(pcInsert,"%s",water_ssi_error); break;
		default:break;
	}
}

/************************************************************
*
* Function name	: httpd_ssi_volt_cur_data_collection_function
* Description	: 电能参数界面更新
* Parameter		: 
* Return		: 
*	
************************************************************/
void httpd_ssi_volt_cur_data_collection_function(char *pcInsert)
{
	char buff[4][8] = {0};
	
	Get_Total_Energy_Handler(buff[0],0);		// 总电压
	Get_Total_Energy_Handler(buff[1],1);		// 总电流
	Get_Total_Energy_Handler(buff[2],2);		// 功率
	Get_Total_Energy_Handler(buff[3],3);		// 用电量
	
	sprintf(pcInsert,"[\"%sV\",\"%smA\",\"%sW\",\"%sWh\"]",
						buff[0],buff[1],buff[2],buff[3]);
}

/************************************************************
*
* Function name	: httpd_ssi_other_data_collection_function
* Description	: 其他参数界面更新
* Parameter		: 
* Return		: 
*	
************************************************************/
void httpd_ssi_other_data_collection_function(char *pcInsert)
{
	char buff[4][8] = {0};
	char new_buff[4][7] = {0};
	char new_buff1[4][30] = {0};
	uint16_t data[2] = {0};
	float    temp    = 0;
	uint8_t  unit[] = {0xe2,0x84,0x83};
	
	temp = det_get_inside_temp();
	if(temp < 0) {
		temp = 0-temp;
		data[0] = (uint16_t)temp;
		temp	= temp - data[0];  
		data[1] = temp*100;
		sprintf(buff[0],"-%d.%02d",data[0],data[1]);		// 温度	
	} else {
		data[0] = (uint16_t)temp;
		temp	= temp - data[0];  
		data[1] = temp*100;
		sprintf(buff[0],"%d.%02d",data[0],data[1]);			// 温度	
	}
	
	temp = det_get_inside_humi();
	data[0] = (uint16_t)temp;
	temp	= temp - data[0];  
	data[1] = temp*100;
	sprintf(buff[1],"%d.%02d",data[0],data[1]);			// 湿度
	
	cabinet_posture_Handler(new_buff[0]);				// 箱体姿态
	open_door_status_Handler(new_buff[1]);				// 箱门状态

	water_status_Handler(new_buff1[1]);

	temp = det_get_miu_value();
	sprintf(new_buff1[2],"%d",(int)temp);			// MIU值
	
	sprintf(pcInsert,"[\"%s%s\",\"%s%%\",\"%s\",\"%s\",\"%s\",\"%s\"]",
			buff[0],unit,buff[1],new_buff[0],new_buff[1],\
			new_buff1[1],new_buff1[2]);
}

/************************************************************
*
* Function name	: httpd_ssi_threshold_seting_function
* Description	: 阈值信息更新
* Parameter		: 
* Return		: 
*	  20230720
************************************************************/
void httpd_ssi_threshold_seting_function(char *pcInsert)
{
	char buff[11][5]   = {0};
	char times[2][15] = {0};
	struct threshold_params *param = app_get_threshold_param_function();

	sprintf(buff[0],"%d",param->volt_max);
	sprintf(buff[1],"%d",param->volt_min);
	sprintf(buff[2],"%d",param->current);
	sprintf(buff[3],"%d",param->angle);
	sprintf(buff[4],"%d",param->temp_high);
	sprintf(buff[5],"%d",param->temp_low);
	sprintf(buff[6],"%d",param->humi_high);
	sprintf(buff[7],"%d",param->humi_low);
	sprintf(buff[8],"%d",param->miu);
	sprintf(buff[9],"%d",param->net_reload);
	sprintf(buff[10],"%d",param->net_retime);
	sprintf(times[0],"%02d:%02d-%02d:%02d",
			param->door_open_time/60,param->door_open_time%60,\
			param->door_close_time/60,param->door_close_time%60);
	
	sprintf(pcInsert,"[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]",
										buff[0],buff[1],buff[2],buff[3],buff[4],buff[5],buff[6],buff[7],times[0],buff[8],buff[9],buff[10]);

}
/************************************************************
*
* Function name	: httpd_ssi_threshold_seting_function
* Description	: 阈值信息更新
* Parameter		: 
* Return		: 
*	  20230720
************************************************************/
void httpd_ssi_bd_data_collection_function(char *pcInsert)
{
	char buff[6][16] = {0};
	atgm336h_data_t *param = atgm336h_get_gnss_data();	
	float    temp    = 0;
	uint32_t data[2] = {0};
	
	temp = param->altitude;
	data[0] = (uint16_t)temp;
	temp	= temp - data[0];  
	data[1] = temp*100;
	sprintf(buff[2],"%d.%02d",data[0],data[1]);		
	
	sprintf(buff[0],"%d",param->num_satellites);
	sprintf(buff[1],"%d",param->num_satellites);
	
	// 处理 latitude
	int lat_int = (int)param->latitude;
	int lat_dec = (int)((param->latitude - lat_int) * 1000000);
	if(lat_dec < 0) lat_dec = -lat_dec;
	if(param->latitude < 0 && lat_int == 0)
		sprintf(buff[3],"-%d.%06d", lat_int, lat_dec);
	else
		sprintf(buff[3],"%d.%06d", lat_int, lat_dec);
		
	// 处理 longitude
	int lon_int = (int)param->longitude;
	int lon_dec = (int)((param->longitude - lon_int) * 1000000);
	if(lon_dec < 0) lon_dec = -lon_dec;
	if(param->longitude < 0 && lon_int == 0)
		sprintf(buff[4],"-%d.%06d", lon_int, lon_dec);
	else
		sprintf(buff[4],"%d.%06d", lon_int, lon_dec);
		
	sprintf(buff[5],"%d",param->fix_status);
	
	sprintf(pcInsert,"[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]",
					buff[0],buff[1],buff[2],buff[3],buff[4],buff[5]);
}

/************************************************************
*
* Function name	: Switch_status_Handler
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
static void Switch_status_Handler(char *pcInsert,RELAY_DEV dev)
{
	int8_t state = relay_get_status(dev);
	
	if(state == RELAY_OFF)
	{
		sprintf(pcInsert,"0");
	}
	else
	{
		sprintf(pcInsert,"1");
	}	
}


/************************************************************
*
* Function name	: httpd_ssi_switch_status_function
* Description	: 开关状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void httpd_ssi_switch_status_function(char *pcInsert)
{
	char buff[8][3] = {0};
	
	Switch_status_Handler(buff[0],RELAY_1);	// 适配器1的开关状态
	Switch_status_Handler(buff[1],RELAY_2);	// 适配器2的开关状态
	Switch_status_Handler(buff[2],RELAY_3);	// 适配器1的开关状态
		
	sprintf(pcInsert,"[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]",
										buff[0],buff[1],buff[2],buff[3],buff[4],buff[5],buff[6],buff[7]);
}

/************************************************************
*
* Function name	: system_param_Handler
* Description	: 系统参数
* Parameter		: 
*	@num		: 0:终端序列号 1：软件版本号 2：硬件版本号 
* Return		: 
*	
************************************************************/
static void system_param_Handler(char *pcInsert, uint8_t num)
{
	uint32_t data[3]= {0};
	switch(num)
	{
		case 0:
			data[0] = *(__I uint32_t *)(0X1FFF7A10 + 0x00);
			data[1] = *(__I uint32_t *)(0X1FFF7A10 + 0x04);
			data[2] = *(__I uint32_t *)(0X1FFF7A10 + 0x08);
			sprintf(pcInsert,"%08X%08X%08X",data[0],data[1],data[2]);
			break;
		case 1:
			sprintf(pcInsert,"%s",SOFT_NO_STR);
			break;
		case 2:
			sprintf(pcInsert,"%s",HARD_NO_STR);
			break;
	}
}

/************************************************************
*
* Function name	: httpd_ssi_system_status_functions
* Description	: 系统状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void httpd_ssi_system_status_function(char *pcInsert)
{
	char buff[3][35] = {0}; // 增加数组长度以防溢出 (24字节的UID + '\0' 需要至少25字节)
	
	system_param_Handler(buff[0], 0);
	system_param_Handler(buff[1], 1);
	system_param_Handler(buff[2], 2);
	
	sprintf(pcInsert,"[\"%s\",\"%s\",\"%s\"]",
			buff[0],buff[1],buff[2]);
}

/************************************************************
*
* Function name	: get_now_time_handler
* Description	: 获取当前时间
* Parameter		: 
* Return		: 
*	
************************************************************/
static void get_now_time_handler(char *pcInsert)
{
	app_get_current_time(pcInsert);
}

/************************************************************
*
* Function name	: device_parameter_handler
* Description	: 获取设备参数
* Parameter		: 
*	@pcInsert	:
*	@num		: 0:ID 1:name 2:password
* Return		: 
*	
************************************************************/
static void device_parameter_handler(char *pcInsert,uint8_t num)
{
	struct device_param *param;
	param = app_get_device_param_function();
	
	switch(num)
	{
		case 0:
//			sprintf(pcInsert,"%08d",param->id);
			sprintf(pcInsert,"%06X",param->id.i);
			break;
		case 1:
			sprintf(pcInsert,"%s",param->name);
			break;
		case 2:
			sprintf(pcInsert,"%s",param->password);
			break;
	}
}

/************************************************************
*
* Function name	: local_network_Handler
* Description	: 本地网络信息
* Parameter		: 
*	@pcInsert	: 
*	@mode		: 0：IP 1：网关 2：掩码 3：DNS 4: 升级地址 5：升级端口 6：传输模式 7: 主网检测地址 8:主网检测地址2
* Return		: 
*	
************************************************************/
static void local_network_Handler(char *pcInsert, uint8_t mode)
{
	struct local_ip_t *local = app_get_local_network_function();
	
	switch(mode)
	{
		case 0:
			sprintf(pcInsert,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
			break;
		case 1:
			sprintf(pcInsert,"%d.%d.%d.%d",local->gateway[0],local->gateway[1],local->gateway[2],local->gateway[3]);
			break;
		case 2:
			sprintf(pcInsert,"%d.%d.%d.%d",local->netmask[0],local->netmask[1],local->netmask[2],local->netmask[3]);
			break;
		case 3:
			sprintf(pcInsert,"%d.%d.%d.%d",local->dns[0],local->dns[1],local->dns[2],local->dns[3]);
			break;
		case 4:
			sprintf(pcInsert,"%s","0.0.0.0");
			break;
		case 5:
			sprintf(pcInsert,"%d",0);
			break;
		case 6:
			switch(local->server_mode) {
				case 1:
					sprintf(pcInsert,"%d",0);
					break;
				case 2:
					sprintf(pcInsert,"%d",1);
					break;
				case 4:
					sprintf(pcInsert,"%d",2);
					break;
			}
			break;
		case 7:
			sprintf(pcInsert,"%d.%d.%d.%d",local->ping_ip[0],local->ping_ip[1],local->ping_ip[2],local->ping_ip[3]);
			break;
		case 8:
			sprintf(pcInsert,"%d.%d.%d.%d",local->ping_sub_ip[0],local->ping_sub_ip[1],local->ping_sub_ip[2],local->ping_sub_ip[3]);
			break;
		case 9:
			sprintf(pcInsert,"%02x-%02x-%02x-%02x-%02x-%02x",local->mac[0],local->mac[1],local->mac[2],local->mac[3],local->mac[4],local->mac[5]);
			break;
		case 10:
			sprintf(pcInsert,"%d.%d.%d.%d",local->multicast_ip[0],local->multicast_ip[1],local->multicast_ip[2],local->multicast_ip[3]);
			break;
		case 11:
			sprintf(pcInsert,"%d",local->multicast_port);
			break;
		case 12:
			sprintf(pcInsert,"%d",local->search_mode - 1);
			break;
	}
}

/************************************************************
*
* Function name	: httpd_ssi_system_seting_function
* Description	: 系统设置信息更新
* Parameter		: 
* Return		: 
*	
************************************************************/
void httpd_ssi_system_seting_function(char *pcInsert)
{
	char buff[4][20] = {0};
	char time[30] 	 = {0};
	char buff2[2][3]    = {0};
	
	get_now_time_handler(time);
	device_parameter_handler(buff[0],0);
	device_parameter_handler(buff[1],1);
	device_parameter_handler(buff[2],2);
	app_get_network_connect_status(buff[3]);
	local_network_Handler(buff2[0],6);
	local_network_Handler(buff2[1],12);
	
	sprintf(pcInsert,"[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]", 
			time,buff[0],buff[1],buff[2],buff[3],buff2[0],buff2[1]);

}


/************************************************************
*
* Function name	: httpd_ssi_nework_gprs_show_function
* Description	: 无线网络设置更新
* Parameter		: 
* Return		: 
*	
************************************************************/
void httpd_ssi_nework_gprs_show_function(char *pcInsert)
{
	char buff1[8] = {0};
	char buff2[24] = {0};
	char buff3[3][30] = {0};
	
	gsm_gst_run_status_function(buff1,0);
	gsm_gst_run_status_function(buff2,1);
	sprintf(buff3[0],"%s",gsm_get_sim_ccid_function());
	sprintf(buff3[1],"%s",gprs_get_model_soft_function());
	sprintf(buff3[2],"%s",gprs_get_imei_function());
	
	sprintf(pcInsert,"[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]",buff1,buff2,buff3[0],buff3[1],buff3[2]);
}

/************************************************************
*
* Function name	: snmp_parameter_handler
* Description	: 获取SNMP IP参数
* Parameter		: 
*	@pcInsert	:
*	@num		: 0:ID 1:name 2:password
* Return		: 
*	
************************************************************/
static void snmp_parameter_handler(char *pcInsert,uint8_t num)
{
	snmp_oid_t *param;
	param = app_get_snmp_oid_function();
	
	switch(num)
	{
		case 0:
			sprintf(pcInsert,"%d.%d.%d.%d",param->switch_ip[0],param->switch_ip[1],param->switch_ip[2],param->switch_ip[3]);
			break;
		case 1:
			break;
		case 2:
			break;
	}
}

/************************************************************
*
* Function name	: httpd_ssi_network_setting_function
* Description	: 网络参数信息更新
* Parameter		: 
* Return		: 
*	
************************************************************/
void httpd_ssi_network_setting_function(char* pcInsert)
{
	char buff[10][20] = {0};
	char time[5] = {0};   // 网络延时时间  20220308
	
	local_network_Handler(buff[0],0);	// IP
	local_network_Handler(buff[1],2);	// 子网掩码
	local_network_Handler(buff[2],1);	// 网关
	local_network_Handler(buff[3],3);	// DNS
	local_network_Handler(buff[6],9);	// MAC
	local_network_Handler(buff[4],7);   // 主网检测地址
	local_network_Handler(buff[5],8);   // 主网检测地址
	sprintf(time,"%d",app_get_network_delay_time());		// 网络延时时间  20220308
	local_network_Handler(buff[7],10);	// IP
	local_network_Handler(buff[8],11);	// 组播
	snmp_parameter_handler(buff[9],0);	// 交换机IP
	
	sprintf(pcInsert,"[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]",
			buff[0],buff[1],buff[2],buff[3],buff[6],buff[4],buff[5],time,buff[7],buff[8],buff[9]);

}

/************************************************************
*
* Function name	: camera_ip_get_Handler
* Description	: 获取摄像机IP信息
* Parameter		: 
*	@pcInsert	: 数据指针
*	@num		: 摄像机编号0-2
* Return		: 
*	
************************************************************/
static void camera_ip_get_Handler(char *pcInsert, uint8_t num)
{
	uint8_t ip[4] = {0};
	
	app_get_camera_function(ip,num);
	sprintf(pcInsert,"%d.%d.%d.%d",ip[0],ip[1],ip[2],ip[3]);
	
}
/************************************************************
*
* Function name	: camera_brand_get_Handler
* Description	: 获取摄像机品牌信息
* Parameter		: 
*	@pcInsert	: 数据指针
*	@num		: 摄像机编号0-2
* Return		: 
*	
************************************************************/
static void camera_brand_get_Handler(char *pcInsert, uint8_t num)
{
	char ip[20] = {0};
	uint8_t brand = 0;
	app_get_camera_param_function(ip,&brand,num);
	sprintf(pcInsert,"%d",brand);
}

/************************************************************
*
* Function name	: httpd_ssi_other_setting_function
* Description	: 其他设置-摄像头
* Parameter		: 
* Return		: 
*	
************************************************************/
void httpd_ssi_other_setting_function(char *pcInsert)
{
	char buff[6][20] = {0};
	char time[2][10] = {0}; // 扩大数组，防止 "%d" + "s" 超出边界
	char brand[6][5] = {0};
	
	/* 摄像头1-6 */
	camera_ip_get_Handler(buff[0],0);
	camera_brand_get_Handler(brand[0],0);
	camera_ip_get_Handler(buff[1],1);
	camera_brand_get_Handler(brand[1],1);
	camera_ip_get_Handler(buff[2],2);
	camera_brand_get_Handler(brand[2],2);
	camera_ip_get_Handler(buff[3],3);
	camera_brand_get_Handler(brand[3],3);
	camera_ip_get_Handler(buff[4],4);
	camera_brand_get_Handler(brand[4],4);
	camera_ip_get_Handler(buff[5],5);
	camera_brand_get_Handler(brand[5],5);	

	sprintf(time[0],"%d",app_get_next_ping_time()/1000);		// PING间隔时间
	sprintf(time[1],"%d",app_get_next_dev_ping_time()/1000);	// PING间隔时间-设备
	// sprintf(time[2],"%d",app_get_onvif_time());	// ONVIF时间  20230811
//	sprintf(time[3],"%d",app_get_device_reload_time()/3600);	// 重启时间       20240904
	
	// 这里不再在占位符中写 "s"，以防前端解析 JSON 数组时遇到包含 "s" 的字符串而失败
	sprintf(pcInsert,"[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]",
			buff[0],brand[0],buff[1],brand[1],buff[2],brand[2],
			buff[3],brand[3],buff[4],brand[4],buff[5],brand[5],
			time[0],time[1]);
}

/************************************************************
*
* Function name	: http_ssi_server_setting_function
* Description	: 服务器设置更新函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_ssi_server_setting_function(char *pcInsert)
{
	struct remote_ip *remote = app_get_remote_network_function();
	struct remote_ip *p_back = app_get_backups_function();	
	
	char ip[20]  = {0};
	char port[6] = {0};

	local_network_Handler(ip,4);
	local_network_Handler(port,5);
	
	sprintf(pcInsert,"[\"%s\",\"%d\",\"%s\",\"%d\",\"%s\",\"%d\",\"%s\",\"%d\",\"%ds\"]",\
			remote->inside_iporname,remote->inside_port,\
			remote->outside_iporname,remote->outside_port,
			p_back->inside_iporname,p_back->inside_port,\
			p_back->outside_iporname,p_back->outside_port,
			app_get_report_time()/1000);
}


/************************************************************
*
* Function name	: httpd_ssi_carema_user_function
* Description	: 摄像机账号
* Parameter		: 
* Return		: 
*	
************************************************************/
void httpd_ssi_carema_user_function(char *pcInsert)
{
	carema_t *user_param = app_get_carema_param_function();
	
	sprintf(pcInsert,"[\"%s\",\"%s\",\"%d\",\"%s\",\"%s\",\"%d\",\"%s\",\"%s\",\"%d\",\
										 \"%s\",\"%s\",\"%d\",\"%s\",\"%s\",\"%d\",\"%s\",\"%s\",\"%d\"	]",
			user_param->name[0],user_param->pwd[0],user_param->port[0],\
			user_param->name[1],user_param->pwd[1],user_param->port[1],\
			user_param->name[2],user_param->pwd[2],user_param->port[2],\
			user_param->name[3],user_param->pwd[3],user_param->port[3],\
			user_param->name[4],user_param->pwd[4],user_param->port[4],\
			user_param->name[5],user_param->pwd[5],user_param->port[5]);
}
/************************************************************
*
* Function name	: http_ssi_update_addr_function
* Description	: 更新地址
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_ssi_update_addr_function(char *pcInsert)
{
	char ip[20]  = {0};
	struct update_addr *param = app_get_http_ota_function();
	sprintf(ip,"%d.%d.%d.%d",param->ip[0],param->ip[1],param->ip[2],param->ip[3]);
	sprintf(pcInsert,"[\"%s\",\"%d\"]",ip,param->port);
}




