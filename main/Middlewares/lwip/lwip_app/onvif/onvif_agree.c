#include "./onvif/onvif_agree.h"
#include "main.h"


/* HTTP 格式
POST /onvif/device_service HTTP/1.1
Content-Type: application/soap+xml; charset=utf-8; action="http://www.onvif.org/ver10/device/wsdl/GetScopes"
Host: 192.168.2.247
Content-Length: 905
Accept-Encoding: gzip, deflate
Connection: Close

*/
int onvif_http_head(char* buf,char *ip,int port,char *onvif_cmd,int xml_len,uint8_t sort,uint8_t brand)
{
    int str_len        = 0;
    str_len = sprintf(buf,"%s","POST /onvif/"); 
    if(sort == 1)
    {
        if(brand == 1) // 大华摄像机
        {
            str_len += sprintf(buf+str_len,"%s","media2_service");  // 设备信息
        }
        else
        {
            str_len += sprintf(buf+str_len,"%s","Media");  // 设备信息
        }
    }
    else if(sort == 0)
        str_len += sprintf(buf+str_len,"%s","device_service"); // 视频信息
    
    str_len += sprintf(buf+str_len,"%s"," HTTP/1.1\r\nContent-Type: application/soap+xml; charset=utf-8; action=\"");
    
    if(sort == 1)
        str_len += sprintf(buf+str_len,"%s","http://www.onvif.org/ver10/media/wsdl/");  // 设备信息
    else if(sort == 0)
        str_len += sprintf(buf+str_len,"%s","http://www.onvif.org/ver10/device/wsdl/"); // 视频信息
    str_len += sprintf(buf+str_len,"%s",onvif_cmd);
    
    str_len += sprintf(buf+str_len,"%s","\"\r\n");
    str_len += sprintf(buf+str_len,"%s","Host: ");  
    str_len += sprintf(buf+str_len,"%s:%d",ip,port);
    str_len += sprintf(buf+str_len,"%s","\r\n"); 
    str_len += sprintf(buf+str_len,"%s","Content-Length: "); 
    str_len += sprintf(buf+str_len,"%d",xml_len);
    str_len += sprintf(buf+str_len,"%s","\r\n"); 
    str_len += sprintf(buf+str_len,"%s","Accept-Encoding: gzip, deflate\r\n");  
    str_len += sprintf(buf+str_len,"%s","Connection: Close\r\n");
    str_len += sprintf(buf+str_len,"%s","\r\n");
    return str_len;
}

/* XML格式（中间没有换行符，为方便查看，进行换行处理）
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope">
    <s:Header>
        <Security s:mustUnderstand="1" xmlns="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd">
            <UsernameToken>
                <Username>admin</Username>
                <Password Type="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest">2V88eeuvEymmfuneh1Lqxe/cEgc=</Password>
                <Nonce EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary">aECmNtV5qEm8JkEcbB0omgUAAAAAAA==</Nonce>
                <Created xmlns="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd">2022-03-19T00:39:39.012Z</Created></UsernameToken>
        </Security>
    </s:Header>
    <s:Body xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
                    xmlns:xsd="http://www.w3.org/2001/XMLSchema">
    <GetScopes xmlns="http://www.onvif.org/ver10/device/wsdl"/>
    </s:Body>
</s:Envelope>
*/
int onvif_xml(char* buf,char *user_name,char *password,char *onvif_cmd,uint8_t sort,uint8_t brand)
{
    int str_len            = 0;
    char nonce_buf[NONCE_B64_LENGTH] = {0};
    char created_buf[CREATED_LENGTH] = {0};
    char pwd_digest_buf[PASSWORD_LENGTH] = {0};
    char *time_buf = NULL;
    char token_buf[64] = {0};
    
    onvif_get_token_function(token_buf);
    GetNonce(nonce_buf);        // 生成加密后的随机数
    GetCreated(created_buf);    // 获取时间    
    GetPasswordDigest(nonce_buf,created_buf,password,pwd_digest_buf);
    
    time_buf = (char *)mymalloc(SRAMIN,450);  // 申请内存    
    memset(time_buf,0,450);
    
    str_len = sprintf(buf,"%s","<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">");    
    str_len += sprintf(buf+str_len,"%s","<s:Header>");
    str_len += sprintf(buf+str_len,"%s","<Security s:mustUnderstand=\"1\" xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\">");    
    str_len += sprintf(buf+str_len,"%s","<UsernameToken>");    
    str_len += sprintf(buf+str_len,"%s","<Username>");
    str_len += sprintf(buf+str_len,"%s",user_name);
    str_len += sprintf(buf+str_len,"%s","</Username>");
    str_len += sprintf(buf+str_len,"%s","<Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">");
    str_len += sprintf(buf+str_len,"%s",pwd_digest_buf);
    str_len += sprintf(buf+str_len,"%s","</Password>");
    str_len += sprintf(buf+str_len,"%s","<Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">");
    str_len += sprintf(buf+str_len,"%s",nonce_buf);
    str_len += sprintf(buf+str_len,"%s","</Nonce>");
    str_len += sprintf(buf+str_len,"%s","<Created xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">");
    str_len += sprintf(buf+str_len,"%s",created_buf);
    str_len += sprintf(buf+str_len,"%s","</Created>");
    str_len += sprintf(buf+str_len,"%s","</UsernameToken>");
    str_len += sprintf(buf+str_len,"%s","</Security></s:Header>");
    str_len += sprintf(buf+str_len,"%s","<s:Body xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">");
    str_len += sprintf(buf+str_len,"%s","<");
    str_len += sprintf(buf+str_len,"%s",onvif_cmd);
    
    if(strcmp(onvif_cmd,"SetSystemDateAndTime") == 0)
    {
        str_len += sprintf(buf+str_len,"%s"," xmlns=\"http://www.onvif.org/ver10/device/wsdl\">"); // 视频信息
        onvif_time_str_create(time_buf);
        str_len += sprintf(buf+str_len,"%s",time_buf);    
    }
    else
    {
        if(sort == 1)
        {
            if((strcmp(onvif_cmd,"GetOSDs") == 0)&&(strcmp(sg_ipc_param.device.manufacturer,"Tiandy Tech") == 0))   // 天地伟业摄像机需要添加TOKEN信息
            {
                str_len += sprintf(buf+str_len,"%s"," xmlns=\"http://www.onvif.org/ver10/media/wsdl\">");   
                str_len += sprintf(buf+str_len,"%s","<ConfigurationToken>");   
                str_len += sprintf(buf+str_len,"%s",token_buf);  
                str_len += sprintf(buf+str_len,"%s","</ConfigurationToken></GetOSDs>");   
            }
            else
                str_len += sprintf(buf+str_len,"%s"," xmlns=\"http://www.onvif.org/ver10/media/wsdl\"/>");  // 设备信息
        }
        else if(sort == 0)
            str_len += sprintf(buf+str_len,"%s"," xmlns=\"http://www.onvif.org/ver10/device/wsdl\"/>"); // 视频信息    
    }
    str_len += sprintf(buf+str_len,"%s","</s:Body></s:Envelope>");
    myfree(SRAMIN,time_buf);   // 释放内存
    return str_len;
}

// HTTP与XML拼接，组成发送数据
int onvif_agreement_create(char* buf,char *onvif_cmd,char *ip,int port,uint8_t sort,uint8_t brand)
{
    int xml_len        = 0;
    int http_len       = 0;
    uint8_t sort_cmd   =0;
    char *xml_buf = NULL;
    char *http_buf = NULL;
    char cmd[30]          = {0}; 
    char user_name[30]= {0};
    char password[30] = {0};
    
    snprintf(user_name,sizeof(user_name),"%s",IPC_USER_NAME);
    snprintf(password,sizeof(password),"%s",IPC_PASSWORD);
//    app_get_camera_login_function(user_name,password,app_get_camera_num_function());
    
    snprintf(cmd,sizeof(cmd),"%s",onvif_cmd);
    sort_cmd = sort;
    
    http_buf = (char *)mymalloc(SRAMIN,300);  // 申请内存
    xml_buf = (char *)mymalloc(SRAMIN,1400);  // 申请内存    

    memset(http_buf,0,300);
    memset(xml_buf,0,1400);

    xml_len = onvif_xml(xml_buf,user_name,password,cmd,sort_cmd,brand);
    http_len = onvif_http_head(http_buf,ip,port,cmd,xml_len,sort_cmd,brand);
    
//    sprintf(buf,"%s",http_buf);
//    sprintf(buf+http_len,"%s",xml_buf);
    strncpy(buf,http_buf,http_len);     
    strncpy(buf+http_len,xml_buf,xml_len); 
    
    myfree(SRAMIN,xml_buf);   // 释放内存
    myfree(SRAMIN,http_buf);   // 释放内存

    return (xml_len+http_len);
}


int onvif_time_str_create(char *buff)
{
    int length=0;
    rtc_time_t times;
//    times = app_get_current_times();    
    onvif_get_set_times(&times);
    
    length = sprintf(buff,"%s","<DateTimeType>Manual</DateTimeType><DaylightSavings>false</DaylightSavings>");
    length += sprintf(buff+length,"%s","<TimeZone><TZ xmlns=\"http://www.onvif.org/ver10/schema\">");
    if(strcmp(sg_ipc_param.device.manufacturer,"Dahua") == 0)  // 大华摄像机
    {
        length += sprintf(buff+length,"%s","PacificStandardTime8DaylightTime,M3.2.0,M11.1.0");
    }    
    else
    {
        length += sprintf(buff+length,"%s","CST-8:00:00");
    }
    
    length += sprintf(buff+length,"%s","</TZ></TimeZone>");
    length += sprintf(buff+length,"%s","<UTCDateTime><Time xmlns=\"http://www.onvif.org/ver10/schema\"><Hour>");
    if(times.hour >= 8 )
        length += sprintf(buff+length,"%d",times.hour-8);
    else
        length += sprintf(buff+length,"%d",times.hour+24-8);
    
    length += sprintf(buff+length,"%s","</Hour><Minute>");
    length += sprintf(buff+length,"%d",times.min);
    length += sprintf(buff+length,"%s","</Minute><Second>");
    length += sprintf(buff+length,"%d",times.sec);
    length += sprintf(buff+length,"%s","</Second></Time><Date xmlns=\"http://www.onvif.org/ver10/schema\"><Year>");
    length += sprintf(buff+length,"%d",times.year);
    length += sprintf(buff+length,"%s","</Year><Month>");
    length += sprintf(buff+length,"%d",times.month);    
    length += sprintf(buff+length,"%s","</Month><Day>");
    if(times.hour >= 8 )
        length += sprintf(buff+length,"%d",times.data);
    else
        length += sprintf(buff+length,"%d",times.data-1);
    length += sprintf(buff+length,"%s","</Day></Date></UTCDateTime></SetSystemDateAndTime>");
    
//    printf("%s\n",buff);
    
    return length;
}


void onvif_get_set_times(rtc_time_t *time_t)
{
  uint32_t second = 0;//onvif_get_times();  
    struct tm *pt,t;
//    rtc_time_t time_t;
    second += 8*60*60;
    if(time_t == NULL)
        return;
    pt = localtime(&second);

    if(pt == NULL)
        return;
    t=*pt;
    t.tm_year+=1900;
    t.tm_mon++;
    time_t->year  = t.tm_year;
    time_t->month = t.tm_mon;
    time_t->data  = t.tm_mday;
    time_t->hour  = t.tm_hour;
    time_t->min   = t.tm_min;
    time_t->sec   = t.tm_sec;
    /* 设置时间 */
}



