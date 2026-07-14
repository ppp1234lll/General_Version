#include "main.h"
#include "./UPDATE/inc/update.h"
#include "./UPDATE/inc/update_http.h"

/*
*********************************************************************************************************
*    文 件 名: update_lwip.c
*    功能说明: 有线(LWIP) OTA 升级 HTTP 传输与 FreeRTOS 后台任务
*    说    明: HTTP 应答解析等公共逻辑见 update_http.c,本文件负责 LWIP TCP 连接/收发
*********************************************************************************************************
*/
#define UPDATE_LWIP_TASK_PRIO            (9U)
#define UPDATE_LWIP_TASK_STK             (4096U)
static TaskHandle_t s_update_lwip_task = NULL;
static volatile uint8_t s_update_lwip_exit_req = 0;  /* 1: 任务已停到安全点, 待外部同步删除 */

/* LWIP 全局变量 */
struct netconn *tcp_update;

/* 内部函数声明 */
static int http_update_connect_server_by_lwip(ip_addr_t *ip, unsigned short port);
static int http_update_send_request_for_info_txt_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port);
static int http_update_recv_reponse_by_lwip(int *out_recv_size);
static int http_update_send_request_for_crcbin_file_size_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port);
static int http_update_send_request_for_crcbin_data_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port);
static void http_update_cb_server_ip(const char *name, const ip_addr_t *ipaddr, void *arg);


/*
*********************************************************************************************************
*    函 数 名: update_lwip_task
*    功能说明: LWIP 有线 OTA FreeRTOS 后台任务,执行升级流程后自删除
*    形    参: pvParameters 未使用
*    返 回 值: 无
*    备    注: 失败时写入失败状态到 Boot 参数区
*********************************************************************************************************
*/
static void update_lwip_task(void *pvParameters)
{
	ip_addr_t server_ipaddr;
	uint16_t  server_port;
	int8_t    ret = 0;
	struct update_addr *param = app_get_http_ota_function();

	(void)pvParameters;

	/* 每次更新前从系统配置同步最新 IP、端口 */
	update_set_update_addr();

	/* 初始化参数 */
	server_port = param->port;
	IP4_ADDR(&server_ipaddr, param->ip[0], param->ip[1], param->ip[2], param->ip[3]);

	sg_http_update_param.section_len = (UPDATE_CHUNK_SIZE - 2);
	sg_http_update_param.http_response_recv_size = 0;

	/* 步骤1: 获得 info.txt 信息 */
	ret = http_update_get_info_txt_by_lwip(&server_ipaddr, server_port);
	if( (ret < 0) || (ret == 2) )
	{
		if(ret < 0){ printf("\nGet info.txt failed! ret: %d\n", ret); }
		else{ printf("\nAlready latest version, no update needed!\n"); }
		goto UPDATE_END;
	}

	/* 步骤2: 获得 crc_bin 文件大小 */
	ret = http_update_get_crc_bin_file_size_by_lwip();
	if(ret < 0)
	{
		printf("\nGet crc_bin file size failed! ret: %d\n", ret);
		goto UPDATE_END;
	}

	/* 步骤3: 获得 crc_bin 文件数据 */
	ret = http_update_get_crc_bin_file_data_by_lwip();
	if(ret < 0)
	{
		printf("\nGet crc_bin file content failed! ret: %d\n", ret);
		goto UPDATE_END;
	}

	/* 升级完成，重启系统 */
	printf("\nUpdate done, restarting device...\n");
	http_update_success_reboot();
	ret = 0;

UPDATE_END:
	http_update_close_connect_by_lwip();

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
	 * xTaskCreate 内存不足)。改为置退出请求并挂起, 由 eth 任务调用 update_lwip_delete()
	 * 在其它任务上下文同步删除, 立即回收栈/TCB */
	s_update_lwip_exit_req = 1;
	for(;;)
	{
		vTaskSuspend(NULL);
	}
}

/******************************************************************************
 *  LWIP HTTP 底层函数
 ******************************************************************************/

/*
*********************************************************************************************************
*    函 数 名: http_update_connect_server_by_lwip
*    功能说明: 通过LWIP TCP连接服务器(最多重试3次)
*    形    参: ip   服务器IP地址
*              port 服务器端口
*    返 回 值: 0:成功  -1:连接失败
*********************************************************************************************************
*/
static int http_update_connect_server_by_lwip(ip_addr_t *ip, unsigned short port)
{
	unsigned char index = 0;
	err_t err;
	update_param_t *updateparam = NULL;

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
*    函 数 名: http_update_close_connect_by_lwip
*    功能说明: 关闭LWIP TCP连接并清理资源
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void http_update_close_connect_by_lwip(void)
{
	update_param_t *updateparam = NULL;
    
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
*    功能说明: 通过LWIP接收HTTP应答数据
*    形    参: out_recv_size 输出本次接收的字节数
*    返 回 值: 0:成功  -1:未连接  -2:缓冲溢出  -3:连接断开
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
			taskENTER_CRITICAL();           /* 进入临界区 */
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

		case ERR_TIMEOUT: // 暂无数据
			if(recvbuf){ netbuf_delete(recvbuf); recvbuf = NULL; }
		return(0);

		case ERR_CLSD: // 对端已经关闭
		default:
			if(recvbuf){ netbuf_delete(recvbuf); recvbuf = NULL; }
		return(-3);
	} // switch()
}

/*
*********************************************************************************************************
*    函 数 名: http_update_cb_server_ip
*    功能说明: DNS解析回调,将解析结果拷贝到输出参数
*    形    参: name   DNS域名
*              ipaddr 解析得到的IP地址
*              arg    输出参数(ip_addr_t*)
*    返 回 值: 无
*********************************************************************************************************
*/
static void http_update_cb_server_ip(const char *name, const ip_addr_t *ipaddr, void *arg)
{
	ip_addr_t *out_addr = (ip_addr_t *)arg;
	if( !ipaddr || !(ipaddr->addr) ){ return; }
	memcpy(out_addr, ipaddr, sizeof(ip_addr_t));
}

/******************************************************************************
 *  LWIP HTTP 业务函数
 ******************************************************************************/
/*
*********************************************************************************************************
*    函 数 名: http_update_send_request_for_info_txt_by_lwip
*    功能说明: 通过LWIP发送HTTP GET请求获取info.txt
*    形    参: server_ipaddr 服务器IP地址
*              server_port   服务器端口
*    返 回 值: ERR_OK:成功  其它:失败
*********************************************************************************************************
*/
static int http_update_send_request_for_info_txt_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port)
{
	char send_buf[256]={0};
	int len = 0;

	len = http_update_build_info_txt_request(send_buf, sizeof(send_buf), ipaddr_ntoa(server_ipaddr), server_port);
	if(len < 0){ return(ERR_ARG); }

	return netconn_write(tcp_update, send_buf, len, NETCONN_COPY);
}
/*
*********************************************************************************************************
*    函 数 名: http_update_get_info_txt_by_lwip
*    功能说明: 通过LWIP获取info.txt升级信息并检查版本
*    形    参: server_ipaddr 服务器IP地址
*              server_port   服务器端口
*    返 回 值: 1:版本号不同需要更新  2:版本号相同无需更新  <0:出错
*********************************************************************************************************
*/
int http_update_get_info_txt_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port)
{
	int ret = 0;

	// 连接服务器
	printf("\nLWIP connecting server %s:%d ...\n", ipaddr_ntoa(server_ipaddr), server_port);
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

	ret = http_update_finish_get_info_txt(http_update_recv_reponse_by_lwip,
	                                       http_update_close_connect_by_lwip);
	led_control_function(LD_LAN, LD_OFF);
	return(ret);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_send_request_for_crcbin_file_size_by_lwip
*    功能说明: 通过LWIP发送HTTP HEAD请求获取crc_bin文件大小
*    形    参: server_ipaddr 服务器IP地址
*              server_port   服务器端口
*    返 回 值: ERR_OK:成功  其它:失败
*********************************************************************************************************
*/
static int http_update_send_request_for_crcbin_file_size_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port)
{
	char send_buf[256]={0};
	int len = 0;

	len = http_update_build_head_request(send_buf, sizeof(send_buf), ipaddr_ntoa(server_ipaddr), server_port);
	if(len < 0){ return(ERR_ARG); }

	return netconn_write(tcp_update, send_buf, len, NETCONN_COPY);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_get_crc_bin_file_size_by_lwip
*    功能说明: 通过LWIP获取crc_bin文件大小(DNS解析+HEAD请求)
*    形    参: 无
*    返 回 值: 0:成功  <0:出错
*********************************************************************************************************
*/
int http_update_get_crc_bin_file_size_by_lwip(void)
{
	int ret = 0;
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
	printf("\nLWIP connecting server %s:%d ...\n", ipaddr_ntoa(&(sg_http_update_param.http_server_addr)), sg_http_update_param.http_port);
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

	ret = http_update_finish_get_crc_bin_size(http_update_recv_reponse_by_lwip,
	                                           http_update_close_connect_by_lwip);
	led_control_function(LD_LAN, LD_OFF);
	return(ret);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_send_request_for_crcbin_data_by_lwip
*    功能说明: 通过LWIP发送HTTP GET请求(Range分段)获取crc_bin数据块
*    形    参: server_ipaddr 服务器IP地址
*              server_port   服务器端口
*    返 回 值: ERR_OK:成功  其它:失败
*********************************************************************************************************
*/
static int http_update_send_request_for_crcbin_data_by_lwip(ip_addr_t *server_ipaddr, uint16_t server_port)
{
	char send_buf[256]={0};
	int len = 0;

	len = http_update_build_range_request(send_buf, sizeof(send_buf), ipaddr_ntoa(server_ipaddr), server_port);
	if(len < 0){ return(ERR_ARG); }

	return netconn_write(tcp_update, send_buf, len, NETCONN_COPY);
}

/*
*********************************************************************************************************
*    函 数 名: http_update_get_crc_bin_file_data_by_lwip
*    功能说明: 通过LWIP分段下载crc_bin文件数据并写入Flash
*    形    参: 无
*    返 回 值: 0:成功  <0:出错
*********************************************************************************************************
*/
int http_update_get_crc_bin_file_data_by_lwip(void)
{
	int ret = 0;
	unsigned int crc_check_err_times = 0, connect_times = 0;

	sg_http_update_param.section_current = 0;

RECONNECT:

	// 连接服务器
	printf("\nLWIP connecting server %s:%d ...\n", ipaddr_ntoa(&(sg_http_update_param.http_server_addr)), sg_http_update_param.http_port);
	ret = http_update_connect_server_by_lwip( &(sg_http_update_param.http_server_addr), sg_http_update_param.http_port );
	if(ret)
	{
		connect_times++;
		if(connect_times > 10){ return(-1); }
		goto RECONNECT;
	}
	connect_times = 0;
	led_control_function(LD_LAN, LD_FLICKER);

	// 循环请求、接收数据块
	while(sg_http_update_param.section_current < sg_http_update_param.section_total)
	{
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

		ret = http_update_recv_parse_one_chunk(http_update_recv_reponse_by_lwip,
		                                        http_update_close_connect_by_lwip);
		if(ret == -1)
		{
			goto RECONNECT;
		}
		else if(ret == -2)
		{
			return(-3);
		}
		else if(ret == 1)
		{
			crc_check_err_times++;
			if(crc_check_err_times > 10){ return(-3); }
			continue;
		}

		crc_check_err_times = 0;
	}

	http_update_close_connect_by_lwip();
	led_control_function(LD_LAN, LD_OFF);

	return(0);
}

/* ======================== 有线 OTA 后台任务管理 ======================== */

/*
*********************************************************************************************************
*    函 数 名: update_lwip_task_create
*    功能说明: 有线 OTA 后台任务创建(由主循环调用)
*    形    参: 无
*    返 回 值: 无
*    备    注: 检测到 LWIP 升级模式且任务未运行时,创建后台任务
*********************************************************************************************************
*/
void update_lwip_task_create(void)
{
	BaseType_t ret;

	if(update_get_mode_function() != UPDATE_MODE_LWIP){ return; }
	if(s_update_lwip_exit_req){ return; }   /* 有任务待回收, 本轮先不创建 */
	if(s_update_lwip_task != NULL){ return; }

	ret = xTaskCreate(  update_lwip_task,
	                    "ota_lwip",
	                    UPDATE_LWIP_TASK_STK,
	                    NULL,
	                    UPDATE_LWIP_TASK_PRIO,
	                    &s_update_lwip_task);
	if(ret != pdPASS){ s_update_lwip_task = NULL; }
}

/*
*********************************************************************************************************
*    函 数 名: update_lwip_delete
*    功能说明: 回收已完成并停到安全点的有线OTA任务; 须由 eth 任务等其它任务上下文周期调用,
*              以在外部上下文 vTaskDelete, 立即回收栈/TCB, 避免自删除延迟回收导致内存不足
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void update_lwip_delete(void)
{
	TaskHandle_t h;

	if(s_update_lwip_exit_req == 0){ return; }

	taskENTER_CRITICAL();
	h = s_update_lwip_task;
	s_update_lwip_task = NULL;
	s_update_lwip_exit_req = 0;
	taskEXIT_CRITICAL();

	if(h != NULL){ vTaskDelete(h); }
}
