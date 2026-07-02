#include "main.h"
#include "./UPDATE/inc/update.h"
#include "./UPDATE/inc/update_http.h"
#include <stdbool.h>

/*
*********************************************************************************************************
*    文 件 名: update_lwip.c
*    功能说明: 有线(lwIP) OTA 升级 HTTP 传输与 FreeRTOS 后台任务
*    说    明: HTTP 应答解析等公共逻辑见 update_http.c,本文件负责 TCP 连接/收发
*********************************************************************************************************
*/

/* ======================== 有线 OTA HTTP ======================== */

struct netconn *tcp_update;

static int http_update_connect_server_by_lwip(ip_addr_t *ip, unsigned short port);
static int http_update_send_request_for_info_txt_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port);
static int http_update_recv_reponse_by_lwip(int *out_recv_size);
static int http_update_send_request_for_crcbin_file_size_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port);
static int http_update_send_request_for_crcbin_data_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port);
static void http_update_cb_server_ip(const char *name, const ip_addr_t *ipaddr, void *arg);

/*
*********************************************************************************************************
*    函 数 名: http_update_get_info_txt_by_lwip
*    功能说明: 通过 lwIP 获取 info.txt,校验版本并提取固件下载 URL
*    形    参: server_ipaddr 升级服务器 IP; server_port 端口
*    返 回 值: 1-需更新 2-版本相同 <0-出错(-1连接 -2发送 -3/-4接收 -5版本 -6URL)
*********************************************************************************************************
*/
int http_update_get_info_txt_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port)
{
    int ret = 0, res;
    int cur_recv_size = 0;
    bool be_timing = false;
    unsigned int begin_ticks = 0, end_ticks = 0;
    ////

    // 连接服务器
    printf("\n有线连接服务器 %s:%d ...\n", ipaddr_ntoa(server_ipaddr), server_port);
    ret = http_update_connect_server_by_lwip(server_ipaddr, server_port);
    if(ret){ return(-1); }
    led_control_function(LD_LAN, LD_FLICKER);

    // 发送http请求
    ret = http_update_send_request_for_info_txt_by_lwip(server_ipaddr, server_port);
    if(ret)
    {
        http_update_close_connect_by_lwip();
        return(-2);
    }

    // 接收完整的http应答数据
    sg_http_update_param.http_response_recv_size = 0;
    if(sg_http_update_param.http_response_buff){ sg_http_update_param.http_response_buff[0] = 0; }
    while(true)
    {
        // 接收数据
        ret = http_update_recv_reponse_by_lwip(&cur_recv_size);
        //printf("\n接收数据: %d 字节\n", cur_recv_size);
        if(ret)
        {
            http_update_close_connect_by_lwip();
            return(-3);
        }

        // 暂时无数据
        if(!cur_recv_size)
        {
            if(!be_timing) // 非计时状态
            {
                be_timing = true; // 开始计时
                begin_ticks = HAL_GetTick();
            }
            else // 计时状态
            {
                end_ticks = HAL_GetTick();
                if( (end_ticks - begin_ticks) >= (10 * configTICK_RATE_HZ) ) // 超时10秒
                {
                    //printf("\nhttp更新,无数据接收超时....\n");
                    http_update_close_connect_by_lwip();
                    return(-4);
                }
            }
            vTaskDelay(10);
            continue;
        }
        else{ be_timing = false; } // 停止计时

        // 判断http应答完整性
        ret = http_update_check_response_completed();
        if(ret != 2){ /*OSTimeDlyHMSM(0,0,0,10);*/ continue; }
        else
        {
            //printf("\nhttp应答:\n%s\n", (char *)(sg_http_update_param.http_response_buff));
            break;
        }
    } //while()
    ////

    // 先关闭连接
    http_update_close_connect_by_lwip();
    led_control_function(LD_LAN, LD_OFF);

    // 判断版本
    ret = http_update_chack_version();
    if(ret < 0){ return(-5); }

    // 提取url
    if(ret == 1) // 需要更新
    {
        res = http_update_get_url();
        if(res){ return(-6); }
    }

    return(ret);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_connect_server_by_lwip
*    功能说明: 通过 lwIP TCP 连接 OTA 服务器,最多重试 3 次
*    形    参: ip 服务器 IP; port 端口
*    返 回 值: 0-成功 -1-失败
*********************************************************************************************************
*/
static int http_update_connect_server_by_lwip(ip_addr_t *ip, unsigned short port)
{
    unsigned char index = 0;
    err_t err;
    update_param_t *updateparam = NULL;
    ////

    updateparam = update_get_infor_data_function();
    for(index=0; index<3; index++)
    {
        tcp_update = netconn_new(NETCONN_TCP);
        if( tcp_update == NULL ) { continue; }

        err = netconn_connect(tcp_update, ip, port);
        if(err != ERR_OK)
        {
            netconn_delete(tcp_update); tcp_update = NULL;
            continue;
        }
        else
        {
            updateparam->tcp_t.connect = 1;
            tcp_update->recv_timeout = 10;
            updateparam->tcp_t.state = 2;
            return(0);
        }
    } //for()

    /* tcp连接失败 */
    eth_set_network_reset();

    return(-1);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_send_request_for_info_txt_by_lwip
*    功能说明: 发送 GET 请求获取 info.txt
*    形    参: server_ipaddr 服务器 IP; server_port 端口
*    返 回 值: netconn_write 返回值(ERR_OK 为成功)
*********************************************************************************************************
*/
static int http_update_send_request_for_info_txt_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port)
{
    char send_buf[256]={0};
    char *append_pt = send_buf;
    int ret = 0;
    ////

    sprintf(append_pt, "GET /%s/info.txt HTTP/1.1\r\n", HARD_NO_STR);
    append_pt += strlen(append_pt);
    sprintf(append_pt, "Host: %s:%d\r\n\r\n", ipaddr_ntoa(server_ipaddr), server_port);
    append_pt += strlen(append_pt);

    //printf("\nhttp请求:\n%s\n", send_buf);
    ret = netconn_write(tcp_update, send_buf, (append_pt - send_buf), NETCONN_COPY);
    return(ret);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_close_connect_by_lwip
*    功能说明: 关闭 lwIP TCP 连接并清除连接状态
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void http_update_close_connect_by_lwip(void)
{
    update_param_t *updateparam = NULL;
    ////

    if(tcp_update)
    {
        netconn_close(tcp_update);
        netconn_delete(tcp_update);
        tcp_update = NULL;
    }

    updateparam = update_get_infor_data_function();
    updateparam->tcp_t.connect = 0;
    updateparam->tcp_t.state = 1;
}

/*
*********************************************************************************************************
*    函 数 名: http_update_recv_reponse_by_lwip
*    功能说明: 从 lwIP TCP 连接读取一次数据并追加到 HTTP 应答缓冲
*    形    参: out_recv_size 输出本次读取字节数(可为 NULL)
*    返 回 值: 0-成功 -1-未连接 -2-内容超大 -3-连接断开
*********************************************************************************************************
*/
static int http_update_recv_reponse_by_lwip(int *out_recv_size)
{
    err_t recv_err = 0;
    struct netbuf *recvbuf = NULL;

    struct pbuf *q = NULL;
    int ret = 0, recv_size = 0;
    ////

    if(out_recv_size){ (*out_recv_size) = 0; }
    if(!tcp_update){ return(-1); }

    recv_err = netconn_recv(tcp_update, &recvbuf);
    switch(recv_err)
    {
        case ERR_OK: // 接收到数据
            taskENTER_CRITICAL();
            {
                for(q = recvbuf->p; q != NULL; q = q->next)  //遍历完整个pbuf链表
                {
                    // 保存到 http 应答buuf 中
                    ret = http_update_save_response( (unsigned char *)(q->payload), q->len );
                    if(ret){ break; }

                    recv_size += q->len;
                } // for()
            }
            taskEXIT_CRITICAL();            /* 退出临界区 */

            netbuf_delete(recvbuf); recvbuf = NULL;
            if(ret){ return(-2); } // 应该是缓冲容纳不了了

            if(out_recv_size){ (*out_recv_size) = recv_size; }
        return(0);
        ////

        case ERR_TIMEOUT: // 暂无数据
            if(recvbuf){ netbuf_delete(recvbuf); recvbuf = NULL; }
            //OSTimeDlyHMSM(0,0,0,10);
        return(0);
        ////

        case ERR_CLSD: // 对端已经关闭
        default:
            if(recvbuf){ netbuf_delete(recvbuf); recvbuf = NULL; }
        return(-3);
    } // switch()
}

/*
*********************************************************************************************************
*    函 数 名: http_update_get_crc_bin_file_size_by_lwip
*    功能说明: 通过 lwIP 发送 HEAD 请求获取 crc_bin 文件大小
*    形    参: 无(使用 sg_http_update_param 中的 host/port/url)
*    返 回 值: 0-成功 <0-出错(-1 DNS -2 IP -3连接 -4发送 -5/-6接收 -7解析)
*********************************************************************************************************
*/
int http_update_get_crc_bin_file_size_by_lwip(void)
{
    int ret = 0;
    int cur_recv_size = 0;
    bool be_timing = false;
    unsigned int begin_ticks = 0, end_ticks = 0;
    ip_addr_t server_addr = {0};
    ////

    // dns
    if( (sg_http_update_param.http_host[0] < '0') || (sg_http_update_param.http_host[0] > '9') )
    {
        ret = dns_gethostbyname(sg_http_update_param.http_host, &server_addr, &http_update_cb_server_ip, (void *)(&server_addr));
        if(ret != ERR_OK){ return(-1); }
    }
    else
    {
        ret = ipaddr_aton(sg_http_update_param.http_host, &server_addr);
        if(ret != 1){ return(-2); }
    }
    memcpy( &(sg_http_update_param.http_server_addr),  &server_addr, sizeof(ip_addr_t) );

    // 连接服务器
    printf("\n有线连接服务器 %s:%d ...\n", ipaddr_ntoa(&(sg_http_update_param.http_server_addr)), sg_http_update_param.http_port);
    ret = http_update_connect_server_by_lwip( &(sg_http_update_param.http_server_addr), sg_http_update_param.http_port );
    if(ret){ return(-3); }
    led_control_function(LD_LAN, LD_FLICKER);

    // 发送http请求(HEAD请求)
    ret = http_update_send_request_for_crcbin_file_size_by_lwip( &(sg_http_update_param.http_server_addr), sg_http_update_param.http_port );
    if(ret != ERR_OK)
    {
        http_update_close_connect_by_lwip();
        return(-4);
    }

    // 接收完整的http应答数据
    sg_http_update_param.http_response_recv_size = 0;
    if(sg_http_update_param.http_response_buff){ sg_http_update_param.http_response_buff[0] = 0; }
    while(true)
    {
        // 接收数据
        ret = http_update_recv_reponse_by_lwip(&cur_recv_size);
        if(ret)
        {
            http_update_close_connect_by_lwip();
            return(-5);
        }

        // 暂时无数据
        if(!cur_recv_size)
        {
            if(!be_timing) // 非计时状态
            {
                be_timing = true; // 开始计时
                begin_ticks = HAL_GetTick();
            }
            else // 计时状态
            {
                end_ticks = HAL_GetTick();
                if( (end_ticks - begin_ticks) >= (10 * configTICK_RATE_HZ) ) // 超时10秒
                {
                    printf("\nhttp更新,无数据接收超时....\n");
                    http_update_close_connect_by_lwip();
                    return(-6);
                }
            }
            vTaskDelay(10);
            continue;
        }
        else{ be_timing = false; } // 停止计时

        // 判断http应答完整性
        ret = http_update_check_response_completed();
        if(ret == 0){ vTaskDelay(10); continue; } // 只接收http头
        else
        {
            //printf("\nhttp应答:\n%s\n", (char *)(sg_http_update_param.http_response_buff));
            break;
        }
    } //while()
    ////

    // 先关闭连接
    http_update_close_connect_by_lwip();
    led_control_function(LD_LAN, LD_OFF);

    // 获得 crc_bin 文件的大小
    ret = http_update_get_crc_bin_size(NULL);
    if(ret < 0){ return(-7); }

    return(0);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_send_request_for_crcbin_file_size_by_lwip
*    功能说明: 发送 HEAD 请求查询 crc_bin 文件 Content-Length
*    形    参: server_ipaddr 服务器 IP; server_port 端口
*    返 回 值: netconn_write 返回值(ERR_OK 为成功)
*********************************************************************************************************
*/
static int http_update_send_request_for_crcbin_file_size_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port)
{
    char send_buf[256]={0};
    char *append_pt = send_buf;
    int ret = 0;
    ////

    sprintf(append_pt, "HEAD %s HTTP/1.1\r\n", sg_http_update_param.http_url); append_pt += strlen(append_pt);
    sprintf(append_pt, "Host: %s:%d\r\n\r\n", ipaddr_ntoa(server_ipaddr), server_port); append_pt += strlen(append_pt); // 填写IP地址(最好不要填写域名 )

    //printf("\nhttp请求:\n%s\n", send_buf);
    ret = netconn_write(tcp_update, send_buf, (append_pt - send_buf), NETCONN_COPY);
    return(ret);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_cb_server_ip
*    功能说明: DNS 解析完成回调,写入解析结果
*    形    参: name 域名; ipaddr 解析 IP; arg 输出缓冲指针
*    返 回 值: 无
*********************************************************************************************************
*/
static void http_update_cb_server_ip(const char *name, const ip_addr_t *ipaddr, void *arg)
{
    struct ip_addr *out_addr = (struct ip_addr *)arg;
    ////

    if( !ipaddr || !(ipaddr->addr) ){ return; }

    memcpy(out_addr, ipaddr, sizeof(ip_addr_t));
}

/*
*********************************************************************************************************
*    函 数 名: http_update_get_crc_bin_file_data_by_lwip
*    功能说明: 通过 lwIP 分块下载 crc_bin 固件并写入 SPI Flash
*    形    参: 无(使用 sg_http_update_param 中的分包参数)
*    返 回 值: 0-成功 <0-出错(-1连接 -2发送 -3 CRC -4解析)
*********************************************************************************************************
*/
int http_update_get_crc_bin_file_data_by_lwip(void)
{
    int ret = 0;
    int cur_recv_size = 0;
    bool be_timing = false;
    unsigned int begin_ticks = 0, end_ticks = 0;
    unsigned int crc_check_err_times = 0, connect_times = 0;
    ////

    sg_http_update_param.section_current = 0;

RECONNECT:

    // 连接服务器
    printf("\n有线连接服务器 %s:%d ...\n", ipaddr_ntoa(&(sg_http_update_param.http_server_addr)), sg_http_update_param.http_port);
    ret = http_update_connect_server_by_lwip( &(sg_http_update_param.http_server_addr), sg_http_update_param.http_port );
    if(ret)
    {
        connect_times++; // 连续连接失败的次数
        if(connect_times > 10){ return(-1); }
        goto RECONNECT;
    }
    connect_times = 0;
    led_control_function(LD_LAN, LD_FLICKER);

    // 循环请求、接收数据块
    while(sg_http_update_param.section_current < sg_http_update_param.section_total)
    {
        // 发送http请求(GET请求)
        ret = http_update_send_request_for_crcbin_data_by_lwip( &(sg_http_update_param.http_server_addr), sg_http_update_param.http_port );
        if(ret == ERR_CLSD)
        {
            http_update_close_connect_by_lwip();
            goto RECONNECT;
        }
        else if(ret != ERR_OK)
        {
            http_update_close_connect_by_lwip();
            return(-2);
        }

        // 接收完整的http应答数据
        sg_http_update_param.http_response_recv_size = 0;
        be_timing = false;
        begin_ticks = 0;
        end_ticks = 0;
        while(true)
        {
            // 接收数据
            ret = http_update_recv_reponse_by_lwip(&cur_recv_size);
            if(ret == -3) // 服务器断开,需要重新连接
            {
                http_update_close_connect_by_lwip();
                goto RECONNECT;
            }
            else if(ret) // 其它异常
            {
                http_update_close_connect_by_lwip();
                return(-3);
            }

            // 暂时无数据
            if(!cur_recv_size)
            {
                if(!be_timing) // 非计时状态
                {
                    be_timing = true; // 开始计时
                    begin_ticks = HAL_GetTick();
                }
                else // 计时状态
                {
                    end_ticks = HAL_GetTick();
                    if( (end_ticks - begin_ticks) >= (10 * configTICK_RATE_HZ) ) // 超时10秒
                    {
                        //printf("\nhttp更新,无数据接收超时,重新发起连接 ....\n");
                        http_update_close_connect_by_lwip();
                        goto RECONNECT;
                    }
                }
                vTaskDelay(10);
                continue;
            }

            else{ be_timing = false; } // 停止计时

            // 判断http应答完整性
            ret = http_update_check_response_completed();
            if(ret != 2){ vTaskDelay(10); continue; }
            else
            {
                //printf("\nhttp应答:\n%s\n", (char *)(sg_http_update_param.http_response_buff));
                printf("\n段: %u/%u\n", sg_http_update_param.section_current, sg_http_update_param.section_total);
                break;
            }
        } //while(接收完整的http应答数据)

        // 解析、保存数据
        ret = http_update_parse_crc_bin_data();
        if(ret)
        {
            if(ret == -5) // 偶尔会出现校验错误,此时重新下载即可
            {
                crc_check_err_times++; // 连续校验错误的次数
                if(crc_check_err_times > 10){ return(-3); }
                continue;
            }
            else{ return(-4); }
        }

        crc_check_err_times = 0;
    } // while(循环请求、接收数据块)
    ////

    // 先关闭连接
    http_update_close_connect_by_lwip();
    led_control_function(LD_LAN, LD_OFF);

    return(0);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_send_request_for_crcbin_data_by_lwip
*    功能说明: 发送带 Range 的 GET 请求下载当前固件分块
*    形    参: server_ipaddr 服务器 IP; server_port 端口
*    返 回 值: netconn_write 返回值(ERR_OK 为成功)
*********************************************************************************************************
*/
static int http_update_send_request_for_crcbin_data_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port)
{
    char send_buf[256]={0};
    char *append_pt = send_buf;
    int ret = 0;
    unsigned int download_start = 0, download_end = 0;
    ////

    sprintf(append_pt, "GET %s HTTP/1.1\r\n", sg_http_update_param.http_url); append_pt += strlen(append_pt);
    sprintf(append_pt, "Host: %s:%d\r\n", ipaddr_ntoa(server_ipaddr), server_port); append_pt += strlen(append_pt); // 填写IP地址(最好不要填写域名 )

    download_start = (sg_http_update_param.section_current * UPDATE_CHUNK_SIZE);
    download_end = (download_start + UPDATE_CHUNK_SIZE - 1);
    sprintf(append_pt, "Range: bytes=%d-%d\r\n\r\n", download_start, download_end); append_pt += strlen(append_pt);

    //printf("\nhttp请求:\n%s\n", send_buf);
    ret = netconn_write(tcp_update, send_buf, (append_pt - send_buf), NETCONN_COPY);
    return(ret);
}

/* ======================== 有线 OTA 后台任务 ======================== */

#define UPDATE_LWIP_TASK_PRIO           (7U)
#define UPDATE_LWIP_TASK_STK            (4096U)
static TaskHandle_t s_update_lwip_task = NULL;

/*
*********************************************************************************************************
*    函 数 名: update_lwip_task_done
*    功能说明: OTA 后台任务结束,清除任务句柄
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void update_lwip_task_done(void)
{
    s_update_lwip_task = NULL;
}

/*
*********************************************************************************************************
*    函 数 名: update_lwip_bg_task
*    功能说明: FreeRTOS 有线 OTA 后台任务,执行升级并清理模式后自删除
*    形    参: pvParameters 未使用
*    返 回 值: 无
*********************************************************************************************************
*/
static void update_lwip_bg_task(void *pvParameters)
{
    (void)pvParameters;

    FeedFwdgt();

    (void)update_lwip_task_function();
    if (update_get_mode_function() != UPDATE_MODE_NULL)
    {
        update_set_update_mode(UPDATE_MODE_NULL);
    }
    update_lwip_task_done();
    vTaskDelete(NULL);
}

/*
*********************************************************************************************************
*    函 数 名: update_lwip_task_function
*    功能说明: 有线 OTA 后台任务入口
*    形    参: 无
*    返 回 值: 0 成功,-1 失败
*********************************************************************************************************
*/
int8_t update_lwip_task_function(void)
{
    ip_addr_t server_ipaddr;
    uint16_t  server_port;
    int8_t    ret = 0;
    update_param_t *updateparam = NULL;

    struct update_addr *param = app_get_http_ota_function();

    if (g_lwipdev.tcp_status != LWIP_TCP_NO_CONNECT)
    {
        eth_set_tcp_connect_reset();
        vTaskDelay(200);
    }
    updateparam = update_get_infor_data_function();
    server_port = param->port;
    IP4_ADDR(&server_ipaddr, param->ip[0], param->ip[1], param->ip[2], param->ip[3]);

    sg_http_update_param.section_len = (UPDATE_CHUNK_SIZE - 2);
    sg_http_update_param.http_response_recv_size = 0;

    ret = http_update_get_info_txt_by_lwip(&server_ipaddr, server_port);
    if ((ret < 0) || (ret == 2))
    {
        if (ret < 0)
        {
            printf("\n获得info.txt信息,失败! ret: %d\n", ret);
        }
        else
        {
            printf("\n版本是最新版本,无需更新!\n");
        }
        goto UPDATE_END;
    }

    ret = http_update_get_crc_bin_file_size_by_lwip();
    if (ret < 0)
    {
        printf("\n获得crc_bin文件大小,失败! ret: %d\n", ret);
        goto UPDATE_END;
    }

    ret = http_update_get_crc_bin_file_data_by_lwip();
    if (ret < 0)
    {
        printf("\n获得crc_bin文件内容,失败! ret: %d\n", ret);
        goto UPDATE_END;
    }

    printf("\n升级完成,重启设备...\n");
    http_update_success_reboot();
    ret = 0;

UPDATE_END:
    http_update_close_connect_by_lwip();
    printf("update end\n");
    updateparam->mode = UPDATE_MODE_NULL;
    if (ret < 0)
    {
        http_update_failed();
        return -1;
    }
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: update_lwip_poll
*    功能说明: 有线 OTA 后台任务轮询
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void update_lwip_poll(void)
{
    BaseType_t ret;
    if (update_get_mode_function() != UPDATE_MODE_LWIP)
    {
        return;
    }
    if (s_update_lwip_task != NULL)
    {
        return;
    }

    ret = xTaskCreate(update_lwip_bg_task,
                        "ota_lwip",
                        UPDATE_LWIP_TASK_STK,
                        NULL,
                        UPDATE_LWIP_TASK_PRIO,
                        &s_update_lwip_task);

    if (ret != pdPASS)
    {
        s_update_lwip_task = NULL;
    }
}

/*
*********************************************************************************************************
*    函 数 名: update_lwip_is_running
*    功能说明: 有线 OTA 后台任务是否运行中
*    形    参: 无
*    返 回 值: 1-运行中 0-空闲
*********************************************************************************************************
*/
uint8_t update_lwip_is_running(void)
{
    return (uint8_t)(s_update_lwip_task != NULL);
}

