#ifndef _HTTPD_CGI_SSI_H_
#define _HTTPD_CGI_SSI_H_

#include "./SYSTEM/sys/sys.h"

/* 网络参数 */
#define INCORRECT_ACCOUNT_OR_PASSWORD_NUM (401)
#define INCORRECT_ACCOUNT_OR_PASSWORD_STR ("\"incorrect account or password!\"") // 密码或账号名称错误

#define PARAMETER_ERROR_NUM (101)
#define PARAMETER_ERROR_STR ("\"parameter error!\"") // 参数错误


#define HTTP_DEBUG  0  // 打印调试
#define CODE_MAX_NUM (12)

void set_return_status_function(uint16_t flag,uint8_t *buff);

/* httpd_cgi */
int8_t httpd_cgi_login_function(int iNumParams, char *pcParam[], char *pcValue[]);           // 网页登录
int8_t httpd_cgi_login_mod_function(int iNumParams, char *pcParam[], char *pcValue[]);       // 密码修改
int8_t httpd_cgi_select_function(char *pcValue[]);                                            // 下拉框状态
int8_t httpd_cgi_switch_function(int iNumParams, char *pcParam[], char *pcValue[]);          // 开关状态
int8_t httpd_cgi_set_threshold_function(int iNumParams, char *pcParam[], char *pcValue[]);   // 阈值设置
int8_t httpd_cgi_set_system_function(int iNumParams, char *pcParam[], char *pcValue[]);      // 系统设置
int8_t httpd_cgi_set_network_function(int iNumParams, char *pcParam[], char *pcValue[]);     // 本地网络设置
int8_t httpd_cgi_set_camera_ip_function(int iNumParams, char *pcParam[], char *pcValue[]);   // 摄像机IP
int8_t httpd_cgi_set_camera_user_function(int iNumParams, char *pcParam[], char *pcValue[]); // 摄像机账号
int8_t httpd_cgi_set_remote_ip_function(int iNumParams, char *pcParam[], char *pcValue[]);   // 远端服务器设置
int8_t httpd_cgi_set_update_addr_function(int iNumParams, char *pcParam[], char *pcValue[]); // 更新地址
int8_t httpd_cgi_snmp_function(int iNumParams, char *pcParam[], char *pcValue[]);            // SNMP设置
int8_t httpd_cgi_snmp_test_function(int iNumParams, char *pcParam[], char *pcValue[]);       // SNMP测试
int8_t httpd_cgi_update_function(int iNumParams, char *pcParam[], char *pcValue[]);          // 系统更新
int8_t httpd_cgi_system_function(int iNumParams, char *pcParam[], char *pcValue[]);          // 系统控制
int8_t httpd_cgi_show_function(char *pcValue[], uint16_t *data, uint8_t *buff);              // 显示更新

/* httpd_ssi */
void httpd_ssi_system_status_function(char *pcInsert);            // 系统状态
void httpd_ssi_volt_cur_data_collection_function(char *pcInsert); // 电能界面更新
void httpd_ssi_switch_status_function(char *pcInsert);            // 开关状态
void httpd_ssi_sensor_data_collection_function(char *pcInsert);   // 传感器数据界面更新
void httpd_ssi_threshold_seting_function(char *pcInsert);         // 阈值信息更新
void httpd_ssi_bd_data_collection_function(char *pcInsert);       // BD数据更新
void httpd_ssi_system_seting_function(char *pcInsert);            // 系统设置
void httpd_ssi_nework_gprs_show_function(char *pcInsert);         // 无线网络信息
void httpd_ssi_network_setting_function(char *pcInsert);          // 网络信息
void httpd_ssi_carema_setting_function(char *pcInsert);           // 摄像头
void http_ssi_server_setting_function(char *pcInsert);            // 远端服务器信息
void httpd_ssi_carema_user_function(char *pcInsert);              // 摄像机账号
void httpd_ssi_snmp_oid_function(char *pcInsert);                 // SNMP OID 配置
void http_ssi_update_addr_function(char *pcInsert);               // 更新地址


void Get_Total_Energy_Handler(char *pcInsert, uint8_t num);
void Get_Output_Energy_Handler(char *pcInsert, uint8_t channel, uint8_t num); // 输出电能界面更新

#endif
