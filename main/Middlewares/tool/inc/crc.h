#ifndef _CRC_H_
#define _CRC_H_

/*
*********************************************************************************************************
*    函 数 名: calc_crc8
*    功能说明: 用于CRC8校验
*    形    参: data 需要校验的首地址  len  数据的长度
*    返 回 值: 
* Note      :  // 按照多项式 X^8+X^2+X^1+1 生成。
*********************************************************************************************************
*/
unsigned char calc_crc8(const unsigned char *data, const unsigned int data_len);

unsigned short usMBCRC16( unsigned char * pucFrame, unsigned short usLen );
unsigned short usSumFunction(unsigned char * pucFrame, unsigned short usLen);

extern unsigned short CRC16_MODBUS(unsigned char *data, unsigned int datalen);

#endif
