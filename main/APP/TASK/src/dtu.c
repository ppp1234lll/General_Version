#include "./Task/inc/dtu.h"
#include "main.h" 

// 全局RS485配置变量
rs485_config_t g_rs485_config = {
    9600,    // 默认波特率
    8,       // 默认数据位
    0,       // 默认无校验
    1        // 默认停止位
};
#define DTU_QUEUE_SIZE 			1
#define DTU_TX_BUFF_SIZE 		64
#define DTU_RX_BUFF_SIZE 		512

// 创建队列，用于接收DTU指令
QueueHandle_t dtu_tx_q = NULL;  // 发送队列句柄
QueueHandle_t dtu_rx_q = NULL;  // 接收队列句柄
QueueHandle_t dtu_mppt_rx_q = NULL;  // MPPT接收队列句柄

// 队列数据缓冲区
uint8_t dtu_tx_buf[DTU_TX_BUFF_SIZE ];
uint8_t dtu_rx_buf[DTU_RX_BUFF_SIZE ];
uint8_t dtu_mppt_rx_buf[DTU_RX_BUFF_SIZE ];

// DTU配置更新信号量
SemaphoreHandle_t dtu_config_sem = NULL;

MPPT_Data_t g_mppt;

#if (configUSE_SUN_POWER == 1)
static void dtu_mppt_send_electricity(void);
static void dtu_mppt_send_02(void);
static void dtu_mppt_send_battery(void);
static void dtu_mppt_send_run_day(void);
static void dtu_mppt_parse_electricity(uint8_t *data, uint8_t len);
static void dtu_mppt_parse_02(uint8_t *data, uint8_t len);
static void dtu_mppt_parse_battery(uint8_t *data, uint8_t len);
static void dtu_mppt_parse_run_day(uint8_t *data, uint8_t len);
static void dtu_mppt_parse_discharge_status(uint32_t reg_value);
static void dtu_mppt_parse_charge_status(uint32_t reg_value);
static void dtu_mppt_parse_battery_status(uint8_t reg_value);
#endif

/*
*********************************************************************************************************
*    函 数 名: dtu_task_function
*    功能说明: 透传任务函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void dtu_task_function(void) 
{
	uint8_t *message_ptr;
#if (configUSE_SUN_POWER == 1)
	uint8_t *rx_ptr;
	static uint8_t  s_mppt_step = 0;
	static TickType_t s_mppt_last_tick = 0;
	static TickType_t s_mppt_timeout = 0;
	TickType_t now_times;
    
#endif
	if (dtu_config_sem == NULL) {
		dtu_config_sem = xSemaphoreCreateBinary();
	}
	if (dtu_tx_q == NULL) {
		dtu_tx_q = xQueueCreate(DTU_QUEUE_SIZE, sizeof(void *));
		configASSERT(dtu_tx_q);
	}
	if (dtu_rx_q == NULL) {
		dtu_rx_q = xQueueCreate(DTU_QUEUE_SIZE, sizeof(void *));
		configASSERT(dtu_rx_q);
	}
	if (dtu_mppt_rx_q == NULL) {
		dtu_mppt_rx_q = xQueueCreate(DTU_QUEUE_SIZE, sizeof(void *));
		configASSERT(dtu_mppt_rx_q);
	}
		
	while(1)
	{
		// 检查DTU配置是否更改
		if (xSemaphoreTake(dtu_config_sem, 0) == pdTRUE) 
		{
			RS485_ReConfig(&g_rs485_config);
		}
		
#if (configUSE_RS485_TRANSPARENT == 1)
		if (xQueueReceive(dtu_tx_q, &message_ptr, 0) == pdTRUE) 
		{
			uint8_t len = message_ptr[0];
			if (len > 0) 
				rs485_send_str(message_ptr + 1, len);
		}
#endif
	// 接收DTU串口数据
#if (configUSE_SUN_POWER == 1)
		{
			now_times = xTaskGetTickCount();

			/* 超时保护：等待响应超过2s则回退到空闲态 */
			if (s_mppt_step >= 2) {
				if ((now_times - s_mppt_timeout) >= pdMS_TO_TICKS(2000)) {
					s_mppt_step = 0;
				}
			}

			/* step 0 空闲态：等待1min间隔 */
			if (s_mppt_step == 0) 
			{
				if ((now_times - s_mppt_last_tick) >= pdMS_TO_TICKS(60000))
					s_mppt_step = 1;
			}

			switch (s_mppt_step) 
			{
				case 1:  /* 发送读取电能参数命令 */
					dtu_mppt_send_electricity();
					s_mppt_timeout = now_times;
					s_mppt_step = 2;
				break;
				case 2:  /* 等待并处理电能参数响应 */
					if (xQueueReceive(dtu_mppt_rx_q, &rx_ptr, 0) == pdTRUE) {
						dtu_mppt_parse_electricity(rx_ptr + 1, rx_ptr[0]);
						dtu_mppt_send_02();
						s_mppt_timeout = now_times;
						s_mppt_step = 3;
					}
				break;
				case 3:  /* 等待并处理02响应 */
					if (xQueueReceive(dtu_mppt_rx_q, &rx_ptr, 0) == pdTRUE) {
						dtu_mppt_parse_02(rx_ptr + 1, rx_ptr[0]);
						dtu_mppt_send_battery();
						s_mppt_timeout = now_times;
						s_mppt_step = 4;
					}
				break;
				case 4:  /* 等待并处理蓄电池响应 */
					if (xQueueReceive(dtu_mppt_rx_q, &rx_ptr, 0) == pdTRUE) {
						dtu_mppt_parse_battery(rx_ptr + 1, rx_ptr[0]);
						dtu_mppt_send_run_day();
						s_mppt_timeout = now_times;
						s_mppt_step = 5;
					}
				break;
				case 5:  /* 等待并处理运行天数响应 */
					if (xQueueReceive(dtu_mppt_rx_q, &rx_ptr, 0) == pdTRUE) {
						dtu_mppt_parse_run_day(rx_ptr + 1, rx_ptr[0]);
						s_mppt_last_tick = now_times;
						s_mppt_step = 0;
					}
				break;
			}
		}
#endif
	}
}

/*
*********************************************************************************************************
*    函 数 名: dtu_get_rs485_config_function
*    功能说明: 获取485配置信息
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void *dtu_get_rs485_config_function(void)
{
	return (&g_rs485_config);
}

/*
*********************************************************************************************************
*    函 数 名: dtu_transmit_enqueue
*    功能说明: 将数据拷贝到发送缓冲区并入队，供外部模块调用，无需直接访问 dtu_tx_buf
*    形    参:  data: 数据指针, len: 数据长度
*    返 回 值: pdTRUE 成功, pdFALSE 队列满
*********************************************************************************************************
*/
BaseType_t dtu_transmit_enqueue(uint8_t *data, uint16_t len)
{
	if (len > DTU_TX_BUFF_SIZE - 1)
		return pdFALSE;

	dtu_tx_buf[0] = len;
	memcpy(dtu_tx_buf + 1, data, len);

	return xQueueSend(dtu_tx_q, &dtu_tx_buf, 0);
}

/*
*********************************************************************************************************
*    函 数 名: dtu_get_receive_data
*    功能说明: 将接收到的数据存入485流缓冲区。
*    形    参:  
*    @data        : 数据指针
*    @len         : 数据长度
*    返 回 值: 无
*********************************************************************************************************
*/
void dtu_get_receive_data(uint8_t *data, uint16_t len)
{
    if (len > DTU_RX_BUFF_SIZE - 1)
        return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

#if (configUSE_RS485_TRANSPARENT == 1)
    dtu_rx_buf[0] = (uint8_t)len;
    memcpy(dtu_rx_buf + 1, data, len);
    xQueueSendFromISR(dtu_rx_q, &dtu_rx_buf, &xHigherPriorityTaskWoken);
#elif (configUSE_SUN_POWER == 1)
    dtu_mppt_rx_buf[0] = (uint8_t)len;
    memcpy(dtu_mppt_rx_buf + 1, data, len);
    xQueueSendFromISR(dtu_mppt_rx_q, &dtu_mppt_rx_buf, &xHigherPriorityTaskWoken);
#endif

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

#if (configUSE_SUN_POWER == 1)
/*
*********************************************************************************************************
*    函 数 名: DTU_MPPT_ReadRegisters
*    功能说明: 构建Modbus读寄存器帧并入队，由任务循环发送
*    形    参:  
*    返 回 值: 无
*********************************************************************************************************
*/
void DTU_MPPT_ReadRegisters(uint8_t slave_id, uint8_t func_code, uint16_t start_addr, uint16_t reg_count)
{
    uint8_t frame[8];
    uint16_t calc_crc;
    
	frame[0] = slave_id;
	frame[1] = func_code;
	frame[2] = (start_addr >> 8) & 0xFF;
	frame[3] = start_addr & 0xFF;
	frame[4] = (reg_count >> 8) & 0xFF;
	frame[5] = reg_count & 0xFF;
	
	calc_crc = usMBCRC16(frame, 6);
	frame[6] = calc_crc & 0xFF;
	frame[7] = (calc_crc >> 8) & 0xFF;
	
	rs485_send_str(frame, 8);
}


// ========== MPPT 命令发送函数 ==========

/*
*********************************************************************************************************
*    函 数 名: dtu_mppt_send_02
*    功能说明: 读开关量输入状态（功能码 0x02），获取白天/夜晚标志
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void dtu_mppt_send_02(void) {
	DTU_MPPT_ReadRegisters( MPPT_SLAVE_ID, MPPT_FUNC_READ_SWITCH,
                            MPPT_REG_02_START, MPPT_REG_02_COUNT);
}

/*
*********************************************************************************************************
*    函 数 名: dtu_mppt_send_electricity
*    功能说明: 读蓄电池、太阳能、负载的电能参数（功能码 0x04，起始 0x3045，共 19 个寄存器）
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void dtu_mppt_send_electricity(void) {
	DTU_MPPT_ReadRegisters( MPPT_SLAVE_ID, MPPT_FUNC_READ_INPUT,
                            MPPT_REG_ELECTRICITY_START, MPPT_REG_ELECTRICITY_COUNT);
}

/*
*********************************************************************************************************
*    函 数 名: dtu_mppt_send_run_day
*    功能说明: 读运行天数（功能码 0x04，起始 0x309D，1 个寄存器）
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void dtu_mppt_send_run_day(void) {
	DTU_MPPT_ReadRegisters( MPPT_SLAVE_ID, MPPT_FUNC_READ_INPUT,
                            MPPT_REG_RUN_DAY_START, MPPT_REG_RUN_DAY_COUNT);
}

/*
*********************************************************************************************************
*    函 数 名: dtu_mppt_send_battery
*    功能说明: 读蓄电池状态（功能码 0x04，起始 0x3032，共 8 个寄存器）
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void dtu_mppt_send_battery(void) {
	DTU_MPPT_ReadRegisters( MPPT_SLAVE_ID, MPPT_FUNC_READ_INPUT,
                            MPPT_REG_BATTERY_START, MPPT_REG_BATTERY_COUNT);
}


// ========== MPPT 响应解析函数 ==========

/*
*********************************************************************************************************
*    函 数 名: dtu_mppt_parse_02
*    功能说明: 解析开关量输入状态响应（功能码 0x02），提取白天/夜晚标志
*    形    参: data: Modbus 响应帧（data[0]=地址, data[1]=功能码, data[2]=字节数, data[3..]=数据）
*              len: 数据总长度（含地址、功能码、字节数）
*    返 回 值: 无
*********************************************************************************************************
*/
static void dtu_mppt_parse_02(uint8_t *data, uint8_t len)
{
	(void)len;
	g_mppt.ai_state.day_night_status = (data[3] << 8) | data[4];
}

/*
*********************************************************************************************************
*    函 数 名: dtu_mppt_parse_electricity
*    功能说明: 解析电能参数响应（功能码 0x04），提取 19 个寄存器的值并写入 g_mppt.realtime
*    形    参: data: Modbus 响应帧（data[0]=地址, data[1]=功能码, data[2]=字节数, data[3..]=数据）
*              len: 数据总长度（含地址、功能码、字节数）
*    返 回 值: 无
*********************************************************************************************************
*/
static void dtu_mppt_parse_electricity(uint8_t *data, uint8_t len)
{
	(void)len;
	uint16_t temp_data[MPPT_REG_ELECTRICITY_COUNT];
	for (int i = 0; i < MPPT_REG_ELECTRICITY_COUNT; i++) {
		temp_data[i] = (data[3 + i * 2] << 8) | data[4 + i * 2];
	}

	// 依次填充 g_mppt.realtime 各字段（对应寄存器 0x3045~0x3057）
	g_mppt.realtime.battery_soc          = temp_data[0];   // 蓄电池剩余电量
	g_mppt.realtime.battery_voltage      = temp_data[1];   // 蓄电池电压
	g_mppt.realtime.battery_current      = abs((int16_t)temp_data[2]); // 蓄电池电流（取绝对值）
	g_mppt.realtime.battery_power_L      = temp_data[3];   // 蓄电池功率低16位
	g_mppt.realtime.battery_power_H      = temp_data[4];   // 蓄电池功率高16位
	g_mppt.realtime.load_voltage         = temp_data[5];   // 负载电压
	g_mppt.realtime.load_current         = temp_data[6];   // 负载电流
	g_mppt.realtime.load_power_L         = temp_data[7];   // 负载功率低16位
	g_mppt.realtime.load_power_H         = temp_data[8];   // 负载功率高16位
	g_mppt.realtime.solar_voltage        = temp_data[9];   // 太阳能电压
	g_mppt.realtime.solar_current        = temp_data[10];  // 太阳能电流
	g_mppt.realtime.charge_power_L       = temp_data[11];  // 发电功率低16位
	g_mppt.realtime.charge_power_H       = temp_data[12];  // 发电功率高16位
	g_mppt.realtime.daily_charge_kwh     = temp_data[13];  // 当日累计充电量
	g_mppt.realtime.total_charge_kwh_L   = temp_data[14];  // 总累计充电量低16位
	g_mppt.realtime.total_charge_kwh_H   = temp_data[15];  // 总累计充电量高16位
	g_mppt.realtime.daily_discharge_kwh  = temp_data[16];  // 当日累计用电量
	g_mppt.realtime.total_discharge_kwh_L = temp_data[17]; // 总累计用电量低16位
	g_mppt.realtime.total_discharge_kwh_H = temp_data[18]; // 总累计用电量高16位
}

/*
*********************************************************************************************************
*    函 数 名: dtu_mppt_parse_run_day
*    功能说明: 解析运行天数响应（功能码 0x04），提取运行天数
*    形    参: data: Modbus 响应帧（data[0]=地址, data[1]=功能码, data[2]=字节数, data[3..]=数据）
*              len: 数据总长度（含地址、功能码、字节数）
*    返 回 值: 无
*********************************************************************************************************
*/
static void dtu_mppt_parse_run_day(uint8_t *data, uint8_t len)
{
	(void)len;
	g_mppt.realtime.running_days_2 = (data[3] << 8) | data[4];
}

/*
*********************************************************************************************************
*    函 数 名: dtu_mppt_parse_battery
*    功能说明: 解析蓄电池状态响应（功能码 0x04），提取 8 个寄存器并解析各状态位
*    形    参: data: Modbus 响应帧（data[0]=地址, data[1]=功能码, data[2]=字节数, data[3..]=数据）
*              len: 数据总长度（含地址、功能码、字节数）
*    返 回 值: 无
*********************************************************************************************************
*/
static void dtu_mppt_parse_battery(uint8_t *data, uint8_t len)
{
	(void)len;
	uint16_t temp_data[MPPT_REG_BATTERY_COUNT];
	for (int i = 0; i < MPPT_REG_BATTERY_COUNT; i++) {
		temp_data[i] = (data[3 + i * 2] << 8) | data[4 + i * 2];
	}

	g_mppt.realtime.battery_voltage_level    = temp_data[0]; // 蓄电池电压等级
	g_mppt.realtime.battery_status           = temp_data[1]; // 蓄电池状态（位解析见 parse_battery_status）
	g_mppt.realtime.charge_device_status     = temp_data[2]; // 充电设备状态（位解析见 parse_charge_status）
	g_mppt.realtime.discharge_device_status  = temp_data[3]; // 放电设备状态（位解析见 parse_discharge_status）
	g_mppt.realtime.ambient_temp             = temp_data[4]; // 环境温度
	g_mppt.realtime.device_temp              = temp_data[5]; // 设备机内温度
	g_mppt.realtime.over_discharge_count     = temp_data[6]; // 过放次数
	g_mppt.realtime.full_charge_count        = temp_data[7]; // 充满次数

	// 解析各状态寄存器中的位字段
	dtu_mppt_parse_discharge_status(g_mppt.realtime.discharge_device_status);
	dtu_mppt_parse_charge_status(g_mppt.realtime.charge_device_status);
	dtu_mppt_parse_battery_status(g_mppt.realtime.battery_status);
}
// ========== 状态位解析函数 ==========
/*
*********************************************************************************************************
*    函 数 名: dtu_mppt_parse_discharge_status
*    功能说明: 解析放电设备状态位（对应寄存器 0x3035），写入 g_mppt.discharge
*    形    参: reg_value: 寄存器原始值
*    返 回 值: 无
*********************************************************************************************************
*/
static void dtu_mppt_parse_discharge_status(uint32_t reg_value) 
{
    g_mppt.discharge.power_level            = (reg_value >> 12) & 0x03;  // D13~D12: 输出功率等级
    g_mppt.discharge.short_circuit_protect  = (reg_value >> 11) & 0x01;  // D11: 短路保护
    g_mppt.discharge.hardware_protect       = (reg_value >> 4)  & 0x01;  // D4: 硬件保护
    g_mppt.discharge.open_circuit_protect   = (reg_value >> 3)  & 0x01;  // D3: 开路保护
    g_mppt.discharge.bit_D2                 = (reg_value >> 2)  & 0x01;  // D2
    g_mppt.discharge.bit_D1                 = (reg_value >> 1)  & 0x01;  // D1
    g_mppt.discharge.bit_D0                 =  reg_value        & 0x01;  // D0
}

/*
*********************************************************************************************************
*    函 数 名: dtu_mppt_parse_charge_status
*    功能说明: 解析充电设备状态位（对应寄存器 0x3034），写入 g_mppt.charge
*    形    参: reg_value: 寄存器原始值
*    返 回 值: 无
*********************************************************************************************************
*/
static void dtu_mppt_parse_charge_status(uint32_t reg_value) 
{
    g_mppt.charge.manual_off       = (reg_value >> 6) & 0x01;  // D6: 手动关闭
    g_mppt.charge.is_night         = (reg_value >> 5) & 0x01;  // D5: 夜晚标志
    g_mppt.charge.charge_overtemp  = (reg_value >> 4) & 0x01;  // D4: 充电过温
    g_mppt.charge.charge_state     = (reg_value >> 2) & 0x03;  // D3~D2: 充电状态 (00=未充电,01=浮充,02=强充,03=均衡充)
    g_mppt.charge.fault            = (reg_value >> 1) & 0x01;  // D1: 故障
    g_mppt.charge.is_charging      =  reg_value       & 0x01;  // D0: 正在充电
}

/*
*********************************************************************************************************
*    函 数 名: dtu_mppt_parse_battery_status
*    功能说明: 解析蓄电池状态位（对应寄存器 0x3033），写入 g_mppt.battery
*    形    参: reg_value: 寄存器原始值（D7~D4: 温度保护，D3~D0: 电压等级）
*    返 回 值: 无
*********************************************************************************************************
*/
static void dtu_mppt_parse_battery_status(uint8_t reg_value)
{    
    // 高4位（D7~D4）：电池温度保护状态
    uint8_t high_nibble = (reg_value >> 4) & 0x0F;
    g_mppt.battery.high_temp_protect = (high_nibble == 0x01) ? 1 : 0;

    // 低4位（D3~D0）：电压等级 (0=正常,1=超压,2=欠压,3=过放)
    g_mppt.battery.voltage_state = reg_value & 0x0F;
}
/*
*********************************************************************************************************
*    函 数 名: dtu_get_mppt_data_function
*    功能说明: 获取 MPPT 全局数据指针，供外部模块读取采集数据
*    形    参: 无
*    返 回 值: &g_mppt
*********************************************************************************************************
*/
void *dtu_get_mppt_data_function(void)
{
    return &g_mppt;
}

/*
*********************************************************************************************************
*    函 数 名: dtu_mppt_report_function
*    功能说明: 将 MPPT 采集数据以键值对字符串拼接到 data（由 com_report_normally_function 调用）
*    形    参: data: 上报数据缓冲区（已包含部分数据，本函数在其末尾追加）
*    返 回 值: 无
*********************************************************************************************************
*/
void dtu_mppt_report_function(uint8_t *data)
{
    MPPT_Data_t *mppt = &g_mppt;
    uint8_t  str[64]     = {0};
    uint8_t  str_buff[16] = {0};
    fp32     temp        = 0;
    uint16_t buff[8]     = {0};

    /** MPPT设备机内超温（白天/夜晚标志） **/
    memset(str, 0, sizeof(str));
    sprintf((char*)str, "MOT=%01d;", (uint8_t)mppt->ai_state.day_night_status);
    strcat((char*)data, (char*)str);

    /** MPPT运行天数 **/
    memset(str, 0, sizeof(str));
    sprintf((char*)str, "MRD=%01d;", (uint16_t)mppt->realtime.running_days_2);
    strcat((char*)data, (char*)str);

    /** MPPT蓄电池电压等级 **/
    memset(str, 0, sizeof(str));
    sprintf((char*)str, "MBL=%01d;", (uint8_t)(mppt->realtime.battery_voltage_level / 100));
    strcat((char*)data, (char*)str);

    /** MPPT蓄电池状态 **/
    memset(str, 0, sizeof(str));
    sprintf((char*)str, "MBSA=%01d;MBSB=%01d;",
            (uint8_t)mppt->battery.voltage_state,
            (uint8_t)mppt->battery.high_temp_protect);
    strcat((char*)data, (char*)str);

    /** MPPT充电设备状态 **/
    memset(str, 0, sizeof(str));
    sprintf((char*)str, "MCSA=%01d;MCSB=%01d;MCSC=%01d;MCSD=%01d;MCSE=%01d;MCSF=%01d;",
            (uint8_t)mppt->charge.is_charging,
            (uint8_t)mppt->charge.fault,
            (uint8_t)mppt->charge.charge_state,
            (uint8_t)mppt->charge.charge_overtemp,
            (uint8_t)mppt->charge.is_night,
            (uint8_t)mppt->charge.manual_off);
    strcat((char*)data, (char*)str);

    /** MPPT放电设备状态 **/
    memset(str, 0, sizeof(str));
    sprintf((char*)str, "MDCSA=%01d;MDCSB=%01d;MDCSC=%01d;MDCSD=%01d;MDCSE=%01d;MDCSF=%01d;MDCSG=%01d;",
            (uint8_t)mppt->discharge.bit_D0,
            (uint8_t)mppt->discharge.bit_D1,
            (uint8_t)mppt->discharge.bit_D2,
            (uint8_t)mppt->discharge.open_circuit_protect,
            (uint8_t)mppt->discharge.hardware_protect,
            (uint8_t)mppt->discharge.short_circuit_protect,
            (uint8_t)mppt->discharge.power_level);
    strcat((char*)data, (char*)str);

    /** MPPT过放、充满次数 **/
    memset(str, 0, sizeof(str));
    sprintf((char*)str, "MODC=%01d;MFC=%01d;",
            (uint8_t)mppt->realtime.over_discharge_count,
            (uint8_t)mppt->realtime.full_charge_count);
    strcat((char*)data, (char*)str);

    /** 电池剩余电量 **/
    memset(str, 0, sizeof(str));
    sprintf((char*)str, "MBS=%01d;", (uint8_t)mppt->realtime.battery_soc);
    strcat((char*)data, (char*)str);

    /** 蓄电池电压、电流 **/
    memset(str, 0, sizeof(str));
    temp = mppt->realtime.battery_voltage / 100.0f;
    buff[0] = (uint16_t)temp;
    buff[1] = (uint16_t)((temp - buff[0]) * 100);
    temp = mppt->realtime.battery_current / 100.0f;
    buff[2] = (uint16_t)temp;
    buff[3] = (uint16_t)((temp - buff[2]) * 100);
    sprintf((char*)str, "MBV=%d.%02d;MBA=%d.%02d;", buff[0], buff[1], buff[2], buff[3]);
    strcat((char*)data, (char*)str);

    /** 负载电压、电流 **/
    memset(str, 0, sizeof(str));
    temp = mppt->realtime.load_voltage / 100.0f;
    buff[0] = (uint16_t)temp;
    buff[1] = (uint16_t)((temp - buff[0]) * 100);
    temp = mppt->realtime.load_current / 100.0f;
    buff[2] = (uint16_t)temp;
    buff[3] = (uint16_t)((temp - buff[2]) * 100);
    sprintf((char*)str, "MLV=%d.%02d;MLA=%d.%02d;", buff[0], buff[1], buff[2], buff[3]);
    strcat((char*)data, (char*)str);

    /** 太阳能电压、电流 **/
    memset(str, 0, sizeof(str));
    temp = mppt->realtime.solar_voltage / 100.0f;
    buff[0] = (uint16_t)temp;
    buff[1] = (uint16_t)((temp - buff[0]) * 100);
    temp = mppt->realtime.solar_current / 100.0f;
    buff[2] = (uint16_t)temp;
    buff[3] = (uint16_t)((temp - buff[2]) * 100);
    sprintf((char*)str, "MSV=%d.%02d;MSA=%d.%02d;", buff[0], buff[1], buff[2], buff[3]);
    strcat((char*)data, (char*)str);

    /** 蓄电池、发电功率 **/
    memset(str, 0, sizeof(str));
    memset(str_buff, 0, sizeof(str_buff));
    temp = (float)abs((int32_t)(((uint32_t)mppt->realtime.battery_power_H << 16) | mppt->realtime.battery_power_L)) / 100.0f;
    buff[0] = (uint16_t)temp;
    buff[1] = (uint16_t)((temp - buff[0]) * 100);
    temp = (float)(((uint32_t)mppt->realtime.charge_power_H << 16) | mppt->realtime.charge_power_L) / 100.0f;
    buff[2] = (uint16_t)temp;
    buff[3] = (uint16_t)((temp - buff[2]) * 100);
    sprintf((char*)str, "MBW=%d.%02d;MSW=%d.%02d;", buff[0], buff[1], buff[2], buff[3]);
    strcat((char*)data, (char*)str);

    /** 用电量 充电量 **/
    memset(str, 0, sizeof(str));
    memset(str_buff, 0, sizeof(str_buff));
    temp = mppt->realtime.daily_charge_kwh / 100.0f;
    buff[0] = (uint16_t)temp;
    buff[1] = (uint16_t)((temp - buff[0]) * 100);
    temp = (float)(((uint32_t)mppt->realtime.total_charge_kwh_H << 16) | mppt->realtime.total_charge_kwh_L) / 100.0f;
    buff[2] = (uint16_t)temp;
    buff[3] = (uint16_t)((temp - buff[2]) * 100);
    temp = mppt->realtime.daily_discharge_kwh / 100.0f;
    buff[4] = (uint16_t)temp;
    buff[5] = (uint16_t)((temp - buff[4]) * 100);
    temp = (float)(((uint32_t)mppt->realtime.total_discharge_kwh_H << 16) | mppt->realtime.total_discharge_kwh_L) / 100.0f;
    buff[6] = (uint16_t)temp;
    buff[7] = (uint16_t)((temp - buff[6]) * 100);
    sprintf((char*)str, "MDCKWH=%d.%02d;MTCKWH=%d.%02d;MDDCKWH=%d.%02d;MTDCKWH=%d.%02d;",
            buff[0], buff[1], buff[2], buff[3], buff[4], buff[5], buff[6], buff[7]);
    strcat((char*)data, (char*)str);
}


#endif  /* configUSE_SUN_POWER */
