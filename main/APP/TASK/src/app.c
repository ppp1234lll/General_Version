#include "main.h"
#include "./Task/inc/app.h"
#include "./Task/inc/send.h"

/* 发送缓冲区 */
#define APP_SEND_BUFF_SIZE  (2048)
#define SERVER_LINK_TIME    (30000) // 服务器连接时间  5min = 30000ms/10ms
#define NETWAOK_RELOAD_TIME   60


typedef struct
{
    struct
    {
        uint32_t       heart_time;      // 心跳计时
        uint32_t       report_time;     // 上报时间
        uint8_t        send_status;     // 发送状态: 0-当前没有发送 1-当前有发送 2-发送有结果
        uint8_t        return_cmd;      // 回复标志
        uint8_t        return_error;    // 回复内容
        uint8_t        return_src;      // 回复目标通道(收到指令的来源通道) com_channel_e
        uint8_t        immediate_save;  // 立即上报是否写日志
    } com;
    struct
    {
        uint32_t status;    /* 存储标志位，按位使用 */
    } save_flag;
    struct
    {
        uint32_t status;    /* 存储标志位，按位使用 */
    } com_flag;
    struct
    {
        uint8_t caramer_num;            // 摄像机编号  20220329
        uint8_t snmp_dev_type;          // SNMP设备类型：摄像机0/1、ONV、交换机
    } sys;
    struct
    {
        uint8_t lwip_reset;                     // 网络重启标志
        uint8_t relay_reset[RELAY_NUM];         // 继电器-标志位

        uint8_t net_reload_id[RELAY_NUM];       // 网络设备重启
        uint8_t net_reload_num[RELAY_NUM];      // 网络设备重启次数    
        uint8_t net_reload_times[RELAY_NUM];    // 网络设备重启计时    
    } sys_flag;
    uint8_t memory;             // 内存利用率
}sys_operate_t;

/* 参数定义 */
__attribute__((section (".RAM_D1"))) sys_backups_t sg_backups_t  ; // 备份信息 20231022
__attribute__((section (".RAM_D1"))) sys_operate_t sg_sysoperate_t; // 系统操作参数：包括通信、存储、计时
__attribute__((section (".RAM_D1"))) sys_param_t   sg_sysparam_t   ; // 系统参数：本地、远端、设备、上报相关参数
__attribute__((section (".RAM_D1"))) carema_t      sg_carema_param_t;   // 摄像头信息
__attribute__((section (".RAM_D1"))) com_param_t   sg_comparam_t    = {
    90000,
    60000,
    60000,
    60000,
};                 // 通信参数：心跳、上报、ping间隔时间
__attribute__((section (".RAM_D1"))) rtc_time_t  sg_rtctime_t = {0};        // rtc采集间隔时间
__attribute__((section (".RAM_D1"))) snmp_oid_t  sg_snmp_oid_t = {0};       // SNMP OID

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
    uint32_t last_get_time = 0;
    
    /* 开启系统指示灯 */
    led_control_function(LD_STATE,LD_FLICKER);

    for(;;)
    {
        app_task_save_function();           // 存储相关任务
        com_deal_main_function();           // 处理接收数据
        app_com_send_function();            // 通信发送
        app_open_exec_task_function();        
        app_sys_net_operate_relay();
        app_sys_operate_relay();
        
        if(HAL_GetTick() - last_get_time >= 1000)
        {
            last_get_time = HAL_GetTick();
            RTC_Get_Time(&sg_rtctime_t);        /* 时间获取 */
            
            /* 内存利用率 */
            sg_sysoperate_t.memory = my_mem_perused(SRAMIN);
        }
        FeedFwdgt();    
        // 接收已事件驱动: 阻塞点在 com_deal_main_function 内的帧队列等待(≤10ms), 不再固定延时
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
            sg_sysoperate_t.com_flag.status |= COM_FLAG_QUERY_CONFIGURATION;
            break;
        case CR_QUERY_INFO:
            sg_sysoperate_t.com_flag.status = (sg_sysoperate_t.com_flag.status & ~COM_FLAG_REPORT_NORMALLY_MASK) | (1 << COM_FLAG_REPORT_NORMALLY_POS);
            break;
        case CR_QUERY_SOFTWARE_VERSION:
            sg_sysoperate_t.com_flag.status |= COM_FLAG_VERSION;
            break;
        case CR_QUERY_IPC_IP:          // 查询IP地址
            sg_sysoperate_t.com_flag.status |= COM_FLAG_IPC_INFO;
            break;
        case CR_QUERY_SNMP_INFO:          // 查询SNMP参数
            sg_sysoperate_t.sys.snmp_dev_type = data;
            sg_sysoperate_t.com_flag.status |= COM_FLAG_SNMP_PARAM;
            break;
        case CR_QUERY_PING_INFO:          // 查询Ping信息
            sg_sysoperate_t.com_flag.status |= COM_FLAG_PING_INFO;
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
    sg_sysoperate_t.com.return_cmd   = cmd;
    sg_sysoperate_t.com.return_error = error;
    sg_sysoperate_t.com_flag.status |= COM_FLAG_CONFIG_RETURN;
}

/*
*********************************************************************************************************
*    函 数 名: app_set_reply_src
*    功能说明: 记录当前正在处理的指令来源通道, 供回复(ACK/查询响应)原路返回使用
*    形    参: @src : com_channel_e(0-TCP 1-GSM)
*    返 回 值: 无
*********************************************************************************************************
*/
void app_set_reply_src(uint8_t src)
{
    sg_sysoperate_t.com.return_src = src;
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
    /* 用标志位去重：多次调用只置同一个 flag，由 app_deal_com_flag_function 统一发送一次 */
    sg_sysoperate_t.com_flag.status |= COM_FLAG_REPORT_IMMEDIATELY;
    if (if_save)
    {
        sg_sysoperate_t.com.immediate_save = 1;
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
    static uint32_t ping_time = 0;

    /* 心跳发送 */
    if (sg_sysoperate_t.com_flag.status & COM_FLAG_HEART_PACK)
    {
        sg_sysoperate_t.com_flag.status &= ~COM_FLAG_HEART_PACK;
        sg_sysoperate_t.com.heart_time  = 0;

        send_msg_t *msg = send_msg_alloc(MSG_PRIO_NORMAL, COM_HEART_UPDATA, 1);
        if (msg) {
            com_heart_pack_function(msg->buf, &msg->size);
            send_msg_enqueue(msg);
        }
    }

    /* 正常/查询上报 */
    if ((sg_sysoperate_t.com_flag.status & COM_FLAG_REPORT_NORMALLY_MASK) != 0)
    {
        uint8_t report_flag = sg_sysoperate_t.com_flag.status & COM_FLAG_REPORT_NORMALLY_MASK;
        uint8_t retries = (report_flag == (2 << COM_FLAG_REPORT_NORMALLY_POS)) ? SEND_RETRY_MAX : 0;
        sg_sysoperate_t.com_flag.status &= ~COM_FLAG_REPORT_NORMALLY_MASK;

        send_msg_t *msg = send_msg_alloc(MSG_PRIO_NORMAL, CR_QUERY_INFO, retries);
        if (msg) {
            com_report_normally_function(msg->buf, &msg->size, CR_QUERY_INFO);
            send_msg_enqueue(msg);
        }
    }

    /* 异常/恢复立即上报 (来自 alarm 等检测，标志位去重：多次置位只发一次) */
    if (sg_sysoperate_t.com_flag.status & COM_FLAG_REPORT_IMMEDIATELY)
    {
        sg_sysoperate_t.com_flag.status &= ~COM_FLAG_REPORT_IMMEDIATELY;

        send_msg_t *msg = send_msg_alloc(MSG_PRIO_HIGH, CR_QUERY_INFO, SEND_RETRY_MAX);
        if (msg) {
            com_report_normally_function(msg->buf, &msg->size, CR_QUERY_INFO);
            if (sg_sysoperate_t.com.immediate_save)
            {
                log_device_write(msg->buf, msg->size);
                sg_sysoperate_t.com.immediate_save = 0;
            }
            send_msg_enqueue(msg);
        }
    }

    /* 查询配置 */
    if (sg_sysoperate_t.com_flag.status & COM_FLAG_QUERY_CONFIGURATION)
    {
        sg_sysoperate_t.com_flag.status &= ~COM_FLAG_QUERY_CONFIGURATION;

        send_msg_t *msg = send_msg_alloc(MSG_PRIO_HIGH, CR_QUERY_CONFIG, 0);
        if (msg) {
            msg->dst = sg_sysoperate_t.com.return_src;   /* 查询响应原路返回 */
            com_query_configuration_function(msg->buf, &msg->size);
            send_msg_enqueue(msg);
        }
    }

    /* 版本信息 */
    if (sg_sysoperate_t.com_flag.status & COM_FLAG_VERSION)
    {
        sg_sysoperate_t.com_flag.status &= ~COM_FLAG_VERSION;

        send_msg_t *msg = send_msg_alloc(MSG_PRIO_HIGH, CR_QUERY_SOFTWARE_VERSION, 0);
        if (msg) {
            msg->dst = sg_sysoperate_t.com.return_src;   /* 版本响应原路返回 */
            com_version_information(msg->buf, &msg->size);
            send_msg_enqueue(msg);
        }
    }

    /* 查询摄像机信息 */
    if (sg_sysoperate_t.com_flag.status & COM_FLAG_IPC_INFO)
    {
        if (onvif_time == 0)
        {
            printf("..........start onvif............\n");
            eth_set_onvif_flag(1);
            onvif_time = HAL_GetTick();
        }
        else
        {
            if ((eth_get_onvif_flag() == ETH_ONVIF_END) || (HAL_GetTick() - onvif_time > 15000))
            {
                printf(".........end onvif............\n");
                onvif_time = 0;
                sg_sysoperate_t.com_flag.status &= ~COM_FLAG_IPC_INFO;

                send_msg_t *msg = send_msg_alloc(MSG_PRIO_HIGH, CR_QUERY_IPC_IP, 0);
                if (msg) {
                    msg->dst = sg_sysoperate_t.com.return_src;   /* IPC查询响应原路返回 */
                    com_ipc_device_information(msg->buf, &msg->size);
                    send_msg_enqueue(msg);
                }
            }
        }
    }

    /* 查询SNMP参数 */
    if (sg_sysoperate_t.com_flag.status & COM_FLAG_SNMP_PARAM)
    {
        if (snmp_time == 0)
        {
            printf("..........start snmp param............\n");
            snmp_set_enable_flag(SNMP_START);   // 先置查询标志
            eth_set_snmp_flag(1);               // 请求 eth 任务经状态机创建SNMP任务
            snmp_time = HAL_GetTick();
        }
        else
        {
            if ((snmp_get_status() == 0) || (HAL_GetTick() - snmp_time > 10000))
            {
                printf(".........end snmp param............\n");
                snmp_time = 0;
                eth_set_snmp_flag(0);   // 清除残留请求(超时/网口未就绪时)
                sg_sysoperate_t.com_flag.status &= ~COM_FLAG_SNMP_PARAM;

                send_msg_t *msg = send_msg_alloc(MSG_PRIO_HIGH, CR_QUERY_SNMP_INFO, 0);
                if (msg) {
                    msg->dst = sg_sysoperate_t.com.return_src;   /* SNMP查询响应原路返回 */
                    com_device_snmp_information(msg->buf, &msg->size);
                    send_msg_enqueue(msg);
                }
            }
        }
    }

    /* 查询PING */
    if (sg_sysoperate_t.com_flag.status & COM_FLAG_PING_INFO)
    {
        if (ping_time == 0)
        {
            printf("..........start ping info............\n");
            eth_set_ping_status(1);
            ping_time = HAL_GetTick();
        }
        else
        {
            if ((eth_get_ping_status() == 0) || (HAL_GetTick() - ping_time > 10000))
            {
                printf(".........end ping info............\n");
                ping_time = 0;
                sg_sysoperate_t.com_flag.status &= ~COM_FLAG_PING_INFO;

                send_msg_t *msg = send_msg_alloc(MSG_PRIO_HIGH, CR_QUERY_PING_INFO, 0);
                if (msg) {
                    msg->dst = sg_sysoperate_t.com.return_src;   /* PING查询响应原路返回 */
                    com_device_ping_information(msg->buf, &msg->size);
                    send_msg_enqueue(msg);
                }
            }
        }
    }

    /* RS485透传 */
#if (configUSE_RS485_TRANSPARENT == 1)
    {
        uint8_t *rx_ptr;
        if (xQueueReceive(dtu_rx_q, &rx_ptr, 0) == pdTRUE)
        {
            send_msg_t *msg = send_msg_alloc(MSG_PRIO_NORMAL, CONFIGURE_RS485_UOSTREAM, 0);
            if (msg) {
                msg->dst = sg_sysoperate_t.com.return_src;   /* RS485透传响应原路返回 */
                com_rs485_up_param_function(rx_ptr, msg->buf, &msg->size);
                send_msg_enqueue(msg);
            }
        }
    }
#endif

    /* 配置回传 ACK */
    if (sg_sysoperate_t.com_flag.status & COM_FLAG_CONFIG_RETURN)
    {
        sg_sysoperate_t.com_flag.status &= ~COM_FLAG_CONFIG_RETURN;

        send_msg_t *msg = send_msg_alloc(MSG_PRIO_HIGH, sg_sysoperate_t.com.return_cmd, 0);
        if (msg) {
            msg->dst = sg_sysoperate_t.com.return_src;   /* ACK 原路返回 */
            com_ack_function(msg->buf, &msg->size,
                            sg_sysoperate_t.com.return_cmd,
                            sg_sysoperate_t.com.return_error);
            send_msg_enqueue(msg);
        }
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
*    函 数 名: app_com_send_function
*    功能说明: 通信发送函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_com_send_function(void)
{
    /* 处理通信标志位：分配消息 → 组包 → 入队 */
    /* 网络不通时消息正常入队，由 send_task 根据网络状态决定何时发送/重试 */
    app_deal_com_flag_function();
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
    /* 通知发送任务结果已到達 */
    send_task_notify_result((uint8_t)data);
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
    if (sg_sysoperate_t.com.heart_time > sg_comparam_t.heart)
    {
        sg_sysoperate_t.com.heart_time = 0;
        /* 发送一次心跳 */
        sg_sysoperate_t.com_flag.status |= COM_FLAG_HEART_PACK;
    }

    /* 正常上报 */
    sg_sysoperate_t.com.report_time++;
    if (sg_sysoperate_t.com.report_time > (sg_comparam_t.report))
    {
        sg_sysoperate_t.com.report_time = 0;
        /* 进行一次上报 */
        sg_sysoperate_t.com_flag.status = (sg_sysoperate_t.com_flag.status & ~COM_FLAG_REPORT_NORMALLY_MASK) | (2 << COM_FLAG_REPORT_NORMALLY_POS);
    }

    /* 注：发送超时由独立发送任务(send_task)自行管理，此处不再追踪 */
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
            /* ACK回传：分配消息 → 组包 → 入队 */
            {
                send_msg_t *msg = send_msg_alloc(MSG_PRIO_HIGH,sg_sysoperate_t.com.return_cmd,3);
                if (msg) {
                    com_ack_function(msg->buf, &msg->size,
                                     sg_sysoperate_t.com.return_cmd,
                                     sg_sysoperate_t.com.return_error);
                    send_msg_enqueue(msg);
                }
            }
            vTaskDelay(100);
            lfs_unmount(&g_lfs_t);
            vTaskDelay(100);
            /* 重启设备 */
            app_system_softreset();
            break;
        case CR_LWIP_NETWORK_RESET:    
            sg_sysoperate_t.com.return_error = 1;
            /* ACK回传 */
            {
                send_msg_t *msg = send_msg_alloc(MSG_PRIO_HIGH,sg_sysoperate_t.com.return_cmd,3);
                if (msg) {
                    com_ack_function(msg->buf, &msg->size,
                                     sg_sysoperate_t.com.return_cmd,
                                     sg_sysoperate_t.com.return_error);
                    send_msg_enqueue(msg);
                }
            }
            vTaskDelay(100);
            eth_set_network_reset();
            break;
            
        case CR_GPRS_NETWORK_RESET:
            sg_sysoperate_t.com.return_error = 1;
            /* ACK回传 */
            {
                send_msg_t *msg = send_msg_alloc(MSG_PRIO_HIGH,sg_sysoperate_t.com.return_cmd,3);
                if (msg) {
                    com_ack_function(msg->buf, &msg->size,
                                     sg_sysoperate_t.com.return_cmd,
                                     sg_sysoperate_t.com.return_error);
                    send_msg_enqueue(msg);
                }
            }
            vTaskDelay(100);
            /* 重启GPRS */
            gsm_set_module_reset_function();
            break;
            
            case CR_GPRS_NETWORK_V_RESET:
            sg_sysoperate_t.com.return_error = 1;
            /* ACK回传 */
            {
                send_msg_t *msg = send_msg_alloc(MSG_PRIO_HIGH,sg_sysoperate_t.com.return_cmd,3);
                if (msg) {
                    com_ack_function(msg->buf, &msg->size,
                                    sg_sysoperate_t.com.return_cmd,
                                    sg_sysoperate_t.com.return_error);
                    send_msg_enqueue(msg);
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
    save_read_local_network(&sg_sysparam_t.local);  // 读取本地网络参数
    save_read_remote_ip_function(&sg_sysparam_t.remote);  // 读取远程网络参数
    save_read_device_paramter_function(&sg_sysparam_t.device);  // 读取设备参数
    save_read_com_param_function(&sg_comparam_t);  // 读取通信相关参数
    save_read_carema_parameter(&sg_carema_param_t);  // 读取摄像头参数
    save_read_threshold_parameter(&sg_sysparam_t.threshold); // 读取阈值参数
    save_read_http_ota_function(&sg_sysparam_t.ota);  // 读取OTA参数
    save_read_backups_function(&sg_backups_t);  // 读取备份参数
    save_read_snmp_oid_parameter(&sg_snmp_oid_t); // 读取OID参数
    save_read_http_upload_function(&sg_sysparam_t.upload); // 读取上传参数
    save_read_electricity_function(&sg_datacollec_t.electricity_all);// 读取用电量参数
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
    if(sg_sysoperate_t.save_flag.status & SAVE_FLAG_DEVICE_PARAM)
    {
        sg_sysoperate_t.save_flag.status &= ~SAVE_FLAG_DEVICE_PARAM;
        save_storage_device_parameter_function(&sg_sysparam_t.device);
    }    
    
    if(sg_sysoperate_t.save_flag.status & SAVE_FLAG_LOCAL_NETWORK)
    {
        sg_sysoperate_t.save_flag.status &= ~SAVE_FLAG_LOCAL_NETWORK;
        save_stroage_local_network(&sg_sysparam_t.local);
        vTaskDelay(100);
        eth_set_network_reset();
    }
    
    if(sg_sysoperate_t.save_flag.status & SAVE_FLAG_REMOTE_NETWORK)
    {
        sg_sysoperate_t.save_flag.status &= ~SAVE_FLAG_REMOTE_NETWORK;
        save_stroage_remote_ip_function(&sg_sysparam_t.remote);
        vTaskDelay(100);
        eth_set_network_reset();              // 重启网络
        gsm_set_module_reset_function();     // 重启GPRS
    }
    /* 存储摄像头参数 */    
    if(sg_sysoperate_t.save_flag.status & SAVE_FLAG_CAREMA)
    {
        sg_sysoperate_t.save_flag.status &= ~SAVE_FLAG_CAREMA;
        save_stroage_carema_parameter(&sg_carema_param_t);
    }
    /* 存储通信相关参数 */    
    if(sg_sysoperate_t.save_flag.status & SAVE_FLAG_COM_PARAMETER)
    {
        sg_sysoperate_t.save_flag.status &= ~SAVE_FLAG_COM_PARAMETER;
        save_stroage_com_param_function(&sg_comparam_t);
    }
    
    if(sg_sysoperate_t.save_flag.status & SAVE_FLAG_THRESHOLD)
    {
        sg_sysoperate_t.save_flag.status &= ~SAVE_FLAG_THRESHOLD;
        save_stroage_threshold_parameter(&sg_sysparam_t.threshold);
    }
    /* 存储SNMP OID参数 */    
    if(sg_sysoperate_t.save_flag.status & SAVE_FLAG_SNMP_OID)
    {
        sg_sysoperate_t.save_flag.status &= ~SAVE_FLAG_SNMP_OID;
        save_stroage_snmp_oid_parameter(&sg_snmp_oid_t);
    }
    
    /* 存储文件上传地址 */    
    if(sg_sysoperate_t.save_flag.status & SAVE_FLAG_UPLOAD_ADDR)
    {
        sg_sysoperate_t.save_flag.status &= ~SAVE_FLAG_UPLOAD_ADDR;
        save_stroage_http_upload_function(&sg_sysparam_t.upload);
    }   
    
    /* 恢复出厂化：产品序列号不变 */
    if(sg_sysoperate_t.save_flag.status & SAVE_FLAG_RESET)
    {
        sg_sysoperate_t.save_flag.status &= ~SAVE_FLAG_RESET;
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
void app_set_save_infor_function(uint32_t flags)
{
    sg_sysoperate_t.save_flag.status |= flags;
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
*    功能说明: 存储部分网络参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_local_network_function(struct local_ip_t param)
{
    memcpy(sg_sysparam_t.local.ip,param.ip,4);
    memcpy(sg_sysparam_t.local.gateway,param.gateway,4);
    memcpy(sg_sysparam_t.local.netmask,param.netmask,4);
    memcpy(sg_sysparam_t.local.dns,param.dns,4);
    {
        uint8_t mac_zero = 1;
        uint8_t mac_diff = 0;
        for (uint8_t j = 0; j < 6; j++)
        {
            if (param.mac[j] != 0) mac_zero = 0;
            if (param.mac[j] != sg_sysparam_t.local.mac[j]) mac_diff = 1;
        }
        if (!mac_zero && mac_diff)
        {
            bsp_WriteCpuFlash_Save(DEVICE_FLASH_STORE, DEVICE_MAC_ADDR, (uint8_t *)param.mac, 6);
        }
    }
    memcpy(sg_sysparam_t.local.mac,param.mac,6);
    memcpy(sg_sysparam_t.local.ping_ip,param.ping_ip,4);
    memcpy(sg_sysparam_t.local.ping_sub_ip,param.ping_sub_ip,4);
    
    memcpy(sg_sysparam_t.local.multicast_ip,param.multicast_ip,4);
    sg_sysparam_t.local.multicast_port = param.multicast_port;

    app_set_save_infor_function(SAVE_FLAG_LOCAL_NETWORK);
}


/*
*********************************************************************************************************
*    函 数 名: app_set_transfer_mode_function
*    功能说明: 存储传输模式信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t app_set_transfer_mode_function(uint8_t mode) 
{
    int8_t ret = 0;
    switch(mode) 
    {
        case 0:
            if(sg_sysparam_t.local.server_mode != SERVER_MODE_LWIP)
            {
                sg_sysparam_t.local.server_mode = SERVER_MODE_LWIP;
                ret = 1;
            }
            break;
        case 1:
            if(sg_sysparam_t.local.server_mode != SERVER_MODE_GPRS)
            {
                sg_sysparam_t.local.server_mode = SERVER_MODE_GPRS;
                ret = 1;
            }
            break;
        case 2:
            if(sg_sysparam_t.local.server_mode != SERVER_MODE_AUTO)
            {
                sg_sysparam_t.local.server_mode = SERVER_MODE_AUTO;
                ret = 1;
            }
            break;
    }
    if(ret)
    {
        /* 模式切换只保存参数, 不触发网络硬件复位 */
        save_stroage_local_network(&sg_sysparam_t.local);
    }
    return ret;
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
    app_set_save_infor_function(SAVE_FLAG_LOCAL_NETWORK);
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
    app_set_save_infor_function(SAVE_FLAG_REMOTE_NETWORK);
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
    sg_sysoperate_t.save_flag.status |= SAVE_FLAG_RESET;
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
    app_set_save_infor_function(SAVE_FLAG_CAREMA);
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
    app_set_save_infor_function(SAVE_FLAG_CAREMA);
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
    char name_tmp[sizeof(sg_carema_param_t.name[num])] = {0};
    char pwd_tmp[sizeof(sg_carema_param_t.pwd[num])] = {0};

    snprintf(name_tmp,sizeof(name_tmp),"%s",name_buf);
    snprintf(pwd_tmp,sizeof(pwd_tmp),"%s",pwd_buf);

    memset(sg_carema_param_t.name[num],0,sizeof(sg_carema_param_t.name[num]));
    memset(sg_carema_param_t.pwd[num],0,sizeof(sg_carema_param_t.pwd[num]));
    snprintf(sg_carema_param_t.name[num],sizeof(sg_carema_param_t.name[num]),"%s",name_tmp);
    snprintf(sg_carema_param_t.pwd[num],sizeof(sg_carema_param_t.pwd[num]),"%s",pwd_tmp);
    sg_carema_param_t.port[num] = port;
    app_set_save_infor_function(SAVE_FLAG_CAREMA);    /* 存储 */
}

/*
*********************************************************************************************************
*    函 数 名: app_get_camera_login_function
*    功能说明: 获取指定摄像机用户名、密码
*    形    参: @ip            :
*    返 回 值: 摄像机号数
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
    app_set_save_infor_function(SAVE_FLAG_CAREMA);
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
    app_set_save_infor_function(SAVE_FLAG_CAREMA);
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
    app_set_save_infor_function(SAVE_FLAG_CAREMA);
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
    app_set_save_infor_function(SAVE_FLAG_THRESHOLD);    /* 存储 */
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
    app_set_save_infor_function(SAVE_FLAG_THRESHOLD);    /* 存储 */
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
    app_set_save_infor_function(SAVE_FLAG_THRESHOLD);
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
    app_set_save_infor_function(SAVE_FLAG_THRESHOLD);
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
    app_set_save_infor_function(SAVE_FLAG_THRESHOLD);    /* 存储 */
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
    app_set_save_infor_function(SAVE_FLAG_THRESHOLD);    /* 存储 */
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
    
    bsp_WriteCpuFlash_Save(DEVICE_FLASH_STORE,DEVICE_ID_ADDR,(uint8_t*)param.id.c,4);
    app_set_save_infor_function(SAVE_FLAG_DEVICE_PARAM);
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
    app_set_save_infor_function(SAVE_FLAG_DEVICE_PARAM);
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
    app_set_save_infor_function(SAVE_FLAG_COM_PARAMETER);
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
        app_set_save_infor_function(SAVE_FLAG_COM_PARAMETER);
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
    app_set_save_infor_function(SAVE_FLAG_COM_PARAMETER);
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
    sg_sysparam_t.threshold.net_delay_time = time_dev;
    app_set_save_infor_function(SAVE_FLAG_THRESHOLD);
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
*    形    参: @mode : 0：秒 1：毫秒
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
    return sg_sysparam_t.threshold.net_delay_time;
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
    app_set_save_infor_function(SAVE_FLAG_LOCAL_NETWORK);
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
    sg_sysoperate_t.com.heart_time = 0;
    sg_sysoperate_t.com_flag.status |= COM_FLAG_HEART_PACK;
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
    sg_sysoperate_t.com_flag.status |= COM_FLAG_QUERY_CONFIGURATION;
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
*    函 数 名: app_set_report_time_function
*    功能说明: 设置上报时间间隔
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_report_time_function(uint32_t time) 
{
    sg_comparam_t.report  = time*1000;
    app_set_save_infor_function(SAVE_FLAG_COM_PARAMETER);
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
    sg_sysparam_t.threshold.volt_max        = param.volt_max;
    sg_sysparam_t.threshold.volt_min        = param.volt_min; 
    sg_sysparam_t.threshold.current         = param.current;
    sg_sysparam_t.threshold.angle           = param.angle;
    sg_sysparam_t.threshold.miu             = param.miu;    
    sg_sysparam_t.threshold.humi_high       = param.humi_high;
    sg_sysparam_t.threshold.humi_low        = param.humi_low; 
    sg_sysparam_t.threshold.temp_high       = param.temp_high;
    sg_sysparam_t.threshold.temp_low        = param.temp_low; 
    sg_sysparam_t.threshold.net_reload      = param.net_reload;
    sg_sysparam_t.threshold.net_retime      = param.net_retime;    
    sg_sysparam_t.threshold.net_delay_time  = param.net_delay_time;
    app_set_save_infor_function(SAVE_FLAG_THRESHOLD);
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
    app_set_save_infor_function(SAVE_FLAG_UPDATE_ADDR);
}
/*
*********************************************************************************************************
*    函 数 名: app_get_http_upload_function
*    功能说明: 获取上传地址
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *app_get_http_upload_function(void)
{
    return (&sg_sysparam_t.upload);
} 
/*
*********************************************************************************************************
*    函 数 名: app_set_http_upload_function
*    功能说明: 存储上传地址
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void app_set_http_upload_function(struct upload_addr param)
{
    memcpy(&sg_sysparam_t.upload,&param,sizeof(struct upload_addr));
    app_set_save_infor_function(SAVE_FLAG_UPLOAD_ADDR);
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

    app_set_save_infor_function(SAVE_FLAG_SNMP_OID);
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
