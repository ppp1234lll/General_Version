#include "mycJSON.h"
#include "string.h"
#include "stdio.h"

/************************************************************
*
* Function name	: my_cjson_create_function
* Description	: 创建函数
* Parameter		: 
*	@mode		: 0-head 1-tail
* Return		: 
*	
************************************************************/
int8_t my_cjson_create_function(uint8_t *buff, uint8_t mode)
{
	uint16_t size = strlen((char*)buff); 
	
	if(mode == 0) {
		buff[0] = '{';
		buff[1] = '\0';
	} else {
		buff[size] = '}';
		buff[size+1] = '\0';
	}
	
	return 0;
}
/************************************************************
*
* Function name	: my_cjson_info_create_function
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t my_cjson_info_create_function(uint8_t *buff, uint8_t mode)
{
	
	if(mode == 0) {
		strcat((char*)buff,"\"info\":{");
	} else {
		strcat((char*)buff,"},");
	}
	
	return 0;
}

/************************************************************
*
* Function name	: my_cjson_data_create_function
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t my_cjson_data_create_function(uint8_t *buff, uint8_t mode)
{
	
	if(mode == 0) {
		strcat((char*)buff,"\"data\":{");
	} else {
		strcat((char*)buff,"}");
	}
	
	return 0;
}

/************************************************************
*
* Function name	: my_cjson_join_string_function
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t my_cjson_join_string_function(uint8_t *buff,uint8_t *join_t,uint8_t *join_d, uint8_t next)
{
	uint8_t pbuff[128] = {0};
	
	if(next == 1) {
		/* 后面还有参数 */
		sprintf((char*)pbuff,"\"%s\":\"%s\",",join_t,join_d);
	} else {
		/* 后面没有参数 */
		sprintf((char*)pbuff,"\"%s\":\"%s\"",join_t,join_d);
	}
	strcat((char*)buff,(char*)pbuff);
	
	return 0;
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t my_cjson_join_int_function(uint8_t *buff,uint8_t *join_t,int32_t number, uint8_t next)
{
	uint8_t pbuff[128] = {0};
	
	if(next == 1) {
		/* 后面还有参数 */
		sprintf((char*)pbuff,"\"%s\":%d,",join_t,number);
	} else {
		/* 后面没有参数 */
		sprintf((char*)pbuff,"\"%s\":%d",join_t,number);
	}
	strcat((char*)buff,(char*)pbuff);
	
	return 0;
}


