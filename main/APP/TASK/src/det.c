#include "main.h"
#include "./Task/inc/det.h"
#include "appconfig.h"

__attribute__((section (".RAM_D1"))) data_collection_t sg_datacollec_t;

/*
*********************************************************************************************************
*    函 数 名: det_task_function
*    功能说明: 检测线程
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void det_task_function(void)
{
    while(1)
    {
        ebtn_APP_Process();             // 定期处理按键事件（建议5-20ms）
        
        det_get_key_status_function();  // 按键检测函数
        det_get_temphumi_function();    // 获取温湿度
        det_get_attitude_state_value(); // 获取姿态数据
		bl0910_work_process_function(); // 数据获取函数
		bl0939_work_process_function();
        atgm336h_decode_nmea_xxgga();   // 获取GPS数据
        
        FeedFwdgt();                    // 喂狗            
        vTaskDelay(10);   // 延时5ms
    }
}

/*
*********************************************************************************************************
*    函 数 名: det_get_key_status_function
*    功能说明: 处理按键值
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void det_get_key_status_function(void)
{
    if(sg_datacollec_t.key_s[RESET_KEY] == KEY_EVNT)     // 恢复出厂化
    {
        det_set_key_value(RESET_KEY,KEY_NONE);
        led_control_function(LD_GPRS,LD_ON); 
        app_set_reset_function();/* 将系统设置参数恢复为默认值，需要重启生效 */
    }
    else if(sg_datacollec_t.key_s[RESET_KEY] == KEY_ERASE)     // 擦除FLASH
    {
        det_set_key_value(RESET_KEY,KEY_NONE);
        led_control_function(LD_GPRS,LD_ON); 
        app_set_reset_function();/* 将系统设置参数恢复为默认值，需要重启生效 */
    }
            
}

/*
*********************************************************************************************************
*    函 数 名: det_main_network_and_camera_network
*    功能说明: 主网络与摄像头网络状态检查
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t det_main_network_and_camera_network(void)
{
    static uint8_t main_ip[2] = {0};
    static uint8_t camera[6]  = {0};
    uint8_t i = 0;
    uint8_t error = 0;
    
    /* 检测主网络 */
    if(sg_datacollec_t.main_ip == 0 && sg_datacollec_t.main_sub_ip == 0) 
    {
        if(main_ip[0] == 1 || main_ip[1] == 1) {
            main_ip[0] = sg_datacollec_t.main_ip;
            main_ip[1] = sg_datacollec_t.main_sub_ip;
            
            // 重启设备
            app_set_net_reload_num(RELAY_1); // 重启路由端口    
            return 1;
        }
    }
    main_ip[0] = sg_datacollec_t.main_ip;
    main_ip[1] = sg_datacollec_t.main_sub_ip;
    
    /* 检测摄像头 */
    for(i=0; i<6; i++) 
    {
        if(camera[i] == 1 && sg_datacollec_t.camera[i] == 0) 
        {
            camera[0] = sg_datacollec_t.camera[0];
            camera[1] = sg_datacollec_t.camera[1];
            camera[2] = sg_datacollec_t.camera[2];
            camera[3] = sg_datacollec_t.camera[3];
            camera[4] = sg_datacollec_t.camera[4];
            camera[5] = sg_datacollec_t.camera[5];
            
            error++;
        }
    }
    
    if(error > 0)
    {
        error = 0;
        app_set_net_reload_num(RELAY_2); // 摄像机网络
        app_set_net_reload_num(RELAY_3); // 摄像机网络
        return 1;        
    }
    camera[0] = sg_datacollec_t.camera[0];
    camera[1] = sg_datacollec_t.camera[1];
    camera[2] = sg_datacollec_t.camera[2];
    camera[3] = sg_datacollec_t.camera[3];
    camera[4] = sg_datacollec_t.camera[4];
    camera[5] = sg_datacollec_t.camera[5];
    
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: det_get_temphumi_function
*    功能说明: 检测温湿度
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void det_get_temphumi_function(void)
{
    double det_temp = 0;
    double det_humi = 0;
    static uint8_t th_count_time = 0;
    
    th_count_time++;
    if(th_count_time >= 100)  // 2s 2000/20 = 100
    {
        th_count_time = 0;
        if(aht20_measure(&det_humi,&det_temp) == 0) {
            sg_datacollec_t.humi_inside = det_humi;
            sg_datacollec_t.temp_inside = det_temp;
        }
    }
}


/*
*********************************************************************************************************
*    函 数 名: det_get_attitude_state_value
*    功能说明: 检测陀螺仪姿态值
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void det_get_attitude_state_value(void)
{
#if (configUSE_TILT == 1)
    uint16_t value = 0;
    value = LIS3DH_GetAngle();

    if(configUSE_CHIP_ORIENTATION == 1)
        value = 180-value;

    sg_datacollec_t.attitude_acc = value;
#endif
}

/*
*********************************************************************************************************
*    函 数 名: det_set_camera_status
*    功能说明: 设置摄像机网络状态，并按状态设置对应故障码
*    形    参: num-摄像机编号(0~5) status-网络状态(NET_STATUS_E)
*    返 回 值: 无
*********************************************************************************************************
*/
void det_set_camera_status(uint8_t num,uint8_t status)
{
    uint32_t code_fault, code_delay, code_loss;

    if(num >= 6)
    {
        return;
    }

    sg_datacollec_t.camera[num] = status;

    /* 摄像机编号编码在故障码 bit16~19，逐路相差 (1<<16)，由摄像机1的码加 num 偏移得到 */
    code_fault = NET_CAREMA1_FAULT + ((uint32_t)num << 16);
    code_delay = NET_CAREMA1_DELAY + ((uint32_t)num << 16);
    code_loss  = NET_CAREMA1_LOSS  + ((uint32_t)num << 16);
    /* 非正常状态置对应故障码；正常/无IP 时清除全部(摄像机无"无IP"码) */
    switch(status)
    {
        case NET_STATUS_FAULT: Error_Set(code_fault); break;   // 2 故障
        case NET_STATUS_DELAY: Error_Set(code_delay); break;   // 4 延时
        case NET_STATUS_LOSS:  Error_Set(code_loss);  break;   // 5 丢包
        default:                                               // 1 正常 / 0 无IP: 清除全部故障码
            Error_Clear(code_fault);
            Error_Clear(code_delay);
            Error_Clear(code_loss);
            break;
    }
}

/*
*********************************************************************************************************
*    函 数 名: det_get_camera_status
*    功能说明: 获取摄像机网络状态
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t det_get_camera_status(uint8_t num)
{
    return sg_datacollec_t.camera[num];
}

/*
*********************************************************************************************************
*    函 数 名: det_set_main_network_status
*    功能说明: 主网1 状态设置
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void det_set_main_network_status(uint8_t status)
{
    sg_datacollec_t.main_ip = status;

    switch(status)
    {
        case NET_STATUS_FAULT: Error_Set(NET_MAIN_FAULT);        break;   // 2 故障
        case NET_STATUS_DELAY: Error_Set(NET_MAIN_DELAY);        break;   // 4 延时
        case NET_STATUS_LOSS:  Error_Set(NET_MAIN_LOSS);         break;   // 5 丢包
        case NET_STATUS_NO_IP: Error_Set(NET_MAIN_IP_UNCONFIG);  break;   // 0 无IP
        default:                                                          // 1 正常: 清除全部故障码
            Error_Clear(NET_MAIN_FAULT);
            Error_Clear(NET_MAIN_DELAY);
            Error_Clear(NET_MAIN_LOSS);
            Error_Clear(NET_MAIN_IP_UNCONFIG);
            break;
    }
}

/*
*********************************************************************************************************
*    函 数 名: det_get_main_network_status
*    功能说明: 主网1 状态获取
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t det_get_main_network_status(void)
{
    return sg_datacollec_t.main_ip;
}

/*
*********************************************************************************************************
*    函 数 名: det_set_main_network_sub_status
*    功能说明: 主网2 状态设置
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void det_set_main_network_sub_status(uint8_t status)
{
    sg_datacollec_t.main_sub_ip = status;

    switch(status)
    {
        case NET_STATUS_FAULT: Error_Set(NET_MAIN2_FAULT);        break;   // 2 故障
        case NET_STATUS_DELAY: Error_Set(NET_MAIN2_DELAY);        break;   // 4 延时
        case NET_STATUS_LOSS:  Error_Set(NET_MAIN2_LOSS);         break;   // 5 丢包
        case NET_STATUS_NO_IP: Error_Set(NET_MAIN2_IP_UNCONFIG);  break;   // 0 无IP
        default:                                                           // 1 正常: 清除全部故障码
            Error_Clear(NET_MAIN2_FAULT);
            Error_Clear(NET_MAIN2_DELAY);
            Error_Clear(NET_MAIN2_LOSS);
            Error_Clear(NET_MAIN2_IP_UNCONFIG);
            break;
    }
}

/*
*********************************************************************************************************
*    函 数 名: det_get_main_network_sub_status
*    功能说明: 主网2 状态获取
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t det_get_main_network_sub_status(void)
{
    return sg_datacollec_t.main_sub_ip;
}

/*
*********************************************************************************************************
*    函 数 名: det_set_total_energy_bl0910
*    功能说明: 计算BL0910电量参数
*    形    参: 
*    返 回 值: 通道
*    @data   : 数据
*    返 回 值: 无
*********************************************************************************************************
*/
void det_set_total_energy_bl0910(uint8_t num,uint32_t data)
{
    switch(num)
    {
		case 0: sg_datacollec_t.total_energy.voltage = data / BL0910_VOLT_KP;            break;
		case 1: sg_datacollec_t.total_energy.current = data / BL0910_CURR_KP; 	    break;
		case 2: sg_datacollec_t.acport_energy[0].current = data / BL0910_CURR_KP; 		break;
		case 3: sg_datacollec_t.acport_energy[1].current = data / BL0910_CURR_KP; 		break;
		case 4: sg_datacollec_t.acport_energy[2].current = data / BL0910_CURR_KP; 		break;
		case 5: sg_datacollec_t.acport_energy[3].current = data / BL0910_CURR_KP; 		break;
		case 6: sg_datacollec_t.acport_energy[4].current = data / BL0910_CURR_KP; 		break;
		case 7: sg_datacollec_t.acport_energy[5].current = data / BL0910_CURR_KP; 		break;
		case 8: sg_datacollec_t.acport_energy[6].current = data / BL0910_CURR_KP; 		break;
		case 9: sg_datacollec_t.acport_energy[7].current = data / BL0910_CURR_KP; 		break;
		case 10:sg_datacollec_t.acport_energy[0].power = complement_to_original(data) / BL0910_POWER_KP;	break;
		case 11:sg_datacollec_t.acport_energy[1].power = complement_to_original(data) / BL0910_POWER_KP;	break;
		case 12:sg_datacollec_t.acport_energy[2].power = complement_to_original(data) / BL0910_POWER_KP;	break;
		case 13:sg_datacollec_t.acport_energy[3].power = complement_to_original(data) / BL0910_POWER_KP;	break;
		case 14:sg_datacollec_t.acport_energy[4].power = complement_to_original(data) / BL0910_POWER_KP;	break;
		case 15:sg_datacollec_t.acport_energy[5].power = complement_to_original(data) / BL0910_POWER_KP;	break;
		case 16:sg_datacollec_t.acport_energy[6].power = complement_to_original(data) / BL0910_POWER_KP;	break;
		case 17:sg_datacollec_t.acport_energy[7].power = complement_to_original(data) / BL0910_POWER_KP;	break;
		case 18:sg_datacollec_t.total_energy.power = complement_to_original(data) / BL0910_POWER_KP;break;
		case 19: sg_datacollec_t.electricity_current.port[0] = data * BL0910_ELEC_Ke;
                sg_datacollec_t.electricity_current.port[0] += sg_datacollec_t.electricity_all.port[0];		break;
		case 20: sg_datacollec_t.electricity_current.port[1] = data * BL0910_ELEC_Ke;		
                sg_datacollec_t.electricity_current.port[1] += sg_datacollec_t.electricity_all.port[1];		break;
		case 21: sg_datacollec_t.electricity_current.port[2] = data * BL0910_ELEC_Ke;		
                sg_datacollec_t.electricity_current.port[2] += sg_datacollec_t.electricity_all.port[2];		break;
		case 22: sg_datacollec_t.electricity_current.port[3] = data * BL0910_ELEC_Ke;		
                sg_datacollec_t.electricity_current.port[3] += sg_datacollec_t.electricity_all.port[3];		break;
		case 23: sg_datacollec_t.electricity_current.port[4] = data * BL0910_ELEC_Ke;		
                sg_datacollec_t.electricity_current.port[4] += sg_datacollec_t.electricity_all.port[4];		break;
		case 24: sg_datacollec_t.electricity_current.port[5] = data * BL0910_ELEC_Ke;		
                sg_datacollec_t.electricity_current.port[5] += sg_datacollec_t.electricity_all.port[5];		break;
		case 25: sg_datacollec_t.electricity_current.port[6] = data * BL0910_ELEC_Ke;		
                sg_datacollec_t.electricity_current.port[6] += sg_datacollec_t.electricity_all.port[6];		break;
		case 26: sg_datacollec_t.electricity_current.port[7] = data * BL0910_ELEC_Ke;		
                sg_datacollec_t.electricity_current.port[7] += sg_datacollec_t.electricity_all.port[7];		break;
		case 27: sg_datacollec_t.electricity_current.total   = data * BL0910_ELEC_Ke;	
                sg_datacollec_t.electricity_current.total += sg_datacollec_t.electricity_all.total;break;
        default:
            break;
    }
}
/*
*********************************************************************************************************
*    函 数 名: det_set_total_energy_bl0906
*    功能说明: 计算BL0906电量参数
*    形    参: 
*    返 回 值: 通道
*    @data        : 数据
*    返 回 值: 无
*********************************************************************************************************
*/
void det_set_total_energy_bl0906(uint8_t num,float data)
{

}
/*
*********************************************************************************************************
*    函 数 名: det_set_total_energy_bl0942
*    功能说明: 计算BL0942电量参数
*    形    参: 
*    返 回 值: 通道
*    @data        : 数据
*    返 回 值: 无
*********************************************************************************************************
*/
void det_set_total_energy_bl0942(uint8_t num,float data)
{

}

/*
*********************************************************************************************************
*    函 数 名: det_set_total_energy_bl0939
*    功能说明: 计算BL0939电量参数
*    形    参: 
*    返 回 值: 通道
*    @data   : 数据
*    返 回 值: 无
*********************************************************************************************************
*/
void det_set_total_energy_bl0939(uint8_t num,float data)
{
    switch(num)
    {
        case 1: sg_datacollec_t.residual_c  = data / BL0939_CURR_KP ; 	break;
    }
}
/*
*********************************************************************************************************
*    函 数 名: det_set_total_energy_bl0972
*    功能说明: 计算BL0972电量参数
*    形    参: 
*    返 回 值: 通道
*    @data        : 数据
*    返 回 值: 无
*********************************************************************************************************
*/
void det_set_total_energy_bl0972(uint8_t num,float data)
{

}
/*
*********************************************************************************************************
*    函 数 名: det_set_ping_status
*    功能说明: 设置ping状态
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void det_set_ping_status(uint8_t status)
{
    sg_datacollec_t.ping_status = status;
}

/*
*********************************************************************************************************
*    函 数 名: det_get_collect_data
*    功能说明: 获取数据信息
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void *det_get_collect_data(void)
{
    return (&sg_datacollec_t);
}

/*
*********************************************************************************************************
*    函 数 名: det_get_total_energy_handler
*    功能说明: 获取总能量参数
*    形    参: 
*    返 回 值: 0:220V电压 1:220电流 2:220功率 3:220用电量
*    返 回 值: 无
*********************************************************************************************************
*/
float det_get_total_energy_handler(uint8_t num)
{
    switch(num)
    {
        case ENERGY_VOLTAGE: return sg_datacollec_t.total_energy.voltage;
        case ENERGY_CURRENT: return sg_datacollec_t.total_energy.current; 
        case ENERGY_POWER: return sg_datacollec_t.total_energy.power;
        case ENERGY_ENERGY: return sg_datacollec_t.electricity_current.total;
    }
    return  0;
}
/*
*********************************************************************************************************
*    函 数 名: det_get_output_energy_handler
*    功能说明: 获取输出接口参数
*    形    参: channel 通道
*      num   : 参数
*    返 回 值: 无
*********************************************************************************************************
*/
float det_get_output_energy_handler(uint8_t channel,uint8_t num)
{
    if(channel >= RELAY_NUM)
    {
        return 0;
    }
    
    switch(num)
    {
        case ENERGY_VOLTAGE: return (relay_get_status((RELAY_DEV)channel) == RELAY_ON) ? sg_datacollec_t.total_energy.voltage : 0;
        case ENERGY_CURRENT: return sg_datacollec_t.acport_energy[channel].current;
        case ENERGY_POWER: return sg_datacollec_t.acport_energy[channel].power;
        case ENERGY_ENERGY: return sg_datacollec_t.electricity_current.port[channel];
    }
    return  0;
}
/*
*********************************************************************************************************
*    函 数 名: det_get_attitude_state_value
*    功能说明: 获取温度
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
float det_get_inside_temp(void)
{
	return (sg_datacollec_t.temp_inside);
}

/*
*********************************************************************************************************
*    函 数 名: det_get_inside_humi
*    功能说明: 检测内部湿度
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
float det_get_inside_humi(void)
{
	return (sg_datacollec_t.humi_inside);
}
/*
*********************************************************************************************************
*    函 数 名: det_set_key_value
*    功能说明: 设置按键数值
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void det_set_key_value(uint8_t key_id,uint8_t key_value)
{
    sg_datacollec_t.key_s[key_id] = key_value;
}
/*
*********************************************************************************************************
*    函 数 名: det_get_key_value
*    功能说明: 获取按键数值
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t det_get_key_value(uint8_t key_id)
{
    return sg_datacollec_t.key_s[key_id];
}
/*
*********************************************************************************************************
*    函 数 名: det_get_miu_value
*    功能说明: 获取漏电数值
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t det_get_miu_value(void)
{
    return sg_datacollec_t.residual_c;
}
/*
*********************************************************************************************************
*    函 数 名: det_get_cabinet_posture
*    功能说明: 获取箱体姿态
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint16_t det_get_cabinet_posture(void)
{
    return sg_datacollec_t.attitude_acc;
}


