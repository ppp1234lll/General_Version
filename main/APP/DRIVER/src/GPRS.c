/*
*********************************************************************************************************
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
*********************************************************************************************************
*/
#include "./Driver/inc/GPRS.h"
#include "main.h"
#include "appconfig.h"
#include "Task/inc/gprs_rx.h"
////

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

/* ???????? */
#define GPRS_BAUDRATE               (115200)
#define GPRS_UART_INIT(baudrate)    bsp_InitUsart1(baudrate)
#define GPRS_STR_SEND(data,len)     usart1_send_str(data,len)
////

static int gprs_wait_feedback(const unsigned char *feedback, int feedback_len, int waittime, int client_id);
static int gprs_wait_mipopen_urc(int client_id, int waittime, uint16_t search_start);
static void gprs_reset_link_rx_stream(GPRS_LINK_E client_id);
static void gprs_compact_at_rx_buff(void);
/* 持有 g_gprs_rx_mutex 时调用：把 gprs_rx_buff[0..cur] 快照到调用方私有 buffer，
 * 解析私有副本以解除共享缓冲区的竞争。resp_out 非 NULL 且 resp_cap>0 时拷 min(cur,cap-1) 字节并补 '\0'。 */
static void gprs_snapshot_resp(uint8_t *resp_out, int resp_cap);

/*
*********************************************************************************************************
*    函 数 名: my_memmem
*    功能说明: 在指定长度的内存区域中搜索字节序列(长度感知,替代strstr以支持二进制数据)
*    形    参: haystack : 搜索区域, haystack_len : 搜索区域长度
*              needle   : 搜索模式, needle_len   : 模式长度
*    返 回 值: 匹配位置指针; 失败返回NULL
*********************************************************************************************************
*/
static void *my_memmem(const void *haystack, size_t haystack_len,
                       const void *needle,   size_t needle_len)
{
    const unsigned char *h;
    const unsigned char *end;

    if(!needle_len)           { return (void *)haystack; }
    if(haystack_len < needle_len) { return NULL; }

    h   = (const unsigned char *)haystack;
    end = h + haystack_len - needle_len;

    while(h <= end)
    {
        if(h[0] == ((const unsigned char *)needle)[0]
            && memcmp(h, needle, needle_len) == 0)
        {
            return (void *)h;
        }
        h++;
    }
    return NULL;
}
////

// AT 指令应答缓冲(驱动内部私有,仅用于 gprs_wait_feedback 匹配 AT 回显;
// 用户数据 URC 已通过 gprs_get_receive_data_function 分发到各链路独立缓冲区。
// 解析每次应答由 gprs_send_cmd 调用 gprs_snapshot_resp 拷到调用方私有 buffer,不再对外暴露。)
static uint16_t gprs_rx_status = 0;
static uint8_t  gprs_rx_buff[GSM_RX_BUFF_SIZE];
static uint16_t gprs_rx_take_point = 0;

// OTA升级接收数据缓冲(链路1)
uint16_t gprs_ota_rx_status = 0;
uint8_t  gprs_ota_rx_buff[GSM_RX_BUFF_SIZE];
uint16_t gprs_ota_rx_take_point = 0;

// 文件上传接收数据缓冲(链路2)
uint16_t gprs_file_rx_status = 0;
uint8_t  gprs_file_rx_buff[GSM_RX_BUFF_SIZE];
uint16_t gprs_file_rx_take_point = 0;  

struct gprs_status_t sg_gprs_status_t = {0};
gprs_log_t sg_gprs_log_t = {0};

/* AT 指令通道互斥锁: 串行化所有 AT 指令(connect/disconnect/send 的指令-响应往返),
 * 防止多条链路并发握手时共享 gprs_rx_buff 导致响应错配。
 * 注意: 此锁为普通互斥锁(非递归),禁止在持有此锁时调用任何会再次取锁的函数
 *      (如 gprs_send_cmd/gprs_send_data/gprs_network_connect/disconnect),
 *      否则必死锁。当前调用链为扁平结构(上层不持锁),安全。 */
SemaphoreHandle_t g_gprs_at_mutex = NULL;
/*
*********************************************************************************************************
*    函 数 名: gprs_gpio_init_function
*    功能说明: 引脚初始化函数
*    形    参: 无
*    返 回 值: 无
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
*    函 数 名: gprs_init_function
*    功能说明: 初始化函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void gprs_init_function(void)
{
    gprs_gpio_init_function();
    gprs_rx_streambuf_init_function();   /* 创建流缓冲区与接收处理任务(须在使能串口接收前) */

    /* 创建 AT 指令互斥锁(串行化所有 AT 指令,多链路并发安全) */
    if(g_gprs_at_mutex == NULL)
    {
        g_gprs_at_mutex = xSemaphoreCreateMutex();
        configASSERT(g_gprs_at_mutex);
    }

    GPRS_UART_INIT(GPRS_BAUDRATE);
}

/*
*********************************************************************************************************
*    函 数 名: gprs_boot_up_function
*    功能说明: 模块开机函数
*    形    参: 无
*    返 回 值: 无
*    ML307: 拉低PWR_ON/OFF引脚2s~3.5s使模组开机
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
*    函 数 名: gprs_shutdown_function
*    功能说明: 模块关机函数
*    形    参: 无
*    返 回 值: 无
*     EC800E: RESET拉低至少50ms，或者PWR拉低至少650ms
*     ML307: 拉低PWR_ON/OFF引脚3.5s~4s后释放，模组将执行关机流程
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
*    函 数 名: gprs_reset_function
*    功能说明: 重启函数
*    形    参: 无
*    返 回 值: 无
*    ML307: 拉低RESET引脚至少300ms或更长时间实现系统复位
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
*    函 数 名: gprs_v_reset_function
*    功能说明: 断电重启函数
*    形    参: 无
*    返 回 值: 无
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
*    函 数 名: gprs_parse_cme_error
*    功能说明: 从接收缓冲区解析+CME ERROR错误码,写入sg_gprs_log_t
*    形    参: 无 (读取全局gprs_rx_buff)
*    返 回 值: 0-未匹配到  非0-匹配到的错误码
*********************************************************************************************************
*/
static uint16_t gprs_parse_cme_error_from(const uint8_t *buf)
{
    uint16_t err_code = 0;
    char *perr = NULL;

    if(buf == NULL) { return 0; }
    /* 解析调用方私有快照,不再读共享 gprs_rx_buff,无需取锁 */
    perr = strstr((char*)buf, "+CME ERROR: ");
    if(perr != NULL)
    {
        sscanf(perr, "+CME ERROR: %hd", &err_code);
        sg_gprs_log_t.errors = err_code;
    }
    return err_code;
}

/* 持 g_gprs_rx_mutex 时调用:把 gprs_rx_buff 有效区快照到调用方私有 buffer。
 * 调用方解析私有副本,不再读共享 gprs_rx_buff,解除多链路并发解析竞争(隐患1机制级修复)。
 * 快照在 send_cmd 持锁段内完成,与生产者 gprs_rx_task 互斥,保证一致性。 */
static void gprs_snapshot_resp(uint8_t *resp_out, int resp_cap)
{
    if((resp_out == NULL) || (resp_cap <= 0)) { return; }

    unsigned short cur = (gprs_rx_status & 0x7fff);
    int copy_len = (int)cur;
    if(copy_len > (resp_cap - 1)) { copy_len = resp_cap - 1; }
    if(copy_len < 0) { copy_len = 0; }
    memcpy(resp_out, gprs_rx_buff, (size_t)copy_len);
    resp_out[copy_len] = 0;
}

/* 持锁清空 gprs_rx_buff(含 status/take_point),供 gprs_status_check_function
 * 各 case 起始调用。防止与 gprs_rx_task 并发写入 memcpy 撕扯(问题2)。 */
static void gprs_clear_rx_buff(void)
{
    xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
    memset(gprs_rx_buff, 0, sizeof(gprs_rx_buff));
    gprs_rx_status = 0;
    gprs_rx_take_point = 0;
    xSemaphoreGive(g_gprs_rx_mutex);
}

/* AT 应答缓冲紧凑: 指令成功且 take_point 已追上有效长度时归零,
 * 释放已扫描完区,降低长期运行后 gprs_rx_buff 溢出风险。
 * 须在持 g_gprs_rx_mutex 时调用。 */
static void gprs_compact_at_rx_buff(void)
{
    unsigned short cur = (gprs_rx_status & 0x7fff);

    if(gprs_rx_take_point >= cur)
    {
        gprs_rx_status = 0;
        gprs_rx_take_point = 0;
        gprs_rx_buff[0] = 0;
    }
}

uint8_t gprs_at_cmdon_active(void)
{
    return (uint8_t)(sg_gprs_status_t.cmdon[0]
                  || sg_gprs_status_t.cmdon[1]
                  || sg_gprs_status_t.cmdon[2]);
}

/*
*********************************************************************************************************
*    函 数 名: gprs_send_at
*    功能说明: 发送单条AT指令并等待单个回馈(子串匹配)
*             统一归口到第二套方案(gprs_send_cmd)。ack为空时只发送不等待。
*    形    参: @cmd         : AT指令字符串(含\r\n),为NULL时不发送
*             @ack          : 期望回馈子串,为NULL/空时只发送不等待
*             @waittime_ms  : 等待回馈超时(ms)
*    返 回 值: 0-成功(收到期望回馈)  非0-失败(超时/异常/断连)
*********************************************************************************************************
*/
static int gprs_send_at(const char *cmd, const char *ack, int waittime_ms,
                        uint8_t *resp_out, int resp_cap)
{
    struct GPRS_FEEDBACK fb[1];

    if(!ack || !ack[0])
    {
        return gprs_send_cmd((const uint8_t *)cmd, cmd ? (int)strlen(cmd) : 0, NULL, 0, 0, -1, resp_out, resp_cap);
    }

    fb[0].feedback = (const unsigned char *)ack;
    fb[0].feedback_len = (unsigned int)strlen(ack);
    return gprs_send_cmd((const uint8_t *)cmd, cmd ? (int)strlen(cmd) : 0, fb, 1, waittime_ms, -1, resp_out, resp_cap);
}

/*
*********************************************************************************************************
*    函 数 名: gprs_deinit_function
*    功能说明: 初始化-清除变量
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void gprs_deinit_function(void)
{
    memset(&sg_gprs_status_t,0,sizeof(struct gprs_status_t));
}

/*
*********************************************************************************************************
*    函 数 名: gprs_status_check_function
*    功能说明: 状态监测函数
*    形    参: 无
*    返 回 值: 无

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
    /* 调用方私有应答快照:send_at/send_cmd 持锁拷出本次应答,本函数解析私有副本,
     * 不再读共享 gprs_rx_buff,解除多链路并发解析竞争(隐患1机制级修复)。各 case 用 break
     * 分隔不交错,单一缓冲在 case 内 send 后立即消费即可复用。 */
    uint8_t resp[GPRS_RESP_SNAPSHOT_MAX];

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
            if(gprs_send_at("AT\r\n","\r\nOK\r\n",250,NULL,0) == 0)
            {
                gprs_send_at("ATE0\r\n",NULL,0,NULL,0); // 关闭回显
                sg_gprs_status_t.step = GPRS_SIM;
                sg_gprs_status_t.status.com = 1; // 通信正常
//                GPRS_DELAY_MS(2010); // 模组开机返回+MATREADY后，间隔至少2s才能执行AT+CFUN=0或AT+CFUN=1
                repeat = 0;
            }
            else 
            {
                GPRS_DELAY_MS(10);
                sg_gprs_status_t.status.com = 0; // 通信异常：模块未启动、串口异常等
                repeat++;
                if(repeat > 30) 
                {
                    sg_gprs_log_t.init_step = GPRS_COMM_CHECK;
                    sg_gprs_status_t.step = GPRS_INIT;
                }
            }
            break;
        case GPRS_SIM:
            /* SIM卡状态检测 */
            gprs_clear_rx_buff();
            if(gprs_send_at("AT+CPIN?\r\n","READY",1000,resp,sizeof(resp)) == 0)
            {
                for(index=0; index<3; index++)
                {
                    res = gprs_send_at("AT+ICCID\r\n","+ICCID:",1000,resp,sizeof(resp));
                    if(res == 0)
                    {
                        p1 = (uint8_t*)strstr((char*)resp,"+ICCID: ");
                        if(p1 != NULL)
                        {
                            p1 += 8;
                            memcpy(sg_gprs_status_t.ccid,p1,20);
                        }
                        break;
                    }
                }
                sg_gprs_status_t.step = GPRS_CFUN;
                sg_gprs_status_t.status.sim = 1;  /* SIM卡正常 */
                repeat = 0;
            }
            else
            {
                /* 未收到READY,根据应答区分SIM状态 */
                uint8_t sim_not_inserted = (strstr((char*)resp, "+CME ERROR: 10") != NULL);
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
            gprs_clear_rx_buff();
            if(gprs_send_at("AT+CFUN?\r\n","+CFUN: 1",250,resp,sizeof(resp)) == 0) {
                sg_gprs_status_t.step = GPRS_CEREG;
                repeat = 0;
            }
            else
            {
                gprs_parse_cme_error_from(resp); // 解析错误码
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
            gprs_clear_rx_buff();
            if(gprs_send_at("AT+CEREG?\r\n","+CEREG:",250,resp,sizeof(resp)) == 0)
            {
                /* 解析私有快照 +CEREG(不再读共享 gprs_rx_buff,无需取锁) */
                p1 = (uint8_t*)strstr((char*)resp,"+CEREG:");
                temp2 = 0;
                temp1 = 0;
                res = (p1 ? sscanf((char*)p1,"+CEREG: %d,%d",&temp1,&temp2) : 0);

                if(temp1 == 0 && res == 2)
                {
                    gprs_send_at("AT+CEREG=2\r\n",NULL,0,NULL,0); //启用带有位置信息的网络注册 URC
                }

                if((temp2 == 1 || temp2 == 5) && res == 2)
                {
                    sg_gprs_status_t.status.net = 1;
                    sg_gprs_status_t.step = GPRS_CCLK;
                    repeat = 0;
                }
                else
                {
                    gprs_parse_cme_error_from(resp); // 解析错误码
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
            gprs_clear_rx_buff();
            if(gprs_send_at("AT+CCLK?\r\n","+CCLK: ",250,resp,sizeof(resp)) == 0) {
                p1 = (uint8_t*)strstr((char*)resp,"+CCLK: ");
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
        case GPRS_MIPCCLK:  // 首先判断是否激活，未激活则手动激活
            if(gprs_send_at("AT+MIPCALL?\r\n","+MIPCALL:",1000,resp,sizeof(resp)) == 0)
            {
                p1 = (uint8_t*)strstr((char*)resp,"+MIPCALL:");
                temp2 = 0;
                temp1 = 0;
                res = (p1 ? sscanf((char*)p1,"+MIPCALL: %d,%d",&temp1,&temp2) : 0);
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
                        /* 设置移动APN   AT+CGDCONT=1,"IPV4V6","cmnet" //配置PDP上下文*/
                        gprs_send_at("AT+CGDCONT=1,\"IP\",\"CMIOT\"\r\n",NULL,0,NULL,0);
                        // AT+QICSGP=1,1,"UNINET","","",1
                        // 场景ID  协议类型  APN接入点名称
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
            /* 激活 PDP 场景 */
            gprs_clear_rx_buff();
            {
                /* 一次发送,顺序等待两段回馈:OK + +MIPCALL: 拨号结果 */
                struct GPRS_FEEDBACK fb[2] = {
                    {(const unsigned char *)"\r\nOK\r\n", 6},
                    {(const unsigned char *)"+MIPCALL:", 9}
                };
                res = (uint8_t)gprs_send_cmd((const uint8_t *)"AT+MIPCALL=1,1\r\n", 15, fb, 2, 2000, -1, resp, sizeof(resp));
            }
            if(res == GPRS_SEND_OK)
            {
                p1 = (uint8_t*)strstr((char*)resp,"+MIPCALL: ");
                temp2 = 0;
                temp1 = 0;
                res = (p1 ? sscanf((char*)p1,"+MIPCALL: %d,%d",&temp1,&temp2) : 0);

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
            break;
        case GPRS_CGPADDR:
            /* 获取IP地址 */
            gprs_clear_rx_buff();
            if(gprs_send_at("AT+CGPADDR=1\r\n","+CGPADDR",500,resp,sizeof(resp)) == 0) // 读取场景ID为1 的IP地址
            {
                p1 = (uint8_t*)strstr((char*)resp,"+CGPADDR: ");
                memset(sg_gprs_status_t.status.ip,0,sizeof(sg_gprs_status_t.status.ip));
                res = (p1 ? sscanf((char*)p1,"+CGPADDR: 1,\"%[^\"]",sg_gprs_status_t.status.ip) : 0);
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
            gprs_clear_rx_buff();
            if(gprs_send_at("AT+CGMR\r\n","\r\nOK\r\n",1000,resp,sizeof(resp)) == 0) // 读取模块版本信息
            {
                p1 = (uint8_t*)strstr((char*)resp,"OK");
                if(p1 != NULL)
                {
                    addr_len = p1 - resp - 6;
                    memset(sg_gprs_status_t.model,0,sizeof(sg_gprs_status_t.model));
                    memcpy(sg_gprs_status_t.model,resp+2,addr_len);
                }
            }
            sg_gprs_status_t.step = GPRS_IMEI;
            break;
        case GPRS_IMEI:
            /* 查询模块IMEI */
            gprs_clear_rx_buff();
            if(gprs_send_at("AT+CGSN=1\r\n","+CGSN: ",500,resp,sizeof(resp)) == 0)
            {
                p1 = (uint8_t*)strstr((char*)resp,"+CGSN: ");
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
            gprs_clear_rx_buff();
            if(gprs_send_at("AT+CSQ\r\n","+CSQ: ",250,resp,sizeof(resp)) == 0)
            {
                p1 = (uint8_t*)strstr((char*)resp,"+CSQ: ");
                temp2 = 0;
                temp1 = 0;
                res = (p1 ? sscanf((char*)p1,"+CSQ: %d,%d",&temp1,&temp2) : 0);
                if(temp1 != 99 && res == 2)
                {
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
            return 0; // 初始化完成
            //break;
    }
    /* 正在初始化 */
    return 1;
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
    unsigned int i;
    for(i = 0; i < 3; i++)
    {
        /* 仅对处于连接态的链路发送 AT+MIPCLOSE,避免对已断开链路发命令致
         * 模块回 ERROR 而等不到 +MIPCLOSE,造成每条 1s 超时、整体阻塞 3s。 */
        uint8_t need_close;
        xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
        need_close = sg_gprs_status_t.network[i];
        xSemaphoreGive(g_gprs_rx_mutex);

        if(need_close)
        {
            gprs_network_disconnect_function((GPRS_LINK_E)i);
        }
        else
        {
            /* 已断开: 仅清状态标志与接收缓冲,不发 AT 命令 */
            xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
            sg_gprs_status_t.network[i] = 0;
            sg_gprs_status_t.disconn_pending[i] = 0;
            gprs_reset_link_rx_stream((GPRS_LINK_E)i);
            xSemaphoreGive(g_gprs_rx_mutex);
        }
    }
    sg_gprs_status_t.mount = 0;
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
    /* 复用第二套方案:gprs_send_data 完成 AT+MIPSEND -> '>' -> 数据 -> +MIPSEND -> OK 全流程 */
    return (uint8_t)gprs_send_data(data, len, 5000, GPRS_LINK_DATA);
}

/*
*********************************************************************************************************
*    函 数 名: gprs_check_data_disconn
*    功能说明: 检查 DATA 链路是否异步断开(ISR 置 disconn_pending[0])
*              若有断开事件则清除标志和 network[0], 供调用层触发重连。
*    形    参: 无
*    返 回 值: 0-正常  非0-发生了异步断开(DATA socket 已断)
*********************************************************************************************************
*/
int gprs_check_data_disconn(void)
{
    int ret = 0;

    xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
    if(sg_gprs_status_t.disconn_pending[0])
    {
        sg_gprs_status_t.disconn_pending[0] = 0;
        sg_gprs_status_t.network[0] = 0;
        ret = 1;
    }
    xSemaphoreGive(g_gprs_rx_mutex);

    return ret;
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
    uint32_t temp1 = 0;
    uint32_t temp2 = 0;
    uint8_t  res = 0;
    uint8_t  *p1 = 0;
    uint8_t  resp[GPRS_RESP_SNAPSHOT_MAX];
    struct GPRS_FEEDBACK fb[1] = { {(const unsigned char *)"+CEREG:", 7} };

    if(gprs_send_cmd((const uint8_t *)"AT+CEREG?\r\n", 11, fb, 1, 500, -1, resp, sizeof(resp)) == GPRS_SEND_OK)
    {
        /* 解析调用方私有快照(不再读共享 gprs_rx_buff,无需取锁,解除多链路并发解析竞争) */
        p1 = (uint8_t*)strstr((char*)resp,"+CEREG:");
        if(p1 != NULL)
        {
            res = sscanf((char*)p1,"+CEREG: %d,%d",&temp1,&temp2);
            if(res == 2)
            {
                if(temp2 == 1 || temp2 == 5)
                {
                    return 0;
                }
                else if(temp2 == 3)
                {
                    sg_gprs_log_t.cereg = temp2;
                }
            }
        }
    }
    else
    {
        gprs_parse_cme_error_from(resp);
        return -1;
    }
    return -1;
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
    uint8_t resp[GPRS_RESP_SNAPSHOT_MAX];

    if(HAL_GetTick() - s_tick < 600000)  /* 10分钟查询一次 */
    {
        return ;
    }
    s_tick = HAL_GetTick();

    if(gprs_send_at("AT+CSQ\r\n", "+CSQ: ", 250, resp, sizeof(resp)) == 0)
    {
        /* 解析调用方私有快照(不再读共享 gprs_rx_buff,无需取锁) */
        uint8_t *p = (uint8_t*)strstr((char*)resp, "+CSQ: ");
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

////////////////////
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
*    形    参: 无
*    返 回 值: 0-未初始化  1-已初始化
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
*    函 数 名: gprs_get_csq_function
*    功能说明: 获取模块信号强度
*    形    参: 无
*    返 回 值: 信号强度值
*********************************************************************************************************
*/
uint8_t gprs_get_csq_function(void)
{
    return sg_gprs_status_t.status.csq;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_ip_addr_function
*    功能说明: 获取ip地址信息
*    形    参: 无
*    返 回 值: ip地址指针
*********************************************************************************************************
*/
void *gprs_get_ip_addr_function(void)
{
    return sg_gprs_status_t.status.ip;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_infor_data_function
*    功能说明: 获取模块数据指针
*    形    参: 无
*    返 回 值: 模块状态结构体指针
*********************************************************************************************************
*/
void* gprs_get_infor_data_function(void)
{
    return &sg_gprs_status_t;
}


/*
*********************************************************************************************************
*    函 数 名: gprs_get_ccid_function
*    功能说明: 获取卡号
*    形    参: 无
*    返 回 值: CCID字符串指针
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
*    形    参: 无
*    返 回 值: 型号字符串指针
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
*    形    参: 无
*    返 回 值: IMEI字符串指针
*********************************************************************************************************
*/
uint8_t *gprs_get_imei_function(void)
{
    return sg_gprs_status_t.imei;
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

/* 解析 +MIPURC:"rtcp" URC 头; 兼容 "\r\n+MIPURC" 与裸 "+MIPURC" 两种行首。
 * 成功时 *payload 指向 TCP 载荷, 返回载荷字节数; 失败返回 0。 */
static int gprs_parse_rtcp_urc(const uint8_t *buff, uint16_t len, int *client_id,
                               const uint8_t **payload)
{
    const char *p;
    const char *end;
    int data_len = 0;

    if(!buff || !client_id || !payload || !len){ return 0; }

    if(len >= 10 && !strncmp((char *)buff, "\r\n+MIPURC:", 10))
    {
        p = (char *)buff + 10;
    }
    else if(len >= 8 && !strncmp((char *)buff, "+MIPURC:", 8))
    {
        p = (char *)buff + 8;
    }
    else
    {
        return 0;
    }

    end = (char *)buff + len;
    while(p < end && ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n'))){ p++; }
    if((end - p) < 6 || strncmp(p, "\"rtcp\"", 6)){ return 0; }
    p += 6;
    while(p < end && ((*p == ' ') || (*p == '\t'))){ p++; }
    if(p >= end || *p != ','){ return 0; }
    p++;

    while(p < end && ((*p == ' ') || (*p == '\t'))){ p++; }
    *client_id = atoi(p);
    while(p < end && ((*p >= '0') && (*p <= '9'))){ p++; }
    while(p < end && ((*p == ' ') || (*p == '\t'))){ p++; }
    if(p >= end || *p != ','){ return 0; }
    p++;

    while(p < end && ((*p == ' ') || (*p == '\t'))){ p++; }
    data_len = atoi(p);
    if(data_len <= 0){ return 0; }
    while(p < end && ((*p >= '0') && (*p <= '9'))){ p++; }
    while(p < end && ((*p == ' ') || (*p == '\t'))){ p++; }

    if(p >= end){ return 0; }
    if(*p == ',')
    {
        p++;
    }
    else if(((p + 1) < end) && (p[0] == '\r') && (p[1] == '\n'))
    {
        p += 2;
    }
    else if(*p == '\n')
    {
        p++;
    }
    else
    {
        return 0;
    }

    if((int)((p - (char *)buff) + data_len) > (int)len){ return 0; }

    *payload = (const uint8_t *)p;
    return data_len;
}

int gprs_rtcp_urc_frame_size(const uint8_t *buff, uint16_t avail)
{
    int client_id = 0;
    const uint8_t *payload = NULL;
    int payload_len = gprs_parse_rtcp_urc(buff, avail, &client_id, &payload);

    if(payload_len <= 0 || !payload){ return 0; }
    return (int)((payload - buff) + payload_len);
}

/*
*********************************************************************************************************
*    函 数 名: gprs_rx_ring_read
*    功能说明: 任务上下文从环形队列批量读取

*********************************************************************************************************
*/
void gprs_get_receive_data_function(uint8_t *buff, uint16_t len)
{
    int16_t cur_data_len = 0;
    char *pt = NULL;
    unsigned short gprs_data_len = 0;
    int client_id = 0;
    const uint8_t *payload = NULL;
    ////
    
    if( (len == 0) || (buff == NULL) ) { return; }

    /* 始终优先解析 +MIPURC:"rtcp" URC,不受 AT 指令模式影响。
     * 修复:cmdon 为全局单标志时,链路 A 发 AT 指令期间链路 B 的 URC 数据被
     * 误当作 AT 回显追加至 rx_buff 而丢失。现改为始终解析 URC 再判断 AT 模式。 */
    gprs_data_len = (unsigned short)gprs_parse_rtcp_urc(buff, len, &client_id, &payload);
    if(gprs_data_len && payload)
    {
        pt = (char *)payload;

        if(client_id == GPRS_LINK_DATA) // 数据平台: 直接进 com_stroage_cache_data
        {
            com_stroage_cache_data((uint8_t *)pt, gprs_data_len);
        }
        else
        {
            // 根据 client_id 选择对应的 buffer
            uint8_t  *rx_buff;
            uint16_t *rx_status;
            uint16_t *rx_take_point;

            if(client_id == GPRS_LINK_OTA)
            {
                rx_buff = gprs_ota_rx_buff;
                rx_status = &gprs_ota_rx_status;
                rx_take_point = &gprs_ota_rx_take_point;
            }
            else if(client_id == GPRS_LINK_FILE)
            {
                rx_buff = gprs_file_rx_buff;
                rx_status = &gprs_file_rx_status;
                rx_take_point = &gprs_file_rx_take_point;
            }
            else // client_id 非法(>=3),静默丢弃
            {
                return;
            }

            /* 收到 TCP 载荷说明链路仍活跃: 清除陈旧/误报的 disconn_pending,
             * 避免 OTA/FILE 在 HTTP 分片到达间隔被 recv 误判为断开(ret:-3)。 */
            if(client_id >= 0 && client_id < 3)
            {
                sg_gprs_status_t.disconn_pending[client_id] = 0;
                sg_gprs_status_t.network[client_id] = 1;
            }

            if( !(*rx_status & 0x8000) ){ cur_data_len = 0; }
            else{ cur_data_len = (*rx_status & 0x7fff); }

            if( (cur_data_len + gprs_data_len) >= GSM_RX_BUFF_SIZE )
            {
                if((*rx_take_point) > 0 && (*rx_take_point) < cur_data_len)
                {
                    uint16_t unconsumed = (uint16_t)(cur_data_len - (*rx_take_point));
                    memmove(rx_buff, rx_buff + (*rx_take_point), unconsumed);
                    cur_data_len = unconsumed;
                    (*rx_take_point) = 0;
                }
                else
                {
                    cur_data_len = 0;
                    (*rx_take_point) = 0;
                }
            }
            if( (cur_data_len + gprs_data_len) >= GSM_RX_BUFF_SIZE ){ return; }

            memcpy( (rx_buff + cur_data_len), pt, gprs_data_len );
            cur_data_len += gprs_data_len;
            (*rx_status) = (cur_data_len | 0x8000);
            rx_buff[cur_data_len] = 0;
        }
        return;
    }

    /* 拦截 +MIPURC: "disconn" URC:
     * - 链路型握手(cmdon[client_id]==1 且非通用命令): disconn 进 gprs_rx_buff,
     *   供 gprs_wait_feedback 同步检测,发送/连接路径需立即感知断开。
     * - 通用命令(at_generic_cmd==1): disconn 一律走异步 disconn_pending 路径,
     *   不进 gprs_rx_buff,避免业务链路断开被误当作通用命令失败返回 DISCONN。 */
    if(!strncmp((char *)buff, "\r\n+MIPURC: \"disconn\",", 21))
    {
        pt = (char *)buff + 21;
    }
    else if(!strncmp((char *)buff, "+MIPURC: \"disconn\",", 18))
    {
        pt = (char *)buff + 18;
    }
    else
    {
        pt = NULL;
    }

    if(pt)
    {
        client_id = atoi(pt);

        if(client_id < 0 || client_id >= 3)
        {
            return; // id 非法,丢弃
        }

        if(sg_gprs_status_t.cmdon[client_id] == 1 && sg_gprs_status_t.at_generic_cmd == 0)
        {
            /* 该链路正在链路型 AT 握手: fall through, 追加到 gprs_rx_buff 由 wait_feedback 同步处理 */
        }
        else
        {
            /* 异步断开: 置标志并清连接态, 由 recv_data_* / check_data_disconn 查询;
             * 不追加到 AT 缓冲区。同步清 network,保持与 disconn_pending 状态一致。 */
            sg_gprs_status_t.disconn_pending[client_id] = 1;
            sg_gprs_status_t.network[client_id] = 0;
            return;
        }
    }

    /* 非 URC 数据: 若任一链路处于 AT 指令模式,则原样追加到 rx_buff 供
     * gprs_wait_feedback 匹配反馈。 */
    if(sg_gprs_status_t.cmdon[0] == 1 || sg_gprs_status_t.cmdon[1] == 1 || sg_gprs_status_t.cmdon[2] == 1)
    {
        if( !(gprs_rx_status & 0x8000) ){ cur_data_len = 0; }
        else{ cur_data_len = (gprs_rx_status & 0x7fff); }

        /* 瀹归噺涓嶈冻鏃朵繚鐣? take_point 涔嬪悗鏈?娑堣垂鐨勬暟鎹?(鍙?鑳藉惈灏氭湭鍖归厤鐨? AT 鍝嶅簲鐗囨??),
         * 閬垮厤 cur_data_len 褰掗浂鑰? take_point 浠嶄负鏃у�煎?艰嚧 wait_feedback 姘镐箙绛変笉鍒板搷搴斻�?
         * 涓? OTA/FILE 閾捐矾鐙?绔嬬紦鍐茬殑婧㈠嚭澶勭悊淇濇寔涓�鑷淬�? */
        if((uint32_t)cur_data_len + len >= GSM_RX_BUFF_SIZE)
        {
            if(gprs_rx_take_point > 0 && gprs_rx_take_point < cur_data_len)
            {
                uint16_t unconsumed = (uint16_t)(cur_data_len - gprs_rx_take_point);
                memmove(gprs_rx_buff, gprs_rx_buff + gprs_rx_take_point, unconsumed);
                cur_data_len = unconsumed;
                gprs_rx_take_point = 0;
            }
            else
            {
                /* take_point 已追上或超过 cur_data_len(无未消费数据):整体重置 */
                cur_data_len = 0;
                gprs_rx_take_point = 0;
            }

            /* 重整后仍放不下本次数据:丢弃本次(无法容纳) */
            if((uint32_t)cur_data_len + len >= GSM_RX_BUFF_SIZE)
            {
                gprs_rx_status = (cur_data_len | 0x8000);
                gprs_rx_buff[cur_data_len] = 0;
                return;
            }
        }
        memcpy( (gprs_rx_buff + cur_data_len), buff, len );
        cur_data_len += len;
        gprs_rx_status = (cur_data_len | 0x8000);
        gprs_rx_buff[cur_data_len] = 0;
    }
    /* 否则(非 AT 模式且非 rtcp URC):丢弃,不做处理 */
}
/*
*********************************************************************************************************
*    函 数 名: gprs_send_data
*    功能说明: 发送数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int gprs_send_data(const uint8_t *data, int len, int waittime, GPRS_LINK_E client_id)
{
    int res = 0;
    char AT_cmd[128];
    int AT_cmd_len = 0;
    ////

    if(!data || !len){ return(GPRS_SEND_OK); }

    /* 获取 AT 通道锁: 串行化 AT 指令往返,防止多条链路并发握手时
     * 共用 gprs_rx_buff 导致响应错配。禁止在持锁时递归调用取本锁的函数。 */
    xSemaphoreTake(g_gprs_at_mutex, portMAX_DELAY);

    // (1) 发送指令
    // 跳过 gprs_rx_buff 中已有数据,保留其完整性;gprs_wait_feedback 仅扫描新到达的应答
    xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
    gprs_rx_take_point = (gprs_rx_status & 0x7fff);
    sg_gprs_status_t.cmdon[client_id] = 1;
    sg_gprs_status_t.at_generic_cmd = 0;
    xSemaphoreGive(g_gprs_rx_mutex);

    sprintf(AT_cmd, "AT+MIPSEND=%d,%d\r\n", (int)client_id, len);
    AT_cmd_len = strlen(AT_cmd);
    //printf("\nGPRS_STR_SEND:\n%s\n", AT_cmd);
    GPRS_STR_SEND( (uint8_t *)AT_cmd, (uint16_t)AT_cmd_len);

    // 等待回显 '>' (ML307 下发裸 '>' 或 '> ' 或 "\r\n>\r\n",子串匹配兼容)
    res = gprs_wait_feedback((unsigned char *)">", 1, waittime, (int)client_id);
    switch(res)
    {
        case GPRS_SEND_OK: break;
        default:
            xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
            sg_gprs_status_t.cmdon[client_id] = 0;
            gprs_compact_at_rx_buff();
            xSemaphoreGive(g_gprs_rx_mutex);
            xSemaphoreGive(g_gprs_at_mutex);
            return(res);
    }//switch()

    // (2) 鍙戦�佹暟鎹?
    // 淇濈暀闃舵??(1)缁撴潫鏃? gprs_wait_feedback 鐣欎笅鐨? take_point(宸插湪 ">" 涔嬪悗),
    // 涓嶉噸缃?鍒版湯灏锯�斺�斿惁鍒欓樁娈?(1)(2)涔嬮棿鍒拌揪鐨勬湰閾捐矾 disconn URC 浼氳??璺宠繃涓㈠け,
    // 瀵艰嚧鍙戦�佽矾寰勬劅鐭ヤ笉鍒版柇寮�銆傚瓙涓插尮閰?(+MIPSEND)鑳借嚜鐒惰烦杩囧洖鏄?/鏉傛暎鏁版嵁銆?
    xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
    sg_gprs_status_t.cmdon[client_id] = 1;
    xSemaphoreGive(g_gprs_rx_mutex);

    //printf("\nGPRS_STR_SEND:\n%s\n", (char *)data);
    GPRS_STR_SEND( (uint8_t *)data, (uint16_t)len);

    // 等待回馈
    // "\r\n+MIPSEND: 1,396\r\n\r\nOK\r\n"
    sprintf(AT_cmd, "+MIPSEND: %d,%d\r\n", (int)client_id, len);
    res = gprs_wait_feedback((unsigned char *)AT_cmd, strlen(AT_cmd), waittime, (int)client_id);
    if(res != GPRS_SEND_OK)
    {
        xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
        sg_gprs_status_t.cmdon[client_id] = 0;
        gprs_compact_at_rx_buff();
        xSemaphoreGive(g_gprs_rx_mutex);
        xSemaphoreGive(g_gprs_at_mutex);
        return(res);
    }

    // 等待第二个回馈
    res = gprs_wait_feedback((unsigned char *)("\r\nOK\r\n"), 6, waittime, (int)client_id);
    xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
    sg_gprs_status_t.cmdon[client_id] = 0;
    gprs_compact_at_rx_buff();
    xSemaphoreGive(g_gprs_rx_mutex);

    xSemaphoreGive(g_gprs_at_mutex);
    return(res);
}
/*
*********************************************************************************************************
*    函 数 名: gprs_wait_feedback
*    功能说明: 等待服务器反馈
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
static int gprs_wait_feedback(const unsigned char *feedback, int feedback_len, int waittime, int client_id)
{
    unsigned short cur_data_len = 0;
    char *pt  = NULL;
    char *pt2 = NULL;
    int disconn_id = 0;
    ////

    if(!feedback || !feedback_len){ return(GPRS_SEND_OK); } // 空操作

    // 在剩余数据流中查找期望回馈
    // 采用子串匹配:容错前导URC/回显/分片到达,且不会读到本次命令之前的残留
    // (gprs_rx_buff 在每次接收后被 cur_data_len 处置0,gprs_send_cmd 起始又将 rx_status/take_point 清0)
    while(1)
    {
        /* 互斥保护 gprs_rx_buff / status / take_point 读写访问
         * gprs_rx任务(优先级9)同时在写,须在循环内加锁,不跨 GPRS_DELAY_MS 持锁 */
        xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
        {
            cur_data_len = (gprs_rx_status & 0x7fff);

            /* 越界防抖: take_point 不应超过 cur_data_len。若因缓冲溢出重整/异常
             * 导致 take_point > cur_data_len,重置到 cur_data_len,避免下方
             * (cur_data_len - gprs_rx_take_point) 下溢成大正数致 my_memmem 越界读。 */
            if(gprs_rx_take_point > cur_data_len)
            {
                gprs_rx_take_point = cur_data_len;
            }

            // 只有"已收数据长度"超过"读指针"时才扫描,避免在没有新数据时重复匹配历史内容
            if(cur_data_len > gprs_rx_take_point)
            {
                // 期望反馈子串匹配(长度感知,兼容二进制数据中的0x00)
                pt = (char *)my_memmem( gprs_rx_buff + gprs_rx_take_point, cur_data_len - gprs_rx_take_point,
                                        feedback, feedback_len );
                if(pt)
                {
                    gprs_rx_take_point = (unsigned short)( (pt + feedback_len) - (char *)gprs_rx_buff );
                    xSemaphoreGive(g_gprs_rx_mutex);
                    return(GPRS_SEND_OK);
                }

                // 检测连接被对端断开的指示
                pt2 = (char *)my_memmem( gprs_rx_buff + gprs_rx_take_point, cur_data_len - gprs_rx_take_point,
                                         "\r\n+MIPURC: \"disconn\",", 21 );
                if(pt2)
                {
                    disconn_id = atoi(pt2 + 21); // 解析断开链路的 id
                    pt = (char *)my_memmem( pt2 + 21, (gprs_rx_buff + cur_data_len) - (unsigned char *)(pt2 + 21),
                                            "\r\n", 2 );

                    if(client_id < 0)
                    {
                        /* 通用命令(client_id<0): disconn 不应导致通用命令失败。
                         * 跳过本条 URC 继续等通用响应(如 +CEREG:)。
                         * 正常情况下通用命令期间 disconn 已走异步路径不进 rx_buff,
                         * 此处为防御性兜底。 */
                        if(pt){ gprs_rx_take_point = (unsigned short)( (pt + 2) - (char *)gprs_rx_buff ); }
                        else{ gprs_rx_take_point = cur_data_len; }
                    }
                    else if(disconn_id != client_id)
                    {
                        /* 不是当前等待的链路:跳过本条 URC 继续等自己的响应。
                         * 同时补记该链路断开(隐患2兜底):当前因 AT 锁串行化,进入
                         * gprs_rx_buff 的 disconn 必属当前链路,本分支不可达;一旦未来
                         * 放宽串行化,此处保证他链路断开不被吞掉,接收路径能正确感知。 */
                        if(pt){ gprs_rx_take_point = (unsigned short)( (pt + 2) - (char *)gprs_rx_buff ); }
                        else{ gprs_rx_take_point = cur_data_len; }
                        if(disconn_id >= 0 && disconn_id < 3)
                        {
                            sg_gprs_status_t.disconn_pending[disconn_id] = 1;
                            sg_gprs_status_t.network[disconn_id] = 0;
                        }
                    }
                    else
                    {
                        /* 当前等待的链路被断开:推进读指针并返回 DISCONN。
                         * 同时置 disconn_pending 并清 network,使接收路径(recv_data_*)
                         * 与状态查询(check_data_disconn)感知一致——否则发送路径已感知
                         * 断开但 disconn URC 被此处消费,接收路径的 disconn_pending 永不置位。 */
                        if(pt){ gprs_rx_take_point = (unsigned short)( (pt + 2) - (char *)gprs_rx_buff ); }
                        else{ gprs_rx_take_point = cur_data_len; }
                        if(disconn_id >= 0 && disconn_id < 3)
                        {
                            sg_gprs_status_t.disconn_pending[disconn_id] = 1;
                            sg_gprs_status_t.network[disconn_id] = 0;
                        }
                        xSemaphoreGive(g_gprs_rx_mutex);
                        return(GPRS_SEND_DISCONN);
                    }
                }
            }
        }
        xSemaphoreGive(g_gprs_rx_mutex);

        // 是否还要等待?
        if(waittime <= 0){ return(GPRS_SEND_TIMEOUT); }

        GPRS_DELAY_MS(5); waittime -= 5;
    }
}
///////////////////

static int gprs_wait_mipopen_urc(int client_id, int waittime, uint16_t search_start)
{
    unsigned short cur_data_len = 0;
    unsigned short scan_pos = 0;
    char *pt = NULL;
    char *pt2 = NULL;
    char *line_end = NULL;
    int urc_id = -1;
    int urc_result = -1;
    int matched = 0;
    int disconn_id = 0;

    while(1)
    {
        xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
        cur_data_len = (gprs_rx_status & 0x7fff);
        if(gprs_rx_take_point > cur_data_len)
        {
            gprs_rx_take_point = cur_data_len;
        }
        scan_pos = search_start;
        if(scan_pos > cur_data_len)
        {
            scan_pos = gprs_rx_take_point;
        }

        if(cur_data_len > scan_pos)
        {
            pt = (char *)my_memmem(gprs_rx_buff + scan_pos,
                                   cur_data_len - scan_pos,
                                   "+MIPOPEN:", 9);
            if(pt)
            {
                char *parse = pt + 9;
                while(parse < (char *)gprs_rx_buff + cur_data_len && ((*parse == ' ') || (*parse == '\t'))){ parse++; }
                urc_id = atoi(parse);
                while(parse < (char *)gprs_rx_buff + cur_data_len && ((*parse >= '0') && (*parse <= '9'))){ parse++; }
                while(parse < (char *)gprs_rx_buff + cur_data_len && ((*parse == ' ') || (*parse == '\t'))){ parse++; }
                if(parse < (char *)gprs_rx_buff + cur_data_len && (*parse == ','))
                {
                    parse++;
                    while(parse < (char *)gprs_rx_buff + cur_data_len && ((*parse == ' ') || (*parse == '\t'))){ parse++; }
                    urc_result = atoi(parse);
                    matched = 2;
                }
                else
                {
                    matched = 0;
                }

                if(matched >= 2 && urc_id == client_id)
                {
                    line_end = (char *)my_memmem(pt,
                        (size_t)((gprs_rx_buff + cur_data_len) - (unsigned char *)pt),
                        "\r\n", 2);
                    if(line_end)
                    {
                        gprs_rx_take_point = (unsigned short)((line_end + 2) - (char *)gprs_rx_buff);
                    }
                    else
                    {
                        gprs_rx_take_point = cur_data_len;
                    }
                    xSemaphoreGive(g_gprs_rx_mutex);
                    return (urc_result == 0) ? GPRS_SEND_OK : GPRS_SEND_ERROR;
                }

                line_end = (char *)my_memmem(pt,
                    (size_t)((gprs_rx_buff + cur_data_len) - (unsigned char *)pt),
                    "\r\n", 2);
                if(line_end)
                {
                    gprs_rx_take_point = (unsigned short)((line_end + 2) - (char *)gprs_rx_buff);
                }
            }

            pt = (char *)my_memmem(gprs_rx_buff + scan_pos,
                                   cur_data_len - scan_pos,
                                   "+CME ERROR: 552", 15);
            if(pt)
            {
                line_end = (char *)my_memmem(pt,
                    (size_t)((gprs_rx_buff + cur_data_len) - (unsigned char *)pt),
                    "\r\n", 2);
                if(line_end)
                {
                    gprs_rx_take_point = (unsigned short)((line_end + 2) - (char *)gprs_rx_buff);
                }
                else
                {
                    gprs_rx_take_point = cur_data_len;
                }
                xSemaphoreGive(g_gprs_rx_mutex);
                return GPRS_SEND_OK;
            }

            pt2 = (char *)my_memmem(gprs_rx_buff + scan_pos,
                                      cur_data_len - scan_pos,
                                      "\r\n+MIPURC: \"disconn\",", 21);
            if(pt2)
            {
                disconn_id = atoi(pt2 + 21);
                pt = (char *)my_memmem(pt2 + 21,
                    (size_t)((gprs_rx_buff + cur_data_len) - (unsigned char *)(pt2 + 21)),
                    "\r\n", 2);
                if(disconn_id == client_id)
                {
                    if(pt){ gprs_rx_take_point = (unsigned short)((pt + 2) - (char *)gprs_rx_buff); }
                    else{ gprs_rx_take_point = cur_data_len; }
                    if(disconn_id >= 0 && disconn_id < 3)
                    {
                        sg_gprs_status_t.disconn_pending[disconn_id] = 1;
                        sg_gprs_status_t.network[disconn_id] = 0;
                    }
                    xSemaphoreGive(g_gprs_rx_mutex);
                    return GPRS_SEND_DISCONN;
                }
                if(pt){ gprs_rx_take_point = (unsigned short)((pt + 2) - (char *)gprs_rx_buff); }
                else{ gprs_rx_take_point = cur_data_len; }
                if(disconn_id >= 0 && disconn_id < 3)
                {
                    sg_gprs_status_t.disconn_pending[disconn_id] = 1;
                    sg_gprs_status_t.network[disconn_id] = 0;
                }
            }
        }
        xSemaphoreGive(g_gprs_rx_mutex);

        if(waittime <= 0){ return(GPRS_SEND_TIMEOUT); }

        GPRS_DELAY_MS(5);
        waittime -= 5;
    }
}

/*
*********************************************************************************************************
*    函 数 名: gprs_network_connect_function
*    功能说明: 连接服务器
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int gprs_network_connect_function(const char *host, unsigned short port, GPRS_LINK_E client_id)
{
    unsigned char buff[128] = {0};
    int res = 0;
    int cmd_len = 0;
    uint16_t search_start = 0;

    /* 始终下发 AT+MIPOPEN,不在本地 network 标志为 1 时跳过。
     * OTA 第一步 MIPCLOSE 后若本地状态未及时清零,旧逻辑会直接 MIPSEND 而模块 socket 已关(CME 551)。
     * 已连接时模块返回 +CME ERROR:552,由 gprs_wait_mipopen_urc 视为成功。 */

    xSemaphoreTake(g_gprs_at_mutex, portMAX_DELAY);

    xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
    search_start = (gprs_rx_status & 0x7fff);
    gprs_rx_take_point = search_start;
    sg_gprs_status_t.cmdon[client_id] = 1;
    sg_gprs_status_t.at_generic_cmd = 0;
    if((int)client_id >= 0 && (int)client_id < 3)
    {
        sg_gprs_status_t.disconn_pending[client_id] = 0;
    }
    xSemaphoreGive(g_gprs_rx_mutex);

    cmd_len = sprintf((char*)buff, "AT+MIPOPEN=%d,\"TCP\",\"%s\",%d,100,0\r\n",
                      (int)client_id, host, port);
    GPRS_STR_SEND((uint8_t *)buff, (uint16_t)cmd_len);

    /* 连接结果以 +MIPOPEN URC 为准。
     * ML307 在重复打开已连接 socket 时可能直接返回 +CME ERROR: 552 而不再返回 OK,
     * 因此不能先死等 OK,否则会把“已连接”误判为超时并触发上层重复连接。 */
    res = gprs_wait_mipopen_urc((int)client_id, 8000, search_start);

    xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
    sg_gprs_status_t.cmdon[client_id] = 0;
    if(res == GPRS_SEND_OK)
    {
        gprs_compact_at_rx_buff();
    }
    xSemaphoreGive(g_gprs_rx_mutex);

    xSemaphoreGive(g_gprs_at_mutex);

    if(res == GPRS_SEND_OK)
    {
        if((int)client_id >= 0 && (int)client_id < 3)
        {
            xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
            sg_gprs_status_t.disconn_pending[client_id] = 0;
            sg_gprs_status_t.network[client_id] = 1;
            gprs_reset_link_rx_stream(client_id);
            xSemaphoreGive(g_gprs_rx_mutex);
        }
    }

    return(res);
}
///////////////////

/*
*********************************************************************************************************
*    函 数 名: gprs_send_cmd
*    功能说明: 发送单一 AT 指令
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int gprs_send_cmd
(
    const uint8_t *AT_cmd,
    int AT_cmd_len,
    const struct GPRS_FEEDBACK *feedback_array,
    unsigned int feedback_count,
    int waittime,
    int client_id,
    uint8_t *resp_out,
    int resp_cap
)
{
    int res = 0;
    unsigned int ii;
    ////

    /* 获取 AT 通道锁: 串行化 AT 指令往返,防止多条链路并发握手响应错配。
     * 禁止在持锁时递归调用取本锁的函数。 */
    xSemaphoreTake(g_gprs_at_mutex, portMAX_DELAY);

    // 发送指令
    if(AT_cmd && (AT_cmd_len > 0))
    {
        /* 跳过 gprs_rx_buff 中已有数据,保留其完整性(可能有其他链路未取的数据);
         * gprs_wait_feedback 仅扫描新到达的应答,不销毁历史数据。
         * 注:不再 xStreamBufferReset,否则会丢弃 DMA 已投递但未被 gprs_rx_task
         * 取走的 rtcp URC 数据,造成其他链路用户数据丢失。 */
        xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
        gprs_rx_take_point = (gprs_rx_status & 0x7fff);
        if(client_id >= 0)
        {
            /* 链路型命令(connect/disconnect/send): 仅置目标链路 cmdon,
             * 该链路 disconn URC 进 gprs_rx_buff 供 wait_feedback 同步检测;
             * 其他链路异步 disconn 由 disconn_pending 路由,不污染本次匹配。 */
            sg_gprs_status_t.cmdon[client_id] = 1;
            sg_gprs_status_t.at_generic_cmd = 0;
        }
        else
        {
            /* 通用型命令(status_check/PDP/CEREG 等): 全开 cmdon 以接收应答,
             * 但 at_generic_cmd=1 使 disconn URC 一律走异步 disconn_pending 路径,
             * 不进 gprs_rx_buff,避免业务链路断开被误当作通用命令失败(DISCONN)。 */
            sg_gprs_status_t.cmdon[0] = 1;
            sg_gprs_status_t.cmdon[1] = 1;
            sg_gprs_status_t.cmdon[2] = 1;
            sg_gprs_status_t.at_generic_cmd = 1;
        }
        xSemaphoreGive(g_gprs_rx_mutex);

        //printf("\nGPRS_STR_SEND:\n%s\n", (const char *)AT_cmd);
        GPRS_STR_SEND( (uint8_t *)AT_cmd, (uint16_t)AT_cmd_len);
    }

    // 等待反馈
    if(!feedback_array || !feedback_count)
    {
        xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
        gprs_snapshot_resp(resp_out, resp_cap);
        if(client_id >= 0) { sg_gprs_status_t.cmdon[client_id] = 0; }
        else { sg_gprs_status_t.cmdon[0] = 0; sg_gprs_status_t.cmdon[1] = 0; sg_gprs_status_t.cmdon[2] = 0; }
        sg_gprs_status_t.at_generic_cmd = 0;
        xSemaphoreGive(g_gprs_rx_mutex);
        xSemaphoreGive(g_gprs_at_mutex);
        return(GPRS_SEND_OK);
    }

    for(ii=0; ii<feedback_count; ii++)
    {
        if( !(feedback_array[ii].feedback) || !(feedback_array[ii].feedback_len) ){ continue; }

        res = gprs_wait_feedback(feedback_array[ii].feedback, feedback_array[ii].feedback_len, waittime, client_id);
        if(res != GPRS_SEND_OK)
        {
            xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
            gprs_snapshot_resp(resp_out, resp_cap);
            if(client_id >= 0) { sg_gprs_status_t.cmdon[client_id] = 0; }
            else { sg_gprs_status_t.cmdon[0] = 0; sg_gprs_status_t.cmdon[1] = 0; sg_gprs_status_t.cmdon[2] = 0; }
            sg_gprs_status_t.at_generic_cmd = 0;
            xSemaphoreGive(g_gprs_rx_mutex);
            xSemaphoreGive(g_gprs_at_mutex);
            return(res);
        }
    } //for()

    xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
    gprs_snapshot_resp(resp_out, resp_cap);
    if(client_id >= 0) { sg_gprs_status_t.cmdon[client_id] = 0; }
    else { sg_gprs_status_t.cmdon[0] = 0; sg_gprs_status_t.cmdon[1] = 0; sg_gprs_status_t.cmdon[2] = 0; }
    sg_gprs_status_t.at_generic_cmd = 0;
    gprs_compact_at_rx_buff();
    xSemaphoreGive(g_gprs_rx_mutex);
    xSemaphoreGive(g_gprs_at_mutex);
    return(GPRS_SEND_OK);
}
///////////////

/*
*********************************************************************************************************
*    函 数 名: gprs_network_disconnect_function
*    功能说明: 断开当前连接
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void gprs_network_disconnect_function(GPRS_LINK_E client_id)
{
    uint8_t buff[128] = {0};
    char feedback_buf[32];
    int res = 0;

    struct GPRS_FEEDBACK feedback_array[2]=
    {
        {(unsigned char *)"\r\nOK\r\n", 6},
        {(unsigned char *)feedback_buf, 0}
    };
    ////
    
    sprintf(feedback_buf, "+MIPCLOSE: %d\r\n", (int)client_id);
    feedback_array[1].feedback_len = strlen(feedback_buf);
    
    sprintf((char*)buff, "AT+MIPCLOSE=%d\r\n", (int)client_id);
    res = gprs_send_cmd(buff, strlen((char*)buff), feedback_array, 2, 1000, (int)client_id, NULL, 0);
    /* MIPCLOSE 已下发后本地一律视为断开;超时多为 +MIPCLOSE URC 未及时匹配,模块侧 socket 通常已关 */
    if((int)client_id >= 0 && (int)client_id < 3
        && (res == GPRS_SEND_OK || res == GPRS_SEND_DISCONN || res == GPRS_SEND_TIMEOUT))
    {
        xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
        sg_gprs_status_t.network[client_id] = 0;
        sg_gprs_status_t.disconn_pending[client_id] = 0;
        gprs_reset_link_rx_stream(client_id);
        xSemaphoreGive(g_gprs_rx_mutex);
    }
}
///////////////////

// 重置指定链路的独立接收流缓冲(OTA/FILE)。DATA 走 com 队列,无独立缓冲,跳过。
// 须在持 g_gprs_rx_mutex 时调用。用于 connect/disconnect 时清除跨会话陈旧数据。
static void gprs_reset_link_rx_stream(GPRS_LINK_E client_id)
{
    if(client_id == GPRS_LINK_OTA)
    {
        gprs_ota_rx_status = 0;
        gprs_ota_rx_take_point = 0;
        gprs_ota_rx_buff[0] = 0;
    }
    else if(client_id == GPRS_LINK_FILE)
    {
        gprs_file_rx_status = 0;
        gprs_file_rx_take_point = 0;
        gprs_file_rx_buff[0] = 0;
    }
    /* GPRS_LINK_DATA: 无独立流缓冲,不处理 */
}

void gprs_reset_ota_rx_stream(void)
{
    xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
    gprs_reset_link_rx_stream(GPRS_LINK_OTA);
    xSemaphoreGive(g_gprs_rx_mutex);
}

// 从指定数据流中读取一段完整的数据,供 gprs_recv_data_ota / _file 共用
static int gprs_recv_data_from_stream
(
    uint8_t  *rx_buff,
    uint16_t *rx_status,
    uint16_t *rx_take_point,
    uint8_t  *out_buf,
    int       out_cap,
    int      *out_size,
    GPRS_LINK_E client_id
)
{
    unsigned short cur_stream_size = 0;
    int avail = 0;
    int copy_len = 0;
    ////

    if(out_size){ (*out_size) = 0; }
    if(!out_buf || (out_cap <= 0)){ return(GPRS_SEND_OK); }

    xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);

    cur_stream_size = (*rx_status & 0x7fff);

    if((*rx_take_point) >= cur_stream_size)
    {
        /* 无新数据: 以 disconn_pending 判断断开(异步/同步 disconn 均会置位)。
         * 不用 network==0 单独判断,避免未 connect 时误报 DISCONN。
         * connect 竞态修复后,pending 不会被成功路径误清。 */
        if((int)client_id >= 0 && (int)client_id < 3
            && sg_gprs_status_t.disconn_pending[client_id])
        {
            sg_gprs_status_t.disconn_pending[client_id] = 0;
            xSemaphoreGive(g_gprs_rx_mutex);
            return(GPRS_SEND_DISCONN);
        }
        xSemaphoreGive(g_gprs_rx_mutex);
        return(GPRS_SEND_OK);
    }

    /* 缓冲区存的是纯 payload(ISR已剥离 URC 头),直接拷出 */
    avail = (int)(cur_stream_size - (*rx_take_point));
    copy_len = (avail > out_cap) ? out_cap : avail;
    memcpy(out_buf, rx_buff + (*rx_take_point), (size_t)copy_len);
    (*rx_take_point) += (uint16_t)copy_len;

    if(out_size){ (*out_size) = copy_len; }

    xSemaphoreGive(g_gprs_rx_mutex);
    return(GPRS_SEND_OK);
}

/*
*********************************************************************************************************
*    函 数 名: gprs_recv_data_ota
*    功能说明: 读取OTA升级接收到的数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int gprs_recv_data_ota(uint8_t *out_buf, int out_cap, int *out_size)
{
    return gprs_recv_data_from_stream(  gprs_ota_rx_buff, &gprs_ota_rx_status,
                                        &gprs_ota_rx_take_point, out_buf, out_cap, out_size,
                                        GPRS_LINK_OTA);
}

/*
*********************************************************************************************************
*    函 数 名: gprs_recv_data_file
*    功能说明: 读取文件上传接收到的数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int gprs_recv_data_file(uint8_t *out_buf, int out_cap, int *out_size)
{
    return gprs_recv_data_from_stream(  gprs_file_rx_buff, &gprs_file_rx_status,
                                        &gprs_file_rx_take_point, out_buf, out_cap, out_size,
                                        GPRS_LINK_FILE);
}

// 测试打印
#if 0
void trace_gprs_recv_buff(const unsigned char *send_buff, int send_size)
{
    int ii;
    ////

    for(ii=0; ii<send_size; ii++)
    {
        #if 1
            if
            (
                (send_buff[ii] == '\r')
             || (send_buff[ii] == '\n')
             || ( (send_buff[ii] >= 0x20) && (send_buff[ii] <= 0x7E) )
            )
            {
                printf("%c", send_buff[ii]);
            }
            else{ printf("0x%02X,", send_buff[ii]); }
        #else
            printf("0x%02X ", send_buff[ii]);
        #endif
    } // for(ii)
}
#endif
/////////////////////


