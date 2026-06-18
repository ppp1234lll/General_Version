#include "./UPDATE/inc/update_http.h"
#include "main.h"

struct netconn *tcp_update;
////

static int http_update_connect_server_by_lwip(ip_addr_t *ip, unsigned short port);
static int http_update_connect_server_by_gprs(ip_addr_t *ip, unsigned short port);
static int http_update_connect_server_by_gprs2(const char *host, unsigned short port);
static int http_update_send_request_for_info_txt_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port);
static int http_update_send_request_for_info_txt_by_gprs(ip_addr_t *server_ipaddr, uint16_t server_port);
static int http_update_recv_reponse_by_lwip(int *out_recv_size);
static int http_update_save_response(const unsigned char *src_data, int src_data_size);
static int http_update_check_response_completed(void);
static int http_update_chack_version(void);
static int http_update_send_request_for_crcbin_file_size_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port);
static int http_update_send_request_for_crcbin_file_size_by_gprs(const char *host, uint16_t server_port);
static int http_update_get_url(void);
static int http_update_get_crc_bin_size(unsigned int *file_size);
static int http_update_send_request_for_crcbin_data_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port);
static int http_update_send_request_for_crcbin_data_by_gprs(const char *host, uint16_t server_port);
static int http_update_parse_crc_bin_data(void);
static int http_update_recv_reponse_by_gprs(int *out_recv_size);
static void http_update_cb_server_ip(const char *name, const ip_addr_t *ipaddr, void *arg);

// http升级信息
struct IAPStruct sg_http_update_param = {0};

// 升级信息文件 url(可配置)
char http_info_txt_url[64] = {"/FN-ST-BJ-01/info.txt"};
////

// 1: 获得info.txt信息
// {"version":"1.5.6.20240905";"url":"http://47.104.98.214:8989/gajc/FN-ZJGD-QG-1.5.6.20240905.bin";}
// 返回值: 1: 版本号不同,更新; 2:版本号相同,无需更新; <0: 出错
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
/////////////////////

// 1: 获得info.txt信息
// {"version":"1.5.6.20240905";"url":"http://47.104.98.214:8989/gajc/FN-ZJGD-QG-1.5.6.20240905.bin";}
// 返回值: 1: 版本号不同,更新; 2:版本号相同,无需更新; <0: 出错
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

    // 发送http请求
    ret = http_update_send_request_for_info_txt_by_gprs(server_ipaddr, server_port);
    if(ret != GPRS_SEND_OK)
    {
        http_update_close_connect_by_gprs();
        return(-2);
    }

    // 接收完整的http应答数据
    sg_http_update_param.http_response_recv_size = 0;
    while(true)
    {
        // 接收数据
        ret = http_update_recv_reponse_by_gprs(&cur_recv_size);
        if(ret)
        {
            http_update_close_connect_by_gprs();
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
                    http_update_close_connect_by_gprs();
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
////////////////////

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
/////////////////////

static int http_update_connect_server_by_gprs(ip_addr_t *ip, unsigned short port)
{
    update_param_t *updateparam = NULL;
    unsigned char index = 0;
    int ret = 0;
    ////

    updateparam = update_get_infor_data_function();
    for(index=0; index<3; index++)
    {
        ret = gprs_network_connect_server(ipaddr_ntoa(ip), port);
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
////////////////////

static int http_update_connect_server_by_gprs2(const char *host, unsigned short port)
{
    update_param_t *updateparam = NULL;
    unsigned char index = 0;
    int ret = 0;
    ////

    updateparam = update_get_infor_data_function();
    for(index=0; index<3; index++)
    {
        ret = gprs_network_connect_server(host, port);
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
////////////////////

// info.txt, 发送http请求
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
/////////////////////

// info.txt, 发送http请求
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
    ret = gprs_send_data( (uint8_t *)send_buf, (append_pt - send_buf), 1000 );

    return(ret);
}
/////////////////////

// 关闭连接
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
/////////////////

void http_update_close_connect_by_gprs(void)
{
    update_param_t *updateparam = NULL;
    ////

    gprs_disconnect();

    updateparam = update_get_infor_data_function();
    updateparam->gprs_t.connect = 0;
}
/////////////////

// 接收http应答
// 0:成功, -1:未连接, -2:内容超大, -3:连接断开
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
//////////////////

// 接收http应答
// 0:成功, -1:未连接, -2:内容超大, -3:连接断开
static int http_update_recv_reponse_by_gprs(int *out_recv_size)
{
    int ret = 0;
    const unsigned char *recv_data = NULL;
    int recv_data_size = 0;
    ////

    if(out_recv_size){ (*out_recv_size) = 0; }

    ret = gprs_recv_data(&recv_data, &recv_data_size);
    if(ret != GPRS_SEND_OK){ return(-3); }

    // 保存数据
    if(!recv_data || !recv_data_size){ return(0); }

    // 保存到 http 应答buff 中
    //printf("\n提取:\n%s\n", (const char *)recv_data);
    ret = http_update_save_response(recv_data, recv_data_size);
    if(ret){ return(-2); }

    if(out_recv_size){ (*out_recv_size) = recv_data_size; }

    return(0);
}
//////////////////

// 保存到 http 应答buuf中 
static int http_update_save_response(const unsigned char *src_data, int src_data_size)
{
    // 开辟空间
    if(!sg_http_update_param.http_response_buff)
    {
        sg_http_update_param.http_response_buff_size = (2*1024); // 初始化为2k,注意!动态开辟内容不能超过2k,否则影响flash的存储,是个隐患.
        sg_http_update_param.http_response_buff = (unsigned char *)mymalloc(SRAMIN, sg_http_update_param.http_response_buff_size);
        sg_http_update_param.http_response_recv_size = 0;
    }

    // 扩展空间
    if( (sg_http_update_param.http_response_recv_size + src_data_size) > (2*1024) ){ return(-1); } // 不能开辟 >2k 的空间

    #if 0 // 扩展没用,带来隐患
    if( (sg_http_update_param.http_response_recv_size + src_data_size) > sg_http_update_param.http_response_buff_size )
    {
        sg_http_update_param.http_response_buff_size = (sg_http_update_param.http_response_recv_size + src_data_size + 1024);
        sg_http_update_param.http_response_buff = (unsigned char *)myrealloc(SRAMIN, (void *)(sg_http_update_param.http_response_buff), sg_http_update_param.http_response_buff_size);
    }
    #endif

    // 追加数据
    memcpy( (void *)(sg_http_update_param.http_response_buff + sg_http_update_param.http_response_recv_size), (void *)src_data, src_data_size );
    sg_http_update_param.http_response_recv_size += src_data_size;
    sg_http_update_param.http_response_buff[ sg_http_update_param.http_response_recv_size ] = 0; // 结尾清0

    return(0);
}
/////////////////

// http应答是否完整
// 0:头没接收完
// 1:body没收完
// 2:全部接收完
static int http_update_check_response_completed(void)
{
    int http_head_size=0;
    char *pt=NULL;
    int body_size = 0;
    ////

    // http头
    pt = strstr((char *)(sg_http_update_param.http_response_buff), "\r\n\r\n");
    if(!pt)
    {
        if(sg_http_update_param.http_response_recv_size >= 1024){ return(-1); } // 超大的头
        return(0);
    }
    pt += 4;
    http_head_size = ( pt - (char*)(sg_http_update_param.http_response_buff) );

    // "Content-Length:"字段
    pt = strstr((char *)(sg_http_update_param.http_response_buff), "Content-Length:");
    if( !pt || (pt >= (char *)(sg_http_update_param.http_response_buff) + http_head_size) ){ return(-1); }
    pt+=15;
    while( ((*pt) == ' ') || ((*pt) == '\t') ){ pt++; }
    body_size = atoi(pt);
    if( body_size >= (8*1024) ){ return(-2); } // 超大body

    // body是否完整
    if( (sg_http_update_param.http_response_recv_size - http_head_size) < body_size){ return(1); }

    return(2);
}
/////////////////////////////

// 查询版本号
// 1: 版本号不同,更新; 2:版本号相同,无需更新; <0: 出错
static int http_update_chack_version(void)
{
    char *str, *pt1, *pt2, *http_body = NULL;
    int ret = 0, version_str_len = 0, url_len = 0;
    char dev_string[50]= {0};
    sprintf(dev_string,"%s-%s",HARD_NO_STR,SOFT_NO_STR);
    ////

    // http应答码
    ret = strncmp( (char*)(sg_http_update_param.http_response_buff), "HTTP/1.1 200 OK\r\n", 17);
    if(ret){ return(-1); }

    http_body = strstr( (char*)(sg_http_update_param.http_response_buff), "\r\n\r\n" );
    if(!http_body){ return(-2); }
    http_body += 4;

    // 获取版本号
    str = strstr(http_body, "\"version\":"); //获取版本号
    if(!str){ return(-3); }

    pt1 = str + 10;
    while( ((*pt1) == ' ') || ((*pt1) == '\t') ){ pt1++; }
    if( (*pt1) != '\"' ){ return(-4); }
    pt1++;

    pt2 = strchr(pt1, '\"');
    if(!pt2){ return(-5); }

    version_str_len = (int)(pt2 - pt1);
    if( !version_str_len || (version_str_len >= sizeof(sg_http_update_param.update_version)) ){ return(-6); }

    memset( sg_http_update_param.update_version, 0, sizeof(sg_http_update_param.update_version) );
    memcpy(sg_http_update_param.update_version, pt1, version_str_len);

    // 获取url
    str = strstr(http_body, "\"url\":");
    if(!str){ return(-6); }
    
    pt1 = str + 6;
    while( ((*pt1) == ' ') || ((*pt1) == '\t') ){ pt1++; }
    if( (*pt1) != '\"' ){ return(-7); }
    pt1++;

    pt2 = strchr(pt1, '\"');
    if(!pt2){ return(-8); }

    url_len = (int)(pt2 - pt1);
    if( !url_len || (url_len >= sizeof(sg_http_update_param.update_url)) ){ return(-9); }

    memset(sg_http_update_param.update_url, 0, sizeof(sg_http_update_param.update_url));
    memcpy(sg_http_update_param.update_url, pt1, url_len);

    // 比较版本号
    ret = strcmp(dev_string, sg_http_update_param.update_version);
    if(ret){ return(1); }
    else if(!ret){ return(2); }

    return(-10);
}
////////////////////////

// 2: 获得crc_bin文件大小
// 返回值: 0: 成功, <0: 出错 
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
////////////////////////

// 2: 获得crc_bin文件大小
// 返回值: 0: 成功, <0: 出错 
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
    while(true)
    {
        // 接收数据
        ret = http_update_recv_reponse_by_gprs(&cur_recv_size);
        if(ret)
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
////////////////////////

// 发送http请求(HEAD请求)
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
////////////////////

// 发送http请求(HEAD请求)
static int http_update_send_request_for_crcbin_file_size_by_gprs(const char *host, uint16_t server_port)
{
    char send_buf[256]={0};
    char *append_pt = send_buf;
    int ret = 0;
    ////

    sprintf(append_pt, "HEAD %s HTTP/1.1\r\n", sg_http_update_param.http_url); append_pt += strlen(append_pt);
    sprintf(append_pt, "Host: %s:%d\r\n\r\n", host, server_port); append_pt += strlen(append_pt); // 填写IP地址(最好不要填写域名 )

    //printf("\nhttp请求:\n%s\n", send_buf);
    ret = gprs_send_data( (uint8_t *)send_buf, (append_pt - send_buf), 1000 );

    return(ret);
}
////////////////////

// 提取url
static int http_update_get_url(void)
{
    char *scan_pt = NULL;
    char *pt1, *pt2;
    unsigned int len = 0, port_val = 0;
    update_param_t *updateparam = NULL;
    ////

    // http://
    if(!strncmp(sg_http_update_param.update_url, "http://", 7)){ scan_pt = sg_http_update_param.update_url + 7; }
    else if(!strncmp(sg_http_update_param.update_url, "https://", 8)){ return(-1); } // 不支持 https
    else{ scan_pt = sg_http_update_param.update_url; }

    // host
    pt1 = scan_pt;
    pt2 = strchr(pt1, ':');
    if(!pt2) // 无端口号
    {
        pt2 = strchr(pt1, '/');
        if(!pt2){ return(-2); }

        len = (unsigned int)(pt2 - pt1); // host 长度
        if(!len) // host 缺省
        {
            updateparam = update_get_infor_data_function();
            sprintf(sg_http_update_param.http_host, "%d.%d.%d.%d", updateparam->ip[0], updateparam->ip[1], updateparam->ip[2], updateparam->ip[3]);
        }
        else
        {
            if( len >= sizeof(sg_http_update_param.http_host) ){ return(-3); }
            memset(sg_http_update_param.http_host, 0, sizeof(sg_http_update_param.http_host));
            memcpy(sg_http_update_param.http_host, pt1, len);
        }

        sg_http_update_param.http_port = 80; // 默认80端口
        scan_pt = pt2;
    }
    else // 有端口号
    {
        len = (unsigned int)(pt2 - pt1); // host 长度
        if(!len) // host 缺省
        {
            updateparam = update_get_infor_data_function();
            sprintf(sg_http_update_param.http_host, "%d.%d.%d.%d", updateparam->ip[0], updateparam->ip[1], updateparam->ip[2], updateparam->ip[3]);
        }
        else
        {
            if( len >= sizeof(sg_http_update_param.http_host) ){ return(-4); }
            memset(sg_http_update_param.http_host, 0, sizeof(sg_http_update_param.http_host));
            memcpy(sg_http_update_param.http_host, pt1, len);
        }

        port_val = atoi(pt2 + 1);
        if(!port_val || (port_val >= 0xFFFF)){ return(-5); }
        sg_http_update_param.http_port = (unsigned short)port_val; // 指定端口号

        pt1 = pt2 + 1; // 冒号后
        pt1 = strchr(pt1, '/');
        if(!pt1){ return(-6); }
        scan_pt = pt1;
    }

    // url
    pt1 = scan_pt; // url开始位置'/'
    len = (unsigned int)strlen(pt1);
    if( !len || (len >= sizeof(sg_http_update_param.http_url)) ){ return(-8); }
    memset(sg_http_update_param.http_url, 0, sizeof(sg_http_update_param.http_url));
    memcpy(sg_http_update_param.http_url, pt1, len);

    return(0);
}
///////////////////////

// DNS解析回调
static void http_update_cb_server_ip(const char *name, const ip_addr_t *ipaddr, void *arg)
{
    struct ip_addr *out_addr = (struct ip_addr *)arg;
    ////

    if( !ipaddr || !(ipaddr->addr) ){ return; }

    memcpy(out_addr, ipaddr, sizeof(ip_addr_t));
}
///////////////////////

// 获得 crc_bin 文件的大小
static int http_update_get_crc_bin_size(unsigned int *file_size)
{
    char *str;
    int ret = 0;
    unsigned int len = 0;
    ////

    // http应答码
    ret = strncmp( (char*)(sg_http_update_param.http_response_buff), "HTTP/1.1 200 OK\r\n", 17);
    if(ret){ return(-1); }

    // "Content-Length:" 字段
    str = strstr( (char*)(sg_http_update_param.http_response_buff), "Content-Length:" );
    if(!str){ return(-2); }
    str += 15;

    while( ((*str) == ' ') || ((*str) == '\t') ){ str++; }

    len = (unsigned int)atol(str);

    if(file_size){ (*file_size) = len; }

    sg_http_update_param.crcfile_length = len;
    sg_http_update_param.section_len = (UPDATE_CHUNK_SIZE - 2); // 块大小统一为 1024 字节
    if(len % UPDATE_CHUNK_SIZE){ return(-3); } // 文件大小不是块的整数倍
    sg_http_update_param.section_total = (len / UPDATE_CHUNK_SIZE);
    sg_http_update_param.section_current = 0;

    return(0);
}
//////////////////////

// 3: 获得crc_bin文件数据
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
/////////////////////

// 3: 获得crc_bin文件数据
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

    // 连接服务器
    printf("\n无线连接服务器 %s:%d ...\n", sg_http_update_param.http_host, sg_http_update_param.http_port);
    ret = http_update_connect_server_by_gprs2(sg_http_update_param.http_host, sg_http_update_param.http_port);
    if(ret)
    {
        vTaskDelay(3000);
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
        // 发送http请求(GET请求)
        ret = http_update_send_request_for_crcbin_data_by_gprs( sg_http_update_param.http_host, sg_http_update_param.http_port );
        if(ret != GPRS_SEND_OK)
        {
            printf("close9:%d\n",ret);
            http_update_close_connect_by_gprs();
            goto RECONNECT;
        }

        // 接收完整的http应答数据
        sg_http_update_param.http_response_recv_size = 0;
        be_timing = false;
        begin_ticks = 0;
        end_ticks = 0;
        while(true)
        {
            // 接收数据
            ret = http_update_recv_reponse_by_gprs(&cur_recv_size);
            if(ret == -3) // 服务器断开,需要重新连接
            {
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
    http_update_close_connect_by_gprs();
    led_control_function(LD_GPRS, LD_OFF);

    return(0);
}
/////////////////////

// 发送http请求(GET请求)
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
////////////////////

// 发送http请求(GET请求)
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
    ret = gprs_send_data( (uint8_t *)send_buf, (append_pt - send_buf), 5000 ); // 这个地方多等待一会儿

    return(ret);
}
////////////////////

// 解析、保存数据
static int http_update_parse_crc_bin_data(void)
{
    char *pt = NULL;
    int ret = 0;
    unsigned int len = 0;
    unsigned char *body_pt = NULL;
    uint16_t count_crc = 0, section_crc = 0;
    unsigned int write_addr = UPDATA_SPIFLASH_ADDR;


    // http状态码
    ret = strncmp( (char*)(sg_http_update_param.http_response_buff), "HTTP/1.1 206 Partial Content", 28);
    if(ret){ return(-1); }

    // "Content-Length:" 字段
    pt = strstr( (char*)(sg_http_update_param.http_response_buff), "Content-Length:" );
    if(!pt){ return(-2); }
    pt += 15;

    while( ((*pt) == ' ') || ((*pt) == '\t') ){ pt++; }

    len = (unsigned int)atol(pt);
    if(len != UPDATE_CHUNK_SIZE){ return(-3); }

    // 验证校验和
    pt = strstr(pt, "\r\n\r\n");
    if(!pt){ return(-4); }
    body_pt = (unsigned char *)(pt + 4);

    count_crc = CRC16_MODBUS(body_pt, (UPDATE_CHUNK_SIZE-2));  //计算数据包crc校验值
    section_crc = ( (body_pt[UPDATE_CHUNK_SIZE - 2] << 8) | (body_pt[UPDATE_CHUNK_SIZE - 1]) ); // 块尾的校验值
    if(count_crc != section_crc){ return(-5); } // 校验失败

    // 保存这块数据
    write_addr = UPDATA_SPIFLASH_ADDR + (sg_http_update_param.section_current * sg_http_update_param.section_len);

    sf_WriteBuffer(body_pt, write_addr, sg_http_update_param.section_len);

    (sg_http_update_param.section_current)++;

    return(0);
}
////////////////////

// 升级完成,重启设备
void http_update_success_reboot(void)
{
    struct BOOT_UPDATE_PARAM boot_update_param = {0};

    // 保存升级参数
    boot_update_param.is_update = true;
    boot_update_param.section_count = sg_http_update_param.section_total;
    boot_update_param.section_size = sg_http_update_param.section_len;

    sf_WriteBuffer((uint8_t *)(&boot_update_param), UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM));

    lfs_unmount(&g_lfs_t);

    app_system_softreset(); // 重启系统
}
////////////////////

#if 0 // 追踪测试用的
void trace_update_param(const char *trace_flag)
{
    struct BOOT_UPDATE_PARAM boot_update_param = {0};
    ////

    W25QXX_Read( (uint8_t*)(&boot_update_param), UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM) );
    //STMFLASH_Read(UPDATA_PARAM_ADDR, (u16 *)(&boot_update_param), sizeof(struct BOOT_UPDATE_PARAM)/2);
    printf("\n追踪读取 %s, is_update: %d, section_count: %u, section_size: %u \n", trace_flag, boot_update_param.is_update, boot_update_param.section_count, boot_update_param.section_size);
}
///////////////////

void trace_update_param_save(void)
{
    struct BOOT_UPDATE_PARAM boot_update_param = {0};
    OS_CPU_SR cpu_sr = 0;
    ////

    // 保存升级标志
    boot_update_param.is_update = 0;
    boot_update_param.section_count = 174;
    boot_update_param.section_size = 1024;
    printf("\n追踪保存, is_update: %d, section_count: %u, section_size: %u\n", boot_update_param.is_update, boot_update_param.section_count, boot_update_param.section_size);
    OS_ENTER_CRITICAL();// 关中断
    {
        W25QXX_Write((uint8_t *)(&boot_update_param), UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM));
        //STMFLASH_Write(UPDATA_PARAM_ADDR, (u16 *)(&boot_update_param), sizeof(struct BOOT_UPDATE_PARAM)/2);
    }
    OS_EXIT_CRITICAL();// 开中断
}
////////////////////
#endif
