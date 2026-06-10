
#include "appconfig.h"
#include "./USER/inc/start.h"

// 芯片ID
ChipID_t g_chipid_t;

/*
*********************************************************************************************************
*	函 数 名:  start_bsp_init
*	功能说明:  初始化所有的硬件设备
*	形    参:  无
*	返 回 值:  无
*********************************************************************************************************
*/
void start_bsp_init(void)
{
	 cJSON_Hooks hook;                // 初始化JSON 

	bsp_InitFwdgt(FWDGT_PSC_DIV64,1000);	// 初始化看门狗(硬件、软件2s)
	start_get_device_id_function();  // 获取本机ID
	my_mem_init(SRAMIN);              // 内存初始化
	
	 hook.malloc_fn = mymalloc_sramin;// 内存分配
	 hook.free_fn   = myfree_sramin;  // 内存释放
	 cJSON_InitHooks(&hook);          // 初始化自定义的内存分配和释放函数
//	bsp_InitTimers(TIMER1, 1000, 6, 0);
	bsp_Init_DWT();                 // 初始化DWT
	bsp_InitLed();                   // LED初始化（已测试）
	bsp_InitRelay();                // 继电器初始化（未测试）	
	bsp_InitKey();
  ebtn_APP_Keys_Init();            // 初始化按键模块（已测试）
	bsp_InitFan();                   // 风扇初始化(已测试)	
	bsp_InitRTC();                  // RTC初始化 (已测试)
	bsp_InitRS485(115200); 	
	bsp_InitUart_GPS(9600);
  adc_config();
//	adc_test();
  bsp_InitTimers(TIMER1, 1000, 6, 0);
  bsp_InitTimers(TIMER2, 1000, 6, 0);
//    bsp_InitTimers(TIMER3, 1000, 6, 0);
//	bsp_InitTimers(TIMER4, 1000, 6, 0);
	bsp_InitTimers(TIMER5, 1, 6, 0);
	bsp_InitTimers(TIMER6, 1, 6, 0);

	// TIM3_Int_Init(1000-1,90-1);       // 定时器3初始化 1KHz(已测试)
	// TIM6_Int_Init(10000-1,9000-1);    // 定时器6初始化 1Hz(已测试)
	// TIM4_Int_Init(10000-1,9000-1);    // 定时器4初始化 1Hz(已测试)	
    
	 hal_lis3dh_init(true);           // 陀螺仪初始化 IIC (已测试)
 //  lis3dh_test();
	 aht20_init_function();			 // 温湿度初始化(已测试)	
	 bl0939_init_function();           // 电能检测初始化(失败)
//   bl0939_test();
	// W25QXX_Init();                      // 初始化spiflash
	bsp_InitSPIBus();
	bsp_InitSFlash();
	enet_system_setup();

	 save_init_function();	
	 com_recevie_function_init();        // 初始化接收缓冲区
	 app_get_storage_param_function();	// 获取本地存储的数据
	 update_status_init();               // 更新检测

	
//	rtc_test();
	FeedFwdgt();
}

/*
*********************************************************************************************************
*	函 数 名:  start_get_device_id
*	功能说明:  获取本机ID
*	形    参:  无
*	返 回 值:  无
*	STM2F1_UUID_ADDR  0X1FFFF7E8   // 任意的一个数
	STM2F3_UUID_ADDR  0X1FFFF7AC   // 任意的一个数
	STM2F4_UUID_ADDR  0X1FFF7A10   // 任意的一个数
	STM2F7_UUID_ADDR  0X1FF0F420   // 任意的一个数
*********************************************************************************************************
*/
void start_get_device_id_function(void)
{
	volatile uint32_t addr = 0X1FFF7A10;
	
	g_chipid_t.id[0] = *(__I uint32_t *)(addr + 0x00);
	g_chipid_t.id[1] = *(__I uint32_t *)(addr + 0x04);
	g_chipid_t.id[2] = *(__I uint32_t *)(addr + 0x08);
}

/************************************************************
*
* Function name	: start_get_device_id_str
* Description	: 获取本机ID
* Parameter		: 
* Return		: 
*	
************************************************************/
void start_get_device_id(uint32_t *id)
{
	id[0] = g_chipid_t.id[0];
	id[1] = g_chipid_t.id[1];
	id[2] = g_chipid_t.id[2];
}

/*
*********************************************************************************************************
*	函 数 名:  start_get_device_id_str
*	功能说明:  获取本机ID,字符串表示
*	形    参:  无
*	返 回 值:  无
*********************************************************************************************************
*/
void start_get_device_id_str(uint8_t *str)
{
	sprintf((char*)str,"%04X%04X%04X",g_chipid_t.id[0],g_chipid_t.id[1],g_chipid_t.id[2]);
}
/*
*********************************************************************************************************
*	函 数 名:  start_get_device_id_hex
*	功能说明:  获取本机ID,十六进制表示
*	形    参:  无
*	返 回 值:  无
*********************************************************************************************************
*/
void start_get_device_id_hex(uint32_t *id)
{
	id[0] = g_chipid_t.id[0];
	id[1] = g_chipid_t.id[1];
	id[2] = g_chipid_t.id[2];
}


/******************************************************************************************************/
/*FreeRTOS配置*/

/* START_TASK 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define START_TASK_PRIO         	2        
#define START_STK_SIZE          	512      
TaskHandle_t StartTask_Handler;          
void start_task(void *pvParameters);     

/* ALARM线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define ALARM_TASK_PRIO     			10       
#define ALARM_STK_SIZE      			1024      
TaskHandle_t ALARM_Task_Handler;     
void alarm_task(void *pvParameters); 

/* APP线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define APP_TASK_PRIO     				9       
#define APP_STK_SIZE      				4096      
TaskHandle_t APP_Task_Handler;     
void app_task(void *pvParameters); 

/* ETH线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define ETH_TASK_PRIO           	8      
#define ETH_STK_SIZE            	1024     
TaskHandle_t ETH_Task_Handler;          
void eth_task(void *pvParameters);      

/* 检测线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define DET_TASK_PRIO           	10      
#define DET_STK_SIZE            	4096     
TaskHandle_t DET_Task_Handler;          
void det_task(void *pvParameters);      

/* GSM线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define GSM_TASK_PRIO           	8      
#define GSM_STK_SIZE            	1024     
TaskHandle_t GSM_Task_Handler;          
void gsm_task(void *pvParameters);      

/* 打印线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define PRINT_TASK_PRIO           2      
#define PRINT_STK_SIZE            512    
TaskHandle_t PRINT_Task_Handler;         
void print_task(void *pvParameters);     

/******************************************************************************************************/

/*
*********************************************************************************************************
*	函 数 名:  start_task_create
*	功能说明:  创建所有的任务
*	形    参:  无
*	返 回 值:  无
*********************************************************************************************************
*/
void start_task_create(void)
{
	/* start_task任务 */
	xTaskCreate((TaskFunction_t )start_task,
							(const char *   )"start_task",
							(uint16_t       )START_STK_SIZE,
							(void *         )NULL,
							(UBaseType_t    )START_TASK_PRIO,
							(TaskHandle_t * )&StartTask_Handler);

	vTaskStartScheduler(); /* 开启任务调度 */
}

/*
*********************************************************************************************************
*	函 数 名:  start_task
*	功能说明:   
*	形    参:  pvParameters : 传入参数(未用到)
*	返 回 值:  无
*********************************************************************************************************
*/
void start_task(void *pvParameters)
{
	pvParameters = pvParameters;

	// save_init_function();
	// com_recevie_function_init();			// 初始化接收缓冲区
	// app_get_storage_param_function();	// 获取本地存储的数据
	// app_get_update_result_function();	// 获取更新结果
	// // update_status_init();	// 更新检测

	// if (lwip_comm_init() != 0)
	// {
	// 	printf("lwIP Init failed!!\n");
	// 	delay_ms(500);
	// 	printf("Retrying...       \n");
	// 	delay_ms(500);
	// }
	// printf("run here!!\n");
	// iwdg_feed();  
	// taskENTER_CRITICAL();           /* 进入临界区 */

//	xTaskCreate((TaskFunction_t )alarm_task,
//							(const char *   )"alarm_task",
//							(uint16_t       )ALARM_STK_SIZE,
//							(void *         )NULL,
//							(UBaseType_t    )ALARM_TASK_PRIO,
//							(TaskHandle_t * )&ALARM_Task_Handler);

	xTaskCreate((TaskFunction_t )det_task,
							(const char *   )"det_task",
							(uint16_t       )DET_STK_SIZE,
							(void *         )NULL,
							(UBaseType_t    )DET_TASK_PRIO,
							(TaskHandle_t * )&DET_Task_Handler);
							
	xTaskCreate((TaskFunction_t )app_task,
							(const char *   )"app_task",
							(uint16_t       )APP_STK_SIZE,
							(void *         )NULL,
							(UBaseType_t    )APP_TASK_PRIO,
							(TaskHandle_t * )&APP_Task_Handler);
	
	xTaskCreate((TaskFunction_t )eth_task,
							(const char *   )"eth_task",
							(uint16_t       )ETH_STK_SIZE,
							(void *         )NULL,
							(UBaseType_t    )ETH_TASK_PRIO,
							(TaskHandle_t * )&ETH_Task_Handler);

//	xTaskCreate((TaskFunction_t )det_task,
//							(const char *   )"det_task",
//							(uint16_t       )DET_STK_SIZE,
//							(void *         )NULL,
//							(UBaseType_t    )DET_TASK_PRIO,
//							(TaskHandle_t * )&DET_Task_Handler);

	xTaskCreate((TaskFunction_t )gsm_task,
							(const char *   )"gsm_task",
							(uint16_t       )GSM_STK_SIZE,
							(void *         )NULL,
							(UBaseType_t    )GSM_TASK_PRIO,
							(TaskHandle_t * )&GSM_Task_Handler);

	xTaskCreate((TaskFunction_t )print_task,
							(const char *   )"print_task",
							(uint16_t       )PRINT_STK_SIZE,
							(void *         )NULL,
							(UBaseType_t    )PRINT_TASK_PRIO,
							(TaskHandle_t * )&PRINT_Task_Handler);

//	freertos_demo();		
	FeedFwdgt();  
	printf("Free heap: %d bytes\n", xPortGetFreeHeapSize());			/*打印剩余堆栈大小*/
	vTaskDelete(StartTask_Handler); /* 删除开始任务 */
	taskEXIT_CRITICAL();            /* 退出临界区 */					 
}

/*
*********************************************************************************************************
*	函 数 名: alarm_task
*	功能说明: 故障检测任务
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void alarm_task(void *pvParameters)
{
	while(1){

		vTaskDelay(500);
	}
	// alarm_task_function();
}

/*
*********************************************************************************************************
*	函 数 名: app_task
*	功能说明: 主任务
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void app_task(void *pvParameters)
{
//	while(1){

//		vTaskDelay(500);
//	}
	 app_task_function();
}

/*
*********************************************************************************************************
*	函 数 名: eth_task
*	功能说明: 网口检测任务:一直对网口进行轮询
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void eth_task(void *pvParameters)
{
//	while(1){

//		vTaskDelay(500);
//	}
//	 eth_task_function();
	eth_network_line_status_detection_function();
}

/*
*********************************************************************************************************
*	函 数 名: det_task
*	功能说明: 检测任务
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void det_task(void *pvParameters)
{
//	while(1){

//		vTaskDelay(500);
//	}
	 det_task_function();
}

/*
*********************************************************************************************************
*	函 数 名: gsm_task
*	功能说明: 无线通信任务
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void gsm_task(void *pvParameters)
{
//	while(1){

//		vTaskDelay(500);
//	}
	 gsm_task_function();
}

/*
*********************************************************************************************************
*	函 数 名: print_task
*	功能说明: 打印任务
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void print_task(void *pvParameters)
{
//	while(1){
//		vTaskDelay(500);
//	}	
	 print_task_function();
}	
/******************************************  (END OF FILE) **********************************************/
