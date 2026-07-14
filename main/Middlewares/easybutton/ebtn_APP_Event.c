/** ***************************************************************************
 * @File Name: ebtn_APP_Event.c
 * @brief 自定义按键状态检测和事件处理函数
 * @credit : bobwenstudy / easy_button https://github.com/bobwenstudy/easy_button
 * @Author : Sighthesia / easy_button-Application https://github.com/Sighthesia/easy_button-Application/tree/main
 * @Version : 1.3.0
 * @Creat Date : 2025-05-16
 * ----------------------------------------------------------------------------
 * @Modification
 * @Author : Sighthesia
 * @Changes :
 *   - 强化事件回调用法示例，按键/组合键分支结构清晰
 *   - 优化注释，突出 keepalive/连击等事件的典型用法
 *   - 注释统一与开源风格对齐
 *   - 实现文件包含 `ebtn_APP_HAL.h` 与 `ebtn_APP_Keys.h`，头文件中保持最小声明以减少传递包含
 * @Modifi Date : 2025-09-14
 */
#include "ebtn_APP_Event.h"
#include "ebtn_APP_Keys.h"
#include "ebtn_APP_HAL.h"
#include "./Task/inc/det.h"
#include "bsp.h"

#define  KEY_DEBUG  1
/* ---------------------------- 此函数中可自定义按键状态检测方式 ---------------------------- */

/** ***************************************************************************
 * @brief  获取按键状态回调函数，每次执行ebtn_process都会调用此函数
 * 此函数默认采用了查表检测，免去需要手动为每个按键添加检测方式
 * 也可为特殊按键自定义检测方法
 * @note   查表检测需要配合ebtn_custom_config.c中的按键配置结构体数组使用
 * @param  btn: easy_button按键结构体指针
 * @return 按键状态，0表示未按下，1表示按下
 */
uint8_t Get_Key_State(struct ebtn_btn *btn)
{
    // 查表法检测
    for (int i = 0; i < keys_list_size; i++)
    {
        if (keys_config_list[i].key_id == btn->key_id)
        {
            uint8_t pin_state = ebtn_APP_HAL.Read_Pin(keys_config_list[i].gpio_port, keys_config_list[i].gpio_pin);
            // 根据有效电平转换
            if (keys_config_list[i].active_level == pin_state)
            {
                pin_state = 1;
            }
            else
            {
                pin_state = 0;
            }
            return pin_state;
        }
    }

    /* ------------------------------- 此处自定义特殊按键检测 ------------------------------ */
    // 可自定义特殊按键的检测方式，如矩阵按键的检测

    return 0; // 未找到按键ID，返回0
}

/* ------------------------------- 此处自定义按键触发事件 ------------------------------ */

/** ***************************************************************************
 * @brief  按键事件处理回调函数，在此定义按键触发事件
 * @param  btn: easy_button按键结构体指针
 * @param  evt: 事件类型
 */
void ebtn_APP_Event(struct ebtn_btn *btn, ebtn_evt_t evt)
{
    switch (btn->key_id) // 按键ID
    {
    /* ---------------------------------- RESET_KEY ---------------------------------- */
    case RESET_KEY:
        /* ---------------------------------- 按下按键时 --------------------------------- */
        if (evt == EBTN_EVT_ONPRESS)
        {
                    
        }
        /* ----------------------------- 短按按键时（可获取连击次数） ----------------------------- */
        else if (evt == EBTN_EVT_ONCLICK)
        {
            /* ----------------------------------- 单击时 ---------------------------------- */
            if (btn->click_cnt == 1)
            {
                if(KEY_DEBUG)  printf("RESET_KEY ONCLICK\n");
            }
            /* ----------------------------------- 6击时 ---------------------------------- */
            else if (btn->click_cnt == 6)
            {
                det_set_key_value(RESET_KEY,KEY_ERASE);
                if(KEY_DEBUG)   printf("RESET_KEY ONCLICK 6 times\n");
            }
        }
        /* ------------------------- 长按达到最短时间（配置默认500ms），触发长按计数时 ------------------------ */
        else if (evt == EBTN_EVT_KEEPALIVE)
        {
            /* ------------------------------- 长按计数到达指定值时 ------------------------------- */
            if (btn->keepalive_cnt == 1)
            {
                det_set_key_value(RESET_KEY,KEY_EVNT);
                if(KEY_DEBUG)  printf("RESET_KEY long\n");
            }
        }
        break;
        /* ----------------------------------- DOOR_KEY ---------------------------------- */
    case DOOR_KEY:
        if (evt == EBTN_EVT_KEEPALIVE)
        {
            /* ------------------------------- 长按计数到达指定值时 ------------------------------- */
            if (btn->keepalive_cnt == 1)
            {
                det_set_key_value(DOOR_KEY,KEY_EVNT);
                if(KEY_DEBUG)  printf("DOOR_KEY long\n");
            }
        }
        else if (evt == EBTN_EVT_ONRELEASE)// 按键 释放处理
        {
            det_set_key_value(DOOR_KEY,KEY_NONE);
        }
        break;

        /* ----------------------------------- PWR_KEY ---------------------------------- */
    case PWR_KEY:
        if (evt == EBTN_EVT_ONPRESS)
        {
            det_set_key_value(PWR_KEY,KEY_EVNT);
            if(KEY_DEBUG)  printf("PWR_KEY ONPRESS\n");
        }
        else if (evt == EBTN_EVT_ONRELEASE)// 按键 释放处理
        {
            det_set_key_value(PWR_KEY,KEY_NONE);
        }
        break;
#if (configUSE_KEY_WATER == 1)
        /* ----------------------------------- WATER_KEY ---------------------------------- */
    case WATER_KEY:
        if (evt == EBTN_EVT_KEEPALIVE)
        {
            /* ------------------------------- 长按计数到达指定值时 ------------------------------- */
            if (btn->keepalive_cnt == 1)
            {
                det_set_key_value(WATER_KEY,KEY_EVNT);
                if(KEY_DEBUG)  printf("WATER_KEY long\n");
            }
        }
        else if (evt == EBTN_EVT_ONRELEASE)// 按键 释放处理
        {
            det_set_key_value(WATER_KEY,KEY_NONE);
        }
        break;
#endif
#if (configUSE_KEY_SPD == 1)
        /* ----------------------------------- SPD_KEY ---------------------------------- */
    case SPD_KEY:
        if (evt == EBTN_EVT_KEEPALIVE)
        {
            /* ------------------------------- 长按计数到达指定值时 ------------------------------- */
            if (btn->keepalive_cnt == 1)
            {
                det_set_key_value(SPD_KEY,KEY_EVNT);
                if(KEY_DEBUG)  printf("SPD_KEY long\n");
            }
        }
        else if (evt == EBTN_EVT_ONRELEASE)// 按键 释放处理
        {
            det_set_key_value(SPD_KEY,KEY_NONE);
        }
        break;
#endif
#if (configUSE_KEY_MCB == 1)
        /* ----------------------------------- MCB_KEY ---------------------------------- */
    case MCB_KEY:
        if (evt == EBTN_EVT_KEEPALIVE)
        {
            /* ------------------------------- 长按计数到达指定值时 ------------------------------- */
            if (btn->keepalive_cnt == 1)
            {
                det_set_key_value(MCB_KEY,KEY_EVNT);
                if(KEY_DEBUG)  printf("MCB_KEY long\n");
            }
        }
        else if (evt == EBTN_EVT_ONRELEASE)// 按键 释放处理
        {
            det_set_key_value(MCB_KEY,KEY_NONE);
        }
        break;
#endif
    }
}



