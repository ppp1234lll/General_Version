#include "./USER/inc/start.h"
#include "main.h"

ChipID_t g_chipid_t;  // 芯片ID

/*
*********************************************************************************************************
*    函 数 名: start_bsp_init
*    功能说明: 初始化所有的硬件设备
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void start_bsp_init(void)
{
    cJSON_Hooks hook;                // 初始化JSON 

    bsp_InitFwdgt(FWDGT_PSC_DIV64,1000);    // 初始化看门狗(硬件、软件2s)
    start_get_device_id();              // 获取本机ID
    my_mem_init(SRAMIN);              // 内存初始化
    
    hook.malloc_fn = mymalloc_sramin;// 内存分配
    hook.free_fn   = myfree_sramin;  // 内存释放
    cJSON_InitHooks(&hook);          // 初始化自定义的内存分配和释放函数

    bsp_Init_DWT();                  // 初始化DWT
    bsp_InitLed();                   // LED初始化（已测试）
    bsp_InitRelay();                 // 继电器初始化（未测试）    
    bsp_InitRTC();                   // RTC初始化 (已测试)  
#if ( configUSE_EXT_FAN == 1 )
    bsp_InitFan();                   // 风扇初始化(已测试)    
#endif
#if (configUSE_EXT_GPS == 1)
    bsp_InitUart_GPS(9600);          // GPS初始化（已测试）
#endif
#if (configUSE_LN_PGND == 1)
    bsp_InitADC();                    // ADC初始化（已测试）
#endif

    bsp_InitTimers(TIMER1, 1000, 6, 0);
    bsp_InitTimers(TIMER2, 1000, 6, 0);
    bsp_InitTimers(TIMER5, 1, 6, 0);
    bsp_InitTimers(TIMER6, 1, 6, 0);

    ebtn_APP_Keys_Init();            // 初始化按键模块（已测试）
    
#if (configUSE_RS485 == 1)
    RS485_Init(115200);          // 485初始化（已测试）
#endif
#if (configUSE_AHT20 == 1)
    aht20_init();             // 温湿度初始化(已测试)   
#endif

#if (configUSE_TILT == 1)
    hal_lis3dh_init(true);   // 陀螺仪初始化 IIC (已测试)
#endif
    bl0939_init();           // 电能检测初始化
    bl0910_init();
    
    bsp_InitSPIBus();
    bsp_InitSFlash();

    FeedFwdgt();
}

/*
*********************************************************************************************************
*    函 数 名: start_get_device_id
*    功能说明: 获取本机ID
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void start_get_device_id(void)
{
    volatile uint32_t addr = 0X1FFF7A10;
    
    g_chipid_t.id[0] = *(__I uint32_t *)(addr + 0x00);
    g_chipid_t.id[1] = *(__I uint32_t *)(addr + 0x04);
    g_chipid_t.id[2] = *(__I uint32_t *)(addr + 0x08);
}

/*
*********************************************************************************************************
*    函 数 名: start_get_device_id_hex
*    功能说明: 获取本机ID,十六进制表示
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void start_get_device_id_hex(uint32_t *id)
{
    id[0] = g_chipid_t.id[0];
    id[1] = g_chipid_t.id[1];
    id[2] = g_chipid_t.id[2];
}

/*
*********************************************************************************************************
*    函 数 名: start_get_device_id_str
*    功能说明: 获取本机ID,字符串表示
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void start_get_device_id_str(uint8_t *str)
{
    sprintf((char*)str,"%04X%04X%04X",g_chipid_t.id[0],g_chipid_t.id[1],g_chipid_t.id[2]);
}

/******************************************************************************************************/
/*FreeRTOS配置*/

/* START_TASK 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define START_TASK_PRIO                 2        
#define START_STK_SIZE                  512      
TaskHandle_t StartTask_Handler;          
void start_task(void *pvParameters);     

/* ALARM线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define ALARM_TASK_PRIO                 17       
#define ALARM_STK_SIZE                  1024      
TaskHandle_t ALARM_Task_Handler;     
void alarm_task(void *pvParameters); 

/* APP线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define APP_TASK_PRIO                   16       
#define APP_STK_SIZE                    4096      
TaskHandle_t APP_Task_Handler;     
void app_task(void *pvParameters); 

/* ETH线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define ETH_TASK_PRIO                   13      
#define ETH_STK_SIZE                    1024     
TaskHandle_t ETH_Task_Handler;          
void eth_task(void *pvParameters);      

/* 检测线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define DET_TASK_PRIO                   12      
#define DET_STK_SIZE                    4096     
TaskHandle_t DET_Task_Handler;          
void det_task(void *pvParameters);      

/* GSM线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define GSM_TASK_PRIO                   14      
#define GSM_STK_SIZE                    1024     
TaskHandle_t GSM_Task_Handler;          
void gsm_task(void *pvParameters);      

/* 打印线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define PRINT_TASK_PRIO           2      
#define PRINT_STK_SIZE            512    
TaskHandle_t PRINT_Task_Handler;         
void print_task(void *pvParameters);     

/* DTU线程 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define DTU_TASK_PRIO     			7       
#define DTU_STK_SIZE      			1024      
TaskHandle_t DTU_Task_Handler;     
void dtu_task(void *pvParameters); 

/******************************************************************************************************/


/*
*********************************************************************************************************
*    函 数 名: start_task_create
*    功能说明: 创建所有的任务
*    形    参: 无
*    返 回 值: 无
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
*    函 数 名: start_task
*    功能说明: 
*    形    参: pvParameters : 传入参数(未用到)
*    返 回 值: 无
*********************************************************************************************************
*/
void start_task(void *pvParameters)
{
    pvParameters = pvParameters;
    
    save_init_function();    
    com_recevie_function_init();        // 初始化接收缓冲区
    app_get_storage_param_function();    // 获取本地存储的数据
    
    log_init_function();
    
    /* 有线网络初始化: 硬件 + lwIP协议栈 */
    if (enet_system_setup() != 0)
    {
        /* 硬件初始化成功, 尝试lwIP协议栈初始化 (最多重试5次, 每次间隔1秒) */
        int lwip_retry = 0;
        while (lwip_comm_init() != 0)
        {
            printf("lwIP Init failed!!\n");
            if (++lwip_retry >= 5)
            {
                printf("lwIP Init failed after 5 retries, skip ethernet!\r\n");
                break;
            }
            delay_ms(1000);
        }
    }
    else
    {
        printf("Ethernet hardware init failed, system running in wireless-only mode!\r\n");
    }
    
    printf("run here!!\n");
    FeedFwdgt();  
    taskENTER_CRITICAL();           /* 进入临界区 */

    /* 初始化发送模块（内部创建消息池、队列和发送任务） */
    send_task_init();

    xTaskCreate((TaskFunction_t )alarm_task,
                            (const char *   )"alarm_task",
                            (uint16_t       )ALARM_STK_SIZE,
                            (void *         )NULL,
                            (UBaseType_t    )ALARM_TASK_PRIO,
                            (TaskHandle_t * )&ALARM_Task_Handler);

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

    xTaskCreate((TaskFunction_t )dtu_task,
                            (const char *   )"dtu_task",
                            (uint16_t       )DTU_STK_SIZE,
                            (void *         )NULL,
                            (UBaseType_t    )DTU_TASK_PRIO,
                            (TaskHandle_t * )&DTU_Task_Handler);

//    freertos_demo();        
    FeedFwdgt();  
    printf("Free heap: %d bytes\n", xPortGetFreeHeapSize());            /*打印剩余堆栈大小*/
    taskEXIT_CRITICAL();            /* 退出临界区 */
    vTaskDelete(StartTask_Handler); /* 删除开始任务 */
}

/*
*********************************************************************************************************
*    函 数 名: alarm_task
*    功能说明: 故障检测任务
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void alarm_task(void *pvParameters)
{
//    while(1){
//        vTaskDelay(500);
//    }
    alarm_task_function();
}

/*
*********************************************************************************************************
*    函 数 名: app_task
*    功能说明: 主任务
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void app_task(void *pvParameters)
{
//    while(1){
//        vTaskDelay(500);
//    }
    app_task_function();
}

/*
*********************************************************************************************************
*    函 数 名: eth_task
*    功能说明: 网口检测任务
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void eth_task(void *pvParameters)
{
//    while(1){
//        vTaskDelay(500);
//    }
    eth_task_function();
}

/*
*********************************************************************************************************
*    函 数 名: det_task
*    功能说明: 检测任务
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void det_task(void *pvParameters)
{
//    while(1){

//        vTaskDelay(500);
//    }
    det_task_function();
}

/*
*********************************************************************************************************
*    函 数 名: gsm_task
*    功能说明: 无线通信任务
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void gsm_task(void *pvParameters)
{
//    while(1){

//        vTaskDelay(500);
//    }
    gsm_task_function();
}   

/*
*********************************************************************************************************
*    函 数 名: print_task
*    功能说明: 打印任务
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void print_task(void *pvParameters)
{
//    while(1){
//        vTaskDelay(500);
//    }    
    print_task_function();
}   
/*
*********************************************************************************************************
*    函 数 名: dtu_task
*    功能说明: 透传任务
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void dtu_task(void *pvParameters)
{
//    while(1){
//        vTaskDelay(500);
//    }    
    dtu_task_function();
}     
/******************************************  (END OF FILE) **********************************************/
