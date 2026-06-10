
#include "hal_lis3dh.h"
#include "lis3dh_driver.h" 
#include "lis3dh_iic.h"
#include "lis3dh_spi.h"
#include "appconfig.h"
/*
	8、3轴加速度计LIS3DH: (模拟IIC)，引脚分配为：  
		SCL:   PE7
		SDA:   PB1

*/
bool  USE_IIC_LIS3DH	= true;


void spi_cs_low()
{ 

}

void spi_cs_high()
{ 

}

uint8_t spi_access(uint8_t data)
{ 
	return LIS3DH_SPI_ReadWriteByte(data);
}

/*
*********************************************************************************************************
*	函 数 名: hal_lis3dh_interface_init
*	功能说明:接口初始化函数 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int hal_lis3dh_interface_init(void)
{ 
	if( USE_IIC_LIS3DH)
	{
		LIS3DH_IIC_Init();
		return 0;
	}
	else
	{
		LIS3DH_SPI_INIT();
		return 1;
	}
}

/*
*********************************************************************************************************
*	函 数 名: hal_lis3dh_init
*	功能说明: 初始化函数 
*	形    参: 
*	@iic_mode_sel 	: 0-IIC 1-SPI
*	返 回 值: 无
*********************************************************************************************************
*/
int hal_lis3dh_init(bool iic_mode_sel)
{  
	uint8_t value;

	USE_IIC_LIS3DH = iic_mode_sel;

	hal_lis3dh_interface_init();
//	REPEAT:
	//读LIS3DH寄存器，确认LIS3DH通信成功
	LIS3DH_ReadReg(LIS3DH_WHO_AM_I, &value) ;
//	printf("%d\r",value);
//	LIS3DH_ReadReg(0x1E, &value) ;
//	printf("%d\r",value);
//	LIS3DH_ReadReg(0x20, &value) ;
//	printf("%d\r",value);
	
	if(value != 0x33)
	{
//		goto REPEAT;
		return -1;
	}

	//复位LIS3DH内部寄存器
	LIS3DH_RebootMemory();
	for(volatile uint32_t i=0;i<1000000;i++);

	//设置LIS3DH采样率
	LIS3DH_SetODR(LIS3DH_ODR_50Hz) ;
//	LIS3DH_ReadReg(0x20, &value) ;
//	printf("%d\n",value);
	//LIS3DH工作模式
	LIS3DH_SetMode(LIS3DH_NORMAL);
	LIS3DH_SetFullScale(LIS3DH_FULLSCALE_8 );
	LIS3DH_SetHPFMode(LIS3DH_HPM_NORMAL_MODE_RES);
	LIS3DH_SetBDU(MEMS_ENABLE);
	LIS3DH_SetAxis(LIS3DH_X_ENABLE | LIS3DH_Y_ENABLE | LIS3DH_Z_ENABLE);

	/**
	 * enable BDU function
	 */
	LIS3DH_SetBDU(MEMS_ENABLE);

	/**
	 * click function configuration
	 */
	LIS3DH_SetClickCFG( LIS3DH_XS_ENABLE );//| LIS3DH_XS_ENABLE);
	LIS3DH_SetClickLIMIT(0x33) ;//127ms
	LIS3DH_SetClickTHS(20);
	LIS3DH_SetClickLATENCY(0xff);   //637ms
	LIS3DH_SetClickWINDOW(0xff);    //637ms


	/**
	 * high pass filter configuration
	 */
	//    LIS3DH_SetHPFMode(LIS3DH_HPM_NORMAL_MODE  ) ;
	//    LIS3DH_SetHPFCutOFF(LIS3DH_HPFCF_3  ) ;
	//    LIS3DH_SetFilterDataSel(MEMS_ENABLE  ) ;


	LIS3DH_SetInt1Pin(
			LIS3DH_CLICK_ON_PIN_INT1_ENABLE |
			LIS3DH_I1_INT1_ON_PIN_INT1_DISABLE |
			LIS3DH_I1_INT2_ON_PIN_INT1_DISABLE |
			LIS3DH_I1_DRDY1_ON_INT1_DISABLE |
			LIS3DH_I1_DRDY2_ON_INT1_DISABLE |
			LIS3DH_WTM_ON_INT1_DISABLE |
			LIS3DH_INT1_OVERRUN_DISABLE
	);

	LIS3DH_SetInt2Pin(
			LIS3DH_CLICK_ON_PIN_INT2_ENABLE |
			LIS3DH_I2_INT1_ON_PIN_INT2_DISABLE |
			LIS3DH_I2_INT2_ON_PIN_INT2_DISABLE |
			LIS3DH_I2_BOOT_ON_INT2_DISABLE |
			LIS3DH_INT_ACTIVE_HIGH );

	return 0;
}


/**
 *
 * @param x
 * @param y
 * @param z
 * @return
 */
int hal_lis3dh_get_xyz(short *x,short *y,short *z)
{
	AxesRaw_t aux_raw;

	LIS3DH_GetAccAxesRaw( &aux_raw );

	*x = aux_raw.AXIS_X;
	*y = aux_raw.AXIS_Y;
	*z = aux_raw.AXIS_Z;

	return 0;
}


int hal_lis3dh_spi_write
(
		uint8_t reg_addr,
		uint8_t const *data,
		uint8_t length
)
{
	spi_cs_low();
	spi_access( reg_addr & (~(0x01<<7)) );

	for(uint16_t i=0;i<length;i++)
	{
		spi_access( *data++ );
	}
	spi_cs_high();
	return 0;
}


int hal_lis3dh_spi_read
(
		uint8_t reg_addr,
		uint8_t *data,
		uint8_t length
)
{
	spi_cs_low();
	spi_access( reg_addr | (0x01<<7));
	for(uint16_t i=0;i<length;i++)
	{
		*data = spi_access(0x00);
	}
	spi_cs_high();
	return 0;
}

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/*******************************************************************************
 * Function Name		: LIS3DH_ReadReg
 * Description		: Generic Reading function. It must be fullfilled with either
 *			: I2C or SPI reading functions
 * Input			: Register Address
 * Output		: Data REad
 * Return		: None
 *******************************************************************************/
u8_t LIS3DH_ReadReg(u8_t Reg, u8_t* Data) {
	if (USE_IIC_LIS3DH)
	{
		HAL_IIC_EMU_Read  (  Reg,  Data,  1);
		return 1;
	}
	else
	{
		hal_lis3dh_spi_read
		(
				Reg,
				Data,
				1
		);

		//To be completed with either I2c or SPI reading function
		//i.e. *Data = SPI_Mems_Read_Reg( Reg );
		return 1;
	}
}


/*******************************************************************************
 * Function Name		: LIS3DH_WriteReg
 * Description		: Generic Writing function. It must be fullfilled with either
 *			: I2C or SPI writing function
 * Input			: Register Address, Data to be written
 * Output		: None
 * Return		: None
 *******************************************************************************/
u8_t LIS3DH_WriteReg(u8_t WriteAddr, u8_t Data) {
	if (USE_IIC_LIS3DH)
	{
		HAL_IIC_EMU_Write(  WriteAddr,   &Data,  1);
		return 1;
	}
	else
	{
		hal_lis3dh_spi_write
		(
				WriteAddr,
				&Data,
				1
		);

		//To be completed with either I2c or SPI writing function
		//i.e. SPI_Mems_Write_Reg(WriteAddr, Data);
		return 1;
	}

}

/*
*********************************************************************************************************
*	函 数 名: lis3dh_test
*	功能说明: 陀螺仪测试 
*	形 
*	返 回 值: 无
*********************************************************************************************************
*/
void lis3dh_test(void)
{
	MENS_XYZ_STATUS_T mens_t;
	uint8_t value;
	short x = 0;
	short y = 0;
	short z = 0;
	while(1)
	{
		
		LIS3DH_ReadReg(LIS3DH_WHO_AM_I, &value) ;
  	printf("%d\n",value);
		LIS3DH_ReadReg(0x1E, &value) ;
  	printf("%d\n",value);
		LIS3DH_ReadReg(LIS3DH_CTRL_REG1, &value) ;
  	printf("%d\n",value);
		hal_lis3dh_get_xyz(&x, &y, &z);
		printf("x=%d...y=%d...z=%d \n",x,y,z);
		
			/* 计算姿态值，并且判断箱体状态 */
	  mens_t.AcceX = x;
	  mens_t.AcceY = y;
	  mens_t.AcceZ = z;
	  sg_datacollec_t.attitude_acc = lean_check(&mens_t);
		printf("%f\n",sg_datacollec_t.attitude_acc);

		delay_ms(1000);	
	}
}



