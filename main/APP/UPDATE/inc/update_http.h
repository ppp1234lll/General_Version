#ifndef _UPDATE_HTTP_H_
#define _UPDATE_HTTP_H_

#include "./SYSTEM/sys/sys.h"
#include "lwip/ip_addr.h"

#define UPDATE_CHUNK_SIZE       (1024 + 2)
#define UPDATA_SPIFLASH_ADDR    (15*1024*1024) // W25Q128升级文件存储地址
#define UPDATA_PARAM_ADDR (UPDATA_SPIFLASH_ADDR + (1*1024*1024) - (1*1024)) // 系统升级参数存储地址


// http 升级步骤信息
struct IAPStruct
{
	// http应答包接收缓冲
	unsigned char *http_response_buff;
	unsigned int http_response_buff_size;
	unsigned int http_response_recv_size;
	////

	// 更新的version
	char update_version[64];
	char update_url[256];
	////

	// 提取的更新信息
	char http_host[64]; // 服务器ip,或域名
	unsigned short http_port;
	char http_url[128];
	ip_addr_t http_server_addr; // crc_bin文件的服务器地址
	////

	uint32_t crcfile_length;    // CRC文件大小(块大小的整数倍)
	uint16_t section_total;     // 文件分包个数
	uint16_t section_len;       // 每包的实际数据(去掉校验2字节)大小
	uint16_t section_current;   // 当前包计数
};
////

// 开机升级参数
struct BOOT_UPDATE_PARAM
{
	unsigned int is_update;     // true:需要升级, false:无需升级
	unsigned int section_size;  // 每包的实际数据(去掉校验2字节)大小
	unsigned int section_count; // 总包数
};

extern struct IAPStruct sg_http_update_param;

extern int http_update_get_info_txt_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port);
extern int http_update_get_info_txt_by_gprs(ip_addr_t *server_ipaddr, uint16_t server_port);
extern int http_update_get_crc_bin_file_size_by_lwip(void);
extern int http_update_get_crc_bin_file_size_by_gprs(void);
extern int http_update_get_crc_bin_file_data_by_lwip(void);
extern int http_update_get_crc_bin_file_data_by_gprs(void);
extern void http_update_close_connect_by_lwip(void);
extern void http_update_close_connect_by_gprs(void);
extern void http_update_success_reboot(void);

#if 0 // 测试追踪
void trace_update_param(const char *trace_flag);
void trace_update_param_save(void);
#endif

#endif
