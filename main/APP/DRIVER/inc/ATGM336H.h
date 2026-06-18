#ifndef _ATGM336H_H_
#define _ATGM336H_H_

#include "./SYSTEM/sys/sys.h"

/* ATGM336H结构体 */
// GNSS data structure definition
typedef struct 
{
    double latitude;          /* 纬度，扩大100000倍，单位：度 */
    double longitude;         /* 经度，扩大100000倍，单位：度 */
    char lat_dir;               /* 指示北纬、南纬 */
    char lon_dir;               /* 指示东经、西经 */
    uint8_t fix_quality;        // Fix quality 0=Invalid, 1=GPS Fix, 2=DGPS Fix, 3=PPS Fix, 4=RTK, 5=Float RTK
    uint8_t fix_status;         /* GPS质量指标 */
    uint8_t num_satellites;     /* 使用的卫星数量 */
    double hdop;              /* 水平精度因子，扩大10倍 */
    double altitude;           /* 海拔高度，扩大10倍 */
    uint16_t dgps_id;           /* DGPS站ID */
    uint8_t is_valid;           // Data validity flag
    uint8_t status;             // 串口数据是否
} atgm336h_data_t;

// Function declarations
void bsp_InitUart_GPS(uint32_t baudrate);
uint8_t atgm336h_decode_nmea_xxgga(void);
atgm336h_data_t* atgm336h_get_gnss_data(void);

void ATGM338H_test(void);

#endif /* _ATGM338H_H_ */
