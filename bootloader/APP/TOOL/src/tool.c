#include "./TOOL/inc/tool.h"
#include "math.h"

/*
*********************************************************************************************************
*	函 数 名: complement_to_original
*	功能说明: 补码转换为原码
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint32_t complement_to_original(uint32_t data)
{
	uint32_t temp;
	if((data&0x00800000) == 0x00800000)  // 判断最高位是否为0，Bit[23]为符号位，Bit[23]=0为正
	{
		data &= 0x007FFFFF;  // 清除符号位 	
		temp =~data;         // 反码
		data = temp & 0x007FFFFF;  // 清除左边多余位
		data += 1;				
	}
	else  // 当前为负功
	{
		data &= 0x007FFFFF;  // 清除符号位
	}
	return data;
}

/************************************************************
*
* Function name	: hex_to_dec
* Description	: 字符串转十六进制（用于解析URL编码的特殊字符）
* Parameter		: buff - 2字节字符串（如"2F"→0x2F）
* Return		: 转换后的十六进制值
*	
************************************************************/
int8_t hex_to_dec(char c)
{
	
    if ('0' <= c && c <= '9') 
    {
        return c - '0';
    } 
    else if ('a' <= c && c <= 'f')
    {
        return c - 'a' + 10;
    } 
    else if ('A' <= c && c <= 'F')
    {
        return c - 'A' + 10;
    } 
    else 
    {
        return -1;
    }
}
/************************************************************
*
* Function name	: str_to_hex
* Description	: 字符转hex
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t str_to_hex(uint8_t *buff)
{
	uint8_t temp;
	
	if(buff[0] >= 'A' && buff[0] <= 'F') {
		temp = buff[0]-'A' + 0x0A;
	} else if(buff[0] >= '0' && buff[0] <= '9') {
		temp = buff[0]-'0';
	}
	temp = temp<<4;
	if(buff[1] >= 'A' && buff[1] <= 'F') {
		temp |= (buff[1]-'A' + 0x0A);
	} else if(buff[1] >= '0' && buff[1] <= '9') {
		temp |= (buff[1]-'0');
	}
	
	return temp;
}



/*功   能： 浮点数保留小数位数四舍五入*/
/*输入参数: 浮点数指针 ， 小数位数*/
void PrecisionHandle(float *value,unsigned char Wrange)
{
	int sign = 1;
 
	if(*value < 0)
	{
		sign = -1;
	}
 
	*value += ((sign * pow(10,((-1) * (Wrange + 1)))) * 5);
}
 
 
/*功   能：浮点数转换成字符串*/
/*输入参数:浮点数 ，输出整数位数，小数位数，输出字符串指针，字符串长度*/
/*返回值  ：-1 错误	0 正常*/
signed char FloatToString(float value,uint8_t int_width,uint8_t Wrange,uint8_t * data,uint8_t len)
{
    signed char result = 0;
    signed char sign = 1;
    uint8_t i = 0;
    int int_value = 0;
	if(value < 0)
	{
		sign = -1;
	}
	
    if(Wrange == 0)
    {
       if( (int_width + 1) > len || int_width == 0)
       {
         result = -1;
       }
       int_value = (int)(sign * value );
       if(sign == -1)
       {
           for(i = int_width;i > 0 ;i--)
           {
                data[i] = (int_value % 10) + '0';
                int_value = int_value / 10;
           }
           data[0] = '-';
       }
       else
       {
           for(i = (int_width-1);i > 0 ;i--)
           {
                data[i] = (int_value % 10) + '0';
                int_value = int_value / 10;
           }
           data[0] = (int_value % 10) + '0';
       }          
    }
    else
    {
        if( (int_width + Wrange +1) > len || int_width == 0)  
        {
            result = -1;
        }
        PrecisionHandle(&value,Wrange);
        int_value = (int)(sign * value * ( pow(10,Wrange)));
        if(sign == -1)
        {
            for(i = (int_width +Wrange +1);i > 0 ;i--)
            {
                if(i == int_width + 1)
                {
                    data[i] = '.'; 
                    continue;
                }
                data[i] = (int_value % 10) + '0';
                int_value = int_value / 10;  
            }
            data[0] = '-';
        }
        else
        {
           for(i = (int_width +Wrange);i > 0 ;i--)
           {  
              if(i == int_width)
              {
                data[i] = '.'; 
                continue;
              }
              data[i] = (int_value % 10) + '0';
              int_value = int_value / 10;
           }
           data[0] = (int_value % 10) + '0';
        }                      
    }
    
    return result;
}

