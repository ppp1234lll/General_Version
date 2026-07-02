#include "main.h"
#include "./UPDATE/inc/update.h"
#include "./UPDATE/inc/update_http.h"
#include <stdbool.h>

/*
*********************************************************************************************************
*    文 件 名: update_gsm.c
*    功能说明: 无线(GPRS) OTA 升级 HTTP 传输与 FreeRTOS 后台任务
*    说    明: HTTP 应答解析等公共逻辑见 update_http.c,本文件负责 GPRS 链路连接/收发
*********************************************************************************************************
*/

/* GPRS HTTP 应答缓冲上限: 206 块 = HTTP头(~512) + UPDATE_CHUNK_SIZE(1026) */
#define HTTP_GPRS_RSP_MAX          (UPDATE_CHUNK_SIZE + 2048)
#define HTTP_GPRS_BODY_WAIT_MS     (5 * configTICK_RATE_HZ)  /* 等待 HTTP 体尾包超时 */

/* ======================== 无线 OTA HTTP ======================== */

static int http_update_connect_server_by_gprs(ip_addr_t *ip, unsigned short port);
static int http_update_connect_server_by_gprs2(const char *host, unsigned short port);
static int http_update_send_request_for_info_txt_by_gprs(ip_addr_t *server_ipaddr, uint16_t server_port);
static int http_update_recv_reponse_by_gprs(int *out_recv_size);
static int http_update_drain_gprs_response(int *out_recv_size);
static void http_update_discard_gprs_ota_pending(void);
static int http_update_send_request_for_crcbin_file_size_by_gprs(const char *host, uint16_t server_port);
static int http_update_send_request_for_crcbin_data_by_gprs(const char *host, uint16_t server_port);

/*
*********************************************************************************************************
*    函 数 名: http_update_get_info_txt_by_gprs
*    功能说明: 通过 GPRS 获取 info.txt,校验版本并提取固件下载 URL
*    形    参: server_ipaddr 升级服务器 IP; server_port 端口
*    返 回 值: 1-需更新 2-版本相同 <0-出错(-1连接 -2发送 -3/-4接收 -5版本 -6URL)
*********************************************************************************************************
*/
int http_update_get_info_txt_by_gprs(ip_addr_t *server_ipaddr, uint16_t server_port)
{
    int ret = 0, res;
    int cur_recv_size = 0;
    bool be_timing = false;
    unsigned int begin_ticks = 0, end_ticks = 0;
    ////

    // 连接服务器
    printf("\n无线连接服务器 %s:%d ...\n", ipaddr_ntoa(server_ipaddr), server_port);
    ret = http_update_connect_server_by_gprs(server_ipaddr, server_port);
    if(ret){ return(-1); }

    led_control_function(LD_GPRS, LD_FLICKER);

    gprs_reset_ota_rx_stream();

    // 发送http请求
    ret = http_update_send_request_for_info_txt_by_gprs(server_ipaddr, server_port);
    if(ret != GPRS_SEND_OK)
    {
        http_update_close_connect_by_gprs();
        return(-2);
    }

    // 接收完整的http应答数据
    sg_http_update_param.http_response_recv_size = 0;
    if(sg_http_update_param.http_response_buff){ sg_http_update_param.http_response_buff[0] = 0; }
    while(true)
    {
        // 接收数据(消化当前已到达的所有 GPRS 分片)
        ret = http_update_drain_gprs_response(&cur_recv_size);
        if(ret == -3)
        {
            int complete = http_update_info_txt_response_ready();
            if(complete == 2){ break; }
            /* 头已齐体未到: disconn 可能早于尾包,继续等待(带超时保护) */
            if(complete == 1)
            {
                if(!be_timing)
                {
                    be_timing = true;
                    begin_ticks = HAL_GetTick();
                }
                else
                {
                    end_ticks = HAL_GetTick();
                    if( (end_ticks - begin_ticks) >= (10 * configTICK_RATE_HZ) )
                    {
                        http_update_close_connect_by_gprs();
                        return(-3);
                    }
                }
                vTaskDelay(10);
                continue;
            }
            http_update_close_connect_by_gprs();
            return(-3);
        }
        else if(ret)
        {
            http_update_close_connect_by_gprs();
            return(-3);
        }

        // 暂时无数据
        if(!cur_recv_size)
        {
            /* HTTP 头已到、体分包未齐: 不走"无数据"10s 超时,继续等尾包 */
            if(http_update_info_txt_response_ready() == 1)
            {
                if(!be_timing)
                {
                    be_timing = true;
                    begin_ticks = HAL_GetTick();
                }
                else
                {
                    end_ticks = HAL_GetTick();
                    if( (end_ticks - begin_ticks) >= (10 * configTICK_RATE_HZ) )
                    {
                        http_update_close_connect_by_gprs();
                        return(-4);
                    }
                }
                vTaskDelay(10);
                continue;
            }

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
                    http_update_close_connect_by_gprs();
                    return(-4);
                }
            }
            vTaskDelay(10);
            continue;
        }
        else{ be_timing = false; } // 停止计时

        // 判断http应答完整性(须含 info.txt 的 version 字段,兼容头/体分包)
        ret = http_update_info_txt_response_ready();
        if(ret != 2){ vTaskDelay(10); continue; }
        else
        {
            //printf("\nhttp应答:\n%s\n", (char *)(sg_http_update_param.http_response_buff));
            break;
        }
    } //while()
    ////

    // 先关闭连接
    http_update_close_connect_by_gprs();
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
*    函 数 名: http_update_connect_server_by_gprs
*    功能说明: 通过 GPRS 以 IP 连接 OTA 服务器,最多重试 3 次
*    形    参: ip 服务器 IP; port 端口
*    返 回 值: 0-成功 -1-失败
*********************************************************************************************************
*/
static int http_update_connect_server_by_gprs(ip_addr_t *ip, unsigned short port)
{
    update_param_t *updateparam = NULL;
    unsigned char index = 0;
    int ret = 0;
    ////

    updateparam = update_get_infor_data_function();
    for(index=0; index<3; index++)
    {
        ret = gprs_network_connect_function(ipaddr_ntoa(ip), port, GPRS_LINK_OTA);
        if(ret == GPRS_SEND_OK)
        {
            updateparam->gprs_t.connect = 1;
            return 0;
        }
        vTaskDelay(1000);
    } // for()

    updateparam->gprs_t.connect = 0;

    return(-1);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_connect_server_by_gprs2
*    功能说明: 通过 GPRS 以主机名/IP 连接 OTA 服务器,最多重试 3 次
*    形    参: host 服务器地址(域名或 IP 字符串); port 端口
*    返 回 值: 0-成功 -1-失败
*********************************************************************************************************
*/
static int http_update_connect_server_by_gprs2(const char *host, unsigned short port)
{
    update_param_t *updateparam = NULL;
    unsigned char index = 0;
    int ret = 0;
    ////

    updateparam = update_get_infor_data_function();
    for(index=0; index<3; index++)
    {
        ret = gprs_network_connect_function(host, port, GPRS_LINK_OTA);
        if(ret == GPRS_SEND_OK)
        {
            updateparam->gprs_t.connect = 1;
            return 0;
        }

        vTaskDelay(50);
    } // for()

    updateparam->gprs_t.connect = 0;

    return(-1);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_send_request_for_info_txt_by_gprs
*    功能说明: 发送 GET 请求获取 info.txt
*    形    参: server_ipaddr 服务器 IP; server_port 端口
*    返 回 值: gprs_send_data 返回值(GPRS_SEND_OK 为成功)
*********************************************************************************************************
*/
static int http_update_send_request_for_info_txt_by_gprs(ip_addr_t *server_ipaddr, uint16_t server_port)
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
    ret = gprs_send_data( (uint8_t *)send_buf, (append_pt - send_buf), 1000, GPRS_LINK_OTA );

    return(ret);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_close_connect_by_gprs
*    功能说明: 断开 GPRS OTA 链路并清除连接状态
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void http_update_close_connect_by_gprs(void)
{
    update_param_t *updateparam = NULL;
    ////

    gprs_network_disconnect_function(GPRS_LINK_OTA);

    updateparam = update_get_infor_data_function();
    updateparam->gprs_t.connect = 0;
}

/*
*********************************************************************************************************
*    函 数 名: http_update_recv_reponse_by_gprs
*    功能说明: 从 GPRS OTA 流读取一次数据并追加到 HTTP 应答缓冲
*    形    参: out_recv_size 输出本次读取字节数(可为 NULL)
*    返 回 值: 0-成功 -2-内容超大 -3-连接断开
*********************************************************************************************************
*/
static int http_update_recv_reponse_by_gprs(int *out_recv_size)
{
    int ret = 0;
    /* 须能一次读出 HTTP头(~300) + 体(1026); 勿用 1024 以免拆包丢字节 */
    static uint8_t recv_buf[HTTP_GPRS_RSP_MAX];
    int recv_data_size = 0;
    ////

    if(out_recv_size){ (*out_recv_size) = 0; }

    ret = gprs_recv_data_ota(recv_buf, (int)sizeof(recv_buf), &recv_data_size);
    if(ret != GPRS_SEND_OK){ return(-3); }

    // 保存数据
    if(recv_data_size == 0){ return(0); }

    if(recv_data_size > (int)sizeof(recv_buf)){ recv_data_size = (int)sizeof(recv_buf); }
    ret = http_update_save_response(recv_buf, recv_data_size);
    if(ret){ return(-2); }

    printf("提取: %d bytes, HTTP累计: %u bytes (头~283 + 体1026 = 1309)\n",
    recv_data_size, sg_http_update_param.http_response_recv_size);

    if(out_recv_size){ (*out_recv_size) = recv_data_size; }

    return(0);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_discard_gprs_ota_pending
*    功能说明: 应答已齐后丢弃 OTA 流中 keep-alive 尾包,避免污染下次接收
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void http_update_discard_gprs_ota_pending(void)
{
    static uint8_t tmp[512];
    int n = 0;
    int guard = 0;

    while(guard++ < 64)
    {
        if(gprs_recv_data_ota(tmp, (int)sizeof(tmp), &n) != GPRS_SEND_OK){ break; }
        if(n <= 0){ break; }
    }
}

/*
*********************************************************************************************************
*    函 数 名: http_update_drain_gprs_response
*    功能说明: 连续读取 GPRS OTA 缓冲直至暂无新数据,拼齐 HTTP 头/体分包
*    形    参: out_recv_size 输出本次累计读取字节数(可为 NULL)
*    返 回 值: 0-成功 -2-内容超大 -3-连接断开(体未齐时可能转为 0 继续等待)
*********************************************************************************************************
*/
static int http_update_drain_gprs_response(int *out_recv_size)
{
    int ret = 0;
    int chunk = 0;
    int total = 0;

    if(out_recv_size){ (*out_recv_size) = 0; }

    for(;;)
    {
        chunk = 0;
        ret = http_update_recv_reponse_by_gprs(&chunk);
        if(ret == -3)
        {
            /* disconn 时若 HTTP 体尚未收齐,不向上抛断开,继续等尾包 */
            if(http_update_check_response_completed() == 1){ ret = 0; }
            break;
        }
        if(ret != 0){ break; }
        if(chunk <= 0){ break; }
        total += chunk;
        if(http_update_check_response_completed() == 2)
        {
            /* 应答已齐,排空 OTA 流中 keep-alive 尾包,避免污染下一次 drain */
            http_update_discard_gprs_ota_pending();
            break;
        }
    }

    if(out_recv_size){ (*out_recv_size) = total; }
    return ret;
}

/*
*********************************************************************************************************
*    函 数 名: http_update_get_crc_bin_file_size_by_gprs
*    功能说明: 通过 GPRS 发送 HEAD 请求获取 crc_bin 文件大小
*    形    参: 无(使用 sg_http_update_param 中的 host/port/url)
*    返 回 值: 0-成功 <0-出错
*********************************************************************************************************
*/
int http_update_get_crc_bin_file_size_by_gprs(void)
{
    int ret = 0;
    int cur_recv_size = 0;
    bool be_timing = false;
    unsigned int begin_ticks = 0, end_ticks = 0;
    ////

    // 连接服务器
    printf("\n无线连接服务器 %s:%d ...\n", sg_http_update_param.http_host, sg_http_update_param.http_port);
    ret = http_update_connect_server_by_gprs2(sg_http_update_param.http_host, sg_http_update_param.http_port);
    if(ret){ return(-1); }
    led_control_function(LD_GPRS, LD_FLICKER);

    // 发送http请求(HEAD请求)
    ret = http_update_send_request_for_crcbin_file_size_by_gprs( sg_http_update_param.http_host, sg_http_update_param.http_port );
    if(ret != GPRS_SEND_OK)
    {
        http_update_close_connect_by_gprs();
        return(-4);
    }

    // 接收完整的http应答数据
    sg_http_update_param.http_response_recv_size = 0;
    if(sg_http_update_param.http_response_buff){ sg_http_update_param.http_response_buff[0] = 0; }
    while(true)
    {
        // 接收数据(消化当前已到达的所有 GPRS 分片)
        ret = http_update_drain_gprs_response(&cur_recv_size);
        if(ret == -3)
        {
            int complete = http_update_check_response_completed();
            if(complete >= 1){ break; } /* HEAD: 头齐即可,无 body */
            http_update_close_connect_by_gprs();
            return(-5);
        }
        else if(ret)
        {
            http_update_close_connect_by_gprs();
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
                    //printf("\nhttp更新,无数据接收超时....\n");
                    http_update_close_connect_by_gprs();
                    return(-6);
                }
            }
            vTaskDelay(10); continue;
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
    http_update_close_connect_by_gprs();
    led_control_function(LD_LAN, LD_OFF);

    // 获得 crc_bin 文件的大小
    ret = http_update_get_crc_bin_size(NULL);
    if(ret < 0){ return(-7); }

    return(0);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_send_request_for_crcbin_file_size_by_gprs
*    功能说明: 发送 HEAD 请求查询 crc_bin 文件 Content-Length
*    形    参: host 服务器地址; server_port 端口
*    返 回 值: gprs_send_data 返回值(GPRS_SEND_OK 为成功)
*********************************************************************************************************
*/
static int http_update_send_request_for_crcbin_file_size_by_gprs(const char *host, uint16_t server_port)
{
    char send_buf[256]={0};
    char *append_pt = send_buf;
    int ret = 0;
    ////

    sprintf(append_pt, "HEAD %s HTTP/1.1\r\n", sg_http_update_param.http_url); append_pt += strlen(append_pt);
    sprintf(append_pt, "Host: %s:%d\r\n\r\n", host, server_port); append_pt += strlen(append_pt); // 填写IP地址(最好不要填写域名 )

    //printf("\nhttp请求:\n%s\n", send_buf);
    ret = gprs_send_data( (uint8_t *)send_buf, (append_pt - send_buf), 1000, GPRS_LINK_OTA );

    return(ret);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_get_crc_bin_file_data_by_gprs
*    功能说明: 通过 GPRS 分块下载 crc_bin 固件并写入 SPI Flash
*    形    参: 无(使用 sg_http_update_param 中的分包参数)
*    返 回 值: 0-成功 <0-出错(-1连接 -2发送/接收 -3 CRC -4解析)
*********************************************************************************************************
*/
int http_update_get_crc_bin_file_data_by_gprs(void)
{
    int ret = 0;
    int cur_recv_size = 0;
    bool be_timing = false;
    unsigned int begin_ticks = 0, end_ticks = 0;
    unsigned int crc_check_err_times = 0, connect_times = 0;
    ////

    sg_http_update_param.section_current = 0;

RECONNECT:
    printf("OTA重连 section=%u HTTP累计=%u\n",
           sg_http_update_param.section_current,
           sg_http_update_param.http_response_recv_size);

    // 连接服务器
    printf("\n无线连接服务器 %s:%d ...\n", sg_http_update_param.http_host, sg_http_update_param.http_port);
    ret = http_update_connect_server_by_gprs2(sg_http_update_param.http_host, sg_http_update_param.http_port);
    if(ret)
    {
        vTaskDelay(1000);
        printf("close15\n");
        http_update_close_connect_by_gprs();
        connect_times++; // 连续连接失败的次数
        if(connect_times > 10){ return(-1); }
        goto RECONNECT;
    }
    connect_times = 0;
    led_control_function(LD_GPRS, LD_FLICKER);

    // 循环请求、接收数据块
    while(sg_http_update_param.section_current < sg_http_update_param.section_total)
    {
        /* 每块开始前清空 OTA 流,避免重试/keep-alive 下两包响应叠入缓冲导致 save 溢出(ret:-2) */
        gprs_reset_ota_rx_stream();
        sg_http_update_param.http_response_recv_size = 0;
        if(sg_http_update_param.http_response_buff){ sg_http_update_param.http_response_buff[0] = 0; }

        // 发送http请求(GET请求)
        ret = http_update_send_request_for_crcbin_data_by_gprs( sg_http_update_param.http_host, sg_http_update_param.http_port );
        if(ret != GPRS_SEND_OK)
        {
            printf("close9:%d\n",ret);
            http_update_close_connect_by_gprs();
            goto RECONNECT;
        }

        be_timing = false;
        begin_ticks = 0;
        end_ticks = 0;
        while(true)
        {
            // 接收数据(消化当前已到达的所有 GPRS 分片)
            ret = http_update_drain_gprs_response(&cur_recv_size);
            if(ret == -3)
            {
                int complete = http_update_check_response_completed();
                if(complete == 2){ break; }
                /* 头已齐体未到: disconn 可能早于尾包,继续等待(带超时保护) */
                if(complete == 1)
                {
                    if(!be_timing)
                    {
                        be_timing = true;
                        begin_ticks = HAL_GetTick();
                    }
                    else
                    {
                        end_ticks = HAL_GetTick();
                        if( (end_ticks - begin_ticks) >= HTTP_GPRS_BODY_WAIT_MS )
                        {
                            http_update_close_connect_by_gprs();
                            goto RECONNECT;
                        }
                    }
                    vTaskDelay(10);
                    continue;
                }
                http_update_close_connect_by_gprs();
                goto RECONNECT;
            }
            else if(ret) // 其它异常
            {
                http_update_close_connect_by_gprs();
                return(-2);
            }

            // 暂时无数据
            if(!cur_recv_size)
            {
                /* HTTP 头已到、体分包未齐: 不走"无数据"10s 超时,继续等尾包 */
                if(http_update_check_response_completed() == 1)
                {
                    if(!be_timing)
                    {
                        be_timing = true;
                        begin_ticks = HAL_GetTick();
                    }
                    else
                    {
                        end_ticks = HAL_GetTick();
                        if( (end_ticks - begin_ticks) >= HTTP_GPRS_BODY_WAIT_MS )
                        {
                            http_update_close_connect_by_gprs();
                            goto RECONNECT;
                        }
                    }
                    vTaskDelay(10);
                    continue;
                }

                if(!be_timing) // 非计时状态
                {
                    be_timing = true; // 开始计时
                    begin_ticks = HAL_GetTick();
                }
                else // 计时状态
                {
                    end_ticks = HAL_GetTick();
                    if( (end_ticks - begin_ticks) >= HTTP_GPRS_BODY_WAIT_MS )
                    {
                        http_update_close_connect_by_gprs();
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
                /* 应答已齐,清 OTA 流中尾包/keep-alive 残留,避免影响下一块拼包 */
                gprs_reset_ota_rx_stream();
                break;
            }
        } //while(接收完整的http应答数据)

        // 解析、保存数据
        ret = http_update_parse_crc_bin_data();
        if(ret)
        {
            if(ret == -5) // 偶尔会出现校验错误,此时重新下载即可
            {
                printf("OTA CRC校验失败 section=%u\n", sg_http_update_param.section_current);
                crc_check_err_times++; // 连续校验错误的次数
                if(crc_check_err_times > 10){ return(-3); }
                gprs_reset_ota_rx_stream();
                continue;
            }
            else
            {
                printf("OTA解析失败 ret=%d section=%u HTTP累计=%u\n",
                       ret, sg_http_update_param.section_current,
                       sg_http_update_param.http_response_recv_size);
                return(-4);
            }
        }

        crc_check_err_times = 0;
    } // while(循环请求、接收数据块)
    ////

    // 先关闭连接
    http_update_close_connect_by_gprs();
    led_control_function(LD_GPRS, LD_OFF);

    return(0);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_send_request_for_crcbin_data_by_gprs
*    功能说明: 发送带 Range 的 GET 请求下载当前固件分块
*    形    参: host 服务器地址; server_port 端口
*    返 回 值: gprs_send_data 返回值(GPRS_SEND_OK 为成功)
*********************************************************************************************************
*/
static int http_update_send_request_for_crcbin_data_by_gprs(const char *host, uint16_t server_port)
{
    char send_buf[256]={0};
    char *append_pt = send_buf;
    int ret = 0;
    unsigned int download_start = 0, download_end = 0;
    ////

    sprintf(append_pt, "GET %s HTTP/1.1\r\n", sg_http_update_param.http_url); append_pt += strlen(append_pt);
    sprintf(append_pt, "Host: %s:%d\r\n", host, server_port); append_pt += strlen(append_pt);

    download_start = (sg_http_update_param.section_current * UPDATE_CHUNK_SIZE);
    download_end = (download_start + UPDATE_CHUNK_SIZE - 1);
    sprintf(append_pt, "Range: bytes=%d-%d\r\n\r\n", download_start, download_end); append_pt += strlen(append_pt);

    //printf("\nhttp请求:\n%s\n", send_buf);
    ret = gprs_send_data( (uint8_t *)send_buf, (append_pt - send_buf), 5000, GPRS_LINK_OTA ); // 这个地方多等待一会儿

    return(ret);
}

/* ======================== 无线 OTA 后台任务 ======================== */

#define UPDATE_GSM_TASK_PRIO            (7U)
#define UPDATE_GSM_TASK_STK             (4096U)
static TaskHandle_t s_update_gsm_task = NULL;

/*
*********************************************************************************************************
*    函 数 名: update_gsm_task_done
*    功能说明: OTA 后台任务结束,清除任务句柄
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void update_gsm_task_done(void)
{
    s_update_gsm_task = NULL;
}

/*
*********************************************************************************************************
*    函 数 名: update_gsm_bg_task
*    功能说明: FreeRTOS 无线 OTA 后台任务,执行升级并清理模式后自删除
*    形    参: pvParameters 未使用
*    返 回 值: 无
*********************************************************************************************************
*/
static void update_gsm_bg_task(void *pvParameters)
{
    (void)pvParameters;
    FeedFwdgt();
    (void)update_gsm_task_function();

    if (update_get_mode_function() != UPDATE_MODE_NULL)
    {
        update_set_update_mode(UPDATE_MODE_NULL);
    }
    update_gsm_task_done();
    vTaskDelete(NULL);
}

/*
*********************************************************************************************************
*    函 数 名: update_gsm_task_function
*    功能说明: 无线 OTA 后台任务入口
*    形    参: 无
*    返 回 值: 0 成功,-1 失败
*********************************************************************************************************
*/
int8_t update_gsm_task_function(void)
{
    struct update_addr *param = app_get_http_ota_function();
    update_param_t *updateparam = NULL;
    int8_t ret = 0;
    ip_addr_t server_ipaddr;
    uint16_t server_port;

    updateparam = update_get_infor_data_function();

    if (gprs_get_module_status_function() != 1)
    {
        updateparam->mode = UPDATE_MODE_NULL;
        return -1;
    }

    gprs_network_disconnect_function(GPRS_LINK_OTA);
    led_control_function(LD_GPRS, LD_OFF);

    server_port = param->port;
    IP4_ADDR(&server_ipaddr, param->ip[0], param->ip[1], param->ip[2], param->ip[3]);

    sg_http_update_param.section_len = (UPDATE_CHUNK_SIZE - 2);
    sg_http_update_param.http_response_recv_size = 0;

    ret = http_update_get_info_txt_by_gprs(&server_ipaddr, server_port);
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

    ret = http_update_get_crc_bin_file_size_by_gprs();
    if (ret < 0)
    {
        printf("\n获得crc_bin文件大小,失败! ret: %d\n", ret);
        goto UPDATE_END;
    }

    ret = http_update_get_crc_bin_file_data_by_gprs();
    if (ret < 0)
    {
        printf("\n获得crc_bin文件内容,失败! ret: %d\n", ret);
        goto UPDATE_END;
    }
    printf("\n升级完成,重启设备...\n");
    http_update_success_reboot();
    ret = 0;

UPDATE_END:

    updateparam->mode = UPDATE_MODE_NULL;
    printf("update end\n");
    if (ret < 0)
    {
        http_update_failed();
        return -1;
    }
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: update_gsm_poll
*    功能说明: 无线 OTA 后台任务轮询
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void update_gsm_poll(void)
{
    BaseType_t ret;
    if (update_get_mode_function() != UPDATE_MODE_GPRS)
    {
        return;
    }

    if (s_update_gsm_task != NULL)
    {
        return;
    }

    ret = xTaskCreate(  update_gsm_bg_task,
                        "ota_gprs",
                        UPDATE_GSM_TASK_STK,
                        NULL,
                        UPDATE_GSM_TASK_PRIO,
                        &s_update_gsm_task);
    if (ret != pdPASS)
    {
        s_update_gsm_task = NULL;
    }
}

/*
*********************************************************************************************************
*    函 数 名: update_gsm_is_running
*    功能说明: 无线 OTA 后台任务是否运行中
*    形    参: 无
*    返 回 值: 1-运行中 0-空闲
*********************************************************************************************************
*/
uint8_t update_gsm_is_running(void)
{
    return (uint8_t)(s_update_gsm_task != NULL);
}

