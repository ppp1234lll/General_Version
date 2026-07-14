#ifndef _LWIP_PING_MULTI_H_
#define _LWIP_PING_MULTI_H_

#include "./SYSTEM/sys/sys.h"

#define LWIP_MULTI_PING_CAMERA_NUM          (6U)
#define LWIP_MULTI_PING_PACKET_NUM          (4U)
#define LWIP_MULTI_PING_SEND_INTERVAL_MS    (1000UL)
#define LWIP_MULTI_PING_PACKET_TIMEOUT_MS   (5000UL)
#define LWIP_MULTI_PING_ROUND_INTERVAL_MS   (20000UL)
#define LWIP_MULTI_PING_DEVICE_STAGGER_MS   (300UL)
#define LWIP_MULTI_PING_DEVICE_NUM          (2U + LWIP_MULTI_PING_CAMERA_NUM)

typedef enum
{
    LWIP_MULTI_PING_DEV_MAIN = 0,
    LWIP_MULTI_PING_DEV_MAIN_SUB,
    LWIP_MULTI_PING_DEV_CAMERA1,
    LWIP_MULTI_PING_DEV_CAMERA2,
    LWIP_MULTI_PING_DEV_CAMERA3,
    LWIP_MULTI_PING_DEV_CAMERA4,
    LWIP_MULTI_PING_DEV_CAMERA5,
    LWIP_MULTI_PING_DEV_CAMERA6,
} lwip_multi_ping_device_id_t;

typedef enum
{
    LWIP_MULTI_PING_RESULT_NONE = 0,
    LWIP_MULTI_PING_RESULT_OK,
    LWIP_MULTI_PING_RESULT_FAIL,
    LWIP_MULTI_PING_RESULT_PARTIAL,
} lwip_multi_ping_result_t;

typedef struct
{
    uint8_t sent;
    uint8_t acked;
    uint8_t timed_out;
    uint16_t seq;
    uint16_t rtt_ms;
    uint32_t send_tick_ms;
    uint32_t deadline_tick_ms;
} lwip_multi_ping_packet_t;

typedef struct
{
    uint8_t enable;
    uint8_t ip[4];
    uint8_t round_active;
    uint8_t send_count;
    uint8_t reply_count;
    uint8_t finalised;
    uint32_t last_round_start_ms;
    uint32_t round_start_ms;
    lwip_multi_ping_result_t result;
    lwip_multi_ping_packet_t packet[LWIP_MULTI_PING_PACKET_NUM];
} lwip_multi_ping_device_t;

uint8_t lwip_ping_multi_init(void);
void lwip_ping_multi_deinit(void);
void lwip_ping_multi_timer_1ms(void);
void lwip_ping_multi_poll(void);
void lwip_ping_multi_refresh_targets(void);

const lwip_multi_ping_device_t *lwip_ping_multi_get_device(uint8_t device_id);
uint8_t lwip_ping_multi_get_reply_count(uint8_t device_id);
lwip_multi_ping_result_t lwip_ping_multi_get_result(uint8_t device_id);
uint16_t lwip_ping_multi_get_packet_rtt(uint8_t device_id, uint8_t packet_index);

#endif
