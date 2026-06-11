#include "bsp_led.h"
#include "bsp.h"

/*
    2、指示灯（原理图）：

        系统状态指示灯  STATE      : PC9
        网口指示灯      LAN        : PA12
        4G指示灯        GPRS       : PA11
*/

#define STATE_GPIO_CLK         RCU_GPIOC
#define STATE_GPIO_PORT        GPIOC
#define STATE_GPIO_PIN         GPIO_PIN_9

#define LAN_GPIO_CLK           RCU_GPIOA
#define LAN_GPIO_PORT          GPIOA
#define LAN_GPIO_PIN           GPIO_PIN_12

#define GPRS_GPIO_CLK          RCU_GPIOA
#define GPRS_GPIO_PORT         GPIOA
#define GPRS_GPIO_PIN          GPIO_PIN_11

/* 指示灯闪烁时间*/
#define FLICKER_TIME_Q        (200)
#define FLICKER_TIME          (500)
#define FLICKER_TIME_1S       (1000)

/* 指示灯状态变量*/
typedef struct
{
    uint8_t gprs;
    uint8_t lan;
    uint8_t state;
    uint8_t lan_out;
    uint8_t power_out;
} led_flicker_t;

led_flicker_t sg_ledflicker_t = {0};

/*
*********************************************************************************************************
*    函 数 名: bsp_InitLed
*    功能说明: 初始化指示灯控制io:默认不开启
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitLed(void)
{
    /* enable the led clock */
    rcu_periph_clock_enable(STATE_GPIO_CLK);
    rcu_periph_clock_enable(LAN_GPIO_CLK);
    rcu_periph_clock_enable(GPRS_GPIO_CLK);

    /* configure led GPIO port */ 
    gpio_mode_set(STATE_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,STATE_GPIO_PIN);
    gpio_output_options_set(STATE_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,STATE_GPIO_PIN);

    gpio_mode_set(LAN_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,LAN_GPIO_PIN);
    gpio_output_options_set(LAN_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,LAN_GPIO_PIN);

    gpio_mode_set(GPRS_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,GPRS_GPIO_PIN);
    gpio_output_options_set(GPRS_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPRS_GPIO_PIN);

    gpio_bit_set(STATE_GPIO_PORT,STATE_GPIO_PIN);
    gpio_bit_set(LAN_GPIO_PORT,LAN_GPIO_PIN);
    gpio_bit_set(GPRS_GPIO_PORT,GPRS_GPIO_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: led_control_function
*    功能说明: 点亮指定的LED指示灯。
*    形    参:  dev  : 指示灯序号
*               state: 指示灯状态
*    返 回 值: 无
*********************************************************************************************************
*/
void led_control_function(LD_DEV dev, LED_STATUS state)
{
    switch(dev)
    {
        case LD_STATE:  // 系统运行指示灯
            sg_ledflicker_t.state = state;
            gpio_bit_write(STATE_GPIO_PORT,STATE_GPIO_PIN,(FlagStatus)state);
        break;
            
        case LD_GPRS:  // 4G指示灯
            sg_ledflicker_t.gprs = state;
            gpio_bit_write(GPRS_GPIO_PORT,GPRS_GPIO_PIN,(FlagStatus)state);
        break;
            
        case LD_LAN:   // 有线指示灯
            sg_ledflicker_t.lan = state;
            gpio_bit_write(LAN_GPIO_PORT,LAN_GPIO_PIN,(FlagStatus)state);
        break;

        default:        break;
    }
}

/*
*********************************************************************************************************
*    函 数 名: led_out_control_function
*    功能说明: 点亮外部LED指示灯。
*    形    参:  dev  : 指示灯序号
*               state: 指示灯状态
*    返 回 值: 无
*********************************************************************************************************
*/
void led_out_control_function(LD_DEV dev, LED_STATUS state)
{

}

/*
*********************************************************************************************************
*    函 数 名: led_flicker_control_timer_function
*    功能说明: led闪动
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void led_flicker_control_timer_function(void)
{
    static uint16_t count   = 0;
    static uint16_t count3    = 0;
    
    count++;
    if(count > FLICKER_TIME)
    {
        count = 0;
        if(sg_ledflicker_t.gprs == LD_FLICKER)        /* 显示无线网络状态 */
        {
            gpio_bit_toggle(GPRS_GPIO_PORT,GPRS_GPIO_PIN);
        }

        if(sg_ledflicker_t.lan == LD_FLICKER)        /* 显示有线网络状态 */
        {
            gpio_bit_toggle(LAN_GPIO_PORT,LAN_GPIO_PIN);
        } 
        
        if(sg_ledflicker_t.state == LD_FLICKER)     /* 系统状态灯 */
        {
            gpio_bit_toggle(STATE_GPIO_PORT,STATE_GPIO_PIN);
        }
    }

    count3++;
    if(count3 > FLICKER_TIME_Q)
    {
        count3 = 0;
        if(sg_ledflicker_t.gprs == LD_FLIC_Q)        /* 显示无线网络状态 */
            gpio_bit_toggle(GPRS_GPIO_PORT,GPRS_GPIO_PIN);

        if(sg_ledflicker_t.lan == LD_FLIC_Q)        /* 显示有线网络状态 */
            gpio_bit_toggle(LAN_GPIO_PORT,LAN_GPIO_PIN);
    }
}

/*
*********************************************************************************************************
*    函 数 名: led_all_on
*    功能说明: led全亮
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void led_all_on(void)
{
    gpio_bit_reset(STATE_GPIO_PORT,STATE_GPIO_PIN);
    gpio_bit_reset(LAN_GPIO_PORT,LAN_GPIO_PIN);
    gpio_bit_reset(GPRS_GPIO_PORT,GPRS_GPIO_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: led_all_off
*    功能说明: led全灭
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void led_all_off(void)
{
    gpio_bit_set(STATE_GPIO_PORT,STATE_GPIO_PIN);
    gpio_bit_set(LAN_GPIO_PORT,LAN_GPIO_PIN);
    gpio_bit_set(GPRS_GPIO_PORT,GPRS_GPIO_PIN);
}

/*
*********************************************************************************************************
*    函 数 名: led_test
*    功能说明: led测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void led_test(void)
{
    while(1)
    {
        gpio_bit_toggle(STATE_GPIO_PORT,STATE_GPIO_PIN);
        gpio_bit_toggle(LAN_GPIO_PORT,LAN_GPIO_PIN);
        gpio_bit_toggle(GPRS_GPIO_PORT,GPRS_GPIO_PIN);
        delay_ms(1000);
    }
}
/******************************************  (END OF FILE) **********************************************/


