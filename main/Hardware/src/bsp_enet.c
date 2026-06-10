/*
 * @Description: 以太网模块源文件
 * @Version: v1.0.0
 * @Autor: gxf
 * @Date: 2022-02-05 22:07:46
 * @LastEditors: gxf
 * @LastEditTime: 2022-02-07 09:12:32
 */

#include "bsp.h"
#include "bsp_enet.h"

static __IO uint32_t enet_init_status = 0;

static void enet_gpio_config(void);
static void enet_mac_dma_config(void);
static void nvic_configuration(void);

/*
*********************************************************************************************************
*	函 数 名: enet_system_setup
*	功能说明: 初始化以太网系统GPIO、时钟、MAC、DMA、SysTick.  
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
#define ENET_INIT_RETRY_MAX   3
#define ENET_INIT_RETRY_DELAY_MS  100

void enet_system_setup(void)
{
	  uint8_t retry_count = 0;
    /* configure the NVIC for ENET */
    nvic_configuration();

    /* configure the GPIO ports for ethernet pins */
    enet_gpio_config();

    /* configure the ethernet MAC/DMA */
//    enet_mac_dma_config();
	
	   do {
        /* configure the ethernet MAC/DMA */
        enet_mac_dma_config();

        if(enet_init_status != 0) {
            break;
        }

        retry_count++;
        printf("Ethernet init failed, retrying (%d/%d)...\r\n", retry_count, ENET_INIT_RETRY_MAX);
        vTaskDelay(ENET_INIT_RETRY_DELAY_MS);

    } while(retry_count < ENET_INIT_RETRY_MAX);

    if(0 == enet_init_status) {
        while(1) {
					Error_Handler(__FILE__, __LINE__);
        }
    }

    enet_interrupt_enable(ENET_DMA_INT_NIE);
    enet_interrupt_enable(ENET_DMA_INT_RIE);

#ifdef SELECT_DESCRIPTORS_ENHANCED_MODE
    enet_desc_select_enhanced_mode();
#endif /* SELECT_DESCRIPTORS_ENHANCED_MODE */

}

/*
*********************************************************************************************************
*	函 数 名: enet_mac_dma_config
*	功能说明: 初始化以太网MAC/DMA.  
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void enet_mac_dma_config(void)
{
    ErrStatus reval_state = ERROR;

    /* enable SYSCFG clock and select RMII */
    rcu_periph_clock_enable(RCU_SYSCFG);
    syscfg_enet_phy_interface_config(SYSCFG_ENET_PHY_RMII);

    /* enable ethernet clock  */
    rcu_periph_clock_enable(RCU_ENET);
    rcu_periph_clock_enable(RCU_ENETTX);
    rcu_periph_clock_enable(RCU_ENETRX);

    /* reset ethernet on AHB bus */
    enet_deinit();

    reval_state = enet_software_reset();
    if(ERROR == reval_state) 
    {
        Error_Handler(__FILE__, __LINE__);
    }

#ifdef CHECKSUM_BY_HARDWARE
    enet_init_status = enet_init(ENET_AUTO_NEGOTIATION, ENET_AUTOCHECKSUM_DROP_FAILFRAMES, ENET_BROADCAST_FRAMES_PASS);
#else
    enet_init_status = enet_init(ENET_AUTO_NEGOTIATION, ENET_NO_AUTOCHECKSUM, ENET_BROADCAST_FRAMES_PASS);
#endif /* CHECKSUM_BY_HARDWARE */
    enet_fliter_feature_enable(ENET_MULTICAST_FILTER_PASS);
}

/*
*********************************************************************************************************
*	函 数 名: nvic_configuration
*	功能说明: 初始化NVIC.  
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void nvic_configuration(void)
{
    nvic_irq_enable(ENET_IRQn, 6, 0); // Must be >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (which is 5)
}

/*
*********************************************************************************************************
*	函 数 名: enet_gpio_config
*	功能说明: 初始化GPIO.  
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void enet_gpio_config(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(ENET_RESET_GPIO_CLK);
    rcu_periph_clock_enable(ENET_ADDR_GPIO_CLK);

    /* PA1: ETH_RMII_REF_CLK */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_1);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_1);

    /* PA2: ETH_MDIO */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_2);

    /* PA7: ETH_RMII_CRS_DV */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_7);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_7);

    gpio_af_set(GPIOA, GPIO_AF_11, GPIO_PIN_1);
    gpio_af_set(GPIOA, GPIO_AF_11, GPIO_PIN_2);
    gpio_af_set(GPIOA, GPIO_AF_11, GPIO_PIN_7);

    /* PB11: ETH_RMII_TX_EN */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_11);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_11);

    /* PB12: ETH_RMII_TXD0 */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_12);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_12);

    /* PB13: ETH_RMII_TXD1 */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_13);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_13);

    gpio_af_set(GPIOB, GPIO_AF_11, GPIO_PIN_11);
    gpio_af_set(GPIOB, GPIO_AF_11, GPIO_PIN_12);
    gpio_af_set(GPIOB, GPIO_AF_11, GPIO_PIN_13);

    /* PC1: ETH_MDC */
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_1);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_1);

    /* PC4: ETH_RMII_RXD0 */
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_4);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_4);

	/* PC5: ETH_RMII_RXD1 */
	gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_5);
	gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_5);

	gpio_af_set(GPIOC, GPIO_AF_11, GPIO_PIN_1);
	gpio_af_set(GPIOC, GPIO_AF_11, GPIO_PIN_4);
	gpio_af_set(GPIOC, GPIO_AF_11, GPIO_PIN_5);

	// 复位
	gpio_mode_set(ENET_RESET_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,ENET_RESET_PIN);
	gpio_output_options_set(ENET_RESET_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,ENET_RESET_PIN);

	/* 地址 */
	gpio_mode_set(ENET_ADDR_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,ENET_ADDR_PIN);
	gpio_output_options_set(ENET_ADDR_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,ENET_ADDR_PIN);

    gpio_bit_reset(ENET_ADDR_GPIO_PORT, ENET_ADDR_PIN); // 地址0
	/* 硬复位 */
	gpio_bit_reset(ENET_RESET_GPIO_PORT, ENET_RESET_PIN);
	delay_ms(50);	
	gpio_bit_set(ENET_RESET_GPIO_PORT, ENET_RESET_PIN);
    delay_ms(50);

}

/*
*********************************************************************************************************
*	函 数 名: enet_hard_reset
*	功能说明: 硬件复位函数.  
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void enet_hard_reset(void)
{
    gpio_bit_reset(ENET_RESET_GPIO_PORT, ENET_RESET_PIN);
	delay_ms(50);	
    gpio_bit_set(ENET_RESET_GPIO_PORT, ENET_RESET_PIN);
	delay_ms(100);

	enet_mac_dma_config();
}




