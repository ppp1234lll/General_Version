#ifndef __GPS_H
#define __GPS_H

#include "./SYSTEM/sys/sys.h"

#define LBS_APIKEY "20dbf2e1e0180790656d9a028245fd4e"  // 高德智能硬件定位1.0

struct locat_infor_t 
{  // 位置信息
	double longitude;	// 经度
	double latitude;	// 纬度
	uint8_t longitude_dir; // E-东经 W-西经
	uint8_t latitude_dir;  // N-北纬 S-南纬
};

int8_t gps_start_function(void);
void gps_close_function(void);
int8_t gps_status_monitor_function(void);
int8_t gps_read_position_information(struct locat_infor_t *infor_t);
int8_t gps_lbs_position_read_function(struct locat_infor_t *infor_t);
#endif

