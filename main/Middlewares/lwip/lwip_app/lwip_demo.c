/**
 ****************************************************************************************************
 * @file        lwip_demo
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-08-01
 * @brief       lwIP HTTPS 实验
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 阿波罗 F429开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */
 
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdint.h>
#include "lwip_demo.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "ethernetif.h"
#include "lwip/tcpip.h"
#include <stdio.h>
#include "lwip/sockets.h"
#include "lwip/api.h"
#include "lwip/dns.h"
#include "MALLOC/malloc.h"


static const char http_html_hdr[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
static const char http_index_html[] =
                                    "<!DOCTYPE html>"\
                                    "<html>"\
                                    "<head>"\
                                    "<title>正点原子 Webserver实验</title>"\
                                    "<meta http-equiv='Content-Type' content='text/html; charset=GB2312'/>"\
                                    "<style type='text/css'>"\
                                    "* {"\
                                    "margin: 0;"\
                                    "padding: 0;"\
                                    "text-decoration: none;"\
                                    "box-sizing: border-box;"\
                                    "}"\
                                    "body {"\
                                    "min-height: 100vh;"\
                                    "background-image: linear-gradient(120deg, #3498db, #8e44ad);"\
                                    "}"\
                                    ".box-form {"\
                                    "width: 360px;"\
                                    "background: #f1f1f1;"\
                                    "height: 500px;"\
                                    "padding: 40px 40px;"\
                                    "border-radius: 10px;"\
                                    "position: absolute;"\
                                    "left: 50%;"\
                                    "top: 50%;"\
                                    "transform: translate(-50%, -50%);"\
                                    "}"\
                                    ".box-form h1 {"\
                                    "text-align: center;"\
                                    "margin-bottom: 20px;"\
                                    "}"\
                                    ".sendbtn {"\
                                    "display: block;"\
                                    "width: 100%;"\
                                    "height: 50px;"\
                                    "border: none;"\
                                    "background: linear-gradient(120deg, #3498db, #8e44ad);"\
                                    "background-size: 200%;"\
                                    "color: #fff;"\
                                    "cursor: pointer;"\
                                    "outline: none;"\
                                    "transition: .5s;"\
                                    "font-size: 20px;"\
                                    "letter-spacing: 10px;"\
                                    "}"\
                                    ".sendbtn:hover {"\
                                    "background-position: right;"\
                                    "}"\
                                    ".bottom-text {"\
                                    "margin-top: 40px;"\
                                    "text-align: center;"\
                                    "font-size: 13px;"\
                                    "}"\
                                    ".checkboxes"\
                                    "{"\
                                    "border-bottom: 1px solid #0064cd;"\
                                    "}"\
                                    ".label"\
                                    "{"\
                                    "background: linear-gradient(120deg, #3498db, #8e44ad);"\
                                    "background-size: 250%;"\
                                    "color:white;"\
                                    "display:inline-block;"\
                                    "font-size:18px;"\
                                    "font-weight:bold;"\
                                    "height:30px;"\
                                    "line-height:30px;"\
                                    "text-align:center;"\
                                    "width:150px;"\
                                    "}"\
                                    "</style>"\
                                    "</head>"\
                                    "<body>"\
                                    "<script type='text/javascript' src='http://lib.sinaapp.com/js/jquery/1.7.2/jquery.min.js'></script>"\
                                    "<form id='LEDSetting' method='POST' action='beep_leds.cgi' class='box-form' >"\
                                    "<div style='text-align:center;width:280px;border:greensolid1px;'>"\
                                    "<img src='https://image.uc.cn/o/wemedia/s/upload/2019/PeVKk81djjeoqio/883a8ce8d02275310a88e7578b82a64d.png;,3,jpegx;3,310x' 'style='margin:0 auto;' width='100' />"\
                                    "</div>"\
                                    "<h1>正点原子</h1>"\
                                    "<div class='label' >"\
                                    "<label>LED State:</label>"\
                                    "</div>"\
                                    "<div class='checkboxes' id='checkboxDiv'>"\
                                    "<input type='checkbox' name='led1' value='1' />打开&nbsp;&nbsp;&nbsp;<input type='checkbox' name='led1' value='2' />关闭"\
                                    "</div>"\
                                    "<br>"\
                                    "<br>"\
                                    "<div class='label'>"\
                                    "<label>BEEP State:</label>"\
                                    "</div>"\
                                    "<div class='checkboxes' id='checkboxBeep'>"\
                                    "<input type='checkbox' name='beep' value='3' />打开&nbsp;&nbsp;&nbsp;<input type='checkbox' name='beep' value='4' />关闭"\
                                    "</div>"\
                                    "<br>"\
                                    "<br>"\
                                    "<input type='submit' class='sendbtn' value='发送'>"\
                                    "<br>"\
                                    "<div style='margin:5px 5px;'>"\
                                    "&copy;Copyright 2023 by 正点原子"\
                                    "</div>"\
                                    "</form>"\
                                    "<script type='text/javascript'>"\
                                    "$(document).ready(function(){"\
                                    "$(function(){"\
                                    "$('#checkboxDiv').find(':checkbox').each(function(){"\
                                    "$(this).click(function(){"\
                                    "if($(this).is(':checked')){ "\
                                    "$(this).attr('checked',true).siblings().attr('checked',false);"\
                                    "}else{"\
                                    "$(this).attr('checked',false).siblings().attr('checked',false);"\
                                    "}"\
                                    "});"\
                                    "});"\
                                    "$('#checkboxBeep').find(':checkbox').each(function(){"\
                                    "$(this).click(function(){"\
                                    "if($(this).is(':checked')){ "\
                                    "$(this).attr('checked',true).siblings().attr('checked',false);"\
                                    "}else{"\
                                    "$(this).attr('checked',false).siblings().attr('checked',false);"\
                                    "}"\
                                    "});"\
                                    "});"\
                                    "});"\
                                    "});"\
                                    "</script>"\
                                    "</body>"\
                                    "</html>";
/**
 * @brief       寻找指定字符位置
 * @param       buf   缓冲区指针
 * @param       name  寻找字符
 * @retval      返回字符的地址
 */
char *lwip_data_locate(char *buf, char *name)
{
    char *p;
    p = strstr((char *)buf, name);

    if (p == NULL)
    {
        return NULL;
    }

    p += strlen(name);
    return p;
}

/**
 * @brief       服务HTTP线程中接受的一个HTTP连接
 * @param       conn   netconn控制块
 * @retval      无
 */
static void lwip_server_netconn_serve(struct netconn *conn)
{
    struct netbuf *inbuf;
    char *buf;
    u16_t buflen;
    err_t err;
    char *ptemp;
    /* 从端口读取数据，如果那里还没有数据，则阻塞。
      我们假设请求(我们关心的部分)在一个netbuf中 */
    err = netconn_recv(conn, &inbuf);

    if (err == ERR_OK)
    {
        netbuf_data(inbuf, (void **)&buf, &buflen);

        /* 这是一个HTTP GET命令吗?只检查前5个字符，因为
           GET还有其他格式，我们保持简单)*/
        if (buflen >= 5 &&
                buf[0] == 'G' &&
                buf[1] == 'E' &&
                buf[2] == 'T' &&
                buf[3] == ' ' &&
                buf[4] == '/' )
        {

start_html:
            /* 发送HTML标题
            从大小中减去1，因为我们没有在字符串中发送\0
            NETCONN_NOCOPY:我们的数据是常量静态的，所以不需要复制它 */
            netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);

            /* 发送我们的HTML页面 */
            netconn_write(conn, http_index_html, sizeof(http_index_html) - 1, NETCONN_NOCOPY);
        }
        else if(buflen>=8&&buf[0]=='P'&&buf[1]=='O'&&buf[2]=='S'&&buf[3]=='T')
        {
            ptemp = lwip_data_locate((char *)buf, "led1=");

            if (ptemp != NULL)
            {
                if (*ptemp == '1')    /* 查看led1的值。为1则灯亮，为2则灭，此值与HTML网页中设置有关 */
                {
//                    LED0(0);  /* 点亮LED1 */
                }
                else
                {
//                    LED0(1);    /* 熄灭LED1 */
                }

            }

            ptemp = lwip_data_locate((char *)buf, "beep=");    /* 查看beep的值。为3则灯亮，为4则灭，此值与HTML网页中设置有关 */

            if (ptemp != NULL )
            {
                if (*ptemp == '3')
                {
                    /* 打开蜂鸣器 */
                }
                else
                {
                    /* 关闭蜂鸣器 */
                }
            }
            goto start_html;
        }
    }

    /* 关闭连接(服务器在HTTP中关闭) */
    netconn_close(conn);

    /* 删除缓冲区(netconn_recv给我们所有权，
       所以我们必须确保释放缓冲区) */
    netbuf_delete(inbuf);
}

/**
* @brief  lwip_demo程序入口
* @param  无
* @retval 无
*/
void lwip_demo(void)
{
    struct netconn *conn, *newconn;
    err_t err;

    /* 创建一个新的TCP连接句柄 */
    /* 使用默认IP地址绑定到端口80 (HTTP) */

    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, IP_ADDR_ANY, 80);

    /* 将连接置于侦听状态 */
    netconn_listen(conn);

    do
    {
        err = netconn_accept(conn, &newconn);

        if (err == ERR_OK)
        {
            lwip_server_netconn_serve(newconn);
            netconn_delete(newconn);
        }
    }
    while (err == ERR_OK);

    netconn_close(conn);
    netconn_delete(conn);
}
