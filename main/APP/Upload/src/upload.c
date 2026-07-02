#include "./Upload/inc/upload.h"
#include "main.h"

upload_param_t sg_uploadparam_t =
{
    .ip = {47, 104, 250, 225},
    .host = "47.104.250.225",
    .port = 8080U,
};

/*
*********************************************************************************************************
*    函 数 名: upload_set_server_config
*    功能说明: 设置上传服务器 IP/域名、端口与 URL。
*    形    参: ip / host / port / url
*    返 回 值: 无。
*********************************************************************************************************
*/
void upload_set_server_config(const uint8_t *ip, const char *host, uint16_t port, const char *url)
{
    if (ip != NULL)
    {
        memcpy(sg_uploadparam_t.ip, ip, 4);
    }

    if ((host != NULL) && (host[0] != '\0'))
    {
        snprintf(sg_uploadparam_t.host, sizeof(sg_uploadparam_t.host), "%s", host);
    }

    if (port != 0U)
    {
        sg_uploadparam_t.port = port;
    }

    if ((url != NULL) && (url[0] != '\0'))
    {
        snprintf(sg_uploadparam_t.url, sizeof(sg_uploadparam_t.url), "%s", url);
    }
}

/*
*********************************************************************************************************
*    函 数 名: upload_set_upload_mode
*    功能说明: 设置当前上传模式(有线/无线/空闲)。
*    形    参: mode 上传模式
*    返 回 值: 无。
*********************************************************************************************************
*/
void upload_set_upload_mode(uint8_t mode)
{
    sg_uploadparam_t.mode = mode;
}

/*
*********************************************************************************************************
*    函 数 名: upload_get_mode_function
*    功能说明: 获取当前上传模式。
*    形    参: 无
*    返 回 值: 上传模式。
*********************************************************************************************************
*/
uint8_t upload_get_mode_function(void)
{
    return sg_uploadparam_t.mode;
}
