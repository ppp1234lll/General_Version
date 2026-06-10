#include "app.h"
#include <stdlib.h>
#include "snmp_udp.h"
#include "bsp_timers.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "appconfig.h"

#define SNMP_DEBUG 		    1

#define SNMP_PRIO		    10
#define SNMP_STK_SIZE	    512
unsigned int SNMP_TASK_STK[SNMP_STK_SIZE];

__attribute__((section (".RAM_D1")))  uint8_t snmp_send_buff[128] = {0};  // 发送缓存
__attribute__((section (".RAM_D1")))  uint8_t snmp_recv_buff[128] = {0};  // 接收缓存

int snmp_sock = -1;

// 配置参数
#define SNMP_PORT             161            // SNMP端口号
#define SNMP_READ_COMMUNITY   "public"       // SNMP社区名（读取）
#define SNMP_WRITE_COMMUNITY  "private"      // SNMP社区名（写入）
#define SNMP_TARGET_IP        "192.168.2.1"  // SNMP目标IP

#define TEST_DAHUA_IPC      "192.168.1.108"
#define TEST_HIK_IPC        "192.168.1.64"
#define TEST_SNMP_IP        "192.168.1.222"

#define MAX_PORT_NUM    10             // 最大端口数（10口）
// 存储SNMP获取的参数
__attribute__((section (".RAM_D1"))) snmp_param_t sg_snmp_param_t;

// SNMP运行状态
typedef struct {
    uint8_t param_times;    // 参数获取次数
    uint8_t circul_times;   // 循环次数
    uint8_t status;         // 运行状态
	char snmp_ipbuf[32];    // SNMP IP地址
	uint8_t snmp_ipc_brand;     // IPC品牌
	uint8_t snmp_dev_type;     // SNMP 设备类型
} snmp_run_t;
__attribute__((section (".RAM_D1"))) snmp_run_t  sg_snmp_status_t;   // 运行状态
__attribute__((section (".RAM_D1"))) snmp_oid_t  *snmp_oid_param; 
// SNMP任务函数
static void snmp_thread(void *arg)
{
	LWIP_UNUSED_ARG(arg);
    int send_len = 0;

    snmp_oid_param = app_get_snmp_oid_function();
	uint32_t recv_times = 0; // 接收计时

	while (1) 
	{
		if(g_lwipdev.snmp_status == LWIP_UDP_INIT_CONNECT) // UDP连接初始化
		{
            g_lwipdev.snmp_status = LWIP_UDP_CONNECT;
            if(SNMP_DEBUG)  printf("snmp successed...\n");
        }
		else if(g_lwipdev.snmp_status == LWIP_UDP_CONNECT) // 连接成功
		{	
			switch(sg_snmp_status_t.status)
            {
				case SNMP_START:
					//1、获取设备类型、IP地址:0-5摄像机  6:光猫  7:交换机
					switch(app_get_snmp_dev_type_function())
					{
						case 0:
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
							sg_snmp_status_t.snmp_dev_type = SNMP_IPC;
							if(app_get_camera_param_function(sg_snmp_status_t.snmp_ipbuf, &sg_snmp_status_t.snmp_ipc_brand, app_get_snmp_dev_type_function()) < 0)
							{
								sg_snmp_status_t.status = SNMP_END;
								if(SNMP_DEBUG) printf("get camera ip failed\n");
							}
                            else {
								sg_snmp_status_t.status = SNMP_LINK;
							}
							break;
						case 6:
							sg_snmp_status_t.snmp_dev_type = SNMP_ONV;
							sprintf(sg_snmp_status_t.snmp_ipbuf,"%d.%d.%d.%d",snmp_oid_param->onv_ip[0],snmp_oid_param->onv_ip[1],snmp_oid_param->onv_ip[2],snmp_oid_param->onv_ip[3]);
                            sg_snmp_status_t.status = SNMP_LINK;
                        break;
						case 7:
							sg_snmp_status_t.snmp_dev_type = SNMP_SWITCH;
							sprintf(sg_snmp_status_t.snmp_ipbuf,"%d.%d.%d.%d",snmp_oid_param->switch_ip[0],snmp_oid_param->switch_ip[1],snmp_oid_param->switch_ip[2],snmp_oid_param->switch_ip[3]);
                            sg_snmp_status_t.status = SNMP_LINK;
                        break;
						default:
							break;
					}
					break;
				case SNMP_LINK:
					// 2、连接SNMP服务器
					if(snmp_link_function(sg_snmp_status_t.snmp_ipbuf,SNMP_PORT) < 0)
					{
						sg_snmp_status_t.status = SNMP_END;
						if(SNMP_DEBUG) printf("snmp link failed\n");
					}
                    else {
						sg_snmp_status_t.status = SNMP_SEND;
					}
					break;
				case SNMP_SEND:
					// 3、发送SNMP请求
					switch(sg_snmp_status_t.snmp_dev_type)
					{
						case SNMP_IPC:
							for(uint8_t i = 0;i < IPC_OID_MAX;i++)
							{
								send_len = snmp_build_get_packet(snmp_send_buff,sizeof(snmp_send_buff),i,sg_snmp_status_t.snmp_dev_type,0);
								if(send_len > 0)
								{
									snmp_send_function((char*)sg_snmp_status_t.snmp_ipbuf,SNMP_PORT,(char*)snmp_send_buff,send_len);
								}
								 vTaskDelay(50);  			// 延时50ms	
								 snmp_recv_function(i);									
							}
							recv_times = 0; // 接收计时重置
							sg_snmp_status_t.status = SNMP_RECV;
							break;
						case SNMP_ONV:
							break;
						case SNMP_SWITCH:
							for(uint8_t i = 0;i < SW_OID_MAX;i++)
							{
								switch(i)
								{
									case SW_PORT:
									case SW_POE:
									case SW_POE_POWER:
										for(uint8_t port = 1;port <= MAX_PORT_NUM;port++)
										{
											send_len = snmp_build_get_packet(snmp_send_buff,sizeof(snmp_send_buff),i,sg_snmp_status_t.snmp_dev_type,port);
											if(send_len > 0)
											{
												snmp_send_function((char*)sg_snmp_status_t.snmp_ipbuf,SNMP_PORT,(char*)snmp_send_buff,send_len);
											}	
										}
										break;
									default:
										send_len = snmp_build_get_packet(snmp_send_buff,sizeof(snmp_send_buff),i,sg_snmp_status_t.snmp_dev_type,0);
										if(send_len > 0)
										{
											snmp_send_function((char*)sg_snmp_status_t.snmp_ipbuf,SNMP_PORT,(char*)snmp_send_buff,send_len);
										}	
										break;
								}
							}
							recv_times = 0; // 接收计时重置
							sg_snmp_status_t.status = SNMP_RECV;
							break;
						default:
							sg_snmp_status_t.status = SNMP_END;
							break;
					}
				break;
				case SNMP_RECV:// 4、接收SNMP响应
					if(recv_times == 0)
					{
						recv_times = HAL_GetTick();
					}
					else	
					{
						// 循环读取，直到UDP接收缓冲区为空
						while(snmp_recv_function(0) > 0) 
						{
							// 如果还有数据就一直收
						}	
						if(HAL_GetTick() - recv_times > 5000)
						{
							recv_times = 0; // 接收计时重置
							sg_snmp_status_t.status = SNMP_END;
						}					
					}
				break;
				case SNMP_END:
                    if(snmp_sock >= 0)
					{
						close(snmp_sock);
						snmp_sock = -1;
					}
                    sg_snmp_status_t.status = SNMP_NONE;
					break;
				default:
					break;
            }
		}
        FeedFwdgt();	
		vTaskDelay(50);  			// 延时10ms		
	}
}

TaskHandle_t snmp_task_handle = NULL;

/*
*********************************************************************************************************
*	函 数 名: snmp_task_init
*	功能说明: 初始化SNMP任务
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
unsigned char snmp_task_init(void)
{
		BaseType_t res;
	
	taskENTER_CRITICAL();	/*进入临界区*/
	
	xTaskCreate((TaskFunction_t )snmp_thread,
							(const char *   )"snmp_thread",
							(uint16_t       )SNMP_STK_SIZE,
							(void *         )NULL,
							(UBaseType_t    )SNMP_PRIO,
							(TaskHandle_t * )&snmp_task_handle);

	taskEXIT_CRITICAL();	/*退出临界区*/
	
	return res;
	
	
//	INT8U res,err;
//	OS_CPU_SR cpu_sr;
//	
//	OS_ENTER_CRITICAL();	//关中断
//	res = OSTaskCreateExt(snmp_thread, 							 //建立扩展任务(任务代码指针) 
//                        (void *)0,										//传递参数指针
//                        (OS_STK*)&SNMP_TASK_STK[SNMP_STK_SIZE-1], 		//分配任务堆栈栈顶指针 
//                        (INT8U)SNMP_PRIO, 								//分配任务优先级
//                        (INT16U)SNMP_PRIO,								//(未来的)优先级标准(与优先级相同) 
//                        (OS_STK *)&SNMP_TASK_STK[0], 					//分配任务堆栈栈底指针 
//                        (INT32U)SNMP_STK_SIZE, 						//指定堆栈的容量(检验用) 
//                        (void *)0,									//指向用户附加的数据域的指针 
//                        (INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 	
//	OSTaskNameSet(SNMP_PRIO, (INT8U *)(void *)"snmp", &err);
//	OS_EXIT_CRITICAL();		//开中断
//	return res;
}

/*
*********************************************************************************************************
*	函 数 名: snmp_start_function
*	功能说明: snmp启动函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void snmp_start_function(void)
{
	g_lwipdev.snmp_reset = 0;
	snmp_task_init();	
    if(SNMP_DEBUG)  printf("snmp_start...\n");
}

/*
*********************************************************************************************************
*	函 数 名: snmp_stop_function
*	功能说明: snmp停止函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void snmp_stop_function(void)
{
	
		g_lwipdev.tcp_reset = 0;

	
	if(snmp_sock >=0)
	{
		close(snmp_sock);
		snmp_sock = -1;
	}
	
	g_lwipdev.snmp_status = LWIP_UDP_NO_CONNECT;
	
	taskENTER_CRITICAL();	/*进入临界区*/
	vTaskDelete(snmp_task_handle);
	taskEXIT_CRITICAL();	/*退出临界区*/
	
	
	
//	g_lwipdev.snmp_reset = 0;
//	OS_CPU_SR cpu_sr;
//	
//	if(snmp_sock >=0)
//	{
//		close(snmp_sock);
//		snmp_sock = -1;
//	}
//	
//	lwipdev.snmp_status = LWIP_UDP_NO_CONNECT;
//	
//	OS_ENTER_CRITICAL();  // 关中断
//	OSTaskDel(SNMP_PRIO);	// 删除UDP任务
//	OS_EXIT_CRITICAL();		// 开中断
}
/****************************************************************************
* 函 数 名: snmp_set_enable_flag
* 功    能：设置允许查询SNMP标志位
* 入口参数：flag 标志位
* 返回参数：无
* 说    明：0：不允许   1：允许
****************************************************************************/
void snmp_set_enable_flag(uint8_t flag) 
{
    sg_snmp_status_t.status = flag;
}

/****************************************************************************
* 函 数 名: snmp_get_status
* 功    能：获取SNMP运行状态
* 入口参数：无
* 返回参数：运行状态
* 说    明：0：未连接   1：运行中
****************************************************************************/
uint8_t snmp_get_status(void) 
{
    return sg_snmp_status_t.status;
}

/*
*********************************************************************************************************
*	函 数 名: snmp_link_function
*	功能说明: snmp连接函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int snmp_link_function(char *ip,int port)
{
	struct sockaddr_in local_addr;
	struct timeval tv_out;
	int ret = 0;
	
	if(snmp_sock >= 0)
	{
		close(snmp_sock);
		snmp_sock = -1;
	}

	snmp_sock = socket(AF_INET, SOCK_DGRAM, 0); // SOCK_DGRAM:提供面向连接的稳定传输，即UDP协议 
	if (snmp_sock < 0)
	{
		if(SNMP_DEBUG) printf("Socket error\n");
		close(snmp_sock);
		return -1;  // 失败
	}			
	else
	{
		/* 设置要连接的服务器的地址 */
		local_addr.sin_family = AF_INET;
		local_addr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr(ip_param); /*<! 待与 socket 绑定的本地网络接口 IP */   
		local_addr.sin_port = htons(port); /*<! 待与 socket 绑定的本地端口号 */  				
		ret = bind(snmp_sock, (struct sockaddr*)&local_addr, sizeof(local_addr));		// 将 Socket 与本地某网络接口绑定 
		if(ret != 0 )
		{	
			if(SNMP_DEBUG) printf("udp bind error \r\n");
		}
		else
		{
			tv_out.tv_sec = 5;
			tv_out.tv_usec = 0;
			setsockopt(snmp_sock, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out));
			if(SNMP_DEBUG) printf("udp bind...success... \n");
		}	
		return ret;  
	}
}

/*
*********************************************************************************************************
*	函 数 名: snmp_send_function
*	功能说明: snmp发送函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int8_t snmp_send_function(char* ip,int port,char* data,int len)
{
	struct sockaddr_in remote_addr;
	int ret = 0;
		
	memset((void *)&remote_addr,0,sizeof(struct sockaddr_in));
	remote_addr.sin_family      = AF_INET;  
	remote_addr.sin_port        = htons(port);    // 转换成网络
	remote_addr.sin_addr.s_addr = inet_addr(ip);  // 将一个字符串格式的ip地址转换成一个uint32_t数字格式

	ret = sendto(snmp_sock, data, len, 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
	if(ret < 0)
	{
		if(SNMP_DEBUG) printf("sendto error \r\n");
	}
	else
	{
		if(SNMP_DEBUG) printf("sendto...success... \n");
	}
	return ret;
}

/*
*********************************************************************************************************
*	函 数 名: snmp_build_get_packet
*	功能说明: 构造构造GET请求包（单OID）
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int snmp_build_get_packet(uint8_t *packet, int max_len, uint8_t oid,uint8_t brand_type,uint8_t port_id) 
{
    uint8_t oid_bytes[32];
	int oid_len = 0;
	snmp_oid_t  *oid_ber = app_get_snmp_oid_function();

	switch(brand_type)
	{
		case SNMP_IPC:
			if(oid_ber->ipc_ber_len[sg_snmp_status_t.snmp_ipc_brand][oid] == 0)
			{
				if(SNMP_DEBUG) printf("ipc ber str is null\n");
				return 0;
			}
			else 
			{
				oid_len = oid_ber->ipc_ber_len[sg_snmp_status_t.snmp_ipc_brand][oid];
				memcpy(oid_bytes, oid_ber->ipc_oid_ber[sg_snmp_status_t.snmp_ipc_brand][oid], oid_len);
			}
		break;

		case SNMP_ONV:
			if(oid_ber->onv_ber_len[0][oid] == 0)
			{
				if(SNMP_DEBUG) printf("onv ber str is null\n");
				return 0;
			}
			else 
			{
				oid_len = oid_ber->onv_ber_len[0][oid];
				memcpy(oid_bytes, oid_ber->onv_oid_ber[0][oid], oid_len);
			}
		break;

		case SNMP_SWITCH:
			switch(oid)
			{
				case SW_PORT:
				case SW_POE:
				case SW_POE_POWER:	
					if(oid_ber->switch_ber_len[0][oid] == 0)
					{
						if(SNMP_DEBUG) printf("switch ber str is null\n");
						return 0;
					}
					else 
					{
                        oid_len = oid_ber->switch_ber_len[0][oid];
						memcpy(oid_bytes, oid_ber->switch_oid_ber[0][oid], oid_len);
						oid_bytes[oid_len] = port_id;
						oid_len++;
					}
					break;
				default:
					if(oid_ber->switch_ber_len[0][oid] == 0)
					{
						if(SNMP_DEBUG) printf("switch ber str is null\n");
						return 0;
					}
					else 
					{
						oid_len = oid_ber->switch_ber_len[0][oid];
						memcpy(oid_bytes, oid_ber->switch_oid_ber[0][oid], oid_len);
					}
				break;
			}
		break;
	}

    int idx = 0;
    // 构造Variable Binding（OID+NULL）
    uint8_t vb[64], vb_idx = 0;
    vb[vb_idx++] = 0x30; vb[vb_idx++] = 2 + oid_len + 2;
    vb[vb_idx++] = 0x06; vb[vb_idx++] = oid_len;
    memcpy(&vb[vb_idx], oid_bytes, oid_len); vb_idx += oid_len;
    vb[vb_idx++] = 0x05; vb[vb_idx++] = 0x00;

    // 构造Variable Bindings集合
    uint8_t vbs[70], vbs_idx = 0;
    vbs[vbs_idx++] = 0x30; vbs[vbs_idx++] = vb_idx;
    memcpy(&vbs[vbs_idx], vb, vb_idx); vbs_idx += vb_idx;

    // 构造GET PDU
    uint8_t pdu[80], pdu_idx = 0;
    
    // 生成4字节随机数作为request-id
    uint32_t req_id = (rand() << 16) ^ rand(); // 生成一个随机的32位ID

    pdu[pdu_idx++] = 0xA0; // GetRequest-PDU标签
    pdu[pdu_idx++] = 6 + 3 + 3 + vbs_idx; // request-id现在是6字节(1字节标签+1字节长度+4字节数据)
    
    pdu[pdu_idx++] = 0x02; // Integer
    pdu[pdu_idx++] = 0x04; // 长度为4字节
    pdu[pdu_idx++] = (req_id >> 24) & 0xFF;
    pdu[pdu_idx++] = (req_id >> 16) & 0xFF;
    pdu[pdu_idx++] = (req_id >> 8) & 0xFF;
    pdu[pdu_idx++] = req_id & 0xFF; // request-id
    
    pdu[pdu_idx++] = 0x02; pdu[pdu_idx++] = 0x01; pdu[pdu_idx++] = 0x00; // error-status
    pdu[pdu_idx++] = 0x02; pdu[pdu_idx++] = 0x01; pdu[pdu_idx++] = 0x00; // error-index
    memcpy(&pdu[pdu_idx], vbs, vbs_idx); pdu_idx += vbs_idx;

    // 构造SNMP消息
    packet[idx++] = 0x30;
    packet[idx++] = 3 + 2 + strlen(SNMP_READ_COMMUNITY) + pdu_idx;
    packet[idx++] = 0x02; packet[idx++] = 0x01; packet[idx++] = 0x01; // version
    packet[idx++] = 0x04; packet[idx++] = strlen(SNMP_READ_COMMUNITY);
    memcpy(&packet[idx], SNMP_READ_COMMUNITY, strlen(SNMP_READ_COMMUNITY)); idx += strlen(SNMP_READ_COMMUNITY);
    memcpy(&packet[idx], pdu, pdu_idx); idx += pdu_idx;

    return idx;
}

/*
*********************************************************************************************************
*	函 数 名: snmp_oid_str_to_ber
*	功能说明: OID字符串转BER编码（SNMP OID标准编码格式）
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int snmp_oid_str_to_ber(const char *oid_str, uint8_t *ber, int max_len) 
{
    int oid[32], oid_len = 0;
    char buf[8];
    int j = 0;

    // 解析OID字符串（按"."分割）
    while (*oid_str) 
	{
        if (*oid_str == '.') {
            if (j > 0) {
                buf[j] = 0;
                oid[oid_len++] = atoi(buf);
                j = 0;
            }
        } else {
            buf[j++] = *oid_str;
        }
        oid_str++;
    }
    if (j > 0) {
        buf[j] = 0;
        oid[oid_len++] = atoi(buf);
    }
    if (oid_len < 2) return 0; // OID至少2级（如1.3）

    // 生成BER编码
    int idx = 0;
    ber[idx++] = 40 * oid[0] + oid[1]; // 前两级合并编码
    for (int i = 2; i < oid_len; i++) { // 后续级编码（>128用多字节）
        uint32_t val = oid[i];
        if (val < 128) {
            ber[idx++] = val;
        } else {
            // 计算需要的字节数
            int bytes = 0;
            uint32_t temp = val;
            while (temp > 0) {
                bytes++;
                temp >>= 7;
            }
            // 从高位到低位依次编码
            for (int k = bytes - 1; k >= 0; k--) {
                uint8_t b = (val >> (k * 7)) & 0x7F;
                if (k > 0) {
                    b |= 0x80; // 非最后一个字节最高位置1
                }
                ber[idx++] = b;
            }
        }
    }
    return idx;
}

/*
*********************************************************************************************************
*	函 数 名: snmp_recv_function
*	功能说明: 接收SNMP响应
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int snmp_recv_function(uint8_t oid)
{
	uint8_t *rcvData = NULL;
	int sender_len = 0;
	int len = 0;
	struct sockaddr_in recv_addr;

	memset(snmp_recv_buff,0,sizeof(snmp_recv_buff));
	rcvData = snmp_recv_buff;

	len = recvfrom(snmp_sock,rcvData,sizeof(snmp_recv_buff),0,(struct sockaddr*)&recv_addr,(socklen_t *)&sender_len);
	if(len > 0)
	{
		// 16进制打印接收数据
		 printf("Received: ");
		 for(int i=0;i<len;i++)
		 {
		 	printf("%02X ",rcvData[i]);
		 }
		 printf("\n");

		switch(sg_snmp_status_t.snmp_dev_type)
		{
			case SNMP_IPC:
				for(uint8_t i = 0;i < IPC_OID_MAX;i++)
				{
					switch(sg_snmp_status_t.snmp_ipc_brand)
					{
						case IPC_HIKVISION:
							snmp_deal_hikvision_response(rcvData,len,i);
							break;
						case IPC_DAHUA:
							snmp_deal_dahua_response(rcvData,len,i);
							break;
						default:
							break;
					}							
				}
				break;
			case SNMP_ONV:
				break;
			case SNMP_SWITCH:
				for(uint8_t i = 0;i < SW_OID_MAX;i++)
				{
					snmp_deal_switch_response(rcvData,len,i);
				}
				break;
			default:
				break;
		}
	}	
	else
	{ 
		len = -1; 
	}
	return len;
}

/*
*********************************************************************************************************
*	函 数 名: snmp_deal_hikvision_response
*	功能说明: 解析并处理海康SNMP设备响应
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int8_t snmp_deal_hikvision_response(uint8_t *buf, int len,uint8_t oid)
{
    if (len < 2) { return 0; }
	snmp_oid_t  *resp_oid = app_get_snmp_oid_function();
	uint8_t ber_oid[32];
	uint8_t ber_oid_len = 0;

	if(resp_oid->ipc_ber_len[IPC_HIKVISION][oid] == 0)
	{
		if(SNMP_DEBUG) printf("ipc oid str is null\n");
		return 0;
	}
	else 
	{
		ber_oid_len = resp_oid->ipc_ber_len[IPC_HIKVISION][oid];
		memcpy(ber_oid, resp_oid->ipc_oid_ber[IPC_HIKVISION][oid], ber_oid_len);
	}
    
	for (int i = 0; i < len - 2; i++) 
	{
       if (buf[i] == 0x06)  // 找到OID标签（0x06）
		{
			int oid_len = buf[i+1];
			int oid_start = i + 2;

			// 1. 解析设备品牌（OID：.1.3.6.1.4.1.39165.1.6.0）
			if (oid_len == ber_oid_len && memcmp(&buf[oid_start], ber_oid, ber_oid_len) == 0) 
			{
				int val_start = oid_start + oid_len;
				if (buf[val_start] == 0x04) 
				{ // 字符串类型
					int val_len = buf[val_start+1];
					memset(sg_snmp_param_t.ipc_param[IPC_HIKVISION][oid], 0, sizeof(sg_snmp_param_t.ipc_param[IPC_HIKVISION][oid]));
					int copy_len = (val_len < sizeof(sg_snmp_param_t.ipc_param[IPC_HIKVISION][oid])) ? val_len : sizeof(sg_snmp_param_t.ipc_param[IPC_HIKVISION][oid]);
					memcpy(sg_snmp_param_t.ipc_param[IPC_HIKVISION][oid], &buf[val_start+2], copy_len);
					if(SNMP_DEBUG) printf("Device Model: %s", sg_snmp_param_t.ipc_param[IPC_HIKVISION][oid]);

					switch(oid)
					{
						case IPC_CPU:
						case IPC_MEM:
						{
							// 去除末尾的非数字字符（如 " PERCENT"）
							char *str = (char *)sg_snmp_param_t.ipc_param[IPC_HIKVISION][oid];
							for (int k = 0; str[k] != '\0'; k++) {
								if (str[k] < '0' || str[k] > '9') {
									str[k] = '\0';
									break;
								}
							}
							break;
						}
						default:
							break;
					}
				}
			}
		}
	}
    return 1;
}

/*
*********************************************************************************************************
*	函 数 名: snmp_deal_dahua_response
*	功能说明: 解析并处理大华视频SNMP设备响应
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int8_t snmp_deal_dahua_response(uint8_t *buf, int len,uint8_t oid)
{
    if (len < 2) { return 0; }
	snmp_oid_t  *resp_oid = app_get_snmp_oid_function();
	uint8_t ber_oid[32];
	uint8_t ber_oid_len = 0;

	if(resp_oid->ipc_ber_len[IPC_DAHUA][oid] == 0)
	{
		if(SNMP_DEBUG) printf("ipc oid str is null\n");
		return 0;
	}
	else 
	{
		ber_oid_len = resp_oid->ipc_ber_len[IPC_DAHUA][oid];
		memcpy(ber_oid, resp_oid->ipc_oid_ber[IPC_DAHUA][oid], ber_oid_len);
	}

	for (int i = 0; i < len - 2; i++) 
	{
       if (buf[i] == 0x06)  // 找到OID标签（0x06）
		{
			int oid_len = buf[i+1];
			int oid_start = i + 2;

			// 1. 解析设备品牌（OID：.1.3.6.1.4.1.39165.1.6.0）
			if (oid_len == ber_oid_len && memcmp(&buf[oid_start], ber_oid, ber_oid_len) == 0) 
			{
				int val_start = oid_start + oid_len;
				if (buf[val_start] == 0x04) 
				{   // 字符串类型
					int val_len = buf[val_start+1];
					memset(sg_snmp_param_t.ipc_param[IPC_DAHUA][oid], 0, sizeof(sg_snmp_param_t.ipc_param[IPC_DAHUA][oid]));
					int copy_len = (val_len < sizeof(sg_snmp_param_t.ipc_param[IPC_DAHUA][oid])) ? val_len : sizeof(sg_snmp_param_t.ipc_param[IPC_DAHUA][oid]);
					memcpy(sg_snmp_param_t.ipc_param[IPC_DAHUA][oid], &buf[val_start+2], copy_len);
					if(SNMP_DEBUG) printf("Device Model: %s", sg_snmp_param_t.ipc_param[IPC_DAHUA][oid]);
				}
				else if (buf[val_start] == 0x02) // integer类型
				{  	
					int val_len = buf[val_start+1];
					memset(sg_snmp_param_t.ipc_param[IPC_DAHUA][oid], 0, sizeof(sg_snmp_param_t.ipc_param[IPC_DAHUA][oid]));
					
					// 提取整数值并将其转换为字符串存储
					uint32_t val = 0;
					for(int k = 0; k < val_len; k++) {
						val = (val << 8) | buf[val_start + 2 + k];
					}
					sprintf((char *)sg_snmp_param_t.ipc_param[IPC_DAHUA][oid], "%u", val);
					
					if(SNMP_DEBUG) printf("Device Model: %s", sg_snmp_param_t.ipc_param[IPC_DAHUA][oid]);
				}
			}
		}
	}
    return 1;
}

/*
*********************************************************************************************************
*	函 数 名: snmp_deal_switch_response
*	功能说明: 解析并处理交换机SNMP设备响应
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int8_t snmp_deal_switch_response(uint8_t *buf, int len,uint8_t oid)
{
    if (len < 2) { return 0; }
	snmp_oid_t  *resp_oid = app_get_snmp_oid_function();
	uint8_t ber_oid[32];
	uint8_t ber_oid_len = 0;

	if(resp_oid->switch_ber_len[0][oid] == 0)
	{
		if(SNMP_DEBUG) printf("switch oid str is null\n");
		return 0;
	}
	else 
	{
		ber_oid_len = resp_oid->switch_ber_len[0][oid];
		memcpy(ber_oid, resp_oid->switch_oid_ber[0][oid], ber_oid_len);
	}

	for (int i = 0; i < len - 2; i++) 
	{
       if (buf[i] == 0x06)  // 找到OID标签（0x06）
		{
			int oid_len = buf[i+1];
			int oid_start = i + 2;

			switch(oid)
			{
				case SW_BRAND:
				case SW_MODEL:
					if (oid_len == ber_oid_len && memcmp(&buf[oid_start], ber_oid, ber_oid_len) == 0) 
					{
						int val_start = oid_start + oid_len;
						if (buf[val_start] == 0x04) 
						{ // 字符串类型
							int val_len = buf[val_start+1];
							if(oid == SW_BRAND) {
								memset(sg_snmp_param_t.switch_param[0].brand, 0, sizeof(sg_snmp_param_t.switch_param[0].brand));
								int copy_len = (val_len < sizeof(sg_snmp_param_t.switch_param[0].brand)) ? val_len : sizeof(sg_snmp_param_t.switch_param[0].brand);
								memcpy(sg_snmp_param_t.switch_param[0].brand, &buf[val_start+2], copy_len);		
							} else if(oid == SW_MODEL) {
								memset(sg_snmp_param_t.switch_param[0].device_model, 0, sizeof(sg_snmp_param_t.switch_param[0].device_model));
								int copy_len = (val_len < sizeof(sg_snmp_param_t.switch_param[0].device_model)) ? val_len : sizeof(sg_snmp_param_t.switch_param[0].device_model);
								memcpy(sg_snmp_param_t.switch_param[0].device_model, &buf[val_start+2], copy_len);
								if(SNMP_DEBUG) printf("Device Model: %s", sg_snmp_param_t.switch_param[0].device_model);
							}
						}
					}
					break;
				case SW_PORT:
				case SW_POE:
				case SW_POE_POWER:	
					for (uint8_t port = 1; port <= MAX_PORT_NUM; port++)
					{
						ber_oid[ber_oid_len] = port;
						// 这里不再修改ber_oid_len，只是在当前循环中使用ber_oid_len+1进行比较
						if (oid_len == ber_oid_len + 1 && memcmp(&buf[oid_start], ber_oid, ber_oid_len + 1) == 0) 
						{
							int val_start = oid_start + oid_len;
							if (buf[val_start] == 0x02) // integer类型
							{
								int val_len = buf[val_start+1], val = 0;
								for (int k = 0; k < val_len; k++) val = (val << 8) | buf[val_start+2+k];
								if(oid == SW_PORT) {
									sg_snmp_param_t.switch_param[0].port_status[port-1] = val;
								} else if(oid == SW_POE) {
									sg_snmp_param_t.switch_param[0].port_poe[port-1] = val;
								} else if(oid == SW_POE_POWER) {
									sg_snmp_param_t.switch_param[0].port_poe_power[port-1] = val;
								}
							}
						}
					}
					break;
				default:
				break;
			}
		}
	}
    return 1;
}

/****************************************************************************
* 名    称: snmp_get_snmp_param
* 功    能：获取SNMP设备信息
* 入口参数：
* 返回参数：SNMP设备信息
* 说    明： 
****************************************************************************/
void *snmp_get_snmp_param(void)    
{
	return  &sg_snmp_param_t;
}

/****************************************************************************
* 名    称: snmp_get_snmp_ipc_brand
* 功    能：获取SNMP设备品牌
* 入口参数：
* 返回参数：SNMP设备品牌
* 说    明： 
****************************************************************************/
uint8_t snmp_get_snmp_ipc_brand(void)    
{
	return  sg_snmp_status_t.snmp_ipc_brand;
}
