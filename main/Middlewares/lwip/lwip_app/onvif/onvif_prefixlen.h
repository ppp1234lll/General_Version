#ifndef __ONVIF_PREFIXLEN_H
#define __ONVIF_PREFIXLEN_H

#include "sys.h"

int ipv4_str2prefixlen(char* ip_str);
int ipv4_prefixlen2str(int prefixlen, uint8_t* ip_str);

#endif

