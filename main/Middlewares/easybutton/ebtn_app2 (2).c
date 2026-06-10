/*
*********************************************************************************************************
*
*	模块名称 : 独立按键驱动模块 (按键库)
*	文件名称 : ebtn_app.c
*	版    本 : V1.0
*	说    明 : 扫描独立按键，具有软件滤波机制，具有按键FIFO。可以检测如下事件：
*			   支持单击、双击、多击、自动消抖、长按、长长按、超长按 | 低功耗支持 | 组合按键支持 | 静态/动态注册支持
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2013-02-01 armfly  正式发布
*
*	Copyright (C), 2016-2020, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "ebtn.h"
#include "ebtn_app.h"

//引脚定义
/*******************************************************/
#define KEY1_PIN                  GPIO_Pin_2                 
#define KEY1_GPIO_PORT            GPIOD                      
#define KEY1_GPIO_CLK             RCC_AHB1Periph_GPIOD

/*******************************************************/

//Step1：定义KEY_ID、按键参数和按键数组和组合按键数组
typedef enum
{
    KEY_RESET = 0,
    USER_BUTTON_MAX,        /* 实体按键个数 */

    USER_BUTTON_COMBO_0 = 0x100,
    USER_BUTTON_COMBO_MAX,  /* 组合按键 */
} user_button_t;


/* 依次定义GPIO */
typedef struct
{
	GPIO_TypeDef* gpio;
	uint16_t      pin;
	uint8_t       ActiveLevel;	/* 激活电平 */
}X_GPIO_T;

/* GPIO和PIN定义 */
static const X_GPIO_T key_gpio_list[USER_BUTTON_MAX] = {
	{KEY1_GPIO_PORT, KEY1_PIN, 0},	/* RESET */

};	
	
/* 定义按键参数 */
const ebtn_btn_param_t defaul_ebtn_param = EBTN_PARAMS_INIT(
	20, 	// 防抖处理，按下防抖超时，配置为0，代表不启动，此处为20ms
	0,  	// 防抖处理，松开防抖超时，配置为0，代表不启动 
	50,  	// 按键超时处理，按键最短时间，配置为0，代表不检查最小值，此处为50ms  
	500, 	// 按键超时处理，按键最长时间，配置为0xFFFF，代表不检查最大值，用于区分长按和按键事件，此处为500ms，超过不算单击
	200,  // 多击处理，两个按键之间认为是连击的超时时间，此处为200ms（两次点击时间超过则重新计数）
	500, 	// 长按处理，长按周期，每个周期增加keepalive_cnt计数 ，（超过500ms后，每500ms触发一次）
	5 		// 最大连击次数，配置为0，代表不进行连击检查
	);

static ebtn_btn_t btns[] = {
        EBTN_BUTTON_INIT(KEY_RESET    , &defaul_ebtn_param),
//        EBTN_BUTTON_INIT(USER_BUTTON_1, &defaul_ebtn_param),
};

static ebtn_btn_combo_t btns_combo[] = {
        EBTN_BUTTON_COMBO_INIT(USER_BUTTON_COMBO_0, &defaul_ebtn_param),
};


/*
*********************************************************************************************************
*	函 数 名: Key_GPIOInit
*	功能说明: 配置按键对应的GPIO
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void Key_GPIOInit(void)
{	
	GPIO_InitTypeDef GPIO_InitStructure;
	uint8_t i;

	/* 第1步：打开GPIO时钟 */
	RCC_AHB1PeriphClockCmd(KEY1_GPIO_CLK,ENABLE);
	
	/* 第2步：配置所有的按键GPIO为浮动输入模式(实际上CPU复位后就是输入状态) */
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AIN;     /* 设置输入 */
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;  /* 上下拉电阻不使能 */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz; /* GPIO速度等级 */
	
	for (i = 0; i < USER_BUTTON_MAX; i++)
	{
		GPIO_InitStructure.GPIO_Pin = key_gpio_list[i].pin;
		GPIO_Init(key_gpio_list[i].gpio, &GPIO_InitStructure);	
	}
}

/*
*********************************************************************************************************
*	函 数 名: KeyPin_Status
*	功能说明: 判断按键是否按下
*	形    参: 无
*	返 回 值: 返回值1 表示按下(导通），0表示未按下（释放）
*********************************************************************************************************
*/
uint8_t KeyPin_Status(uint8_t _id)
{
	uint8_t level;
	
	if ((key_gpio_list[_id].gpio->IDR & key_gpio_list[_id].pin) == 0)
	{
		level = 0;
	}
	else
	{
		level = 1;
	}

	if (level == key_gpio_list[_id].ActiveLevel)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/*
*********************************************************************************************************
*	函 数 名: prv_btn_get_state
*	功能说明: 获取按键实际状态
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t prv_btn_get_state(struct ebtn_btn *btn)
{
	//获取键码值
	switch(btn->key_id)
	{
		case USER_BUTTON_0:   //ID绑定读取函数
			return KeyPin_Status(USER_BUTTON_0);
		case USER_BUTTON_1:
			return KeyPin_Status(USER_BUTTON_1);
		default:
			//对于库内部处理组合键等情况，或者未知的 key_id，安全起见返回 0(未按下)
		return 0;
	}
    // 注意:返回值 1表示“活动/按下”，0 表示“非活动/释放”
}

/**
 * \brief           Button event
 *
 * \param           btn: Button instance
 * \param           evt: Button event
 */
void prv_btn_event(struct ebtn_btn *btn, ebtn_evt_t evt)
{
	const char *s;
	/* Get event string */
	// 用于长按处理，根据过程中有多少KEEPALIVE事件以及time_keepalive_period可以实现各种复杂的长按功能需求
    if (evt == EBTN_EVT_KEEPALIVE)
    {
        s = "KEEPALIVE";
    }
    else if (evt == EBTN_EVT_ONPRESS)
    {
        s = "ONPRESS";
    }
    else if (evt == EBTN_EVT_ONRELEASE)
    {
        s = "ONRELEASE";
    }
    else if (evt == EBTN_EVT_ONCLICK)
    {
        s = "ONCLICK";
    }
    else
    {
        s = "UNKNOWN";
    }
    printf("ID(hex):%4x, evt: %10s, keep-alive cnt: %3u, click cnt: %3u\r\n", btn->key_id, s,
          (unsigned)btn->keepalive_cnt, (unsigned)btn->click_cnt);
}



/*
*********************************************************************************************************
*	函 数 名: ebtn_app_init
*	功能说明: 初始化按键驱动
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void ebtn_app_init(void)
{
	Key_GPIOInit();
	
  /* Step2：初始化按键驱动 */
  ebtn_init(btns, EBTN_ARRAY_SIZE(btns), btns_combo, EBTN_ARRAY_SIZE(btns_combo), prv_btn_get_state, prv_btn_event);
	/*Step3：配置组合按键comb_key，必须在按键注册完毕后再配置，不然需要`ebtn_combo_btn_add_btn_by_idx`用这个接口 */
  ebtn_combo_btn_add_btn(&btns_combo[0], KEY_RESET);
//	ebtn_combo_btn_add_btn(&btns_combo[0], USER_BUTTON_1);
  /* Step4：动态注册所需按键，并配置comb_key。 */
// dynamic register
//	for (int i = 0; i < (EBTN_ARRAY_SIZE(btns_dyn)); i++)
//	{
//			ebtn_register(&btns_dyn[i]);
//	}

//	ebtn_combo_btn_add_btn(&btns_combo_dyn[0].btn, USER_BUTTON_4);
//	ebtn_combo_btn_add_btn(&btns_combo_dyn[0].btn, USER_BUTTON_5);

//	ebtn_combo_btn_add_btn(&btns_combo_dyn[1].btn, USER_BUTTON_6);
//	ebtn_combo_btn_add_btn(&btns_combo_dyn[1].btn, USER_BUTTON_7);

//	for (int i = 0; i < (EBTN_ARRAY_SIZE(btns_combo_dyn)); i++)
//	{
//			ebtn_combo_register(&btns_combo_dyn[i]);
//	}
}

/*
*********************************************************************************************************
*	函 数 名: etbn_Scan1ms
*	功能说明: 启动按键扫描。非阻塞，被基础定时器中断周期性的调用，1ms一次.
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void etbn_Scan1ms(uint32_t tick)
{
	ebtn_process(tick);
}





