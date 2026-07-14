#ifndef _UPDATE_HTTP_H_
#define _UPDATE_HTTP_H_

#include "./SYSTEM/sys/sys.h"
#include "lwip/ip_addr.h"

// http 升级步骤信息
struct IAPStruct
{
    // http应答包接收缓冲
    unsigned char *http_response_buff;
    unsigned int http_response_buff_size;
    unsigned int http_response_recv_size;

    // 更新的version
    char update_version[64];
    char update_url[256];

    // 提取的更新信息
    char http_host[64]; // 服务器ip,或域名
    unsigned short http_port;
    char http_url[128];
    ip_addr_t http_server_addr; // crc_bin文件的服务器地址

    uint32_t crcfile_length;    // CRC文件大小(块大小的整数倍)
    uint16_t section_total;     // 文件分包个数
    uint16_t section_len;       // 每包的实际数据(去掉校验2字节)大小
    uint16_t section_current;   // 当前包计数
};

extern struct IAPStruct sg_http_update_param;

/* 公共 HTTP 应答解析(供 update_lwip / update_gsm 调用) */
int http_update_save_response(const unsigned char *src_data, int src_data_size);
int http_update_check_response_completed(void);

/* 公共 HTTP 请求构造 */
int http_update_build_info_txt_request(char *buf, int buf_size, const char *host, uint16_t port);
int http_update_build_head_request(char *buf, int buf_size, const char *host, uint16_t port);
int http_update_build_range_request(char *buf, int buf_size, const char *host, uint16_t port);

/* 公共 HTTP 业务收尾(接收+解析) */
int http_update_finish_get_info_txt(int (*recv_func)(int *out_size),
                                     void (*close_func)(void));
int http_update_finish_get_crc_bin_size(int (*recv_func)(int *out_size),
                                         void (*close_func)(void));
int http_update_recv_parse_one_chunk(int (*recv_func)(int *out_size),
                                      void (*close_func)(void));

int http_update_chack_version(void);
int http_update_get_url(void);
int http_update_get_crc_bin_size(unsigned int *file_size);
int http_update_parse_crc_bin_data(void);

/* 有线 OTA HTTP */
int http_update_get_info_txt_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port);
int http_update_get_crc_bin_file_size_by_lwip(void);
int http_update_get_crc_bin_file_data_by_lwip(void);
void http_update_close_connect_by_lwip(void);

/* 无线 OTA HTTP */
int http_update_get_info_txt_by_gprs(ip_addr_t *server_ipaddr, uint16_t server_port);
int http_update_get_crc_bin_file_size_by_gprs(void);
int http_update_get_crc_bin_file_data_by_gprs(void);
void http_update_close_connect_by_gprs(void);

/* 公共升级结果处理 */
void http_update_success_reboot(void);
void http_update_failed(void);
void http_update_clear_param(void);

#if 0 // 测试追踪
void trace_update_param(const char *trace_flag);
void trace_update_param_save(void);
#endif

#endif
