#include "./UPDATE/inc/update_http.h"
#include "main.h"
#include <stdbool.h>

// http升级信息
struct IAPStruct sg_http_update_param = {0};

// 升级信息文件 url(可配置)
char http_info_txt_url[64] = {"/FN-ST-BJ-01/info.txt"};

/* GPRS HTTP 应答缓冲上限: 206 块 = HTTP头(~512) + UPDATE_CHUNK_SIZE(1026) */
#define HTTP_GPRS_RSP_MAX          (UPDATE_CHUNK_SIZE + 2048)

static void *http_update_memmem(const void *haystack, int haystack_len, const void *needle, int needle_len);
static int http_update_expected_response_size(void);
static int http_update_chunk_starts_with_http(const unsigned char *data, int len);

/* GPRS 分片 payload 起始处是否为 HTTP 应答行。
 * 固件 bin 体内可能含 "HTTP/1.1" 子串,不可在整段 memmem 误判为新应答。 */
static int http_update_chunk_starts_with_http(const unsigned char *data, int len)
{
    if(len < 12){ return 0; }
    if(!memcmp(data, "HTTP/1.1 ", 9) || !memcmp(data, "HTTP/1.0 ", 9))
    {
        if(data[9] >= '0' && data[9] <= '9'){ return 1; }
    }
    return 0;
}

// 保存到 http 应答buuf中
int http_update_save_response(const unsigned char *src_data, int src_data_size)
{
    const unsigned char *save_ptr = src_data;
    int save_len = src_data_size;
    int expected_size = 0;
    int remain = 0;
    int recv_size = 0;
    void *http_pos = NULL;

    if(!src_data || src_data_size <= 0){ return(0); }

    // 开辟空间
    if(!sg_http_update_param.http_response_buff)
    {
        sg_http_update_param.http_response_buff_size = HTTP_GPRS_RSP_MAX;
        sg_http_update_param.http_response_buff = (unsigned char *)mymalloc(SRAMIN, sg_http_update_param.http_response_buff_size);
        if(!sg_http_update_param.http_response_buff){ return(-1); }
        sg_http_update_param.http_response_recv_size = 0;
    }

    recv_size = (int)sg_http_update_param.http_response_recv_size;
    expected_size = http_update_expected_response_size();

    /* 已有完整应答: 忽略 keep-alive 后续整包,不再参与拼包 */
    if(expected_size > 0 && recv_size >= expected_size)
    {
        if(http_update_chunk_starts_with_http(src_data, src_data_size))
        {
            printf("丢弃重复HTTP头数据: %d bytes\n", src_data_size);
        }
        return(0);
    }

    if(recv_size == 0)
    {
        http_pos = http_update_memmem(src_data, src_data_size, "HTTP/1.1 ", 9);
        if(!http_pos)
        {
            http_pos = http_update_memmem(src_data, src_data_size, "HTTP/1.0 ", 9);
        }
        if(!http_pos)
        {
            printf("丢弃无HTTP头数据: %d bytes\n", src_data_size);
            return(0);
        }
        save_ptr = (const unsigned char *)http_pos;
        save_len = src_data_size - (int)(save_ptr - src_data);
    }
    else
    {
        /* 当前响应未收齐时又收到新的 HTTP 应答行: 说明前一份是残缺/过期响应,从新应答重拼。
         * 典型现象: HTTP累计只有 430/530/701/766,随后又来 1314/1315/1316 字节完整包。 */
        if(http_update_chunk_starts_with_http(src_data, src_data_size))
        {
            if(expected_size == 0 || recv_size < expected_size)
            {
                sg_http_update_param.http_response_recv_size = 0;
                if(sg_http_update_param.http_response_buff){ sg_http_update_param.http_response_buff[0] = 0; }
                expected_size = 0;
                save_ptr = src_data;
                save_len = src_data_size;
            }
        }
        /* 否则为头/体续包,直接追加; 不在二进制体内搜索 HTTP 子串 */
    }

    expected_size = http_update_expected_response_size();
    if(expected_size > 0)
    {
        recv_size = (int)sg_http_update_param.http_response_recv_size;
        remain = expected_size - recv_size;
        if(remain <= 0){ return(0); }
        if(save_len > remain){ save_len = remain; }
    }

    if(save_len <= 0){ return(0); }
    if( (sg_http_update_param.http_response_recv_size + save_len) > HTTP_GPRS_RSP_MAX ){ return(-1); }

    #if 0 // 扩展没用,带来隐患
    if( (sg_http_update_param.http_response_recv_size + src_data_size) > sg_http_update_param.http_response_buff_size )
    {
        sg_http_update_param.http_response_buff_size = (sg_http_update_param.http_response_recv_size + src_data_size + 1024);
        sg_http_update_param.http_response_buff = (unsigned char *)myrealloc(SRAMIN, (void *)(sg_http_update_param.http_response_buff), sg_http_update_param.http_response_buff_size);
    }
    #endif

    // 追加数据
    memcpy( (void *)(sg_http_update_param.http_response_buff + sg_http_update_param.http_response_recv_size),
            (void *)save_ptr, (size_t)save_len );
    sg_http_update_param.http_response_recv_size += (unsigned int)save_len;
    sg_http_update_param.http_response_buff[ sg_http_update_param.http_response_recv_size ] = 0; // 结尾清0

    return(0);
}
/////////////////

static void *http_update_memmem(const void *haystack, int haystack_len, const void *needle, int needle_len)
{
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    int i = 0;

    if(!haystack || !needle || needle_len <= 0){ return(NULL); }
    if(haystack_len < needle_len){ return(NULL); }

    for(i=0; i <= (haystack_len - needle_len); i++)
    {
        if(h[i] == n[0] && !memcmp(&h[i], n, (size_t)needle_len)){ return((void *)&h[i]); }
    }

    return(NULL);
}
/////////////////

static int http_update_expected_response_size(void)
{
    char *head_end = NULL;
    char *pt = NULL;
    int head_size = 0;
    int body_size = 0;

    if(!sg_http_update_param.http_response_buff){ return(0); }

    head_end = strstr((char *)sg_http_update_param.http_response_buff, "\r\n\r\n");
    if(!head_end){ return(0); }
    head_end += 4;
    head_size = (int)(head_end - (char *)sg_http_update_param.http_response_buff);

    pt = strstr((char *)sg_http_update_param.http_response_buff, "Content-Length:");
    if(!pt || (pt >= head_end)){ return(0); }
    pt += 15;
    while( ((*pt) == ' ') || ((*pt) == '\t') ){ pt++; }
    body_size = atoi(pt);
    if(body_size < 0 || body_size >= (8*1024)){ return(0); }

    return(head_size + body_size);
}
/////////////////
// http应答是否完整
// 0:头没接收完
// 1:body没收完
// 2:全部接收完
int http_update_check_response_completed(void)
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

    // "Content-Length:"字段(无此字段时按 Connection:close 处理,头齐后仍视为 body 未齐)
    pt = strstr((char *)(sg_http_update_param.http_response_buff), "Content-Length:");
    if( !pt || (pt >= (char *)(sg_http_update_param.http_response_buff) + http_head_size) ){ return(1); }
    pt+=15;
    while( ((*pt) == ' ') || ((*pt) == '\t') ){ pt++; }
    body_size = atoi(pt);
    if( body_size >= (8*1024) ){ return(-2); } // 超大body

    // body是否完整
    if( (sg_http_update_param.http_response_recv_size - http_head_size) < body_size){ return(1); }

    return(2);
}
/////////////////////////////

/* info.txt 应答是否可解析; 在 Content-Length 满足后再确认 body 含 version 字段,
 * 避免 GPRS 头/体分包时仅收到 HTTP 头即误判为完整。 */
int http_update_info_txt_response_ready(void)
{
    char *body;
    int ret = http_update_check_response_completed();

    if(ret != 2){ return ret; }

    body = strstr((char *)(sg_http_update_param.http_response_buff), "\r\n\r\n");
    if(!body){ return 0; }
    body += 4;

    if(!strstr(body, "\"version\":")){ return 1; }

    return 2;
}
/////////////////////////////

// 查询版本号
// 1: 版本号不同,更新; 2:版本号相同,无需更新; <0: 出错
int http_update_chack_version(void)
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

// 提取url
int http_update_get_url(void)
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

// 获得 crc_bin 文件的大小
int http_update_get_crc_bin_size(unsigned int *file_size)
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

// 解析、保存数据
int http_update_parse_crc_bin_data(void)
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
    boot_update_param.update_status = UPDATE_NONE;
    sf_WriteBuffer((uint8_t *)(&boot_update_param), UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM));

    lfs_unmount(&g_lfs_t);

    app_system_softreset(); // 重启系统
}
// 升级失败
void http_update_failed(void)
{
    struct BOOT_UPDATE_PARAM boot_update_param = {0};

    // 保存升级参数
    boot_update_param.is_update = false;
    boot_update_param.section_count = 0;
    boot_update_param.section_size = 0;
    boot_update_param.update_status = UPDATE_FAILED;
    sf_WriteBuffer((uint8_t *)(&boot_update_param), UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM));
}
// 清除升级参数
void http_update_clear_param(void)
{
    struct BOOT_UPDATE_PARAM boot_update_param = {0};

    // 保存升级参数
    boot_update_param.is_update = false;
    boot_update_param.section_count = 0;
    boot_update_param.section_size = 0;
    boot_update_param.update_status = UPDATE_NONE;
    sf_WriteBuffer((uint8_t *)(&boot_update_param), UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM));
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
