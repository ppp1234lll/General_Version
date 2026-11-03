#ifndef __SADP_H
#define __SADP_H

#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/err.h"

/* Hikvision SADP L2 EtherType */
#define ETHTYPE_SADP           0x8033U

/* Inquiry 负载长度 = 0x42, 与校验算法入参一致 */
#define SADP_INQUIRY_LEN       66

err_t sadp_ethernet_input(struct pbuf *p, struct netif *netif);
int   sadp_send_inquiry(void);
int   sadp_recv_process(void);
void  sadp_reset(void);

#endif
