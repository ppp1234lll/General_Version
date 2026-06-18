#include "main.h"
#include "./Task/inc/app.h"

/* 发送状态 */
#define SEND_STATUS_NO      (0) // 当前没有发送
#define SEND_STATUS_SENGING (1) // 当前正在发送
#define SEND_STATUS_RESULT  (2) // 当前发送有了结果

#define SERVER_LINK_TIME    (30000) // 服务器连接时间  5min = 30000ms/10ms
#define NETWAOK_RELOAD_TIME   60
typedef struct
{
    struct
    {
        send_result_e  send_result; // 发送结果
        uint32_t       heart_time;    // 心跳计时
        uint32_t       report_time;     // 上报时间
        uint16_t       send_time;       // 发送计时
        uint8_t        send_mode;       // send mode:0-LWIP 1-GPRS
        uint8_t        repeat;             // 重复次数: 针对发送
        uint8_t        send_status;   // 发送状态: 0-当前没有发送 1-当前有发送 2-发送有结果
        uint8_t        send_cmd;         // 发送内容，直接使用对应命令字
        uint8_t        return_cmd;      // 回复标志
        uint8_t        return_error;     // 回复内容
    } com;
    struct
    {
        uint8_t save_device_param;       // 存储设备参数
        uint8_t save_local_network;      // 保存本地网络参数
        uint8_t save_remote_network;     // 保存远端网络参数
        uint8_t save_update_addr;        // 保存更新地址
        uint8_t com_parameter;                 // 通信相关参数
        uint8_t save_carema;               // 摄像头参数 20230712
        uint8_t save_threshold;       // 阈值
        uint8_t save_reset;                     // 恢复出厂化
        uint8_t save_snmp_oid;               // 保存SNMP OID参数
    } save_flag;
    struct
    {
        uint8_t report_normally;       // 正常上报
        uint8_t query_configuration; // 查询配置上传
        uint8_t heart_pack;                 // 心跳包
        uint8_t version;                   // 版本信息
        uint8_t config_return;           // 配置回复
        uint8_t lbs_info;                       // 参数查询   20220329
        uint8_t ipc_info;                       // 查询摄像机信息
        uint8_t snmp_param;                       // 查询snmp参数
    } com_flag;
    struct
    {
        uint8_t caramer_num;      // 摄像机编号  20220329
        uint8_t adapter_num;          // 适配器 - 编号
        uint8_t snmp_dev_type;    // SNMP设备类型：摄像机0/1、ONV、交换机
    } sys;
    struct
    {
        uint8_t lwip_reset;         // 网络重启标志
        uint8_t adapter_reset;          // 适配器-重启标志
        uint8_t relay_reset[RELAY_NUM];    

        uint8_t net_reload_id[RELAY_NUM];        // 网络设备重启
        uint8_t net_reload_num[RELAY_NUM];        // 网络设备重启次数    
        uint8_t net_reload_times[RELAY_NUM];    // 网络设备重启计时    
    } sys_flag;
    struct
    {
        uint8_t status;         // 更新结果
    } update;
}sys_operate_t;

/* 参数定义 */
__attribute__((section (".RAM_D1"))) sys_backups_t sg_backups_t       = {0}; // 备份信息 20231022
__attribute__((section (".RAM_D1"))) sys_operate_t sg_sysoperate_t; // 系统操作参数：包括通信、存储、计时
__attribute__((section (".RAM_D1"))) sys_param_t   sg_sysparam_t   = {0}; // 系统参数：本地、远端、设备、上报相关参数
__attribute__((section (".RAM_D1"))) carema_t      sg_carema_param_t;   //onvif 摄像头信息
__attribute__((section (".RAM_D1"))) com_param_t   sg_comparam_t           = {
    90000,
    60000,
    60000,
    60000,
    200,    // 网络延时时间  20220308
    120,    // ONVIF搜索时间  20230811
};                 // 通信参数：心跳、上报、ping间隔时间
__attribute__((section (".RAM_D1"))) uint8_t     sg_send_buff[2048] = {0}; // 发送缓存区
__attribute__((section (".RAM_D1"))) uint16_t    sg_send_size =  0;     // 发送数据长度
__attribute__((section (".RAM_D1"))) rtc_time_t  sg_rtctime_t = {0}; // rtc采集间隔时间
__attribute__((section (".RAM_D1"))) snmp_oid_t sg_snmp_oid_t = {0}; // SNMP OID

uint16_t sg_err_count = 0; // 错误数量
/*    
*********************************************************************************************************
*    函 数 名: app_task_function
*    功能说明: 应用程序主任务 
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void app_task_function(void)
{
    uint8_t get_time_cnt = 0;
    
    /* 开启系统指示灯 */
    led_control_function(LD_STATE,LD_FLICKER);

    for(;;)
    {
        app_task_save_function();                   // 存储相关任务
        com_deal_main_function();                     // 处理接收数据
        app_com_send_function();                        // 通信发送
        app_open_exec_task_function();        
        app_sys_net_operate_relay();
        app_sys_operate_relay();
        
        get_time_cnt++;
        if(get_time_cnt>100)
        {
            get_time_cnt = 0;
            RTC_Get_Time(&sg_rtctime_t);        /* 时间获取 */
            
            /* 内存利用率 */
            sg_sysparam_t.mem = my_mem_perused(SRAMIN);
        }
        FeedFwdgt();    
        vTaskDelay(10);              // 延时10ms
    }
}

/*
*********************************************************************************************************
*    函 数 名: app_set_com_send_flag_function
*    功能说明: 设置发送标志
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void app_set_com_send_flag_function(uint8_t cmd, uint8_t data)
{
    switch(cmd)
    {
        case CR_QUERY_CONFIG:
            sg_sysoperate_t.com_flag.query_configuration = 1;
            break;
        case CR_QUERY_INFO:
            sg_sysoperate_t.com_flag.report_normally = 1;
            break;
        case CR_QUERY_SOFTWARE_VERSION:
            sg_sysoperate_t.com_flag.version = 1;
            break;
        case CR_QUERY_LBS_INFO:          // 查询LBS信息
            sg_sysoperate_t.com_flag.lbs_info = 1;
            break;
        case CR_QUERY_IPC_IP:          // 查询IP地址
            sg_sysoperate_t.com_flag.ipc_info = 1;
            break;
        case CR_QUERY_SNMP_INFO:          // 查询SNMP参数
            sg_sysoperate_t.sys.snmp_dev_type = data;
            sg_sysoperate_t.com_flag.snmp_param = 1;
            break;
    }
}

/*
*********************************************************************************************************
*    函 数 名: app_set_reply_parameters_function
*    功能说明: 设置回复参数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void app_set_reply_parameters_function(uint8_t cmd, uint8_t error)
{
    sg_sysoperate_t.com.return_cmd          = cmd;
    sg_sysoperate_t.com.return_error        = error;
    sg_sysoperate_t.com_flag.config_return  = 1;
}

/*
*********************************************************************************************************
*    函 数 名: app_report_information_immediately
*    功能说明: 立即上报信息 - 主要针对于出现异常数据时的上报
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void app_report_information_immediately(uint8_t if_save) 
{
    if ( eth_get_tcp_status() != 2 && gsm_get_network_connect_status_function() == 0)
    {
        /* 网络异常，存储本次数据到本地 */
        sg_sysoperate_t.com_flag.report_normally = 2;
        if(if_save == 1)
        {
            sg_err_count++;
            memset(sg_send_buff,0,sizeof(sg_send_buff));
            com_report_normally_function(sg_send_buff,&sg_send_size,CR_QUERY_INFO,if_save);
            save_stroage_error_information(sg_send_buff,sg_send_size,sg_err_count);
        }
        return;
    }
    /* 发送正常上报数据 */
    memset(sg_send_buff,0,sizeof(sg_send_buff));
    com_report_normally_function(sg_send_buff,&sg_send_size,CR_QUERY_INFO,if_save);
    if(if_save == 1)
    {
        sg_err_count++;
        save_stroage_error_information(sg_send_buff,sg_send_size,sg_err_count);
    }
    /* 设置发送参数 */
    sg_sysoperate_t.com.send_cmd = CR_QUERY_INFO;
    sg_sysoperate_t.com.repeat   = 0;         // 重启一次正常上报计时   
    
    /* 数据发送 */
    if(sg_sysoperate_t.com.send_cmd != 0)
    {
        app_send_data_task_function();    
        sg_sysoperate_t.com.send_status = SEND_STATUS_SENGING;
        return ;        // 如果检测到需要重复发送，则不进行下一次发送
    }
}

/*
*********************************************************************************************************
*    函 数 名: app_deal_com_flag_function
*    功能说明: 用来处理通信发送标志
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_deal_com_flag_function(void)
{
    static uint32_t onvif_time = 0;
    static uint32_t snmp_time = 0;
    
    /* 数据发送 */
    if(sg_sysoperate_t.com.send_cmd != 0)
    {
        app_send_data_task_function();
        sg_sysoperate_t.com.send_status = SEND_STATUS_SENGING;        /* 转换为正在发送 */
        return ;                                 // 如果检测到需要重复发送，则不进行下一次发送
    }
    
    if(sg_sysoperate_t.com_flag.heart_pack == 1)    /* 检测心跳发送 */
    {
        sg_sysoperate_t.com_flag.heart_pack = 0;
        com_heart_pack_function(sg_send_buff,&sg_send_size);        /* 发送心跳 */
        sg_sysoperate_t.com.send_cmd    = COM_HEART_UPDATA;        /* 设置发送参数 */
        sg_sysoperate_t.com.heart_time  = 0;      // 重启一次心跳计时
    }
    
    /* 立即上报设备状态 */
    if(sg_sysoperate_t.com_flag.report_normally != 0)
    {
        sg_sysoperate_t.com_flag.report_normally = 0;
        memset(sg_send_buff,0,sizeof(sg_send_buff));
        if(sg_sysoperate_t.com_flag.report_normally == 1)
            com_report_normally_function(sg_send_buff,&sg_send_size,CR_QUERY_INFO,0);        /* 发送正常上报数据 */
        else
            com_report_normally_function(sg_send_buff,&sg_send_size,CR_QUERY_INFO,1);
        
        /* 设置发送参数 */
        sg_sysoperate_t.com.send_cmd = CR_QUERY_INFO;
        sg_sysoperate_t.com.repeat   = 0;         // 重启一次正常上报计时   
    }
    
    /* 直接发送，不需要检测回传 */
    /* 查询配置当前参数设置 */
    if(sg_sysoperate_t.com_flag.query_configuration == 1)
    {
        sg_sysoperate_t.com_flag.query_configuration = 0;
        /* 发送查询配置 */
        memset(sg_send_buff,0,sizeof(sg_send_buff));
        com_query_configuration_function(sg_send_buff,&sg_send_size);
        app_send_data_task_function();
    }
    
    /* 上报软件版本信息 */
    if(sg_sysoperate_t.com_flag.version == 1)
    {
        sg_sysoperate_t.com_flag.version = 0;
        /* 发送正常上报数据 */
        com_version_information(sg_send_buff,&sg_send_size);
        app_send_data_task_function();
    }
    
    /* 查询摄像机信息 */
    if(sg_sysoperate_t.com_flag.ipc_info == 1)
    {
        if(onvif_time == 0)
        {
            printf("..........start onvif............\n");
            eth_set_onvif_flag(1); // 开始检测
            memset(sg_send_buff,0,sizeof(sg_send_buff));
            onvif_time = HAL_GetTick();
        }
        else
        {
            if((eth_get_onvif_flag() == ETH_ONVIF_END) || (HAL_GetTick()-onvif_time > 10000))
            {
                printf(".........end onvif............\n");
                onvif_time = 0;
                sg_sysoperate_t.com_flag.ipc_info = 0;
                com_ipc_device_information(sg_send_buff,&sg_send_size);
                for(uint8_t i=0;i<5;i++)
                {
                    app_send_data_task_function();    
                    vTaskDelay(1000);    
                }
            }    
        }
    }
    /* 查询SNMP参数 */
    if(sg_sysoperate_t.com_flag.snmp_param == 1)
    {
        if(snmp_time == 0)
        {
            printf("..........start snmp param............\n");
            snmp_set_enable_flag(SNMP_START); // 开始检测
            memset(sg_send_buff,0,sizeof(sg_send_buff));
            snmp_time = HAL_GetTick();
        }
        else
        {
            if((snmp_get_status() == 0) || (HAL_GetTick()- snmp_time > 10000))
            {
                printf(".........end snmp param............\n");
                snmp_time = 0;
                sg_sysoperate_t.com_flag.snmp_param = 0;
                com_device_snmp_information(sg_send_buff,&sg_send_size);
//                for(uint8_t i=0;i<3;i++)
//                {
                    app_send_data_task_function();    
//                    OSTimeDlyHMSM(0,0,0,500);    
//                }
            }    
        }
    }
        
    /* 回传信号 */
    if(sg_sysoperate_t.com_flag.config_return == 1)
    {
        sg_sysoperate_t.com_flag.config_return = 0;
        /* 数据回传 */
        com_ack_function(sg_send_buff,&sg_send_size,sg_sysoperate_t.com.return_cmd,sg_sysoperate_t.com.return_error);
        
        app_send_data_task_function();
    }
}

/*
*********************************************************************************************************
*    函 数 名: app_get_com_send_status_function
*    功能说明: 获取当前通信状态
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t app_get_com_send_status_function(void)
{
    return sg_sysoperate_t.com.send_status;
}

/*
*********************************************************************************************************
*    函 数 名: app_deal_com_send_wait_function
*    功能说明: 发送等待处理任务
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_deal_com_send_wait_function(void)
{
    switch(sg_sysoperate_t.com.send_result)
    {
        case SR_TIMEOUT:                                            // 发送超时                            
            /* 本次发送超时 */
            sg_sysoperate_t.com.repeat++;
            if(sg_sysoperate_t.com.repeat >= COM_SEND_MAX_NUM)
            {
                /* 发送响应超时 */
                sg_sysoperate_t.com.repeat = 0;
                /* 发送超时，服务器无响应或者网络已断开 */
                eth_set_tcp_connect_reset();                        // 重启TCP连接
                gsm_set_network_reset_function();                    // 重启GRPS连接
                
                /* 数据清空 */
                sg_sysoperate_t.com.send_status = SEND_STATUS_NO;     // 进行下一次发送
                sg_sysoperate_t.com.send_cmd = 0;
                
                sg_sysoperate_t.com.heart_time  = 0;                 // 重启一次心跳计时
                sg_sysoperate_t.com.repeat   = 0;                     // 重启一次正常上报计时   
            }
            else
            {
                /* 重新发送 */
                sg_sysoperate_t.com.send_status = SEND_STATUS_NO;
            }
            break;
        case SR_OK:                                                    // 发送成功
            /* 发送成功 */
            sg_sysoperate_t.com.send_status = SEND_STATUS_NO;         // 进行下一次发送
            sg_sysoperate_t.com.send_cmd = 0;
            /* 清空数据 */
            memset(sg_send_buff,0,sizeof(sg_send_buff));
            sg_send_size = 0;
            break;
        case SR_ERROR:                                                // 响应错误
        case SR_SEND_ERROR:                                            // 发送错误
            sg_sysoperate_t.com.repeat++;
            if(sg_sysoperate_t.com.repeat >= COM_SEND_MAX_NUM)
            {
                /* 发送次数到达上限 */
                sg_sysoperate_t.com.repeat = 0;
                sg_sysoperate_t.com.send_status = SEND_STATUS_NO;     // 进行下一次发送
                sg_sysoperate_t.com.send_cmd = 0;
                /* 清空数据 */
                memset(sg_send_buff,0,sizeof(sg_send_buff));
                sg_send_size = 0;
            }
            else
            {
                /* 重新发送 */
                sg_sysoperate_t.com.send_status = SEND_STATUS_NO;     // 进行下一次发送
            }
            break;
        default:
            sg_sysoperate_t.com.send_cmd = 0;
            sg_sysoperate_t.com.send_status = SEND_STATUS_NO;         // 进行下一次发送
            /* 清空数据 */
            memset(sg_send_buff,0,sizeof(sg_send_buff));
            sg_send_size = 0;
            break;
    }    
    /* 结果清空 */
    sg_sysoperate_t.com.send_result = SR_WAIT;
}

/*
*********************************************************************************************************
*    函 数 名: app_com_send_function
*    功能说明: 通信发送函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_com_send_function(void)
{
    /* 检测网络状态 */
    if(gsm_get_network_connect_status_function() == 0 && eth_get_tcp_status() != 2)
    {
        return ;
    }
    if(sg_sysoperate_t.com.send_mode == 0 && eth_get_tcp_status() != 2)
    {
        /* 网络错误，准备重新连接，清除本次发送参数 */
        memset(&sg_sysoperate_t.com_flag,0,sizeof(sg_sysoperate_t.com_flag));
        sg_sysoperate_t.com.send_cmd = 0;
        /* 重启网络 */
        eth_set_tcp_connect_reset();
        gsm_set_network_reset_function(); // 重启GPRS
    }
    if(sg_sysoperate_t.com.send_mode == 1 && gsm_get_network_connect_status_function() == 0)
    {
        /* 网络错误，准备重新连接，清除本次发送参数 */
        memset(&sg_sysoperate_t.com_flag,0,sizeof(sg_sysoperate_t.com_flag));
        sg_sysoperate_t.com.send_cmd = 0;
        /* 重启网络 */
        eth_set_tcp_connect_reset();
        gsm_set_network_reset_function(); // 重启GPRS
    }
    
    /* 检测是否能发送数据 */
    if(sg_sysoperate_t.com.send_status == SEND_STATUS_NO)
    {
        app_deal_com_flag_function();
    }
    /* 检测到发送有结果了 */
    else if(sg_sysoperate_t.com.send_status == SEND_STATUS_RESULT)
    {
        app_deal_com_send_wait_function();
    }
    /* 正在发送中 */
    else
    {
        
    }
}

/*
*********************************************************************************************************
*    函 数 名: app_set_send_result_function
*    功能说明: 设置发送结果
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_send_result_function(send_result_e data)
{
    sg_sysoperate_t.com.send_result = data;
    sg_sysoperate_t.com.send_status = SEND_STATUS_RESULT;
}
    
/*
*********************************************************************************************************
*    函 数 名: app_com_time_function
*    功能说明: 通信计时函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_com_time_function(void)
{
    /* 心跳计时 */
    sg_sysoperate_t.com.heart_time++;
    if(sg_sysoperate_t.com.heart_time > sg_comparam_t.heart)
    {
        sg_sysoperate_t.com.heart_time = 0;
        /* 发送一次心跳 */
        sg_sysoperate_t.com_flag.heart_pack = 1;
    }
    
    /* 正常上报 */
    sg_sysoperate_t.com.report_time++;
    if(sg_sysoperate_t.com.report_time > (sg_comparam_t.report))
    {
        sg_sysoperate_t.com.report_time = 0;
        /* 进行一次上报 */
        sg_sysoperate_t.com_flag.report_normally = 1;
    }
    
    /* 发送计时 */
    if(sg_sysoperate_t.com.send_status == 1)
    {
        sg_sysoperate_t.com.send_time++;
        if(sg_sysoperate_t.com.send_time > COM_SEND_MAX_TIME)
        {
            sg_sysoperate_t.com.send_time = 0;
            
            sg_sysoperate_t.com.send_status = SEND_STATUS_RESULT;
            sg_sysoperate_t.com.send_result = SR_TIMEOUT;
        }
    }
    else
    {
        sg_sysoperate_t.com.send_time = 0;
    }
}

/*
*********************************************************************************************************
*    函 数 名: app_set_peripheral_switch
*    功能说明: 外设开关控制
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_peripheral_switch(uint8_t cmd, uint8_t data,uint8_t num)
{
    switch (cmd) 
    {
        case CONTROL_FAN:
            if (data == 1)
            {
                fan_control(FAN_ON); // 开风扇1
            }
            else if(data == 2)
            {
                fan_control(FAN_OFF); // 开风扇1
            }
            break;
            
        case CONTROL_OUT_PWR:
            if (num == 1)
            {
                relay_control((RELAY_DEV)(data-1),RELAY_ON); // 开外设1
            }
            else if(num == 2)
            {
                relay_control((RELAY_DEV)(data-1),RELAY_OFF);
            }
            else if(num == 3)
            {
                app_opeare_relay_function(data);
            }
            break;
        default:
            break;
    }
    
    app_set_reply_parameters_function(cmd,0x01);
}
/*
*********************************************************************************************************
*    函 数 名: app_opeare_relay_function
*    功能说明: 操作继电器
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int app_opeare_relay_function(uint8_t num)
{
    if(sg_sysoperate_t.sys_flag.relay_reset[num-1] == 0)
    {
        sg_sysoperate_t.sys_flag.relay_reset[num-1] = 1;
        return 0;
    }
    else
        return -1;
}

/*
*********************************************************************************************************
*    函 数 名: app_set_sys_opeare_function
*    功能说明: 设置操作任务 - 立即回发
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_sys_opeare_function(uint8_t cmd, uint8_t data)
{
    int8_t ret = 0;
    int8_t cnt = 0;
    
    switch(cmd)
    {
        case CR_SINGLE_CAMERA_CONTROL:
            if(data>=0x01 && data <= RELAY_NUM)
            {
                app_opeare_relay_function(data);
                app_set_reply_parameters_function(CR_SINGLE_CAMERA_CONTROL,0x01);
            }
            else
            {
                app_set_reply_parameters_function(CR_SINGLE_CAMERA_CONTROL,0x74);
            }
            break;

        case CR_POWER_RESETART:
            sg_sysoperate_t.com.return_error = 1;
            /* 数据回传 */
            com_ack_function(sg_send_buff,&sg_send_size,\
                             sg_sysoperate_t.com.return_cmd,\
                             sg_sysoperate_t.com.return_error);
            cnt = 0;
REPEAT1:
            /* 立即发送 */
            app_send_data_task_function();    

            if(ret < 0)
            {
                cnt++;
                if(cnt<=3)
                {
                    vTaskDelay(100); 
                    goto REPEAT1;
                }
            }
                
            lfs_unmount(&g_lfs_t);
            vTaskDelay(100); 
            /* 重启设备 */
            app_system_softreset();
            break;
        case CR_LWIP_NETWORK_RESET:    
            sg_sysoperate_t.com.return_error = 1;
            /* 数据回传 */
            com_ack_function(sg_send_buff,&sg_send_size,\
                             sg_sysoperate_t.com.return_cmd,\
                             sg_sysoperate_t.com.return_error);
            cnt = 0;
REPEAT2:
            /* 立即发送 */
            app_send_data_task_function();    

            if(ret < 0)
            {
                cnt++;
                if(cnt<=3)
                {
                    vTaskDelay(100); 
                    goto REPEAT2;
                }
            }
            vTaskDelay(100); 
            eth_set_network_reset();
            break;
            
        case CR_GPRS_NETWORK_RESET:
            sg_sysoperate_t.com.return_error = 1;
            /* 数据回传 */
            com_ack_function(sg_send_buff,&sg_send_size,\
                             sg_sysoperate_t.com.return_cmd,\
                             sg_sysoperate_t.com.return_error);
            cnt = 0;
REPEAT3:
            /* 立即发送 */
            app_send_data_task_function();    

            if(ret < 0)
            {
                cnt++;
                if(cnt<=3)
                {
                    vTaskDelay(100); 
                    goto REPEAT3;
                }
            }            
            vTaskDelay(100); 
            /* 重启GPRS */
            gsm_set_module_reset_function();
            break;
            
            case CR_GPRS_NETWORK_V_RESET:
            sg_sysoperate_t.com.return_error = 1;
            /* 数据回传 */
            com_ack_function(sg_send_buff,&sg_send_size,sg_sysoperate_t.com.return_cmd,sg_sysoperate_t.com.return_error);
            cnt = 0;
REPEAT4:
            /* 立即发送 */
            app_send_data_task_function();    
            if(ret < 0)
            {
                cnt++;
                if(cnt<=3)
                {
                    vTaskDelay(100); 
                    goto REPEAT4;
                }
            }            
            vTaskDelay(100); 
            /* 重启GPRS */
            gprs_v_reset_function();
            gsm_set_module_reset_function();
            break;
    }
}

/*
*********************************************************************************************************
*    函 数 名: app_sys_operate_timer_function
*    功能说明: 操作命令的时间处理函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_sys_operate_timer_function(void)
{
    static uint16_t time = 0;
        
    /* 重启指定适配器 */
    if(sg_sysoperate_t.sys_flag.adapter_reset == 1)
    {
        /* 关闭指的适配器 */
        if(sg_sysoperate_t.sys.adapter_num <= RELAY_NUM)
        {
            switch(sg_sysoperate_t.sys.adapter_num)
            {
                case 1:    relay_control(RELAY_1,RELAY_OFF);    break;
                case 2:    relay_control(RELAY_2,RELAY_OFF);    break;
                case 3:    relay_control(RELAY_3,RELAY_OFF);    break;
                default:    break;

            }
            sg_sysoperate_t.sys_flag.adapter_reset = 2;
            time = 15*1000; // 15s重启
        }
        else
        {
            sg_sysoperate_t.sys_flag.adapter_reset = 0;
        }
    }    
    else if(sg_sysoperate_t.sys_flag.adapter_reset == 2)
    {
        time--;
        if(time == 0)
        {
            switch(sg_sysoperate_t.sys.adapter_num)
            {
                case 1:    relay_control(RELAY_1,RELAY_ON);    break;
                case 2:    relay_control(RELAY_2,RELAY_ON);    break;
                case 3:    relay_control(RELAY_3,RELAY_ON);    break;
                default:
                    break;
            }
            sg_sysoperate_t.sys_flag.adapter_reset = 0;
        }
    }
}
/*
*********************************************************************************************************
*    函 数 名: app_set_net_operate_relay_id
*    功能说明: 根据网络重启设置继电器重启
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_net_operate_relay_id(uint8_t num)
{
    if(sg_sysoperate_t.sys_flag.net_reload_id[num] == 0)
        sg_sysoperate_t.sys_flag.net_reload_id[num] = 1;
}
/*
*********************************************************************************************************
*    函 数 名: app_set_net_reload_num
*    功能说明: 根据网络重启设置继电器重启
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_net_reload_num(uint8_t num)
{
    if(sg_sysoperate_t.sys_flag.net_reload_num[num] == 0)
    {
        if(sg_sysparam_t.threshold.net_reload > 0)
        {
            sg_sysoperate_t.sys_flag.net_reload_num[num] = sg_sysparam_t.threshold.net_reload;
            sg_sysoperate_t.sys_flag.net_reload_id[num] = 1;    
        }
        else
            sg_sysoperate_t.sys_flag.net_reload_times[num] = 0;
    }
}
/*
*********************************************************************************************************
*    函 数 名: app_sys_net_operate_relay
*    功能说明: 根据网络重启设置继电器重启
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_sys_net_operate_relay(void)
{
    static uint32_t relay_time[RELAY_NUM] = {0};
    uint32_t current_time = HAL_GetTick();    
//    uint32_t net_retime_th = sg_sysparam_t.threshold.net_retime * 1000;
    
    for(uint8_t i=0;i<RELAY_NUM;i++)
    {
        if(sg_sysoperate_t.sys_flag.net_reload_id[i] == 1)
        {
            if(sg_sysparam_t.threshold.net_retime == 0)
                sg_sysoperate_t.sys_flag.net_reload_id[i]    = 0;        
            else
            {
                relay_time[i] = HAL_GetTick();
//                relay_time[i] = sg_sysparam_t.threshold.net_retime;
                relay_control((RELAY_DEV)i,RELAY_OFF);
                sg_sysoperate_t.sys_flag.net_reload_id[i] = 2;                            
            }
        }
        else if(sg_sysoperate_t.sys_flag.net_reload_id[i] == 2)
        {        
//            relay_time[i]--;
//            if(relay_time[i] == 0)
            if((current_time - relay_time[i]) > 5000)
            {        
                relay_control((RELAY_DEV)i,RELAY_ON);
                sg_sysoperate_t.sys_flag.net_reload_id[i]    = 0;            
                sg_sysoperate_t.sys_flag.net_reload_times[i] = NETWAOK_RELOAD_TIME;
            }
        }    
    }
}
/*
*********************************************************************************************************
*    函 数 名: app_sys_net_relay_reload_num_times
*    功能说明: 重启次数计时
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_sys_net_relay_reload_num_times(void)
{
        for(uint8_t i=0;i<RELAY_NUM;i++)
        {
            if(sg_sysoperate_t.sys_flag.net_reload_times[i] > 0) // 下一轮计时
            {
            sg_sysoperate_t.sys_flag.net_reload_times[i]--;
            if(sg_sysoperate_t.sys_flag.net_reload_times[i] == 0)
            {
                if((sg_sysoperate_t.sys_flag.net_reload_num[i]-1) > 0) 
                {
                    sg_sysoperate_t.sys_flag.net_reload_num[i]--;
                    sg_sysoperate_t.sys_flag.net_reload_id[i] = 1;
                }
                else
                    sg_sysoperate_t.sys_flag.net_reload_times[i] = 0;
            }
        }
        
        
        
        
//        if(sg_sysoperate_t.sys_flag.net_reload_num[i] > 0)  // 时间计数
//        {
//            sg_sysoperate_t.sys_flag.net_reload_num[i]--;
//            if(sg_sysoperate_t.sys_flag.net_reload_num[i] == 0)
//            {
//                if(sg_sysoperate_t.sys_flag.net_reload_num[i] > 0)
//                {
//                    sg_sysoperate_t.sys_flag.net_reload_num[i]--;
//                    sg_sysoperate_t.sys_flag.net_reload_id[i] = 1;
//                }
//                else
//                    sg_sysoperate_t.sys_flag.net_reload_num[i] = 0;
//            }
//        }
    }
}
/*
*********************************************************************************************************
*                    参数的配置与获取
*********************************************************************************************************
*/
/*
*********************************************************************************************************
*    函 数 名: app_get_storage_param_function
*    功能说明: 用于获取存储的参数的函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_get_storage_param_function(void)
{
    save_read_local_network(&sg_sysparam_t.local);
    save_read_remote_ip_function(&sg_sysparam_t.remote);
    save_read_device_paramter_function(&sg_sysparam_t.device);
    save_read_com_param_function(&sg_comparam_t);
    save_read_carema_parameter(&sg_carema_param_t);   //20230712
    save_read_threshold_parameter(&sg_sysparam_t.threshold); // 20230720
    save_read_http_ota_function(&sg_sysparam_t.ota);
    save_read_backups_function(&sg_backups_t); // 20231022
    save_read_snmp_oid_parameter(&sg_snmp_oid_t); // 20231022
    // 读取用电量参数
    save_read_electricity_function(&sg_datacollec_t.electricity_all);
}

/*
*********************************************************************************************************
*    函 数 名: app_task_save_function
*    功能说明: 用于存储本机的设置参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_task_save_function(void)
{
    if(sg_sysoperate_t.save_flag.save_device_param == 1)
    {
        sg_sysoperate_t.save_flag.save_device_param = 0;
        /* 存储设备参数信息 */
        save_storage_device_parameter_function(&sg_sysparam_t.device);
    }    
    
    if(sg_sysoperate_t.save_flag.save_local_network == 1)
    {
        sg_sysoperate_t.save_flag.save_local_network = 0;
        /* 存储本地网络参数信息 */
        save_stroage_local_network(&sg_sysparam_t.local);
        vTaskDelay(100);
        eth_set_network_reset();
    }
    
    if(sg_sysoperate_t.save_flag.save_remote_network == 1)
    {
        sg_sysoperate_t.save_flag.save_remote_network = 0;
        /* 存储本地网络参数信息 */
        save_stroage_remote_ip_function(&sg_sysparam_t.remote);
        
        vTaskDelay(100);
        app_system_softreset();
//        set_reboot_time_function(1000); // 配置服务器后系统重启，防止设备未传到新平台
    }
    /* 存储摄像头参数 */    
    if(sg_sysoperate_t.save_flag.save_carema == 1)
    {
        sg_sysoperate_t.save_flag.save_carema = 0;
        save_stroage_carema_parameter(&sg_carema_param_t);
    }
    /* 存储通信相关参数 */    
    if(sg_sysoperate_t.save_flag.com_parameter == 1)
    {
        sg_sysoperate_t.save_flag.com_parameter = 0;
        save_stroage_com_param_function(&sg_comparam_t);
    }
    
    if(sg_sysoperate_t.save_flag.save_threshold == 1)
    {
        sg_sysoperate_t.save_flag.save_threshold = 0;
        save_stroage_threshold_parameter(&sg_sysparam_t.threshold);
    }
    /* 存储SNMP OID参数 */    
    if(sg_sysoperate_t.save_flag.save_snmp_oid == 1)
    {
        sg_sysoperate_t.save_flag.save_snmp_oid = 0;
        save_stroage_snmp_oid_parameter(&sg_snmp_oid_t);
    }    
    /* 恢复出厂化：产品序列号不变 */
    if(sg_sysoperate_t.save_flag.save_reset == 1)
    {
        sg_sysoperate_t.save_flag.save_reset = 0;
        save_clear_file_function(0);
        app_get_storage_param_function();
        
        eth_set_network_reset();              // 重启网络
        gsm_set_module_reset_function();     // 重启GPRS
    }
}

/*
*********************************************************************************************************
*    函 数 名: app_set_save_infor_function
*    功能说明: 设置存储标志位
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_save_infor_function(uint8_t mode)
{
    switch(mode)
    {
        case SAVE_LOCAL_NETWORK:
            sg_sysoperate_t.save_flag.save_local_network = 1;
            break;
        case SAVE_REMOTE_IP:
            sg_sysoperate_t.save_flag.save_remote_network = 1;
            break;
        case SAVE_DEVICE_PARAM:
            sg_sysoperate_t.save_flag.save_device_param = 1;
            break;
        case SAVE_COM_PARAMETER:
            sg_sysoperate_t.save_flag.com_parameter = 1;
            break;
        case SAVE_UPDATE:
            sg_sysoperate_t.save_flag.save_update_addr = 1;
            break;
        case SAVE_CAREMA:
            sg_sysoperate_t.save_flag.save_carema = 1;
            break;
        case SAVE_THRESHOLD:
            sg_sysoperate_t.save_flag.save_threshold = 1;
            break;
        case SAVE_SNMP_OID:
            sg_sysoperate_t.save_flag.save_snmp_oid = 1;
            break;
        default:
            break;
    }
}

/*
*********************************************************************************************************
*    函 数 名: app_get_local_network_function
*    功能说明: 获取本机网络信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *app_get_local_network_function(void)
{
    return (&sg_sysparam_t.local);
}

/*
*********************************************************************************************************
*    函 数 名: app_set_local_network_function
*    功能说明: 设置本地网络参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_local_network_function(struct local_ip_t param)
{
    uint8_t mac[6] = {0};
    uint8_t main_ip[4] = {0};
    
    memcpy(mac,sg_sysparam_t.local.mac,6);             // 备份
    memcpy(main_ip,sg_sysparam_t.local.ping_ip,4);     // 备份
    memset((uint8_t*)&sg_sysparam_t.local,0,sizeof(struct local_ip_t));
    memcpy((uint8_t*)&sg_sysparam_t.local,&param,sizeof(struct local_ip_t));
    memcpy(sg_sysparam_t.local.mac,mac,6);             // 还原
    memcpy(sg_sysparam_t.local.ping_ip,main_ip,4);     // 还原
    /* 保存 */
    app_set_save_infor_function(SAVE_LOCAL_NETWORK);
}

/*
*********************************************************************************************************
*    函 数 名: app_set_local_network_function_two
*    功能说明: 存储部分网络参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_local_network_function_two(struct local_ip_t param)
{
    memcpy(sg_sysparam_t.local.ip,param.ip,4);
    memcpy(sg_sysparam_t.local.gateway,param.gateway,4);
    memcpy(sg_sysparam_t.local.netmask,param.netmask,4);
    memcpy(sg_sysparam_t.local.dns,param.dns,4);
    memcpy(sg_sysparam_t.local.mac,param.mac,6);
    memcpy(sg_sysparam_t.local.ping_ip,param.ping_ip,4);
    memcpy(sg_sysparam_t.local.ping_sub_ip,param.ping_sub_ip,4);
    
    memcpy(sg_sysparam_t.local.multicast_ip,param.multicast_ip,4);
    sg_sysparam_t.local.multicast_port = param.multicast_port;

//    STMFLASH_Write_SAVE(DEVICE_FLASH_STORE,DEVICE_MAC_ADDR,(uint32_t *)&sg_sysparam_t.local.mac,2);
//    bsp_WriteCpuFlash_Save(DEVICE_FLASH_STORE,DEVICE_MAC_ADDR,(uint8_t *)&local.mac,6);

    app_set_save_infor_function(SAVE_LOCAL_NETWORK);
}


/*
*********************************************************************************************************
*    函 数 名: app_set_transfer_mode_function
*    功能说明: 存储传输模式信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_transfer_mode_function(uint8_t mode) 
{
    switch(mode) {
        case 0:
            sg_sysparam_t.local.server_mode = SERVER_MODE_LWIP;
            break;
        case 1:
            sg_sysparam_t.local.server_mode = SERVER_MODE_GPRS;
            break;
        case 2:
            sg_sysparam_t.local.server_mode = SERVER_MODE_AUTO;
            break;
    }
    /* 保存 */
    app_set_save_infor_function(SAVE_LOCAL_NETWORK);
}
/*
*********************************************************************************************************
*    函 数 名: app_set_carema_search_mode_function
*    功能说明: 设置摄像机搜索协议
*    形    参: config_mode:配置方式 web/平台
*    返 回 值: 20230810
*********************************************************************************************************
*/
void app_set_carema_search_mode_function(uint8_t mode,uint8_t config_mode)  
{
    if(config_mode == 0 )  // web
    {        
        sg_sysparam_t.local.search_mode = mode+1;
    }
    else if(config_mode == 1 )  // 平台
    {
        sg_sysparam_t.local.search_mode = mode;
    }
    /* 保存 */
    app_set_save_infor_function(SAVE_LOCAL_NETWORK);
}

/*
*********************************************************************************************************
*    函 数 名: app_get_remote_network_function
*    功能说明: 获取远端网络信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *app_get_remote_network_function(void)
{
    return (&sg_sysparam_t.remote);
}
/*
*********************************************************************************************************
*    函 数 名: app_get_backups_function
*    功能说明: 获取备份信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *app_get_backups_function(void)
{
    return (&sg_backups_t.remote);
}
/*
*********************************************************************************************************
*    函 数 名: app_set_remote_network_function
*    功能说明: 存储远端网络信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_remote_network_function(struct remote_ip param)
{
    app_save_backups_remote_param_function();  // 备份服务器信息
    
    memcpy(&sg_sysparam_t.remote,&param,sizeof(struct remote_ip));
    /* 存储 */
    app_set_save_infor_function(SAVE_REMOTE_IP);
}

/*
*********************************************************************************************************
*    函 数 名: app_set_reset_function
*    功能说明: 恢复出厂化
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_reset_function(void)
{
    sg_sysoperate_t.save_flag.save_reset = 1;
}

/*
*********************************************************************************************************
*    函 数 名: app_get_camera_function
*    功能说明: 获取相机IP信息
*    形    参: @ip            : ip数据
*    返 回 值: 品牌
*    @num        : 摄像机号数
*    ? ? ?: *    
*********************************************************************************************************
*/
int8_t app_get_camera_param_function(char *ip, uint8_t *brand, uint8_t num)
{    
    /* 验证是否有ip地址 */
    if( sg_carema_param_t.ip[num][0] == 0 && \
        sg_carema_param_t.ip[num][1] == 0 && \
        sg_carema_param_t.ip[num][2] == 0 && \
        sg_carema_param_t.ip[num][3] == 0)
    {
        return -1;
    }
    else
    {
        sprintf(ip,"%d.%d.%d.%d",sg_carema_param_t.ip[num][0],sg_carema_param_t.ip[num][1],sg_carema_param_t.ip[num][2],sg_carema_param_t.ip[num][3]);
        if (brand) {
            *brand = sg_carema_param_t.brand[num];
        }
    }
    return 0;
}
/*
*********************************************************************************************************
*    函 数 名: app_get_camera_function
*    功能说明: 获取相机IP信息
*    形    参: @ip            : ip数据
*    返 回 值: 摄像机号数
*    ? ? ?: *    
*********************************************************************************************************
*/
int8_t app_get_camera_function(uint8_t *ip, uint8_t num)
{    
    /* 验证是否有ip地址 */
    if( sg_carema_param_t.ip[num][0] == 0 && \
          sg_carema_param_t.ip[num][1] == 0 && \
          sg_carema_param_t.ip[num][2] == 0 && \
          sg_carema_param_t.ip[num][3] == 0)
    {
        return -1;
    }
    else
    {
        memcpy(ip,sg_carema_param_t.ip[num],4);
    }
    return 0;
}
/*
*********************************************************************************************************
*    函 数 名: app_get_camera_mac_function
*    功能说明: 获取相机mac信息
*    形    参: @ip            : ip数据
*    返 回 值: 摄像机号数
*    ? ? ?: *    
*********************************************************************************************************
*/
int8_t app_get_camera_mac_function(uint8_t *mac, uint8_t num)
{    
    /* 验证是否有ip地址 */
    uint8_t ree = 0;
    for(uint8_t cnt=0; cnt< 6 ;cnt++)  // 10路摄像机循环检测
    {    
        if(sg_carema_param_t.mac[num][cnt] == 0)
            ree++;
    }
    if( ree >= 5)
    {
        return -1;
    }
    else
    {
        memcpy(mac,sg_carema_param_t.mac[num],6);
    }
    return 0;
}
/*
*********************************************************************************************************
*    函 数 名: app_set_camera_function
*    功能说明: 设置摄像机ip地址
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_camera_function(uint8_t *ip)
{
    memset(sg_carema_param_t.ip,0,sizeof(sg_carema_param_t.ip));
    memcpy(sg_carema_param_t.ip,ip,sizeof(sg_carema_param_t.ip));
    /* 存储 */
    app_set_save_infor_function(SAVE_CAREMA);
}
/*
*********************************************************************************************************
*    函 数 名: app_set_camera_brand_function
*    功能说明: 设置摄像机品牌
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_camera_brand_function(uint8_t *brand)
{
    memset(sg_carema_param_t.brand,0,sizeof(sg_carema_param_t.brand));
    memcpy(sg_carema_param_t.brand,brand,sizeof(sg_carema_param_t.brand));
    app_set_save_infor_function(SAVE_CAREMA);
}
/*
*********************************************************************************************************
*    函 数 名: app_set_camera_login_function
*    功能说明: 设置指定摄像机的用户名、密码
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_camera_login_function(char *name_buf,char *pwd_buf,int port,uint8_t num)
{
    memset(sg_carema_param_t.name[num],0,sizeof(sg_carema_param_t.name[num]));
    memset(sg_carema_param_t.pwd[num],0,sizeof(sg_carema_param_t.pwd[num]));
    sprintf(sg_carema_param_t.name[num],"%s",name_buf);
    sprintf(sg_carema_param_t.pwd[num],"%s",pwd_buf);
    sg_carema_param_t.port[num] = port;
    app_set_save_infor_function(SAVE_CAREMA);    /* 存储 */
}

/*
*********************************************************************************************************
*    函 数 名: app_get_camera_login_function
*    功能说明: 获取指定摄像机用户名、密码
*    形    参: @ip            :
*    返 回 值: 摄像机号数
*    ? ? ?: *                 20220329
*********************************************************************************************************
*/
int8_t app_get_camera_login_function(char *name_buf,char *pwd_buf,uint8_t num)
{    
    /* 验证信息是否为空 */
    if( strlen(sg_carema_param_t.name[num]) == 0 &&strlen(sg_carema_param_t.pwd[num]) == 0)
    {
        return -1;
    }
    else
    {
        sprintf(name_buf,"%s",sg_carema_param_t.name[num]);
        sprintf(pwd_buf,"%s",sg_carema_param_t.pwd[num]);
    }
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: app_get_camera_port_function
*    功能说明: 获取指定摄像机端口号
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int app_get_camera_port_function(uint8_t num)
{    
    return sg_carema_param_t.port[num];
}
/*
*********************************************************************************************************
*    函 数 名: app_get_camera_num_function
*    功能说明: 获取指定摄像机编号
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t app_get_camera_num_function(void)
{    
    return  sg_sysoperate_t.sys.caramer_num;
}

/*
*********************************************************************************************************
*    函 数 名: app_set_camera_id_num_function
*    功能说明: 设置摄像机的编号
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_camera_id_num_function(uint8_t data)  // 设置摄像机的编号
{    
    sg_sysoperate_t.sys.caramer_num = data;
}
/*
*********************************************************************************************************
*    函 数 名: app_get_carema_param_function
*    功能说明: 获取摄像机参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *app_get_carema_param_function(void)
{
    return (&sg_carema_param_t);
}

/*
*********************************************************************************************************
*    函 数 名: app_set_camera_num_function
*    功能说明: 设置摄像机ip - 指定摄像机
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_camera_num_function(uint8_t *ip, uint8_t num)
{
    sg_carema_param_t.ip[num][0] = ip[0];
    sg_carema_param_t.ip[num][1] = ip[1];
    sg_carema_param_t.ip[num][2] = ip[2];
    sg_carema_param_t.ip[num][3] = ip[3];
    app_set_save_infor_function(SAVE_CAREMA);
}
/*
*********************************************************************************************************
*    函 数 名: app_set_camera_num_brand_function
*    功能说明: 设置指定摄像机的品牌
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_camera_num_brand_function(uint8_t brand, uint8_t num)
{
    sg_carema_param_t.brand[num] = brand;
    app_set_save_infor_function(SAVE_CAREMA);
}
/*
*********************************************************************************************************
*    函 数 名: app_set_camera_mac_function
*    功能说明: 设置摄像机mac
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_camera_mac_function(uint8_t *mac, uint8_t num)
{
    sg_carema_param_t.mac[num][0] = mac[0];
    sg_carema_param_t.mac[num][1] = mac[1];
    sg_carema_param_t.mac[num][2] = mac[2];
    sg_carema_param_t.mac[num][3] = mac[3];
    sg_carema_param_t.mac[num][4] = mac[4];
    sg_carema_param_t.mac[num][5] = mac[5];    
    app_set_save_infor_function(SAVE_CAREMA);
}

/*
*********************************************************************************************************
*    函 数 名: app_match_local_camera_ip
*    功能说明: 匹配本地IP
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t app_match_local_camera_ip(uint8_t *ip)
{
    int8_t  ret   = 0;
    uint8_t index = 0;
    
    for(index = 0; index<10; index++)
    {
        if(memcmp(sg_carema_param_t.ip[index],ip,4) == 0)
        {
            ret = -1;
            break;
        }
    }
    return ret;
}

/*
*********************************************************************************************************
*    函 数 名: app_set_fan_humi_param_function
*    功能说明: 设置风扇湿度启动参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_fan_humi_param_function(uint8_t *data)
{
    sg_sysparam_t.threshold.humi_high = data[0];
    sg_sysparam_t.threshold.humi_low  = data[1];
    app_set_save_infor_function(SAVE_THRESHOLD);    /* 存储 */
}

/*
*********************************************************************************************************
*    函 数 名: app_set_fan_param_function
*    功能说明: 配置风扇温度参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_fan_param_function(int8_t *data)
{
    sg_sysparam_t.threshold.temp_high = data[0];
    sg_sysparam_t.threshold.temp_low = data[1];
    app_set_save_infor_function(SAVE_THRESHOLD);    /* 存储 */
}

/*
*********************************************************************************************************
*    函 数 名: app_set_fill_light_function
*    功能说明: 设置补光灯开启时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_fill_light_function(uint16_t *time)
{
    sg_sysparam_t.threshold.light_open_time = time[0];
    sg_sysparam_t.threshold.light_close_time= time[1];
    app_set_save_infor_function(SAVE_THRESHOLD);
}

/*
*********************************************************************************************************
*    函 数 名: app_set_door_time_function
*    功能说明: 设置箱门开启时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_door_time_function(uint16_t *time)
{
    sg_sysparam_t.threshold.door_open_time = time[0];
    sg_sysparam_t.threshold.door_close_time= time[1];
    app_set_save_infor_function(SAVE_THRESHOLD);
}

/*
*********************************************************************************************************
*    函 数 名: app_set_threshold_param_function
*    功能说明: 配置阈值
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_vol_current_param(uint16_t *data)
{
    sg_sysparam_t.threshold.volt_max = data[0];
    sg_sysparam_t.threshold.volt_min = data[1];
    sg_sysparam_t.threshold.current  = data[2];
    sg_sysparam_t.threshold.angle    = data[3];
    sg_sysparam_t.threshold.miu      = data[4];
    /* 保存 */
    app_set_save_infor_function(SAVE_THRESHOLD);    /* 存储 */
}

/*
*********************************************************************************************************
*    函 数 名: app_set_network_reload_param
*    功能说明: 配置阈值
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_network_reload_param(uint16_t *data)
{
    sg_sysparam_t.threshold.net_reload = data[0];
    sg_sysparam_t.threshold.net_retime = data[1];

    app_set_save_infor_function(SAVE_THRESHOLD);    /* 存储 */
}
/*
*********************************************************************************************************
*    函 数 名: app_get_device_param_function
*    功能说明: 获取设备参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *app_get_device_param_function(void)
{
    return (&sg_sysparam_t.device);
}

/*
*********************************************************************************************************
*    函 数 名: app_match_password_function
*    功能说明: 密码比较函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t app_match_password_function(char *password)
{
    if(strcmp(password , (char*)sg_sysparam_t.device.password)==0)
    {
        return 0;
    }
    return -1;
}
/*
*********************************************************************************************************
*    函 数 名: app_match_set_code_function
*    功能说明: 确认是否需要需要修改默认密码
*    形    参: 1-需要修改
*    返 回 值: 
*********************************************************************************************************
*/
int8_t app_match_set_code_function(void)
{
    return sg_sysparam_t.device.default_password;
}

/*
*********************************************************************************************************
*    函 数 名: app_set_device_param_function
*    功能说明: 设置设备参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_device_param_function(struct device_param param)
{
    memset((uint8_t*)&sg_sysparam_t.device,0,sizeof(struct device_param));
    memcpy((uint8_t*)&sg_sysparam_t.device,&param,sizeof(struct device_param));
    
//    STMFLASH_Write_SAVE(DEVICE_FLASH_STORE,DEVICE_ID_ADDR,(uint32_t*)param.id.c,1);
    bsp_WriteCpuFlash_Save(DEVICE_FLASH_STORE,DEVICE_ID_ADDR,(uint8_t*)param.id.c,4);
    
    /* 保存 */
    app_set_save_infor_function(SAVE_DEVICE_PARAM);
}

/*
*********************************************************************************************************
*    函 数 名: app_set_code_function
*    功能说明: 设置密码
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_code_function(struct device_param param)
{
    sg_sysparam_t.device.default_password = 0;
    memcpy(sg_sysparam_t.device.password,param.password,sizeof(param.password));
    
    /* 保存 */
    app_set_save_infor_function(SAVE_DEVICE_PARAM);
}

/*
*********************************************************************************************************
*    函 数 名: app_get_com_heart_time
*    功能说明: 获取心跳参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint16_t app_get_com_heart_time(void)
{
    return sg_comparam_t.heart;
}

/*
*********************************************************************************************************
*    函 数 名: app_set_next_report_time
*    功能说明: 设置上报间隔时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_next_report_time(uint16_t time)
{
    /* 将时间单位转换为ms */
    sg_comparam_t.report = time*1000;
        
    app_set_save_infor_function(SAVE_COM_PARAMETER);
}

/*
*********************************************************************************************************
*    函 数 名: app_set_next_report_time_other
*    功能说明: 设置上报间隔时间-可选择
*    形    参: @time        : 上报时间
*    返 回 值: 设置编号
*********************************************************************************************************
*/
void app_set_next_report_time_other(uint16_t time,uint8_t sel)
{
    if(sel == 1) {
        sg_comparam_t.report = time*1000;
        app_set_save_infor_function(SAVE_COM_PARAMETER);
    }
}

/*
*********************************************************************************************************
*    函 数 名: app_set_next_ping_time
*    功能说明: 设置下一次ping的时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_next_ping_time(uint16_t time, uint8_t time_dev)
{
    /* 将时间单位转换为ms */
    sg_comparam_t.ping = time*1000;
    sg_comparam_t.dev_ping = time_dev*1000;
    
    app_set_save_infor_function(SAVE_COM_PARAMETER);
}
/*
*********************************************************************************************************
*    函 数 名: app_set_network_delay_time
*    功能说明: 设置网络延时时间  20220308
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_network_delay_time(uint8_t time_dev)
{
    /* 将时间单位转换为ms */
    sg_comparam_t.network_time = time_dev;
    app_set_save_infor_function(SAVE_COM_PARAMETER);
}

/*
*********************************************************************************************************
*    函 数 名: app_set_onvif_reload_time
*    功能说明: 设置搜索协议时间、重启时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_onvif_reload_time(uint8_t time_dev,uint8_t mode)
{
    switch(mode)
    {
        case 0: sg_comparam_t.onvif_time = time_dev; break;
        default: break;
    }
    app_set_save_infor_function(SAVE_COM_PARAMETER);
}


/*
*********************************************************************************************************
*    函 数 名: app_get_current_time
*    功能说明: 获取当前时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_get_current_time(char *time)
{
    
    sprintf(time,"%04d/%02d/%02d %02d:%02d:%02d",sg_rtctime_t.year,\
                                                 sg_rtctime_t.month,\
                                                 sg_rtctime_t.data,\
                                                 sg_rtctime_t.hour,\
                                                 sg_rtctime_t.min,\
                                                 sg_rtctime_t.sec);
}
/*
*********************************************************************************************************
*    函 数 名: app_get_current_times
*    功能说明: 获取当前时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *app_get_current_times(void)
{
    return &sg_rtctime_t;
}

/*
*********************************************************************************************************
*    函 数 名: app_get_report_current_time
*    功能说明: 获取当前上报的实时时间
*    形    参: @mode        : 0：秒 1：毫秒
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t* app_get_report_current_time(uint8_t mode)
{
    static uint8_t times[20];
    
    memset(times,0,sizeof(times));
    if(mode == 0) {
        sprintf((char*)times,"%04d%02d%02d%02d%02d%02d",sg_rtctime_t.year,
                                                    sg_rtctime_t.month,
                                                    sg_rtctime_t.data,
                                                    sg_rtctime_t.hour,
                                                    sg_rtctime_t.min,
                                                    sg_rtctime_t.sec);
    } 
    else if(mode == 1) 
    {
        sprintf((char*)times,"%04d%02d%02d%02d%02d%02d",sg_rtctime_t.year,
                                                    sg_rtctime_t.month,
                                                    sg_rtctime_t.data,
                                                    sg_rtctime_t.hour,
                                                    sg_rtctime_t.min,
                                                    sg_rtctime_t.sec);
        times[14] = sg_rtctime_t.hour%10+'0';
        times[15] = sg_rtctime_t.min%10+'0';
        times[16] = sg_rtctime_t.sec%10+'0';
    }
    else if(mode == 2) 
    {
        sprintf((char*)times,"%04d%02d%02d",sg_rtctime_t.year,
                                            sg_rtctime_t.month,
                                            sg_rtctime_t.data);
    } 
    return times;
    
}

/*
*********************************************************************************************************
*    函 数 名: app_set_current_time
*    功能说明: 设置时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_current_time(int *time,uint8_t conv)
{
    rtc_time_t time_t;
    
    time_t.year  = time[0];
    time_t.month = time[1];
    time_t.data  = time[2];
    time_t.hour  = time[3];
    time_t.min   = time[4];
    time_t.sec   = time[5];
    
    if(conv) // 是否需要转换
    {
        rtc_time_t conv_time;
        local_to_utc_time(&conv_time,8,time_t);
        RTC_set_Time(conv_time);
    }
    else
    RTC_set_Time(time_t);
}

/*
*********************************************************************************************************
*    函 数 名: app_get_next_ping_time
*    功能说明: 获取ping的间隔时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint32_t app_get_next_ping_time(void)
{
    return sg_comparam_t.ping;
}

/*
*********************************************************************************************************
*    函 数 名: app_get_next_dev_ping_time
*    功能说明: 获取下一个设备的ping时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint32_t app_get_next_dev_ping_time(void)
{
    return sg_comparam_t.dev_ping;
}

/*
*********************************************************************************************************
*    函 数 名: app_get_report_time
*    功能说明: 获取通信上报间隔时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint32_t app_get_report_time(void) 
{
    return sg_comparam_t.report;
}
/*
*********************************************************************************************************
*    函 数 名: app_get_report_time
*    功能说明: 获取通信上报间隔时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t app_get_network_delay_time(void) 
{
    return sg_comparam_t.network_time;
}
/*
*********************************************************************************************************
*    函 数 名: app_get_onvif_time
*    功能说明: 获取ONVIF间隔时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t app_get_onvif_time(void) 
{
    return sg_comparam_t.onvif_time;
}

/*
*********************************************************************************************************
*    函 数 名: app_get_main_network_ping_ip_addr
*    功能说明: 获取主网络ping地址
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_get_main_network_ping_ip_addr(uint8_t* ip)
{
    ip[0] = sg_sysparam_t.local.ping_ip[0];
    ip[1] = sg_sysparam_t.local.ping_ip[1];
    ip[2] = sg_sysparam_t.local.ping_ip[2];
    ip[3] = sg_sysparam_t.local.ping_ip[3];
}

/*
*********************************************************************************************************
*    函 数 名: app_get_main_network_sub_ping_ip_addr
*    功能说明: 获取主网pingip - 2
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_get_main_network_sub_ping_ip_addr(uint8_t* ip)
{
    ip[0] = sg_sysparam_t.local.ping_sub_ip[0];
    ip[1] = sg_sysparam_t.local.ping_sub_ip[1];
    ip[2] = sg_sysparam_t.local.ping_sub_ip[2];
    ip[3] = sg_sysparam_t.local.ping_sub_ip[3];
}
    
/*
*********************************************************************************************************
*    函 数 名: app_set_main_network_ping_ip
*    功能说明: 设置主网检测IP
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_main_network_ping_ip(uint8_t *ip)
{
    sg_sysparam_t.local.ping_ip[0] = ip[0];
    sg_sysparam_t.local.ping_ip[1] = ip[1];
    sg_sysparam_t.local.ping_ip[2] = ip[2];
    sg_sysparam_t.local.ping_ip[3] = ip[3];
    
    sg_sysparam_t.local.ping_sub_ip[0] = ip[4];
    sg_sysparam_t.local.ping_sub_ip[1] = ip[5];
    sg_sysparam_t.local.ping_sub_ip[2] = ip[6];
    sg_sysparam_t.local.ping_sub_ip[3] = ip[7];
    
    /* 保存 */
    app_set_save_infor_function(SAVE_LOCAL_NETWORK);
}

/*
*********************************************************************************************************
*    函 数 名: app_set_com_interface_selection_function
*    功能说明: 通信接口选择函数
*    形    参: @mode        : 0:有线 1:外网
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_com_interface_selection_function(uint8_t mode)
{
    sg_sysoperate_t.com.send_mode = mode;
}


/*
*********************************************************************************************************
*    函 数 名: app_get_com_interface_selection_function
*    功能说明: 通信接口选择函数
*    形    参: @mode        : 0:有线 1:外网
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t app_get_com_interface_selection_function(void)
{
    return sg_sysoperate_t.com.send_mode;
}


/*
*********************************************************************************************************
*    函 数 名: app_send_once_heart_infor
*    功能说明: 立刻进行一次心跳发生
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_send_once_heart_infor(void)
{
    sg_sysoperate_t.com_flag.heart_pack = 1;
}

/*
*********************************************************************************************************
*    函 数 名: app_send_query_configuration_infor
*    功能说明: 连接平台后，发送配置
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_send_query_configuration_infor(void)
{
    sg_sysoperate_t.com_flag.query_configuration = 1;
}


/*
*********************************************************************************************************
*    函 数 名: app_get_com_time_infor
*    功能说明: 获取通信间隔时间
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *app_get_com_time_infor(void)
{
    return &sg_comparam_t;
}

/*
*********************************************************************************************************
*    函 数 名: app_get_network_mode
*    功能说明: 获取网络模式
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t app_get_network_mode(void) 
{
    return sg_sysparam_t.local.server_mode;
}
/*
*********************************************************************************************************
*    函 数 名: app_get_carema_search_mode
*    功能说明: 获取摄像机搜索协议
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t app_get_carema_search_mode(void) 
{
    return sg_sysparam_t.local.search_mode;
}

/*
*********************************************************************************************************
*    函 数 名: app_set_com_time_param_function
*    功能说明: 设置通信相关时间参数:ping、上报
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_com_time_param_function(uint32_t *time,uint8_t mode) 
{
    if(mode == 0) 
    {
        sg_comparam_t.dev_ping   = time[1]*1000;
        sg_comparam_t.ping       = time[0]*1000;
        sg_comparam_t.onvif_time = time[2];  // ONVIF时间  20230811
    } 
    else     if(mode == 1) 
    {
        sg_comparam_t.report  = time[0]*1000;
    }
    else     if(mode == 2)   // 网络延时时间  20220308
    {
        sg_comparam_t.network_time  = time[0];
    }
    /* 保存 */
    app_set_save_infor_function(SAVE_COM_PARAMETER);
}


/*
*********************************************************************************************************
*    函 数 名: app_get_device_name
*    功能说明: 获取设备名称
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t *app_get_device_name(void)
{
    return sg_sysparam_t.device.name;
}

/*
*********************************************************************************************************
*    函 数 名: app_set_threshold_param_function
*    功能说明: 设置阈值
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_threshold_param_function(struct threshold_params param)
{
    sg_sysparam_t.threshold.volt_max = param.volt_max;
    sg_sysparam_t.threshold.volt_min = param.volt_min; 
    sg_sysparam_t.threshold.current = param.current;
    sg_sysparam_t.threshold.angle = param.angle;
    sg_sysparam_t.threshold.miu = param.miu;    
    sg_sysparam_t.threshold.humi_high = param.humi_high;
    sg_sysparam_t.threshold.humi_low = param.humi_low; 
    sg_sysparam_t.threshold.temp_high = param.temp_high;
    sg_sysparam_t.threshold.temp_low = param.temp_low; 
    sg_sysparam_t.threshold.net_reload = param.net_reload;
    sg_sysparam_t.threshold.net_retime = param.net_retime;    
    
    save_stroage_threshold_parameter(&sg_sysparam_t.threshold); 
}
/*
*********************************************************************************************************
*    函 数 名: app_get_threshold_param_function
*    功能说明: 获取阈值
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *app_get_threshold_param_function(void)
{
    return (&sg_sysparam_t.threshold);
}


/*
*********************************************************************************************************
*    函 数 名: app_get_backups_param_function
*    功能说明: 获取备份数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *app_get_backups_param_function(void)
{
    return (&sg_backups_t);
}
/*
*********************************************************************************************************
*    函 数 名: app_server_link_status_function
*    功能说明: 服务器连接时间判断
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_server_link_status_function(void)
{
    static uint32_t server_time_count = 0;

    server_time_count++;
    if(server_time_count >= SERVER_LINK_TIME)
    {
        server_time_count = 0;
        if(sg_backups_t.config_flag == 1)  // 配置过服务器
        {
            sg_backups_t.config_flag = 0;
            save_stroage_backups_function(&sg_backups_t);
            if((eth_get_tcp_status() != 2 ) && ( gsm_get_network_connect_status_function() != 1 ))
            {
                save_read_default_remote_ip(&sg_sysparam_t.remote);
                save_stroage_remote_ip_function(&sg_sysparam_t.remote);
                vTaskDelay(100);
//                set_reboot_time_function(1000); // 配置服务器后系统重启，防止设备未传到新平台
                app_system_softreset();
            }
        }
    }
}

/*
*********************************************************************************************************
*    函 数 名: app_save_backups_remote_param_function
*    功能说明: 备份服务器信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_save_backups_remote_param_function(void)
{
    memset(sg_backups_t.remote.outside_iporname,0,sizeof(sg_backups_t.remote.outside_iporname));
    memset(sg_backups_t.remote.inside_iporname,0,sizeof(sg_backups_t.remote.inside_iporname));
    strcpy((char*)sg_backups_t.remote.inside_iporname,(char*)sg_sysparam_t.remote.inside_iporname);
    sg_backups_t.remote.inside_port = sg_sysparam_t.remote.inside_port;
    strcpy((char*)sg_backups_t.remote.outside_iporname,(char*)sg_sysparam_t.remote.outside_iporname);    
    sg_backups_t.remote.outside_port = sg_sysparam_t.remote.outside_port;
    sg_backups_t.config_flag = 1;
    
    save_stroage_backups_function(&sg_backups_t); // 存储备份信息
}


/*
*********************************************************************************************************
*    函 数 名: app_power_fail_protection_function
*    功能说明: 掉电保护
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_power_fail_protection_function(void)
{
    /* 开关全部关闭 */
    if(relay_get_status(RELAY_1) == RELAY_ON) 
        relay_control(RELAY_1,RELAY_OFF);
    
    if(relay_get_status(RELAY_2) == RELAY_ON) 
        relay_control(RELAY_2,RELAY_OFF);

    if(relay_get_status(RELAY_3) == RELAY_ON) 
        relay_control(RELAY_3,RELAY_OFF);
}
/*
*********************************************************************************************************
*    函 数 名: app_power_open_protection_function
*    功能说明: 打开继电器
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_power_open_protection_function(void)
{
    uint8_t relay_index = 0;

    /* 开关全部打开 */
    for (relay_index = RELAY_1; relay_index <= RELAY_3; relay_index++)
    {
        relay_control((RELAY_DEV)relay_index, RELAY_ON); // 开继电器
        vTaskDelay(10); // 延时10ms
    }
}
/*
*********************************************************************************************************
*    函 数 名: app_open_exec_task_function
*    功能说明: 开关状态执行函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_open_exec_task_function(void)
{
    /* 检测设备是否需要散热 */
    if( det_get_inside_temp() < (sg_sysparam_t.threshold.temp_high-10))
    {
        fan_control(FAN_OFF);
    }
    else if( det_get_inside_temp() > sg_sysparam_t.threshold.temp_high)
    {
        fan_control(FAN_ON);
    }
}
/*
*********************************************************************************************************
*    函 数 名: app_send_data_task_function
*    功能说明: 发送数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_send_data_task_function(void)
{
    if(sg_sysoperate_t.com.send_mode == 0)
        tcp_set_send_buff(sg_send_buff,sg_send_size);
    else
        gsm_send_tcp_data(sg_send_buff,sg_send_size);
}

/*
*********************************************************************************************************
*    函 数 名: app_set_update_status_function
*    功能说明: 更新结果
*    形    参: @mode        :
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_update_status_function(uint8_t flag)
{
    sg_sysoperate_t.update.status = flag;
}

/*
*********************************************************************************************************
*    函 数 名: app_set_update_status_function
*    功能说明: 更新结果
*    形    参: @mode        :
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t app_get_update_status_function(void)
{
    return sg_sysoperate_t.update.status;
}

/*
*********************************************************************************************************
*    函 数 名: app_get_http_ota_function
*    功能说明: 获取更新
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *app_get_http_ota_function(void)
{
    return (&sg_sysparam_t.ota);
} 
/*
*********************************************************************************************************
*    函 数 名: app_set_http_ota_function
*    功能说明: 存储更新地址
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_http_ota_function(struct update_addr param)
{
    memcpy(&sg_sysparam_t.ota,&param,sizeof(struct update_addr));
    save_stroage_http_ota_function(&sg_sysparam_t.ota);
}

/*
*********************************************************************************************************
*    函 数 名: app_get_snmp_oid_function
*    功能说明: 获取snmp OID
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *app_get_snmp_oid_function(void)
{
    return (&sg_snmp_oid_t);
} 
/*
*********************************************************************************************************
*    函 数 名: app_get_snmp_dev_type_function
*    功能说明: 获取SNMP设备类型
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t app_get_snmp_dev_type_function(void)
{    
    return  sg_sysoperate_t.sys.snmp_dev_type;
}

/*
*********************************************************************************************************
*    函 数 名: app_system_softreset
*    功能说明: 系统重启
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_system_softreset(void)
{
    save_stroage_electricity_function(&sg_datacollec_t.electricity_current); // 重启之前先保存下用电量
    set_reboot_time_function(1000);
}

/*
*********************************************************************************************************
*    函 数 名: app_set_snmp_ip_function
*    功能说明: 存储SNMP IP
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_snmp_ip_function(uint8_t ip[4])
{
    sg_snmp_oid_t.switch_ip[0] = ip[0];
    sg_snmp_oid_t.switch_ip[1] = ip[1];
    sg_snmp_oid_t.switch_ip[2] = ip[2];
    sg_snmp_oid_t.switch_ip[3] = ip[3];

    app_set_save_infor_function(SAVE_SNMP_OID);
}

/*
*********************************************************************************************************
*    函 数 名: app_sys_operate_relay
*    功能说明: 继电器重启
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_sys_operate_relay(void)
{
    static uint32_t relay_timess[RELAY_NUM] = {0};
    uint32_t current_times = HAL_GetTick();
    
    for(uint8_t i=0;i<RELAY_NUM;i++)
    {
        if(sg_sysoperate_t.sys_flag.relay_reset[i] == 1)
        {
            relay_control((RELAY_DEV)i,RELAY_OFF);
            relay_timess[i] = HAL_GetTick();
            sg_sysoperate_t.sys_flag.relay_reset[i] = 2;
        }
        else if(sg_sysoperate_t.sys_flag.relay_reset[i] == 2)
        {        
//            relay_time[i]--;Timer_GetTick
//            if(relay_time[i] == 0)
            if((current_times - relay_timess[i]) > 15000)
            {        
                relay_control((RELAY_DEV)i,RELAY_ON);
                sg_sysoperate_t.sys_flag.relay_reset[i]    = 0;            
            }
        }    
    }
}
