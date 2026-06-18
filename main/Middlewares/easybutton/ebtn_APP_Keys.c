/** ***************************************************************************
 * @File Name: ebtn_APP_Keys.c
 * @brief 自定义按键参数配置文件及按键定义
 * @credit : bobwenstudy / easy_button https://github.com/bobwenstudy/easy_button
 * @Author : Sighthesia / easy_button-Application https://github.com/Sighthesia/easy_button-Application/tree/main
 * @Version : 1.3.0
 * @Creat Date : 2025-05-10
 * ----------------------------------------------------------------------------
 * @Modification
 * @Author : Sighthesia
 * @Changes :
 *   - 使用查表法定义键位与GPIO映射，增加 `EBTN_ACTIVE_LOW/HIGH` 语义化电平
 *   - 组合键配置改为变参宏 `COMBO_KEYS`，支持自适应长度
 *   - 导出数组与大小常量，便于 `ebtn_APP` 自动化初始化
 *   - 注释精简与开源风格统一
 *   - 通过包含 `ebtn_APP_HAL.h` 获取平台定义（HAL 头默认包含 `main.h`），移除对 `main.h` 的直接包含
 * @Modifi Date : 2025-09-14
 */
#include "ebtn_APP_Keys.h"
#include "ebtn_APP_HAL.h"

/* -------------------------------- 此处修改按键引脚定义-------------------------------- */
/*
    4、输入检测
        按键(恢复出厂设置):    PD2
        箱门检测:             PA11
        12V电源输入监测:       PD0
        水浸 :               PD13	
        防雷检测：            PD14
        空开检测：            PD15
*/
#define RESET_KEY_PIN               GPIO_PIN_2                
#define RESET_KEY_GPIO_PORT         GPIOD                      
#define RESET_KEY_GPIO_CLK          RCU_GPIOD
#define RESET_KEY_ACTIVE_LEVEL      EBTN_ACTIVE_LOW

#define DOOR_KEY_PIN                GPIO_PIN_11                
#define DOOR_KEY_GPIO_PORT          GPIOA                      
#define DOOR_KEY_GPIO_CLK           RCU_GPIOA    
#define DOOR_KEY_ACTIVE_LEVEL       EBTN_ACTIVE_LOW

#define PWR_KEY_PIN                 GPIO_PIN_0                
#define PWR_KEY_GPIO_PORT           GPIOD                      
#define PWR_KEY_GPIO_CLK            RCU_GPIOD    
#define PWR_KEY_ACTIVE_LEVEL        EBTN_ACTIVE_HIGH

#if (configUSE_KEY_WATER == 1)
    #define WATER_KEY_PIN               GPIO_PIN_13
    #define WATER_KEY_GPIO_PORT         GPIOD
    #define WATER_KEY_GPIO_CLK          RCU_GPIOD    
    #define WATER_KEY_ACTIVE_LEVEL      EBTN_ACTIVE_HIGH
#endif

#if (configUSE_KEY_SPD == 1)
    #define SPD_KEY_PIN               GPIO_PIN_14
    #define SPD_KEY_GPIO_PORT         GPIOD                      
    #define SPD_KEY_GPIO_CLK          RCU_GPIOD
    #define SPD_KEY_ACTIVE_LEVEL      EBTN_ACTIVE_LOW
#endif

#if (configUSE_KEY_MCB == 1)
    #define MCB_KEY_PIN               GPIO_PIN_15
    #define MCB_KEY_GPIO_PORT         GPIOD                      
    #define MCB_KEY_GPIO_CLK          RCU_GPIOD    
    #define MCB_KEY_ACTIVE_LEVEL      EBTN_ACTIVE_HIGH
#endif

/** ***************************************************************************
 * @brief 定义默认按键参数结构体
 * @ref ebtn_APP_Keys.h
 */
ebtn_btn_param_t buttons_parameters = EBTN_PARAMS_INIT(
    DEBOUNCE_TIME,            // 按下防抖超时
    RELEASE_DEBOUNCE_TIME,    // 松开防抖超时
    CLICK_AND_PRESS_MIN_TIME, // 按键最短时间
    CLICK_AND_PRESS_MAX_TIME, // 按键最长时间
    MULTI_CLICK_MAX_TIME,     // 连续点击最大间隔(ms)
    KEEPALIVE_TIME_PERIOD,    // 长按报告事件间隔(ms)
    MAX_CLICK_COUNT           // 最大连续点击次数
);

/* -------------------------------- 此处修改按键定义 -------------------------------- */

// 按键ID为 ebtn_APP_Keys.h 中的枚举定义，初始化函数会为所有枚举值初始化

/** ***************************************************************************
 * @brief 按键列表结构体数组，用于将按键ID与GPIO引脚以及触发电平进行绑定，
 * 同时使用查表检测,免去需要为每个按键手动添加检测方式
 * @note 此处填入所需的按键ID及其GPIO信息和触发时电平
 */
    
/* 使能GPIO时钟 */

                                                                    
key_config_t keys_config_list[] = {
    // 示例：四个按键
    // 按键ID，    GPIO端口，            GPIO引脚，        触发电平
    {RESET_KEY,  RESET_KEY_GPIO_PORT,  RESET_KEY_PIN,  RESET_KEY_ACTIVE_LEVEL},  // 复位       PD2， 低电平触发
    {DOOR_KEY,   DOOR_KEY_GPIO_PORT,   DOOR_KEY_PIN,   DOOR_KEY_ACTIVE_LEVEL},  // 箱门       PA11，低电平触发
    {PWR_KEY  ,  PWR_KEY_GPIO_PORT,    PWR_KEY_PIN,    PWR_KEY_ACTIVE_LEVEL}, // 12V电源    PD0， 高电平触发
#if (configUSE_KEY_WATER == 1)
    {WATER_KEY,  WATER_KEY_GPIO_PORT,  WATER_KEY_PIN,  WATER_KEY_ACTIVE_LEVEL}, // 水浸       PD13，高电平触发
#endif
#if (configUSE_KEY_SPD == 1)
    {SPD_KEY,    SPD_KEY_GPIO_PORT,  SPD_KEY_PIN,  SPD_KEY_ACTIVE_LEVEL}, // 防雷检测 PD14，低电平触发
#endif
#if (configUSE_KEY_MCB == 1)
    {MCB_KEY,    MCB_KEY_GPIO_PORT,  MCB_KEY_PIN,  MCB_KEY_ACTIVE_LEVEL}, // 空开检测 PD15，高电平触发
#endif
};

/* --------------------------------- 此处修改组合键配置 -------------------------------- */

/** ***************************************************************************
 * @brief 组合键配置表，定义每个组合键包含哪些单独按键
 * @note 支持自适应长度的按键数组，无固定长度限制
 */
const combo_config_t combo_keys_config_list[1] = {{(combo_key_enum_t)0, 0, NULL}  // 底部亚元，避免空数组警告
    // 示例：四个组合键
    //         组合键ID,    按键1, 按键2, ..., 按键N
//    COMBO_KEYS(COMBO_KEY_1, KEY_1, KEY_2),
//    COMBO_KEYS(COMBO_KEY_2, KEY_2, KEY_3),
//    COMBO_KEYS(COMBO_KEY_3, KEY_1, KEY_2, KEY_3),
//    COMBO_KEYS(COMBO_KEY_4, KEY_1, KEY_2, KEY_3, KEY_4),
};

/* -------------------------------- 此处修改可选参数 -------------------------------- */

/** ***************************************************************************
 * @brief 按键单独参数配置表（可选）
 * @note 只为需要单独参数的按键配置，未配置的按键将自动使用默认的 buttons_parameters
 * @note 如果所有按键都使用默认参数，此表可以为空
 */
const special_key_list_t special_keys_list[1] = {{(key_enum_t)KEYS_COUNT, NULL}  // 底部亚元，避免空数组警告
    /* --------------------------------- 此处配置需要单独参数的按键 -------------------------------- */
    // 示例：只配置需要单独参数的按键

    // 方式1：使用单独参数配置宏（推荐）
    // KEY_SPECIAL_CONFIG(KEY_2, fast_response_parameters),  // KEY_2使用快速响应参数
    // KEY_SPECIAL_CONFIG(KEY_4, slow_response_parameters),  // KEY_4使用慢速响应参数

    // 方式2：手动配置
    // {KEY_3, &special_parameters},  // KEY_3使用单独参数

    // 如果所有按键都使用默认参数，可以留空
    // 初始化函数会为所有按键使用 buttons_parameters
};

/** ***************************************************************************
 * @brief 组合键单独参数配置表（可选）
 * @note 只为需要单独参数的组合键配置，未配置的组合键将自动使用默认的 buttons_parameters
 * @note 如果所有组合键都使用默认参数，此表可以为空
 */
const special_combo_key_list_t special_combo_keys_list[1] = {{(combo_key_enum_t)0, NULL}  // 底部亚元，避免空数组警告
    /* --------------------------------- 此处配置需要单独参数的组合键 -------------------------------- */
    // 示例：只配置需要单独参数的组合键

    // 方式1：使用单独参数配置宏（推荐）
    // COMBO_SPECIAL_CONFIG(COMBO_KEY_1, long_press_parameters),  // COMBO_KEY_1使用长按参数
    // COMBO_SPECIAL_CONFIG(COMBO_KEY_3, quick_combo_parameters), // COMBO_KEY_3使用快速组合参数

    // 方式2：手动配置
    // {COMBO_KEY_2, &special_combo_parameters},  // COMBO_KEY_2使用单独组合参数

    // 如果所有组合键都使用默认参数，可以留空
    // 初始化函数会为所有组合键使用 buttons_parameters
};

/* -------------------------------- 自定义配置部分结束 ------------------------------- */

ebtn_btn_t btn_array[KEYS_COUNT] = {0};                                               // 基于枚举自动确定数组大小
ebtn_btn_combo_t btn_combo_array[COMBO_KEYS_COUNT] = {0};                             // 使用实际组合键数量
const uint8_t btn_array_size = KEYS_COUNT;                                            // 按键数组大小由枚举自动确定
const uint8_t btn_combo_array_size = COMBO_KEYS_COUNT;                                // 组合键数组大小为实际数量
const uint8_t keys_list_size = EBTN_ARRAY_SIZE(keys_config_list);                     // 硬件配置列表大小
const uint8_t combo_config_list_size = EBTN_ARRAY_SIZE(combo_keys_config_list);       // 组合键配置表大小
const uint8_t special_key_config_list_size = EBTN_ARRAY_SIZE(special_keys_list);      // 按键配置表大小
const uint8_t special_combo_key_list_size = EBTN_ARRAY_SIZE(special_combo_keys_list); // 组合键配置表大小



/*
*********************************************************************************************************
*    函 数 名: bsp_InitKeyHard
*    功能说明: 配置按键对应的GPIO
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitKeyHard(void)
{    
    uint8_t i;
    /* 第1步：打开GPIO时钟 */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_GPIOF);
        
   /* 第2步：配置所有的按键GPIO为浮动输入模式(实际上CPU复位后就是输入状态) */
    for (i = 0; i < KEYS_COUNT; i++)
    {
        gpio_mode_set(keys_config_list[i].gpio_port, GPIO_MODE_INPUT, GPIO_PUPD_NONE,keys_config_list[i].gpio_pin);
    }
}


