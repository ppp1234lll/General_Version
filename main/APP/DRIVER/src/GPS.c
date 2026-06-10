#include "GPS.h"
#include "GPRS.h"
//#include "includes.h"

/************************************************************
*
* Function name	: gps_start_function
* Description	: GNSS开启函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t gps_start_function(void)
{
//	struct ml302_status_t *data_t = ml302_get_infor_data_function();
//	uint8_t index = 0;
//	
//	data_t->status.gps = 0;
//	for(index=0; index<3; index++) {
//		if(ml302_send_cmd_function((uint8_t*)"AT+MGNSS=1\r\n",(uint8_t*)"OK",20) == 0) {
//			/* GNSS启动成功 */
//			data_t->status.gps = 1;
//			ml302_send_cmd_over_function();
//			return 0;
//		}
//	}
//	data_t->status.gps = 0;
//	ml302_send_cmd_over_function();
//	/* GNSS启动失败 */
	return -1;
}

/************************************************************
*
* Function name	: gps_close_function
* Description	: 关闭GNSS
* Parameter		: 
* Return		: 
*	
************************************************************/
void gps_close_function(void)
{
//	struct ml302_status_t *data_t = ml302_get_infor_data_function();
//	
//	data_t->status.gps = 0;
//	ml302_send_cmd_function((uint8_t*)"AT+MGNSS=0\r\n",0,0);
//	ml302_send_cmd_over_function();
}

/************************************************************
*
* Function name	: gps_status_monitor_function
* Description	: 状态监测函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t gps_status_monitor_function(void)
{
//	struct ml302_status_t *data_t = ml302_get_infor_data_function();
//	uint8_t  *p1 = NULL;
//	uint16_t size = 0;
//	
//	if(ml302_send_cmd_function((uint8_t*)"AT+MGNSS?\r\n",(uint8_t*)"+MGNSS: ",20) == 0) {
//		p1 = ml302_get_rec_buff_function(&size);
//		p1 = (uint8_t*)strstr((char*)p1,"+MGNSS: ");
//		if(p1 != NULL) {
//			p1 += 8;
//			if(p1[0] == '1') {
//				data_t->status.gps = 1;
//			} else if(p1[0] == '0') {
//				data_t->status.gps = 0;
//			}
//		}
//	}
//	ml302_send_cmd_over_function();
	return 0;
}

/************************************************************
*
* Function name	: gps_read_position_information
* Description	: 读取定位信息
* Parameter		: 
* Return		: 0-获取到定位信息 other-未获取到定位信息
*	
************************************************************/
int8_t gps_read_position_information(struct locat_infor_t *infor_t)
{
//	uint8_t *p1 = NULL;
//	uint16_t size = 0;
//	uint8_t res = 0;
//	double temp1 = 0;
//	double temp2 = 0;
//	
//	if( ml302_send_cmd_function((uint8_t*)"AT+MGNSSINFO\r\n",(uint8_t*)"+MGNSSINFO:",100) == 0) {
//		p1 = ml302_get_rec_buff_function(&size);
//		p1 = (uint8_t*)strstr((char*)p1,"+MGNSSINFO: ");
//		if(p1 != NULL) {
//			p1 += 12;
//			res = sscanf((char*)p1,"E%lf,N%lf",&temp1,&temp2);
//			if(res == 2) {
//				/* 读取到GPS定位信息 */
//				infor_t->longitude = temp1;
//				infor_t->latitude  = temp2;
//				ml302_send_cmd_over_function();
//				return 0;
//			}
//		} else {
//			p1 = ml302_get_rec_buff_function(&size);
//			p1 = (uint8_t*)strstr((char*)p1,"ERROR");
//			if(p1 != NULL) {
//				ml302_send_cmd_over_function();
//				return -2;
//			}
//		}
//	}
//	ml302_send_cmd_over_function();
	return -1;
}


/************************************************************
*
* Function name	: gprs_lbs_position_read_function
* Description	: 读取基站定位信息
* Parameter		: 
* Return		: 0-获取到定位信息 other-未获取到定位信息
*	
************************************************************/
int8_t gps_lbs_position_read_function(struct locat_infor_t *infor_t)
{
	uint8_t index = 0;
	uint8_t *p1 = NULL;
	uint16_t size = 0;
	uint8_t res = 0;
	int state = 0;
	double temp1 = 0;
	double temp2 = 0;
	
	for(index=0; index<3; index++) {
		//配置LBS接口，根据申请的<apikey>对应的接口配置成10或11
		if(gprs_send_cmd_function((uint8_t*)"AT+MLBSCFG=\"method\",10\r\n",(uint8_t*)"OK",100) == 0) {
			break;
		}			
	}
	
	for(index=0; index<3; index++) {
		//配置apikey
		if(gprs_send_cmd_function((uint8_t*)"AT+MLBSCFG=\"apikey\",\"20dbf2e1e0180790656d9a028245fd4e\"\r\n",(uint8_t*)"OK",100) == 0) {
			break;
		}
	}	

	for(index=0; index<3; index++) {
		if( gprs_send_cmd_function((uint8_t*)"AT+MLBSLOC\r\n",(uint8_t*)"+MLBSLOC:",1000) == 0) {
			p1 = gprs_get_rec_buff_function(&size);
			p1 = (uint8_t*)strstr((char*)p1,"+MLBSLOC: ");
			if(p1 != NULL) {
				p1 += 10;
				res = sscanf((char*)p1,"%d,%lf,%lf",&state,&temp1,&temp2);
				if((res == 3)&&(state == 100)) 
				{
					/* 读取到GPS定位信息 */
					infor_t->longitude = temp1;
					infor_t->latitude  = temp2;
					gprs_send_cmd_over_function();
					return 0;
				}
			} 
			else 
			{
				gprs_send_cmd_over_function();
				return -2;
			}
		}
	}
	gprs_send_cmd_over_function();
	return -1;
}


