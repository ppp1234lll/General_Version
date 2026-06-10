#include "appconfig.h"
#include "./UPDATE/inc/update_http.h"

int8_t update_lwip_task_function(void)
{
	ip_addr_t server_ipaddr;
	uint16_t  server_port;
	int8_t    ret = 0;
	update_param_t *updateparam = NULL;
	struct update_addr *param = app_get_http_ota_function();
	
	/* 初始化参数 */
	updateparam = update_get_infor_data_function();
	server_port = param->port;
	IP4_ADDR(&server_ipaddr, param->ip[0], param->ip[1], param->ip[2], param->ip[3]);

	sg_http_update_param.section_len = (UPDATE_CHUNK_SIZE - 2);
	sg_http_update_param.http_response_recv_size = 0;
	////

	// 1: 获得info.txt信息
	ret = http_update_get_info_txt_by_lwip(&server_ipaddr, server_port);
	if( (ret < 0) || (ret == 2) )
	{
		if(ret < 0){ printf("\n获得info.txt信息,失败! ret: %d\n", ret); }
		else{ printf("\n版本是最新版本,无需更新!\n"); }
		goto UPDATE_END;
	}

	// 2: 获得crc_bin文件大小
	ret = http_update_get_crc_bin_file_size_by_lwip();
	if(ret < 0)
	{
		printf("\n获得crc_bin文件大小,失败! ret: %d\n", ret);
		goto UPDATE_END;
	}
	
	// 3: 获得crc_bin文件数据
	ret = http_update_get_crc_bin_file_data_by_lwip();
	if(ret < 0)
	{
		printf("\n获得crc_bin文件内容,失败! ret: %d\n", ret);
		goto UPDATE_END;
	}

	// 重启系统
	printf("\n升级完成,重启设备...\n");
	http_update_success_reboot();

	ret = 0;

	// 更新结束
UPDATE_END:
	http_update_close_connect_by_lwip(); // 先关闭连接
	//updateparam->error	 = 0;
	//if(ret < 0){ updateparam->success = 0; }
	//else{ updateparam->success = 1; }
	//updateparam->end = 1;
	updateparam->mode = UPDATE_MODE_NULL; // 更新结束

	led_control_function(LD_LAN, LD_OFF);
	
	if(ret < 0){ return(-1); }
	return(0);
}
//////////////////////////////
