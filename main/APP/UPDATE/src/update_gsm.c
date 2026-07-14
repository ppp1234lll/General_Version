#include "main.h"
#include "./UPDATE/inc/update.h"
#include "./UPDATE/inc/update_http.h"

/*
*********************************************************************************************************
*    文 件 名: update_gsm.c
*    功能说明: 无线(GPRS) OTA 升级 HTTP 传输与 FreeRTOS 后台任务
*    说    明: HTTP 应答解析等公共逻辑见 update_http.c,本文件负责 GPRS 链路连接/收发
*********************************************************************************************************
*/
#define UPDATE_GSM_TASK_PRIO            (9U)
#define UPDATE_GSM_TASK_STK             (4096U)
static TaskHandle_t s_update_gsm_task = NULL;
static volatile uint8_t s_update_gsm_exit_req = 0;  /* 1: 任务已停到安全点, 待外部同步删除 */

/* GPRS 模块内部静态函数声明 */
static int http_update_connect_server_by_gprs(const char *host, unsigned short port, unsigned int retry_delay_ms);
static int http_update_send_request_for_info_txt_by_gprs(ip_addr_t *server_ipaddr, uint16_t server_port);
static int http_update_recv_reponse_by_gprs(int *out_recv_size);
static int http_update_send_request_for_crcbin_file_size_by_gprs(const char *host, uint16_t server_port);
static int http_update_send_request_for_crcbin_data_by_gprs(const char *host, uint16_t server_port);

/*
*********************************************************************************************************
*    函 数 名: update_gsm_task
*    功能说明: GPRS 无线 OTA FreeRTOS 后台任务,执行升级流程后自删除
*    形    参: pvParameters 未使用
*    返 回 值: 无
*    备    注: 先断开旧 GPRS 连接重新开始，失败时写入失败状态到 Boot 参数区
*********************************************************************************************************
*/
static void update_gsm_task(void *pvParameters)
{
	struct update_addr *param = app_get_http_ota_function();
	int8_t ret = 0;
	ip_addr_t server_ipaddr;
	uint16_t server_port;

	(void)pvParameters;

	/* 每次更新前从系统配置同步最新 IP、端口 */
	update_set_update_addr();

	/* 检查 GPRS 模块是否就绪 */
	if(gprs_get_module_status_function() != 1)
	{
		goto UPDATE_END;
	}

	/* 断开旧连接，关闭 GPRS 指示灯 */
	gprs_network_disconnect_function(GPRS_LINK_OTA);
	led_control_function(LD_GPRS,LD_OFF);

	server_port = param->port;
	IP4_ADDR(&server_ipaddr, param->ip[0], param->ip[1], param->ip[2], param->ip[3]);

	/* 初始化分块参数 */
	sg_http_update_param.section_len = (UPDATE_CHUNK_SIZE - 2);
	sg_http_update_param.http_response_recv_size = 0;

	/* 步骤1: 通过 GPRS 获取 info.txt，比对版本号 */
	ret = http_update_get_info_txt_by_gprs(&server_ipaddr, server_port);
	if( (ret < 0) || (ret == 2) )
	{
		if(ret < 0){ printf("\nGet info.txt failed! ret: %d\n", ret); }
		else{ printf("\nAlready latest version, no update needed!\n"); }
		goto UPDATE_END;
	}

	/* 步骤2: 通过 GPRS 获取 crc.bin 文件大小 */
	ret = http_update_get_crc_bin_file_size_by_gprs();
	if(ret < 0)
	{
		printf("\nGet crc_bin file size failed! ret: %d\n", ret);
		goto UPDATE_END;
	}

	/* 步骤3: 通过 GPRS 分块下载 crc.bin 文件数据 */
	ret = http_update_get_crc_bin_file_data_by_gprs();
	if(ret < 0)
	{
		printf("\nGet crc_bin file content failed! ret: %d\n", ret);
		goto UPDATE_END;
	}

	/* 升级完成，保存参数并重启 */
	printf("\nUpdate done, restarting device...\n");
	http_update_success_reboot();

	ret = 0;

UPDATE_END:
	if(ret < 0){ 
		http_update_failed(); 
		app_set_reply_parameters_function(CONFIGURE_UPDATE_SYSTEM, 0x00);  // 立即通知平台升级失败
		http_update_clear_param();  // 清除Flash状态, 防止重启后 update_status_detection 重复发送
	}

	/* 任务结束：清除模式、释放句柄、自删除 */
	if(update_get_mode_function() != UPDATE_MODE_NULL)
	{
		update_set_update_mode(UPDATE_MODE_NULL);
	}
	/* 不自删除(自删除的栈/TCB 会推迟到空闲任务回收, 频繁重建易堆积/碎片, 最终
	 * xTaskCreate 内存不足)。改为置退出请求并挂起, 由 gsm 任务调用 update_gsm_delete()
	 * 在其它任务上下文同步删除, 立即回收栈/TCB */
	s_update_gsm_exit_req = 1;
	for(;;)
	{
		vTaskSuspend(NULL);
	}
}

/*
*********************************************************************************************************
*                               GPRS HTTP 底层函数
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*    函 数 名: http_update_connect_server_by_gprs
*    功能说明: 通过 GPRS 连接服务器，支持重试
*    形    参: host            服务器地址(IP 或域名)
*              port            服务器端口
*              retry_delay_ms  重试间隔(ms)
*    返 回 值:  0  连接成功
*              -1  3次重试后仍失败
*********************************************************************************************************
*/
static int http_update_connect_server_by_gprs(const char *host, unsigned short port, unsigned int retry_delay_ms)
{
	update_param_t *updateparam = NULL;
	unsigned char index = 0;
	int ret = 0;

	updateparam = update_get_infor_data_function();

	/* 先断开旧连接，确保干净状态再重试 */
	gprs_network_disconnect_function(GPRS_LINK_OTA);
	updateparam->gprs_t.connect = 0;

	/* 最多重试 3 次 */
	for(index=0; index<3; index++)
	{
		ret = gprs_network_connect_server((uint8_t *)host, port, GPRS_LINK_OTA);
		if(ret == GPRS_SEND_OK)
		{
			updateparam->gprs_t.connect = 1;
			return 0;
		}
		GPRS_DELAY_MS(retry_delay_ms);
	}
	updateparam->gprs_t.connect = 0;
	return(-1);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_close_connect_by_gprs
*    功能说明: 断开 GPRS OTA 连接并清除连接状态
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void http_update_close_connect_by_gprs(void)
{
	update_param_t *updateparam = NULL;
	gprs_network_disconnect_function(GPRS_LINK_OTA);
	updateparam = update_get_infor_data_function();
	updateparam->gprs_t.connect = 0;
}

/*
*********************************************************************************************************
*    函 数 名: http_update_recv_reponse_by_gprs
*    功能说明: 通过 GPRS 接收 HTTP 响应数据并保存到缓冲区
*    形    参: out_recv_size  输出 本次接收数据大小
*    返 回 值:  0  成功(含暂无数据)
*              -3  链路断开
*              -2  保存数据失败
*********************************************************************************************************
*/
static int http_update_recv_reponse_by_gprs(int *out_recv_size)
{
	int ret = 0;
	const unsigned char *recv_data = NULL;
	int recv_data_size = 0;

	if(out_recv_size){ (*out_recv_size) = 0; }

	/* 从 GPRS 模块读取数据；暂无数据时 gprs_recv_data 返回 GPRS_SEND_ERROR，需继续轮询 */
	ret = gprs_recv_data(GPRS_LINK_OTA, &recv_data, &recv_data_size);
	if(ret == GPRS_SEND_DISCONN){ return(-3); }
	if(ret != GPRS_SEND_OK){ return(0); }

	if(!recv_data || !recv_data_size){ return(0); }

	/* 将数据追加到 HTTP 应答缓冲区 */
	ret = http_update_save_response(recv_data, recv_data_size);
	if(ret){ return(-2); }

	if(out_recv_size){ (*out_recv_size) = recv_data_size; }
	return(0);
}

/*
*********************************************************************************************************
*                               GPRS HTTP 业务函数
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*    函 数 名: http_update_send_request_for_info_txt_by_gprs
*    功能说明: 通过 GPRS 发送获取 info.txt 的 HTTP GET 请求
*    形    参: server_ipaddr  服务器 IP 地址
*              server_port    服务器端口
*    返 回 值: GPRS_SEND_OK  发送成功
*              其他          发送失败
*********************************************************************************************************
*/
static int http_update_send_request_for_info_txt_by_gprs(ip_addr_t *server_ipaddr, uint16_t server_port)
{
	char send_buf[256]={0};
	int len = 0;

	len = http_update_build_info_txt_request(send_buf, sizeof(send_buf), ipaddr_ntoa(server_ipaddr), server_port);
	if(len < 0){ return(GPRS_SEND_ERROR); }

	return gprs_send_data( (uint8_t *)send_buf, len, GPRS_LINK_OTA, 1000 );
}

/*
*********************************************************************************************************
*    函 数 名: http_update_get_info_txt_by_gprs
*    功能说明: 通过 GPRS 获取 info.txt 并解析版本信息
*    形    参: server_ipaddr  服务器 IP 地址
*              server_port    服务器端口
*    返 回 值:  0/1  成功(1=需升级)
*              2    版本已是最新
*             -1    连接服务器失败
*             -2    发送请求失败
*             -3/-6 接收超时
*             -5    版本解析失败
*             -6    URL 解析失败
*********************************************************************************************************
*/
int http_update_get_info_txt_by_gprs(ip_addr_t *server_ipaddr, uint16_t server_port)
{
	int ret = 0;

	/* 连接服务器 */
	printf("\nGPRS connecting server %s:%d ...\n", ipaddr_ntoa(server_ipaddr), server_port);
	ret = http_update_connect_server_by_gprs(ipaddr_ntoa(server_ipaddr), server_port, 1000);
	if(ret){ return(-1); }

	led_control_function(LD_GPRS, LD_FLICKER);

	/* 发送 HTTP 请求 */
	ret = http_update_send_request_for_info_txt_by_gprs(server_ipaddr, server_port);
	if(ret != GPRS_SEND_OK)
	{
		http_update_close_connect_by_gprs();
		return(-2);
	}

	ret = http_update_finish_get_info_txt(http_update_recv_reponse_by_gprs,
	                                       http_update_close_connect_by_gprs);
	led_control_function(LD_LAN, LD_OFF);
	return(ret);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_send_request_for_crcbin_file_size_by_gprs
*    功能说明: 通过 GPRS 发送 HEAD 请求获取 crc.bin 文件大小
*    形    参: host        服务器地址
*              server_port 服务器端口
*    返 回 值: GPRS_SEND_OK  发送成功
*              其他          发送失败
*********************************************************************************************************
*/
static int http_update_send_request_for_crcbin_file_size_by_gprs(const char *host, uint16_t server_port)
{
	char send_buf[256]={0};
	int len = 0;

	len = http_update_build_head_request(send_buf, sizeof(send_buf), host, server_port);
	if(len < 0){ return(GPRS_SEND_ERROR); }

	return gprs_send_data( (uint8_t *)send_buf, len, GPRS_LINK_OTA, 1000 );
}

/*
*********************************************************************************************************
*    函 数 名: http_update_get_crc_bin_file_size_by_gprs
*    功能说明: 通过 GPRS 获取 crc.bin 文件大小并计算分块信息
*    形    参: 无
*    返 回 值:  0  成功
*              -1  连接失败
*              -4  发送请求失败
*              -5/-6 接收超时
*              -7  解析 Content-Length 失败
*********************************************************************************************************
*/
int http_update_get_crc_bin_file_size_by_gprs(void)
{
	int ret = 0;

	/* 连接服务器 */
	printf("\nGPRS connecting server %s:%d ...\n", sg_http_update_param.http_host, sg_http_update_param.http_port);
	ret = http_update_connect_server_by_gprs(sg_http_update_param.http_host, sg_http_update_param.http_port, 50);
	if(ret){ return(-1); }
	led_control_function(LD_GPRS, LD_FLICKER);

	/* 发送 HEAD 请求 */
	ret = http_update_send_request_for_crcbin_file_size_by_gprs( sg_http_update_param.http_host, sg_http_update_param.http_port );
	if(ret != GPRS_SEND_OK)
	{
		http_update_close_connect_by_gprs();
		return(-4);
	}

	ret = http_update_finish_get_crc_bin_size(http_update_recv_reponse_by_gprs,
	                                           http_update_close_connect_by_gprs);
	led_control_function(LD_LAN, LD_OFF);
	return(ret);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_send_request_for_crcbin_data_by_gprs
*    功能说明: 通过 GPRS 发送 Range 分块下载请求获取 crc.bin 指定数据段
*    形    参: host        服务器地址
*              server_port 服务器端口
*    返 回 值: GPRS_SEND_OK  发送成功
*              其他          发送失败
*    备    注: 使用 HTTP Range 头部实现断点续传式分块下载
*********************************************************************************************************
*/
static int http_update_send_request_for_crcbin_data_by_gprs(const char *host, uint16_t server_port)
{
	char send_buf[256]={0};
	int len = 0;

	len = http_update_build_range_request(send_buf, sizeof(send_buf), host, server_port);
	if(len < 0){ return(GPRS_SEND_ERROR); }

	return gprs_send_data( (uint8_t *)send_buf, len, GPRS_LINK_OTA, 1000 );
}

/*
*********************************************************************************************************
*    函 数 名: http_update_get_crc_bin_file_data_by_gprs
*    功能说明: 通过 GPRS 循环分块下载 crc.bin 全部数据
*    形    参: 无
*    返 回 值:  0  下载成功
*              -1  连接重试超过 10 次
*              -2  接收数据异常
*              -3  CRC 校验连续失败超过 10 次
*              -4  数据解析失败
*    备    注: 支持断线重连(RECONNECT)，CRC 校验失败时重试当前分块
*********************************************************************************************************
*/
int http_update_get_crc_bin_file_data_by_gprs(void)
{
	int ret = 0;
	unsigned int crc_check_err_times = 0, connect_times = 0;

	sg_http_update_param.section_current = 0;

RECONNECT:
	/* 连接服务器，支持断线重连(最多 10 次) */
	printf("\nGPRS connecting server %s:%d ...\n", sg_http_update_param.http_host, sg_http_update_param.http_port);
	ret = http_update_connect_server_by_gprs(sg_http_update_param.http_host, sg_http_update_param.http_port, 50);
	if(ret)
	{
		connect_times++;
		if(connect_times > 10){ return(-1); }

		/* 检查 CEREG 网络注册状态，若不正常则等待恢复(最多 20s) */
		{
			unsigned int cereg_wait_s = 0;

			while(gprs_network_status_monitoring_function() != 0)
			{
				GPRS_DELAY_MS(1000);
				cereg_wait_s++;
				if(cereg_wait_s >= 20){ return(-1); }
			}
		}

		goto RECONNECT;
	}
	connect_times = 0;
	led_control_function(LD_GPRS, LD_FLICKER);

	/* 循环下载每个分块 */
	while(sg_http_update_param.section_current < sg_http_update_param.section_total)
	{
		ret = http_update_send_request_for_crcbin_data_by_gprs( sg_http_update_param.http_host, sg_http_update_param.http_port );
		if(ret != GPRS_SEND_OK)
		{
			http_update_close_connect_by_gprs();
			goto RECONNECT;
		}

		ret = http_update_recv_parse_one_chunk(http_update_recv_reponse_by_gprs,
		                                        http_update_close_connect_by_gprs);
		if(ret == -1)
		{
			goto RECONNECT;
		}
		else if(ret == -2)
		{
			return(-2);
		}
		else if(ret == 1)
		{
			crc_check_err_times++;
			if(crc_check_err_times > 10){ return(-3); }
			continue;
		}

		crc_check_err_times = 0;
	}

	http_update_close_connect_by_gprs();
	led_control_function(LD_GPRS, LD_OFF);

	return(0);
}

/* ======================== 无线 OTA 后台任务管理 ======================== */

/*
*********************************************************************************************************
*    函 数 名: update_gsm_task_create
*    功能说明: 无线 OTA 后台任务创建(由主循环调用)
*    形    参: 无
*    返 回 值: 无
*    备    注: 检测到 GPRS 升级模式且任务未运行时,创建后台任务
*********************************************************************************************************
*/
void update_gsm_task_create(void)
{
	BaseType_t ret;

	if(update_get_mode_function() != UPDATE_MODE_GPRS){ return; }
	if(s_update_gsm_exit_req){ return; }   /* 有任务待回收, 本轮先不创建 */
	if(s_update_gsm_task != NULL){ return; }

	ret = xTaskCreate(  update_gsm_task,
						"ota_gprs",
						UPDATE_GSM_TASK_STK,
						NULL,
						UPDATE_GSM_TASK_PRIO,
						&s_update_gsm_task);
	if(ret != pdPASS){ s_update_gsm_task = NULL; }
}

/*
*********************************************************************************************************
*    函 数 名: update_gsm_delete
*    功能说明: 回收已完成并停到安全点的无线OTA任务; 须由 gsm 任务等其它任务上下文周期调用,
*              以在外部上下文 vTaskDelete, 立即回收栈/TCB, 避免自删除延迟回收导致内存不足
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void update_gsm_delete(void)
{
	TaskHandle_t h;

	if(s_update_gsm_exit_req == 0){ return; }

	taskENTER_CRITICAL();
	h = s_update_gsm_task;
	s_update_gsm_task = NULL;
	s_update_gsm_exit_req = 0;
	taskEXIT_CRITICAL();

	if(h != NULL){ vTaskDelete(h); }
}

