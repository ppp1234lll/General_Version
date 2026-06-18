#include "bsp_debug.h"
#include "bsp.h"

/*
*********************************************************************************************************
*                                时钟，引脚，DMA，中断等宏定义
*********************************************************************************************************
*/
#define DEBUG_TX_GPIO_CLK               RCU_GPIOA
#define DEBUG_TX_GPIO_PORT              GPIOA
#define DEBUG_TX_PIN                    GPIO_PIN_9
#define DEBUG_TX_PIN_AF                 GPIO_AF_7

#define DEBUG_RX_GPIO_CLK               RCU_GPIOA
#define DEBUG_RX_GPIO_PORT              GPIOA
#define DEBUG_RX_PIN                    GPIO_PIN_10
#define DEBUG_RX_PIN_AF                 GPIO_AF_7

#define DEBUG_UART_CLK                  RCU_USART0
#define DEBUG_UARTx                     USART0


/*
*********************************************************************************************************
*    函 数 名: bsp_InitDebug
*    功能说明: 初始化调试串口硬件，并对全局变量赋初值.
*    形    参: baudrate: 波特率
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitDebug(uint32_t baudrate)
{    
    bsp_InitDebug_GPIO();
    bsp_InitDebug_Config(baudrate);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitDebug_GPIO
*    功能说明: 初始化调试串口硬件
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitDebug_GPIO(void)
{
    /* enable GPIO clock */
    rcu_periph_clock_enable(DEBUG_TX_GPIO_CLK);
    rcu_periph_clock_enable(DEBUG_RX_GPIO_CLK);

    /* configure the DEBUG TX pin and DEBUG RX pin */
    gpio_af_set(DEBUG_TX_GPIO_PORT, DEBUG_TX_PIN_AF, DEBUG_TX_PIN);
    gpio_af_set(DEBUG_RX_GPIO_PORT, DEBUG_RX_PIN_AF, DEBUG_RX_PIN);

    /* configure DEBUG TX as alternate function push-pull */
    gpio_mode_set(DEBUG_TX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, DEBUG_TX_PIN);
    gpio_output_options_set(DEBUG_TX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, DEBUG_TX_PIN);

    /* configure DEBUG RX as alternate function push-pull */
    gpio_mode_set(DEBUG_RX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, DEBUG_RX_PIN);
    gpio_output_options_set(DEBUG_RX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, DEBUG_RX_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitDebug_Config
*    功能说明: 初始化调试串口硬件
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitDebug_Config(uint32_t baudrate)
{
    /* enable USART clock */
    rcu_periph_clock_enable(DEBUG_UART_CLK);
    
    /* DEBUG configure */
    usart_deinit(DEBUG_UARTx);
    usart_word_length_set(DEBUG_UARTx, USART_WL_8BIT);
    usart_stop_bit_set(DEBUG_UARTx, USART_STB_1BIT);
    usart_parity_config(DEBUG_UARTx, USART_PM_NONE);
    usart_baudrate_set(DEBUG_UARTx, baudrate);
    usart_receive_config(DEBUG_UARTx, USART_RECEIVE_ENABLE);
    usart_transmit_config(DEBUG_UARTx, USART_TRANSMIT_ENABLE);
    usart_enable(DEBUG_UARTx);
}

/*
*********************************************************************************************************
*    函 数 名: debug_send_char
*    功能说明: 向调试串口发送1个字节。
*    形    参: 
*    返 回 值: 待发送的字节数据
*    返 回 值: 无
*********************************************************************************************************
*/
void debug_send_char(uint8_t ch)
{
    usart_data_transmit(DEBUG_UARTx, (uint8_t)ch);
    while (RESET == usart_flag_get(DEBUG_UARTx, USART_FLAG_TBE));
}

/*
*********************************************************************************************************
*    函 数 名: debug_send_str
*    功能说明: 向调试串口发送字符串。
*    形    参: 
*    返 回 值: 字符串指针
*    @len        : 发送数据长度
*    返 回 值: 无
*********************************************************************************************************
*/
void debug_send_str(uint8_t *buff, uint16_t len)
{
    while(len--) 
    {
        debug_send_char(buff[0]);
        buff++;
    }
}

/*
*********************************************************************************************************
*    函 数 名: debug_test
*    功能说明: 调试串口测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void debug_test(void)
{
    while(1)
    {
        printf("Hello, debug!\n");
        dwt_delay_ms(1000);
    }
}

//////////////////////////////////////////////////////////////// 
///重定向c库函数printf到串口USARTx，重定向后可使用printf函数
int fputc(int ch, FILE *f)
{
    /* 发送一个字节数据到串口USARTx */
    usart_data_transmit(DEBUG_UARTx, (uint8_t) ch);
    while(RESET == usart_flag_get(DEBUG_UARTx, USART_FLAG_TBE));
    return ch;
}

///重定向c库函数scanf到串口USARTx，重写向后可使用scanf、getchar等函数
int fgetc(FILE *f)
{    
    /* 等待串口USARTx输入数据 */
    while(RESET == usart_flag_get(DEBUG_UARTx, USART_FLAG_RBNE)); //等待串口收到数据
    return usart_data_receive(DEBUG_UARTx); //将收到的数据返回给上一层函数
}

/******************************************  (END OF FILE) **********************************************/




