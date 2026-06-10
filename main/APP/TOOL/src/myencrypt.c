#include "myencrypt.h"
#include "cmox_crypto.h"
#include "md5.h"
#include "appconfig.h"
#include "malloc.h"

size_t AES_ECB_Encrypt(const uint8_t *P_pInput,size_t Inputlength,
											 const uint8_t *Key,size_t Keylength,uint8_t *P_pOutput)
{
	cmox_ecb_handle_t ECB_Ctx; /* ECB context handle */
  cmox_cipher_handle_t *cipher_ctx;
	uint32_t index = 0;
	cmox_cipher_retval_t retval;
	size_t computed_size = 0;
	
  cipher_ctx = cmox_ecb_construct(&ECB_Ctx, CMOX_AES_ECB_ENC);
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

size_t AES_CBC_Encrypt(	const uint8_t *P_pInput,size_t Inputlength,const uint8_t *Key,size_t Keylength,
												const uint8_t *IV,size_t IVlength,uint8_t *P_pOutput)
{
	size_t computed_size = 0;
	cmox_cipher_retval_t retval;

  retval = cmox_cipher_encrypt(CMOX_AES_CBC_ENC_ALGO,                  /* Use AES CBC algorithm */
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


size_t SHA_Encrypt(cmox_hash_algo_t P_algo,const uint8_t *P_pInput,size_t Inputlength,uint8_t *P_pOutput)
{
	size_t computed_size = 0;
	cmox_cipher_retval_t retval;

  retval = cmox_hash_compute(P_algo,                  /* 使用 SHA 算法 */
                             P_pInput, Inputlength, 	/* Message to digest */
                             P_pOutput,            		/* 用于接收摘要数据的数据缓冲区 */
                             CMOX_SHA256_SIZE,         /* 预期摘要大小 */
                             &computed_size);          /* 计算摘要的大小 */

  /* Verify API returned value */
  if (retval != CMOX_CIPHER_SUCCESS)
  {
    if(ENCRYPT_DEBUG) printf("error\n");
  }

	if(ENCRYPT_DEBUG) printf("SHA256 encrypt data：\r\n");
	
	for(int i=0; i<computed_size; i++)
	{
		if(ENCRYPT_DEBUG) printf("%02x",P_pOutput[i]);
	}
	if(ENCRYPT_DEBUG) printf("\r\n");		

	return computed_size;
}

	
/**
* @brief  MD5 HASH digest compute example.
* @param  InputMessage: pointer to input message to be hashed.
* @param  InputMessageLength: input data message length in byte.
* @param  MessageDigest: pointer to output parameter that will handle message digest
* @param  MessageDigestLength: pointer to output digest length.
* @retval error status: can be HASH_SUCCESS if success or one of
*         HASH_ERR_BAD_PARAMETER, HASH_ERR_BAD_CONTEXT,
*         HASH_ERR_BAD_OPERATION if error occured.
*/
size_t MD5_HASH_Encrypt(uint8_t* P_pInput, size_t Inputlength,uint8_t *P_pOutput)
{
	MD5_CTX md5c; 

	MD5Init(&md5c); //初始化

	MD5Update(&md5c,P_pInput,Inputlength);  

	MD5Final(&md5c,P_pOutput); 

	if(ENCRYPT_DEBUG) printf("md5 encrypt data：\r\n");
	for(int i=0;i<16;i++)
	{
		if(ENCRYPT_DEBUG) printf("%02x",P_pOutput[i]);
	}
	if(ENCRYPT_DEBUG) printf("\r\n");		

  return 0;
}

	
/**
  * @brief  RSA Encryption with PKCS#1v1.5
  * @param  P_pPubKey The RSA public key structure, already initialized
  * @param  P_pInputMessage Input Message to be signed
  * @param  P_MessageSize Size of input message
  * @param  P_pOutput Pointer to output buffer
  * @retval error status: can be RSA_SUCCESS if success or one of
  * RSA_ERR_BAD_PARAMETER, RSA_ERR_MESSAGE_TOO_LONG, RSA_ERR_BAD_OPERATION
*/
size_t RSA_Encrypt( const uint8_t *P_pModulus,size_t P_ModulusLen,
										const uint8_t *P_pExp,		size_t P_ExpLen,
										const uint8_t *P_pInput,	size_t P_InputLen,
										const uint8_t *P_pRandom, size_t P_RandomLen,
										      uint8_t *P_pOutput)
{
	cmox_rsa_handle_t Rsa_Ctx;  /* RSA context */
	cmox_rsa_key_t Rsa_Key;  		/* RSA key  秘钥*/
  cmox_rsa_retval_t retval;
  size_t computed_size;	
	uint8_t working_buffer[7000];
	
//	working_buffer = mymalloc(SRAMCCM,7000);  // 申请内存
//	memset(working_buffer,0,7000);
//	if(working_buffer == NULL)
//		if(ENCRYPT_DEBUG) printf("working_buffer MALLOC error\n");
	
  /* 构造一个 RSA 上下文，指定数学实现和工作缓冲区以供以后处理 */
  /* Note: CMOX_RSA_MATH_FUNCS refer to the default mathematics implementation
   * selected in cmox_default_config.h. To use a specific implementation, user can
   * directly choose:
   * - CMOX_MATH_FUNCS_SMALL to select the mathematics small implementation
   * - CMOX_MATH_FUNCS_FAST to select the mathematics fast implementation
   */
  cmox_rsa_construct(&Rsa_Ctx, CMOX_RSA_MATH_FUNCS, CMOX_MODEXP_PUBLIC, working_buffer, sizeof(working_buffer));

  /* 使用公钥表示形式填写 RSA 密钥结构 */
  retval = cmox_rsa_setKey(&Rsa_Key,                                      /* RSA key structure to fill */
                           P_pModulus, P_ModulusLen,                      /* Key modulus */
                           P_pExp, P_ExpLen);     /* Public key exponent */

  /* Verify API returned value */
  if (retval != CMOX_RSA_SUCCESS)
  {
    if(ENCRYPT_DEBUG) printf("cmox_rsa_setKey error\n");
  }

  /*直接计算传递所有所需参数的加密消息*/
  retval = cmox_rsa_pkcs1v15_encrypt(&Rsa_Ctx,                      /* RSA context */
                                     &Rsa_Key,                      /* RSA key to use */
                                     P_pInput, sizeof(P_InputLen),  /* Message to encrypt */
                                     P_pRandom, sizeof(P_RandomLen),/* Random seed */
                                     P_pOutput, &computed_size);  /* Data buffer to receive encrypted text */

  /* Verify API returned value */
  if (retval != CMOX_RSA_SUCCESS)
  {
    if(ENCRYPT_DEBUG) printf("cmox_rsa_pkcs1v15_encrypt error\n");
  }
  cmox_rsa_cleanup(&Rsa_Ctx); 		 /* Cleanup context */	
//	myfree(SRAMCCM,working_buffer);   // 释放内存
  return computed_size;
}






















