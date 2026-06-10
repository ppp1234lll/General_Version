#ifndef AHT20_H_
#define AHT20_H_
#include "appconfig.h"
#include "bsp.h"


void aht20_init_function(void);
int8_t aht20_measure(double *humidity, double *temperature);
uint8_t cal_crc_table(uint8_t *ptr, uint8_t len);
void aht20_test(void);
#endif
