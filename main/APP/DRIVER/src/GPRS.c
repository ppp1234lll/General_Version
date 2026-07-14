/********************************************************************************
* @File name  : 4G模块
* @Description: 串口2-对应4G
* @Author     : ZHLE
*  Version Date        Modification Description
*    12、ML307（4G模块）：串口2，波特率115200，引脚分配为：   
*        4G-TXD：    PD5
*        4G-RXD：    PD6
*        4G_PWRK:    PB7
*        4G_NRST:    PB6
*        4G_CTRL:    PD1
*        SIM-Sel:    PE3     选择SIM
*        SIM-DET:    PE1     SIM卡检测
********************************************************************************/

#include "./Driver/inc/GPRS.h"
#include "main.h"
#include "appconfig.h"
#include "Task/inc/gprs_rx.h"


/* 控制IO */
#define GPRS_NRST_GPIO_CLK              RCU_GPIOB
#define GPRS_NRST_GPIO                  GPIOB   
#define GPRS_NRST_PIN                   GPIO_PIN_6

#define GPRS_PWRK_GPIO_CLK              RCU_GPIOB
#define GPRS_PWRK_GPIO                  GPIOB
#define GPRS_PWRK_PIN                   GPIO_PIN_7

#define GPRS_CTRL_GPIO_CLK              RCU_GPIOD
#define GPRS_CTRL_GPIO                  GPIOD
#define GPRS_CTRL_PIN                   GPIO_PIN_1

#define GPRS_Sel_GPIO_CLK               RCU_GPIOE
#define GPRS_Sel_GPIO                   GPIOE
#define GPRS_Sel_PIN                    GPIO_PIN_3

#define GPRS_DET_GPIO_CLK               RCU_GPIOE
#define GPRS_DET_GPIO                   GPIOE
#define GPRS_DET_PIN                    GPIO_PIN_1
#define GPRS_NRST_H gpio_bit_set(GPRS_NRST_GPIO,GPRS_NRST_PIN)
#define GPRS_NRST_L gpio_bit_reset(GPRS_NRST_GPIO,GPRS_NRST_PIN)

#define GPRS_PWRK_H gpio_bit_set(GPRS_PWRK_GPIO,GPRS_PWRK_PIN)
#define GPRS_PWRK_L gpio_bit_reset(GPRS_PWRK_GPIO,GPRS_PWRK_PIN)

#define GPRS_CTRL_H gpio_bit_set(GPRS_CTRL_GPIO,GPRS_CTRL_PIN)
#define GPRS_CTRL_L gpio_bit_reset(GPRS_CTRL_GPIO,GPRS_CTRL_PIN)

#define GPRS_Sel_H gpio_bit_set(GPRS_Sel_GPIO,GPRS_Sel_PIN)     // ?????SIM
#define GPRS_Sel_L gpio_bit_reset(GPRS_Sel_GPIO,GPRS_Sel_PIN)   // ??????SIM??

#define GPRS_DET_READ gpio_input_bit_get(GPRS_DET_GPIO,GPRS_DET_PIN)

/* 串口初始化 */
#define GPRS_BAUDRATE               (115200)
#define GPRS_UART_INIT(baudrate)    bsp_InitUsart1(baudrate)
#define GPRS_STR_SEND(data,len)     usart1_send_str(data,len)
////

static int gprs_wait_feedback(const unsigned char *feedback, int feedback_len, int waittime);
////

gprs_rx_t sg_gprs_rx_t[GPRS_LINK_MAX] = {0};  // 数据平台接收数据缓冲
uint8_t  gprs_at_buff[GPRS_AT_BUFF_SIZE]; // 存储AT指令
static uint16_t s_gprs_at_idx  = 0;         // AT缓冲写偏移
static uint16_t s_gprs_at_take = 0;         // AT缓冲读偏移(已消费)

/* AT 指令递归互斥锁: 序列化所有 AT 往返, 保证 DATA/OTA/FILE 多链路命令粒度时分复用 */
SemaphoreHandle_t g_gprs_at_mutex = NULL;

/* 数据 */
struct gprs_status_t sg_gprs_status_t = {0};
gprs_log_t sg_gprs_log_t = {0};

/*
*********************************************************************************************************
*	函 数 名: gprs_gpio_init_function
*	功能说明: 引脚初始化函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void gprs_gpio_init_function(void)
{
    /* enable GPIO clock */
    rcu_periph_clock_enable(GPRS_NRST_GPIO_CLK);

    /* configure USART0 TX as alternate function push-pull */
    gpio_mode_set(GPRS_NRST_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,GPRS_NRST_PIN);
    gpio_output_options_set(GPRS_NRST_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPRS_NRST_PIN);

    /* configure USART0 RX as alternate function push-pull */
    gpio_mode_set(GPRS_PWRK_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,GPRS_PWRK_PIN);
    gpio_output_options_set(GPRS_PWRK_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPRS_PWRK_PIN);
    
    gpio_mode_set(GPRS_CTRL_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,GPRS_CTRL_PIN);
    gpio_output_options_set(GPRS_CTRL_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPRS_CTRL_PIN);
    
    gpio_mode_set(GPRS_Sel_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,GPRS_Sel_PIN);
    gpio_output_options_set(GPRS_Sel_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPRS_Sel_PIN);
    
    gpio_mode_set(GPRS_DET_GPIO, GPIO_MODE_INPUT, GPIO_PUPD_NONE,GPRS_DET_PIN);

    GPRS_CTRL_H; // 默认打开电源 
    GPRS_NRST_L;
    GPRS_PWRK_L;

#if configUSE_EXT_SIM == 1
    if(GPRS_DET_READ == 0)
    {
        sg_gprs_status_t.sim_status = SIM_INT;
        GPRS_Sel_L;
    }
    else
    {
        sg_gprs_status_t.sim_status = SIM_EXT;
        GPRS_Sel_H;
    }   
#else
    GPRS_Sel_H;
#endif
}   

/*
*********************************************************************************************************
*	函 数 名: gprs_init_function
*	功能说明: 初始化函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void gprs_init_function(void)
{
	gprs_gpio_init_function();
    gprs_rx_queue_init_function();   /* (须在使能串口接收前) */

    /* 创建 AT 指令递归互斥锁(串行化所有 AT 指令,多链路并发安全) */
    if(g_gprs_at_mutex == NULL)
    {
        g_gprs_at_mutex = xSemaphoreCreateRecursiveMutex();
        configASSERT(g_gprs_at_mutex);
    }
	GPRS_UART_INIT(GPRS_BAUDRATE);
}

/*
*********************************************************************************************************
*	函 数 名: gprs_boot_up_function
*	功能说明: 模块开机函数
*	形    参: 无
*	返 回 值: 无
*	ML307: 拉低PWR_ON/OFF引脚2s~3.5s使模组开机
*********************************************************************************************************
*/
void gprs_boot_up_function(void)
{
	GPRS_PWRK_H;
	GPRS_DELAY_MS(2010); // 开机需要拉低PWRK至少1s
	GPRS_PWRK_L;
	GPRS_DELAY_MS(100);
}

/*
*********************************************************************************************************
*	函 数 名: gprs_shutdown_function
*	功能说明: 模块关机函数
*	形    参: 无
*	返 回 值: 无
*	 EC800E: RESET拉低至少50ms，或者PWR拉低至少650ms
*	 ML307: 拉低PWR_ON/OFF引脚3.5s~4s后释放，模组将执行关机流程
*********************************************************************************************************
*/
void gprs_shutdown_function(void)
{
	GPRS_PWRK_H;
	GPRS_DELAY_MS(3600); // 关机需要拉低PWRK至少2s
	GPRS_PWRK_L;
}

/*
*********************************************************************************************************
*	函 数 名: gprs_reset_function
*	功能说明: 重启函数
*	形    参: 无
*	返 回 值: 无
*	ML307: 拉低RESET引脚至少300ms或更长时间实现系统复位
*********************************************************************************************************
*/
void gprs_reset_function(void)
{
	GPRS_NRST_H;
	GPRS_DELAY_MS(500); // 复位需要将NRST拉低50ms到100ms
	GPRS_NRST_L;
	GPRS_DELAY_MS(100);
}

/*
*********************************************************************************************************
*	函 数 名: gprs_v_reset_function
*	功能说明: 断电重启函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void gprs_v_reset_function(void)
{
	GPRS_CTRL_L;
	GPRS_DELAY_MS(10000); // 复位需要将NRST拉低50ms到100ms
	GPRS_CTRL_H;
}


/*
*********************************************************************************************************
*	函 数 名: gprs_deinit_function
*	功能说明: 初始化-清除变量
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void gprs_deinit_function(void)
{
    memset(&sg_gprs_status_t,0,sizeof(struct gprs_status_t));
}

/*
*********************************************************************************************************
*	函 数 名: gprs_send_at
*	功能说明: 发送 AT 指令并等待回馈(薄封装 gprs_send_cmd)
*	形    参: cmd : AT 指令, ack : 期望回馈(NULL 则不等待), waittime : 超时 ms
*	返 回 值: GPRS_SEND_OK 成功
*********************************************************************************************************
*/
static int gprs_send_at(const char *cmd, const char *ack, uint16_t waittime)
{
	if(ack)
	{
		struct GPRS_FEEDBACK fb = {(uint8_t *)ack, (uint16_t)strlen(ack)};
		return gprs_send_cmd((uint8_t *)cmd, strlen(cmd), &fb, 1, waittime);
	}
	return gprs_send_cmd((uint8_t *)cmd, strlen(cmd), NULL, 0, 0);
}

/*
*********************************************************************************************************
*	函 数 名: gprs_status_check_function
*	功能说明: 状态监测函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int8_t gprs_status_check_function(void)
{
	static uint8_t repeat = 0;
	uint32_t temp1 = 0;
	uint32_t temp2 = 0;
	uint8_t  res   = 0;
	uint8_t  index = 0;
	uint8_t *p1 = NULL;
	uint32_t time[6] = {0};
	uint8_t addr_len = 0;

	switch(sg_gprs_status_t.step) 
	{
		case GPRS_BOOT:
			/* 数据清零 */
			repeat = 0;
			/* 设备开机 */
			gprs_boot_up_function();
			sg_gprs_status_t.step = GPRS_INIT;
			break;
		case GPRS_INIT:
			gprs_reset_function();
			sg_gprs_status_t.step = GPRS_COMM_CHECK;
			repeat = 0;
			break;
		case GPRS_COMM_CHECK:
			/* 通信检测 */
			if(gprs_send_at("AT\r\n", "\r\nOK\r\n", 25) == GPRS_SEND_OK) 
			{
				gprs_send_at("ATE0\r\n", NULL, 0);
				sg_gprs_status_t.step = GPRS_SIM;
				sg_gprs_status_t.status.com = 1;
				repeat = 0;
			}
			else 
			{
				GPRS_DELAY_MS(10);
				sg_gprs_status_t.status.com = 0;
				repeat++;
				if(repeat > 30) 
				{
					sg_gprs_status_t.step = GPRS_INIT;
				}
			}
			break;
		case GPRS_SIM:
			/* SIM卡状态检测 */
			if(gprs_send_at("AT+CPIN?\r\n", "+CPIN: READY", 100) == GPRS_SEND_OK) 
			{
				gprs_send_at("AT+MCFG=\"simhot\",0\"\r\n", NULL, 0);
				GPRS_DELAY_MS(20);
				for(index=0; index<3; index++) 
				{
					res = gprs_send_at("AT+ICCID\r\n", "+ICCID:", 100);
					if(res == GPRS_SEND_OK) 
					{
						p1 = (uint8_t*)strstr((char*)gprs_at_buff,"+ICCID: ");
						p1 += 8;
						memcpy(sg_gprs_status_t.ccid,p1,20);
						break;
					}
				}
				sg_gprs_status_t.step = GPRS_CFUN;
				sg_gprs_status_t.status.sim = 1;
				repeat = 0;
			} 
			else 
			{
                /* 未收到READY,根据应答区分SIM状态 */
                uint8_t sim_not_inserted = (strstr((char*)gprs_at_buff, "+CME ERROR: 10") != NULL);
                if(sim_not_inserted)
                {
                    printf("SIM卡未插入\n");
                    sg_gprs_status_t.status.sim = 2;  /* SIM卡未插入 */
                    // 切换SIM卡
                    if((gpio_input_bit_get(GPRS_Sel_GPIO, GPRS_Sel_PIN)) == 1)
                        GPRS_Sel_L;    
                    else
                        GPRS_Sel_H; 
                    sg_gprs_status_t.step = GPRS_INIT;                   
                }
                else
                {
                    sg_gprs_status_t.status.sim = 0;  /* 通信超时或其他异常 */
                }
                GPRS_DELAY_MS(20);
				repeat++;
				if(repeat > 20) 
				{
                    sg_gprs_log_t.init_step = GPRS_SIM;
					sg_gprs_status_t.step = GPRS_INIT;
				}
			}
			break;
			
		case GPRS_CFUN:
			/* 协议栈状态 */
			if(gprs_send_at("AT+CFUN?\r\n", "+CFUN: 1", 25) == GPRS_SEND_OK) {
				sg_gprs_status_t.step = GPRS_CEREG;
				repeat = 0;
			} else {
				GPRS_DELAY_MS(20);
				repeat++;
				if(repeat > 10) {
                    sg_gprs_log_t.init_step = GPRS_CFUN;
					sg_gprs_status_t.step = GPRS_INIT;
				}
			}
			break;

		case GPRS_CEREG:
			/* 网络注册状态 */
			if(gprs_send_at("AT+CEREG?\r\n", "\r\nOK\r\n", 1000) == GPRS_SEND_OK) 
			{
				p1 = (uint8_t*)strstr((char*)gprs_at_buff,"+CEREG:");
				temp2 = 0;
				temp1 = 0;
				res = sscanf((char*)p1,"+CEREG: %d,%d",&temp1,&temp2);
				if(temp1 == 0 && res == 2) 
				{
					gprs_send_at("AT+CEREG=2\r\n", NULL, 0);
				}
								
				if((temp2 == 1 || temp2 == 5) && res == 2) 
				{
					sg_gprs_status_t.status.net = 1;
					sg_gprs_status_t.step = GPRS_CCLK;
					repeat = 0;
				} 
				else 
				{
					if(temp2 == 3)
                    {
                        sg_gprs_log_t.cereg = 3;
                        printf("SIM Registration denied!!!\n");
                    }
					sg_gprs_status_t.status.net = 0;
					GPRS_DELAY_MS(260);
					repeat++;
					if(repeat > 50) 
					{
                        sg_gprs_log_t.init_step = GPRS_CEREG;
						sg_gprs_status_t.step = GPRS_INIT;
					}
				}
			} 
			else {
				sg_gprs_status_t.status.net = 0;
				GPRS_DELAY_MS(260);
				repeat++;
				if(repeat > 50) {
                    sg_gprs_log_t.init_step = GPRS_CEREG;
					sg_gprs_status_t.step = GPRS_INIT;
				}
			}
			break;
			
		case GPRS_CCLK:
			/* 同步时间 */
			if(gprs_send_at("AT+CCLK?\r\n", "+CCLK: ", 25) == GPRS_SEND_OK) {
				p1 = (uint8_t*)strstr((char*)gprs_at_buff,"+CCLK: ");
				if(p1 != NULL) {
					p1 += 8;
					memset(time,0,sizeof(time));
					sscanf((char*)p1,"%d/%d/%d,%d:%d:%d",&time[0],&time[1],&time[2],&time[3],&time[4],&time[5]);
					time[0] += 2000;
					app_set_current_time((int*)time,1);
					repeat = 0;
					sg_gprs_status_t.step = GPRS_MIPCCLK;
				} else {
					sg_gprs_status_t.status.net = 0;
					GPRS_DELAY_MS(200);
					repeat++;
					if(repeat > 20) {
                        sg_gprs_log_t.init_step = GPRS_CCLK;
						sg_gprs_status_t.step = GPRS_INIT;
					}
				}
			} else {
				sg_gprs_status_t.status.net = 0;
				GPRS_DELAY_MS(200);
				repeat++;
				if(repeat > 20) {
                    sg_gprs_log_t.init_step = GPRS_CCLK;
					sg_gprs_status_t.step = GPRS_INIT;
				}
			}
			break;
		case GPRS_MIPCCLK:
			if(gprs_send_at("AT+MIPCALL?\r\n", "+MIPCALL:", 100) == GPRS_SEND_OK) 
			{
				p1 = (uint8_t*)strstr((char*)gprs_at_buff,"+MIPCALL:");
				temp2 = 0;
				temp1 = 0;
				res = sscanf((char*)p1,"+MIPCALL: %d,%d",&temp1,&temp2);		
				if((temp2 == 1) && res == 2) 
				{
					sg_gprs_status_t.step = GPRS_CGPADDR;
					repeat = 0;
				} 
				else 
				{
					GPRS_DELAY_MS(100);
					repeat++;
					if(repeat > 20) 
					{
						gprs_send_at("AT+CGDCONT=1,\"IP\",\"CMIOT\"\r\n", NULL, 0);
						sg_gprs_status_t.step = GPRS_PDP;
					}
				}			
			}
			else 
			{
				sg_gprs_status_t.status.net = 0;
				GPRS_DELAY_MS(100);
				repeat++;
				if(repeat > 20) {
                    sg_gprs_log_t.init_step = GPRS_MIPCCLK;
					sg_gprs_status_t.step = GPRS_INIT;
				}
			}
			break;
		case GPRS_PDP:
			/* 激活 PDP 场景: 合并 OK 和 +MIPCALL 两个反馈 */
			{
				char fb_pdp_buf[16];
				struct GPRS_FEEDBACK fb_pdp[2] = {
					{(uint8_t*)"\r\nOK\r\n", 6},
					{(uint8_t*)fb_pdp_buf, 0}
				};
				sprintf(fb_pdp_buf, "+MIPCALL:");
				fb_pdp[1].feedback_len = 9;

				if(gprs_send_cmd((uint8_t*)"AT+MIPCALL=1,1\r\n", 18,
					fb_pdp, 2, 200) == GPRS_SEND_OK)
				{
					p1 = (uint8_t*)strstr((char*)gprs_at_buff,"+MIPCALL: ");
					temp2 = 0;
					temp1 = 0;
					res = sscanf((char*)p1,"+MIPCALL: %d,%d",&temp1,&temp2);

					if((temp2 == 1) && res == 2) {
						sg_gprs_status_t.step = GPRS_CGPADDR;
						repeat = 0;
					} 
					else 
					{
						GPRS_DELAY_MS(260);
						repeat++;
						if(repeat > 20) {
                        sg_gprs_log_t.init_step = GPRS_PDP;
							sg_gprs_status_t.step = GPRS_INIT;
						}
					}
				} 
				else 
				{
					sg_gprs_status_t.status.net = 0;
					GPRS_DELAY_MS(100);
					repeat++;
					if(repeat > 20) {
                    sg_gprs_log_t.init_step = GPRS_PDP;
						sg_gprs_status_t.step = GPRS_INIT;
					}
				}
			}
			break;
		case GPRS_CGPADDR:
			/* 获取IP地址 */
			if(gprs_send_at("AT+CGPADDR=1\r\n", "+CGPADDR", 50) == GPRS_SEND_OK)
			{
				p1 = (uint8_t*)strstr((char*)gprs_at_buff,"+CGPADDR: ");
				memset(sg_gprs_status_t.status.ip,0,sizeof(sg_gprs_status_t.status.ip));
				res = sscanf((char*)p1,"+CGPADDR: 1,\"%[^\"]",sg_gprs_status_t.status.ip);
				if(res == 1) 
				{
					sg_gprs_status_t.step = GPRS_CGMR;
				} 
				else 
				{
					GPRS_DELAY_MS(200);
					repeat++;
					if(repeat > 20) {
					sg_gprs_status_t.status.net = 0;
                        sg_gprs_log_t.init_step = GPRS_CGPADDR;
					sg_gprs_status_t.step = GPRS_INIT;
					}
				}
			} 
			else 
			{
				GPRS_DELAY_MS(200);
				repeat++;
				if(repeat > 20) 
				{
                    sg_gprs_log_t.init_step = GPRS_CGPADDR;
					sg_gprs_status_t.step = GPRS_INIT;
                    sg_gprs_status_t.status.net = 0;
				}
			}
			break;
		case GPRS_CGMR:
			/* 查询模块版本信息 */
			if(gprs_send_at("AT+CGMR\r\n", "\r\nOK\r\n", 100) == GPRS_SEND_OK)
			{
				p1 = (uint8_t*)strstr((char*)gprs_at_buff,"OK");
				if(p1 != NULL) 
				{
					addr_len = p1 - gprs_at_buff - 6;
					memset(sg_gprs_status_t.model,0,sizeof(sg_gprs_status_t.model));
					memcpy(sg_gprs_status_t.model,gprs_at_buff+2,addr_len);	
				}
			}
			sg_gprs_status_t.step = GPRS_IMEI;
			break;		
		case GPRS_IMEI:
			/* 查询模块IMEI */			
			if(gprs_send_at("AT+CGSN=1\r\n", "+CGSN: ", 50) == GPRS_SEND_OK) 
			{
				p1 = (uint8_t*)strstr((char*)gprs_at_buff,"+CGSN: ");
				if(p1 != NULL) 
				{
					memset(sg_gprs_status_t.imei,0,sizeof(sg_gprs_status_t.imei));
					memcpy(sg_gprs_status_t.imei,p1+7,15);	
				}
			}
			sg_gprs_status_t.step = GPRS_CSQ;
			break;
			
		case GPRS_CSQ:
			/* 信号强度 */
			if(gprs_send_at("AT+CSQ\r\n", "+CSQ: ", 25) == GPRS_SEND_OK) 
			{
				p1 = (uint8_t*)strstr((char*)gprs_at_buff,"+CSQ: ");
				temp2 = 0;
				temp1 = 0;
				res = sscanf((char*)p1,"+CSQ: %d,%d",&temp1,&temp2);
				if(temp1 != 99 && res == 2) {
					sg_gprs_status_t.status.csq = temp1+1;
                    sg_gprs_log_t.csq = temp1;
					sg_gprs_status_t.step = GPRS_SUCCESS;
					repeat = 0;
				} 
				else 
				{
					GPRS_DELAY_MS(200);
					repeat++;
					if(repeat > 10) {
                        sg_gprs_log_t.csq = 0;
						sg_gprs_status_t.step = GPRS_SUCCESS;
					}
				}
			}
			break;
			
		default:
			/* 初始化完成 */
			sg_gprs_status_t.mount = 1;
			repeat = 0;
			gprs_send_cmd_over_function();
			return 0;
	}
	
	/* 正在初始化 */
	return 1;
	
}

/*
*********************************************************************************************************
*    函 数 名: gprs_csq_status_monitoring_function
*    功能说明: CSQ信号监测函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void gprs_csq_status_monitoring_function(void)
{
    static uint32_t s_tick = 0;

    if(HAL_GetTick() - s_tick < 600000)  /* 10分钟查询一次 */
    {
        return ;
    }
    s_tick = HAL_GetTick();

	if(gprs_send_at("AT+CSQ\r\n", "+CSQ: ", 250) == GPRS_SEND_OK)
	{
		uint8_t *p = (uint8_t*)strstr((char*)gprs_at_buff, "+CSQ: ");
		if(p != NULL)
		{
			uint32_t rssi = 0, ber = 0;
			if(sscanf((char*)p, "+CSQ: %d,%d", &rssi, &ber) == 2)
			{
				if(rssi != 99)
				{
					sg_gprs_log_t.csq = (uint8_t)rssi;
					sg_gprs_status_t.status.csq = (uint8_t)rssi;
				}
				else
				{
					sg_gprs_log_t.csq = 0;
				}
			}
		}
	}
}

/*
*********************************************************************************************************
*    函 数 名: gprs_sim_status_monitoring_function
*    功能说明: SIM卡状态监测
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void gprs_sim_status_monitoring_function(void)
{
#if configUSE_EXT_SIM == 1
    if(GPRS_DET_READ == 0)
    {
        if(sg_gprs_status_t.sim_status == SIM_EXT)
        {
            GPRS_Sel_L;
            gprs_module_restart_function();  // 重启模块
        }
    }
    else
    {
        if(sg_gprs_status_t.sim_status == SIM_INT)
        {
            GPRS_Sel_H;
            gprs_module_restart_function();  // 重启模块
        }
    }
#endif
}

/*
*********************************************************************************************************
*    函 数 名: gprs_module_restart_function
*    功能说明: 模块重启函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void gprs_module_restart_function(void)
{
	gprs_network_disconnect_function(GPRS_LINK_DATA);
	gprs_network_disconnect_function(GPRS_LINK_OTA);
	gprs_network_disconnect_function(GPRS_LINK_FILE);
	sg_gprs_status_t.mount = 0;
}


/*
*********************************************************************************************************
*    函 数 名: gprs_network_status_monitoring_function
*    功能说明: 网络状态监测函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t gprs_network_status_monitoring_function(void)
{
	uint32_t temp1;
	uint32_t temp2;
	uint8_t  res = 0;
	uint8_t  *p1 = 0;
	
	if(gprs_send_at("AT+CEREG?\r\n", "+CEREG:", 200) == GPRS_SEND_OK) 
	{
		p1 = (uint8_t*)strstr((char*)gprs_at_buff,"+CEREG:");
		res = sscanf((char*)p1,"+CEREG: %d,%d",&temp1,&temp2);
		
		if(res == 2 && (temp2 == 1 || temp2 == 5)) 
		{
			gprs_send_cmd_over_function();
			return 0;
		}
	}
	gprs_send_cmd_over_function();
	return -1;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_module_status_function
*    功能说明: 获取模块状态
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t gprs_get_module_status_function(void)
{
	return sg_gprs_status_t.mount;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_module_init_state
*    功能说明: 获取模块初始化状态
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t gprs_get_module_init_state(void)
{
    switch(sg_gprs_status_t.step) {
        case GPRS_SIM:
            return 1; // 查找sim卡
        case GPRS_CFUN: 
					return 6; // 查询协议栈
        case GPRS_CSQ:
            return 2; // 查找信号
        case GPRS_CEREG:
            return 3; // 注册网络
        case GPRS_CCLK:
            return 4; // 同步时间
        case GPRS_MIPCCLK:
					return 7; // 查询拨号状态
        case GPRS_PDP:
            return 5; // 激活网络
        default:
            return 0; // 模块初始化
    } 
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_tcp_status
*    功能说明: 获取TCP连接状态
*    形    参: 客户端ID
*    返 回 值: 连接状态（1：已连接，0：未连接）
*********************************************************************************************************
*/	
uint8_t gprs_get_tcp_status(GPRS_LINK_E client_id)
{
	if(client_id >= GPRS_LINK_MAX){ return 0; }
	return sg_gprs_status_t.network[client_id];
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_csq_function
*    功能说明: 获取模块信号强度
*    形    参: 
*    返 回 值: 信号强度
*********************************************************************************************************
*/
uint8_t gprs_get_csq_function(void)
{
	return sg_gprs_status_t.status.csq;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_ip_function
*    功能说明: 获取ip地址信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void *gprs_get_ip_addr_function(void)
{
	return sg_gprs_status_t.status.ip;
}


/*
*********************************************************************************************************
*    函 数 名: gprs_get_infor_data_function
*    功能说明: 获取模块数据指针
*    形    参: 
*    返 回 值: 指针
*********************************************************************************************************
*/
void* gprs_get_infor_data_function(void)
{
	return &sg_gprs_status_t;
}
/*
*********************************************************************************************************
*    函 数 名: gprs_get_log_function
*    功能说明: 获取模块日志数据
*    形    参: 无
*    返 回 值: 日志结构体指针
*********************************************************************************************************
*/
void *gprs_get_log_function(void)
{
    return &sg_gprs_log_t;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_ccid_function
*    功能说明: 获取卡号
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t *gprs_get_ccid_function(void)
{
	return sg_gprs_status_t.ccid;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_model_soft_function
*    功能说明: 获取模块型号
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t *gprs_get_model_soft_function(void)
{
	return sg_gprs_status_t.model;
}
/*
*********************************************************************************************************
*    函 数 名: gprs_get_imei_function
*    功能说明: 获取模块imei
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t *gprs_get_imei_function(void)
{
	return sg_gprs_status_t.imei;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_send_data
*    功能说明: 发送数据
*    形    参: @data        : 数据指针
*    @len         : 数据长度
*    @client_id   : 客户端ID
*    @waittime    : 等待时间（毫秒）
*    返 回 值: 发送状态
*********************************************************************************************************
*/
int gprs_send_data(uint8_t *data, int len, GPRS_LINK_E client_id, int waittime)
{
	int res = 0;
	char AT_cmd[128];

	if(!data || !len){ return(GPRS_SEND_OK); }
	if(client_id >= GPRS_LINK_MAX){ return(GPRS_SEND_ERROR); }

	/* 持锁: MIPSEND 三段式必须整段原子, 防止其他链路在 '>' 与数据之间插入 */
	if(g_gprs_at_mutex != NULL){ xSemaphoreTakeRecursive(g_gprs_at_mutex, portMAX_DELAY); }

	// (1) 发送 MIPSEND 指令, 等待 "\r\n>\r\n"
	sprintf(AT_cmd, "AT+MIPSEND=%d,%d\r\n", (int)client_id, len);
	res = gprs_send_at(AT_cmd, "\r\n>\r\n", waittime);
	if(res != GPRS_SEND_OK)
	{
		if(g_gprs_at_mutex != NULL){ xSemaphoreGiveRecursive(g_gprs_at_mutex); }
		return(res);
	}

	// (2) 发送实际数据
	s_gprs_at_idx  = 0;
	s_gprs_at_take = 0;
	sg_gprs_status_t.cmdon = 1;
	GPRS_STR_SEND((uint8_t *)data, (uint16_t)len);

	// 等待 "\r\n+MIPSEND: client_id,len\r\n"
	sprintf(AT_cmd, "\r\n+MIPSEND: %d,%d\r\n", (int)client_id, len);
	res = gprs_wait_feedback((unsigned char *)AT_cmd, strlen(AT_cmd), waittime);
	if(res != GPRS_SEND_OK){ sg_gprs_status_t.cmdon = 0; if(g_gprs_at_mutex != NULL){ xSemaphoreGiveRecursive(g_gprs_at_mutex); } return(res); }

	// 等待 "\r\nOK\r\n"
	res = gprs_wait_feedback((unsigned char *)"\r\nOK\r\n", 6, waittime);
	sg_gprs_status_t.cmdon = 0;

	if(g_gprs_at_mutex != NULL){ xSemaphoreGiveRecursive(g_gprs_at_mutex); }
	return(res);
}
/*
*********************************************************************************************************
*    函 数 名: gprs_network_data_send_function
*    功能说明: 网络数据发送函数
*    形    参: @data        : 数据指针
*    返 回 值: 数据长度
*********************************************************************************************************
*/
uint8_t gprs_network_data_send_function(uint8_t *data, uint16_t len)
{
//    printf("gprs_network_data_send_function: %s, %d\n", (char *)data, len);
    return (uint8_t)gprs_send_data(data, len, GPRS_LINK_DATA, 5000);
}

/*
*********************************************************************************************************
*    函 数 名: gprs_wait_feedback
*    功能说明: 等待服务器反馈
*    形    参: @feedback    : 反馈字符串指针
*    @feedback_len: 反馈字符串长度
*    返 回 值: 等待状态
*********************************************************************************************************
*/
static int gprs_wait_feedback(const unsigned char *feedback, int feedback_len, int waittime)
{
	char *pt;
	char *disconn_pt;

	if(!feedback || !feedback_len){ return(GPRS_SEND_OK); } // 空操作

	while(waittime >= 0)
	{
		unsigned short avail = s_gprs_at_idx - s_gprs_at_take;

		/* +CME ERROR: 终结性错误响应, 不受期望回馈长度门限限制, 命中即提前失败,
		 * 避免短错误响应在等待长回馈(如 +MIPSEND)时被拖到超时;
		 * 550(TCP发送失败)视为链路断开, 其它(如551连接失败)返回一般错误, 由已知 client_id 的调用方处理 */
		if(avail > 0)
		{
			pt = strstr((char *)(gprs_at_buff + s_gprs_at_take), "+CME ERROR:");
			if(pt)
			{
				if(atoi(pt + 11) == 550){ return(GPRS_SEND_DISCONN); }
				return(GPRS_SEND_ERROR);
			}
		}

		/* 只有在有新数据到达后才搜索匹配 */
		if(avail >= (unsigned short)feedback_len)
		{
			pt = strstr((char *)(gprs_at_buff + s_gprs_at_take), (const char *)feedback);
			if(pt)
			{
				s_gprs_at_take = (pt + feedback_len - (char *)gprs_at_buff);
				return(GPRS_SEND_OK);
			}

			disconn_pt = strstr((char *)(gprs_at_buff + s_gprs_at_take), "\r\n+MIPURC: \"disconn\",");
			if(disconn_pt)
			{
				pt = strstr(disconn_pt + 21, "\r\n");
				if(pt){ s_gprs_at_take = (pt + 2 - (char *)gprs_at_buff); }
				return(GPRS_SEND_DISCONN);
			}

			return(GPRS_SEND_ERROR);
		}

		if(waittime <= 0){ break; }
		GPRS_DELAY_MS(5);
		waittime -= 5;
	}

	return(GPRS_SEND_TIMEOUT);
}

/*
*********************************************************************************************************
*    函 数 名: gprs_network_connect_server
*    功能说明: 连接服务器
*    形    参: host: 服务器 IP 地址
*    port: 服务器端口号
*    client_id: 连接 ID
*    返 回 值: 
*********************************************************************************************************
*/
int gprs_network_connect_server(uint8_t *host, uint16_t port, GPRS_LINK_E client_id)
{
	uint8_t buff[128] = {0};
	char feedback_buf[32];
	int res = 0;
	struct GPRS_FEEDBACK feedback_array[2]=
	{
		{(uint8_t *)"\r\nOK\r\n", 6},
		{(uint8_t *)feedback_buf, 0}
	};
	sprintf(feedback_buf, "\r\n+MIPOPEN: %d,0\r\n", (int)client_id);
	feedback_array[1].feedback_len = strlen(feedback_buf);

	// 发送连接命令
	sprintf((char*)buff, "AT+MIPOPEN=%d,\"TCP\",\"%s\",%d,100,0\r\n", (int)client_id, host, port);
	res = gprs_send_cmd((uint8_t*)buff, strlen((char *)buff), feedback_array, 2, 1000);
	if(res == GPRS_SEND_OK){ sg_gprs_status_t.disconn[client_id] = 0; } /* 新连接建立, 清除旧断链标记 */
	return(res);
}
/*
*********************************************************************************************************
*    函 数 名: gprs_network_disconnect_function
*    功能说明: 断开当前连接
*    形    参: client_id: 连接 ID
*    返 回 值: 
*********************************************************************************************************
*/
int8_t gprs_network_disconnect_function(GPRS_LINK_E client_id)
{
    uint8_t buff[128] = {0};
    char feedback_buf[32];
    int res = 0;

    struct GPRS_FEEDBACK feedback_array[2]=
    {
        {(uint8_t *)"\r\nOK\r\n", 6},
        {(uint8_t *)feedback_buf, 0}
    };
    
    sprintf(feedback_buf, "+MIPCLOSE: %d\r\n", (int)client_id);
    feedback_array[1].feedback_len = strlen(feedback_buf);
    
    sprintf((char*)buff, "AT+MIPCLOSE=%d\r\n", (int)client_id);
    res = gprs_send_cmd((uint8_t*)buff, strlen((char*)buff), feedback_array, 2, 1000);
    /* MIPCLOSE 已下发后本地一律视为断开;超时多为 +MIPCLOSE URC 未及时匹配,模块侧 socket 通常已关 */
	sg_gprs_status_t.network[client_id] = 0;
	sg_gprs_status_t.disconn[client_id] = 0; /* 主动断开, 清除待消费断链标记 */
    return res;
}

/*
*********************************************************************************************************
*	函 数 名: gprs_send_cmd
*	功能说明: 发送单一 AT 指令
*	形    参: AT_cmd: AT 指令
*    AT_cmd_len: AT 指令长度
*    feedback_array: 回馈数组
*    feedback_count: 回馈数量
*    waittime: 等待时间
*	返 回 值:  无
*********************************************************************************************************
*/
int gprs_send_cmd(uint8_t *AT_cmd,int AT_cmd_len,struct GPRS_FEEDBACK *feedback_array,uint8_t feedback_count,int waittime)
{
	int res = 0;
	unsigned int ii;

	/* 持锁: 序列化整条 AT 往返, 与其他链路命令时分复用 */
	if(g_gprs_at_mutex != NULL){ xSemaphoreTakeRecursive(g_gprs_at_mutex, portMAX_DELAY); }

	// 发送指令
	if(AT_cmd && (AT_cmd_len > 0))
	{
		s_gprs_at_idx  = 0;
			s_gprs_at_take = 0;
			memset(gprs_at_buff, 0, sizeof(gprs_at_buff));
			sg_gprs_status_t.cmdon = 1;

			GPRS_STR_SEND( (uint8_t *)AT_cmd, (uint16_t)AT_cmd_len);
	}

	// 等待回馈
	if(!feedback_array || !feedback_count)
	{
		sg_gprs_status_t.cmdon = 0;
		if(g_gprs_at_mutex != NULL){ xSemaphoreGiveRecursive(g_gprs_at_mutex); }
		return(GPRS_SEND_OK);
	}

	for(ii=0; ii<feedback_count; ii++)
	{
		if( !(feedback_array[ii].feedback) || !(feedback_array[ii].feedback_len) ){ continue; }

		res = gprs_wait_feedback(feedback_array[ii].feedback, feedback_array[ii].feedback_len, waittime);
		if(res != GPRS_SEND_OK){ sg_gprs_status_t.cmdon = 0; if(g_gprs_at_mutex != NULL){ xSemaphoreGiveRecursive(g_gprs_at_mutex); } return(res); }
	} //for()

	sg_gprs_status_t.cmdon = 0;
	if(g_gprs_at_mutex != NULL){ xSemaphoreGiveRecursive(g_gprs_at_mutex); }
	return(GPRS_SEND_OK);
}
/*
*********************************************************************************************************
*	函 数 名: gprs_send_cmd_over_function
*	功能说明: 退出命令发送函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void gprs_send_cmd_over_function(void)
{
	s_gprs_at_idx  = 0;
	s_gprs_at_take = 0;
	sg_gprs_status_t.cmdon = 0;
	memset(gprs_at_buff, 0, sizeof(gprs_at_buff));
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_receive_data_function
*    功能说明: 获取接收到的数据
*    形    参: buff: 数据缓冲区
*    len: 数据长度
*    返 回 值: 
*********************************************************************************************************
*/
void gprs_get_receive_data_function(uint8_t *buff, uint16_t len)
{
	int16_t cur_data_len = 0;
	char *pt = NULL;
	unsigned short gprs_data_len = 0;
	unsigned short client_id = 0;

	if( (len == 0) || (buff == NULL) ) { return; }

	/* 1. rtcp URC 优先分发: 报文自带 client_id, 无论是否处于 AT 命令模式都按 id 路由,
	 *    避免并发时其它链路的下行数据被当作 AT 应答吞入 gprs_at_buff 而丢失 */
	if(strncmp((char *)buff, "\r\n+MIPURC: \"rtcp\",", 18) == 0)
	{
		pt = (char *)buff + 18;

		/* 解析 client_id */
		client_id = (unsigned short)atoi(pt);
		pt = strchr(pt, ',');
		if(!pt){ return; }
		pt++;

		/* 解析数据长度 */
		gprs_data_len = atoi(pt);
		if(!gprs_data_len){ return; }

		pt = strchr(pt, ',');
		if(!pt){ return; }
		pt++; // 此时指向真实数据

		/* 根据 client_id 路由分发 */
		switch(client_id)
		{
			case GPRS_LINK_DATA: // 0: 自定义数据
				com_rx_feed(COM_CH_GSM, (uint8_t *)pt, gprs_data_len);
				break;

			case GPRS_LINK_OTA:  // 1: OTA升级
			case GPRS_LINK_FILE: // 2: 文件上传
			{
				gprs_rx_t *p_rx = &sg_gprs_rx_t[client_id];

				cur_data_len = 0;
				if(p_rx->status & 0x8000){ cur_data_len = (p_rx->status & 0x7fff); }
				if( (cur_data_len + gprs_data_len) >= GPRS_DATA_BUFF_SIZE ){ return; }

				memcpy( (p_rx->buff + cur_data_len), pt, gprs_data_len );
				cur_data_len += gprs_data_len;
				p_rx->status = (cur_data_len | 0x8000);
				p_rx->buff[cur_data_len] = 0;
			}
			break;

			default:
				break;
		}
		return;
	}

	/* 1b. disconn URC 优先处理: 报文自带 client_id, 按 id 异步置断链标记, 与 cmdon 无关, 不进 AT 缓冲,
	 *     供各链路 gprs_recv_data / 上层按 id 独立感知断开, 互不干扰 */
	if(strncmp((char *)buff, "\r\n+MIPURC: \"disconn\",", 21) == 0)
	{
		client_id = (unsigned short)atoi((char *)buff + 21);
		if(client_id < GPRS_LINK_MAX)
		{
			sg_gprs_status_t.disconn[client_id] = 1;
		}
		return;
	}

	/* 2. 非 rtcp 数据: 若有命令在途, 作为 AT 应答存入 gprs_at_buff, 供 gprs_wait_feedback 匹配
	 *    (含 OK/ERROR/+CEREG/+CSQ/+MIPOPEN/+MIPCLOSE/disconn 等, 靠"唯一在途命令"归属, 无需 client 区分) */
	if(sg_gprs_status_t.cmdon == 1)
	{
		if( (s_gprs_at_idx + len) >= GPRS_AT_BUFF_SIZE ){ return; }

		memcpy( (gprs_at_buff + s_gprs_at_idx), buff, len );
		s_gprs_at_idx += len;
		gprs_at_buff[s_gprs_at_idx] = 0;
		return;
	}
}

/*
*********************************************************************************************************
*    函 数 名: gprs_recv_data
*    功能说明: 从指定链路缓冲区读取一段接收到的数据
*    形    参: @client_id  : 客户端ID
*    @recv_data   : 接收数据指针
*    @recv_data_size: 接收数据长度指针
*    返 回 值: 接收状态
*********************************************************************************************************
*/
int gprs_recv_data(GPRS_LINK_E client_id, const unsigned char **recv_data, int *recv_data_size)
{
	unsigned short data_len = 0;
	gprs_rx_t *p_rx;

	if(recv_data){ (*recv_data) = NULL; }
	if(recv_data_size){ (*recv_data_size) = 0; }
	if(client_id >= GPRS_LINK_MAX){ return(GPRS_SEND_ERROR); }

	p_rx = &sg_gprs_rx_t[client_id];
	if( (p_rx->status & 0x8000) && ((p_rx->status & 0x7fff) > 0) )
	{
		data_len = (p_rx->status & 0x7fff);
		if(recv_data){ (*recv_data) = p_rx->buff; }
		if(recv_data_size){ (*recv_data_size) = data_len; }
		p_rx->status = 0;
		p_rx->take_point = 0;
		return(GPRS_SEND_OK);
	}

	/* 无数据: 若该链路收到过 disconn URC, 消费标记并上报断链(先取数据后报断链, 保证已缓冲数据不丢) */
	if(sg_gprs_status_t.disconn[client_id])
	{
		sg_gprs_status_t.disconn[client_id] = 0;
		return(GPRS_SEND_DISCONN);
	}

	return(GPRS_SEND_ERROR);
}
///////////////////


/*
当*start！='\0'的时候，就把start赋值给s1，让他去查找,把str2赋值给s2
让s2也从起始位置开始，然后循环的判断条件 *s1 != '\0' && *s2 != '\0' && *s1 == *s2
然后s1和s2进行加加，加加完了之后，再上去判断，当有一次，s1或者s2
等于'\0’的时候，或者他们不相等的时候，就跳出来，如果*str2=='\0'的时候，就是找到了，然后跳出来
如果找不到的话，就返回空指针。
如果要找一个空字符串的话（特殊情况）：
在库里面，对于这种特殊情况的处理，就是直接返回str1
*/
char* my_strstr(const char* str1, const char* str2)
{
	const char* s1 = str1;
	const char* s2 = str2;
	const char* start = str1;
	if (*str2 == '\0')
	{
		return (char *)str1;   //找空字符串，直接返回str1
	}
	while (*start!='\0')//当start遇到'\0'的时候就没有比要再继续查找了，那一定是查找不到的了
	{
		s2 = str2;
		s1 = start;
		while ( *s1 == *s2)
		{
			s1++;
			s2++;
		}
		if (*s2 == '\0')
		{
			return (char *)start;
		}
		start++;
	}
	return NULL;
}


