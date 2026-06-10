#include "bsp_fwdgt.h"
#include "bsp.h"

#define SOFT_FWDGT_ENABLE  0  // 软件看门狗
#define HARD_FWDGT_ENABLE  0  // 硬件看门狗


void bsp_InitFwdgt(uint8_t prer,uint16_t rlr)
{
	#if SOFT_FWDGT_ENABLE > 0U

    /* 看门狗溢出时间Tout=64× 500/32=1000ms */
    fwdgt_config(rlr, prer);

	#endif
	
	#if HARD_FWDGT_ENABLE > 0U
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE); //使能GPIOB|GPIOE的时钟
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;  	// 上拉
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	GPIO_SetBits(GPIOA,GPIO_Pin_0);	
	#endif
}

//喂独立看门狗
void FeedFwdgt(void)
{
	#if SOFT_FWDGT_ENABLE > 0U
	fwdgt_counter_reload();//reload
	#endif
	
	#if HARD_FWDGT_ENABLE > 0U	
	if(GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_0) == 0)
	{
		GPIO_SetBits(GPIOA, GPIO_Pin_0);
	}
	else
	{
		GPIO_ResetBits(GPIOA, GPIO_Pin_0);
	}	
	#endif
}

/*
*********************************************************************************************************
*	函 数 名: fwdgt_test
*	功能说明: FWDGT测试
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void fwdgt_test(void)
{
    /* confiure FWDGT counter clock: 32KHz(IRC32K) / 64 = 0.5 KHz */
    bsp_InitFwdgt(FWDGT_PSC_DIV64,500);

	while(1)
	{
		printf("fwdgt_test\n");
        FeedFwdgt();
		delay_ms(500);
	}

}
		
