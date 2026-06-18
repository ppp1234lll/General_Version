#include "./Task/inc/print.h"
#include "main.h"

/* 发送状态 */
#define SEND_TIME_MAX     (3000) // 3s上报
#define PRINT_BUFF_LEN    (500) // 3s上报

__attribute__((section (".RAM_D1"))) uint8_t  print_buff[PRINT_BUFF_LEN];  // 接收缓存
    
typedef struct
{
    uint8_t recv_flag;  // 接收到数据标志
    struct
    {
        uint8_t return_cmd;      // 回复命令
        uint8_t return_result; // 回复内容
        uint32_t    report_time;     // 上报时间
    } send;
    struct
    {
        uint8_t search;            // 搜索
        uint8_t inquiry;            // 查询配置
        uint8_t update_mac;        // 配置MAC
        uint8_t update_relay;    // 配置继电器
        uint8_t update_ip;        // 配置IP
        uint8_t config_return;        // 回复
    } send_flag; 
    
}sys_print_t;

typedef struct
{
    uint8_t   mac[6];          // 网络参数
    uint8_t   net_ip[IP_ALL][4];  // 本机网络参数
    uint32_t  id;                          // 设备ID
    uint8_t     adapter[RELAY_ALL];  // 适配器
    char           types[15];          // 命令类型
    char           password[64];    // 密码
    uint32_t  addr;

    char      username[64];
    uint8_t   inside[128];
    uint8_t   outside[128];
    uint32_t   port1;
    uint32_t   port2;    
} print_param_t;

char matchip_list[IP_ALL][5] = {
DEVICE_IP,            
DEVICE_NM,             
DEVICE_GW,        
DEVICE_MAINIP1, 
DEVICE_MAINIP2, 
DEVICE_CAM1,          
DEVICE_CAM2,          
DEVICE_CAM3,         
DEVICE_CAM4,        
DEVICE_CAM5,    
DEVICE_CAM6,
};

char matchrelay_list[RELAY_ALL][4] = {
DEVICE_RELAY1,
DEVICE_RELAY2,
DEVICE_RELAY3,
DEVICE_RELAY4,
DEVICE_RELAY5,
DEVICE_RELAY6,
};


__attribute__((section (".RAM_D1"))) print_param_t print_param;
__attribute__((section (".RAM_D1"))) sys_print_t     print_status;

__attribute__((section (".RAM_D1"))) uint8_t   print_send_buff[1000];
__attribute__((section (".RAM_D1"))) uint16_t    print_send_size          = 0;     // 发送数据长度
/*
*********************************************************************************************************
*    函 数 名: print_task_function
*    功能说明: 检测任务：包括adc采集、开关量数据等
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_task_function(void)
{
    while(1)
    {
        print_buff_deal_function();     // 处理接收数据
        print_udp_send_function();    // 数据发送
        FeedFwdgt();
        vTaskDelay(50);           // 延时5ms
    }
}

/*
*********************************************************************************************************
*    函 数 名: print_stroage_queue_data
*    功能说明: 将数据存储到缓存区
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_stroage_queue_data(uint8_t *buff,uint16_t len)
{
    print_status.recv_flag = 0;
    memset(print_buff,0,PRINT_BUFF_LEN);
    memcpy(print_buff,buff,len);
    print_status.recv_flag = 1;  // 标志位置1
}

/*
*********************************************************************************************************
*    函 数 名: print_getparam_fun
*    功能说明: 根据str 取出xml中相应的参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int print_getparam_fun(char *data,char *sStr)
{
  char *str  = NULL;
    str = strstr(data,sStr);
    if(!str)
      return -1;
  else
        return 0;
}


/*
*********************************************************************************************************
*    函 数 名: print_queue_find_msg
*    功能说明: 获取缓存区中的数据,并根据秘钥解析
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int print_queue_find_msg(void)
{    
    int8_t     ret     = 0;
    int           temp[6] = {0};
    char    *str  = NULL;
    char    id_str[20] = {0};
        
    if(print_status.recv_flag == 1)
    {
        memset(&print_param,0,sizeof(print_param_t));
        print_status.recv_flag = 0;
        
        if((str = strstr((char *)print_buff,DEVICE_ID)) != NULL)   //     获取设备ID
        {
            str = strstr((char *)str,":");
            sscanf((char*)str,"%*[^\"]\"%[^\"]",id_str); 
            print_param.id = strtol(id_str, NULL, 16);
        }
        else
            return -1;
        
        if((str = strstr((char *)print_buff,DEVICE_TYPES)) != NULL) // 获取命令类型
        {
            str = strstr((char *)str,":");
            sscanf((char*)str,"%*[^\"]\"%[^\"]",print_param.types); // %*跳过，[^>]直到匹配到第一个:"，>%从:"开始，[^</]直到匹配到",结束
        }
        else
            return -2;    

        if((str = strstr((char *)print_buff,DEVICE_MAC)) != NULL) // 获取MAC地址
        {
            memset(id_str,0,sizeof(id_str));
            str = strstr((char *)str,":");
            sscanf((char*)str,"%*[^\"]\"%[^\"]",id_str); 
            ret = sscanf((char*)id_str,"%2x:%2x:%2x:%2x:%2x:%2x",&temp[0],&temp[1],&temp[2],&temp[3],&temp[4],&temp[5]);
            if(ret == 6) {
                print_param.mac[0] = temp[0];
                print_param.mac[1] = temp[1];
                print_param.mac[2] = temp[2];
                print_param.mac[3] = temp[3];
                print_param.mac[4] = temp[4];
                print_param.mac[5] = temp[5];
                ret = 0;
            }
            else
                return -3;                    
        }
        
        if((str = strstr((char *)print_buff,DEVICE_PWD)) != NULL)   // 获取密码
        {
            str = strstr((char *)str,":");
            sscanf((char*)str,"%*[^\"]\"%[^\"]",print_param.password); 
        }
        
        if((str = strstr((char *)print_buff,DEVICE_NAME)) != NULL)   // 获取用户名
        {
            str = strstr((char *)str,":");
            sscanf((char*)str,"%*[^\"]\"%[^\"]",print_param.username); 
        }
                
        for(uint8_t i=0;i < IP_ALL;i++)
        {
            if((str = strstr((char *)print_buff,matchip_list[i])) != NULL) 
            {
                memset(id_str,0,sizeof(id_str));
                str = strstr((char *)str,":");
                sscanf((char*)str,"%*[^\"]\"%[^\"]",id_str); 
                ret = sscanf((char*)id_str,"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
                if(ret == 4) {
                    print_param.net_ip[i][0] = temp[0];
                    print_param.net_ip[i][1] = temp[1];
                    print_param.net_ip[i][2] = temp[2];
                    print_param.net_ip[i][3] = temp[3];
                    ret = 0;
                }    
            }
        }

        for(uint8_t i=0;i < RELAY_ALL;i++)
        {
            if((str = strstr((char *)print_buff,matchrelay_list[i])) != NULL) 
            {
                memset(id_str,0,sizeof(id_str));
                str = strstr((char *)str,":");
                sscanf((char*)str,"%*[^\"]\"%[^\"]",id_str); 
                ret = sscanf((char*)id_str,"%01d",&temp[0]);
                if(ret == 1) {
                    print_param.adapter[i] = temp[0];
                    ret = 0;
                }
            }
        }
    if((str = strstr((char *)print_buff,DEVICE_INIP)) != NULL) 
        {
            str = strstr((char *)str,":");
            sscanf((char*)str,"%*[^\"]\"%[^\"]",print_param.inside);
        }
        
        if((str = strstr((char *)print_buff,DEVICE_OUTIP)) != NULL) 
        {
            str = strstr((char *)str,":");
            sscanf((char*)str,"%*[^\"]\"%[^\"]",print_param.outside);
        }        
        return 0;    
    }
    return  -1;    
}

/*
*********************************************************************************************************
*    函 数 名: print_buff_deal_function
*    功能说明: 缓存接收处理函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t print_buff_deal_function(void)
{
    struct device_param *device    = app_get_device_param_function();

    if(print_queue_find_msg() == 0)
    {
        /* ID验证 */
        if(print_param.id == 0 && (strcmp(print_param.types, CMD_SEARCH)==0)) 
        {
            print_status.send_flag.search =1;  // 搜索命令
            return 0;
        }
        
        /* ID错误 */
        if(print_param.id != device->id.i && (strcmp(print_param.types, CMD_UPDATE_MAC)!=0)) 
        {
            return -1;
        }
        
        if(strcmp(print_param.types, CMD_UPDATE_MAC) == 0 )
            print_tcp_configure_mac();  // 配置MAC
        
        if(print_param.id == device->id.i )        /* 根据命令解析数据 */
        {
            if(strcmp(print_param.types, CMD_INQUIRY)==0)
                print_status.send_flag.inquiry =1;  // 查询
            else if(strcmp(print_param.types, CMD_UPDATE_RELAY)==0)
                print_tcp_configure_rlelay();  // 配置继电器
            else if(strcmp(print_param.types, CMD_UPDATE_IP)==0)
                print_configure_local_network();  // 配置IP
            else if(strcmp(print_param.types, CMD_UPDATE_ADDR)==0)
                print_configure_run_addr();  // 切换程序
            else if(strcmp(print_param.types, CMD_UPDATE_SERVER)==0)
            {
                printf_tcp_configure_server_network();  // 配置服务器地址
            }
            else if(strcmp(print_param.types, CMD_UPDATE_NP)==0)
            {
                printf_tcp_configure_pwd_name();  // 配置用户名密码
            }
        }
    }
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: app_set_reply_parameters_function
*    功能说明: 设置回复参数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_reply_parameters_function(uint8_t cmd, uint8_t error)
{
    print_status.send.return_cmd = cmd;
    print_status.send.return_result = error;
    print_status.send_flag.config_return     = 1;
}

/*
*********************************************************************************************************
*    函 数 名: print_tcp_send_function
*    功能说明: tcp通信发送函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_udp_send_function(void)
{
    if(eth_get_udp_status() != 2)
    {
        return ;
    }
    print_deal_com_flag_function();
}


/*
*********************************************************************************************************
*    函 数 名: app_deal_com_flag_function
*    功能说明: 用来处理通信回复信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_deal_com_flag_function(void)
{
    /* 搜索设备回复 */
    if(print_status.send_flag.search == 1)
    {
        print_status.send_flag.search = 0;
        memset(print_send_buff,0,sizeof(print_send_buff));
        print_search_ack_function(print_send_buff,&print_send_size,0,0);
        udp_multicast_send_buff(print_send_buff,print_send_size);
    }
    
    /* 查询配置当前参数设置 */
    if(print_status.send_flag.inquiry == 1)
    {
        print_status.send_flag.inquiry = 0;
        memset(print_send_buff,0,sizeof(print_send_buff));
        print_inquiry_ack_function(print_send_buff,&print_send_size);
        udp_multicast_send_buff(print_send_buff,print_send_size);
    }
    /* 回传信号 */
    if(print_status.send_flag.config_return == 1)
    {
        print_status.send_flag.config_return = 0;
        switch(print_status.send.return_cmd)
        {
            case MAC_ACK:
                print_search_ack_function(print_send_buff,&print_send_size,MAC_ACK,print_status.send.return_result);
                break;
            case RELAY_ACK:        
                print_relay_ack_function(print_send_buff,&print_send_size,print_status.send.return_result);
                break;
            case IP_ACK:    
                print_search_ack_function(print_send_buff,&print_send_size,IP_ACK,print_status.send.return_result);
                break;
            case SERVER_ACK:    
                print_search_ack_function(print_send_buff,&print_send_size,SERVER_ACK,print_status.send.return_result);
                break;
            case NAMEPWD_ACK:    
                print_search_ack_function(print_send_buff,&print_send_size,NAMEPWD_ACK,print_status.send.return_result);
                break;
            default: break;            
        }
        udp_multicast_send_buff(print_send_buff,print_send_size);
    }
}

/*
*********************************************************************************************************
*    函 数 名: print_search_report_infor_function
*    功能说明: 生成查询回传包
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_search_ack_function(uint8_t *pdata, uint16_t *len,uint8_t ack,uint8_t result)
{
    struct device_param     *device = app_get_device_param_function();
    struct local_ip_t       *local  = app_get_local_network_function();
           carema_t       *ipc    = app_get_carema_param_function();
    struct remote_ip        *remote = app_get_remote_network_function();
    char  temp[30]     = {0};
    char  str[5]     = {0};
    
    my_cjson_create_function(pdata,0); // 开始
    my_cjson_info_create_function(pdata,0); // 开始    
    /* 设备唯一标识TID */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%X",device->id.i);
    my_cjson_join_string_function(pdata,(uint8_t*)"tid",(uint8_t*)temp,1);

    /** 系统时间 **/
    my_cjson_join_string_function(pdata,(uint8_t*)"date",app_get_report_current_time(2),0);
    my_cjson_info_create_function(pdata,1); // 结束
    
    my_cjson_data_create_function(pdata,0); // 开始
    switch(ack)
    {
        case NO_ACK:
            my_cjson_join_string_function(pdata,(uint8_t*)"types",(uint8_t*)"search",1);
            break;
        case MAC_ACK:
            my_cjson_join_string_function(pdata,(uint8_t*)"types",(uint8_t*)"update_mac",1);
            if(result == 1)
                my_cjson_join_string_function(pdata,(uint8_t*)"result",(uint8_t*)"success",1);
            else
                my_cjson_join_string_function(pdata,(uint8_t*)"result",(uint8_t*)"fault",1);        
            break;
        case IP_ACK:        
            my_cjson_join_string_function(pdata,(uint8_t*)"types",(uint8_t*)"update_ip",1);                
            if(result == 1)
                my_cjson_join_string_function(pdata,(uint8_t*)"result",(uint8_t*)"success",1);
            else
                my_cjson_join_string_function(pdata,(uint8_t*)"result",(uint8_t*)"fault",1);        
            break;
        case SERVER_ACK:        
            my_cjson_join_string_function(pdata,(uint8_t*)"types",(uint8_t*)"update_server",1);                
            if(result == 1)
                my_cjson_join_string_function(pdata,(uint8_t*)"result",(uint8_t*)"success",1);
            else
                my_cjson_join_string_function(pdata,(uint8_t*)"result",(uint8_t*)"fault",1);        
            break;
        case NAMEPWD_ACK:        
            my_cjson_join_string_function(pdata,(uint8_t*)"types",(uint8_t*)"update_np",1);                
            if(result == 1)
                my_cjson_join_string_function(pdata,(uint8_t*)"result",(uint8_t*)"success",1);
            else
                my_cjson_join_string_function(pdata,(uint8_t*)"result",(uint8_t*)"fault",1);        
            break;
        default: break;
    }
    my_cjson_join_string_function(pdata,(uint8_t*)"tnam",(uint8_t*)"FNVIMT_JY",1);    
    /* 终端序列号 */
    memset(temp,0,sizeof(temp));
    start_get_device_id_str((uint8_t*)temp);
    my_cjson_join_string_function(pdata,(uint8_t*)"mcu",(uint8_t*)temp,1);    
    my_cjson_join_string_function(pdata,(uint8_t*)"mod",(uint8_t*)HARD_NO_STR,1);    
    my_cjson_join_string_function(pdata,(uint8_t*)"sv",(uint8_t*)SOFT_NO_STR,1);    
    
    /* 网卡MAC地址 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",local->mac[0],local->mac[1],local->mac[2],
                                                                                             local->mac[3],local->mac[4],local->mac[5]);
    my_cjson_join_string_function(pdata,(uint8_t*)"mac",(uint8_t*)temp,1);
    
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

    /* 主网检测IP1 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d.%d.%d.%d",local->ping_ip[0],local->ping_ip[1],local->ping_ip[2],local->ping_ip[3]);
    my_cjson_join_string_function(pdata,(uint8_t*)"mip1",(uint8_t*)temp,1);
//        /* 主网检测IP2 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d.%d.%d.%d",local->ping_sub_ip[0],local->ping_sub_ip[1],local->ping_sub_ip[2],local->ping_sub_ip[3]);
    my_cjson_join_string_function(pdata,(uint8_t*)"mip2",(uint8_t*)temp,1);
    
    /* 摄像机IP */
    for(uint8_t i=0;i<6;i++)
    {
        memset(str,0,sizeof(str));
        memset(temp,0,sizeof(temp));
        sprintf(str,"c%d",i+1);
        sprintf(temp,"%d.%d.%d.%d",ipc->ip[i][0],ipc->ip[i][1],\
                                                             ipc->ip[i][2],ipc->ip[i][3]);
        my_cjson_join_string_function(pdata,(uint8_t*)str,(uint8_t*)temp,1);
    }
    
    /* 内外服务器IP、域名+端口 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%s:%d",remote->inside_iporname,remote->inside_port);
    my_cjson_join_string_function(pdata,(uint8_t*)"inip",(uint8_t*)temp,1);
    /* 外网服务器IP、域名+端口 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%s:%d",remote->outside_iporname,remote->outside_port);
    my_cjson_join_string_function(pdata,(uint8_t*)"outip",(uint8_t*)temp,1);
    
    /* SIM卡序列号 */
    my_cjson_join_string_function(pdata,(uint8_t*)"iccid",(uint8_t*)gsm_get_sim_ccid_function(),1);
    
        /* 用户名 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%s",device->name);
    my_cjson_join_string_function(pdata,(uint8_t*)"name",(uint8_t*)temp,1);
    
        /* 密码 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%s",device->password);
    my_cjson_join_string_function(pdata,(uint8_t*)"password",(uint8_t*)temp,1);
    
    my_cjson_data_create_function(pdata,1); // 结束
    my_cjson_create_function(pdata,1); // 结束
    
    *len = strlen((char*)pdata);
    
}

/*
*********************************************************************************************************
*    函 数 名: print_inquiry_ack_function
*    功能说明: 查询配置
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_inquiry_ack_function(uint8_t *pdata, uint16_t *len)
{
    struct device_param     *device = app_get_device_param_function();
    char   temp[50]     = {0};
    uint16_t    buff[4] = {0};
    fp32            data_temp         = 0;
    
    my_cjson_create_function(pdata,0); // 开始
    my_cjson_info_create_function(pdata,0); // 开始    
    /* 设备唯一标识TID */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%X",device->id.i);
    my_cjson_join_string_function(pdata,(uint8_t*)"tid",(uint8_t*)temp,1);

    /** 系统时间 **/
    my_cjson_join_string_function(pdata,(uint8_t*)"date",app_get_report_current_time(2),0);
    my_cjson_info_create_function(pdata,1); // 结束
        
    my_cjson_data_create_function(pdata,0); // 开始
    my_cjson_join_string_function(pdata,(uint8_t*)"types",(uint8_t*)"inquiry",1);
    
    /** 电压、电流 **/
    data_temp = det_get_total_energy_handler(0);
    buff[0] = (uint16_t)data_temp;
    buff[1] = data_temp*100-buff[0]*100;
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d.%02d",buff[0],buff[1]);    
    my_cjson_join_string_function(pdata,(uint8_t*)"volt",(uint8_t*)temp,1);
        
    data_temp = det_get_total_energy_handler(1);
    buff[2] = (uint16_t)data_temp;
    buff[3] = data_temp*100-buff[2]*100;
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d.%02d",buff[2],buff[3]);
    my_cjson_join_string_function(pdata,(uint8_t*)"cur",(uint8_t*)temp,1);
    
    /** 湿度、温度 **/
    memset(temp,0,sizeof(temp));
    data_temp = det_get_inside_humi();
    buff[0] = (uint16_t)data_temp;
    buff[1] = data_temp*100-buff[0]*100;
    sprintf(temp,"%d.%02d",buff[0],buff[1]);
    my_cjson_join_string_function(pdata,(uint8_t*)"humi",(uint8_t*)temp,1);    
        
    memset(temp,0,sizeof(temp));
    data_temp = det_get_inside_temp();
    if(data_temp < 0) {
        data_temp = 0 - data_temp;
        buff[2] = (uint16_t)data_temp;
        buff[3] = data_temp*100-buff[2]*100;
        sprintf(temp,"-%d.%02d",buff[2],buff[3]);
    } 
    else 
    {
        buff[2] = (uint16_t)data_temp;
        buff[3] = data_temp*100-buff[2]*100;
        sprintf(temp,"%d.%02d",buff[2],buff[3]);
    }    
    my_cjson_join_string_function(pdata,(uint8_t*)"temp",(uint8_t*)temp,1);    

    /* 门状态 */


    /* 箱体姿态 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",det_get_cabinet_posture());
    my_cjson_join_string_function(pdata,(uint8_t*)"angle",(uint8_t*)temp,1);    
    
    /* 无线状态 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%d",gsm_gst_init_status_function(0));
    my_cjson_join_string_function(pdata,(uint8_t*)"csq",(uint8_t*)temp,1);    

    memset(temp,0,sizeof(temp));
    sprintf(temp,"%01d",gsm_gst_init_status_function(1));
    my_cjson_join_string_function(pdata,(uint8_t*)"wire",(uint8_t*)temp,1);    

    /* SIM卡序列号 */
    my_cjson_join_string_function(pdata,(uint8_t*)"ccid",(uint8_t*)gsm_get_sim_ccid_function(),1);    

    /* 网络状态 */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%01d",com_report_get_main_network_status(0));    
    my_cjson_join_string_function(pdata,(uint8_t*)"lan",(uint8_t*)temp,1);        
    
    /* 继电器 */


    /* 故障码 */
    my_cjson_join_string_function(pdata,(uint8_t*)"error",(uint8_t*)"100",0);    

    my_cjson_data_create_function(pdata,1); // 结束
    my_cjson_create_function(pdata,1); // 结束
    
    *len = strlen((char*)pdata);
    
}

/*
*********************************************************************************************************
*    函 数 名: com_deal_configure_mac
*    功能说明: 配置设备mac
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_tcp_configure_mac(void)
{
    struct local_ip_t     *local  = app_get_local_network_function();
    struct device_param *device = app_get_device_param_function();
    
    device->id.i = print_param.id;
    local->mac[0] = print_param.mac[0];
    local->mac[1] = print_param.mac[1];
    local->mac[2] = print_param.mac[2];
    local->mac[3] = print_param.mac[3];
    local->mac[4] = print_param.mac[4];
    local->mac[5] = print_param.mac[5]; 
    /* 设置回传 */
    print_reply_parameters_function(MAC_ACK,1);
    
    /* 保存 */
//    STMFLASH_Write(DEVICE_ID_ADDR,(uint32_t*)device->id.c,4);    
//    STMFLASH_Write(DEVICE_MAC_ADDR, (uint32_t *)&local->mac, sizeof(local->mac) / 4);

    bsp_WriteCpuFlash_Save(DEVICE_FLASH_STORE,DEVICE_ID_ADDR,(uint8_t*)device->id.c,4);
    bsp_WriteCpuFlash_Save(DEVICE_FLASH_STORE,DEVICE_MAC_ADDR,(uint8_t *)&local->mac,6);    
    
//    STMFLASH_Write(DEVICE_ID_ADDR,(uint32_t*)device->id.c,1);
//    STMFLASH_Write(DEVICE_MAC_ADDR,(uint32_t *)&local->mac,2);    
    
    app_set_save_infor_function(SAVE_DEVICE_PARAM);
    app_set_save_infor_function(SAVE_LOCAL_NETWORK);
    vTaskDelay(100);
//    app_system_softreset();
}

/*
*********************************************************************************************************
*    函 数 名: printf_tcp_configure_rlelay
*    功能说明: 配置继电器
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_tcp_configure_rlelay(void)
{
    for(uint8_t i=0;i<RELAY_ALL;i++)
    {
        switch(print_param.adapter[i])
        {
            case 1:
                relay_control((RELAY_DEV)i,RELAY_ON); // 继电器开
                print_reply_parameters_function(RELAY_ACK,1);
            break;
        
            case 2:
                relay_control((RELAY_DEV)i,RELAY_OFF); // 继电器关
                print_reply_parameters_function(RELAY_ACK,1);
            break;

            default:
//                print_reply_parameters_function(RELAY_ACK,0);  // 返回错误信息
            break;
        }    
    }
    
}
/*
*********************************************************************************************************
*    函 数 名: print_relay_ack_function
*    功能说明: 继电器回复数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_relay_ack_function(uint8_t *pdata, uint16_t *len, uint8_t result)
{
    struct device_param     *device = app_get_device_param_function();
    char  temp[10]     = {0};
    
    my_cjson_create_function(pdata,0); // 开始
    my_cjson_info_create_function(pdata,0); // 开始    
    /* 设备唯一标识TID */
    memset(temp,0,sizeof(temp));
    sprintf(temp,"%X",device->id.i);
    my_cjson_join_string_function(pdata,(uint8_t*)"tid",(uint8_t*)temp,1);

    /** 系统时间 **/
    my_cjson_join_string_function(pdata,(uint8_t*)"date",app_get_report_current_time(2),0);
    my_cjson_info_create_function(pdata,1); // 结束
    
    my_cjson_data_create_function(pdata,0); // 开始
    
    my_cjson_join_string_function(pdata,(uint8_t*)"types",(uint8_t*)"update_relay",1);

    if(result == 1)
        my_cjson_join_string_function(pdata,(uint8_t*)"result",(uint8_t*)"success",1);
    else
        my_cjson_join_string_function(pdata,(uint8_t*)"result",(uint8_t*)"fault",1);        
            
    /* 继电器 */
        
    my_cjson_data_create_function(pdata,1); // 结束
    my_cjson_create_function(pdata,1); // 结束
    
    *len = strlen((char*)pdata);
}
/*
*********************************************************************************************************
*    函 数 名: printf_tcp_configure_local_network
*    功能说明: 配置设备IP、子网掩码、网关
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_configure_local_network(void)
{
    struct local_ip_t *local = app_get_local_network_function();
    uint8_t ip[8];
    int8_t  error[IP_ALL] = {0};
    int8_t  ret ;
    
    local->ip[0] = print_param.net_ip[IP][0];
    local->ip[1] = print_param.net_ip[IP][1];
    local->ip[2] = print_param.net_ip[IP][2];
    local->ip[3] = print_param.net_ip[IP][3];
    error[IP] = 0;
    
    local->netmask[0] = print_param.net_ip[NM][0];
    local->netmask[1] = print_param.net_ip[NM][1];
    local->netmask[2] = print_param.net_ip[NM][2];
    local->netmask[3] = print_param.net_ip[NM][3];
    error[NM] = 0;

    local->gateway[0] = print_param.net_ip[GW][0];
    local->gateway[1] = print_param.net_ip[GW][1];
    local->gateway[2] = print_param.net_ip[GW][2];
    local->gateway[3] = print_param.net_ip[GW][3];
    error[GW] = 0;
    
    ip[0] = print_param.net_ip[MIP1][0];
    ip[1] = print_param.net_ip[MIP1][1];
    ip[2] = print_param.net_ip[MIP1][2];
    ip[3] = print_param.net_ip[MIP1][3];
    error[MIP1] = 0;
    
    ip[4] = print_param.net_ip[MIP2][0];
    ip[5] = print_param.net_ip[MIP2][1];
    ip[6] = print_param.net_ip[MIP2][2];
    ip[7] = print_param.net_ip[MIP2][3];
    error[MIP2] = 0;
    
    app_set_main_network_ping_ip(ip);
    
    for(uint8_t i=0;i<6;i++)
    {
        error[i+CAM1] = print_configure_camera_ip(i);
    }
    
    for(uint8_t i=0;i < IP_ALL;i++)
    {
        if(error[i] < 0)
            ret = -1;
        else
            ret = 0;
    }
    
    if(ret == 0)
        print_reply_parameters_function(IP_ACK,1);  /* 设置回传 */    
    else
        print_reply_parameters_function(IP_ACK,0);  // 返回错误信息
    
    /* 保存 */
//    app_set_save_infor_function(SAVE_LOCAL_NETWORK);
    eth_set_network_reset();
    
}


/*
*********************************************************************************************************
*    函 数 名: printf_tcp_configure_server_network
*    功能说明: 配置服务器地址、端口
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void printf_tcp_configure_server_network(void)
{
    struct remote_ip *remote     = app_get_remote_network_function();
//    sys_backups_t    *param_back = app_get_backups_function();
//    struct only_send_ip *sendip = app_get_only_send_ip();
//    int8_t ret = 0;
//    char *p1 = NULL;
//    char *p2 = NULL;
//    int32_t temp[4] = {0};
//    uint8_t ip[4] = {0};
//    uint32_t port = 0;
//    uint8_t mode  = 0;
    
    /* 辨析命令 */

        memset(remote->inside_iporname,0,sizeof(remote->inside_iporname));
        sscanf((char*)print_param.inside,"%[^:]:%d",remote->inside_iporname,&remote->inside_port);

    
        memset(remote->outside_iporname,0,sizeof(remote->outside_iporname));
        sscanf((char*)print_param.outside,"%[^:]:%d",remote->outside_iporname,&remote->outside_port);

    
    print_reply_parameters_function(SERVER_ACK,1);
      app_set_save_infor_function(SAVE_REMOTE_IP);
    

    }

    /************************************************************
*    ? ? ?: printf_tcp_configure_pwd_name
*    ????: 修改用户名、密码
*    ?    ?: * Return        : 
*    
************************************************************/
void printf_tcp_configure_pwd_name(void)
{
    static struct device_param param = {0};
  
    memcpy(param.password,print_param.password,sizeof(print_param.password));
//  memcpy(param.name,print_param.username,sizeof(print_param.username));
    
    app_set_code_function( param);
    
  print_reply_parameters_function(NAMEPWD_ACK,1);

}

/*
*********************************************************************************************************
*    函 数 名: printf_tcp_configure_camera
*    功能说明: 处理配置摄像头信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t print_configure_camera_ip(uint8_t id)
{
    uint8_t ip[4] = {0};
    int8_t  ret   = 0;
    
    ip[0] = print_param.net_ip[id+CAM1][0];
    ip[1] = print_param.net_ip[id+CAM1][1];
    ip[2] = print_param.net_ip[id+CAM1][2];
    ip[3] = print_param.net_ip[id+CAM1][3];

    if((ip[0] == 0) && (ip[1] == 0) && (ip[2] == 0) && (ip[3] == 0))
    {
        app_set_camera_num_function(ip,id);        /* 清除指定IP */
        ret = 0;
    }
    else
    {
        ret = app_match_local_camera_ip(ip);
        if(ret != 0)
        {
            ret = -1;
        }
        else
        {
            app_set_camera_num_function(ip,id);
            ret = 0;
        }
    }
    return ret;
}


/*
*********************************************************************************************************
*    函 数 名: print_configure_run_addr
*    功能说明: 处理程序运行地址
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_configure_run_addr(void)
{

}

/*
*********************************************************************************************************
*    函 数 名: print_report_time_function
*    功能说明: 通信计时函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void print_report_time_function(void)
{
    /* 正常上报 */
    print_status.send.report_time++;
    if(print_status.send.report_time > SEND_TIME_MAX)
    {
        print_status.send.report_time = 0;
        
        print_status.send_flag.inquiry = 1;/* 进行一次上报 */
    }
}



