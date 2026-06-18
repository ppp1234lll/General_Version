
#include "hal_lis3dh.h"
#include "lis3dh_driver.h" 
#include "lis3dh_iic.h"
#include "lis3dh_spi.h"
#include "main.h"
#include <stdint.h>
/*
    8、3轴加速度计LIS3DH: (模拟IIC)，引脚分配为：  
        SCL:   PE7
        SDA:   PB1
*/

typedef struct {
    int32_t AcceX;                /*acceleration x*/
    int32_t AcceY;                /*acceleration y*/
    int32_t AcceZ;                /*acceleration z*/
} MENS_XYZ_STATUS_T;

bool  USE_IIC_LIS3DH    = true; // true-IIC false-SPI

#define spi_access(data)    LIS3DH_SPI_ReadWriteByte(data)
void spi_cs_low()
{ 

}

void spi_cs_high()
{ 

}

/*
*********************************************************************************************************
*    函 数 名: hal_lis3dh_interface_init
*    功能说明: 接口初始化函数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
int hal_lis3dh_interface_init(void)
{ 
    if( USE_IIC_LIS3DH )
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
*    函 数 名: hal_lis3dh_init
*    功能说明: 初始化函数
*    形    参: 
*    返 回 值: true-IIC false-SPI
*    返 回 值: 无
*********************************************************************************************************
*/
int hal_lis3dh_init(bool iic_mode_sel)
{  
    uint8_t value;

    USE_IIC_LIS3DH = iic_mode_sel;

    hal_lis3dh_interface_init();
//    REPEAT:
    //读LIS3DH寄存器，确认LIS3DH通信成功
    LIS3DH_ReadReg(LIS3DH_WHO_AM_I, &value) ;
    if(value != 0x33)
    {
        printf("LIS3DH communication error\n");
        return -1;
    }

    //复位LIS3DH内部寄存器
    LIS3DH_RebootMemory();
    for(volatile uint32_t i=0;i<1000000;i++);

    //设置LIS3DH采样率
    LIS3DH_SetODR(LIS3DH_ODR_50Hz) ;
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

/*
*********************************************************************************************************
*    函 数 名: LIS3DH_ReadReg
*    功能说明: Generic Reading function. It must be fullfilled with either
*    形    参: I2C or SPI reading functions
*    返 回 值: Register Address
* Output        : Data REad
*    ? ? ?: None
*********************************************************************************************************
*/
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


/*
*********************************************************************************************************
*    函 数 名: LIS3DH_WriteReg
*    功能说明: Generic Writing function. It must be fullfilled with either
*    形    参: I2C or SPI writing function
*    返 回 值: Register Address, Data to be written
* Output        : None
*    ? ? ?: None
*********************************************************************************************************
*/
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
*    函 数 名: lean_check
*    功能说明: 角度计算
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint16_t LIS3DH_GetAngle(void)
{
#if (configUSE_CHIP_ORIENTATION == 0)
    float ax_ref = 0;
    float ay_ref = 0;
    float az_ref = 1;
#elif (configUSE_CHIP_ORIENTATION == 1)
    float ax_ref = 0;
    float ay_ref = 1;
    float az_ref = 0;
#endif
    float norm_ref = 1;
    
    double temp = 0;
    double norm = 0;
    double dot_norm = 0;
//    bool Ret = false;
    float ax,ay,az,dot; 
    MENS_XYZ_STATUS_T tmp;
    static MENS_XYZ_STATUS_T acc;
    
    float angle;

    short acc_x = 0;
    short acc_y = 0;
    short acc_z = 0;
    hal_lis3dh_get_xyz(&acc_x, &acc_y, &acc_z);

    /*去除加速度计抖动,30mg变化视为抖动*/    
    if(abs(acc_x-acc.AcceX)>=30){
        tmp.AcceX = acc_x;
        acc.AcceX = acc_x;
    }else{
        tmp.AcceX = acc.AcceX;
    }
    if(abs(acc_y-acc.AcceY)>=30){
        tmp.AcceY = acc_y;
        acc.AcceY = acc_y;
    }else{
        tmp.AcceY = acc.AcceY;
    }

    if(abs(acc_z-acc.AcceZ)>=30){
        tmp.AcceZ = acc_z;
        acc.AcceZ = acc_z;
    }else{
        tmp.AcceZ = acc.AcceZ;
    }

    /*归一化处理*/
    temp = tmp.AcceX*tmp.AcceX + tmp.AcceY*tmp.AcceY + tmp.AcceZ*tmp.AcceZ;
    norm = (float)sqrt(temp);
    if (norm == 0.0) norm = 0.000001;  //avoid Nan happen
    ax = tmp.AcceX / norm;
    ay = tmp.AcceY / norm;
    az = tmp.AcceZ / norm;

    /*检测倾角是否超限40°*/

    /*ax_ref,ay_ref,az_ref
    为归一化后的参考向量的分量，可以自己设置，如定义为ax_ref=0,ay_ref=0,az_ref=1，表示加速度计正立放置
    也可以让终端程序稳定运行一段时间后，取一个加速度值作为参考向量，这样就可以不论物体怎么摆放，当他稳定后就可以确定初始向量*/
    dot = ax*ax_ref+ay*ay_ref+az*az_ref;
    
    dot_norm = (float)sqrt((double)(ax*ax) + (double)(ay*ay) + (double)(az*az));
    if (dot_norm == 0.0) dot_norm = 0.000001;
    //针对不少人的疑问:norm_ref和dot_norm 一样是归一化后的向量的模，norm_ref是参考的向量而已,可以在开机时记录，或者在某个特定时刻指定,计算公式参考dot_norm ,需要注意的是参考向量也需要归一化
    angle = acos(dot/(dot_norm*norm_ref))*180/3.14;//弧度转化为角度，
    
    return (uint16_t)(angle);
}


/*
*********************************************************************************************************
*    函 数 名: lis3dh_test
*    功能说明: 陀螺仪测试
*    形    参: 无
*    返 回 值: 
*********************************************************************************************************
*/
void lis3dh_test(void)
{
    uint16_t value;
    short x = 0;
    short y = 0;
    short z = 0;
    while(1)
    {
        hal_lis3dh_get_xyz(&x, &y, &z);
        printf("x=%d...y=%d...z=%d \n",x,y,z);

        /* 计算姿态值，并且判断箱体状态 */
        value = LIS3DH_GetAngle();
        printf("%d\n",value);

        delay_ms(1000);    
    }
}



