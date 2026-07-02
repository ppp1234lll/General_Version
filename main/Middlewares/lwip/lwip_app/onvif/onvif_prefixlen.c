
#include "./onvif/onvif_prefixlen.h"
#include "./MALLOC/malloc.h"
#include "string.h"
#include "stdio.h"

// 由子网掩码算出前缀长度
int ipv4_str2prefixlen(char* ip_str) // 输入为字符串
{
    unsigned int ip_num = 0;
    int c1,c2,c3,c4;
    int cnt = 0;

    sscanf(ip_str, "%d.%d.%d.%d", &c1, &c2, &c3, &c4);
    ip_num = c1<<24 | c2<<16 | c3<<8 | c4;

    // fast...
    if (ip_num == 0xffffffff) return 32;
    if (ip_num == 0xffffff00) return 24;
    if (ip_num == 0xffff0000) return 16;
    if (ip_num == 0xff000000) return 6;

    // just in case
    for (int i = 0; i < 32; i++)
    {
        if ((ip_num<<i) & 0x80000000)
            cnt++;
        else
            break;
    }
    return cnt;
}

// 由前缀长度算出子网掩码
int ipv4_prefixlen2str(int prefixlen, uint8_t* ip_str) // 输出为数组
{
    unsigned int ip_num = 0;

    if (ip_str == NULL) return -1;
    if (prefixlen > 32) return -1;

    // fast...
    if (prefixlen == 8) // strcpy(ip_str, "255.0.0.0");
    {        
        ip_str[0] = 255;
        ip_str[1] = 0;    
        ip_str[2] = 0;
        ip_str[3] = 0;
    }
    if (prefixlen == 16) // strcpy(ip_str, "255.255.0.0");
    {
        ip_str[0] = 255;
        ip_str[1] = 255;    
        ip_str[2] = 0;
        ip_str[3] = 0;
    }
    if (prefixlen == 24) // strcpy(ip_str, "255.255.255.0");
    {
        ip_str[0] = 255;
        ip_str[1] = 255;    
        ip_str[2] = 255;
        ip_str[3] = 0;
    }
    if (prefixlen == 32) // strcpy(ip_str, "255.255.255.255");
    {
        ip_str[0] = 255;
        ip_str[1] = 255;    
        ip_str[2] = 255;
        ip_str[3] = 255;
    }

    // just in case
    for (int i = prefixlen, j = 31; i > 0; i--, j--)
    {
        ip_num += (1<<j);
    }
    //printf("ip_num: %08x\n", ip_num);
//    sprintf(ip_str, "%d.%d.%d.%d", (ip_num>>24)&0xff, (ip_num>>16)&0xff, (ip_num>>8)&0xff, ip_num&0xff);
    ip_str[0] = (ip_num>>24)&0xff;
    ip_str[1] = (ip_num>>16)&0xff;    
    ip_str[2] = (ip_num>>8)&0xff;
    ip_str[3] =  ip_num&0xff;
    
    return 0;
}







