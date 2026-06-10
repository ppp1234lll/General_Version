/********************************************************************************
* @File name  : GPS模块
* @Description: 串口2-对应GPS
* @Author     : ZHLE
*  Version Date        Modification Description
	13、GPS(4G模块)： 串口2， 波特率：9600，引脚分配为： 
		BDS_TX：    PD5
        BDS_RX：    PD6	
********************************************************************************/

#include "ATGM336H.h"
#include "appconfig.h"

//#define USE_USART1_TX_DMA		/* 发送 DMA方式  */

// #define USE_USART1_INT		/* 中断方式 */
 #define USE_USART1_IDEL		/* DMA接收+空闲中断方式 */
//#define USE_USART1_TIMEOUT		/* DMA接收+超时检测中断方式 */

__attribute__((section (".RAM_D1"))) atgm336h_data_t sg_atgm336h_param_t; // 定位信息

#define USART_GPS_TX_GPIO_CLK               RCU_GPIOD
#define USART_GPS_TX_GPIO_PORT              GPIOD
#define USART_GPS_TX_PIN                    GPIO_PIN_5
#define USART_GPS_TX_PIN_AF			           GPIO_AF_7

#define USART_GPS_RX_GPIO_CLK               RCU_GPIOD
#define USART_GPS_RX_GPIO_PORT              GPIOD
#define USART_GPS_RX_PIN                    GPIO_PIN_6
#define USART_GPS_RX_PIN_AF			           GPIO_AF_7

//#define UART_GPS_CLK                       RCC_APB1Periph_USART2
//#define UART_GPS                           USART2
//#define UART_GPS_IRQn                      USART2_IRQn
//#define UART_GPS_IRQHandler                USART2_IRQHandler

#define USART_GPS_DMA_CLK					         RCU_DMA0
#define USART_GPS_DMAx						           DMA0
#define USART_GPS_TX_DMA_CHANNEL			       DMA_CH6
#define USART_GPS_RX_DMA_CHANNEL			       DMA_CH5

#define USART_GPS_TX_DMA_PERIEN			       DMA_SUBPERI4
#define USART_GPS_RX_DMA_PERIEN			       DMA_SUBPERI4

#define USART_GPS_DMA_TX_IRQn				       DMA0_Channel6_IRQn
#define USART_GPS_DMA_RX_IRQn				       DMA0_Channel5_IRQn

#define USART_GPS_DMA_TX_IRQHandler		     DMA0_Channel6_IRQHandler
#define USART_GPS_DMA_RX_IRQHandler 		     DMA0_Channel5_IRQHandler


//enum {
//	TRANSFER_WAIT,
//	TRANSFER_TX_COMPLETE,
//	TRANSFER_RX_COMPLETE,
//	TRANSFER_ERROR,
//};

/* 参数 */
#define UART_GPS_RX_MAX  2048
uint8_t *g_usart1_tx_buff = NULL;

uint8_t uart_gps_rx_buff[UART_GPS_RX_MAX] = {0};
uint32_t gps_timeout = 0;
uint32_t g_usart1_Len;
//uint32_t g_usart1_TransferState = TRANSFER_WAIT;
/*
*********************************************************************************************************
*	函 数 名: bsp_InitUart_GPS
*	功能说明: 初始化串口硬件 
*	形    参: baudrate: 波特率
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUart_GPS(uint32_t baudrate)
{
	bsp_InitUsart_GPS_GPIO();
	bsp_InitUsart_GPS_Config(baudrate);
	bsp_InitUsart_GPS_DMA();                     // 使能串口 
}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart5
*	功能说明: 初始化串口硬件 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart_GPS_GPIO(void)
{
	/* enable GPIO clock */
	rcu_periph_clock_enable(USART_GPS_TX_GPIO_CLK);
	rcu_periph_clock_enable(USART_GPS_RX_GPIO_CLK);

	/* configure the USART5 TX pin and USART5 RX pin */
	gpio_af_set(USART_GPS_TX_GPIO_PORT, USART_GPS_TX_PIN_AF, USART_GPS_TX_PIN);
	gpio_af_set(USART_GPS_RX_GPIO_PORT, USART_GPS_RX_PIN_AF, USART_GPS_RX_PIN);

	/* configure USART5 TX as alternate function push-pull */
	gpio_mode_set(USART_GPS_TX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART_GPS_TX_PIN);
	gpio_output_options_set(USART_GPS_TX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART_GPS_TX_PIN);

	/* configure USART5 RX as alternate function push-pull */
	gpio_mode_set(USART_GPS_RX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART_GPS_RX_PIN);
	gpio_output_options_set(USART_GPS_RX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART_GPS_RX_PIN);

}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart5_Config
*	功能说明: 初始化串口硬件 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart_GPS_Config(uint32_t baudrate)
{
	/* enable USART clock */
	rcu_periph_clock_enable(RCU_USART1);
	
	/* USART5 configure */
	usart_deinit(USART1);
    usart_word_length_set(USART1, USART_WL_8BIT);
    usart_stop_bit_set(USART1, USART_STB_1BIT);
    usart_parity_config(USART1, USART_PM_NONE);
    usart_baudrate_set(USART1, baudrate);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
    usart_enable(USART1);
	
	#ifdef USE_USART1_INT
	{
		usart_interrupt_flag_clear(USART1, USART_INT_FLAG_RBNE);
		usart_interrupt_flag_clear(USART1, USART_INT_FLAG_TC);
		usart_interrupt_enable(USART1, USART_INT_RBNE);
		nvic_irq_enable(USART1_IRQn, 7, 0);
	}
	#endif

	#ifdef USE_USART1_IDEL
	{
		usart_interrupt_enable(USART1, USART_INT_IDLE);
		nvic_irq_enable(USART1_IRQn, 7, 0);
	}
	#endif

	#ifdef USE_USART1_TIMEOUT
	{
		/* enable the USART receive timeout and configure the time of timeout */
		usart_interrupt_flag_clear(USART1, USART_INT_FLAG_RT);
		/*向寄存器填入需要超时的长度，单位为一个波特时长,3.5个字节*11波特长度 = 39  */
		usart_receiver_timeout_threshold_config(USART1, 39);
		usart_receiver_timeout_enable(USART1);
		usart_interrupt_enable(USART1, USART_INT_RT);
		nvic_irq_enable(USART1_IRQn, 7, 0);
	}
	#endif
}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitUsart5_DMA
*	功能说明: 初始化串口DMA硬件 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUsart_GPS_DMA(void)
{
	/* 配置TX DMA和NVIC */
	#ifdef USE_USART1_TX_DMA
	{
		dma_single_data_parameter_struct dma_tx_struct;
		
		/* enable DMA1 */
		rcu_periph_clock_enable(USART_GPS_DMA_CLK);

		/* deinitialize DMA USART5 TX */
		dma_deinit(USART_GPS_DMAx, USART_GPS_TX_DMA_CHANNEL);
		dma_tx_struct.periph_addr         = (uint32_t)&USART_DATA(USART1);
		dma_tx_struct.memory0_addr        = (uint32_t)g_usart1_tx_buff;
		dma_tx_struct.direction           = DMA_MEMORY_TO_PERIPH;
		dma_tx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
		dma_tx_struct.priority            = DMA_PRIORITY_MEDIUM;
		dma_tx_struct.number              = UART_GPS_RX_MAX;
		dma_tx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
		dma_tx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
		dma_tx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
		dma_single_data_mode_init(USART_GPS_DMAx, USART_GPS_TX_DMA_CHANNEL, &dma_tx_struct);
		dma_channel_subperipheral_select(USART_GPS_DMAx, USART_GPS_TX_DMA_CHANNEL, USART_GPS_TX_DMA_PERIEN);
		
		/* USART DMA enable for transmission and reception */
		usart_dma_transmit_config(USART1, USART_TRANSMIT_DMA_ENABLE);	
		
		/* enable DMA1 channel7 transfer complete interrupt */
		dma_interrupt_flag_clear(USART_GPS_DMAx, USART_GPS_TX_DMA_CHANNEL, DMA_INT_FLAG_FTF);
		dma_interrupt_enable(USART_GPS_DMAx, USART_GPS_TX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
		nvic_irq_enable(USART_GPS_DMA_TX_IRQn, 7, 0);

	}
	#endif
	/* 配置RX DMA和NVIC */
	#if defined(USE_USART1_IDEL) || defined(USE_USART1_TIMEOUT)
	{
		dma_single_data_parameter_struct dma_rx_struct;

		/* enable DMA1 */
		rcu_periph_clock_enable(USART_GPS_DMA_CLK);

		/* deinitialize DMA USART5 RX */
		dma_deinit(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL);
		dma_rx_struct.periph_addr         = (uint32_t)&USART_DATA(USART1);
		dma_rx_struct.memory0_addr        = (uint32_t)uart_gps_rx_buff;
		dma_rx_struct.direction           = DMA_PERIPH_TO_MEMORY;
		dma_rx_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
		dma_rx_struct.priority            = DMA_PRIORITY_MEDIUM;
		dma_rx_struct.number              = UART_GPS_RX_MAX;
		dma_rx_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
		dma_rx_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
		dma_rx_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
		dma_single_data_mode_init(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL, &dma_rx_struct);
		dma_channel_subperipheral_select(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL, USART_GPS_RX_DMA_PERIEN);
		
		/* USART DMA enable for transmission and reception */
		usart_dma_receive_config(USART1, USART_RECEIVE_DMA_ENABLE);	

		/* enable DMA1 channel2 transfer complete interrupt */
		// dma_interrupt_enable(USART5_DMAx, USART5_RX_DMA_CHANNEL, DMA_CHXCTL_FTFIE);
		// nvic_irq_enable(USART5_DMA_RX_IRQn, 7, 0);

		/* enable DMA1 channel2 */
		dma_channel_enable(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL);
	}
	#endif
}

/*
*********************************************************************************************************
*	函 数 名: uart_gps_send_char
*	功能说明: 向串口发送1个字节。
*	形    参: 
*	@ch			: 待发送的字节数据
*	返 回 值: 无
*********************************************************************************************************
*/
void uart_gps_send_char(uint8_t ch)
{
	usart_data_transmit(USART1, (uint8_t)ch);
	while (RESET == usart_flag_get(USART1, USART_FLAG_TBE));
}

/*
*********************************************************************************************************
*	函 数 名: uart_gps_send_str
*	功能说明: 向串口发送字符串。
*	形    参:  
*	@buff		: 字符串指针
*	@len		: 发送数据长度
*	返 回 值: 无
*********************************************************************************************************
*/
void uart_gps_send_str(uint8_t *buff, uint16_t len)
{
	while(len--) {
		uart_gps_send_char(buff[0]);
		buff++;
	}
}

/*
*********************************************************************************************************
*	函 数 名: UART_GPS_IRQHandler
*	功能说明: 供中断服务程序调用，通用串口中断处理函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void USART1_IRQHandler(void)
{
	
		#ifdef USE_USART1_INT
	{
		static uint16_t rxcount = 0;
		static uint16_t txcount = 0;
		if((RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_RBNE)) &&
			(RESET != usart_flag_get(USART1, USART_FLAG_RBNE))) 
		{
			/* receive data */
			uart_gps_rx_buff[rxcount++] = usart_data_receive(USART1);
			if(rxcount == 10) 
			{
				rxcount = 0;
				g_usart1_TransferState = TRANSFER_RX_COMPLETE;
				// usart_interrupt_disable(USART5, USART_INT_RBNE);
			}
		}
		if((RESET != usart_flag_get(USART1, USART_FLAG_TBE)) &&
			(RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_TBE))) 
		{
			/* transmit data */
			usart_data_transmit(USART1, g_usart1_tx_buff[txcount++]);
			if(txcount == g_usart1_Len)
			{
				txcount = 0;
				g_usart1_TransferState = TRANSFER_TX_COMPLETE;
				usart_interrupt_disable(USART1, USART_INT_TBE);
			}
		}
	}
	#endif

	#ifdef USE_USART1_IDEL
	{
		if(RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_IDLE))
		{
			/* clear IDLE flag */
			USART_STAT0(USART1);   // 第一步：读状态寄存器
			usart_data_receive(USART1);
			
			/* number of data received */
			g_usart1_Len = UART_GPS_RX_MAX - (dma_transfer_number_get(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL));
//			g_usart1_TransferState = TRANSFER_RX_COMPLETE;
			
			/* disable DMA and reconfigure */
			dma_channel_disable(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL);
			dma_flag_clear(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL, DMA_FLAG_FTF);
			
			sg_atgm336h_param_t.status = 1;
		  gps_timeout = 0;
			
			dma_transfer_number_config(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL, UART_GPS_RX_MAX);
			dma_channel_enable(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL);
		}
    }
	#endif

	#ifdef USE_USART1_TIMEOUT
	{
		if(RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_RT))
		{
			usart_interrupt_flag_clear(USART1, USART_INT_FLAG_RT);
			
			/* number of data received */
			g_usart1_Len = UART_GPS_RX_MAX - (dma_transfer_number_get(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL));
			g_usart1_TransferState = TRANSFER_RX_COMPLETE;
			
			/* disable DMA and reconfigure */
			dma_channel_disable(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL);
			dma_flag_clear(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL, DMA_FLAG_FTF);
			dma_transfer_number_config(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL, UART_GPS_RX_MAX);
			dma_channel_enable(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL);
		}
	}
	#endif
		uint8_t res = 0;
	static uint8_t test = 0;

	if((RESET != usart_flag_get(USART1, USART_FLAG_ORERR)) ||
		(RESET != usart_flag_get(USART1, USART_FLAG_FERR)) ||
		(RESET != usart_flag_get(USART1, USART_FLAG_PERR)))
	{
		USART_STAT0(USART1);
		res = usart_data_receive(USART1);
//		uart_gps_rx_buff[test++] = res;
	}
	
	
//#ifdef UART_GPS_RX_DMA
//	uint16_t size = 0;
//	size = size;
//	if (USART_GetITStatus(UART_GPS, USART_IT_IDLE) != RESET) 
//	{
//		USART_ClearITPendingBit(UART_GPS, USART_IT_IDLE);
//		USART_ReceiveData(UART_GPS);
//		
//		DMA_Cmd(UART_GPS_RX_DMA_STREAM, DISABLE);/* 停止DMA */
//		size = UART_GPS_RX_MAX - DMA_GetCurrDataCounter(UART_GPS_RX_DMA_STREAM);
//        sg_atgm336h_param_t.status = 1;
//		gps_timeout = 0;
//		DMA_Enable(UART_GPS_RX_DMA_STREAM,UART_GPS_RX_MAX);/* 设置传输模式 */
//	}
//#else
//	static uint8_t test = 0;
//	uint8_t res = 0;
//	
//	if (USART_GetITStatus(UART_GPS, USART_IT_RXNE) != RESET) {
//		USART_ClearITPendingBit(UART_GPS, USART_IT_RXNE);
//		res = USART_ReceiveData(UART_GPS);
////		printf("%c",res);
//		uart_gps_rx_buff[test++] = res;
//	}
//#endif
}

/*
*********************************************************************************************************
*	函 数 名: atgm336h_decode_nmea_xxgga
*	功能说明: 解析$XXGGA类型的NMEA消息
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t atgm336h_decode_nmea_xxgga(void)
{
	if(gps_timeout == 0)
		gps_timeout = HAL_GetTick();
	else
	{	
		if(HAL_GetTick() - gps_timeout > 3000)
		{
            printf("gps_timeout!!!\n");
//			DMA_Cmd(UART_GPS_RX_DMA_STREAM, DISABLE);/* 停止DMA */
//			uint16_t gps_len = UART_GPS_RX_MAX - DMA_GetCurrDataCounter(UART_GPS_RX_DMA_STREAM);
//			sg_atgm336h_param_t.status = 1;
//			DMA_Enable(UART_GPS_RX_DMA_STREAM,UART_GPS_RX_MAX);/* 设置传输模式 */
			dma_channel_disable(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL);
			dma_flag_clear(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL, DMA_FLAG_FTF);
			g_usart1_Len = UART_GPS_RX_MAX - (dma_transfer_number_get(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL));
			
			sg_atgm336h_param_t.status = 1;
			dma_transfer_number_config(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL, UART_GPS_RX_MAX);
			dma_channel_enable(USART_GPS_DMAx, USART_GPS_RX_DMA_CHANNEL);			
		}
	}
	
	if(sg_atgm336h_param_t.status == 1)
	{
		gps_timeout = 0;
		sg_atgm336h_param_t.status = 0;
		sg_atgm336h_param_t.is_valid = 1;
		
		// Find GNGGA sentence (GNSS combined positioning data including GPS and BDS)
		char *gga_sentence = strstr((char*)uart_gps_rx_buff, "$GNGGA");
		if (gga_sentence == NULL) {
			// If GNGGA not found, try to find GPGGA
			gga_sentence = strstr((char*)uart_gps_rx_buff, "$GPGGA");
			if (gga_sentence == NULL) {
				// If still not found, try to find BDGA (BDS-only GGA)
				gga_sentence = strstr((char*)uart_gps_rx_buff, "$BDGGA");
				if (gga_sentence == NULL) {
					return 1;
				}
			}
		}
		// The rest of the parsing logic remains the same for all GGA variants
		// Split each field of GGA sentence
		char *token = strtok(gga_sentence, ",");
		uint8_t field_index = 0;
		char latitude_str[20] = {0};
		char longitude_str[20] = {0};
		char altitude_str[20] = {0};
		char hdop_str[20] = {0};
		char satellites_str[5] = {0};
		char fix_quality_str[5] = {0};
		
		while (token != NULL && field_index <= 10) {
			switch (field_index) {
			case 1: // Time (HHMMSS format)
				break;
			case 2: // Latitude (ddmm.mmmm format)
				strncpy(latitude_str, token, sizeof(latitude_str) - 1);
				break;
			case 3: // Latitude direction (N/S)
				sg_atgm336h_param_t.lat_dir = token[0];
				break;
			case 4: // Longitude (dddmm.mmmm format)
				strncpy(longitude_str, token, sizeof(longitude_str) - 1);
				break;
			case 5: // Longitude direction (E/W)
				sg_atgm336h_param_t.lon_dir = token[0];
				break;
			case 6: // Fix quality indicator
				strncpy(fix_quality_str, token, sizeof(fix_quality_str) - 1);
				break;
			case 7: // Number of satellites in use
				strncpy(satellites_str, token, sizeof(satellites_str) - 1);
				break;
			case 8: // Horizontal dilution of precision
				strncpy(hdop_str, token, sizeof(hdop_str) - 1);
				break;
			case 9: // Altitude
				strncpy(altitude_str, token, sizeof(altitude_str) - 1);
				break;
			case 10:  
				break;
			}
			token = strtok(NULL, ",");
			field_index++;
		}
		
		// Convert fix quality
		if (strlen(fix_quality_str) > 0) {
			sg_atgm336h_param_t.fix_quality = atoi(fix_quality_str);
		}
		
		// Convert number of satellites
		if (strlen(satellites_str) > 0) {
			sg_atgm336h_param_t.num_satellites = atoi(satellites_str);
		}
		
		// Convert horizontal dilution of precision
		if (strlen(hdop_str) > 0) {
			sg_atgm336h_param_t.hdop = atof(hdop_str);
		}
		
		// Convert altitude
		if (strlen(altitude_str) > 0) {
			sg_atgm336h_param_t.altitude = atof(altitude_str);
		}
		
		// Convert latitude (from ddmm.mmmm format to decimal format)
		if (strlen(latitude_str) > 0) {
			char deg_str[4] = {0};
			char min_str[10] = {0};
			int deg = 0;
			double min = 0.0;
			
			// For latitude, first two digits are degrees, the rest are minutes
			strncpy(deg_str, latitude_str, 2);
			deg = atoi(deg_str);
			strcpy(min_str, latitude_str + 2);
			min = atof(min_str);
			
			// Convert to decimal format
			sg_atgm336h_param_t.latitude = deg + min / 60.0;
			
			// Adjust sign according to direction
			if (sg_atgm336h_param_t.lat_dir == 'S') {
				sg_atgm336h_param_t.latitude = -sg_atgm336h_param_t.latitude;
			}
		}
		
		// Convert longitude (from dddmm.mmmm format to decimal format)
		if (strlen(longitude_str) > 0) {
			char deg_str[5] = {0};
			char min_str[10] = {0};
			int deg = 0;
			double min = 0.0;
			
			// For longitude, first three digits are degrees, the rest are minutes
			strncpy(deg_str, longitude_str, 3);
			deg = atoi(deg_str);
			strcpy(min_str, longitude_str + 3);
			min = atof(min_str);
			
			// Convert to decimal format
			sg_atgm336h_param_t.longitude = deg + min / 60.0;
			
			// Adjust sign according to direction
			if (sg_atgm336h_param_t.lon_dir == 'W') {
				sg_atgm336h_param_t.longitude = -sg_atgm336h_param_t.longitude;
			}
		}
		
		// Only consider data valid when fix quality is greater than 0
		if (sg_atgm336h_param_t.fix_quality > 0) {
			sg_atgm336h_param_t.is_valid = 0;
		}
		memset(uart_gps_rx_buff,0,UART_GPS_RX_MAX);
		return sg_atgm336h_param_t.is_valid;
	}
	else
	{
		return 2;
	}
}

/*
*********************************************************************************************************
*	函 数 名: atgm336h_decode_nmea_xxgga
*	功能说明: 解析$XXGGA类型的NMEA消息
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
atgm336h_data_t* atgm336h_get_gnss_data(void)
{
	return &sg_atgm336h_param_t;
}


/*
*********************************************************************************************************
*	函 数 名: ATGM338H_test
*	功能说明: 测试
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void ATGM338H_test(void)
{
	while(1)
	{
		atgm336h_decode_nmea_xxgga();
		
		if (sg_atgm336h_param_t.is_valid == 0) {

			printf("\n=== GNSS Data Parsing Result ===\n");
			printf("Fix Status: Valid\n");
			printf("Fix Quality: %d (0=Invalid, 1=GPS Fix, 2=DGPS Fix, 3=PPS Fix, 4=RTK, 5=Float RTK)\n", sg_atgm336h_param_t.fix_quality);
			printf("Number of Satellites: %d\n", sg_atgm336h_param_t.num_satellites);
			printf("Horizontal Dilution of Precision: %.2f\n", sg_atgm336h_param_t.hdop);
			printf("Latitude: %.10f %c\n", fabs(sg_atgm336h_param_t.latitude), sg_atgm336h_param_t.lat_dir);
			printf("Longitude: %.10f %c\n", fabs(sg_atgm336h_param_t.longitude), sg_atgm336h_param_t.lon_dir);
			printf("Decimal Latitude: %.10f\n", sg_atgm336h_param_t.latitude);
			printf("Decimal Longitude: %.10f\n", sg_atgm336h_param_t.longitude);
			printf("Altitude: %.10f\n", sg_atgm336h_param_t.altitude);
			printf("==============================\n\n");

		} else {

			printf("\n=== GNSS Data Parsing Result ===\n");
			printf("Fix Status: Invalid\n");
			printf("No valid positioning data found. Please ensure the device is connected to satellites and working properly\n");
			printf("==============================\n\n");

		}
		delay_ms(1000);
	}
}










