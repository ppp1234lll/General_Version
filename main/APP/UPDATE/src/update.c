#include "main.h"
#include "./UPDATE/inc/update.h"

update_param_t sg_updateparam_t =
{
    .ip={47, 104, 98, 214}, // 47.104.98.214:8989
    .port = 8989
};
////

/*
*********************************************************************************************************
*    函 数 名: update_status_detection
*    功能说明: 更新状态检测
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void update_status_detection(void)
{
    struct BOOT_UPDATE_PARAM boot_update_param = {0};
    update_read_boot_param(&boot_update_param);
    
    switch(boot_update_param.update_status)
    {
        case UPDATE_SUCCESS:
            app_set_reply_parameters_function(CONFIGURE_UPDATE_SYSTEM,0x01);  // 更新成功
            http_update_clear_param();
            break;
        case UPDATE_FAILED:
            app_set_reply_parameters_function(CONFIGURE_UPDATE_SYSTEM,0x00);  // 更新失败
            http_update_clear_param();
            break;
        default:
            break;
    }
}

/*
*********************************************************************************************************
*    函 数 名: update_set_update_mode
*    功能说明: 设置更新方式
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void update_set_update_mode(uint8_t mode)
{
    sg_updateparam_t.mode = mode;
}

/*
*********************************************************************************************************
*    函 数 名: update_detection_status_function
*    功能说明: 监测更新方式
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t update_get_mode_function(void)
{
    return sg_updateparam_t.mode;
}

/*
*********************************************************************************************************
*    函 数 名: update_addr_ip
*    功能说明: 获取更新地址端口ip
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t *update_addr_ip(void)
{
    static uint8_t ip[20] = {0};
    
    sprintf((char*)ip,"%d.%d.%d.%d",sg_updateparam_t.ip[0],sg_updateparam_t.ip[1],sg_updateparam_t.ip[2],sg_updateparam_t.ip[3]);
    
    return ip;
}

/*
*********************************************************************************************************
*    函 数 名: update_addr_port
*    功能说明: 获取更新地址端口
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint32_t update_addr_port(void)
{
    return sg_updateparam_t.port;
}
/*
*********************************************************************************************************
*    函 数 名: update_get_infor_data_function
*    功能说明: 获取更新数据信息
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *update_get_infor_data_function(void)
{
    return &sg_updateparam_t;
}

/*
*********************************************************************************************************
*    函 数 名: update_read_boot_param
*    功能说明: 读取更新信息
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void update_read_boot_param(struct BOOT_UPDATE_PARAM *boot_update_param)
{
    sf_ReadBuffer((uint8_t*)boot_update_param, UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM));
}

/*
*********************************************************************************************************
*    函 数 名: update_write_boot_param
*    功能说明: 保存升级参数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void update_write_boot_param(struct BOOT_UPDATE_PARAM *boot_update_param)
{
    if(sf_WriteBuffer((uint8_t *)boot_update_param, UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM)) == 0)
    {
        printf("写串行Flash出错！\r\n");
    }
}
//////////////
