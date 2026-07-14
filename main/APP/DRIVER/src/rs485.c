
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

// 485相关定义，根据实际使用修改为对应的串口
#define RS485_UARTx                 USART2
#define RS485_CONFIG                bsp_InitUsart2
#define RS485_SEND_BUFF             usart2_send_str
#define RS485_DMA_DISABLE           usart2_disable_dma
#define RS485_DMA_ENABLE            usart2_enable_dma
#define RS485_RECV_BUFF             usart2_rx_get_frame

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
    RS485_CONFIG(baudrate);
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
*    函 数 名: RS485_GPIO_Init
*    功能说明: 初始化RE引脚GPIO 
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void RS485_ReConfig(rs485_config_t *config)
{
    RS485_DMA_DISABLE();
    usart_receive_config(RS485_UARTx, USART_RECEIVE_DISABLE);
    usart_transmit_config(RS485_UARTx, USART_TRANSMIT_DISABLE);    
    
	// 配置数据位
	if (config->data_bits == 8)
		usart_word_length_set(RS485_UARTx, USART_WL_8BIT);
	else if (config->data_bits == 9) 
		usart_word_length_set(RS485_UARTx, USART_WL_9BIT);

	// 配置停止位
	if (config->stop_bits == 2.0f) 
		usart_stop_bit_set(RS485_UARTx, USART_STB_2BIT);
	else if (config->stop_bits == 1.0f) 
		usart_stop_bit_set(RS485_UARTx, USART_STB_1BIT);
	else if (config->stop_bits == 0.5f) 
		usart_stop_bit_set(RS485_UARTx, USART_STB_0_5BIT);
	else if (config->stop_bits == 1.5f) 
		usart_stop_bit_set(RS485_UARTx, USART_STB_1_5BIT);

	// 配置校验位
	if (config->parity == 0) 
		usart_parity_config(RS485_UARTx, USART_PM_NONE);
	else if (config->parity == 1) 
		usart_parity_config(RS485_UARTx, USART_PM_ODD);
	else if (config->parity == 2) 
		usart_parity_config(RS485_UARTx, USART_PM_EVEN);
        
    usart_baudrate_set(RS485_UARTx, config->baudrate);

    usart_receive_config(RS485_UARTx, USART_RECEIVE_ENABLE);
    usart_transmit_config(RS485_UARTx, USART_TRANSMIT_ENABLE); 
    RS485_DMA_ENABLE();
}
/*
*********************************************************************************************************
*    函 数 名: rs485_send_str
*    功能说明: rs485发送1个字节。
*    形    参:  
*    @buff        : 字符串指针
*    @len        : 发送数据长度
*    返 回 值: 无
*********************************************************************************************************
*/
void rs485_send_str(uint8_t *data, uint16_t len)
{
    RS485_RE_SET();
    RS485_SEND_BUFF(data, len);
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
        rs485_test = RS485_RECV_BUFF();
        if(rs485_test != NULL)
            RS485_SEND_BUFF(rs485_test, strlen((const char*)rs485_test));
        delay_ms(1000);        
    }
}
#endif


