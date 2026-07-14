#ifndef __AHT20_H_
#define __AHT20_H_

#include "./SYSTEM/sys/sys.h"

/*供外部调用的函数声明 */
void aht20_init(void);
int8_t aht20_measure(double *humidity, double *temperature);

void aht20_test(void);
#endif
