#ifndef __PORT_SCAN_H__
#define __PORT_SCAN_H__

#include <stdint.h>
#include "lwip/ip_addr.h"

void port_scan_timer_function(void);
void port_scan_detection_function(void);
int8_t port_scan_start(uint8_t ip[4],int port);

#endif // __PORT_SCAN_H__
