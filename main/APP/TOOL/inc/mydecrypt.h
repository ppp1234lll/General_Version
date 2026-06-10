#ifndef __DECRYPT_H
#define __DECRYPT_H

#include "sys.h"
#include "stdio.h"

#define ENCRYPT_SIZE  64u   /* 块处理要加密或解密的数据时的块大小（以字节为单位） */


size_t AES_ECB_Decrypt(const uint8_t *P_pInput,size_t Inputlength,
											 const uint8_t *Key,size_t Keylength,uint8_t *P_pOutput);


size_t AES_CBC_Decrypt(	const uint8_t *P_pInput,size_t Inputlength,const uint8_t *Key,size_t Keylength,
												const uint8_t *IV,size_t IVlength,uint8_t *P_pOutput);

size_t RSA_Decrypt( const uint8_t *P_pModulus,size_t P_ModulusLen,
										const uint8_t *P_pExp,		size_t P_ExpLen,
										const uint8_t *P_pInput,	size_t P_InputLen,
										      uint8_t *P_pOutput);
										



#endif






	
	

	


