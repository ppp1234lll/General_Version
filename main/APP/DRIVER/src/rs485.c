
#include "./Driver/inc/rs485.h"
#include "bsp.h"
#include "appconfig.h"

/*
    5、485通信：串口3 ,波特率115200    
            485_TX : PD8
            485_RX : PD9
            485_RE : PD10    
*/

#if (configUSE_RS485 == 1)

#define RS485_RE_GPIO_CLK           RCU_GPIOD
#define RS485_RE_GPIO_PORT          GPIOD
#define RS485_RE_PIN                GPIO_PIN_10

uint8_t *g_rs485_tx_buff = NULL;
uint8_t *g_rs485_rx_buff = NULL;
uint8_t g_rs485_rx_len = 0;

#define RS485_RE_SET()    gpio_bit_set(GPIOD, GPIO_PIN_10)   // RE 引脚置高
#define RS485_RE_RESET()  gpio_bit_reset(GPIOD, GPIO_PIN_10) // RE 引脚置低

/*
*********************************************************************************************************
*    函 数 名: RS485_Init
*    功能说明: 485初始化函数
*    形    参: baudrate 485波特率
*    返 回 值: 无
*********************************************************************************************************
*/
void RS485_Init(uint32_t baudrate)
{
    RS485_GPIO_Init();
    bsp_InitUsart2(baudrate);
}

/*
*********************************************************************************************************
*    函 数 名: RS485_GPIO_Init
*    功能说明: 初始化RE引脚GPIO
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void RS485_GPIO_Init(void)
{
    rcu_periph_clock_enable(RS485_RE_GPIO_CLK);

    gpio_mode_set(RS485_RE_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, RS485_RE_PIN);
    gpio_output_options_set(RS485_RE_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_RE_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: rs485_send_str
*    功能说明: rs485发送1个字节。
*    形    参: 
*    返 回 值: 字符串指针
*    @len        : 发送数据长度
*    返 回 值: 无
*********************************************************************************************************
*/
void rs485_send_str(uint8_t *data, uint16_t len)
{
    RS485_RE_SET();
    usart2_send_str(data, len);
    RS485_RE_RESET();
}

/*
*********************************************************************************************************
*    函 数 名: rs485_test
*    功能说明: 485测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void rs485_test(void)
{ 
    uint8_t *rs485_test = NULL;
    while(1)
    {
//        rs485_send_str((uint8_t*)"rs485_test\n",11);
        rs485_test = usart2_rx_get_frame();
        if(rs485_test != NULL)
            rs485_send_str(rs485_test, strlen(rs485_test));
        delay_ms(1000);        
    }
}
#endif


