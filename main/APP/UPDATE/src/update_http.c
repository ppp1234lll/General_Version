#include "./UPDATE/inc/update_http.h"
#include "main.h"
#include <stdbool.h>

/* 全局 HTTP 升级参数结构体，保存升级过程中所有状态信息 */
struct IAPStruct sg_http_update_param = {0};

/*
*********************************************************************************************************
*    函 数 名: http_update_save_response
*    功能说明: 将接收到的 HTTP 响应数据追加保存到缓冲区
*    形    参: src_data      接收到的数据指针
*              src_data_size 数据长度
*    返 回 值:  0  成功
*              -1  缓冲区溢出(超过2KB)
*    备    注: 首次调用时自动分配 2KB 缓冲区，末尾自动补 '\0'
*********************************************************************************************************
*/
int http_update_save_response(const unsigned char *src_data, int src_data_size)
{
	/* 首次调用时分配接收缓冲区 */
	if(!sg_http_update_param.http_response_buff)
	{
		sg_http_update_param.http_response_buff_size = (2*1024);
		sg_http_update_param.http_response_buff = (unsigned char *)mymalloc(SRAMIN, sg_http_update_param.http_response_buff_size);
		sg_http_update_param.http_response_recv_size = 0;
	}

	/* 越界检查：总数据不能超过2KB */
	if( (sg_http_update_param.http_response_recv_size + src_data_size) > (2*1024) ){ return(-1); }

	/* 追加数据到缓冲区，末尾补 '\0' 便于字符串操作 */
	memcpy( (void *)(sg_http_update_param.http_response_buff + sg_http_update_param.http_response_recv_size), (void *)src_data, src_data_size );
	sg_http_update_param.http_response_recv_size += src_data_size;
	sg_http_update_param.http_response_buff[ sg_http_update_param.http_response_recv_size ] = 0;

	return(0);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_check_response_completed
*    功能说明: 检查 HTTP 响应是否接收完成
*    形    参: 无
*    返 回 值:  0  未完成(包头尚未完整，仍在接收中)
*               1  包头完整但 Body 尚未接收完成
*               2  完整接收
*              -1  数据异常(已收1KB仍无包头，或无 Content-Length)
*              -2  Body 长度超过 8KB 上限
*********************************************************************************************************
*/
int http_update_check_response_completed(void)
{
	int http_head_size=0;
	char *pt=NULL;
	int body_size = 0;

	/* 查找 HTTP 头部结束标记 "\r\n\r\n" */
	pt = strstr((char *)(sg_http_update_param.http_response_buff), "\r\n\r\n");
	if(!pt)
	{
		/* 已收到1KB数据仍未找到头部结束符，视为异常 */
		if(sg_http_update_param.http_response_recv_size >= 1024){ return(-1); }
		return(0);
	}
	pt += 4;
	http_head_size = ( pt - (char*)(sg_http_update_param.http_response_buff) );

	/* 从 HTTP 头部中提取 Content-Length */
	pt = strstr((char *)(sg_http_update_param.http_response_buff), "Content-Length:");
	if( !pt || (pt >= (char *)(sg_http_update_param.http_response_buff) + http_head_size) ){ return(-1); }
	pt+=15;
	while( ((*pt) == ' ') || ((*pt) == '\t') ){ pt++; }
	body_size = atoi(pt);
	if( body_size >= (8*1024) ){ return(-2); }  /* Body 超过限制 */

	/* 判断 Body 是否接收完整 */
	if( (sg_http_update_param.http_response_recv_size - http_head_size) < body_size){ return(1); }

	return(2);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_recv_http_response
*    功能说明: HTTP 响应接收循环：持续接收直到响应完成或超时
*    形    参: recv_func   传输层接收函数 int (*)(int *out_size)
*              close_func  传输层关闭连接函数 void (*)(void)
*              timeout_ms  超时时间(ms)
*              head_only   0=需要完整响应体  1=只需要响应头
*    返 回 值:  0  响应完整接收
*              -1  接收数据出错(已调用 close_func)
*              -2  接收超时(已调用 close_func)
*              -3  链路断开(已调用 close_func)
*    备    注: 供 update_lwip / update_gsm 共用，封装超时检测+完整性判断
*********************************************************************************************************
*/
static int http_update_recv_http_response(int (*recv_func)(int *out_size),
                                    void (*close_func)(void),
                                    int timeout_ms,
                                    int head_only)
{
	int ret = 0;
	int cur_recv_size = 0;
	bool be_timing = false;
	unsigned int begin_ticks = 0, end_ticks = 0;

	sg_http_update_param.http_response_recv_size = 0;

	while(true)
	{
		/* 调用传输层接收一包数据 */
		ret = recv_func(&cur_recv_size);
		if(ret == -3)
		{
			close_func();
			return(-3);
		}
		if(ret)
		{
			close_func();
			return(-1);
		}

		/* 本次未收到数据，启动/维持超时计时 */
		if(!cur_recv_size)
		{
			if(!be_timing)
			{
				be_timing = true;
				begin_ticks = xTaskGetTickCount();
			}
			else
			{
				end_ticks = xTaskGetTickCount();
				if( (end_ticks - begin_ticks) >= pdMS_TO_TICKS(timeout_ms) )
				{
					close_func();
					return(-2);
				}
			}
			vTaskDelay(pdMS_TO_TICKS(10));
			continue;
		}
		else{ be_timing = false; }

		/* 检查 HTTP 响应是否接收完成 */
		ret = http_update_check_response_completed();
		if(head_only)
		{
			/* HEAD 请求：只需接收到头部即可 */
			if(ret == 0){ vTaskDelay(pdMS_TO_TICKS(10)); continue; }
			else{ break; }
		}
		else
		{
			/* GET 请求：需要完整响应体 */
			if(ret != 2){ continue; }
			else{ break; }
		}
	}

	return(0);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_build_info_txt_request
*    功能说明: 构造获取 info.txt 的 HTTP GET 请求
*    形    参: buf       输出缓冲区
*              buf_size  缓冲区大小
*              host      服务器地址
*              port      服务器端口
*    返 回 值:  请求长度  成功
*              -1        参数或缓冲区异常
*********************************************************************************************************
*/
int http_update_build_info_txt_request(char *buf, int buf_size, const char *host, uint16_t port)
{
	char *append_pt = buf;
	int len = 0;

	if(!buf || !host || (buf_size < 64)){ return(-1); }

	sprintf(append_pt, "GET /%s/info.txt HTTP/1.1\r\n", HARD_NO_STR);
	append_pt += strlen(append_pt);
	sprintf(append_pt, "Host: %s:%d\r\n\r\n", host, port);
	append_pt += strlen(append_pt);

	len = (int)(append_pt - buf);
	if(len >= buf_size){ return(-1); }
	return(len);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_build_head_request
*    功能说明: 构造获取文件大小的 HTTP HEAD 请求
*    形    参: buf       输出缓冲区
*              buf_size  缓冲区大小
*              host      服务器地址
*              port      服务器端口
*    返 回 值:  请求长度  成功
*              -1        参数或缓冲区异常
*********************************************************************************************************
*/
int http_update_build_head_request(char *buf, int buf_size, const char *host, uint16_t port)
{
	char *append_pt = buf;
	int len = 0;

	if(!buf || !host || (buf_size < 64)){ return(-1); }

	sprintf(append_pt, "HEAD %s HTTP/1.1\r\n", sg_http_update_param.http_url);
	append_pt += strlen(append_pt);
	sprintf(append_pt, "Host: %s:%d\r\n\r\n", host, port);
	append_pt += strlen(append_pt);

	len = (int)(append_pt - buf);
	if(len >= buf_size){ return(-1); }
	return(len);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_build_range_request
*    功能说明: 构造分块下载 crc.bin 的 HTTP GET(Range) 请求
*    形    参: buf       输出缓冲区
*              buf_size  缓冲区大小
*              host      服务器地址
*              port      服务器端口
*    返 回 值:  请求长度  成功
*              -1        参数或缓冲区异常
*    备    注: 使用 sg_http_update_param.section_current 计算 Range 范围
*********************************************************************************************************
*/
int http_update_build_range_request(char *buf, int buf_size, const char *host, uint16_t port)
{
	char *append_pt = buf;
	int len = 0;
	unsigned int download_start = 0, download_end = 0;

	if(!buf || !host || (buf_size < 128)){ return(-1); }

	download_start = (sg_http_update_param.section_current * UPDATE_CHUNK_SIZE);
	download_end = (download_start + UPDATE_CHUNK_SIZE - 1);

	sprintf(append_pt, "GET %s HTTP/1.1\r\n", sg_http_update_param.http_url);
	append_pt += strlen(append_pt);
	sprintf(append_pt, "Host: %s:%d\r\n", host, port);
	append_pt += strlen(append_pt);
	sprintf(append_pt, "Range: bytes=%d-%d\r\n\r\n", download_start, download_end);
	append_pt += strlen(append_pt);

	len = (int)(append_pt - buf);
	if(len >= buf_size){ return(-1); }
	return(len);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_finish_get_info_txt
*    功能说明: info.txt 公共收尾：接收响应、解析版本、提取升级 URL
*    形    参: recv_func  传输层接收函数
*              close_func 传输层关闭连接函数
*    返 回 值:  0/1/2  同 http_update_chack_version
*              -3/-5/-6 出错
*********************************************************************************************************
*/
int http_update_finish_get_info_txt(int (*recv_func)(int *out_size),
                                    void (*close_func)(void))
{
	int ret = 0, res = 0;

	ret = http_update_recv_http_response(recv_func, close_func, 10000, 0);
	if(ret)
	{
		if(ret == -2){ return(-6); }
		return(-3);
	}

	close_func();

	ret = http_update_chack_version();
	if(ret < 0){ return(-5); }

	if(ret == 1)
	{
		res = http_update_get_url();
		if(res){ return(-6); }
	}

	return(ret);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_finish_get_crc_bin_size
*    功能说明: crc.bin 文件大小查询公共收尾：接收 HEAD 响应并解析 Content-Length
*    形    参: recv_func  传输层接收函数
*              close_func 传输层关闭连接函数
*    返 回 值:  0  成功
*              -5/-6/-7 出错
*********************************************************************************************************
*/
int http_update_finish_get_crc_bin_size(int (*recv_func)(int *out_size),
                                         void (*close_func)(void))
{
	int ret = 0;

	ret = http_update_recv_http_response(recv_func, close_func, 10000, 1);
	if(ret)
	{
		if(ret == -2){ return(-6); }
		return(-5);
	}

	close_func();

	ret = http_update_get_crc_bin_size(NULL);
	if(ret < 0){ return(-7); }

	return(0);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_recv_parse_one_chunk
*    功能说明: 接收并解析单个 crc.bin 分块(完整 HTTP 响应 + CRC 校验 + 写 Flash)
*    形    参: recv_func  传输层接收函数
*              close_func 传输层关闭连接函数
*    返 回 值:  0   本分块成功
*               1   CRC 校验失败，需重试当前分块
*              -1   链路异常或超时，需重连
*              -2   其它致命错误
*********************************************************************************************************
*/
int http_update_recv_parse_one_chunk(int (*recv_func)(int *out_size),
                                      void (*close_func)(void))
{
	int ret = 0;

	ret = http_update_recv_http_response(recv_func, close_func, 10000, 0);
	if(ret == -3 || ret == -2){ return(-1); }
	if(ret == -1){ return(-2); }

	printf("\n Bulk: %u/%u\n", sg_http_update_param.section_current, sg_http_update_param.section_total);

	ret = http_update_parse_crc_bin_data();
	if(ret == -5){ return(1); }
	if(ret){ return(-2); }

	return(0);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_chack_version
*    功能说明: 解析 info.txt 响应，比对版本号并提取升级包 URL
*    形    参: 无
*    返 回 值:  0    版本一致，无需升级
*               1    版本不一致，需要升级
*              -1    非 200 OK 响应
*              -2    未找到 HTTP Body
*              -3    未找到 "version" 字段
*              -4/-5 版本号格式错误
*              -6    版本号长度异常 或 未找到 "url" 字段
*              -7/-8 url 格式错误
*              -9    url 长度异常
*              -10   未知错误(不应到达)
*    备    注: 设备版本号格式: HARD_NO_STR-SOFT_NO_STR
*********************************************************************************************************
*/
int http_update_chack_version(void)
{
	char *str, *pt1, *pt2, *http_body = NULL;
	int ret = 0, version_str_len = 0, url_len = 0;
	char dev_string[50]= {0};
	sprintf(dev_string,"%s-%s",HARD_NO_STR,SOFT_NO_STR);  /* 组装本地设备版本号 */

	/* 验证 HTTP 状态码是否为 200 OK */
	ret = strncmp( (char*)(sg_http_update_param.http_response_buff), "HTTP/1.1 200 OK\r\n", 17);
	if(ret){ return(-1); }

	/* 定位 HTTP Body 起始位置 */
	http_body = strstr( (char*)(sg_http_update_param.http_response_buff), "\r\n\r\n" );
	if(!http_body){ return(-2); }
	http_body += 4;

	/* 解析 JSON: "version":"xxx" */
	str = strstr(http_body, "\"version\":");
	if(!str){ return(-3); }

	pt1 = str + 10;
	while( ((*pt1) == ' ') || ((*pt1) == '\t') ){ pt1++; }
	if( (*pt1) != '\"' ){ return(-4); }
	pt1++;                              /* 跳过前引号，指向版本号起始 */

	pt2 = strchr(pt1, '\"');            /* 查找版本号结束引号 */
	if(!pt2){ return(-5); }

	version_str_len = (int)(pt2 - pt1);
	if( !version_str_len || (version_str_len >= sizeof(sg_http_update_param.update_version)) ){ return(-6); }

	/* 保存版本号 */
	memset( sg_http_update_param.update_version, 0, sizeof(sg_http_update_param.update_version) );
	memcpy(sg_http_update_param.update_version, pt1, version_str_len);

	/* 解析 JSON: "url":"xxx" */
	str = strstr(http_body, "\"url\":");
	if(!str){ return(-6); }

	pt1 = str + 6;
	while( ((*pt1) == ' ') || ((*pt1) == '\t') ){ pt1++; }
	if( (*pt1) != '\"' ){ return(-7); }
	pt1++;                              /* 跳过前引号，指向 URL 起始 */

	pt2 = strchr(pt1, '\"');            /* 查找 URL 结束引号 */
	if(!pt2){ return(-8); }

	url_len = (int)(pt2 - pt1);
	if( !url_len || (url_len >= sizeof(sg_http_update_param.update_url)) ){ return(-9); }

	/* 保存升级包 URL */
	memset(sg_http_update_param.update_url, 0, sizeof(sg_http_update_param.update_url));
	memcpy(sg_http_update_param.update_url, pt1, url_len);

	/* 比对版本号：0 表示相同，非 0 表示不同 */
	ret = strcmp(dev_string, sg_http_update_param.update_version);
	if(ret){ return(1); }
	else if(!ret){ return(2); }

	return(-10);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_get_url
*    功能说明: 解析升级 URL，提取主机地址、端口号和文件路径
*    形    参: 无
*    返 回 值:  0  成功
*              -1  不支持 HTTPS
*              -2  无端口号且无 '/' 分隔符
*              -3/-4 主机名过长
*              -5  端口号非法
*              -6  带端口号时缺少路径 '/'
*              -8  路径长度异常
*    备    注: 支持格式: http://host:port/path 或 http://host/path
*              未指定主机时使用默认服务器 IP
*********************************************************************************************************
*/
int http_update_get_url(void)
{
	char *scan_pt = NULL;
	char *pt1, *pt2;
	unsigned int len = 0, port_val = 0;
	update_param_t *updateparam = NULL;

	/* 跳过协议前缀 http:// (不支持 https) */
	if(!strncmp(sg_http_update_param.update_url, "http://", 7)){ scan_pt = sg_http_update_param.update_url + 7; }
	else if(!strncmp(sg_http_update_param.update_url, "https://", 8)){ return(-1); }
	else{ scan_pt = sg_http_update_param.update_url; }

	pt1 = scan_pt;
	pt2 = strchr(pt1, ':');
	if(!pt2)  /* 无端口号，使用默认端口80 */
	{
		pt2 = strchr(pt1, '/');
		if(!pt2){ return(-2); }

		len = (unsigned int)(pt2 - pt1);
		if(!len)  /* 主机名为空，使用默认服务器 IP */
		{
			updateparam = update_get_infor_data_function();
			sprintf(sg_http_update_param.http_host, "%d.%d.%d.%d", updateparam->ip[0], updateparam->ip[1], updateparam->ip[2], updateparam->ip[3]);
		}
		else  /* 保存域名/IP */
		{
			if( len >= sizeof(sg_http_update_param.http_host) ){ return(-3); }
			memset(sg_http_update_param.http_host, 0, sizeof(sg_http_update_param.http_host));
			memcpy(sg_http_update_param.http_host, pt1, len);
		}

		sg_http_update_param.http_port = 80;  /* 默认 HTTP 端口 */
		scan_pt = pt2;
	}
	else  /* 有端口号 */
	{
		len = (unsigned int)(pt2 - pt1);
		if(!len)  /* 主机名为空 */
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

		/* 解析端口号 */
		port_val = atoi(pt2 + 1);
		if(!port_val || (port_val >= 0xFFFF)){ return(-5); }
		sg_http_update_param.http_port = (unsigned short)port_val;

		pt1 = pt2 + 1;
		pt1 = strchr(pt1, '/');  /* 跳过端口号，查找路径起始 '/' */
		if(!pt1){ return(-6); }
		scan_pt = pt1;
	}

	/* 保存 URL 路径部分 */
	pt1 = scan_pt;
	len = (unsigned int)strlen(pt1);
	if( !len || (len >= sizeof(sg_http_update_param.http_url)) ){ return(-8); }
	memset(sg_http_update_param.http_url, 0, sizeof(sg_http_update_param.http_url));
	memcpy(sg_http_update_param.http_url, pt1, len);

	return(0);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_get_crc_bin_size
*    功能说明: 从 HTTP 响应中提取 crc.bin 文件大小，计算分块参数
*    形    参: file_size  输出 文件总大小
*    返 回 值:  0  成功
*              -1  非 200 OK 响应
*              -2  未找到 Content-Length 字段
*              -3  文件大小不是 UPDATE_CHUNK_SIZE 的整数倍
*********************************************************************************************************
*/
int http_update_get_crc_bin_size(unsigned int *file_size)
{
	char *str;
	int ret = 0;
	unsigned int len = 0;

	/* 验证 HTTP 状态码 */
	ret = strncmp( (char*)(sg_http_update_param.http_response_buff), "HTTP/1.1 200 OK\r\n", 17);
	if(ret){ return(-1); }

	/* 提取 Content-Length */
	str = strstr( (char*)(sg_http_update_param.http_response_buff), "Content-Length:" );
	if(!str){ return(-2); }
	str += 15;

	while( ((*str) == ' ') || ((*str) == '\t') ){ str++; }

	len = (unsigned int)atol(str);

	if(file_size){ (*file_size) = len; }

	/* 计算分块参数：每块数据去掉2字节 CRC 校验 */
	sg_http_update_param.crcfile_length = len;
	sg_http_update_param.section_len = (UPDATE_CHUNK_SIZE - 2);
	if(len % UPDATE_CHUNK_SIZE){ return(-3); }  /* 文件大小必须为块大小的整数倍 */
	sg_http_update_param.section_total = (len / UPDATE_CHUNK_SIZE);
	sg_http_update_param.section_current = 0;

	return(0);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_parse_crc_bin_data
*    功能说明: 解析分段下载的 crc.bin 数据块，校验 CRC 后写入 SPI Flash
*    形    参: 无
*    返 回 值:  0  成功
*              -1  非 206 Partial Content 响应
*              -2  未找到 Content-Length 字段
*              -3  Content-Length 与预期块大小不一致
*              -4  未找到 HTTP Body
*              -5  CRC16 校验失败
*    备    注: 每次调用处理一个分块，分块末尾 2 字节为 CRC16 校验值
*********************************************************************************************************
*/
int http_update_parse_crc_bin_data(void)
{
	char *pt = NULL;
	int ret = 0;
	unsigned int len = 0;
	unsigned char *body_pt = NULL;
	uint16_t count_crc = 0, section_crc = 0;
	unsigned int write_addr = UPDATA_SPIFLASH_ADDR;

	/* 验证 HTTP 响应码(分块下载使用 206) */
	ret = strncmp( (char*)(sg_http_update_param.http_response_buff), "HTTP/1.1 206 Partial Content", 28);
	if(ret){ return(-1); }

	/* 提取 Content-Length 并校验长度 */
	pt = strstr( (char*)(sg_http_update_param.http_response_buff), "Content-Length:" );
	if(!pt){ return(-2); }
	pt += 15;

	while( ((*pt) == ' ') || ((*pt) == '\t') ){ pt++; }

	len = (unsigned int)atol(pt);
	if(len != UPDATE_CHUNK_SIZE){ return(-3); }

	/* 定位 HTTP Body */
	pt = strstr(pt, "\r\n\r\n");
	if(!pt){ return(-4); }
	body_pt = (unsigned char *)(pt + 4);

	/* CRC16 校验：计算数据体 CRC 并与包尾 2 字节校验值比对 */
	count_crc = CRC16_MODBUS(body_pt, (UPDATE_CHUNK_SIZE-2));
	section_crc = ( (body_pt[UPDATE_CHUNK_SIZE - 2] << 8) | (body_pt[UPDATE_CHUNK_SIZE - 1]) );
	if(count_crc != section_crc){ return(-5); }

	/* 写入 SPI Flash 对应偏移位置(临界区保护) */
	write_addr = UPDATA_SPIFLASH_ADDR + (sg_http_update_param.section_current * sg_http_update_param.section_len);
	taskENTER_CRITICAL();
	{
		sf_WriteBuffer(body_pt, write_addr, sg_http_update_param.section_len);
	}
	taskEXIT_CRITICAL();

	(sg_http_update_param.section_current)++;

	return(0);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_success_reboot
*    功能说明: 升级成功后写入 Boot 参数并重启系统
*    形    参: 无
*    返 回 值: 无
*    备    注: 保存分块信息到 SPI Flash 参数区，卸载文件系统后软复位进入 Boot 升级流程
*********************************************************************************************************
*/
void http_update_success_reboot(void)
{
	struct BOOT_UPDATE_PARAM boot_update_param = {0};

	/* 设置 Boot 升级参数：标记需要升级并记录分块信息 */
	boot_update_param.is_update = true;
	boot_update_param.section_count = sg_http_update_param.section_total;
	boot_update_param.section_size = sg_http_update_param.section_len;

	taskENTER_CRITICAL();
	{
		sf_WriteBuffer((uint8_t *)(&boot_update_param), UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM));
	}
	taskEXIT_CRITICAL();

	/* 卸载文件系统后软复位进入 Boot 升级流程 */
	lfs_unmount(&g_lfs_t);

	app_system_softreset();
}

/*
*********************************************************************************************************
*    函 数 名: http_update_failed
*    功能说明: 升级失败时写入失败状态到 Boot 参数区
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void http_update_failed(void)
{
	struct BOOT_UPDATE_PARAM boot_update_param = {0};

	boot_update_param.is_update = false;
	boot_update_param.section_count = 0;
	boot_update_param.section_size = 0;
	boot_update_param.update_status = UPDATE_FAILED;
	sf_WriteBuffer((uint8_t *)(&boot_update_param), UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM));
}

/*
*********************************************************************************************************
*    函 数 名: http_update_clear_param
*    功能说明: 清除 Boot 参数区升级标记，恢复为无升级状态
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void http_update_clear_param(void)
{
	struct BOOT_UPDATE_PARAM boot_update_param = {0};

	boot_update_param.is_update = false;
	boot_update_param.section_count = 0;
	boot_update_param.section_size = 0;
	boot_update_param.update_status = UPDATE_NONE;
	sf_WriteBuffer((uint8_t *)(&boot_update_param), UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM));
}
