#ifndef HAL_LIS3DH_H_
#define HAL_LIS3DH_H_
#include <stdbool.h>
#include <stdint.h>
#include <stdint.h>
#include "bsp.h"

#include "lis3dh_driver.h"

uint8_t LIS3DH_ReadReg(uint8_t Reg, uint8_t* Data) ;
uint8_t LIS3DH_WriteReg(uint8_t WriteAddr, uint8_t Data) ;
int hal_lis3dh_init(bool iic_mode_sel);

int hal_lis3dh_i2c_write
(
        uint8_t reg_addr,
        uint8_t const *data,
        uint8_t length
);


int hal_lis3dh_i2c_read
(
        uint8_t reg_addr,
        uint8_t *data,
        uint8_t length
);

int hal_lis3dh_spi_write
(
        uint8_t reg_addr,
        uint8_t const *data,
        uint8_t length
);


int hal_lis3dh_spi_read
(
        uint8_t reg_addr,
        uint8_t *data,
        uint8_t length
);

int hal_lis3dh_get_xyz(short *x,short *y,short *z);

bool hal_lis3dh_get_int1_status(void);
bool hal_lis3dh_get_int2_status(void);

u8_t LIS3DH_ReadReg(u8_t Reg, u8_t* Data);
void lis3dh_test(void);

#endif /* HAL_LIS3DH_H_ */
