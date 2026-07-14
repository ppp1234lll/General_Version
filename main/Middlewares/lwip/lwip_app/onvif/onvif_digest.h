#ifndef __ONVIF_DIGEST_H
#define __ONVIF_DIGEST_H

#include "./SYSTEM/sys/sys.h"

#define NONCE_LENGTH          22      // 定义nonce字节长度
#define NONCE_B64_LENGTH  (((NONCE_LENGTH + 2) / 3) * 4 + 1)
#define CREATED_LENGTH      32    // 定义时间戳字节长度
#define PASSWORD_LENGTH   32    // 定义密码鉴权字节长度

int charIndex(const char* str, char c);
int base64_encode(unsigned char * sourcedata, char * base64,int len);
int base64_decode(char * base64, unsigned char * dedata);

void StrToHex(char *pbDest, char *pbSrc);
void stohex(char *buf, char *dst);

int sha1(const char *input, uint8_t output[20], unsigned size);

int appendArray(char *arr1,int len1, char* arr2,int len2,char* arr3);

void GetNonce(char *nonce);
void GetCreated(char *datetime);
int buildBytes(char *nonce,char *created,char *password,char *pwd_dat);
void GetPasswordDigest(char *nonce,char *created,char *password,char *pwd_digest);

void HttpAuthTest(void);

#endif
