/*
*********************************************************************************************************
*
*	模块名称 : TIM基本定时中断和PWM驱动模块
*	文件名称 : bsp_tim_pwm.c
*	版    本 : V1.6
*	说    明 : 利用STM32H7内部TIM输出PWM信号， 并实现基本的定时中断
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2013-08-16 armfly  正式发布
*		V1.1	2014-06-15 armfly  完善 bsp_SetTIMOutPWM，当占空比=0和100%时，关闭定时器，GPIO配置为输出
*		V1.2	2015-05-08 armfly  解决TIM8不能输出PWM的问题。
*		V1.3	2015-07-23 armfly  初始化定时器，必须设置 TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0x0000;		
*								   TIM1 和 TIM8 必须设置。否则蜂鸣器的控制不正常。
*		V1.4	2015-07-30 armfly  增加反相引脚输出PWM函数 bsp_SetTIMOutPWM_N();
*		V1.5	2016-02-01 armfly  去掉 TIM_OC1PreloadConfig(TIMx, TIM_OCPreload_Enable);
*		V1.6	2016-02-27 armfly  解决TIM14无法中断的BUG, TIM8_TRG_COM_TIM14_IRQn
*
*	Copyright (C), 2018-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp_timers.h"
#include "bsp.h"
#include "appconfig.h"

//#define  TIMER_DEBUG

/*
	注意，GD32F460有TIM0 – TIM13共计14个定时器。
	Two 16-bit advanced timer (TIMER0 & TIMER7), eight 16-bit general timers (TIMER2,
	TIMER3, TIMER8 ~ TIMER13), two 32-bit general timers (TIMER1 & TIMER4) and two
	16-bit basic timer (TIMER5 & TIMER6)	

	bsp.c 文件中 void SystemClock_Config(void) 函数对时钟的配置如下: 

	System Clock source       = PLL (HSE)
	SYSCLK(Hz)                = 240000000 (CPU Clock)
	APB1 Prescaler            = 4 (APB1 Clock  240MHz)
	APB2 Prescaler            = 4 (APB2 Clock  240MHz)

	APB1 定时器有 TIM1, TIM2 ,TIM3, TIM4, TIM5, TIM6, TIM11, TIM12, TIM13
	APB2 定时器有 TIM0, TIM7 , TIM8, TIM9，TIM10
*/

/*
	全局运行时间，单位1ms
	最长可以表示 24.85天，如果你的产品连续运行时间超过这个数，则必须考虑溢出问题
*/
__IO uint32_t g_iRunTime = 0;

#ifdef TIMER_DEBUG
uint32_t g_timer_test[6] = {0};
#endif

/*
*********************************************************************************************************
*	函 数 名: bsp_SetTIMforInt
*	功能说明: 配置TIM和NVIC，用于简单的定时中断，开启定时中断。另外注意中断服务程序需要由用户应用程序实现。
*	形    参: TIMx : 定时器
*			  _ulFreq : 定时频率 （Hz）。 0 表示关闭。
*			  _PreemptionPriority : 抢占优先级
*			  _SubPriority : 子优先级
*	返 回 值: 无
*********************************************************************************************************
*/

void bsp_InitTimers(uint32_t TIMx, uint32_t _ulFreq, uint8_t _PreemptionPriority, uint8_t _SubPriority)
{
	timer_parameter_struct   timere_initpara = {0};
	uint16_t usPeriod;
	uint16_t usPrescaler;
	uint32_t uiTIMxCLK;
	
	/* 使能TIM时钟 */
	bsp_RCC_TIM_Enable(TIMx);
	rcu_timer_clock_prescaler_config(RCU_TIMER_PSC_MUL4); // 配置定时器时钟

	if ((TIMx == TIMER0) || (TIMx == TIMER7) || (TIMx == TIMER8) || (TIMx == TIMER9) || (TIMx == TIMER10))
	{
		/* APB2 定时器时钟 = 240M */
		uiTIMxCLK = rcu_clock_freq_get(CK_APB2) * 4;
	}
	else	
	{
		/* APB1 定时器 = 240M */
		uiTIMxCLK = rcu_clock_freq_get(CK_APB1) * 4;
	}

	if (_ulFreq < 100)
	{
		usPrescaler = 10000 - 1;					/* 分频比 = 10000 */
		usPeriod =  (uiTIMxCLK / 10000) / _ulFreq  - 1;		/* 自动重装的值 */
	}
	else if (_ulFreq < 3000)
	{
		usPrescaler = 100 - 1;					/* 分频比 = 100 */
		usPeriod =  (uiTIMxCLK / 100) / _ulFreq  - 1;		/* 自动重装的值 */
	}
	else	/* 大于4K的频率，无需分频 */
	{
		usPrescaler = 0;					/* 分频比 = 1 */
		usPeriod = uiTIMxCLK / _ulFreq - 1;	/* 自动重装的值 */
	}

	/* 
		定时器中断更新周期 = TIMxCLK / usPrescaler + 1）/usPeriod + 1）
	*/
	timer_deinit(TIMx);
	timere_initpara.prescaler = usPrescaler;               //  时钟预分频值 0-65535 
	timere_initpara.alignedmode = TIMER_COUNTER_EDGE;     // 边缘对齐                  
	timere_initpara.counterdirection = TIMER_COUNTER_UP;  // 向上计数    
	timere_initpara.period = usPeriod;                       // 周期  
	/* 在输入捕获的时候使用  数字滤波器使用的采样频率之间的分频比例 */
	timere_initpara.clockdivision = TIMER_CKDIV_DIV1;     // 分频因子         
	/* 只有高级定时器才有 配置为x，就重复x+1次进入中断 */    
	timere_initpara.repetitioncounter = 0;					// 重复计数器 0-255  
	
	timer_init(TIMx,&timere_initpara);					// 初始化定时器

	/* 使能定时器中断  */
	timer_interrupt_enable(TIMx,TIMER_INT_UP); 
	
	/* 配置TIM定时更新中断 (Update) */
	{
		uint8_t irq = 0;	/* 中断号, 定义在 stm32h7xx.h */

		if (TIMx == TIMER0) irq = TIMER0_UP_TIMER9_IRQn;
		else if (TIMx == TIMER1) irq = TIMER1_IRQn;
		else if (TIMx == TIMER2) irq = TIMER2_IRQn;
		else if (TIMx == TIMER3) irq = TIMER3_IRQn;
		else if (TIMx == TIMER4) irq = TIMER4_IRQn;
		else if (TIMx == TIMER5) irq = TIMER5_DAC_IRQn;
		else if (TIMx == TIMER6) irq = TIMER6_IRQn;
		else if (TIMx == TIMER7) irq = TIMER7_UP_TIMER12_IRQn;
		else if (TIMx == TIMER8) irq = TIMER0_BRK_TIMER8_IRQn;
		else if (TIMx == TIMER9) irq = TIMER0_UP_TIMER9_IRQn;
		else if (TIMx == TIMER10) irq = TIMER0_TRG_CMT_TIMER10_IRQn;
		else if (TIMx == TIMER11) irq = TIMER7_BRK_TIMER11_IRQn;
		else if (TIMx == TIMER12) irq = TIMER7_UP_TIMER12_IRQn;
		else if (TIMx == TIMER13) irq = TIMER7_TRG_CMT_TIMER13_IRQn;
		else
		{
			Error_Handler(__FILE__, __LINE__);
		}	
		nvic_irq_enable((IRQn_Type)irq,_PreemptionPriority,_SubPriority);	
	}
	
	timer_enable(TIMx);
}

/*
*********************************************************************************************************
*	函 数 名: bsp_RCC_TIM_Enable
*	功能说明: 使能TIM RCC 时钟
*	形    参: TIMx : TIM0 - TIM13
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_RCC_TIM_Enable(uint32_t TIMx)
{
	if (TIMx == TIMER0) 	rcu_periph_clock_enable(RCU_TIMER0);
	else if (TIMx == TIMER1) rcu_periph_clock_enable(RCU_TIMER1);
	else if (TIMx == TIMER2) rcu_periph_clock_enable(RCU_TIMER2);
	else if (TIMx == TIMER3) rcu_periph_clock_enable(RCU_TIMER3);
	else if (TIMx == TIMER4) rcu_periph_clock_enable(RCU_TIMER4);
	else if (TIMx == TIMER5) rcu_periph_clock_enable(RCU_TIMER5);
	else if (TIMx == TIMER6) rcu_periph_clock_enable(RCU_TIMER6);
	else if (TIMx == TIMER7) rcu_periph_clock_enable(RCU_TIMER7);
	else if (TIMx == TIMER8) rcu_periph_clock_enable(RCU_TIMER8);
	else if (TIMx == TIMER9) rcu_periph_clock_enable(RCU_TIMER9);
	else if (TIMx == TIMER10) rcu_periph_clock_enable(RCU_TIMER10);
	else if (TIMx == TIMER11) rcu_periph_clock_enable(RCU_TIMER11);
	else if (TIMx == TIMER12) rcu_periph_clock_enable(RCU_TIMER12);
	else if (TIMx == TIMER13) rcu_periph_clock_enable(RCU_TIMER13);
	else
	{
		Error_Handler(__FILE__, __LINE__);
	}	
}

/*
*********************************************************************************************************
*	函 数 名: bsp_RCC_TIM_Disable
*	功能说明: 关闭TIM RCC 时钟
*	形    参: TIMx : TIM0 - TIM13
*	返 回 值: TIM外设时钟名
*********************************************************************************************************
*/
void bsp_RCC_TIM_Disable(uint32_t TIMx)
{
	/*
		APB1 定时器有 TIM1, TIM2 ,TIM3, TIM4, TIM5, TIM6, TIM11, TIM12, TIM13
		APB2 定时器有 TIM0, TIM7 , TIM8, TIM9，TIM10
	*/
	if (TIMx == TIMER0) 		rcu_periph_clock_disable(RCU_TIMER0);
	else if (TIMx == TIMER1) 	rcu_periph_clock_disable(RCU_TIMER1);
	else if (TIMx == TIMER2) 	rcu_periph_clock_disable(RCU_TIMER2);
	else if (TIMx == TIMER3) 	rcu_periph_clock_disable(RCU_TIMER3);
	else if (TIMx == TIMER4) 	rcu_periph_clock_disable(RCU_TIMER4);
	else if (TIMx == TIMER5) 	rcu_periph_clock_disable(RCU_TIMER5);
	else if (TIMx == TIMER6) 	rcu_periph_clock_disable(RCU_TIMER6);
	else if (TIMx == TIMER7) 	rcu_periph_clock_disable(RCU_TIMER7);
	else if (TIMx == TIMER8) 	rcu_periph_clock_disable(RCU_TIMER8);
	else if (TIMx == TIMER9) 	rcu_periph_clock_disable(RCU_TIMER9);
	else if (TIMx == TIMER10) 	rcu_periph_clock_disable(RCU_TIMER10);
	else if (TIMx == TIMER11) 	rcu_periph_clock_disable(RCU_TIMER11);
	else if (TIMx == TIMER12) 	rcu_periph_clock_disable(RCU_TIMER12);
	else if (TIMx == TIMER13) 	rcu_periph_clock_disable(RCU_TIMER13);
	else
	{
		Error_Handler(__FILE__, __LINE__);
	}
}
/*
*********************************************************************************************************
*	函 数 名: TIMER1_IRQHandler
*	功能说明: 定时器中断函数
*	形    参: 无
*	返 回 值: 无
*	TIM定时中断服务程序范例，必须清中断标志
*********************************************************************************************************
*/
void TIMER1_IRQHandler(void)
{
	if(timer_interrupt_flag_get(TIMER1,TIMER_INT_FLAG_UP) != RESET)
	{
		timer_interrupt_flag_clear(TIMER1,TIMER_INT_FLAG_UP);  // 清除中断标志位 
		/* 全局运行时间每1ms增1 */
		g_iRunTime++;		
 
#ifdef TIMER_DEBUG
		g_timer_test[0]++;
		if(g_timer_test[0] >= 1000)
		{
			g_timer_test[0] = 0;
			printf("TIMER1 test\n");
		}
#endif		
	}
}



/*
*********************************************************************************************************
*	函 数 名: TIMER2_IRQHandler
*	功能说明: 定时器中断函数
*	形    参: 无
*	返 回 值: 无
*	TIM定时中断服务程序范例，必须清中断标志
*********************************************************************************************************
*/
void TIMER2_IRQHandler(void)
{
	if(timer_interrupt_flag_get(TIMER2,TIMER_INT_FLAG_UP) != RESET)
	{
		timer_interrupt_flag_clear(TIMER2,TIMER_INT_FLAG_UP);  // 清除中断标志位 

		 lwip_ping_timer_function();
		 app_com_time_function();
		 app_sys_operate_timer_function();
		 eth_ping_timer_function();
		 led_flicker_control_timer_function();
		 //app_reboot_timer_run();
		// bl0906_run_timer_function();
		 device_reboot_timer_function();
		 com_queue_time_function();
		 bl0939_run_timer_function();
		
#ifdef TIMER_DEBUG
		g_timer_test[1]++;
		if(g_timer_test[1] >= 1000)
		{
			g_timer_test[1] = 0;
			printf("TIMER2 test\n");
		}
#endif		
	}
}

/*
*********************************************************************************************************
*	函 数 名: TIMER3_IRQHandler
*	功能说明: 定时器中断函数
*	形    参: 无
*	返 回 值: 无
*	TIM定时中断服务程序范例，必须清中断标志
*********************************************************************************************************
*/
void TIMER3_IRQHandler(void)
{
	if(timer_interrupt_flag_get(TIMER3,TIMER_INT_FLAG_UP) != RESET)
	{
		timer_interrupt_flag_clear(TIMER3,TIMER_INT_FLAG_UP);  // 清除中断标志位 
		
#ifdef TIMER_DEBUG
		g_timer_test[2]++;
		if(g_timer_test[2] >= 1000)
		{
			g_timer_test[2] = 0;
			printf("TIMER3 test\n");
		}
#endif		
	}
}

/*
*********************************************************************************************************
*	函 数 名: TIMER4_IRQHandler
*	功能说明: 定时器中断函数
*	形    参: 无
*	返 回 值: 无
*	TIM定时中断服务程序范例，必须清中断标志
*********************************************************************************************************
*/
void TIMER4_IRQHandler(void)
{
	if(timer_interrupt_flag_get(TIMER4,TIMER_INT_FLAG_UP) != RESET)
	{
		timer_interrupt_flag_clear(TIMER4,TIMER_INT_FLAG_UP);  // 清除中断标志位 
		
#ifdef TIMER_DEBUG
		g_timer_test[3]++;
		if(g_timer_test[3] >= 1000)
		{
			g_timer_test[3] = 0;
			printf("TIMER4 test\n");
		}
#endif		
	}
}

/*
*********************************************************************************************************
*	函 数 名: TIMER5_DAC_IRQHandler
*	功能说明: 定时器中断函数
*	形    参: 无
*	返 回 值: 无
*	TIM定时中断服务程序范例，必须清中断标志
*********************************************************************************************************
*/
void TIMER5_DAC_IRQHandler(void)
{
	if(timer_interrupt_flag_get(TIMER5,TIMER_INT_FLAG_UP) != RESET)
	{
		port_scan_timer_function();
		rtsp_timer_function();
		timer_interrupt_flag_clear(TIMER5,TIMER_INT_FLAG_UP);  // 清除中断标志位 

#ifdef TIMER_DEBUG
		g_timer_test[4]++;
		if(g_timer_test[4] >= 5)
		{
			g_timer_test[4] = 0;
			printf("TIMER5 test\n");
		}
#endif		
	}
}

/*
*********************************************************************************************************
*	函 数 名: TIMER6_IRQHandler
*	功能说明: 定时器中断函数
*	形    参: 无
*	返 回 值: 无
*	TIM定时中断服务程序范例，必须清中断标志
*********************************************************************************************************
*/
void TIMER6_IRQHandler(void)
{
	if(timer_interrupt_flag_get(TIMER6,TIMER_INT_FLAG_UP) != RESET)
	{
		
		app_sys_net_relay_reload_num_times();
		app_sys_net_operate_relay();
		timer_interrupt_flag_clear(TIMER6,TIMER_INT_FLAG_UP);  // 清除中断标志位 

#ifdef TIMER_DEBUG
		g_timer_test[5]++;
		if(g_timer_test[5] >= 5)
		{
			g_timer_test[5] = 0;
			printf("TIMER6 test\n");
		}
#endif		
	}
}

uint32_t HAL_GetTick(void)
{
	return g_iRunTime;
}

static uint32_t sg_reboot_time = 0;

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
void set_reboot_time_function(uint32_t time)
{
	lfs_unmount(&g_lfs_t);
	sg_reboot_time = time;
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
static void device_reboot_timer_function(void)
{
	if(sg_reboot_time != 0) {
		sg_reboot_time--;
		if(sg_reboot_time==0) {
			sys_soft_reset();
		}
	}
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
