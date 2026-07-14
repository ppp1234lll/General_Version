#include "./tool/inc/convert.h"
#include "math.h"

/*
*********************************************************************************************************
*    函 数 名: complement_to_original
*    功能说明: 补码转换为原码
*    形    参: 无
*    返 回 值: 无
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

/*
*********************************************************************************************************
*    函 数 名: hex_to_dec
*    功能说明: 字符串转十六进制（用于解析URL编码的特殊字符）
*    形    参: buff - 2字节字符串（如"2F"→0x2F）
*    返 回 值: 转换后的十六进制值
*********************************************************************************************************
*/
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
/*
*********************************************************************************************************
*    函 数 名: str_to_hex
*    功能说明: 字符转hex
*    形    参: 
*    返 回 值: 
*********************************************************************************************************
*/
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

/*
*********************************************************************************************************
*    函 数 名: uint64_to_str        
*    功能说明: 无符号64位整数转字符串
*    形    参: buff - 输出缓冲区
*    value - 输入无符号64位整数
*    返 回 值: 输出缓冲区指针
*********************************************************************************************************
*/
static char *uint64_to_str(char *buff, uint64_t value)
{
    char temp[21] = {0};
    uint8_t index = 0;

    if (value == 0)
    {
        *buff++ = '0';
        *buff = '\0';
        return buff;
    }

    while (value > 0)
    {
        temp[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0)
    {
        *buff++ = temp[--index];
    }

    *buff = '\0';
    return buff;
}
/*
*********************************************************************************************************
*    函 数 名: uint64_str_len        
*    功能说明: 无符号64位整数字符串长度
*    形    参: value - 输入无符号64位整数
*    返 回 值: 字符串长度
*********************************************************************************************************
*/
static uint8_t uint64_str_len(uint64_t value)
{
    uint8_t len = 1;

    while (value >= 10)
    {
        value /= 10;
        len++;
    }

    return len;
}
/*
*********************************************************************************************************
*    函 数 名: float_to_str         
*    功能说明: 浮点数转字符串
*    形    参: value - 输入浮点数
*    返 回 值: 字符串长度
*********************************************************************************************************
*/
int8_t float_to_str(float value, uint8_t precision, uint8_t *data, uint16_t len)
{
    static const uint64_t scale_table[] = {
        1ULL,
        10ULL,
        100ULL,
        1000ULL,
        10000ULL,
        100000ULL,
        1000000ULL,
        10000000ULL,
        100000000ULL,
        1000000000ULL,
        10000000000ULL
    };
    uint64_t scale = 0;
    uint64_t scaled_value = 0;
    uint64_t integer_part = 0;
    uint64_t fraction_part = 0;
    uint16_t need_len = 0;
    uint8_t is_negative = 0;
    uint8_t zero_count = 0;
    char *buff = (char *)data;

    if (data == NULL || len == 0)
    {
        return -1;
    }

    data[0] = '\0';

    if (precision > 10)
    {
        precision = 10;
    }

    scale = scale_table[precision];

    if (value < 0)
    {
        is_negative = 1;
        need_len++;
        value = 0 - value;
    }

    scaled_value = (uint64_t)(value * (float)scale + 0.5f);
    integer_part = scaled_value / scale;
    fraction_part = scaled_value % scale;
    need_len += uint64_str_len(integer_part);

    if (precision > 0)
    {
        need_len += (uint16_t)precision + 1;
    }

    need_len += 1;
    if (need_len > len)
    {
        return -1;
    }

    if (is_negative)
    {
        *buff++ = '-';
        *buff = '\0';
    }

    if (precision == 0)
    {
        uint64_to_str(buff, integer_part);
    }
    else
    {
        buff = uint64_to_str(buff, integer_part);
        *buff++ = '.';

        zero_count = precision;
        while (zero_count > 1 && fraction_part < scale_table[zero_count - 1])
        {
            *buff++ = '0';
            zero_count--;
        }

        uint64_to_str(buff, fraction_part);
    }
    return 0;
}

static const char g_base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
*********************************************************************************************************
*    函 数 名: base64_encode
*    功能说明: Base64 编码
*    形    参: input      : 输入数据
*              input_len  : 输入长度
*              output     : 输出缓冲区
*              output_size: 输出缓冲区大小
*    返 回 值: 编码后长度，失败返回 -1
*********************************************************************************************************
*/
int base64_encode_http(const uint8_t *input, uint32_t input_len, char *output, uint32_t output_size)
{
    uint32_t i, j;
    uint32_t output_len;

    if ((input == NULL) || (input_len == 0U) || (output == NULL) || (output_size == 0U))
    {
        return -1;
    }

    output_len = ((input_len + 2U) / 3U) * 4U;

    if ((output_len + 1U) > output_size)
    {
        return -1;
    }

    for (i = 0U, j = 0U; i < input_len; )
    {
        uint32_t a = input[i++];
        uint32_t b = (i < input_len) ? input[i++] : 0U;
        uint32_t c = (i < input_len) ? input[i++] : 0U;
        uint32_t triple = (a << 16U) | (b << 8U) | c;

        output[j++] = g_base64_chars[(triple >> 18U) & 0x3FU];
        output[j++] = g_base64_chars[(triple >> 12U) & 0x3FU];
        output[j++] = g_base64_chars[(triple >> 6U) & 0x3FU];
        output[j++] = g_base64_chars[triple & 0x3FU];
    }

    if ((input_len % 3U) == 1U)
    {
        output[output_len - 2U] = '=';
        output[output_len - 1U] = '=';
    }
    else if ((input_len % 3U) == 2U)
    {
        output[output_len - 1U] = '=';
    }

    output[output_len] = '\0';
    return (int)output_len;
}
