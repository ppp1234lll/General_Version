#include "bsp_rng.h"
#include "bsp.h"
#include <stdint.h>

/*
*********************************************************************************************************
*    函 数 名: bsp_InitRNG
*    功能说明: 初始化RNG
*    形    参: 无
*    返 回 值: 0,成功;1,失败
*********************************************************************************************************
*/
uint8_t bsp_InitRNG(void)
{
    uint32_t timeout = 0;
    FlagStatus trng_flag = RESET;
    /* TRNG module clock enable */
    rcu_periph_clock_enable(RCU_TRNG);

    /* TRNG registers reset */
    trng_deinit();
    trng_enable();

    /* check wherther the random data is valid */
    do{
        timeout++;
        trng_flag = trng_flag_get(TRNG_FLAG_DRDY);
    }while((RESET == trng_flag) &&(0xFFFF > timeout));

    if(RESET == trng_flag)
    {
        /* ready check timeout */
        printf("Error: TRNG can't ready \r\n");
        trng_flag = trng_flag_get(TRNG_FLAG_CECS);
        printf("Clock error current status: %d \r\n", trng_flag);
        trng_flag = trng_flag_get(TRNG_FLAG_SECS);
        printf("Seed error current status: %d \r\n", trng_flag);  
        return 1;
    }
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: RNG_Get_RandomNum
*    功能说明: 得到随机数
*    形    参: 无
*    返 回 值: 获取到的随机数
*********************************************************************************************************
*/
uint32_t RNG_Get_RandomNum(void)
{     
    while((trng_flag_get(TRNG_FLAG_DRDY)) == RESET);    //等待随机数就绪  
    return trng_get_true_random_data();
}

/*
*********************************************************************************************************
*    函 数 名: RNG_Get_RandomRange
*    功能说明: 得到某个范围内的随机数。
*    形    参: min,max,最小,最大值
*    返 回 值: 得到的随机数(rval),满足:min<=rval<=max
*********************************************************************************************************
*/
int RNG_Get_RandomRange(int min,int max)
{ 
    return RNG_Get_RandomNum()%(max-min+1)+min;
}

#ifdef MBEDTLS_ENTROPY_HARDWARE_ALT

#include "mbedtls/entropy_poll.h"

extern RNG_HandleTypeDef hrng;

int mbedtls_hardware_poll( void *Data, unsigned char *Output, size_t Len, size_t *oLen )
{
  uint32_t index;
  uint32_t randomValue;
        
  for (index = 0; index < Len/4; index++)
  {
        RNG_Get_RandomRange(&hrng, &randomValue);
        *oLen += 4;
        memset(&(Output[index * 4]), (int)randomValue, 4);
  }
  
  return 0;
}


#endif /*MBEDTLS_ENTROPY_HARDWARE_ALT*/

/*
*********************************************************************************************************
*    函 数 名: rng_test
*    功能说明: RNG测试
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void rng_test(void)
{
    while(1)
    {
        printf("%u", RNG_Get_RandomNum());
        printf("\r\n\r\n");
        delay_ms(200);
    
        printf("%u", RNG_Get_RandomRange(0, 9));
        printf("\r\n\r\n");
        delay_ms(1000);        
    }
}




