#include "appconfig.h"

#define GSM_GET_GPS_TIME (90*1000) // 90s
#define GSM_DET_STATUS_TIME (60*100) // 60s
#define GSM_START_RUN_TIME (60*60*100) // 1h

typedef struct
{
	#ifdef WIRED_PRIORITY_CONNECTION
	uint8_t tcp_cmd;	    // 1-允许TCP连接 0-禁止tcp连接
	#endif
	uint8_t tcp_status;     // tcp连接状态
	uint8_t tcp_error_cnt;  // 连接错误计数
	struct {
		uint8_t module;     // 模块重启
		uint8_t network;    // 连接重启
	} reset;
	struct 
	{
		uint8_t step;
//		nmea_msg  gpsx ;
		double longitude;	// 经度
		double latitude;	// 纬度
		uint8_t time_flag;  // 计时
		uint32_t start_time; // 开始时间
	} gps;	
} gsm_operate_t;

gsm_operate_t sg_gsmoperate_t;
uint8_t  *sg_gsm_buff;
uint16_t sg_gsm_flag   = 0;

/************************************************************
*
* Function name	: gsm_task_function
* Description	: gsm功能函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void gsm_task_function(void)
{
	uint32_t status_count = 0;
	#ifdef COM_GPS_ENABLE
	uint16_t gps_count = 0;
	#endif
	
	#ifdef WIRELESS_PRIORITY_CONNECTION
	/* 判断网络模式 */
	if(app_get_network_mode() == 1) {
		eth_set_tcp_cmd(1);
	}
	#endif
	
  /* 检测函数 */
	gprs_init_function();

  __RESET:
	led_control_function(LD_GPRS,LD_OFF);
	/* 通信模块初始化 */
	memset(&sg_gsmoperate_t,0,sizeof(gsm_operate_t));
	gprs_deinit_function(); // 清除数据再进行初始化
	while(gprs_status_check_function() == 1) {
		FeedFwdgt();	
		vTaskDelay(10);
	}
	/* 模块状态检测 */
	if( gprs_get_module_status_function() != 1) {

		/* 模块初始化失败 */
		#ifdef WIRELESS_PRIORITY_CONNECTION
		eth_set_tcp_cmd(1); // 启动有线网
		#endif
		goto __RESET;
	}
	/* 初始化成功 - 指示灯亮 */
	led_control_function(LD_GPRS,LD_ON);
    
	/* 执行主功能函数 */
	while(1)
	{
		/* 查看是否允许无线网络连接 */
		if( (app_get_network_mode() != 1 ))	
		{
			if(update_get_mode_function() != UPDATE_MODE_GPRS) // 是否在升级程序状态下
			{
				gsm_tcp_control_function();	// tcp连接
				gsm_reset_task_function();	// 重启软件
					
				if(sg_gsmoperate_t.tcp_status == 1 && (sg_gsm_flag&0x8000))		/* 数据发送 */
				{
					gprs_network_data_send_function(sg_gsm_buff,(sg_gsm_flag&0x7fff));

					sg_gsm_flag = 0;
				}
			}
			else if( update_get_mode_function() == UPDATE_MODE_GPRS)
			{
				update_mobile_task_function();		/* 通过无线更新 */
			}		
		}
		/* 模块状态监测-只有在tcp功能未启动时监测 */
		#ifdef WIRED_PRIORITY_CONNECTION
		if((++status_count) > GSM_DET_STATUS_TIME && sg_gsmoperate_t.tcp_cmd == 0)
		#else
		if((++status_count) > GSM_DET_STATUS_TIME && sg_gsmoperate_t.tcp_status == 0) 
		#endif
		{
			status_count = 0;

			if(gprs_network_status_monitoring_function() != 0) 
			{
				/* 网络注册有问题,需要重启模块*/
				gprs_module_restart_function();
				gsm_set_module_reset_function();
			}
		} 
		else 
		{
			status_count = 0;
		}
		
		/* 需要重新挂载 */
		if( gprs_get_module_status_function() != 1 || sg_gsmoperate_t.reset.module != 0 )
//				gsm_network_status_check()< 0)	
		{
			#ifdef WIRELESS_PRIORITY_CONNECTION
			eth_set_tcp_cmd(1);
			#endif
			gprs_module_restart_function();
			sg_gsmoperate_t.reset.module = 0;
			goto __RESET;
		}

		#ifdef COM_GPS_ENABLE
		/* GPS定位信息查询 */
		/* 不在发送状态 */
		if((++gps_count) > 100 && app_get_com_send_status_function() == 0) {
			gps_count = 0;
			gsm_gps_task_function();
		}
		#endif
		gsm_gps_task_function();
		FeedFwdgt();
		vTaskDelay(10);  // 延时10ms
	}
}

/************************************************************
*
* Function name	: gsm_tcp_control_function
* Description	: tcp控制函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void gsm_tcp_control_function(void)
{
	static 			 uint8_t flag   = 0;
	uint8_t 		 ret 	 		= 0;
	uint8_t 		 port[6] 		= {0};
	struct remote_ip *remote 		= app_get_remote_network_function();
		
	/* 检测tcp状态:tcp未连接 且 tcp被允许连接 */
	#ifdef WIRED_PRIORITY_CONNECTION
	if(sg_gsmoperate_t.tcp_status == 0 && sg_gsmoperate_t.tcp_cmd == 1) 
	{
	#endif
	#ifdef WIRELESS_PRIORITY_CONNECTION
	if(sg_gsmoperate_t.tcp_status == 0) 
	{
	#endif
		if(flag == 0) {
			/* 进行状态监测 */
			if(gprs_network_status_monitoring_function() != 0) 
			{
				/* 网络注册有问题,需要重启模块*/
				gprs_module_restart_function();
				gsm_set_module_reset_function();
        led_control_function(LD_GPRS,LD_OFF);
				return;
			}
			flag = 1;
		}
		
		if(remote->outside_port == 0 || remote->outside_iporname[0] == 0 ||
			(strcmp((char*)remote->outside_iporname,"0.0.0.0")==0)) { 
			/* 地址不符合连接需求，不进行连接 */
			#ifdef WIRED_PRIORITY_CONNECTION
			sg_gsmoperate_t.tcp_cmd = 0;
			#else
			eth_set_tcp_cmd(1);
			led_control_function(LD_GPRS,LD_ON);

			#endif
		} 
		else 
		{
			/* 连接主线路 */
			sprintf((char*)port,"%d",remote->outside_port);
			ret = gprs_network_connect_function(remote->outside_iporname,port);

			if(ret == 0) 					  // 连接成功
			{
				#ifdef WIRELESS_PRIORITY_CONNECTION
				eth_set_tcp_cmd(0);
				#endif			
				led_control_function(LD_GPRS,LD_FLICKER);
				app_set_com_interface_selection_function(1);
				sg_gsmoperate_t.tcp_status = 1;
				app_send_once_heart_infor();  // 发送一次心跳
				app_detect_function();
			}
			else		 					  // 连接失败
			{
        led_control_function(LD_GPRS,LD_ON);
				sg_gsmoperate_t.tcp_status = 0;
				sg_gsmoperate_t.tcp_error_cnt++;
				if(sg_gsmoperate_t.tcp_error_cnt > GSM_TCP_CONNECT_TIME)
				{
					sg_gsmoperate_t.tcp_error_cnt = 0;
					/* 需要对sim800c进行初始化 */
					gprs_module_restart_function();
					gsm_set_module_reset_function();
				}
			}
		}
	}
	#ifdef WIRED_PRIORITY_CONNECTION
	/* tcp连接被拒绝 */
	if(sg_gsmoperate_t.tcp_cmd == 0 && sg_gsmoperate_t.tcp_status == 1)
	{
		gprs_network_disconnect_function(0);
		
		app_set_com_interface_selection_function(0);
	}
	#endif
  #ifdef WIRED_PRIORITY_CONNECTION
	if(sg_gsmoperate_t.tcp_cmd == 0) {
		flag = 0;
	}
    #else 
    if(sg_gsmoperate_t.tcp_status == 0) {
        flag = 0;
    }
	#endif
}

/************************************************************
*
* Function name	: gsm_reset_task_function
* Description	: 重启任务函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void gsm_reset_task_function(void)
{
	if(sg_gsmoperate_t.reset.network != 0) 
	{
		sg_gsmoperate_t.reset.network = 0;
		/* 断开网络连接 */
		gprs_network_connection_restart_function();

//		gsm_set_module_reset_function(); // 重启模块
		sg_gsmoperate_t.tcp_status = 0;
	}
}

#ifdef WIRED_PRIORITY_CONNECTION
/************************************************************
*
* Function name	: gsm_set_tcp_cmd
* Description	: 设置tcp功能状态
* Parameter		: 
*	@cmd		: 0：停止 1：启动
* Return		: 
*	
************************************************************/
void gsm_set_tcp_cmd(uint8_t cmd)
{
	sg_gsmoperate_t.tcp_cmd = cmd;
}
#endif

/************************************************************
*
* Function name	: gsm_send_tcp_data
* Description	: 数据发送函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void gsm_send_tcp_data(uint8_t *data, uint16_t size)
{
	sg_gsm_buff = data;
	sg_gsm_flag = size + 0x8000;
}

const char gsm_moduble_init[]   = {0x20,0xe5,0xb7,0xb2,0xe6,0x8c,0x82,0xe8,0xbd,0xbd,0x20,0x00}; // 已挂在
const char gsm_moduble_uninit[] = {0x20,0xe6,0x9c,0xaa,0xe6,0x8c,0x82,0xe8,0xbd,0xbd,0x20,0x00}; // 未挂在
const char gsm_find_signal[]    = {0xe6,0x9f,0xa5,0xe6,0x89,0xbe,0xe4,0xbf,0xa1,0xe5,0x8f,0xb7,0x20,0x00}; // 查找信号
const char gsm_find_sim[]       = {0xe6,0x9f,0xa5,0xe6,0x89,0xbe,0x53,0x49,0x4d,0x20,0x00};      // 查找sim   
const char gsm_find_network[]   = {0xe6,0xb3,0xa8,0xe5,0x86,0x8c,0xe7,0xbd,0x91,0xe7,0xbb,0x9c,0x20,0x00};  // 注册网络
const char gsm_find_time[]      = {0xe5,0x90,0x8c,0xe6,0xad,0xa5,0xe6,0x97,0xb6,0xe9,0x97,0xb4,0x20,0x00};  // 同步时间
const char gsm_find_module[]    = {0xe6,0xa8,0xa1,0xe5,0x9d,0x97,0xe5,0x88,0x9d,0xe5,0xa7,0x8b,0xe5,0x8c,0x96,0x20,0x00};   // 模块初始化
const char gsm_start_network[]  = {0xe6,0xbf,0x80,0xe6,0xb4,0xbb,0xe7,0xbd,0x91,0xe7,0xbb,0x9c,0x00};    // 启动服务

const char gsm_find_cfun[]      = {0xe6,0x9f,0xa5,0xe8,0xaf,0xa2,0xe5,0x8d,0x8f,0xe8,0xae,0xae,0xe6,0xa0,0x88,0x00};// 查询协议栈 
const char gsm_find_mipcall[]   = {0xe6,0x9f,0xa5,0xe8,0xaf,0xa2,0xe6,0x8b,0xa8,0xe5,0x8f,0xb7,0xe7,0x8a,0xb6,0xe6,0x80,0x81,0x00};// 查询拨号状态               // 启动服务

/************************************************************
*
* Function name	: gsm_gst_init_status_function
* Description	: 获取gsm工作状态:
* Parameter		: 
*	@buff		: 数据指针
*	@sel		: 0-信号强度 1-入网地址 2-拨号状态
* Return		: 
*	
************************************************************/
uint8_t  gsm_gst_init_status_function(uint8_t sel)
{
	uint8_t ret;
	switch(sel) 
	{
		case 0:
			ret = gprs_get_csq_function();
		break;
		case 1:				
			if(gprs_get_module_status_function() == 0) 	
				ret =  gprs_get_module_init_state();
			else
				ret =  6;
			break;
		default: break;
		}
	return ret;
}

/************************************************************
*
* Function name	: gsm_gst_run_status_function
* Description	: 获取gsm工作状态:
* Parameter		: 
*	@buff		: 数据指针
*	@sel		: 0-信号强度 1-入网地址 2-拨号状态
* Return		: 
*	
************************************************************/
void gsm_gst_run_status_function(char *buff, uint8_t sel)
{
	uint8_t ret = 0;
    
	switch(sel) {
		case 0:
			sprintf(buff,"%d",gprs_get_csq_function());
		
			break;
		case 1:
			if(gprs_get_module_status_function() == 0) {

			ret = gprs_get_module_init_state();

			switch(ret) 
			{
					case 0: // 模块初始化
							sprintf(buff,"%s",gsm_find_module);
							break;
					case 1: // 查找SIM卡
							sprintf(buff,"%s",gsm_find_sim);
							break;
					case 6: // 查询协议栈
							sprintf(buff,"%s",gsm_find_cfun);
							break;
					case 2: // 查找信号
							sprintf(buff,"%s",gsm_find_signal);
							break;
					case 3: // 注册网络
							sprintf(buff,"%s",gsm_find_network);
							break;
					case 4: // 同步时间
							sprintf(buff,"%s",gsm_find_time);
							break;
					case 7: // 查询拨号状态
							sprintf(buff,"%s",gsm_find_mipcall);
							break;
					case 5: // 激活网络
							sprintf(buff,"%s",gsm_start_network);
							break;
					default: // 未挂载
							sprintf(buff,"%s",gsm_moduble_uninit);
							break;
				}
			} 
			else 
			{
				sprintf(buff,"%s",gsm_moduble_init);
			}
			break;
      case 2: // 入网IP
			sprintf(buff,"%s",(char *)gprs_get_ip_addr_function());
      break;
		default:
			break;
	}
}

/************************************************************
*
* Function name	: gsm_get_sim_ccid_function
* Description	: 获取sim卡的序列号
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t *gsm_get_sim_ccid_function(void)
{
	return gprs_get_ccid_function();
}


/************************************************************
*
* Function name	: gsm_get_network_connect_status_function
* Description	: 获取网络连接状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t gsm_get_network_connect_status_function(void)
{
	return sg_gsmoperate_t.tcp_status;
}

/************************************************************
*
* Function name	: gsm_set_network_reset_function
* Description	: 连接重置
* Parameter		: 
* Return		: 
*	
************************************************************/
void gsm_set_network_reset_function(void)
{
	sg_gsmoperate_t.reset.network = 1;
}

/************************************************************
*
* Function name	: gsm_set_module_reset_function
* Description	: 通信模块重启设置
* Parameter		: 
* Return		: 
*	
************************************************************/
void gsm_set_module_reset_function(void)
{
	sg_gsmoperate_t.reset.module = 1;
}

/************************************************************
*
* Function name	: gsm_data_send_function
* Description	: 网络数据发送函数
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t gsm_data_send_function(uint8_t *buff, uint16_t len)
{
	return gprs_network_data_send_function(buff,len);
}

/************************************************************
*
* Function name	: gsm_gps_task_function
* Description	: gps定位信息获取函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t gsm_gps_task_function(void)
{
	int8_t ret = 0;
	struct locat_infor_t locat_t;
	
	if(sg_gsmoperate_t.gps.step)
	{
		sg_gsmoperate_t.gps.step = 0;
		ret = gps_lbs_position_read_function(&locat_t);
		if(ret == 0) 
		{
			sg_gsmoperate_t.gps.latitude = locat_t.latitude;
			sg_gsmoperate_t.gps.longitude = locat_t.longitude;
		} 

	}
	
	return 0;
}

/************************************************************
*
* Function name	: gsm_run_gps_task_function
* Description	: 获取一次gps数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void gsm_run_gps_task_function(void)
{
	sg_gsmoperate_t.gps.start_time = 0;
	sg_gsmoperate_t.gps.step = 1;
}
/************************************************************
*
* Function name	: gsm_task_timer_function
* Description	: 任务时间函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void gsm_task_timer_function(void)
{
	static uint32_t gps_times = 0;
	
	/* gps获取计时 */
	if(sg_gsmoperate_t.gps.time_flag != 0) {
		gps_times++;
		if(sg_gsmoperate_t.gps.time_flag == 1) {
			sg_gsmoperate_t.gps.time_flag = 2;
			gps_times = 0;
		} else {
			if(gps_times > GSM_GET_GPS_TIME) {
				gps_times = 0;
				sg_gsmoperate_t.gps.time_flag = 3;
			}
		}
	}
	
	/* GPS获取倒计时 */
	if(sg_gsmoperate_t.gps.step == 0) 
	{
		sg_gsmoperate_t.gps.start_time++;
		if(GSM_START_RUN_TIME < sg_gsmoperate_t.gps.start_time) 
		{
			sg_gsmoperate_t.gps.start_time = 0;
			/* 启动一次GPS */
			gsm_run_gps_task_function();
		}
	}
}

/************************************************************
*
* Function name	: gsm_get_location_information_function
* Description	: 获取定位信息
* Parameter		: 
*	@mode		: 0-经度 1-纬度
* Return		: 对应数据值
*	
************************************************************/
double gsm_get_location_information_function(uint8_t mode)
{
	if( mode == 0 ) {
		return sg_gsmoperate_t.gps.longitude;
	} else {
		return sg_gsmoperate_t.gps.latitude;
	}
}

