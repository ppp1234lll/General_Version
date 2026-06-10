#include "de_encode.h"
#include "includes.h"
#include "malloc.h"
#include "SHA1.h"

// 全局常量定义
const char * base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char padding_char = '=';


/*base63编码
* const unsigned char * sourcedata， 源数组
* char * base64 ，码字保存
* len      原数组长度（注意0x00）
*/
int base64_encode(unsigned char * sourcedata, char * base64,int len)
{
	int i=0, j=0;
	unsigned char trans_index=0;    // 索引是8位，但是高两位都为0

	for (; i < len; i += 3)		// 每三个一组，进行编码
	{
		// 要编码的数字的第一个
		trans_index = ((sourcedata[i] >> 2) & 0x3f);
		base64[j++] = base64char[(int)trans_index];
		// 第二个
		trans_index = ((sourcedata[i] << 4) & 0x30);
		if (i + 1 < len)
		{
				trans_index |= ((sourcedata[i + 1] >> 4) & 0x0f);
				base64[j++] = base64char[(int)trans_index];
		}
		else
		{
			base64[j++] = base64char[(int)trans_index];
			base64[j++] = padding_char;
			base64[j++] = padding_char;
			break;   // 超出总长度，可以直接break
		}
		// 第三个
		trans_index = ((sourcedata[i + 1] << 2) & 0x3c);
		if (i + 2 < len)// 有的话需要编码2个
		{ 
			trans_index |= ((sourcedata[i + 2] >> 6) & 0x03);
			base64[j++] = base64char[(int)trans_index];
			trans_index = sourcedata[i + 2] & 0x3f;
			base64[j++] = base64char[(int)trans_index];
		}
		else
		{
			base64[j++] = base64char[(int)trans_index];
			base64[j++] = padding_char;
			break;
		}
   }
	base64[j] = '\0'; 
	return 0;
}

/* Base64解密 */
int base64_decode(char *base64, unsigned char * deData)
{
	int i = 0, j = 0;
	int len =0;;
	int trans[4] = {0};

	for(i=0;i<strlen(base64);i++)
	{
		if(base64[i] == '=')
			break;
		else
			len++;
	}
	len = len*6 / 8;    // 获取deData的长度，防止0x00出现错误
	
	for (i=0; base64[i] != '\0'; i += 4)
	{
		//获取第一与第二个字符，在base表中的索引值
		trans[0] = charIndex(base64char, base64[i]);
		trans[1] = charIndex(base64char, base64[i+1]);
 
		// 1/3
		deData[j++] = ((trans[0] << 2) & 0xfc) | ((trans[1] >> 4) & 0x03);
		if (base64[i + 2] == '=')
			continue;
		else
			trans[2] = charIndex(base64char, base64[i + 2]);
 
		// 2/3
		deData[j++] = ((trans[1] << 4) & 0xf0) | ((trans[2] >> 2) & 0x0f);
		if (base64[i + 3] == '=')
			continue;
		else
			trans[3] = charIndex(base64char, base64[i + 3]);
 
		// 3/3
		deData[j++] = ((trans[2] << 6) & 0xc0) | (trans[3] & 0x3f);
	}
 
	deData[j] = '\0';
	return len;
}

/** 在字符串中查询特定字符位置索引
* const char *str ，字符串
* char c，要查找的字符
*/
/* 在字符串中查询特定字符的索引 */
int charIndex(const char* str, char c)
{
	const char *pIndex = strchr(str, c);
	if(pIndex == NULL)
		return -1;
	else
		return (pIndex - str);
}


/// 将随机的16位字节数据加密成nonce
void GetNonce(char *nonce)
{
  uint8_t   Flagrand[NONCE_LENGTH];
	uint8_t i;
	uint8_t *sourcedata = Flagrand ;
	// 生成uuid,为了保证每次搜索的时候MessageID都是不相同的！因为简单，直接取了随机值
	for(i=0; i<NONCE_LENGTH; i++)
	{
		Flagrand[i] = rand()%90 + 10; //保证2位整数 
	}
	base64_encode((uint8_t *)sourcedata, nonce,NONCE_LENGTH);	
} 


void StrToHex(char *pbDest, char *pbSrc)
{
  char h1,h2;
  char s1,s2;
  int i;
	int nLen = strlen(pbSrc);
	for (i=0; i<nLen/2; i++)
	{
		h1 = pbSrc[2*i];
		h2 = pbSrc[2*i+1];

		s1 = toupper(h1) - 0x30; //toupper 转换为大写字母
		if (s1 > 9)
			s1 -= 7;
		
		s2 = toupper(h2) - 0x30;
		if (s2 > 9)
			s2 -= 7;
		pbDest[i] = s1*16 + s2;
	}
}


void stohex(char *buf, char *dst)
{
	char buf_temp[1024] = {0};
	int x = 0;
	unsigned long i;
	while(*buf != '\0')
	{
		if(*buf == '\\')
		{
			strcpy(buf_temp,buf);
			*buf_temp = '0';
			*(buf_temp + 4) = '\0';
			i = strtoul(buf_temp, NULL, 16);
			dst[x] = i;
			buf += 3;
		}
		else
			dst[x] = *buf;
		
		x++;
		buf++;
	}	
	dst[x] = '\0';
}

/*
* 进行sha1算法运算
* @input 输入的字符串
* @output 输出的字符串
* @size 输入字符串长度
* return  0 成功，-1 失败
*/
int sha1(const char *input, uint8_t output[20], unsigned size)
{
	int  err;
	SHA1Context sha;

	err = SHA1Reset(&sha);
	err = SHA1Input(&sha,(const unsigned char *) input, size);
	if (err){
//		printf("SHA1Input Error %d.\n", err);
		return -1;
	}
	err = SHA1Result(&sha, output);
	if (err){
//		printf("SHA1Result Error %d, could not compute message digest.\n",err );
		return -1;
	} 
	return 0;
}


int appendArray(char *arr1,int len1, char* arr2,int len2,char* arr3)//此处的data1[]本质上是一个指针
{
	int i = 1;//用来计数
	
	memcpy(arr3,arr1,len1);//把源数组的内容拷贝到目标数组 
	arr3 = arr3 + len1;//此处把data1指向str1数组的末端
	while (i <= len2)//刚好循环的次数是str2的长度
	{
		*arr3 = *arr2;//把str2的值赋在str1的末端
		arr3++;
		arr2++;
		i++;
	}
	return len1+len2;
}

int buildBytes(char *nonce,char *created,char *password,char *pwd_dat)
{
	int length;   // 字节长度
	char nonceBytes[22];
	char time[30];
	char pwd[30];
	
	// 第一步：对nonce进行解码
	length = base64_decode(nonce,(uint8_t*)nonceBytes);   // nonce 解码	

	stohex(created,time);	// 第二步：将字符串created转为数组形式
	stohex(password,pwd); // 第三步：将字符串password转为数组形式	
	
	// 第四步：将数组nonceBytes、time、pwd进行拼接
	length = appendArray(nonceBytes,length,time,strlen(time),pwd_dat);
	length = appendArray(pwd_dat,length,pwd,strlen(pwd),pwd_dat);  
	
	return length;
}

// 计算公式 Digest = B64E/NCODE( SHA1( B64DECODE( Nonce ) + Date + Password ) ),注意这里的加密不是使用字符串进行加密，而是字节数组
void GetPasswordDigest(char *nonce,char *created,char *password,char *pwd_digest)
{	
	int length;
	char sha1_encode[20];
	char pwd_dat[100];
	
	length = buildBytes(nonce,created,password,pwd_dat);
	sha1(pwd_dat,(uint8_t*)sha1_encode,length);
	base64_encode((uint8_t*)sha1_encode, pwd_digest,strlen(sha1_encode));
}

// ONVIF 鉴权测试（抓包工具）
//void HttpAuthTest(void)
//{
//	char pwd_digest[] = "o5oi6QnIdzuL40Ww5XD5NVa6azE=";
//	char password[]   = "11111111zm";
//	char nonce[]   = "Rv0rliNllk6hBKKSh4HvzFwAAAAAAA==";
//	char created[] = "2022-03-17T02:50:14.000Z";
//	uint8_t i,flag=0;
//	
//	char pwd_digest_t[30] ={0} ;
//	
//	//用官方的example，测试一下我们的摘要算法是否有问题
//	GetPasswordDigest(nonce,created,password,pwd_digest_t);
//		
//	for(i=0;i<strlen(pwd_digest_t);i++)
//	{
//		if(pwd_digest_t[i] == pwd_digest[i])
//		{
//			flag = 1;
//		}
//		else
//		{
//			flag = 0;
//			break;
//		}
//	}	
////	if(flag)
////		printf("Test Passed! \r\n");
////	else
////		printf("Test Failed! \r\n");
//}

