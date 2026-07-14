#ifndef _LWIP_PING_REMOTE_H_
#define _LWIP_PING_REMOTE_H_

#include "bsp.h"

#define LWIP_REMOTE_PING_PACKET_NUM          (4U)
#define LWIP_REMOTE_PING_SEND_INTERVAL_MS    (1000UL)
#define LWIP_REMOTE_PING_PACKET_TIMEOUT_MS   (5000UL)

typedef enum
{
    LWIP_REMOTE_PING_RESULT_NONE = 0,
    LWIP_REMOTE_PING_RESULT_OK,
    LWIP_REMOTE_PING_RESULT_FAIL,
    LWIP_REMOTE_PING_RESULT_PARTIAL,
} lwip_remote_ping_result_t;

typedef struct
{
    uint8_t sent;
    uint8_t acked;
    uint8_t timed_out;
    uint16_t seq;
    uint16_t rtt_ms;
    uint32_t send_tick_ms;
    uint32_t deadline_tick_ms;
} lwip_remote_ping_packet_t;

typedef struct
{
    uint8_t  status[LWIP_REMOTE_PING_PACKET_NUM];
    uint16_t times[LWIP_REMOTE_PING_PACKET_NUM];
    lwip_remote_ping_result_t result;
} lwip_remote_ping_report_t;

uint8_t lwip_ping_remote_init(void);
void lwip_ping_remote_deinit(void);
void lwip_ping_remote_timer_1ms(void);
void lwip_ping_remote_cancel(void);
int8_t lwip_ping_remote_task(const uint8_t ip[4], uint8_t run_flag, lwip_remote_ping_report_t *report);

#endif
