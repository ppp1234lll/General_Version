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
*********************************************************************************************************
*/
#include "./Driver/inc/GPRS.h"
#include "main.h"
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

#define GPRS_NRST_H gpio_bit_set(GPRS_NRST_GPIO,GPRS_NRST_PIN)
#define GPRS_NRST_L gpio_bit_reset(GPRS_NRST_GPIO,GPRS_NRST_PIN)

#define GPRS_PWRK_H gpio_bit_set(GPRS_PWRK_GPIO,GPRS_PWRK_PIN)
#define GPRS_PWRK_L gpio_bit_reset(GPRS_PWRK_GPIO,GPRS_PWRK_PIN)

#define GPRS_CTRL_H gpio_bit_set(GPRS_CTRL_GPIO,GPRS_CTRL_PIN)
#define GPRS_CTRL_L gpio_bit_reset(GPRS_CTRL_GPIO,GPRS_CTRL_PIN)

#define GPRS_Sel_H gpio_bit_set(GPRS_Sel_GPIO,GPRS_Sel_PIN)
#define GPRS_Sel_L gpio_bit_reset(GPRS_Sel_GPIO,GPRS_Sel_PIN)

/* 串口初始化 */
#define GPRS_BAUDRATE               (115200)
#define GPRS_UART_INIT(baudrate)    bsp_InitUsart1(baudrate)
#define GPRS_STR_SEND(data,len)     usart1_send_str(data,len)
////

static int gprs_wait_feedback(const unsigned char *feedback, int feedback_len, int waittime);
////

// GPRS接收数据流
uint16_t gprs_rx_status = 0;  
uint8_t  gprs_rx_buff[GSM_RX_BUFF_SIZE];
uint16_t gprs_rx_take_point = 0;  

/* 数据 */
struct gprs_status_t sg_gprs_status_t = {0};

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
    
    GPRS_CTRL_H; // 默认打开电源
    GPRS_NRST_L;
    GPRS_PWRK_L;
    GPRS_Sel_H;
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
*    函 数 名: gprs_check_cmd_function
*    功能说明: 验证响应数据
*    形    参: 
*    返 回 值: 期望应答的数据
*    返 回 值:  0-没有的到期望数据 other-得到了期望数据
*********************************************************************************************************
*/
uint8_t* gprs_check_cmd_function(uint8_t *str) 
{
    char *strx=0;

    if(gprs_rx_status&0x8000) 
    {
        gprs_rx_status &= 0x7fff;
        strx = my_strstr((const char*)gprs_rx_buff,(const char*)str);
    }
    return ((uint8_t*)strx);
}

/*
*********************************************************************************************************
*    函 数 名: gprs_send_cmd_function
*    功能说明: 数据发送函数
*    形    参: 
*    返 回 值: 命令
*    @ack        : 响应
*    @waittime    : 命令等待时间
*    返 回 值:  无
*********************************************************************************************************
*/
uint8_t gprs_send_cmd_function(uint8_t *cmd, uint8_t *ack, uint16_t waittime)
{
    uint8_t res = 0;
    
    sg_gprs_status_t.cmdon = 1;
    
    GPRS_STR_SEND(cmd,strlen((char*)cmd));
    
    if(ack && waittime) {
        while(--waittime) {
            if(gprs_check_cmd_function(ack) != NULL) {
                res = 0;
                break;
            }
            GPRS_DELAY_MS(10);
        }
        if(waittime == 0) {
            res = 2;
        }
    }
    
    return res;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_send_cmd_over_function
*    功能说明: 退出命令发送函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void gprs_send_cmd_over_function(void)
{
    gprs_rx_status = 0;
    sg_gprs_status_t.cmdon = 0;
    memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
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
            if(gprs_send_cmd_function((uint8_t*)"AT\r\n",(uint8_t*)"OK",25) == 0) 
            {
                gprs_send_cmd_function((uint8_t*)"ATE0\r\n",0,0); // 关闭回显
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
                    sg_gprs_status_t.step = GPRS_INIT;
                }
            }
            break;
        case GPRS_SIM:
            /* SIM卡状态检测 */
            memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
            if(gprs_send_cmd_function((uint8_t*)"AT+CPIN?\r\n",(uint8_t*)"READY",100) == 0) 
            {
                gprs_send_cmd_function((uint8_t*)"AT+MCFG=\"simhot\",0\"\r\n",0,0); // 关闭SIM检测
                GPRS_DELAY_MS(20);
                for(index=0; index<3; index++) 
                {
                    res = gprs_send_cmd_function((uint8_t*)"AT+ICCID\r\n",(uint8_t*)"+ICCID:",100);
                    if(res == 0) 
                    {
                        p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+ICCID: ");
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
                GPRS_DELAY_MS(20);
                sg_gprs_status_t.status.sim = 0;
                repeat++;
                if(repeat > 20) 
                {
                    sg_gprs_status_t.step = GPRS_INIT;
                    
                    if((gpio_input_bit_get(GPRS_Sel_GPIO, GPRS_Sel_PIN)) == 1)
                        GPRS_Sel_L;    
                    else
                        GPRS_Sel_H;
                }
            }
            break;
            
        case GPRS_CFUN:
            /* 协议栈状态 */
            memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
            if(gprs_send_cmd_function((uint8_t*)"AT+CFUN?\r\n",(uint8_t*)"+CFUN: 1",25) == 0) {
                sg_gprs_status_t.step = GPRS_CEREG;
                repeat = 0;
            } else {
                GPRS_DELAY_MS(20);
                repeat++;
                if(repeat > 10) {
                    sg_gprs_status_t.step = GPRS_INIT;
                }
            }
            break;

        case GPRS_CEREG:
            /* 网络注册状态 */
            memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));                                                        
            if(gprs_send_cmd_function((uint8_t*)"AT+CEREG?\r\n",(uint8_t*)"+CEREG:",25) == 0) 
            {
                p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+CEREG:");
                temp2 = 0;
                temp1 = 0;
                res = sscanf((char*)p1,"+CEREG: %d,%d",&temp1,&temp2);
                if(temp1 == 0 && res == 2) 
                {
                    gprs_send_cmd_function((uint8_t*)"AT+CEREG=2\r\n",0,0); //启用带有位置信息的网络注册 URC
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
                        printf("SIM Registration denied!!!\n");
                    sg_gprs_status_t.status.net = 0;
                    GPRS_DELAY_MS(260);
                    repeat++;
                    if(repeat > 50) 
                    {
                        sg_gprs_status_t.step = GPRS_INIT;
                    }
                }
            } 
            else {
                sg_gprs_status_t.status.net = 0;
                GPRS_DELAY_MS(260);
                repeat++;
                if(repeat > 50) {
                    sg_gprs_status_t.step = GPRS_INIT;
                }
            }
            break;
            
        case GPRS_CCLK:
            /* 同步时间 */
            memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));                                                        
            if(gprs_send_cmd_function((uint8_t*)"AT+CCLK?\r\n",(uint8_t*)"+CCLK: ",25) == 0) {
                p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+CCLK: ");
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
                        sg_gprs_status_t.step = GPRS_INIT;
                    }
                }
            } else {
                sg_gprs_status_t.status.net = 0;
                GPRS_DELAY_MS(200);
                repeat++;
                if(repeat > 20) {
                    sg_gprs_status_t.step = GPRS_INIT;
                }
            }
            break;
        case GPRS_MIPCCLK:  // 首先判断是否激活，未激活则手动激活
          if(gprs_send_cmd_function((uint8_t*)"AT+MIPCALL?\r\n",(uint8_t*)"+MIPCALL:",100) == 0) 
            {
                p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+MIPCALL:");
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
                        /* 设置移动APN   AT+CGDCONT=1,"IPV4V6","cmnet" //配置PDP上下文*/
                        gprs_send_cmd_function((uint8_t*)"AT+CGDCONT=1,\"IP\",\"CMIOT\"\r\n",0,0);
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
                    sg_gprs_status_t.step = GPRS_INIT;
                }
            }
            break;
        case GPRS_PDP:
            /* 激活 PDP 场景 */
            memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
            if(gprs_send_cmd_function((uint8_t*)"AT+MIPCALL=1,1\r\n",(uint8_t*)"OK",25) == 0) 
            {
                gprs_send_cmd_function(0,(uint8_t*)"+MIPCALL:",200);
                p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+MIPCALL: ");
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
                    sg_gprs_status_t.step = GPRS_INIT;
                }
            }
            break;
        case GPRS_CGPADDR:
            /* 获取IP地址 */
            memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
            if(gprs_send_cmd_function((uint8_t*)"AT+CGPADDR=1\r\n",(uint8_t*)"+CGPADDR",50) == 0) // 读取场景ID为1 的IP地址
            {
                p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+CGPADDR: ");
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
                    sg_gprs_status_t.step = GPRS_INIT;
          sg_gprs_status_t.status.net = 0;
                }
            }
            break;
        case GPRS_CGMR:
            /* 查询模块版本信息 */
            memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
            if(gprs_send_cmd_function((uint8_t*)"AT+CGMR\r\n",(uint8_t*)"OK",100) == 0) // 读取场景ID为1 的IP地址
            {
                p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"OK");
                if(p1 != NULL) 
                {
                    addr_len = p1 - gprs_rx_buff - 6;
                    memset(sg_gprs_status_t.model,0,sizeof(sg_gprs_status_t.model));
                    memcpy(sg_gprs_status_t.model,gprs_rx_buff+2,addr_len);    
                }
            }
            sg_gprs_status_t.step = GPRS_IMEI;
            break;        
        case GPRS_IMEI:
            /* 查询模块IMEI */            
            memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
            if(gprs_send_cmd_function((uint8_t*)"AT+CGSN=1\r\n",(uint8_t*)"+CGSN: ",50) == 0) 
            {
                p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+CGSN: ");
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
            memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
            if(gprs_send_cmd_function((uint8_t*)"AT+CSQ\r\n",(uint8_t*)"+CSQ: ",25) == 0) 
            {
                p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+CSQ: ");
                temp2 = 0;
                temp1 = 0;
                res = sscanf((char*)p1,"+CSQ: %d,%d",&temp1,&temp2);
                if(temp1 != 99 && res == 2) {
                    sg_gprs_status_t.status.csq = temp1+1;
                    sg_gprs_status_t.step = GPRS_SUCCESS;
                    repeat = 0;
                } 
                else 
                {
                    GPRS_DELAY_MS(200);
                    repeat++;
                    if(repeat > 10) {
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
            return 0; // 初始化完成
            //break;
    }
    
    /* 正在初始化 */
    return 1;
    
}

/*
*********************************************************************************************************
*    函 数 名: gprs_network_connection_restart_function
*    功能说明: 网络连接重启函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void gprs_network_connection_restart_function(void)
{
    gprs_network_disconnect_function(0);
    sg_gprs_status_t.network = 0;
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
    gprs_network_disconnect_function(0);
    sg_gprs_status_t.mount = 0;
}


/*
*********************************************************************************************************
*    函 数 名: gprs_network_data_send_function
*    功能说明: 网络数据发送函数
*    形    参: @data        : 数据指针
*    返 回 值: 数据长度
*    ? ? ?: *    
*********************************************************************************************************
*/
uint8_t gprs_network_data_send_function(uint8_t *data, uint16_t len)
{
    uint8_t buff[32] = {0};
    uint8_t res      = 0;
//    sprintf((char*)buff,"AT+MIPSEND=%d,%d\r\n",1,len);
//    sprintf((char*)buff,"AT+QISEND=0\r\n"); // 发送可变长度
//    sprintf((char*)buff,"AT+QISEND=%d,%d\r\n",1,len);// 发送固定长度
//    res = gprs_send_cmd_function(buff,(uint8_t*)">",20);
//    gprs_send_cmd_over_function();

//    GPRS_STR_SEND(data,len);
////    usart3_send_char(0x1A); // 最后发送1A
//    GPRS_DELAY_MS(10);

    sprintf((char*)buff,"AT+MIPSEND=%d,%d\r\n",1,len);
    res = gprs_send_cmd_function(buff,(uint8_t*)">",20);
    gprs_send_cmd_over_function();
    if(res == 0) {
        GPRS_STR_SEND(data,len);
    } else {
        GPRS_STR_SEND(data,len);
    }
    
    GPRS_DELAY_MS(10);
    
    return res;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_network_disconnect_function
*    功能说明: 连接断开函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void gprs_network_disconnect_function(uint8_t data)
{
    uint8_t buff[32] = {0};
    
    sprintf((char*)buff,"AT+MIPCLOSE=%d\r\n",1);
    gprs_send_cmd_function(buff,0,0);
    GPRS_DELAY_MS(10);
    sg_gprs_status_t.network = 0;
    gprs_send_cmd_over_function();
    
}

/*
*********************************************************************************************************
*    函 数 名: gprs_network_connect_function
*    功能说明: 网络连接函数
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
int8_t gprs_network_connect_function(uint8_t *ip, uint8_t *port) 
{
    uint8_t buff[64] = {0};
    int8_t ret      = 0;
    uint32_t temp1 = 0;
    uint32_t temp2 = 0;
    uint8_t  res   = 0;
    uint8_t *p1 = NULL;
    
    /* 连接断开操作 */
    gprs_network_disconnect_function(0);
    /* 开始连接 */
    sprintf((char*)buff,"AT+MIPOPEN=%d,\"TCP\",\"%s\",%s,100,0\r\n",1,ip,port);
    if(gprs_send_cmd_function((uint8_t*)buff,(uint8_t*)"OK",100) == 0) 
    {
        gprs_send_cmd_function(0,(uint8_t*)"+MIPOPEN:",500);
        p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+MIPOPEN: ");
        temp2 = 0;
        temp1 = 0;
        res = sscanf((char*)p1,"+MIPOPEN: %d,%d",&temp1,&temp2);
        if(temp2 == 0 && res == 2)
        { 
            sg_gprs_status_t.network = 1;
        } 
        else 
        {
            ret = -1;
        }
    } 
    else 
    {
        ret = -1;
    }
    
    gprs_send_cmd_over_function();
    
    return ret;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_mult_network_connect_function
*    功能说明: 多链路网络连接函数
*    形    参: @ip            : ip地址
*    返 回 值: 端口
*    @mult        : 多链路
*    ? ? ?: 0-正常 other-异常
*    
*********************************************************************************************************
*/
uint8_t gprs_mult_network_connect_function(uint8_t *ip, uint8_t *port, uint8_t mult)
{
    uint16_t dport = mult+1102;
    uint8_t buff[64] = {0};
    int8_t ret      = 0;
    
    sprintf((char*)buff,"AT+MIPOPEN=%d,\"TCP\",\"%s\",%s,100,0,1,1,%d\r\n",mult,ip,port,dport);
    if(gprs_send_cmd_function((uint8_t*)buff,(uint8_t*)"OK",100) == 0) {
        ret = gprs_send_cmd_function(0,(uint8_t*)"CONNECT OK",500);
        if(ret == 0) {
            gprs_send_cmd_over_function();
        }
    } else {
        ret = -1;
    }
    
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
    uint32_t temp1;
    uint32_t temp2;
    uint8_t  res = 0;
    uint8_t  index = 0;
    uint8_t  *p1 = 0;
    
    for(index=0; index<3; index++) 
    {
        memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
        if(gprs_send_cmd_function((uint8_t*)"AT+CEREG?\r\n",(uint8_t*)"+CEREG:",50) == 0) 
        {
            p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+CEREG:");
            res = sscanf((char*)p1,"+CEREG: %d,%d",&temp1,&temp2);
            
            if(res == 2 && (temp2 == 1 || temp2 == 5)) 
            {
                gprs_send_cmd_over_function();
                return 0;
            }
        }
        GPRS_DELAY_MS(200);
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
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t gprs_get_tcp_status(void)
{
    return sg_gprs_status_t.network;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_csq_function
*    功能说明: 获取模块信号强度
*    形    参: 
*    返 回 值: 
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
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
void *gprs_get_ip_addr_function(void)
{
    return sg_gprs_status_t.status.ip;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_receive_data_function
*    功能说明: 获取通信数据或命令数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
#if 0
void gprs_get_receive_data_function(uint8_t *buff, uint16_t len)
{
    uint16_t index = 0; 
    int8_t   ret   = 0;

    if(len == 0 || buff == NULL) 
        return;
    
    /* 更新数据检测 */
    ret = update_gsm_recevie_data_function(buff,len);
    if(ret == 0) {
        return;
    }
        
    /* 检测当前模块模式 */
    if(sg_gprs_status_t.cmdon == 1) 
    {
        for(index=0; index < len; index++)
        {
            gprs_rx_buff[index] = buff[index];
            buff[index]         = 0;
        }
        gprs_rx_status = len | 0x8000;
    } 
    else 
    {
        /* 检测数据是否有接收数据 */
        com_stroage_cache_data(buff,len);
    }
}
#endif

void gprs_get_receive_data_function(uint8_t *buff, uint16_t len)
{
    int16_t cur_data_len = 0;
    char *pt = NULL;
    unsigned short gprs_data_len = 0;
    ////
    
    if( (len == 0) || (buff == NULL) ) { return; }

    /* 检测当前模块模式 */
    if
    (
        (sg_gprs_status_t.cmdon == 1) // 发送信令时
     || ( (update_get_mode_function() == UPDATE_MODE_GPRS) && (gsm_get_network_connect_status_function() == 1 ) ) // 无线更新时
    )
    {
        // 防止溢出
        if( !(gprs_rx_status & 0x8000) ){ cur_data_len = 0; }
        else{ cur_data_len = (gprs_rx_status & 0x7fff); }
        if( (cur_data_len + len) >= GSM_RX_BUFF_SIZE ){ return; } // 丢弃

        // 追加到数据流缓冲中
        memcpy( (gprs_rx_buff + cur_data_len), buff, len );
        cur_data_len += len;
        gprs_rx_status = (cur_data_len | 0x8000);
        gprs_rx_buff[cur_data_len] = 0;

        #if 0 // 测试完立即注释掉!
            printf("\n无线接收到 %d 字节\n", len);
            trace_gprs_recv_buff( buff,len );
        #endif
    } 
    else
    {
        /* 云平台命令数据 */
        #if 0 // 测试完立即注释掉!
            printf("\n无线接收到 %d 字节\n", len);
            trace_gprs_recv_buff( buff,len );
        #endif

        // 丢弃AT命令或回馈,只保留真实数据
        // \r\n+MIPURC: "rtcp",1,20,******
        if(strncmp((char *)buff, "\r\n+MIPURC: \"rtcp\",", 18)){ return; }

        pt = (char *)buff + 18;
        pt = strchr(pt, ','); // 第二个逗号
        if(!pt){ return; }
        pt++;

        gprs_data_len = atoi(pt); // 携带数据长度
        if(!gprs_data_len){ return; }

        pt = strchr(pt, ','); // 第三个逗号
        if(!pt){ return; }
        pt++; // 此时指向真实的数据

        //printf("\ngprs接收 %d 个字节\n", gprs_data_len);
        com_stroage_cache_data((uint8_t *)pt, gprs_data_len);
    }
}
////////////////////

/*
*********************************************************************************************************
*    函 数 名: gprs_get_infor_data_function
*    功能说明: 获取模块数据指针
*    形    参: 指针
*    返 回 值: 
*********************************************************************************************************
*/
void* gprs_get_infor_data_function(void)
{
    return &sg_gprs_status_t;
}

/*
*********************************************************************************************************
*    函 数 名: gprs_get_rec_buff_function
*    功能说明: 获取接收数据
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
uint8_t* gprs_get_rec_buff_function(uint16_t *len)
{
    *len = gprs_rx_status&0x7fff;
    return gprs_rx_buff;
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
////////////////////////////

// 返回值: enum GPRS_SEND_CODE_E
int gprs_send_data(const uint8_t *data, int len, int waittime)
{
    int res = 0;
    char AT_cmd[128];
    int AT_cmd_len = 0;
    ////

    if(!data || !len){ return(GPRS_SEND_OK); }

    // (1) 发送指令
    gprs_rx_status = 0;
    gprs_rx_take_point = 0;
    sg_gprs_status_t.cmdon = 1;

    sprintf(AT_cmd, "AT+MIPSEND=%d,%d\r\n", 1, len);
    AT_cmd_len = strlen(AT_cmd);
    //printf("\nGPRS_STR_SEND:\n%s\n", AT_cmd);
    GPRS_STR_SEND( (uint8_t *)AT_cmd, (uint16_t)AT_cmd_len);

    // 等待回馈"\r\n>\r\n"
    res = gprs_wait_feedback((unsigned char *)"\r\n>\r\n", 5, waittime);
    switch(res)
    {
        case GPRS_SEND_OK: break;
        default: sg_gprs_status_t.cmdon = 0; return(res);
    }//switch()

    // (2) 发送数据
    gprs_rx_status = 0;
    gprs_rx_take_point = 0;
    sg_gprs_status_t.cmdon = 1;

    //printf("\nGPRS_STR_SEND:\n%s\n", (char *)data);
    GPRS_STR_SEND( (uint8_t *)data, (uint16_t)len);

    // 等待回馈
    // "\r\n+MIPSEND: 1,396\r\n\r\nOK\r\n"
    sprintf(AT_cmd, "\r\n+MIPSEND: 1,%d\r\n", len); // 借用 AT_cmd
    res = gprs_wait_feedback((unsigned char *)AT_cmd, strlen(AT_cmd), waittime);
    if(res != GPRS_SEND_OK){ sg_gprs_status_t.cmdon = 0; return(res); }

    // 等待第二个回馈
    res = gprs_wait_feedback((unsigned char *)("\r\nOK\r\n"), 6, waittime);
    sg_gprs_status_t.cmdon = 0;

    return(res);
}
//////////////////

// 等待服务器反馈
static int gprs_wait_feedback(const unsigned char *feedback, int feedback_len, int waittime)
{
    unsigned short cur_data_len = (gprs_rx_status & 0x7fff);
    char *pt;
    ////

    if(!feedback || !feedback_len){ return(GPRS_SEND_OK); } // 空操作

    // 先判断是否匹配
    if(cur_data_len >= feedback_len)
    {
        if( !memcmp( (gprs_rx_buff + gprs_rx_take_point), feedback, feedback_len ) )
        {
            gprs_rx_take_point += feedback_len;
            return(GPRS_SEND_OK);
        }
        else if( !memcmp( (gprs_rx_buff + gprs_rx_take_point), "\r\n+MIPURC: \"disconn\",", 21) )
        {
            pt = strstr( (char *)(gprs_rx_buff + gprs_rx_take_point + 21), "\r\n" );
            if(pt){ gprs_rx_take_point = (pt + 2 - (char*)gprs_rx_buff); }
            return(GPRS_SEND_DISCONN);
        }
        else{ return(GPRS_SEND_ERROR); }
    }

    // 是否等待
    if(waittime <= 0){ return(GPRS_SEND_ERROR); }

    // 等待
    while(waittime > 0)
    {
        GPRS_DELAY_MS(5); waittime -= 5;
        cur_data_len = (gprs_rx_status & 0x7fff);

        // 是否有了回馈
        if(cur_data_len >= feedback_len)
        {
            if( !memcmp( (gprs_rx_buff + gprs_rx_take_point), feedback, feedback_len ) )
            {
                gprs_rx_take_point += feedback_len;
                return(GPRS_SEND_OK);
            }
            else if( !memcmp( (gprs_rx_buff + gprs_rx_take_point), "\r\n+MIPURC: \"disconn\",", 21) )
            {
                pt = strstr( (char *)(gprs_rx_buff + gprs_rx_take_point + 21), "\r\n" );
                if(pt){ gprs_rx_take_point = (pt + 2 - (char *)gprs_rx_buff); }
                return(GPRS_SEND_DISCONN);
            }
            else{ return(GPRS_SEND_ERROR); }
        }
    }//while()

    return(GPRS_SEND_TIMEOUT);
}
///////////////////

// 连接服务
// 返回值 enum GPRS_SEND_CODE_E
int gprs_network_connect_server(const char *host, unsigned short port)
{
    unsigned char buff[128] = {0};
    int res = 0;
    struct GPRS_FEEDBACK feedback_array[2]=
    {
        {(unsigned char *)"\r\nOK\r\n", 6},
        {(unsigned char *)("\r\n+MIPOPEN: 1,0\r\n"), 17}
    };
    ////

    // 发送连接命令
    sprintf((char*)buff, "AT+MIPOPEN=%d,\"TCP\",\"%s\",%d,100,0\r\n", 1, host, port);

    // "\r\nOK\r\n", "\r\n+MIPOPEN: 1,0\r\n"
    res = gprs_send_cmd((uint8_t*)buff, strlen((char *)buff), feedback_array, 2, 1000);

    return(res);
}
///////////////////

// 发送单一 AT 指令
// 返回值: enum GPRS_SEND_CODE_E
int gprs_send_cmd
(
    const uint8_t *AT_cmd,
    int AT_cmd_len,
    const struct GPRS_FEEDBACK *feedback_array,
    unsigned int feedback_count,
    int waittime
)
{
    int res = 0;
    unsigned int ii;
    ////

    // 发送指令
    if(AT_cmd && (AT_cmd_len > 0))
    {
        gprs_rx_status = 0;
        gprs_rx_take_point = 0;
        sg_gprs_status_t.cmdon = 1;

        //printf("\nGPRS_STR_SEND:\n%s\n", (const char *)AT_cmd);
        GPRS_STR_SEND( (uint8_t *)AT_cmd, (uint16_t)AT_cmd_len);
    }

    // 等待回馈
    if(!feedback_array || !feedback_count)
    {
        sg_gprs_status_t.cmdon = 0;
        return(GPRS_SEND_OK);
    }

    for(ii=0; ii<feedback_count; ii++)
    {
        if( !(feedback_array[ii].feedback) || !(feedback_array[ii].feedback_len) ){ continue; }

        res = gprs_wait_feedback(feedback_array[ii].feedback, feedback_array[ii].feedback_len, waittime);
        if(res != GPRS_SEND_OK){ sg_gprs_status_t.cmdon = 0; return(res); }
    } //for()

    sg_gprs_status_t.cmdon = 0;
    return(GPRS_SEND_OK);
}
///////////////

// 断开当前连接 
void gprs_disconnect(void)
{
    uint8_t buff[128] = {0};

    // "\r\nOK\r\n", "\r\n+MIPCLOSE: 1\r\n"
    struct GPRS_FEEDBACK feedback_array[2]=
    {
        {(unsigned char *)"\r\nOK\r\n", 6},
        {(unsigned char *)("\r\n+MIPCLOSE: 1\r\n"), 16}
    };
    ////
    
    sprintf((char*)buff, "AT+MIPCLOSE=%d\r\n", 1);
    gprs_send_cmd(buff, strlen((char*)buff), feedback_array, 2, 1000);
    sg_gprs_status_t.network = 0;
}
///////////////////

// 读取一段接收到的数据
// 返回值: enum GPRS_SEND_CODE_E
int gprs_recv_data(const unsigned char **recv_data, int *recv_data_size)
{
    unsigned short cur_stream_size = 0;
    char *pt = NULL, *pt2 = NULL;
    int section_size = 0;
    ////

    if(recv_data){ (*recv_data) = NULL; }
    if(recv_data_size){ (*recv_data_size) = 0; }

    // 流中的数据长度
    cur_stream_size = (gprs_rx_status & 0x7fff);

    if(gprs_rx_take_point >= cur_stream_size){ return(GPRS_SEND_OK); }

    // 扫描一段数据
    // \r\n+MIPURC: "rtcp",1,238,HTTP/1.1 200 OK ...
    pt = strstr( (char *)(gprs_rx_buff + gprs_rx_take_point), "+MIPURC: \"rtcp\",");
    if(!pt)
    {
        // 判断服务器是否断开了
        pt = strstr( (char *)(gprs_rx_buff + gprs_rx_take_point), "+MIPURC: \"disconn\",1,1");
        if(pt)
        {
            gprs_rx_take_point += ( (pt + 24) - (char *)(gprs_rx_buff + gprs_rx_take_point) );
            return(GPRS_SEND_DISCONN);
        }

        pt = strstr( (char *)(gprs_rx_buff + gprs_rx_take_point), "+CME ERROR: 550");
        if(pt)
        {
            gprs_rx_take_point += ( (pt + 17) - (char *)(gprs_rx_buff + gprs_rx_take_point) );
            return(GPRS_SEND_DISCONN);
        }

        // 暂无数据
        return(GPRS_SEND_OK);
    }
    ////

    // 提取数据给上层
    pt += 16;

    pt2 = strchr(pt, ',');
    if(!pt2)
    {
        gprs_rx_take_point += ( pt - (char *)(gprs_rx_buff + gprs_rx_take_point) );
        return(GPRS_SEND_OK);
    }

    pt = pt2 + 1; // 逗号后
    section_size = atoi(pt);

    pt2 = strchr(pt, ',');
    if(!pt2)
    {
        gprs_rx_take_point += ( pt - (char *)(gprs_rx_buff + gprs_rx_take_point) );
        return(GPRS_SEND_OK);
    }

    pt = pt2 + 1; // 逗号后
    if(recv_data){ (*recv_data) = (unsigned char *)pt; }
    if(recv_data_size){ (*recv_data_size) = section_size; }

    gprs_rx_take_point += ( (pt + section_size) - (char *)(gprs_rx_buff + gprs_rx_take_point) );
    return(GPRS_SEND_OK);
}
///////////////////

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


