#ifndef __ONVIF_AGREE_H
#define __ONVIF_AGREE_H

#include "sys.h"
#include "bsp.h"

#define IPC_USER_NAME  "admin"
#define IPC_PASSWORD   "TESTfnwlw"
#define IPC_PASSWORD2   "11111111zm"

int onvif_http_head(char* buf,char *ip,int port,char *onvif_cmd,int xml_len,uint8_t sort,uint8_t brand);

int onvif_xml(char* buf,char *user_name,char *password,char *onvif_cmd,uint8_t sort,uint8_t brand);

int onvif_agreement_create(char* buf,char *onvif_cmd,char *ip,int port,uint8_t sort,uint8_t brand);

char *strrpc(char *str,char *oldstr,char *newstr);

int onvif_time_str_create(char *buff);
void onvif_get_set_times(rtc_time_t *time_t);
#endif

