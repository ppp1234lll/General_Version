#ifndef _DTU_H_
#define _DTU_H_

#include "./SYSTEM/sys/sys.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "semphr.h"

extern SemaphoreHandle_t dtu_config_sem;
extern QueueHandle_t dtu_tx_q;
extern QueueHandle_t dtu_rx_q;

// 控制器配置
#define MPPT_SLAVE_ID           0x01    // 控制器默认ID
// 功能码定义
#define MPPT_FUNC_READ_SWITCH    0x02    // 读开关输入状态
#define MPPT_FUNC_READ_HOLD      0x03    // 读保持寄存器
#define MPPT_FUNC_READ_INPUT     0x04    // 读输入寄存器
#define MPPT_FUNC_READ_COIL      0x05    // 写单个线圈
#define MPPT_FUNC_WRITE_SINGLE   0x06    // 写单个寄存器
#define MPPT_FUNC_WRITE_MULTI    0x10    // 写多个寄存器

// 寄存器地址定义
#define MPPT_REG_02_START                0x2000  //机内超温
#define MPPT_REG_02_COUNT                1

#define MPPT_REG_CFS_START               0x3011  //控制器功能状态
#define MPPT_REG_CFS_COUNT               3

#define MPPT_REG_ELECTRICITY_START       0x3045  //蓄电池、太阳能及负载的电能参数
#define MPPT_REG_ELECTRICITY_COUNT       19

#define MPPT_REG_BATTERY_START           0x3032  //蓄电池状态
#define MPPT_REG_BATTERY_COUNT           8

#define MPPT_REG_RUN_DAY_START           0x309D  //运行天数
#define MPPT_REG_RUN_DAY_COUNT           1

// ==================== 写入功能码定义 ====================
#define MPPT_FUNC_WRITE_SINGLE  0x06    // 写单个寄存器
#define MPPT_FUNC_WRITE_MULTI   0x10    // 写多个寄存器
#define MPPT_FUNC_WRITE_COIL    0x05    // 写单个线圈

// 写入相关常量
#define MPPT_WRITE_TIMEOUT      1000    // 写入超时时间(ms)
#define MPPT_WRITE_RETRY_MAX    3       // 最大重试次数


typedef struct {
		// 开关量输入状态（功能码 0x02）
		uint16_t device_over_temp;              // 0x2000: 设备机内超温
		uint16_t day_night_status;              // 0x200C: 白天夜晚
}MPPT_AI_State;

typedef struct {
		// 开关量输入状态（功能码 0x02）
		uint16_t device_over_temp;              // 0x2000: 设备机内超温
		uint16_t day_night_status;              // 0x200C: 白天夜晚

		// 输入寄存器（功能码 0x04）
		uint16_t controller_func_status1;       // 0x3011: 控制器功能状态1
		uint16_t controller_func_status2;       // 0x3012: 控制器功能状态2
		uint16_t controller_func_status3;       // 0x3013: 控制器功能状态3
		uint16_t controller_func_status4;       // 0x3014: 控制器功能状态4
		uint16_t li_lvd_min;                    // 0x3015: 锂电LVD最小设置值
		uint16_t li_lvd_max;                    // 0x3016: 锂电LVD最大设置值
		uint16_t li_lvd_default;                // 0x3017: 锂电LVD默认设置值
		uint16_t li_lvr_min;                    // 0x3018: 锂电LVR最小设置值
		uint16_t li_lvr_max;                    // 0x3019: 锂电LVR最大设置值
		uint16_t li_lvr_default;                // 0x301A: 锂电LVR默认设置值
		uint16_t li_cvt_min;                    // 0x301B: 锂电CVT最小设置值
		uint16_t li_cvt_max;                    // 0x301C: 锂电CVT最大设置值
		uint16_t li_cvt_default;                // 0x301D: 锂电CVT默认设置值
		uint16_t li_cvr_min;                    // 0x301E: 锂电CVR最小设置值
		uint16_t li_cvr_max;                    // 0x301F: 锂电CVR最大设置值
		uint16_t li_cvr_default;                // 0x3020: 锂电CVR默认设置值
		uint16_t li_dark_ctrl_min;              // 0x3021: 锂电天黑光控点最小设置值
		uint16_t li_dark_ctrl_max;              // 0x3022: 锂电天黑光控点最大设置值
		uint16_t li_dark_ctrl_default;          // 0x3023: 锂电天黑光控点默认设置值
		uint16_t li_auto_power_off_min;         // 0x3024: 锂电自动降功率点最小设置值
		uint16_t li_auto_power_off_max;         // 0x3025: 锂电自动降功率点最大设置值
		uint16_t li_auto_power_off_default;     // 0x3026: 锂电自动降功率点默认设置值
		uint16_t load_current_min;              // 0x3027: 负载电流最小设置值
		uint16_t load_current_max;              // 0x3028: 负载电流最大设置值
		uint16_t li_cvt_cvr_diff_max;           // 0x3029: 锂电池CVT与CVR最大允许压差
		uint16_t li_cvt_cvr_diff_min;           // 0x302A: 锂电池CVT与CVR最小允许压差
		uint16_t li_lvd_lvr_min_diff;           // 0x302B: 锂电LVD与LVR最小允许压差
		uint16_t li_cvr_lvd_cvt_lvr_min_diff;   // 0x302C: 锂电CVR与LVD及CVT与LVR最小允许压差
		uint16_t over_temp_protect_min;         // 0x302D: 过温保护最小设置值
		uint16_t over_temp_protect_max;         // 0x302E: 过温保护最大设置值
		uint16_t over_temp_protect_default;     // 0x302F: 过温保护默认设置值
		uint16_t slave_id;                      // 0x3030: 从机ID
		uint16_t running_days;                  // 0x3031: 运行天数
		uint16_t battery_voltage_level;         // 0x3032: 当前蓄电池电压等级
		uint16_t battery_status;                // 0x3033: 蓄电池状态
		uint16_t charge_device_status;          // 0x3034: 充电设备状态
		uint16_t discharge_device_status;       // 0x3035: 放电设备状态
		uint16_t ambient_temp;                  // 0x3036: 环境温度
		uint16_t device_temp;                   // 0x3037: 设备机内温度
		uint16_t over_discharge_count;          // 0x3038: 过放次数
		uint16_t full_charge_count;             // 0x3039: 充满次数
		uint16_t over_voltage_count;            // 0x303A: 过压保护次数
		uint16_t over_current_count;            // 0x303B: 过流保护次数
		uint16_t short_circuit_count;           // 0x303C: 短路保护次数
		uint16_t open_circuit_count;            // 0x303D: 开路保护次数
		uint16_t hardware_protect_count;        // 0x303E: 硬件保护次数
		uint16_t charge_over_temp_count;        // 0x303F: 充电过温保护次数
		uint16_t discharge_over_temp_count;     // 0x3040: 放电过温保护次数
		uint16_t battery_soc;                  // 0x3045: 蓄电池剩余电量 (%)
		uint16_t battery_voltage;              // 0x3046: 蓄电池电压 (V/100)
		uint16_t battery_current;              // 0x3047: 蓄电池电流 (A/100, 充电为正)
		uint16_t battery_power_L;              // 0x3048: 蓄电池功率低16位 (W/100)
		uint16_t battery_power_H;              // 0x3049: 蓄电池功率高16位
		uint16_t load_voltage;                 // 0x304A: 负载电压 (V/100)
		uint16_t load_current;                 // 0x304B: 负载电流 (A/100)
		uint16_t load_power_L;                 // 0x304C: 负载功率低16位 (W/100)
		uint16_t load_power_H;                 // 0x304D: 负载功率高16位
		uint16_t solar_voltage;                 // 0x304E: 太阳能电压
		uint16_t solar_current;                 // 0x304F: 太阳能电流
		uint16_t charge_power_L;                // 0x3050: 发电功率L
		uint16_t charge_power_H;                // 0x3051: 发电功率H
		uint16_t daily_charge_kwh;              // 0x3052: 当日累计充电量
		uint16_t total_charge_kwh_L;            // 0x3053: 总累计充电量L
		uint16_t total_charge_kwh_H;            // 0x3054: 总累计充电量H
		uint16_t daily_discharge_kwh;           // 0x3055: 当日累计用电量
		uint16_t total_discharge_kwh_L;         // 0x3056: 总累计用电量L
		uint16_t total_discharge_kwh_H;         // 0x3057: 总累计用电量H
		uint16_t history_light_minutes;         // 0x3058: 历史亮灯时长
		uint16_t cascade_total_charge_L;        // 0x3059: 多台设备级联时总累计充电量L
		uint16_t cascade_total_charge_H;        // 0x305A: 多台设备级联时总累计充电量H
		uint16_t cascade_total_discharge_L;     // 0x305B: 多台设备级联时总累计用电量L
		uint16_t cascade_total_discharge_H;     // 0x305C: 多台设备级联时总累计用电量H
		uint16_t month_charge_kwh_L;            // 0x305D: 当月累计充电量L
		uint16_t month_charge_kwh_H;            // 0x305E: 当月累计充电量H
		uint16_t year_charge_kwh_L;             // 0x305F: 当年累计充电量L
		uint16_t year_charge_kwh_H;             // 0x3060: 当年累计充电量H
		uint16_t charge_kwh_1d_ago;             // 0x3061: 1天前充电量
		uint16_t charge_kwh_2d_ago;             // 0x3062: 2天前充电量
		uint16_t charge_kwh_3d_ago;             // 0x3063: 3天前充电量
		uint16_t charge_kwh_4d_ago;             // 0x3064: 4天前充电量
		// ... 0x3065 ~ 0x309B: 5天前 ~ 59天前充电量（55项，省略）
		uint16_t charge_kwh_60d_ago;            // 0x309C: 60天前充电量
		uint16_t running_days_2;                // 0x309D: 运行天数
		uint16_t reserved_309E;                 // 0x309E: 预留
		uint16_t reserved_309F;                 // 0x309F: 预留
		uint16_t battery_voltage1;               // 0x30A0: 蓄电池电压
		uint16_t battery_current1;               // 0x30A1: 蓄电池电流
		uint16_t ambient_temp_2;                // 0x30A2: 环境温度
		uint16_t battery_status_2;              // 0x30A3: 蓄电池状态
		uint16_t charge_device_status_2;        // 0x30A4: 充电设备状态
		uint16_t discharge_device_status_2;     // 0x30A5: 放电设备状态
		uint16_t over_discharge_count_2;        // 0x30A6: 过放次数
		uint16_t full_charge_count_2;           // 0x30A7: 充满次数
		uint16_t daily_max_battery_voltage;     // 0x30A8: 当日最高蓄电池电压
		uint16_t daily_min_battery_voltage;     // 0x30A9: 当日最低蓄电池电压
		uint16_t battery_max_voltage_1d_ago;    // 0x30AA: 1天前电池最高电压
		uint16_t battery_max_voltage_2d_ago;    // 0x30AB: 2天前电池最高电压
		// ... 0x30AC ~ 0x30E4: 3天前 ~ 59天前电池最高电压（57项，省略）
		uint16_t battery_max_voltage_60d_ago;   // 0x30E5: 60天前电池最高电压
		uint16_t battery_min_voltage_1d_ago;    // 0x30E6: 1天前电池最低电压
		uint16_t battery_min_voltage_2d_ago;    // 0x30E7: 2天前电池最低电压
		// ... 0x30E8 ~ 0x3120: 3天前 ~ 59天前电池最低电压（57项，省略）
		uint16_t battery_min_voltage_60d_ago;   // 0x3121: 60天前电池最低电压

		uint16_t load_voltage1;                  // 0x3125: 负载电压
		uint16_t load_current1;                  // 0x3126: 负载电流
		uint16_t load_power_L1;                  // 0x3127: 负载功率L
		uint16_t load_power_H1;                  // 0x3128: 负载功率H
		uint16_t daily_load_kwh;                // 0x3129: 当日累计用电量
		uint16_t month_load_kwh_L;              // 0x312A: 当月累计用电量L
		uint16_t month_load_kwh_H;              // 0x312B: 当月累计用电量H
		uint16_t year_load_kwh_L;               // 0x312C: 当年累计用电量L
		uint16_t year_load_kwh_H;               // 0x312D: 当年累计用电量H
		uint16_t total_load_kwh_L;              // 0x312E: 总累计用电量L
		uint16_t total_load_kwh_H;              // 0x312F: 总累计用电量H
		uint16_t load_kwh_1d_ago;               // 0x3130: 1天前用电量
		uint16_t load_kwh_2d_ago;               // 0x3131: 2天前用电量
		uint16_t load_kwh_3d_ago;               // 0x3132: 3天前用电量
		uint16_t load_kwh_4d_ago;               // 0x3133: 4天前用电量
		// ... 0x3134 ~ 0x316A: 5天前 ~ 59天前用电量（55项，省略）
		uint16_t load_kwh_60d_ago;              // 0x316B: 60天前用电量
		uint16_t running_days_3;                // 0x316C: 运行天数
		uint16_t reserved_316D;                 // 0x316D: 预留
		uint16_t reserved_316E;                 // 0x316E: 预留
		uint16_t reserved_316F;                 // 0x316F: 预留
		uint16_t reserved_3170;                 // 0x3170: 预留
		uint16_t reserved_3171;                 // 0x3171: 预留
		uint16_t reserved_3172;                 // 0x3172: 预留
		uint16_t reserved_3173;                 // 0x3173: 预留
		uint16_t running_days_4;                // 0x3174: 运行天数
		uint16_t time_offset_1d_ago;            // 0x3175: 1天前具体对应时间
		uint16_t time_offset_2d_ago;            // 0x3176: 2天前具体对应时间
		uint16_t time_offset_3d_ago;            // 0x3177: 3天前具体对应时间
		uint16_t time_offset_4d_ago;            // 0x3178: 4天前具体对应时间
		// ... 0x3179 ~ 0x31AF: 5天前 ~ 59天前具体对应时间（55项，省略）
		uint16_t time_offset_60d_ago;           // 0x31B0: 60天前具体对应时间
} MPPT_RealtimeData_t;

typedef struct {
		// 额定参数（输入寄存器，功能码 0x04）
		uint16_t solar_rated_voltage;           // 0x3000: 太阳能额定电压 (V/100)
		uint16_t solar_rated_current;           // 0x3001: 太阳能额定电流 (A/100)
		uint16_t solar_rated_power_L;           // 0x3002: 太阳能额定功率低16位 (W/100)
		uint16_t solar_rated_power_H;           // 0x3003: 太阳能额定功率高16位
		uint16_t battery_rated_voltage;         // 0x3004: 蓄电池额定电压 (V/100)
		uint16_t battery_rated_current;         // 0x3005: 蓄电池额定电流 (A/100)
		uint16_t battery_rated_power_L;         // 0x3006: 蓄电池额定功率低16位 (W/100)
		uint16_t battery_rated_power_H;         // 0x3007: 蓄电池额定功率高16位
		uint16_t load_rated_voltage;            // 0x3008: 负载额定电压 (V/100)
		uint16_t load_rated_current;            // 0x3009: 负载额定电流 (A/100)
		uint16_t load_rated_power_L;            // 0x300A: 负载额定功率低16位 (W/100)
		uint16_t load_rated_power_H;            // 0x300B: 负载额定功率高16位
} MPPT_Rated_Parameters;

typedef struct {
		// 设备参数（保持寄存器，功能码 0x03/0x06/0x10）
		uint16_t realtime_clock_sec;                // 0x9017: 实时时钟-秒
		uint16_t realtime_clock_min;                // 0x9018: 实时时钟-分
		uint16_t realtime_clock_hour;               // 0x9019: 实时时钟-时
		uint16_t realtime_clock_day;                // 0x901A: 实时时钟-日
		uint16_t realtime_clock_month;              // 0x901B: 实时时钟-月
		uint16_t realtime_clock_year;               // 0x901C: 实时时钟-年 (年的低两位，范围0~99)
		uint16_t baud_rate;                         // 0x901D: 波特率 (D3~D0: 00H=4800, 01H=9600, 02H=19200, 03H=57600, 04H=115200)
		uint16_t backlight_time;                    // 0x901E: 液晶背光点亮延时 (秒，范围0~600)
		uint16_t device_password;                   // 0x901F: 设备密码 (D15~D12最高位, D11~D8第三位, D5~D4第二位, D3~D0最低位)
		uint16_t slave_id_9020;                     // 0x9020: 从机ID (范围1~247，忘记ID可用254获取)
} MPPT_Parameters;

typedef struct {
		// 蓄电池及负载参数（保持寄存器，功能码 0x03/0x06/0x10）
		uint16_t battery_type;                      // 0x9021: 电池类型 (0000H=锂电, 0001H=液体, 0002H=胶体, 0003H=AGM)
		uint16_t low_voltage_protect;               // 0x9022: 低压保护 (V/100)
		uint16_t low_voltage_recover;               // 0x9023: 低压恢复 (V/100)
		uint16_t boost_voltage;                     // 0x9024: 强充电压 (V/100)
		uint16_t equalize_voltage;                  // 0x9025: 均衡充电压 (V/100)
		uint16_t float_voltage;                     // 0x9026: 浮充电压 (V/100)
		uint16_t system_rated_voltage_level;        // 0x9027: 系统额定电压等级 (0=自动识别, 1=12V, 2=24V, 3=36V, 4=48V, 5=60V, 6=110V, 7=120V, 8=220V, 9=240V)
		uint16_t li_overcharge_protect;             // 0x9028: 锂电过充保护 (V/100)
		uint16_t li_overcharge_recover;             // 0x9029: 锂电过充恢复 (V/100)
		uint16_t li_zero_deg_charge;                // 0x902A: 锂电零度充电 (D3~D0: 00H=正常充, 01H=禁充, 02H=慢充)
		uint16_t mt_work_mode;                      // 0x902B: MT系列工作模式 (0~12对应各种模式)
		uint16_t mt_manual_control;                 // 0x902C: MT系列手动控制条件下默认设定的开/关 (0=开, 1=关)
		uint16_t mt_timer_on_period1;               // 0x902D: MT系列定时开时段1 (D15~D8=小时, D7~D0=分钟)
		uint16_t mt_timer_on_period2;               // 0x902E: MT系列定时开时段2 (D15~D8=小时, D7~D0=分钟)
		uint16_t mt_timer_on_time1_sec;             // 0x902F: 定时开时刻1-秒
		uint16_t mt_timer_on_time1_min;             // 0x9030: 定时开时刻1-分
		uint16_t mt_timer_on_time1_hour;            // 0x9031: 定时开时刻1-时
		uint16_t mt_timer_off_time1_sec;            // 0x9032: 定时关时刻1-秒
		uint16_t mt_timer_off_time1_min;            // 0x9033: 定时关时刻1-分
		uint16_t mt_timer_off_time1_hour;           // 0x9034: 定时关时刻1-时
		uint16_t mt_timer_on_time2_sec;             // 0x9035: 定时开时刻2-秒
		uint16_t mt_timer_on_time2_min;             // 0x9036: 定时开时刻2-分
		uint16_t mt_timer_on_time2_hour;            // 0x9037: 定时开时刻2-时
		uint16_t mt_timer_off_time2_sec;            // 0x9038: 定时关时刻2-秒
		uint16_t mt_timer_off_time2_min;            // 0x9039: 定时关时刻2-分
		uint16_t mt_timer_off_time2_hour;           // 0x903A: 定时关时刻2-时
		uint16_t dc_timer_control_enable;           // 0x903B: DC系列定时控制使能
		uint16_t dc_timer_group_select;             // 0x903C: DC系列定时组选择
		uint16_t dc_timer_control_mode;             // 0x903D: DC系列定时控制模式
		uint16_t dc_timer_power_period1;            // 0x903E: DC系列定时控制时间段1功率 (0~10对应0%~100%)
		uint16_t dc_timer_power_period2;            // 0x903F: DC系列定时控制时间段2功率 (0~10对应0%~100%)
		uint16_t dc_time_period1;                   // 0x9040: DC系列第一时间段 (分钟)
		uint16_t dc_power_period1;                  // 0x9041: DC系列第一功率 (0~10对应0%~100%)
		uint16_t dc_time_period2;                   // 0x9042: DC系列第二时间段 (分钟, 0~15对应0~450分钟)
		uint16_t dc_power_period2;                  // 0x9043: DC系列第二功率 (0~10对应0%~100%)
		uint16_t dc_time_period3;                   // 0x9044: DC系列第三时间段 (分钟, 0~15对应0~450分钟)
		uint16_t dc_power_period3;                  // 0x9045: DC系列第三功率 (0~10对应0%~100%)
		uint16_t dc_time_period4;                   // 0x9046: DC系列第四时间段 (分钟, 0~15对应0~450分钟, 15=TOT)
		uint16_t dc_power_period4;                  // 0x9047: DC系列第四功率 (0~10对应0%~100%)
		uint16_t dc_time_period5;                   // 0x9048: DC系列第五时间段 (分钟, 0~15对应0~450分钟)
		uint16_t dc_power_period5;                  // 0x9049: DC系列第五功率 (0~10对应0%~100%)
		uint16_t dc_load_current_set;               // 0x904A: DC系列负载电流设置 (A/100, 范围10~1000)
		uint16_t dc_auto_power_off_mode;            // 0x904B: DC系列自动降功率模式 (00H=自动降功率, 01H=365模式, 02H/03H=不降功率)
		uint16_t dc_power_off_point;                // 0x904C: DC系列降功率点 (V/100)
		uint16_t dc_power_off_ratio;                // 0x904D: DC系列降功率比例 (范围1~20)
		uint16_t dc_infrared_delay_off;             // 0x904E: DC系列红外延时关闭时间 (1~15对应10~150秒)
		uint16_t dc_infrared_unoccupied_power;      // 0x904F: DC系列红外无人功率 (0~10对应0%~100%)
		uint16_t light_control_switch;              // 0x9052: 光控开关 (0=关, 1=开)
		uint16_t light_control_day_voltage;         // 0x9053: 光控天亮电压 (V/100)
		uint16_t dimming_ratio;                     // 0x9054: 调光亮度比例 (范围0~100)
		uint16_t time_period1_hour;                 // 0x9055: 时间段1时间-时 (6时段模式)
		uint16_t time_period1_min;                  // 0x9056: 时间段1时间-分 (6时段模式)
		uint16_t time_period1_power;                // 0x9057: 时间段1功率 (范围0~100)
		uint16_t time_period2_hour;                 // 0x9058: 时间段2时间-时 (6时段模式)
		uint16_t time_period2_min;                  // 0x9059: 时间段2时间-分 (6时段模式)
		uint16_t time_period2_power;                // 0x905A: 时间段2功率 (范围0~100)
		uint16_t time_period3_hour;                 // 0x905B: 时间段3时间-时 (6时段模式)
		uint16_t time_period3_min;                  // 0x905C: 时间段3时间-分 (6时段模式)
		uint16_t time_period3_power;                // 0x905D: 时间段3功率 (范围0~100)
		uint16_t time_period4_hour;                 // 0x905E: 时间段4时间-时 (6时段模式)
		uint16_t time_period4_min;                  // 0x905F: 时间段4时间-分 (6时段模式)
		uint16_t time_period4_power;                // 0x9060: 时间段4功率 (范围0~100)
		uint16_t time_period5_hour;                 // 0x9061: 时间段5时间-时 (6时段模式)
		uint16_t time_period5_min;                  // 0x9062: 时间段5时间-分 (6时段模式)
		uint16_t time_period5_power;                // 0x9063: 时间段5功率 (范围0~100)
		uint16_t time_period6_hour;                 // 0x9064: 时间段6时间-时 (6时段模式)
		uint16_t time_period6_min;                  // 0x9065: 时间段6时间-分 (6时段模式)
		uint16_t time_period6_power;                // 0x9066: 时间段6功率 (范围0~100)
		uint16_t end_time_hour;                     // 0x9067: 结束时间-时 (6时段模式)
		uint16_t end_time_min;                      // 0x9068: 结束时间-分 (6时段模式)
		uint16_t max_charge_current_set;            // 0x9069: 最大充电电流设定 (A/100, 需0x8FF2-D8为1才可用)
		uint16_t over_temp_protect_temp;            // 0x906A: 过温保护温度 (℃/100)
} MPPT_Battery_Load;

typedef struct {
    uint16_t control_status1;
    uint16_t control_status2;
    uint16_t control_status3;
    uint16_t control_status4;
    uint16_t lvd_min;
    uint16_t lvd_max;
    uint16_t lvd_default;
    uint16_t lvr_min;
    uint16_t lvr_max;
    uint16_t lvr_default;
    uint16_t cvt_min;
    uint16_t cvt_max;
    uint16_t cvt_default;
    uint16_t cvr_min;
    uint16_t cvr_max;
    uint16_t cvr_default;
    uint16_t dark_min;
    uint16_t dark_max;
    uint16_t dark_default;
    uint16_t power_down_min;
    uint16_t power_down_max;
    uint16_t power_down_default;
    uint16_t load_current_min;
    uint16_t load_current_max;
    uint16_t cvt_cvr_max;
    uint16_t cvt_cvr_min;
    uint16_t lvd_lvr_min;
    uint16_t cvr_lvd_cvt_lvr_min;
    uint16_t temperature_protection_min;
    uint16_t temperature_protection_max;
    uint16_t temperature_protection_default;
    uint16_t battery_volt_level;
} MPPT_ConfigData_t;

typedef struct {
    uint8_t is_connected;
    uint8_t last_error;
    uint32_t last_update_time;
    uint8_t  mppt_cycle_time;
    uint8_t  flag;
} MPPT_CommStatus_t;

typedef struct {
    uint8_t power_level;            // D13~D12: 输出功率等级
    uint8_t short_circuit_protect;     // D11: 短路保护标志
    uint8_t hardware_protect;          // D4: 硬件保护标志
    uint8_t open_circuit_protect;      // D3: 开路保护标志
    uint8_t bit_D2;                    // D2 单独提取
    uint8_t bit_D1;                    // D1 单独提取
    uint8_t bit_D0;                    // D0 单独提取
} DischargeDeviceStatus;

typedef struct {
    uint8_t manual_off;            // D6  1=被手动关闭, 0=正常充电
    uint8_t is_night;              //D5 1=夜晚, 0=白天
    uint8_t charge_overtemp;       //D4 1=充电过温, 0=正常
    uint8_t charge_state;       //D2-D3 00=未充电,01=浮充,02=强充,03=均衡充   
    uint8_t fault;                 //D1 1=故障, 0=正常
    uint8_t is_charging;           //D0 1=正在充电, 0=未充电
} ChargeDeviceStatus;

// 电池状态结构体
typedef struct {
    uint8_t high_temp_protect;  // 高温保护状态：0=正常, 1=高温保护
    uint8_t voltage_state;     // 电压状态：0=正常,1=超压,2=欠压,3=过放
    const char *desc;           // 状态描述字符串
} BatteryStatus_t;

// MPPT 全局数据聚合结构体
typedef struct {
    MPPT_RealtimeData_t   realtime; //
    MPPT_ConfigData_t     config;
    MPPT_AI_State         ai_state;
    DischargeDeviceStatus discharge;
    ChargeDeviceStatus    charge;
    BatteryStatus_t       battery;
} MPPT_Data_t;

extern MPPT_Data_t g_mppt;


/* 提供给其他C文件调用的函数 */
void dtu_task_function(void);

void *dtu_get_rs485_config_function(void);
BaseType_t dtu_transmit_enqueue(uint8_t *data, uint16_t len);
void dtu_get_receive_data(uint8_t *data, uint16_t len);

void *dtu_get_mppt_data_function(void);
void dtu_mppt_report_function(uint8_t *data);

#endif // _DTU_H_
