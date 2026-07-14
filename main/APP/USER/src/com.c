#include "main.h"
#include "./User/inc/com.h"

#define COM_HEAD_HEX (0x0F0F)         // 数据头
#define COM_TAIL_HEX (0xFFFF)         // 数据尾

#define COM_REC_HAED_HEX (0xF0F0)     // 接收数据头
#define COM_NUM_VAERSION (0x11)       // 数据版本
static struct com_qn_t sg_comqn_t;    // 请求标识码

static com_parser_t  sg_parser[COM_CH_MAX] = {0};   // 每通道独立组帧状态(静态)
static QueueHandle_t sg_rx_q = NULL;                // 组满帧队列(多生产者/单消费者)

/*
*********************************************************************************************************
*    函 数 名: com_report_get_camera_status
*    功能说明: 获取摄像机工作状态(故障码由 det_set_camera_status 统一设置)
*    形    参: @camera        : 摄像机编号
*    返 回 值: 0-不存在/无IP 1-网络正常 2-网络故障 4-延时 5-丢包
*********************************************************************************************************
*/
uint8_t com_report_get_camera_status(uint8_t camera)
{
    uint8_t ip[4]  = {0};
    uint8_t status = 0;

    if(app_get_camera_function(ip,camera) < 0)
    {
        status = 0;            // 不存在/无IP
    }
    else
    {
        if(eth_get_network_cable_status() == 0)
        {
            status = 2;        // 网线断开 → 网络故障
        }
        else
        {
            status = det_get_camera_status(camera);  // 0-无IP 1-正常 2-故障 4-延时 5-丢包
        }
    }
    return status;
}

/*
*********************************************************************************************************
*    函 数 名: com_report_get_main_network_status
*    功能说明: 获取主网络状态(故障码由 det_set_main_network_status 统一设置)
*    形    参: @main        : 0-主网络1 1-主网络2
*    返 回 值: 0-无IP 1-网络正常 2-网络故障 3-网线断开 4-延时 5-丢包
*********************************************************************************************************
*/
uint8_t com_report_get_main_network_status(uint8_t main)
{
    uint8_t ip[4]  = {0};
    uint8_t status = 0;

    switch(main) 
    {
        case 0:            /* 主网络1 */
            if(eth_get_network_cable_status() == 0) 
            {
                status = 3;                              // 网线断开
            } 
            else 
            {
                app_get_main_network_ping_ip_addr(ip);
                if(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) 
                    status = 0;                          // 无IP
                else 
                    status = det_get_main_network_status();      // 0-无IP 1-正常 2-故障 4-延时 5-丢包
            }
            break;
        case 1:            /* 主网络2 */
            app_get_main_network_sub_ping_ip_addr(ip); 
            if(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) 
                status = 0;                              // 无IP
            else 
                status = det_get_main_network_sub_status();      // 0-无IP 1-正常 2-故障 4-延时 5-丢包
            break;
    }
    return status;
}

/*
*********************************************************************************************************
*    函 数 名: com_report_normally_function
*    功能说明: 正常上报
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void com_report_normally_function(uint8_t *data, uint16_t *len, uint8_t cmd)
{
    struct device_param *device = app_get_device_param_function();
    uint16_t        index   = 0;
    uint16_t        size    = 0;
    uint8_t         crc     = 0;
    uint8_t         str[64] ={0};
    uint8_t         str_buff[16] ={0};
    uint8_t         *p      = 0;
    fp32            temp    = 0;
    
    /* 数据头 */
    data[index++] = '#';
    data[index++] = '#';
    /* 数据长度 */
    data[index++] = '0';
    data[index++] = '0';
    data[index++] = '0';
    data[index++] = '0';
    /* 请求编码QN */
    memset(str,0,sizeof(str));
    
    if(sg_comqn_t.flag == 1) {
        /* 请求上报 */
        sg_comqn_t.flag = 0;
        sprintf((char*)str,"QN=%08d%09d;",sg_comqn_t.qn1,sg_comqn_t.qn2);
        strcat((char*)data,(char*)str);
        sg_comqn_t.qn1 = 0;
        sg_comqn_t.qn2 = 0;
    } else {
        /* 主动上报 */
        sprintf((char*)str,"QN=0;");
        strcat((char*)data,(char*)str);
    }
    
    /* 设备唯一标识TID */
    memset(str,0,sizeof(str));
    sprintf((char*)str,"TID=%x;",device->id.i);
    strcat((char*)data,(char*)str);
    /* 版本号 */
    memset(str,0,sizeof(str));
    sprintf((char*)str,"VER=%02x;",COM_DATA_VERSION);
    strcat((char*)data,(char*)str);
//    strcat((char*)data,"VER=11;");
#if (configUSE_DEVICE_TYPE == 1)
    /** 设备类型 **/
    memset(str,0,sizeof(str));
    sprintf((char*)str,"DEVTYPE=%04x;",DEVICE_TYPE);
    strcat((char*)data,(char*)str);
#endif

    /* 指令参数CP */
    strcat((char*)data,"CP=&&");
    
    /** 系统时间 **/
    memset(str,0,sizeof(str));
    sprintf((char*)str,"DT=%s;",app_get_report_current_time(0));
    strcat((char*)data,(char*)str);

    /** 摄像机的网络状态 **/
    memset(str,0,sizeof(str));
    sprintf((char*)str,"CNS=%01d,%01d,%01d,%01d,%01d,%01d;",
                        com_report_get_camera_status(0),
                        com_report_get_camera_status(1),
                        com_report_get_camera_status(2),
                        com_report_get_camera_status(3),
                        com_report_get_camera_status(4),
                        com_report_get_camera_status(5));
    strcat((char*)data,(char*)str);
    /** 主网网络状态 **/
    memset(str,0,sizeof(str));
    sprintf((char*)str,"MN=%d,%d;", com_report_get_main_network_status(0),
                                    com_report_get_main_network_status(1));
    strcat((char*)data,(char*)str);
    /** 电压、电流 **/
    memset(str,0,sizeof(str));
    Get_Total_Energy_Handler((char*)str_buff,0);
    sprintf((char*)str,"V=%s;",str_buff);
    strcat((char*)data,(char*)str);

    memset(str,0,sizeof(str));
    Get_Total_Energy_Handler((char*)str_buff,1);
    sprintf((char*)str,"A=%s;",str_buff);
    strcat((char*)data,(char*)str);

    /** 总功率 总用电量 **/
    memset(str,0,sizeof(str));
    Get_Total_Energy_Handler((char*)str_buff,2);
    sprintf((char*)str,"APOWER=%s;",str_buff);
    strcat((char*)data,(char*)str);

    memset(str,0,sizeof(str));
    Get_Total_Energy_Handler((char*)str_buff,3);
    sprintf((char*)str,"AKW=%s;",str_buff);
    strcat((char*)data,(char*)str);

    /** 湿度、温度 **/
    memset(str,0,sizeof(str));
    temp = det_get_inside_humi();
    float_to_str(temp, 2,(uint8_t*)str_buff,sizeof(str_buff));    
    sprintf((char*)str,"H=%s;",str_buff);
    strcat((char*)data,(char*)str);

    memset(str,0,sizeof(str));
    temp = det_get_inside_temp();
    float_to_str(temp, 2,(uint8_t*)str_buff,sizeof(str_buff)); 
    sprintf((char*)str,"T=%s;",str_buff);
    strcat((char*)data,(char*)str);

    /** 箱体姿态 **/
    memset(str,0,sizeof(str));
    sprintf((char*)str,"P=%d;",det_get_cabinet_posture());
    strcat((char*)data,(char*)str);    

	/** 继电器状态 **/
    strcat((char*)data, "RELAY=");
    for (uint8_t i = 0; i < RELAY_NUM; i++)
    {
        memset(str_buff, 0, sizeof(str_buff));
        sprintf((char*)str_buff, "%01d", relay_get_status((RELAY_DEV)i));
        strcat((char*)data, (char*)str_buff);
        strcat((char*)data, (i < (RELAY_NUM - 1)) ? "," : ";");
    }

	/** 支路电压 **/
    strcat((char*)data, "CHV=");
    for (uint8_t i = 0; i < RELAY_NUM; i++)
    {
        memset(str_buff, 0, sizeof(str_buff));
        Get_Output_Energy_Handler((char*)str_buff, i, ENERGY_VOLTAGE);
        strcat((char*)data, (char*)str_buff);
        strcat((char*)data, (i < (RELAY_NUM - 1)) ? "," : ";");
    }
	
	/** 支路电流 **/
    strcat((char*)data, "CHA=");
    for (uint8_t i = 0; i < RELAY_NUM; i++)
    {
        memset(str_buff, 0, sizeof(str_buff));
        Get_Output_Energy_Handler((char*)str_buff, i, ENERGY_CURRENT);
        strcat((char*)data, (char*)str_buff);
        strcat((char*)data, (i < (RELAY_NUM - 1)) ? "," : ";");
    }
	
	/** 功率 **/
    strcat((char*)data, "POWER=");
    for (uint8_t i = 0; i < RELAY_NUM; i++)
    {
        memset(str_buff, 0, sizeof(str_buff));
        Get_Output_Energy_Handler((char*)str_buff, i, ENERGY_POWER);
        strcat((char*)data, (char*)str_buff);
        strcat((char*)data, (i < (RELAY_NUM - 1)) ? "," : ";");
    }

	/** 用电量 **/
    strcat((char*)data, "ELEC=");
    for (uint8_t i = 0; i < RELAY_NUM; i++)
    {
        memset(str_buff, 0, sizeof(str_buff));
        Get_Output_Energy_Handler((char*)str_buff, 1 + i, ENERGY_ENERGY);
        strcat((char*)data, (char*)str_buff);
        strcat((char*)data, (i < (RELAY_NUM - 1)) ? "," : ";");
    }
    
    /** 信号强度 **/
    memset(str,0,sizeof(str));
    sprintf((char*)str,"CSQ=%d;",gprs_get_csq_function());
    strcat((char*)data,(char*)str);    
    
    /** 北斗定位信息只包含经纬度即可 */
#if (configUSE_EXT_GPS == 1)
    atgm336h_data_t *gnss_data = atgm336h_get_gnss_data();
    sprintf((char*)str,"LAT=%.06f;LNG=%.06f;",
                        fabs(gnss_data->latitude),fabs(gnss_data->longitude));
    strcat((char*)data,(char*)str);
#endif
#if (configUSE_SUN_POWER == 1)
    dtu_mppt_report_function(data);
#endif

    // 错误码
	uint8_t error_buf_str[512] = {0};	
	Error_Get_Codesbuf(error_buf_str, sizeof(error_buf_str));	
	strcat((char*)data,(char*)error_buf_str);

    strcat((char*)data,"&&");
    
    /* 数据长度 */
    size = strlen((char*)data);
    sprintf((char*)str,"%04d",size-6);
    data[2] = str[0];
    data[3] = str[1];
    data[4] = str[2];
    data[5] = str[3];
    /* CRC校验 */
    p = (uint8_t*)strstr((char*)data,"&&");
    if(p == 0) {
        *len = 0;
    } 
    else 
    {
        crc = calc_crc8(p,strlen((char*)p));
        memset(str,0,sizeof(str));
        sprintf((char*)str,"%02x##",crc);
        strcat((char*)data,(char*)str);
        *len = strlen((char*)data);
    }
}

/*
*********************************************************************************************************
*    函 数 名: com_query_configuration_function
*    功能说明: 查询配置
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void com_query_configuration_function(uint8_t *pdata, uint16_t *len)
{
    struct device_param     *device     = app_get_device_param_function();
    struct remote_ip        *remote     = app_get_remote_network_function();
    struct local_ip_t       *local      = app_get_local_network_function();
           com_param_t      *comparam   = app_get_com_time_infor();
           carema_t         *ipc        = app_get_carema_param_function();
    struct threshold_params *threshol   = app_get_threshold_param_function();
    struct update_addr    *ota          = app_get_http_ota_function();    
    uint8_t name[5]     = {0};
    char  temp[128]     = {0};
    char  crc_buff[20]  = {0};
    uint8_t crc         = 0;
    uint8_t index       = 0;
    
    /* 生成校验码 */
#if (configUSE_DEVICE_TYPE == 1)
    sprintf(crc_buff, "%02x%04x%dE1", COM_DATA_VERSION, DEVICE_TYPE, device->id.i);
#else
    sprintf(crc_buff, "%02x%dE1", COM_DATA_VERSION, device->id.i);
#endif
    crc = calc_crc8((uint8_t *)crc_buff, strlen(crc_buff));
    
    my_cjson_create_function(pdata,0); // 开始
    my_cjson_join_int_function(pdata,(uint8_t *)"code",0,1);
    my_cjson_data_create_function(pdata,0); // 开始

    /* 版本号 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%02x",COM_DATA_VERSION);
    my_cjson_join_string_function(pdata,(uint8_t *)"ver",(uint8_t *)temp,1);
#if (configUSE_DEVICE_TYPE == 1)
    /* 设备类型 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%04x",DEVICE_TYPE);
    my_cjson_join_string_function(pdata,(uint8_t *)"devtype",(uint8_t *)temp,1);
#endif
//    my_cjson_join_string_function(pdata,(uint8_t *)"ver",(uint8_t *)"11",1);
    /* 添加QN */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%08d%09d",sg_comqn_t.qn1,sg_comqn_t.qn2);
    my_cjson_join_string_function(pdata,(uint8_t *)"qn",(uint8_t *)temp,1);
    sg_comqn_t.qn1 = 0;
    sg_comqn_t.qn2 = 0;
    sg_comqn_t.flag = 0;
    
    /* ID */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%x",device->id.i);
    my_cjson_join_string_function(pdata,(uint8_t *)"tid",(uint8_t *)temp,1);
    /* 通信命令 */
    my_cjson_join_string_function(pdata,(uint8_t *)"cmd",(uint8_t *)"E1",1);

    /* 设备名称 */
    my_cjson_join_string_function(pdata,(uint8_t *)"tn",(uint8_t *)app_get_device_name(),1);
    /* 终端序列号 */
    memset(temp,0,sizeof(temp));
    start_get_device_id_str((uint8_t*)temp);
    my_cjson_join_string_function(pdata,(uint8_t*)"tsn",(uint8_t*)temp,1);

    /* 内外服务器IP、域名+端口 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%s:%d",remote->inside_iporname,remote->inside_port);
    my_cjson_join_string_function(pdata,(uint8_t*)"isi",(uint8_t*)temp,1);
    /* 外网服务器IP、域名+端口 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%s:%d",remote->outside_iporname,remote->outside_port);
    my_cjson_join_string_function(pdata,(uint8_t*)"osi",(uint8_t*)temp,1);
    
    /* 升级服务器IP、域名+端口 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d.%d.%d.%d:%d",ota->ip[0],ota->ip[1],ota->ip[2],ota->ip[3],ota->port);
    my_cjson_join_string_function(pdata,(uint8_t*)"usi",(uint8_t*)temp,1);

    /* SIM卡序列号 */
    my_cjson_join_string_function(pdata,(uint8_t*)"iccid",(uint8_t*)gsm_get_sim_ccid_function(),1);
    /* 4G模组IMEI */
    my_cjson_join_string_function(pdata,(uint8_t*)"imei",(uint8_t*)gprs_get_imei_function(),1);
    /* SIM卡信号强度 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",gprs_get_csq_function());
    my_cjson_join_string_function(pdata,(uint8_t*)"dbm",(uint8_t*)temp,1);
    /* 传输模式 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",app_get_network_mode());
    my_cjson_join_string_function(pdata,(uint8_t*)"tm",(uint8_t*)temp,1);
    /* 网卡MAC地址 */
    my_cjson_join_string_function(pdata,(uint8_t*)"mac",lwip_get_mac_addr(),1);
    /* IP */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
    my_cjson_join_string_function(pdata,(uint8_t*)"ip",(uint8_t*)temp,1);
    /* 子网掩码 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d.%d.%d.%d",local->netmask[0],local->netmask[1],local->netmask[2],local->netmask[3]);
    my_cjson_join_string_function(pdata,(uint8_t*)"nm",(uint8_t*)temp,1);
    /* 网关 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d.%d.%d.%d",local->gateway[0],local->gateway[1],local->gateway[2],local->gateway[3]);
    my_cjson_join_string_function(pdata,(uint8_t*)"gw",(uint8_t*)temp,1);
    /* 主网检测IP */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d.%d.%d.%d,%d.%d.%d.%d", local->ping_ip[0],local->ping_ip[1],local->ping_ip[2],local->ping_ip[3],
                                            local->ping_sub_ip[0],local->ping_sub_ip[1],local->ping_sub_ip[2],local->ping_sub_ip[3]);
    my_cjson_join_string_function(pdata,(uint8_t*)"mip",(uint8_t*)temp,1);
    /* 上报间隔 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d,%d,%d",comparam->report/1000,0,0);
    my_cjson_join_string_function(pdata,(uint8_t*)"rt",(uint8_t*)temp,1);
    /* ping每轮间隔时间 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",comparam->ping/1000);
    my_cjson_join_string_function(pdata,(uint8_t*)"pl",(uint8_t*)temp,1);

    /* ping每个设备的间隔时间 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",comparam->dev_ping/1000);
    my_cjson_join_string_function(pdata,(uint8_t*)"pn",(uint8_t*)temp,1);

    /* 风扇启动、停止温度 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d,%d",threshol->temp_high,threshol->temp_low);
    my_cjson_join_string_function(pdata,(uint8_t*)"ft",(uint8_t*)temp,1);

    /* 风扇启动湿度 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d,%d",threshol->humi_high,threshol->humi_low);
    my_cjson_join_string_function(pdata,(uint8_t*)"fh",(uint8_t*)temp,1);

    /* 网络延时时间设置  20220308*/  
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",threshol->net_delay_time);
    my_cjson_join_string_function(pdata,(uint8_t*)"pt",(uint8_t*)temp,1);

    /* 摄像机IP */
    for(index=0;index<6;index++)
    {
        memset(temp,0,sizeof(temp));
        sprintf(temp,"%d.%d.%d.%d", ipc->ip[index][0],ipc->ip[index][1],\
                                    ipc->ip[index][2],ipc->ip[index][3]);
        sprintf((char*)name,"c%d",index+1);
        my_cjson_join_string_function(pdata,name,(uint8_t*)temp,1);
    }
#if (configUSE_DEVICE_TYPE == 1)    
    /* 过压*/
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",threshol->volt_max);
    my_cjson_join_string_function(pdata,(uint8_t*)"ov",(uint8_t*)temp,1);
    /* 欠压*/
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",threshol->volt_min);
    my_cjson_join_string_function(pdata,(uint8_t*)"uv",(uint8_t*)temp,1);

    /* 过流 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",threshol->current);
    my_cjson_join_string_function(pdata,(uint8_t*)"oc",(uint8_t*)temp,1);

    /* 倾斜角度 */  
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",threshol->angle);
    my_cjson_join_string_function(pdata,(uint8_t*)"angle",(uint8_t*)temp,1);

    /* 漏电 */  
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",threshol->miu);
    my_cjson_join_string_function(pdata,(uint8_t*)"miu",(uint8_t*)temp,1);
#else
    /* 过压、欠压、过流、倾斜度、漏电*/
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d,%d,%d,%d,%d",threshol->volt_max,threshol->volt_min,threshol->current,threshol->angle,threshol->miu);
    my_cjson_join_string_function(pdata,(uint8_t*)"opovc",(uint8_t*)temp,1);
#endif
    /* 重启次数  20220908*/  
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",threshol->net_reload);
    my_cjson_join_string_function(pdata,(uint8_t*)"lannum",(uint8_t*)temp,1);
    
    /* 重启时间  20220908*/  
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",threshol->net_retime);
    my_cjson_join_string_function(pdata,(uint8_t*)"lantime",(uint8_t*)temp,1);
    
    /* 签名校验 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%x",crc);
    my_cjson_join_string_function(pdata,(uint8_t*)"crc",(uint8_t*)temp,0);
    
    my_cjson_data_create_function(pdata,1); // 结束
    my_cjson_create_function(pdata,1); // 结束
    
    *len = strlen((char*)pdata);
    
}
    
/*
*********************************************************************************************************
*    函 数 名: com_heart_pack_function
*    功能说明: 心跳包
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void com_heart_pack_function(uint8_t *data, uint16_t *len)
{
    struct device_param *device = app_get_device_param_function();
    uint8_t             index   = 0;
    uint16_t            crc     = 0;
    
    /* 数据头 */
    data[index++] = (COM_HEAD_HEX&0xff00)>>8; 
    data[index++] = COM_HEAD_HEX&0xff;
    /* 数据版本 */
    data[index++] = COM_DATA_VERSION;

#if (configUSE_DEVICE_TYPE == 1)
    // 设备类型
    data[index++] = (DEVICE_TYPE>>8)&0xff;
    data[index++] = DEVICE_TYPE&0xff;
#endif

    /* 设备ID */
    data[index++] = (device->id.i>>16)&0xff;
    data[index++] = (device->id.i>>8)&0xff;
    data[index++] = (device->id.i>>0)&0xff;
    /* 命令 */
    data[index++] = COM_HEART_UPDATA;
    
    /* 请求标识码 */
    data[index++] = 0;
    data[index++] = 0;
    data[index++] = 0;
    data[index++] = 0;
    
    data[index++] = 0;
    data[index++] = 0;
    data[index++] = 0;
    data[index++] = 0;
    
    /* 数据长度 */
    data[index++] = 0x01;

    /* 数据内容 */
    data[index++] = 0x01;

    /* crc校验 */
    crc = calc_crc8(&data[2],index-2);
    data[index++] = crc;
    
    /* 数据尾 */
    data[index++] = (COM_TAIL_HEX>>8)&0xff;
    data[index++] = (COM_TAIL_HEX)&0xff;
    
    *len = index;
}

/*
*********************************************************************************************************
*    函 数 名: com_ack_function
*    功能说明: 回复数据
*    形    参: ack无
*    返 回 值: 无
*********************************************************************************************************
*/
void com_ack_function(uint8_t *data, uint16_t *len, uint8_t ack, uint8_t error)
{
    struct device_param *device = app_get_device_param_function();
    uint8_t             index   = 0;
    uint16_t            crc     = 0;
    
    /* 数据头 */
    data[index++] = (COM_HEAD_HEX&0xff00)>>8; 
    data[index++] = COM_HEAD_HEX&0xff;
    /* 数据版本 */
    data[index++] = COM_DATA_VERSION;

#if (configUSE_DEVICE_TYPE == 1)
    // 设备类型
    data[index++] = (DEVICE_TYPE>>8)&0xff;
    data[index++] = DEVICE_TYPE&0xff;
#endif

    /* 设备ID */
    data[index++] = (device->id.i>>16)&0xff;
    data[index++] = (device->id.i>>8)&0xff;
    data[index++] = (device->id.i>>0)&0xff;
    /* 命令 */
    data[index++] = ack;
    /* 请求标识码QN */
    if(sg_comqn_t.flag == 1) {
        sg_comqn_t.flag = 0;
    }
    data[index++] = (sg_comqn_t.qn1>>24) & 0xff;
    data[index++] = (sg_comqn_t.qn1>>16) & 0xff;
    data[index++] = (sg_comqn_t.qn1>>8) & 0xff;
    data[index++] = (sg_comqn_t.qn1>>0) & 0xff;
    data[index++] = (sg_comqn_t.qn2>>24) & 0xff;
    data[index++] = (sg_comqn_t.qn2>>16) & 0xff;
    data[index++] = (sg_comqn_t.qn2>>8) & 0xff;
    data[index++] = (sg_comqn_t.qn2>>0) & 0xff;
    
    sg_comqn_t.qn1 = 0;
    sg_comqn_t.qn2 = 0;
    
    /* 数据长度 */
    data[index++] = 0x01;
    
    /* 错误码 */
    data[index++] = error;
    
    /* crc校验 */
    crc = calc_crc8(&data[2],index-2);
    data[index++] = crc;
    
    /* 数据尾 */
    data[index++] = (COM_TAIL_HEX>>8)&0xff;
    data[index++] = (COM_TAIL_HEX)&0xff;
    
    *len = index;
}

/*
*********************************************************************************************************
*    函 数 名: com_version_information
*    功能说明: 上传软件、硬件版本号
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void com_version_information(uint8_t *pdata, uint16_t *size)
{
    struct device_param *device = app_get_device_param_function();
    char  temp[128]     = {0};
    char  crc_buff[50]  = {0};
    uint8_t crc         = 0;

    /* 获取crc校验结果 */
#if (configUSE_DEVICE_TYPE == 1)
    sprintf(crc_buff, "%02x%04x%dE3", COM_DATA_VERSION, DEVICE_TYPE, device->id.i);
#else
    sprintf(crc_buff, "%02x%dE3", COM_DATA_VERSION, device->id.i);
#endif
    crc = calc_crc8((uint8_t *)crc_buff, strlen(crc_buff));
    
    my_cjson_create_function(pdata,0); // 开始
    my_cjson_join_int_function(pdata,(uint8_t *)"code",0,1);
    
    /* 添加QN */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%08d%09d",sg_comqn_t.qn1,sg_comqn_t.qn2);
    my_cjson_join_string_function(pdata,(uint8_t *)"qn",(uint8_t *)temp,1);
    sg_comqn_t.qn1 = 0;
    sg_comqn_t.qn2 = 0;
    sg_comqn_t.flag = 0;
    
    my_cjson_data_create_function(pdata,0); // 开始
    /* 版本号 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%02x",COM_DATA_VERSION);
    my_cjson_join_string_function(pdata,(uint8_t *)"ver",(uint8_t *)temp,1);
#if (configUSE_DEVICE_TYPE == 1)
    /* 设备类型 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%04x",DEVICE_TYPE);
    my_cjson_join_string_function(pdata,(uint8_t *)"devtype",(uint8_t *)temp,1);
#endif

    /* ID */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%x",device->id.i);
    my_cjson_join_string_function(pdata,(uint8_t *)"tid",(uint8_t *)temp,1);
    /* 通信命令 */
    my_cjson_join_string_function(pdata,(uint8_t *)"cmd",(uint8_t *)"E3",1);

    my_cjson_join_string_function(pdata,(uint8_t *)"mod",(uint8_t *)HARD_NO_STR,1);
    my_cjson_join_string_function(pdata,(uint8_t *)"sysver",(uint8_t *)SOFT_NO_STR,1);    

    /* 终端序列号 */
    memset(temp,0,sizeof(temp));
    start_get_device_id_str((uint8_t*)temp);
    my_cjson_join_string_function(pdata,(uint8_t *)"tsn",(uint8_t *)temp,1);

    /* 签名校验 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%x",crc);
    my_cjson_join_string_function(pdata,(uint8_t*)"crc",(uint8_t*)temp,0);
    
    my_cjson_data_create_function(pdata,1); // 结束
    my_cjson_create_function(pdata,1); // 结束
    
    *size = strlen((char*)pdata);
}

/*
*********************************************************************************************************
*    函 数 名: com_ipc_device_information
*    功能说明: 上传摄像机设备信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int com_ipc_device_information(uint8_t *pdata, uint16_t *size)
{
    struct device_param     *device   = app_get_device_param_function();
    IPC_Info_t *ipc_data = onvif_get_ipc_param();

    uint8_t name[5]     = {0};
    char  temp[128]     = {0};
    char  crc_buff[20]    = {0};
    uint8_t crc            = 0;

    /* 生成校验码 */
#if (configUSE_DEVICE_TYPE == 1)
    sprintf(crc_buff, "%02x%04x%dE4", COM_DATA_VERSION, DEVICE_TYPE, device->id.i);
#else
    sprintf(crc_buff, "%02x%dE4", COM_DATA_VERSION, device->id.i);
#endif
    crc = calc_crc8((uint8_t *)crc_buff, strlen(crc_buff));
    
    my_cjson_create_function(pdata,0); // 开始
    my_cjson_join_int_function(pdata,(uint8_t *)"code",0,1);
    
    /* 添加QN */ 
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%08d%09d",sg_comqn_t.qn1,sg_comqn_t.qn2);
    my_cjson_join_string_function(pdata,(uint8_t *)"qn",(uint8_t *)temp,1);
    sg_comqn_t.qn1 = 0;
    sg_comqn_t.qn2 = 0;
    sg_comqn_t.flag = 0;
    
    my_cjson_data_create_function(pdata,0); // 开始
    /* 版本号 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%02x",COM_DATA_VERSION);
    my_cjson_join_string_function(pdata,(uint8_t *)"ver",(uint8_t *)temp,1);
#if (configUSE_DEVICE_TYPE == 1)
    /* 设备类型 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%04x",DEVICE_TYPE);
    my_cjson_join_string_function(pdata,(uint8_t *)"devtype",(uint8_t *)temp,1);
#endif
    
    /* ID */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%x",device->id.i);
    my_cjson_join_string_function(pdata,(uint8_t *)"tid",(uint8_t *)temp,1);
    /* 通信命令 */
    my_cjson_join_string_function(pdata,(uint8_t *)"cmd",(uint8_t *)"E4",1);

    // 摄像机数量
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",ipc_data->ipc_num);
    my_cjson_join_string_function(pdata,(uint8_t *)"num",(uint8_t *)temp,1);

    for(uint8_t i = 0;i<ipc_data->ipc_num;i++)
    {
        memset(temp,0,sizeof(temp));
        memset(name,0,sizeof(name));
        sprintf(temp,"%d.%d.%d.%d", ipc_data->ipc_param[i].ip[0],ipc_data->ipc_param[i].ip[1],\
                                    ipc_data->ipc_param[i].ip[2],ipc_data->ipc_param[i].ip[3]);
        sprintf((char*)name,"ip%d",i);    
        my_cjson_join_string_function(pdata,name,(uint8_t*)temp,1);

        memset(temp,0,sizeof(temp));
        memset(name,0,sizeof(name));
        sprintf(temp,"%02x:%02x:%02x:%02x:%02x:%02x", 
                        ipc_data->ipc_param[i].mac[0],ipc_data->ipc_param[i].mac[1],\
                        ipc_data->ipc_param[i].mac[2],ipc_data->ipc_param[i].mac[3],\
                        ipc_data->ipc_param[i].mac[4],ipc_data->ipc_param[i].mac[5]);
        sprintf((char*)name,"mac%d",i);
        my_cjson_join_string_function(pdata,name,(uint8_t*)temp,1);

        memset(name,0,sizeof(name));
        sprintf((char*)name,"bd%d",i);
        my_cjson_join_int_function(pdata,name,ipc_data->ipc_param[i].brand,1);
        
        memset(temp,0,sizeof(temp));
        memset(name,0,sizeof(name));
        sprintf(temp,"%s",ipc_data->ipc_param[i].model);
        sprintf((char*)name,"mod%d",i);
        my_cjson_join_string_function(pdata,name,(uint8_t*)temp,1);
        
        memset(temp,0,sizeof(temp));
        memset(name,0,sizeof(name));
        sprintf(temp,"%s",ipc_data->ipc_param[i].osd);
        sprintf((char*)name,"osd%d",i);
        my_cjson_join_string_function(pdata,name,(uint8_t*)temp,1);
        
    }

    /* 签名校验 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%x",crc);
    my_cjson_join_string_function(pdata,(uint8_t*)"crc",(uint8_t*)temp,0);
    
    my_cjson_data_create_function(pdata,1); // 结束
    my_cjson_create_function(pdata,1); // 结束
    
    // printf("data:%s\n",pdata);

    *size = strlen((char*)pdata);
    return 0;
} 

/*
*********************************************************************************************************
*    函 数 名: com_device_snmp_information
*    功能说明: 上传SNMP设备信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int com_device_snmp_information(uint8_t *pdata, uint16_t *size)
{
    struct device_param     *device   = app_get_device_param_function();
    snmp_param_t *snmp_param = snmp_get_snmp_param();
    uint8_t snmp_dev_type = app_get_snmp_dev_type_function();//设备类型:0-5摄像机  6:光猫  7:交换机
    uint8_t snmp_ipc_brand = snmp_get_snmp_ipc_brand();
        
    char  temp[128]     = {0};
    char  crc_buff[20]    = {0};
    uint8_t crc            = 0;

    /* 生成校验码 */
#if (configUSE_DEVICE_TYPE == 1)
    sprintf(crc_buff, "%02x%04x%dE7", COM_DATA_VERSION, DEVICE_TYPE, device->id.i);
#else
    sprintf(crc_buff, "%02x%dE7", COM_DATA_VERSION, device->id.i);
#endif
    crc = calc_crc8((uint8_t *)crc_buff, strlen(crc_buff));
    
    my_cjson_create_function(pdata,0); // 开始
    my_cjson_join_int_function(pdata,(uint8_t *)"code",0,1);
    
    /* 添加QN */ 
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%08d%09d",sg_comqn_t.qn1,sg_comqn_t.qn2);
    my_cjson_join_string_function(pdata,(uint8_t *)"qn",(uint8_t *)temp,1);
    sg_comqn_t.qn1 = 0;
    sg_comqn_t.qn2 = 0;
    sg_comqn_t.flag = 0;
    
    my_cjson_data_create_function(pdata,0); // 开始
    /* 版本号 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%02x",COM_DATA_VERSION);
    my_cjson_join_string_function(pdata,(uint8_t *)"ver",(uint8_t *)temp,1);
#if (configUSE_DEVICE_TYPE == 1)
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%04x",DEVICE_TYPE);
    my_cjson_join_string_function(pdata,(uint8_t *)"devtype",(uint8_t *)temp,1);
#endif    
    /* ID */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%x",device->id.i);
    my_cjson_join_string_function(pdata,(uint8_t *)"tid",(uint8_t *)temp,1);
    /* 通信命令 */
    my_cjson_join_string_function(pdata,(uint8_t *)"cmd",(uint8_t *)"E7",1);

    // SNMP设备类型
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",snmp_dev_type+1);
    my_cjson_join_string_function(pdata,(uint8_t *)"num",(uint8_t *)temp,1);

    switch(snmp_dev_type)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            // SNMP设备品牌
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->ipc_param[snmp_ipc_brand][IPC_BRAND]);
            my_cjson_join_string_function(pdata,"brand",(uint8_t*)temp,1);
            // 型号
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->ipc_param[snmp_ipc_brand][IPC_MODEL]);
            my_cjson_join_string_function(pdata,"model",(uint8_t*)temp,1);
            // 序列号
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->ipc_param[snmp_ipc_brand][IPC_SERIAL]);
            my_cjson_join_string_function(pdata,"sid",(uint8_t*)temp,1);
            // CPU使用率
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->ipc_param[snmp_ipc_brand][IPC_CPU]);
            my_cjson_join_string_function(pdata,"cpu",(uint8_t*)temp,1);
            // 内存使用率
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->ipc_param[snmp_ipc_brand][IPC_MEM]);
            my_cjson_join_string_function(pdata,"mem",(uint8_t*)temp,1);
            // 实时码流
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->ipc_param[snmp_ipc_brand][IPC_RATE]);
            my_cjson_join_string_function(pdata,"rate",(uint8_t*)temp,1);
            // mac
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->ipc_param[snmp_ipc_brand][IPC_MAC]);
            my_cjson_join_string_function(pdata,"mac",(uint8_t*)temp,1);
            break;
        case 6:
            // SNMP设备品牌
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->onv_param[snmp_ipc_brand][ONV_BRAND]);
            my_cjson_join_string_function(pdata,"brand",(uint8_t*)temp,1);
            // 型号
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->onv_param[snmp_ipc_brand][ONV_MODEL]);
            my_cjson_join_string_function(pdata,"model",(uint8_t*)temp,1);
            // 光功率
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->onv_param[snmp_ipc_brand][ONV_POWER]);
            my_cjson_join_string_function(pdata,"lpower",(uint8_t*)temp,1);
            // 端口状态
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->onv_param[snmp_ipc_brand][ONV_PORT]);
            my_cjson_join_string_function(pdata,"port",(uint8_t*)temp,1);
        break;
        case 7:
            // SNMP设备品牌
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->switch_param[0].brand);
            my_cjson_join_string_function(pdata,"brand",(uint8_t*)temp,1);
            // 型号
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%s", snmp_param->switch_param[0].device_model);
            my_cjson_join_string_function(pdata,"model",(uint8_t*)temp,1);
            // 端口状态
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", \
                snmp_param->switch_param[0].port_status[0], \
                snmp_param->switch_param[0].port_status[1], \
                snmp_param->switch_param[0].port_status[2], \
                snmp_param->switch_param[0].port_status[3], \
                snmp_param->switch_param[0].port_status[4], \
                snmp_param->switch_param[0].port_status[5], \
                snmp_param->switch_param[0].port_status[6], \
                snmp_param->switch_param[0].port_status[7], \
                snmp_param->switch_param[0].port_status[8], \
                snmp_param->switch_param[0].port_status[9]);
            my_cjson_join_string_function(pdata,(uint8_t*)"port",(uint8_t*)temp,1);
            // POE端口状态
            memset(temp,0,sizeof(temp));
            sprintf(temp,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", \
                snmp_param->switch_param[0].port_poe[0], \
                snmp_param->switch_param[0].port_poe[1], \
                snmp_param->switch_param[0].port_poe[2], \
                snmp_param->switch_param[0].port_poe[3], \
                snmp_param->switch_param[0].port_poe[4], \
                snmp_param->switch_param[0].port_poe[5], \
                snmp_param->switch_param[0].port_poe[6], \
                snmp_param->switch_param[0].port_poe[7], \
                snmp_param->switch_param[0].port_poe[8], \
                snmp_param->switch_param[0].port_poe[9]);
                my_cjson_join_string_function(pdata,(uint8_t*)"poe",(uint8_t*)temp,1);
        break;
    }
    /* 签名校验 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%x",crc);
    my_cjson_join_string_function(pdata,(uint8_t*)"crc",(uint8_t*)temp,0);
    
    my_cjson_data_create_function(pdata,1); // 结束
    my_cjson_create_function(pdata,1); // 结束
    *size = strlen((char*)pdata);
    return 0;
} 

/*
*********************************************************************************************************
*    函 数 名: com_device_ping_information
*    功能说明: 上传ping信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int com_device_ping_information(uint8_t *pdata, uint16_t *size)
{
    struct device_param     *device   = app_get_device_param_function();
    eth_ping_t *ping_info = eth_get_ping_info();
        
    char  temp[128]     = {0};
    char  crc_buff[20]  = {0};
    uint8_t crc         = 0;
    uint8_t index       = 0;
    uint8_t name[5]     = {0};
    uint16_t ping_time  = 0;

    /* 生成校验码 */
#if (configUSE_DEVICE_TYPE == 1)
    sprintf(crc_buff, "%02x%04x%dE8", COM_DATA_VERSION, DEVICE_TYPE, device->id.i);
#else
    sprintf(crc_buff, "%02x%dE8", COM_DATA_VERSION, device->id.i);
#endif
    crc = calc_crc8((uint8_t *)crc_buff, strlen(crc_buff));
    
    my_cjson_create_function(pdata,0); // 开始
    my_cjson_join_int_function(pdata,(uint8_t *)"code",0,1);
    
    /* 添加QN */ 
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%08d%09d",sg_comqn_t.qn1,sg_comqn_t.qn2);
    my_cjson_join_string_function(pdata,(uint8_t *)"qn",(uint8_t *)temp,1);
    sg_comqn_t.qn1 = 0;
    sg_comqn_t.qn2 = 0;
    sg_comqn_t.flag = 0;

    my_cjson_data_create_function(pdata,0); // 开始
    /* 版本号 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%02x",COM_DATA_VERSION);
    my_cjson_join_string_function(pdata,(uint8_t *)"ver",(uint8_t *)temp,1);
#if (configUSE_DEVICE_TYPE == 1)
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%04x",DEVICE_TYPE);
    my_cjson_join_string_function(pdata,(uint8_t *)"devtype",(uint8_t *)temp,1);
#endif    

    /* ID */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%x",device->id.i);
    my_cjson_join_string_function(pdata,(uint8_t *)"tid",(uint8_t *)temp,1);
    /* 通信命令 */
    my_cjson_join_string_function(pdata,(uint8_t *)"cmd",(uint8_t *)"E8",1);

    /* ping信息: st1/dt1 ~ st4/dt4 */
    if (ping_info != NULL)
    {
        for (index = 0; index < 4; index++)
        {
            memset(name, 0, sizeof(name));
            sprintf((char *)name, "st%d", index + 1);
            my_cjson_join_int_function(pdata, name, ping_info->status[index], 1);

            ping_time = (ping_info->status[index] != 0U) ? ping_info->times[index] : 0U;
            memset(name, 0, sizeof(name));
            sprintf((char *)name, "dt%d", index + 1);
            my_cjson_join_int_function(pdata, name, ping_time, 1);
        }
    }

    /* 签名校验 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%x",crc);
    my_cjson_join_string_function(pdata,(uint8_t*)"crc",(uint8_t*)temp,0);
    
    my_cjson_data_create_function(pdata,1); // 结束
    my_cjson_create_function(pdata,1); // 结束
    
    // printf("data:%s\n",pdata);

    *size = strlen((char*)pdata);
    return 0;
} 
	
/*
*********************************************************************************************************
*    函 数 名: com_rs485_up_param_function
*    功能说明: RS485上行数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_rs485_up_param_function(uint8_t *rs485data, uint8_t *data, uint16_t *len)
{
	struct device_param *device = app_get_device_param_function();
	uint8_t 			index   = 0;
	uint16_t			crc     = 0;
	
	/* 帧头 */
	data[index++] = (COM_HEAD_HEX&0xff00)>>8; 
	data[index++] =  COM_HEAD_HEX&0xff;
	/* 数据版本 */
	data[index++] = COM_NUM_VAERSION;

#if (configUSE_DEVICE_TYPE == 1)
    // 设备类型
    data[index++] = (DEVICE_TYPE>>8)&0xff;
    data[index++] = DEVICE_TYPE&0xff;
#endif

	/* 设备ID */
	data[index++] = (device->id.i>>16)&0xff;
	data[index++] = (device->id.i>>8)&0xff;
	data[index++] = (device->id.i>>0)&0xff;
	/* 命令 */
	data[index++] = CONFIGURE_RS485_UOSTREAM;
	
	/* 标识 */
	data[index++] = 0;
	data[index++] = 0;
	data[index++] = 0;
	data[index++] = 0;
	
	data[index++] = 0;
	data[index++] = 0;
	data[index++] = 0;
	data[index++] = 0;
	
	/* 数据长度 */
	data[index++] = rs485data[0];

	/* 数据 */
	for(uint8_t i=0;i < rs485data[0];i++)
		data[index++] = rs485data[i+1];

	/* crc校验 */
	crc = calc_crc8(&data[2],index-2);
	data[index++] = crc;
	
	/* 帧尾 */
	data[index++] = (COM_TAIL_HEX>>8)&0xff;
	data[index++] = (COM_TAIL_HEX)&0xff;
	
	*len = index;
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_configure_server_domain_name
*    功能说明: 处理配置服务器域名函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t com_deal_configure_server_domain_name(com_rec_data_t *buff)
{
    struct remote_ip *remote     = app_get_remote_network_function();
    sys_backups_t    *param_back = app_get_backups_function();
    struct update_addr *ota_p    = app_get_http_ota_function();
    int temp[4] = {0};
    int port[1] = {0};
    int8_t ret = 0;
    char *p1 = NULL;
    char *p2 = NULL;
    uint8_t mode  = 0;
    
    p1 = strstr((char*)buff->buff,"outer##");
    if (p1 != NULL) {
        memset(remote->outside_iporname,0,sizeof(remote->outside_iporname));
        p1 += 7;
        sscanf((char*)p1,"%[^:]:%d",remote->outside_iporname,&remote->outside_port);
        mode = 1;
        goto EXIT;
    }
        
    p1 = strstr((char*)buff->buff,"all##");
    if (p1 != NULL) 
    {
        p1 += 5;
        p2 = p1;
        p1 = strchr((char*)p1,',');
        if( p1 != NULL ) 
        {
            p1[0] = 0;
            p1     += 1;
            
            app_save_backups_remote_param_function();  // 备份服务器信息
            memset(remote->inside_iporname,0,sizeof(remote->inside_iporname));
            memset(remote->outside_iporname,0,sizeof(remote->outside_iporname));
            sscanf((char*)p2,"%[^:]:%d",remote->inside_iporname,&remote->inside_port);
            sscanf((char*)p1,"%[^:]:%d",remote->outside_iporname,&remote->outside_port);
        }
        mode = 1;
        goto EXIT;
    }
    
    p1 = strstr((char*)buff->buff,"update##");
    if (p1 != NULL) {
        memset(ota_p->ip,0,sizeof(ota_p->ip));
        p1 += 8;
        ret = sscanf(p1,"%d.%d.%d.%d:%d",&temp[0],&temp[1],&temp[2],&temp[3],&port[0]);
        ota_p->ip[0] = temp[0];
        ota_p->ip[1] = temp[1];
        ota_p->ip[2] = temp[2];
        ota_p->ip[3] = temp[3];
        ota_p->port  = port[0];
//        sscanf((char*)p1,"%[^:]:%d",ota_p->update_url,&ota_p->update_port);
        mode = 3;
        goto EXIT;
    }
    
    if ( p1 == NULL ) {
        /* 设置回传 */
        app_set_reply_parameters_function(buff->cmd,0x74);
        return ret;
    }
    EXIT:
    /* 设置回传 */
    app_set_send_result_function(SR_OK);
    app_set_reply_parameters_function(buff->cmd,0x01);
    vTaskDelay(100);              // 延时10ms
    
    switch(mode)
    {
        case 1:app_set_save_infor_function(SAVE_FLAG_REMOTE_NETWORK); break;
        case 3:app_set_http_ota_function(*ota_p);  break;
        default:  break;
    }
    return ret;
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_configure_server_mode
*    功能说明: 设置服务器模式
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_configure_server_mode(com_rec_data_t *buff)
{
    struct local_ip_t *local = app_get_local_network_function();

    local->server_mode = buff->buff[0];
    app_set_send_result_function(SR_OK);
    app_set_reply_parameters_function(buff->cmd,0x01);
    vTaskDelay(100);              // 延时10ms
    app_set_save_infor_function(SAVE_FLAG_LOCAL_NETWORK);
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_update_system_function
*    功能说明: 处理更新
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_update_system_function(com_rec_data_t *buff)
{
    /* 更新与上传互斥，任一进行中则拒绝 */
    if((update_get_mode_function() != UPDATE_MODE_NULL) || (upload_get_mode_function() != UPLOAD_MODE_NULL))
    {
        app_set_reply_parameters_function(buff->cmd,0x77);  // 错误，正在更新或上传
    }
    else
    {
        if(gsm_get_network_connect_status_function() != 0)
            update_set_update_mode(UPDATE_MODE_GPRS);
        else
            printf("gsm is not connect!!!\n");

        if(eth_get_tcp_status() == 2)
            update_set_update_mode(UPDATE_MODE_LWIP);
        else
            printf("eth is not connect!!!\n");
    }
}

/*
*********************************************************************************************************
*    函 数 名: com_set_now_time_function
*    功能说明: 设置当前时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_set_now_time_function(com_rec_data_t *buff)
{
    uint32_t time = buff->buff[0];
    
    time = (time<<8)|buff->buff[1];
    time = (time<<8)|buff->buff[2];
    time = (time<<8)|buff->buff[3];
    
    TimeBySecond(time);
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_configure_server_ip_port
*    功能说明: 处理配置服务器IP端口
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_configure_server_ip_port(com_rec_data_t *buff)
{
    struct remote_ip *remote = app_get_remote_network_function();
    uint16_t temp = 0;
    
    memset(remote->inside_iporname,0,sizeof(remote->inside_iporname));
    sprintf((char*)remote->inside_iporname,"%d.%d.%d.%d",buff->buff[0],\
                                                         buff->buff[1],\
                                                         buff->buff[2],\
                                                         buff->buff[3]);
    temp = buff->buff[4];
    remote->inside_port = (temp<<8)|buff->buff[5];
    
    app_set_save_infor_function(SAVE_FLAG_REMOTE_NETWORK);
    app_set_reply_parameters_function(buff->cmd,0x01);
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_configure_local_network
*    功能说明: 配置设备IP、子网掩码、网关
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_configure_local_network(com_rec_data_t *buff)
{
    struct local_ip_t *local = app_get_local_network_function();
    
    local->ip[0] = buff->buff[0];
    local->ip[1] = buff->buff[1];
    local->ip[2] = buff->buff[2];
    local->ip[3] = buff->buff[3];
    
    local->netmask[0] = buff->buff[4];
    local->netmask[1] = buff->buff[5];
    local->netmask[2] = buff->buff[6];
    local->netmask[3] = buff->buff[7];
    
    local->gateway[0] = buff->buff[8];
    local->gateway[1] = buff->buff[9];
    local->gateway[2] = buff->buff[10];
    local->gateway[3] = buff->buff[11];

    /* 设置回传 */
    app_set_send_result_function(SR_OK);
    app_set_reply_parameters_function(buff->cmd,0x01);
    vTaskDelay(100);              // 延时10ms
    
    /* 保存 */
    app_set_save_infor_function(SAVE_FLAG_LOCAL_NETWORK);
    eth_set_network_reset();
    
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_configure_mac
*    功能说明: 配置设备mac
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_configure_mac(com_rec_data_t *buff)
{
    struct local_ip_t *local = app_get_local_network_function();
    
    local->mac[0] = buff->buff[0];
    local->mac[1] = buff->buff[1];
    local->mac[2] = buff->buff[2];
    local->mac[3] = buff->buff[3];
    local->mac[4] = buff->buff[4];
    local->mac[5] = buff->buff[5]; 
    
    /* 设置回传 */
    app_set_send_result_function(SR_OK);
    app_set_reply_parameters_function(buff->cmd,0x01);
    vTaskDelay(100);              // 延时10ms
    
    /* 保存 */
    bsp_WriteCpuFlash_Save(DEVICE_FLASH_STORE,DEVICE_MAC_ADDR,(uint8_t *)&local->mac,6);
//    STMFLASH_Write(DEVICE_MAC_ADDR,(uint32_t *)&local->mac,2);
    app_set_save_infor_function(SAVE_FLAG_LOCAL_NETWORK);
    eth_set_network_reset();
    
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_configure_device_id
*    功能说明: 配置设备ID
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_configure_device_id(com_rec_data_t *buff)
{
    struct device_param  *param  = app_get_device_param_function();
        
    /* 获取ID */
    param->id.i   = (buff->buff[0]<<16)|(buff->buff[1]<<8)|(buff->buff[2]<<0);
    
    /* 设置回传 */
    app_set_send_result_function(SR_OK);
    app_set_reply_parameters_function(buff->cmd,0x01);
    vTaskDelay(100);              // 延时10ms
    
    /* 保存 */
    bsp_WriteCpuFlash_Save(DEVICE_FLASH_STORE,DEVICE_ID_ADDR,(uint8_t*)param->id.c,4);
//    STMFLASH_Write(DEVICE_ID_ADDR,(uint32_t*)param->id.c,1);
    app_set_save_infor_function(SAVE_FLAG_DEVICE_PARAM);
    
    vTaskDelay(100);
    eth_set_tcp_connect_reset();                /* 重启TCP连接 */
    gsm_set_module_reset_function();                /* 重启无线连接 */
}


/*
*********************************************************************************************************
*    函 数 名: com_deal_configure_onvif_carema
*    功能说明: onvif配置摄像机IP
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_configure_onvif_carema(com_rec_data_t *buff)
{

    /* 设置回传 */
    app_set_reply_parameters_function(buff->cmd,0x74);
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_camera_config
*    功能说明: 处理配置摄像头信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_camera_config(com_rec_data_t *buff)
{
    uint8_t num   = buff->buff[0]-1;
    uint8_t ip[4] = {0};
    int8_t  ret   = 0;
    
    ip[0] = buff->buff[1];
    ip[1] = buff->buff[2];
    ip[2] = buff->buff[3];
    ip[3] = buff->buff[4];
    
    if( ip[0] == 0 &&ip[1] == 0 &&ip[2] == 0 &&ip[3] == 0)
    {
        /* 清除指定IP */
        app_set_camera_num_function(ip,num);
        app_set_reply_parameters_function(buff->cmd,0x01);
    }
    else
    {
        IPC_Info_t *ipc_data = onvif_get_ipc_param(); // 获取onvif参数
        for(uint8_t i = 0;i<ipc_data->ipc_num;i++)
        {
            if(ipc_data->ipc_param[i].ip[0] == ip[0] && ipc_data->ipc_param[i].ip[1] == ip[1] && ipc_data->ipc_param[i].ip[2] == ip[2] && ipc_data->ipc_param[i].ip[3] == ip[3])
            {
                /* 设置品牌 */
                app_set_camera_num_brand_function(ipc_data->ipc_param[i].brand,num);
                break;
            }
        }

        ret = app_match_local_camera_ip(ip);
        if(ret != 0)
        {
            /* 设置回传 */
            app_set_reply_parameters_function(buff->cmd,0x74); // IP已存在
        }
        else
        {
            app_set_camera_num_function(ip,num);
            app_set_reply_parameters_function(buff->cmd,0x01);
        }
    }
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_camera_config
*    功能说明: 配置摄像头用户名、密码
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_camera_login(com_rec_data_t *buff)
{
    char namebuf[20] = {0};
    char pwdbuf[20]  = {0};
    int  port_num    = 0;
    uint8_t num   ; // = buff->buff[0]-1;  // 摄像机接口位置（ID）
    char *p1 = NULL;
    char *p2 = NULL;
    char *p3 = NULL;    
    
    num = Ascii2Hex(buff->buff[0])-1;
    
    p1 = (char*)&buff->buff[2];
    p2 = strchr((char*)p1,' ');
    p3 = strchr((char*)(p2+1),' ');
    if ((p1 != NULL) && ( p2 != NULL ) )
    {
        sscanf((char*)p1,"%s",namebuf);
        sscanf((char*)p2,"%s",pwdbuf);
        sscanf((char*)p3," %d",&port_num);    
        
        app_set_camera_login_function(namebuf,pwdbuf,port_num,num);  // 设置用户名、密码、端口
        app_set_reply_parameters_function(buff->cmd,0x01);        /* 设置回传 */        
    }
    else
    {
        app_set_reply_parameters_function(buff->cmd,0x74);        /* 设置回传 */
    }
}
/*
*********************************************************************************************************
*    函 数 名: com_del_device_name
*    功能说明: 
*    形    参: 20220416 新加配置设备名称
*    返 回 值: 
*********************************************************************************************************
*/
void com_set_device_name_function(com_rec_data_t *buff)
{
    struct device_param *param = app_get_device_param_function();
    uint8_t          size                 = 0;
    
    memset(param->name,0,sizeof(param->name));
    size = strlen((const char*)(buff->buff));
//    printf("zhongwen:");
//    for(uint8_t i=0;i<size;i++)
//        printf("%x",buff->buff[i]);
    
    if(size ==0)
    {
        app_set_reply_parameters_function(buff->cmd,0x74);        /* 错误 */
    }
    else
    {
        for(uint8_t index=0; index<size; index++) 
        {
            param->name[index] = buff->buff[index];
        }    
        app_set_save_infor_function(SAVE_FLAG_DEVICE_PARAM);    /* 存储 */
        app_set_reply_parameters_function(buff->cmd,0x01);    /* 设置回传 */
    }
//    for(uint8_t i=0;i<size;i++)
//        printf("%x",param->name[i]);
}

/*
*********************************************************************************************************
*    函 数 名: com_set_threshold_params_function
*    功能说明: 设置阈值
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_set_threshold_params_function(com_rec_data_t *buff)
{    
    uint16_t data[5]={0};
    
    data[0] = (buff->buff[0]<<8|buff->buff[1]); // 前2个字节
    data[1] = (buff->buff[2]<<8|buff->buff[3]); // 前2个字节
    data[2] = (buff->buff[4]<<8|buff->buff[5]); // 前2个字节
    data[3] = (buff->buff[7]);
    data[4] = (buff->buff[8]<<8|buff->buff[9]);
    
    app_set_vol_current_param(data);    /* 存储 */
    app_set_reply_parameters_function(buff->cmd,0x01);
}

/*
*********************************************************************************************************
*    函 数 名: com_set_carema_search_mode_function
*    功能说明: 设置搜索模式
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_set_carema_search_mode_function(com_rec_data_t *buff)
{	
	uint8_t  mode  = buff->buff[0];

	if((mode==0)||(mode > 3))
	{
		app_set_reply_parameters_function(buff->cmd,0x74);		/* 设置回传 */
	}
	else
	{
		app_set_carema_search_mode_function(mode,1);
		app_set_reply_parameters_function(buff->cmd,0x01);		/* 设置回传 */
	}
}
/*
*********************************************************************************************************
*    函 数 名: com_deal_fan_temp_parmaeter
*    功能说明: 处理风扇温度
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_fan_temp_parmaeter(com_rec_data_t *buff)
{
    int8_t data[2] = {0};
    
    data[0] = buff->buff[0];
    data[1] = buff->buff[1];
    
    app_set_fan_param_function(data);

    /* 设置回传 */
    app_set_reply_parameters_function(buff->cmd,0x01);
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_fan_humi_param
*    功能说明: 设置风扇启动湿度参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_fan_humi_param(com_rec_data_t *buff)
{
    uint8_t data[2] = {0};
    
    data[0] = buff->buff[0];
    data[1] = buff->buff[1];
    
    app_set_fan_humi_param_function(data);
    app_set_reply_parameters_function(buff->cmd,0x01);
}

/*
*********************************************************************************************************
*    函 数 名: com_set_next_report_time
*    功能说明: 设置上报间隔时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_set_next_report_time(com_rec_data_t *buff)
{
    uint8_t  sel  = buff->buff[0];
    uint16_t time = (buff->buff[1]<<8|buff->buff[2]);

    if(time == 0)
    {
        /* 设置回传 */
        app_set_reply_parameters_function(buff->cmd,0x74);
    }
    else
    {
        app_set_next_report_time_other(time,sel);
        /* 设置回传 */
        app_set_reply_parameters_function(buff->cmd,0x01);
    }
    
}

/*
*********************************************************************************************************
*    函 数 名: com_set_next_ping_time
*    功能说明: 设置ping的时间间隔
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_set_next_ping_time(com_rec_data_t *buff)
{
    uint16_t time       = (buff->buff[0]<<8|buff->buff[1]); // 前2个字节
    uint8_t     time_dev = buff->buff[2];                      // 后1个字节
    
    if(time == 0)
    {
        /* 设置回传 */
        app_set_reply_parameters_function(buff->cmd,0x74);
    }
    else
    {
        /* 保存参数 */
        app_set_next_ping_time(time, time_dev);
        
        /* 设置回传 */
        app_set_reply_parameters_function(buff->cmd,0x01);
    }
}

/*
*********************************************************************************************************
*    函 数 名: com_set_network_delay_time
*    功能说明: // 配置网络延时时间          20220308
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_set_network_delay_time(com_rec_data_t *buff)
{
    uint8_t     time = buff->buff[0];                
    
    if(time == 0)
    {
        app_set_reply_parameters_function(buff->cmd,0x74);        /* 设置回传 */
    }
    else
    {
        app_set_network_delay_time(time);        /* 保存参数 */
        app_set_reply_parameters_function(buff->cmd,0x01);        /* 设置回传 */
    }
}

/*
*********************************************************************************************************
*    函 数 名: com_set_device_password
*    功能说明: 设置密码
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_set_device_password(com_rec_data_t *buff)
{
    struct device_param  *device = app_get_device_param_function();
    memset(device->password,0,sizeof(device->name));
    sprintf((char*)device->password,DEFALUT_PASSWORD);
    device->default_password = 1; // 默认开机需要修改密码
    
    app_set_save_infor_function(SAVE_FLAG_DEVICE_PARAM);    /* 存储 */
    app_set_reply_parameters_function(buff->cmd,0x01);    /* 设置回传 */    
    
}

/*
*********************************************************************************************************
*    函 数 名: com_set_main_ping_ip
*    功能说明: 设置主机pingip
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_set_main_ping_ip(com_rec_data_t *buff)
{
    uint8_t ip[8];
    
    ip[0] = buff->buff[0];
    ip[1] = buff->buff[1];
    ip[2] = buff->buff[2];
    ip[3] = buff->buff[3];
    
    ip[4] = buff->buff[4];
    ip[5] = buff->buff[5];
    ip[6] = buff->buff[6];
    ip[7] = buff->buff[7];
    
    app_set_main_network_ping_ip(ip);
    
    /* 设置回传 */
    app_set_reply_parameters_function(buff->cmd,0x01);    
}

/*
*********************************************************************************************************
*    函 数 名: com_set_work_time
*    功能说明: 配置箱门、补光灯工作时间段
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_set_work_time(com_rec_data_t *buff,uint8_t mode)
{
    int time[4]        = {0};
    uint16_t temp[2] = {0};

    sscanf((char*)buff->buff,"%d:%d-%d:%d",&time[0],&time[1],&time[2],&time[3]);
    for(uint8_t i=0;i<4;i++)
    {
        if(time[i] < 0)
            time[i] = 0;    
    }
    if(time[0] > 23)
        time[0] = 23;

    if(time[2] > 23)
        time[2] = 23;
    
    if(time[1] > 59)
        time[1] = 59;

    if(time[3] > 59)
        time[3] = 59;
    
    temp[0] = time[0]*60+time[1];
    temp[1] = time[2]*60+time[3];
    if(mode == 0)
        app_set_door_time_function(temp);
    else
        app_set_fill_light_function(temp);
    app_set_reply_parameters_function(buff->cmd,0x01);
}
/*
*********************************************************************************************************
*    函 数 名: com_set_threshold_params_function
*    功能说明: 设置阈值
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_set_network_reload(com_rec_data_t *buff)
{    
    uint16_t data[2]={0};
    
    data[0] = buff->buff[0]; // 1个字节
    data[1] = buff->buff[1]; // 1个字节
    
    app_set_network_reload_param(data);    /* 存储 */
    app_set_reply_parameters_function(buff->cmd,0x01);
}
/************************************************************
* Function name	: com_deal_configure_snmp_oid
* Description	: 配置OID
* Parameter		: len 数据长度
* Return		: 
************************************************************/
void com_deal_configure_snmp_oid(com_rec_data_t *buff,uint8_t len)
{
    snmp_oid_t *snmp = app_get_snmp_oid_function();
    uint8_t ber_buf[32] = {0};
	uint8_t ber_len = 0;
		
	switch(buff->buff[0])
	{
		case 0:  // 海康
		case 1:  // 大华
		case 2:  // 宇视
			memcpy(snmp->ipc_oid[buff->buff[0]][buff->buff[1]-1],buff->buff+2,len-2);
			// 转换为BER编码
			ber_len = snmp_oid_str_to_ber(snmp->ipc_oid[buff->buff[0]][buff->buff[1]-1],ber_buf,32);
			if(ber_len > 0) 
			{
				memset(snmp->ipc_oid_ber[buff->buff[0]][buff->buff[1]-1],0,sizeof(snmp->ipc_oid_ber[buff->buff[0]][buff->buff[1]-1]));
				memcpy(snmp->ipc_oid_ber[buff->buff[0]][buff->buff[1]-1],ber_buf,ber_len);
				snmp->ipc_ber_len[buff->buff[0]][buff->buff[1]-1] = ber_len;
			}
			break;
		case 3:	 // 光猫
			memcpy(snmp->onv_oid[0][buff->buff[1]-1],buff->buff+2,len-2);
			// 转换为BER编码
			ber_len = snmp_oid_str_to_ber(snmp->onv_oid[0][buff->buff[1]-1],ber_buf,32);
			if(ber_len > 0) 
			{
				memset(snmp->onv_oid_ber[0][buff->buff[1]-1],0,sizeof(snmp->onv_oid_ber[0][buff->buff[1]-1]));
				memcpy(snmp->onv_oid_ber[0][buff->buff[1]-1],ber_buf,ber_len);
				snmp->onv_ber_len[0][buff->buff[1]-1] = ber_len;
			}
			break;
		case 4:	 // 交换机
			memcpy(snmp->switch_oid[0][buff->buff[1]-1],buff->buff+2,len-2);
			// 转换为BER编码
			ber_len = snmp_oid_str_to_ber(snmp->switch_oid[0][buff->buff[1]-1],ber_buf,32);
			if(ber_len > 0) 
			{
				memset(snmp->switch_oid_ber[0][buff->buff[1]-1],0,sizeof(snmp->switch_oid_ber[0][buff->buff[1]-1]));
				memcpy(snmp->switch_oid_ber[0][buff->buff[1]-1],ber_buf,ber_len);
				snmp->switch_ber_len[0][buff->buff[1]-1] = ber_len;
			}
			break;
	}
	app_set_save_infor_function(SAVE_FLAG_SNMP_OID);
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_upload_file_function
*    功能说明: 文件上传
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_upload_file_function(com_rec_data_t *buff)
{
    /* 更新与上传互斥，任一进行中则拒绝 */
    if((upload_get_mode_function() != UPLOAD_MODE_NULL) || (update_get_mode_function() != UPDATE_MODE_NULL))
    {
        app_set_reply_parameters_function(buff->cmd,0x77);  // 错误，正在上传或更新
    }
    else
    {
        app_set_reply_parameters_function(buff->cmd,0x01);
        vTaskDelay(1000);

        if(gsm_get_network_connect_status_function() != 0)
            upload_set_upload_mode(UPLOAD_MODE_GPRS);
        else
            printf("gsm is not connect!!!\n");

        if(eth_get_tcp_status() == LWIP_TCP_CONNECT)
            upload_set_upload_mode(UPLOAD_MODE_LWIP);
        else
            printf("eth is not connect!!!\n");
    }
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_configure_rs485_format
*    功能说明: 配置485数据格式
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_configure_rs485_format(com_rec_data_t *buff)
{
    rs485_config_t *config = dtu_get_rs485_config_function();
	rs485_config_t new_config = {0};
	float float_value = 0.0f;

	/* 读取波特率值（4字节） */
	new_config.baudrate = ( (buff->buff[0] << 24) | (buff->buff[1] << 16) |
                            (buff->buff[2] << 8)  | buff->buff[3]);

	/* 读取数据位长度 */
	new_config.data_bits = (uint8_t)buff->buff[4];

	/* 读取奇偶校验位 */
	new_config.parity = buff->buff[5];

	/* 将byte 6、7、8、9转换为float数 */
	if (buff->size >= 10) {
		union {
			uint8_t bytes[4];
			float value;
		} converter;

		converter.bytes[0] = buff->buff[6];
		converter.bytes[1] = buff->buff[7];
		converter.bytes[2] = buff->buff[8];
		converter.bytes[3] = buff->buff[9];
		float_value = converter.value;
		new_config.stop_bits = float_value;
	}

	/* 验证数据格式参数是否合法 */
	if( (new_config.baudrate < 1200 || new_config.baudrate > 115200) ||
        (new_config.data_bits != 8 && new_config.data_bits != 9) ||
        (new_config.parity > 2) ||
        (new_config.stop_bits != 0.5f && new_config.stop_bits != 1.0f &&
        new_config.stop_bits != 1.5f && new_config.stop_bits != 2.0f))
	{
		app_set_reply_parameters_function(buff->cmd, 0x74);
		return;
	}

	/* 与现有配置对比，相同则不处理 */
	if( config->baudrate == new_config.baudrate &&
        config->data_bits == new_config.data_bits &&
        config->parity == new_config.parity &&
        config->stop_bits == new_config.stop_bits)
	{
        app_set_reply_parameters_function(buff->cmd, 0x01);
		return;
	}

	config->baudrate = new_config.baudrate;
	config->data_bits = new_config.data_bits;
	config->parity = new_config.parity;
	config->stop_bits = new_config.stop_bits;

	app_set_reply_parameters_function(buff->cmd, 0x01);
	app_set_save_infor_function(SAVE_FLAG_RS485_CONFIG);
	if(dtu_config_sem != NULL) {
		xSemaphoreGive(dtu_config_sem);
	}
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_configure_rs485_uostream
*    功能说明: 配置485发送数据队列
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_configure_rs485_uostream(com_rec_data_t *buff)
{
	uint16_t data_len = buff->size;  // 获取数据长度

	// 1. 检查数据长度是否有效
	if(data_len <= 0 || data_len > 256) // 限制最大数据长度为256字节
	{
		// 数据长度无效，返回错误
		app_set_reply_parameters_function(buff->cmd, 0x74);
		return;
	}
	
	// 2. 将下发的命令数据放入发送队列
	if(dtu_transmit_enqueue(buff->buff, data_len) != pdTRUE)
	{
		// 发送队列已满，返回错误
		app_set_reply_parameters_function(buff->cmd, 0x74);
	}	
	else
	{	
	}
}
/*
*********************************************************************************************************
*    函 数 名: com_deal_ack_parameter
*    功能说明: 处理回复数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_deal_ack_parameter(com_rec_data_t *buff)
{
    uint8_t error = buff->buff[0];
    
    if(error == 0x01)
        app_set_send_result_function(SR_OK);
    else
        app_set_send_result_function(SR_ERROR);
}
/*
*********************************************************************************************************
*    函 数 名: com_query_processing_function
*    功能说明: 查询处理函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_query_processing_function(uint8_t cmd, uint8_t* data)
{
    if(cmd == CR_QUERY_PING_INFO)          // 查询Ping信息
    {
        eth_set_ping_ip_address(data);
    }
	app_set_com_send_flag_function(cmd,data[0]);
}

/*
*********************************************************************************************************
*    函 数 名: com_recevie_function_init
*    功能说明: 通信接收初始化函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void com_recevie_function_init(void)
{
    my_mem_set(sg_parser, 0, sizeof(sg_parser));
    sg_rx_q = xQueueCreate(COM_RX_Q_DEPTH, sizeof(com_frame_t));
    configASSERT(sg_rx_q != NULL);
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_main_function
*    功能说明: 通信接收处理函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
static int8_t com_deal_frame(com_frame_t *frame)
{
    struct device_param *device     = app_get_device_param_function();
    com_rec_data_t      recdata_t   = {0};
    uint32_t             temp       = 0;
#if (configUSE_DEVICE_TYPE == 1)
    uint16_t            device_type = 0;
#endif
    uint8_t            *rec_buff    = frame->buf;
    uint16_t            size        = frame->len;
    uint8_t             crc         = 0;
    uint8_t             ret         = 0;

    recdata_t.src = frame->src;
    app_set_reply_src(frame->src);   /* 记录来源通道, ACK/查询响应原路返回 */

    if(size != 0)
    {

        /* 校验CRC:去除crc校验与数据尾 */
        crc = calc_crc8(&rec_buff[2],size-5);
        if(crc != rec_buff[size-3])
        {
            /* CRC校验错误 */
            ret = CR_CHECK_ERROR;
            app_set_reply_parameters_function(recdata_t.cmd,ret);
            goto __ERROR;
        }
        rec_buff[size-3] = 0; // 将CRC校验对应的数据直接转换为字符串尾
        
        /* 获取版本 */
        recdata_t.version = rec_buff[2];
        
        /* 数据版本 */
        if(recdata_t.version != COM_DATA_VERSION)
//        if(recdata_t.version != COM_NUM_VAERSION)
        {
            /* 数据版本错误 */
            goto __ERROR;
        }
        
#if (configUSE_DEVICE_TYPE == 1)
        /* 获取设备类型 */
        device_type = (rec_buff[3] << 8) | rec_buff[4];
        /* 获取ID */
        recdata_t.id = (rec_buff[5] << 16) | (rec_buff[6] << 8) | (rec_buff[7] << 0);
        /* 获取指令类型 */
        recdata_t.cmd = rec_buff[8];

        /* 设备类型验证 */
        if(device_type != DEVICE_TYPE)
        {
            ret = CR_DEVICE_NUMBER_ERROR;
            app_set_reply_parameters_function(recdata_t.cmd,ret);
            goto __ERROR;
        }

        /* ID验证 */
        if(recdata_t.id != device->id.i && recdata_t.cmd != CONFIGURE_NOW_TIME)
        {
            /* ID错误 */
            ret = CR_DEVICE_NUMBER_ERROR;
            app_set_reply_parameters_function(recdata_t.cmd,ret);
            goto __ERROR;
        }

        /* 获取请求标识码 */
        temp = rec_buff[9];
        temp = (temp<<8)|rec_buff[10];
        temp = (temp<<8)|rec_buff[11];
        temp = (temp<<8)|rec_buff[12];
        sg_comqn_t.qn1 = temp;
        temp = rec_buff[13];
        temp = (temp<<8)|rec_buff[14];
        temp = (temp<<8)|rec_buff[15];
        temp = (temp<<8)|rec_buff[16];
        sg_comqn_t.qn2 = temp;
        /* 获取长度 */
        recdata_t.size = rec_buff[17];
        /* 获取内容 */
        recdata_t.buff = &rec_buff[18];
#else
        /* 获取ID */
        recdata_t.id = (rec_buff[3] << 16) | (rec_buff[4] << 8) | (rec_buff[5] << 0);
        /* 获取指令类型 */
        recdata_t.cmd = rec_buff[6];

        /* ID验证 */
        if(recdata_t.id != device->id.i && recdata_t.cmd != CONFIGURE_NOW_TIME)
        {
            /* ID错误 */
            ret = CR_DEVICE_NUMBER_ERROR;
            app_set_reply_parameters_function(recdata_t.cmd,ret);
            goto __ERROR;
        }

        /* 获取请求标识码 */
        temp = rec_buff[7];
        temp = (temp<<8)|rec_buff[8];
        temp = (temp<<8)|rec_buff[9];
        temp = (temp<<8)|rec_buff[10];
        sg_comqn_t.qn1 = temp;
        temp = rec_buff[11];
        temp = (temp<<8)|rec_buff[12];
        temp = (temp<<8)|rec_buff[13];
        temp = (temp<<8)|rec_buff[14];
        sg_comqn_t.qn2 = temp;
        /* 获取长度 */
        recdata_t.size = rec_buff[15];
        /* 获取内容 */
        recdata_t.buff = &rec_buff[16];
#endif
        printf("recv cmd:0x%02x\n",recdata_t.cmd);
        /* 根据命令解析数据 */
        switch(recdata_t.cmd)
        {
            /* 配置指令 */
            case CONFIGURE_SERVER_DOMAIN_NAME:  // 设置服务器信息
                com_deal_configure_server_domain_name(&recdata_t);
                break;
            case CONFIGURE_LOCAL_NETWORK:         // 设置本地网络信息
                com_deal_configure_local_network(&recdata_t);
                break;
            case CONFIGURE_CAMERA_CONFIG:        // 设置摄像头IP
                com_deal_camera_config(&recdata_t);
                break;
            case CONFIGURE_IPC_LOGIN_INFO:        // 设置摄像头用户名、密码  20220329
                com_deal_camera_login(&recdata_t);
                break;
            case CONFIGURE_FAN_PARAMETER:        // 设置风扇温度
                com_deal_fan_temp_parmaeter(&recdata_t);
                break;
            case CONFIGURE_HEART_TIME:            // 设置上报时间
                com_set_next_report_time(&recdata_t);
                break;
            case CONFIGURE_PING_INTERVAL:        // 设置ping间隔时间
                com_set_next_ping_time(&recdata_t);
                break;
            case CONFIGURE_NETWORK_DELAY:        // 配置网络延时时间          20220308
                com_set_network_delay_time(&recdata_t);
                break;
            case CONFIGURE_MAIN_NETWORK_IP:     // 设置主网络
                com_set_main_ping_ip(&recdata_t);
                break;
            case CONFIGURE_FAN_HUMI:            // 设置风扇湿度
                com_deal_fan_humi_param(&recdata_t);
                break;
            case CONFIGURE_SERVER_MODE:         // 配置网络连接模式
                com_deal_configure_server_mode(&recdata_t);
                break;
            case CONFIGURE_UPDATE_SYSTEM:        // 启用系统更新
                com_deal_update_system_function(&recdata_t);
                break;
            case CONFIGURE_NOW_TIME:                 // 配置当前时间
                com_set_now_time_function(&recdata_t);
                break;
            case CONFIGURE_DEVICE_NAME:              // 设置设备名称
                com_set_device_name_function(&recdata_t);
                break;
            case CONFIGURE_THRESHOLD_PARAMS:        // 配置阈值 
                com_set_threshold_params_function(&recdata_t);
                break;
            case CONFIGURE_SEARCH_MODE:          // 配置搜索模式
                com_set_carema_search_mode_function(&recdata_t);
                break;
            case CONFIGURE_DEVICE_ID:             // 设置设备ID  20231026
                com_deal_configure_device_id(&recdata_t);
                break;
            case CONFIGURE_ONVIF_CAREMA:          // 搜索协议配置摄像机IP  20240201
                com_deal_configure_onvif_carema(&recdata_t);
                break;
            case CONFIGURE_FILL_LIGHT_TIME:        // 设置补光灯时间
                com_set_work_time(&recdata_t,1);
                break;
            case CONFIGURE_DOOR_TIME:              // 设置箱门时间
                com_set_work_time(&recdata_t,0);
                break;    
            case CONFIGURE_NETWOR_RELOAD:  // 配置网络传输设备电源
                com_set_network_reload(&recdata_t);
                break;                
            case CONFIGURE_SNMP_OID:  // 配置SNMP OID
                com_deal_configure_snmp_oid(&recdata_t,recdata_t.size);
                break;            
            case CONFIGURE_UPLOAD_FILE:        // 文件上传
                com_deal_upload_file_function(&recdata_t);
                break;
			case CONFIGURE_RS485_DATA_FORMAT: 		// 配置485数据格式
				com_deal_configure_rs485_format(&recdata_t);
				break;
			case CONFIGURE_RS485_UOSTREAM: 	// 配置485透传
				com_deal_configure_rs485_uostream(&recdata_t);

            /* 查询指令 */
            case CR_QUERY_CONFIG:               // 查询设备当前参数设置 - 对应上传查询配置
            case CR_QUERY_INFO:                 // 立即上报设备状态        - 正常上报
            case CR_QUERY_SOFTWARE_VERSION:     // 查询设备软件版本号
            case CR_QUERY_IPC_IP:               // 查询摄像机IP地址    20220329
            case CR_QUERY_IPC_INFO:             // 查询摄像机相关参数  20220329
            case CR_QUERY_SNMP_INFO:
            case CR_QUERY_PING_INFO:                
                sg_comqn_t.flag = 1;
                com_query_processing_function(recdata_t.cmd,recdata_t.buff);
                break;
            
            /* 操作指令 */
            case CR_SINGLE_CAMERA_CONTROL:
            case CR_POWER_RESETART:
            case CR_GPRS_NETWORK_RESET:
            case CR_LWIP_NETWORK_RESET:
            case CR_IPC_REBOOT:            // IPC重启  20220329
            case CR_GPRS_NETWORK_V_RESET:
                app_set_sys_opeare_function(recdata_t.cmd,recdata_t.buff[0]);
                break;
            
            /* 开关控制 */
            case CONTROL_FAN:
            case CONTROL_OUT_PWR:
                app_set_peripheral_switch(recdata_t.cmd,recdata_t.buff[0],recdata_t.buff[1]);
                break;
            
            default:
                com_deal_ack_parameter(&recdata_t);
                break;
        }
    } 
    else 
        size = 0;
    
    return ret;
__ERROR:
    printf("com_deal_cmd error\n");
    return ret;
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_main_function
*    功能说明: 通信接收处理主函数(事件驱动:阻塞等待帧队列, 有帧即处理, 消除轮询延迟)
*    形    参: 无
*    返 回 值: 处理结果
*********************************************************************************************************
*/
int8_t com_deal_main_function(void)
{
    com_frame_t frame;

    if(sg_rx_q == NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        return 0;
    }

    /* 阻塞等待一帧, 最长10ms; 帧到达即刻唤醒处理 */
    if(xQueueReceive(sg_rx_q, &frame, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        return com_deal_frame(&frame);
    }
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: com_rx_feed
*    功能说明: 生产者接口:将某通道收到的原始字节喂入该通道独立的组帧状态机,
*              组满一整帧后打上来源通道标签投递到帧队列(多生产者安全)
*    形    参: @ch  : 来源通道 com_channel_e
*              @data: 原始字节缓冲
*              @len : 字节数
*    返 回 值: 无
*********************************************************************************************************
*/
void com_rx_feed(com_channel_e ch, uint8_t *data, uint16_t len)
{
    com_parser_t *p;
    com_frame_t   frame;
    uint16_t      i;
    uint8_t       d;

    if(ch >= COM_CH_MAX || data == NULL || len == 0)
        return;

    p = &sg_parser[ch];

    /* 帧间超时:距上次收到字节过久则丢弃半截帧(替代原1ms递减的 com_queue_time_function) */
    if(p->pos != 0 && (HAL_GetTick() - p->last_tick) > COM_FRAME_CUTDOWN_MS)
        p->pos = 0;
    p->last_tick = HAL_GetTick();

    for(i = 0; i < len; i++)
    {
        d = data[i];
        p->head = (uint16_t)((p->head << 8) | d);

        /* 未对齐帧头前, 持续滑动查找帧头 */
        if(p->pos == 0 && p->head != COM_REC_HAED_HEX)
            continue;

        /* 补齐帧头第一个字节 */
        if(p->pos == 0)
        {
            p->buf[p->pos++] = (COM_REC_HAED_HEX >> 8) & 0xff;
            p->body_size = 0;
        }

#if (configUSE_DEVICE_TYPE == 1)
        if(p->pos == (9 + 8)) p->body_size = d;
#else
        if(p->pos == (7 + 8)) p->body_size = d;
#endif

        if(p->pos < COM_FRAME_MAX)
        {
            p->buf[p->pos++] = d;
        }
        else
        {
            /* 超出缓冲, 丢弃复位 */
            p->pos = 0; p->tail = 0; p->body_size = 0;
            continue;
        }

        p->tail = (uint16_t)((p->tail << 8) | d);

        /* 帧尾匹配且长度足够 → 得到完整帧 */
#if (configUSE_DEVICE_TYPE == 1)
        if(p->tail == COM_TAIL_HEX && ((p->body_size + 21) <= p->pos))
#else
        if(p->tail == COM_TAIL_HEX && ((p->body_size + 19) <= p->pos))
#endif
        {
            frame.src = (uint8_t)ch;
            frame.len = p->pos;
            memcpy(frame.buf, p->buf, p->pos);
            if(sg_rx_q != NULL)
                xQueueSend(sg_rx_q, &frame, 0);
            p->pos = 0; p->tail = 0; p->body_size = 0;
            continue;
        }

        /* 协议数据出错(已达预期长度却无帧尾) */
#if (configUSE_DEVICE_TYPE == 1)
        if((21 + p->body_size) <= p->pos)
#else
        if((19 + p->body_size) <= p->pos)
#endif
        {
            p->pos = 0; p->tail = 0; p->body_size = 0;
        }
    }
}

