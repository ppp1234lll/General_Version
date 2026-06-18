#include "onvif_agree.h"
#include "onvif_digest.h"
#include "./MALLOC/malloc.h"
#include "onvif_deal.h"
#include <lwip/sockets.h>
#include "lwip/opt.h"
#include "onvif.h"
#include "onvif_tcp.h"
#include "onvif_prefixlen.h"

#define ONVIF_DEBUG 1

__attribute__((section (".RAM_D1")))    char onvif_tcp_recv_buff[TCP_RX_BUFSIZE] = {0};

int onvif_send(int sock_t,char *onvif_cmd,uint8_t sort,char *ip,int port,uint8_t brand)
{
    char *onvif_send_data = NULL;
    int str_len    = 0;

    onvif_send_data = (char *)mymalloc(SRAMIN,1700);  // 申请内存
    if(onvif_send_data != NULL)
    {
        memset(onvif_send_data,0,1700);
        str_len = onvif_agreement_create(onvif_send_data,onvif_cmd,ip,port,sort,brand);
        send(sock_t, onvif_send_data, str_len, 0);        // socket数据发送
    }
    myfree(SRAMIN,onvif_send_data);   // 释放内存
    return 0;
}

/*根据str 取出xml中相应的参数*/
int ONVIF_IPC_GetParam_FUN(char *data,char *sStr,char *param)
{
  char *str  = NULL;
    str = strstr(data,sStr);
    if(!str)
    {
        return -1;
    }    
    sscanf(str,"%*[^>]>%[^</]",param);
    return 0;
}

/*校验是否成功*/
int ONVIF_IPC_ChkData_FUN(char *data)
{
    char *str = NULL;
    char buf[100] = {0};
    
    str = strstr(data,"HTTP/1.1 200 OK");    /*SOAP-ENV:Sender*/
    if(str == NULL)      // 未找到指定字符串     
    {
        Get_Str_Between(data,buf,"<env:Text xml:lang=\"en\">","</env:Text>");
        return -1;
    }
    return 0;
}
/*读取两个字符之间的字符串*/
int Get_Str_Between(char *pcBuf, char *pcRes,char *begin,char *end)
{
    char *pcBegin = NULL;
    char *pcEnd = NULL;
 
    pcBegin = strstr(pcBuf, begin);
    pcEnd = strstr(pcBuf, end);
 
    if(pcBegin == NULL || pcEnd == NULL || pcBegin > pcEnd)
    {
//        printf("Mail name not found!\n");
        return -1;
    }
    else
    {
        pcBegin += strlen(begin);
        memcpy(pcRes, pcBegin, pcEnd-pcBegin);
    }
    return 0;
}

/* 获取Configurations token */
int ONVIF_IPC_Token_API(int socket_t,char *ip,int port,uint8_t brand)
{
    int ret    = 0;
    char *rcvData;
    int  total_len = 0;    
    // rcvData = (char *)mymalloc(SRAMIN,TCP_RX_BUFSIZE);  // 申请内存
    rcvData = onvif_tcp_recv_buff;

    onvif_send(socket_t,Get_CONFIG_TOKEN,1,ip,port,brand);     // 发送数据
    
    memset(rcvData,0,TCP_RX_BUFSIZE);    
    vTaskDelay(200);   //延时200ms
    
    int ret_recv = recv(socket_t, rcvData, TCP_RX_BUFSIZE - 1, 0);
    if (ret_recv > 0) {
        total_len += ret_recv;
        // 非阻塞读取剩余分片/粘包数据
        while (total_len < TCP_RX_BUFSIZE - 1) {
            ret_recv = recv(socket_t, rcvData + total_len, TCP_RX_BUFSIZE - 1 - total_len, 0x08 /* MSG_DONTWAIT */);
            if (ret_recv > 0) total_len += ret_recv;
            else break;
        }
    }
    rcvData[total_len] = '\0';

//    printf("token_over.....  \n");    
//    printf("%d...........%s\r\n",ret,rcvData);
    ret = Get_IPC_Config_Token(rcvData);             // 处理数据，获得设备参数
    // myfree(SRAMIN,rcvData);    // 释放内存

    return ret;
}
int Get_IPC_Config_Token(char *buf)
{
    int ret    = 0;
    char token[30] = {0};
    char *str_data = NULL;
    
    if(ONVIF_IPC_ChkData_FUN(buf) == 0)
    {
        str_data = strstr(buf,"GetVideoSourceConfigurationsResponse");
        str_data = strstr(str_data,"token");        
        
        if(Get_Str_Between(str_data,token,"=\"","\">")<0)// 获取信息
            ret  =  -2;        
        else
        {
            sprintf((char*)sg_ipc_param.OSD_info.config_token,"%s",token);
//            printf("%s\n",sg_ipc_param.OSD_info.config_token);  // 16进制打印
            ret = 0;
        }
    }
    else
        ret = -1;    
    return ret;
}
/* 获取OSD标注 */
int ONVIF_IPC_OSD_API(int socket_t,char *ip,int port,uint8_t brand)
{
    int ret    = 0;
    char *rcvData;
    int  total_len = 0;    

    // rcvData = (char *)mymalloc(SRAMIN,TCP_RX_BUFSIZE);  // 申请内存
    rcvData = onvif_tcp_recv_buff;

    onvif_send(socket_t,Get_OSD_INFO,1,ip,port,brand);     // 发送数据
    
    memset(rcvData,0,TCP_RX_BUFSIZE);    
    vTaskDelay(200);   //延时200ms

    int ret_recv = recv(socket_t, rcvData, TCP_RX_BUFSIZE - 1, 0);
    if (ret_recv > 0) {
        total_len += ret_recv;
        // 非阻塞读取剩余分片/粘包数据
        while (total_len < TCP_RX_BUFSIZE - 1) {
            ret_recv = recv(socket_t, rcvData + total_len, TCP_RX_BUFSIZE - 1 - total_len, 0x08 /* MSG_DONTWAIT */);
            if (ret_recv > 0) total_len += ret_recv;
            else break;
        }
    }
    rcvData[total_len] = '\0';

    // printf("osd_read_over.....  \n");    
    // printf("%d...........%s\r\n",ret,rcvData);
    ret = Get_IPC_OSD_Info(rcvData);             // 处理数据，获得设备参数
    myfree(SRAMIN,rcvData);                // 释放内存

    return ret;
}

int Get_IPC_OSD_Info(char *buf)
{
    int ret    = 0;
    char osd_name[128] = {0};
    char *str_buff = NULL;

    if(ONVIF_IPC_ChkData_FUN(buf) == 0)
    {
        if(strcmp(sg_ipc_param.device.manufacturer,"Tiandy Tech") == 0)  // 天地伟业 第一个<tt:PlainText>为空
        {    
            str_buff = strstr(buf,"<tt:PlainText>");
            str_buff = strstr(str_buff+20,"<tt:PlainText>");  // 搜索第二个字符串
        }
        else
            str_buff = strstr(buf,"<tt:PlainText>");

        if(str_buff != NULL && sscanf(str_buff, "<tt:PlainText>%127[^<]</tt:PlainText>", osd_name) == 1) // ??
        {
            sprintf((char*)sg_ipc_param.OSD_info.name,"%s",osd_name);
            sprintf((char*)osd_params.name,"%s",osd_name);
        }
        else
        {
            ret  =  -2;
        }
    }
    else
        ret = -1;    
    return ret;
}


/*
*********************************************************************************************************
* 名    称: onvif_get_device_param_function
* 功    能：返回IPC设备信息：
* 入口参数：
* 返回参数：sg_ipc_param.device
* 说    明： 
*********************************************************************************************************
*/
void *onvif_get_osd_param_function(void)
{
    return &sg_ipc_param.OSD_info;
}

void onvif_get_token_function(char *buff)
{
    sprintf(buff,"%s",sg_ipc_param.OSD_info.config_token);
}
