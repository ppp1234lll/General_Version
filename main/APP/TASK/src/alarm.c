#include "main.h"
#include "./Task/inc/alarm.h"
#include "appconfig.h"

typedef struct
{
	uint8_t current; 
	uint8_t volt;	   
	uint8_t miu;	  
	uint8_t mcb;	  	
}alarm_code_t;


__attribute__((section (".RAM_D1")))  alarm_code_t sg_alarm_code_t	= {0}; 
struct threshold_params  *sg_alarm_threshold_t; 
data_collection_t *sg_alarm_data_t;
/*
*********************************************************************************************************
*    函 数 名: alarm_task_function
*    功能说明: 告警
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void alarm_task_function(void)
{
	sg_alarm_threshold_t = app_get_threshold_param_function();
	sg_alarm_data_t = det_get_collect_data();
	for(;;)
	{
		alarm_elec_collection_param();  
		alarm_net_collection_param();  
		alarm_sensor_collection_param();  
		FeedFwdgt();	
		vTaskDelay(10);
	}
}

/*
*********************************************************************************************************
*    函 数 名: alarm_elec_collection_param
*    功能说明: 电源判断
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void alarm_elec_collection_param(void)
{
	static uint32_t elec_error  = 0;  // 故障上报状态
	static uint32_t elec_normal	= 0;  // 正常上报状态：

	/* 适配器、断电上报 */
	if(det_get_key_value(PWR_KEY) == KEY_EVNT) // 12V断电
	{
		if(det_get_total_energy_handler(ENERGY_VOLTAGE) < 50)  // 市电电压 < 50V，说明断电
		{
			if( (elec_error & 0x01) == 0) 
			{
				elec_error |= 0x01;
				elec_normal &=~0x01;
				app_power_fail_protection_function();  // 关闭继电器
				Error_Set(ELEC_MAIN_AC);
				app_report_information_immediately(1);
			}						
		}
		else 					   // 市电电压 > 50V，说明适配器故障
		{
			if((elec_error & 0x02) == 0) 
			{
				elec_error  |= 0x02;
				elec_normal &=~0x02;
				Error_Set(ELEC_ACDC_MODULE);
				app_report_information_immediately(1);
			}
		}
	}		
	else            // 12V有电，220V存在
	{
		if(((elec_normal & 0x01) == 0) || ((elec_normal&0x02) == 0))
		{
			if((elec_error&0x01) || (elec_error&0x02))// 适配器故障、220断电
			{
				/* 适配器重新上电 - 重启设备*/
				lfs_unmount(&g_lfs_t);
				vTaskDelay(100);
				app_system_softreset();
			}
			vTaskDelay(100);
			elec_normal |= 0x01;
			elec_error  &=~0x01;
			Error_Clear(ELEC_MAIN_AC);
			elec_normal |= 0x02;
			elec_error &=~ 0x02;
			Error_Clear(ELEC_ACDC_MODULE);
			app_report_information_immediately(0);
			vTaskDelay(2000);
			app_power_open_protection_function();  // 打开继电器

			sg_alarm_code_t.current = 0;
			sg_alarm_code_t.volt = 0;
			Error_Clear(ELEC_AC_OVER_V);
			Error_Clear(ELEC_AC_LOW_V);
			Error_Clear(ELEC_AC_OVER_C);
		}		
	}

	// 高压报警
	if( sg_alarm_threshold_t->volt_max == 0 )  // 阈值为0，不作处理
	{
		if((elec_normal & 0x04) == 0)  // 正常上报标志位是0
		{
			elec_normal |= 0x04;  // 标志位置1，表示已上报
			elec_error &=~ 0x04; // 故障上报标志位清0
			if(sg_alarm_code_t.volt == 1)
			{
				sg_alarm_code_t.volt = 0;
				Error_Clear(ELEC_AC_OVER_V);
				app_report_information_immediately(1);
				vTaskDelay(2000);
				app_power_open_protection_function();  // 打开继电器
			}
		}
	}
	else
	{
		if(det_get_total_energy_handler(0) >= sg_alarm_threshold_t->volt_max) 
		{	
			if((elec_error & 0x04) == 0)  // 故障上报标志位是0
			{
				elec_error |= 0x04;        // 标志位置1，表示已上报
				elec_normal &=~ 0x04;      // 正常上报标志位清0
				app_power_fail_protection_function(); // 关闭继电器		
				sg_alarm_code_t.volt = 1;
				Error_Set(ELEC_AC_OVER_V);
				app_report_information_immediately(1);	
			}
		}
		else 
		{
			if(det_get_total_energy_handler(0) >= 50) // 市电有电情况下
			{
				if((elec_normal & 0x04) == 0)  // 正常上报标志位是0
				{
					elec_normal |= 0x04;  // 标志位置1，表示已上报
					elec_error &=~ 0x04; // 故障上报标志位清0
					if(sg_alarm_code_t.volt == 1)
					{
						sg_alarm_code_t.volt = 0;
						Error_Clear(ELEC_AC_OVER_V);
						app_report_information_immediately(0);
						vTaskDelay(2000);
						app_power_open_protection_function();  // 打开继电器
					}
				}
			}	
		}
	}

	// 低压报警
	if( sg_alarm_threshold_t->volt_min == 0 )  // 阈值为0，不作处理
	{	
		if((elec_normal & 0x08) == 0)  // 正常上报标志位是0
		{
			elec_normal |= 0x08;  // 标志位置1，表示已上报
			elec_error &=~ 0x08; // 故障上报标志位清0
			if(sg_alarm_code_t.volt == 2)
			{
					Error_Clear(ELEC_AC_LOW_V);
					sg_alarm_code_t.volt = 0;
					app_report_information_immediately(1);
					app_power_open_protection_function();  // 打开继电器
			}
		}	
	}
	else
	{
		if((det_get_total_energy_handler(0) <= sg_alarm_threshold_t->volt_min) && \
			det_get_total_energy_handler(0) >= 20)
		{	
			if((elec_error & 0x08) == 0)  // 故障上报标志位是0
			{
				elec_error |= 0x08;        // 标志位置1，表示已上报
				elec_normal &=~ 0x08;      // 正常上报标志位清0		
				
				app_power_fail_protection_function();  // 关闭继电器
				Error_Set(ELEC_AC_LOW_V);
				sg_alarm_code_t.volt = 2;
				app_report_information_immediately(1);
			}
		}
		else if(det_get_total_energy_handler(0) >= sg_alarm_threshold_t->volt_min) // 市电有电情况下	
		{
			if((elec_normal & 0x08) == 0)  // 正常上报标志位是0
			{
				elec_normal |= 0x08;  // 标志位置1，表示已上报
				elec_error &=~ 0x08; // 故障上报标志位清0
				if(sg_alarm_code_t.volt == 2)
				{
					Error_Clear(ELEC_AC_LOW_V);
					sg_alarm_code_t.volt = 0;
					app_report_information_immediately(0);
					app_power_open_protection_function();  // 打开继电器
				}
			}
		}
	}

	/* 检测市电电流使用情况 */
	if( sg_alarm_threshold_t->current == 0 )  // 阈值为0，不作处理
	{	}
	else
	{
		if(det_get_total_energy_handler(1) >= sg_alarm_threshold_t->current)
		{
			/* 电流过大 , 关闭所有外设，并报警 */
			app_power_fail_protection_function(); // 关闭继电器
			if( (elec_error & 0x10) == 0) 
			{
				elec_error |= 0x10;
				elec_normal &=~ 0x10;
				sg_alarm_code_t.current = 1;
				Error_Set(ELEC_AC_OVER_C);
				app_report_information_immediately(1);
			}
		} 
		else  if(det_get_total_energy_handler(1) >= 20)
		{
			if( (elec_normal & 0x10) == 0)
			{
				elec_normal |= 0x10;
				elec_error &=~ 0x10;
				sg_alarm_code_t.current = 0;
				app_report_information_immediately(0);
			}
		}
	}
	/* 漏电预警 */
	if(det_get_miu_value() > sg_alarm_threshold_t->miu)
	{
		if((elec_error & 0x20) == 0) 
		{
			elec_error  |= 0x20;
			elec_normal &=~0x20;
			app_power_fail_protection_function();  // 关闭继电器
			Error_Set(ELEC_AC_LEAKAGE);
			sg_alarm_code_t.miu = 2;
			app_report_information_immediately(1);
		}
	} 
	else 
	{
		if((elec_normal & 0x20) == 0) 
		{
			elec_normal |= 0x20;
			elec_error  &=~0x20;
			if(sg_alarm_code_t.miu == 2)
			{
				// Error_Clear(ELEC_AC_LEAKAGE);
				sg_alarm_code_t.miu = 0;
				app_report_information_immediately(0);
//				app_power_open_protection_function();  // 打开继电器
			}
		}	
	}	

	if(det_get_key_value(MCB_KEY) == KEY_EVNT)
	{
		if(det_get_total_energy_handler(0) < 50)
		{
			if((elec_error & 0x40) == 0) 
			{
				elec_error  |= 0x40;
				elec_normal &=~0x40;
				sg_alarm_code_t.mcb = 2;
				Error_Set(SENSOR_WATER_LEAK);				
				app_report_information_immediately(1);
			}
		}
		else
		{
			if((elec_normal & 0x40) == 0) 
			{
				elec_normal |= 0x40;
				elec_error  &=~0x40;
				Error_Clear(SENSOR_WATER_LEAK);
				sg_alarm_code_t.mcb = 1;	
				app_report_information_immediately(0);
			}		
		}
	} 
	else  
	{
		Error_Clear(SENSOR_WATER_LEAK);
		sg_alarm_code_t.mcb = 0;	
	}				
}

/*
*********************************************************************************************************
*    函 数 名: alarm_net_collection_param
*    功能说明: 网络判断
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void alarm_net_collection_param(void)
{
	static uint32_t net_error  = 0;   
	static uint32_t net_normal	= 0;  
 
	/* 运维网口 */
	if(eth_get_network_cable_status() == 0) 
	{
		if((net_error & 0x01) == 0) 
		{
			net_error  |= 0x01;
			net_normal &=~0x01;
			Error_Set(NET_LAN_PORT);
			app_report_information_immediately(1);
		}
	} 
	else
	{
		if((net_normal & 0x01) == 0) 
		{
			net_normal |= 0x01;
			net_error  &=~0x01;
			Error_Clear(NET_LAN_PORT);
			app_report_information_immediately(0);
		}	
	}		
	
	/* 检测主网与摄像头是否发送状态变化 */
	if(det_main_network_and_camera_network() == 1) 
	{
		if(com_report_get_main_network_status(0) == 0)
		{
			Error_Clear(NET_MAIN_IP);
			Error_Clear(NET_MAIN_IP_DELAY);
		}
		else 
		{
			if(com_report_get_main_network_status(1) == 0)
			{
				switch(com_report_get_main_network_status(0))
				{		
					case 2:
						Error_Set(NET_MAIN_IP);
						break;
					case 4:
						Error_Set(NET_MAIN_IP_DELAY);
						break;
					default:
						Error_Clear(NET_MAIN_IP);
						Error_Clear(NET_MAIN_IP_DELAY);	
						break;
				}
			}
			else 
			{
				if((com_report_get_main_network_status(0)==1)&&(com_report_get_main_network_status(1)==1))
				{
					Error_Clear(NET_MAIN_IP);
					Error_Clear(NET_MAIN_IP_DELAY);	
				}
				if((com_report_get_main_network_status(0)==2)||(com_report_get_main_network_status(1)==2))
				{
					Error_Set(NET_MAIN_IP);
				}
				if((com_report_get_main_network_status(0)==4)||(com_report_get_main_network_status(1)==4))
				{
					Error_Set(NET_MAIN_IP_DELAY);
				}

			}
		}
	}
}
/*
*********************************************************************************************************
*    函 数 名: alarm_sensor_collection_param
*    功能说明: 传感器判断
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void alarm_sensor_collection_param(void)
{
	static uint32_t sensor_error  = 0;   
	static uint32_t sensor_normal	= 0;   
	
	/* 箱体姿态 */
	if( sg_alarm_data_t->attitude_acc >= sg_alarm_threshold_t->angle) 
	{
		if( (sensor_error & 0x01) == 0) 
		{
			sensor_error  |= 0x01;
			sensor_normal &=~0x01;
			Error_Set(SENSOR_BOX_TILT);
			app_report_information_immediately(1);
		}
	} 
	else 
	{
		if((sensor_normal&0x01) == 0) 
		{
			sensor_normal |= 0x01;
			sensor_error  &=~0x01;
			Error_Clear(SENSOR_BOX_TILT);
			app_report_information_immediately(0);
		}
	}

	/* 箱门	*/
	if(sg_alarm_data_t->key_s[DOOR_KEY] == KEY_EVNT)
	{
		if((sensor_error & 0x02) == 0) 
		{
			sensor_error  |= 0x02;
			sensor_normal &=~0x02;
			Error_Set(SENSOR_DOOR_OPEN);
			app_report_information_immediately(1);
		}
	} 
	else 
	{
		if( (sensor_normal & 0x02) == 0) 
		{
			sensor_normal |= 0x02;
			sensor_error  &=~0x02;
			Error_Clear(SENSOR_DOOR_OPEN);
			app_report_information_immediately(0);
		}
	}
	
	/* 浸水检测模块 */
#if (configUSE_KEY_WATER == 1)
	if(sg_alarm_data_t->key_s[ WATER_KEY] == KEY_EVNT)
	{
		if((sensor_error & 0x04) == 0) 
		{
			sensor_error  |= 0x04;
			sensor_normal &=~0x04;
			Error_Set(SENSOR_WATER_LEAK);
			app_report_information_immediately(1);
		}
	} 
	else  
	{
		if((sensor_normal & 0x04) == 0) 
		{
			sensor_normal |= 0x04;
			sensor_error  &=~0x04;
			Error_Clear(SENSOR_WATER_LEAK);
			app_report_information_immediately(0);
		}	
	}
#endif

	/* 防雷检测 */
#if (configUSE_KEY_SPD == 1)
	if(sg_alarm_data_t->key_s[ SPD_KEY] == KEY_EVNT)
	{
		if((sensor_error & 0x08) == 0) 
		{
			sensor_error  |= 0x08;
			sensor_normal &=~0x08;
			Error_Set(SENSOR_SPD_FAULT);
			app_report_information_immediately(1);
		}
	} 
	else  
	{
		if((sensor_normal & 0x08) == 0) 
		{
			sensor_normal |= 0x08;
			sensor_error  &=~0x08;
			Error_Clear(SENSOR_SPD_FAULT);
			app_report_information_immediately(0);
		}	
	}
#endif

	/* 温度高 */
	if(sg_alarm_data_t->temp_inside >= sg_alarm_threshold_t->temp_high)
	{
		if((sensor_error & 0x0100) == 0) 
		{
			sensor_error  |= 0x0100;
			sensor_normal &=~0x0100;
			Error_Set(SENSOR_TEMP_HIGH);
			app_report_information_immediately(1);
		}
	} 
	else  
	{
		if((sensor_normal & 0x0100) == 0) 
		{
			sensor_normal |= 0x0100;
			sensor_error  &=~0x0100;
			Error_Clear(SENSOR_TEMP_HIGH);
			app_report_information_immediately(0);
		}	
	}

	/* 温度低 */
	if(sg_alarm_data_t->temp_inside <= sg_alarm_threshold_t->temp_low)
	{
		if((sensor_error & 0x0200) == 0) 
		{
			sensor_error  |= 0x0200;
			sensor_normal &=~0x0200;
			Error_Set(SENSOR_TEMP_LOW);
			app_report_information_immediately(1);
		}
	} 
	else  
	{
		if((sensor_normal & 0x0200) == 0) 
		{
			sensor_normal |= 0x0200;
			sensor_error  &=~0x0200;
			Error_Clear(SENSOR_TEMP_LOW);
			app_report_information_immediately(0);
		}	
	}

	/* 湿度高 */
	if(sg_alarm_data_t->humi_inside >= sg_alarm_threshold_t->humi_high)
	{
		if((sensor_error & 0x0400) == 0) 
		{
			sensor_error  |= 0x0400;
			sensor_normal &=~0x0400;
			Error_Set(SENSOR_HUMI_HIGH);
			app_report_information_immediately(1);
		}
	} 
	else  
	{
		if((sensor_normal & 0x0400) == 0) 
		{
			sensor_normal |= 0x0400;
			sensor_error  &=~0x0400;
			Error_Clear(SENSOR_HUMI_HIGH);
			app_report_information_immediately(0);
		}	
	}

}

/*
*********************************************************************************************************
*    函 数 名: alarm_get_vlot_protec_status
*    功能说明: 
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t alarm_get_vlot_protec_status(void)
{
	return sg_alarm_code_t.volt;
}
/*
*********************************************************************************************************
*    函 数 名: alarm_get_current_protec_status
*    功能说明: 
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t alarm_get_current_protec_status(void)
{
	return sg_alarm_code_t.current;
}
/*
*********************************************************************************************************
*    函 数 名: alarm_get_miu_protec_status
*    功能说明: 
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t alarm_get_miu_protec_status(void)
{
	return sg_alarm_code_t.miu;
}
/*
*********************************************************************************************************
*    函 数 名: alarm_get_mcb_protec_status
*    功能说明: 
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t alarm_get_mcb_protec_status(void)
{
	return sg_alarm_code_t.mcb;
}


