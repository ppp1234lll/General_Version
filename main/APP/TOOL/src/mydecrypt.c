#include "mydecrypt.h"
#include "cmox_crypto.h"
#include "appconfig.h"
#include "bsp.h"
#include "malloc.h"

size_t AES_ECB_Decrypt(const uint8_t *P_pInput,size_t Inputlength,
											 const uint8_t *Key,size_t Keylength,uint8_t *P_pOutput)
{
	cmox_ecb_handle_t ECB_Ctx; /* ECB context handle */
  cmox_cipher_handle_t *cipher_ctx;
	uint32_t index = 0;
	cmox_cipher_retval_t retval;
	size_t computed_size = 0;
	
  cipher_ctx = cmox_ecb_construct(&ECB_Ctx, CMOX_AES_ECB_DEC);
  if (cipher_ctx == NULL)
  {
    if(ENCRYPT_DEBUG) printf("error\n");
  }

  if (cmox_cipher_init(cipher_ctx) != CMOX_CIPHER_SUCCESS)/* Initialize the cipher context */
  {
    if(ENCRYPT_DEBUG) printf("error\n");
  }
	if(ENCRYPT_DEBUG) printf("ECB encrypt data：\r\n");
	
  /* 在上下文中设置加密密钥 */
  retval = cmox_cipher_setKey(cipher_ctx, Key, Keylength);  /* AES key to use */
  if (retval != CMOX_CIPHER_SUCCESS)
  {
    if(ENCRYPT_DEBUG) printf("error\n");
  }

  /* 通过附加 CHUNK_SIZE 字节块，分多个步骤加密明文 */
  for (index = 0; index < (Inputlength - ENCRYPT_SIZE); index += ENCRYPT_SIZE)
  {
    retval = cmox_cipher_append(cipher_ctx,
                                &P_pInput[index], ENCRYPT_SIZE,    /* 要加密的明文块 */
                                P_pOutput, &computed_size);        /* 用于接收生成的密文块的数据缓冲区 */

    /* Verify API returned value */
    if (retval != CMOX_CIPHER_SUCCESS)
    {
      if(ENCRYPT_DEBUG) printf("error\n");
    }

		for(int i=0; i<computed_size; i++)
		{
			if(ENCRYPT_DEBUG) printf("%02x",P_pOutput[i]);
		}
		if(ENCRYPT_DEBUG) printf("\r\n");
  }
  /* 如果需要，对最后一部分进行加密处理 */
  if (index < Inputlength)
  {
    retval = cmox_cipher_append(cipher_ctx,
                                &P_pInput[index],
                                Inputlength - index,     /* 要加密的明文的最后一部分 */
                                P_pOutput, &computed_size);   /* 接收生成的密文最后一部分的数据缓冲区 */

    /* Verify API returned value */
    if (retval != CMOX_CIPHER_SUCCESS)
    {
      if(ENCRYPT_DEBUG) printf("error\n");
    }
		for(int i=0; i<computed_size; i++)
		{
			if(ENCRYPT_DEBUG) printf("%02x",P_pOutput[i]);
		}
		if(ENCRYPT_DEBUG) printf("\r\n");		
  }

  /* Cleanup the context */
  retval = cmox_cipher_cleanup(cipher_ctx);
  if (retval != CMOX_CIPHER_SUCCESS)
  {
    if(ENCRYPT_DEBUG) printf("error\n");
  }
	return computed_size;
}

size_t AES_CBC_Decrypt(	const uint8_t *P_pInput,size_t Inputlength,const uint8_t *Key,size_t Keylength,
												const uint8_t *IV,size_t IVlength,uint8_t *P_pOutput)
{
	size_t computed_size = 0;
	cmox_cipher_retval_t retval;

  retval = cmox_cipher_decrypt(CMOX_AES_CBC_DEC_ALGO,                  /* Use AES CBC algorithm */
                               P_pInput, Inputlength,           /* Plaintext to encrypt */
                               Key, Keylength,                       /* AES key to use */
                               IV, IVlength,                         /* Initialization vector */
                               P_pOutput, &computed_size);   /* Data buffer to receive generated ciphertext */

  /* Verify API returned value */
  if (retval != CMOX_CIPHER_SUCCESS)
  {
    if(ENCRYPT_DEBUG) printf("error\n");
  }

	if(ENCRYPT_DEBUG) printf("CBC encrypt data：\r\n");
	for(int i=0; i<computed_size; i++)
	{
		if(ENCRYPT_DEBUG) printf("%02x",P_pOutput[i]);
	}
	if(ENCRYPT_DEBUG) printf("\r\n");		

	return computed_size;
}


/**
  * @brief  RSA Decryption with PKCS#1v1.5
  * @param  P_pPrivKey The RSA private key structure, already initialized
  * @param  P_pInputMessage Input Message to be signed
  * @param  P_MessageSize Size of input message
  * @param  P_pOutput Pointer to output buffer
  * @retval error status: can be RSA_SUCCESS if success or RSA_ERR_GENERIC in case of fail
*/ 
size_t RSA_Decrypt( const uint8_t *P_pModulus,size_t P_ModulusLen,
										const uint8_t *P_pExp,		size_t P_ExpLen,
										const uint8_t *P_pInput,	size_t P_InputLen,
										      uint8_t *P_pOutput)
{
	cmox_rsa_handle_t Rsa_Ctx;  /* RSA context */
	cmox_rsa_key_t Rsa_Key;  		/* RSA key  秘钥*/
  cmox_rsa_retval_t retval;
  size_t computed_size;	
	uint8_t *working_buffer;
	
	working_buffer = mymalloc(SRAMCCM,7000);  // 申请内存
//	memset(working_buffer,0,7000);
	if(working_buffer == NULL)
		if(ENCRYPT_DEBUG) printf("working_buffer MALLOC error\n");

  /* 构造一个 RSA 上下文，指定数学实现和工作缓冲区以供以后处理 */
  /* 注意：CMOX_RSA_MATH_FUNCS参考 cmox_default_config.h 中选择的默认数学实现。要使用特定的实现，用户可以直接选择
   * - CMOX_MATH_FUNCS_SMALL to select the mathematics small implementation
   * - CMOX_MATH_FUNCS_FAST to select the mathematics fast implementation
   */
  /* 注意：CMOX_MODEXP_PRIVATE参考 cmox_default_config.h 中选择的默认模块化幂实现。要使用特定的实现，用户可以直接选择：
   * - CMOX_MODEXP_PRIVATE_LOWMEM  to select the modular exponentiation low RAM usage implementation
   * - CMOX_MODEXP_PRIVATE_MIDMEM to select the modular exponentiation mid RAM usage implementation
   * - CMOX_MODEXP_PRIVATE_HIGHMEM to select the modular exponentiation high RAM usage implementation
   */
  cmox_rsa_construct(&Rsa_Ctx, CMOX_RSA_MATH_FUNCS, CMOX_MODEXP_PRIVATE, working_buffer, sizeof(working_buffer));

  /* 使用常规私钥表示形式填写 RSA 密钥结构 */
  retval = cmox_rsa_setKey(&Rsa_Key,                 /* 要填充的 RSA 密钥结构 */
                           P_pModulus, P_ModulusLen, /* 私钥模数 */
                           P_pExp, P_ExpLen);   			/* 私钥指数 */

  /* 验证 API 返回值 */
  if (retval != CMOX_RSA_SUCCESS)
  {
    if(ENCRYPT_DEBUG) printf("cmox_rsa_setKey error\n");
  }

  /* 直接计算传递所有必要参数的明文消息 */
  retval = cmox_rsa_pkcs1v15_decrypt(&Rsa_Ctx,               /* RSA context */
                                     &Rsa_Key,               /* RSA key to use */
                                     P_pInput, P_InputLen,   /* 加密消息 */
                                     P_pOutput, &computed_size);   /* 用于接收明文的数据缓冲区 */

  /* Verify API returned value */
  if (retval != CMOX_RSA_SUCCESS)
  {
    if(ENCRYPT_DEBUG) printf("cmox_rsa_pkcs1v15_decrypt error\n");
  }
	
  /* Cleanup context */
  cmox_rsa_cleanup(&Rsa_Ctx);
	myfree(SRAMCCM,working_buffer);   // 释放内存	
  return computed_size;
}


	
	

	


