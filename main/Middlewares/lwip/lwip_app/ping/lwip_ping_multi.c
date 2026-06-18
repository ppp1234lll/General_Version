#include "lwip_ping_multi.h"

#include <string.h>

#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet.h"
#include "lwip/inet_chksum.h"
#include "lwip/def.h"
#include "lwip_comm.h"

#include "./Task/inc/app.h"
#include "./Task/inc/det.h"

#define LWIP_MULTI_PING_ID            (0x4D50U)
#define LWIP_MULTI_PING_FRAME_SIZE    (32U)

typedef struct
{
    uint8_t init;
    uint32_t tick_ms;
    uint16_t next_seq;
    struct raw_pcb *pcb;
    lwip_multi_ping_device_t device[LWIP_MULTI_PING_DEVICE_NUM];
} lwip_multi_ping_ctrl_t;

static lwip_multi_ping_ctrl_t sg_ping_multi_t = {0};

static uint8_t lwip_ping_multi_raw_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr);
static void lwip_ping_multi_reset_device(lwip_multi_ping_device_t *device);
static void lwip_ping_multi_update_target(uint8_t device_id, uint8_t enable, const uint8_t ip[4], uint8_t clear_status_on_disable);
static void lwip_ping_multi_start_round(lwip_multi_ping_device_t *device);
static int8_t lwip_ping_multi_send_packet(lwip_multi_ping_device_t *device, lwip_multi_ping_packet_t *packet);
static void lwip_ping_multi_process_device(lwip_multi_ping_device_t *device);
static void lwip_ping_multi_process_timeout(lwip_multi_ping_device_t *device);
static uint8_t lwip_ping_multi_packet_finished(const lwip_multi_ping_packet_t *packet);
static uint8_t lwip_ping_multi_round_finished(const lwip_multi_ping_device_t *device);
static void lwip_ping_multi_finish_round(uint8_t device_id);
static void lwip_ping_multi_apply_result(uint8_t device_id, lwip_multi_ping_result_t result);
static uint8_t lwip_ping_multi_ip_is_zero(const uint8_t ip[4]);
static uint8_t lwip_ping_multi_time_reached(uint32_t target_ms);
static void lwip_ping_multi_get_source_ip(const ip_addr_t *addr, uint8_t ip[4]);

uint8_t lwip_ping_multi_init(void)
{
    if (sg_ping_multi_t.init != 0U)
    {
        return 0;
    }

    memset(&sg_ping_multi_t, 0, sizeof(sg_ping_multi_t));
    sg_ping_multi_t.next_seq = 1U;

    sg_ping_multi_t.pcb = raw_new(IP_PROTO_ICMP);
    if (sg_ping_multi_t.pcb == NULL)
    {
        return 1;
    }

    raw_recv(sg_ping_multi_t.pcb, lwip_ping_multi_raw_recv, NULL);
    raw_bind(sg_ping_multi_t.pcb, IP_ADDR_ANY);

    lwip_ping_multi_refresh_targets();
    sg_ping_multi_t.init = 1U;

    return 0;
}

void lwip_ping_multi_deinit(void)
{
    if (sg_ping_multi_t.pcb != NULL)
    {
        raw_remove(sg_ping_multi_t.pcb);
    }

    memset(&sg_ping_multi_t, 0, sizeof(sg_ping_multi_t));
}

void lwip_ping_multi_timer_1ms(void)
{
    sg_ping_multi_t.tick_ms++;
}

void lwip_ping_multi_refresh_targets(void)
{
    uint8_t ip[4] = {0};
    uint8_t device_id = 0;
    uint8_t camera_index = 0;
    uint8_t camera_ping_enable = 0;
    int8_t ret = 0;

    camera_ping_enable = (app_get_carema_search_mode() == 1U) ? 1U : 0U;

    app_get_main_network_ping_ip_addr(ip);
    lwip_ping_multi_update_target(LWIP_MULTI_PING_DEV_MAIN, (uint8_t)(lwip_ping_multi_ip_is_zero(ip) == 0U), ip, 1U);

    app_get_main_network_sub_ping_ip_addr(ip);
    lwip_ping_multi_update_target(LWIP_MULTI_PING_DEV_MAIN_SUB, (uint8_t)(lwip_ping_multi_ip_is_zero(ip) == 0U), ip, 1U);

    for (device_id = LWIP_MULTI_PING_DEV_CAMERA1; device_id <= LWIP_MULTI_PING_DEV_CAMERA6; device_id++)
    {
        memset(ip, 0, sizeof(ip));
        if (camera_ping_enable == 0U)
        {
            lwip_ping_multi_update_target(device_id, 0U, ip, 0U);
            continue;
        }

        camera_index = (uint8_t)(device_id - LWIP_MULTI_PING_DEV_CAMERA1);
        ret = app_get_camera_function(ip, camera_index);
        lwip_ping_multi_update_target(device_id, (uint8_t)(ret >= 0), ip, 1U);
    }
}

void lwip_ping_multi_poll(void)
{
    uint8_t device_id = 0;

    if ((sg_ping_multi_t.init == 0U) || (sg_ping_multi_t.pcb == NULL))
    {
        return;
    }

    lwip_ping_multi_refresh_targets();

    if (g_lwipdev.netif_state != 1U)
    {
        for (device_id = 0; device_id < LWIP_MULTI_PING_DEVICE_NUM; device_id++)
        {
            if (sg_ping_multi_t.device[device_id].round_active != 0U)
            {
                lwip_ping_multi_finish_round(device_id);
            }
        }
        return;
    }

    for (device_id = 0; device_id < LWIP_MULTI_PING_DEVICE_NUM; device_id++)
    {
        lwip_ping_multi_process_device(&sg_ping_multi_t.device[device_id]);
    }
}

const lwip_multi_ping_device_t *lwip_ping_multi_get_device(uint8_t device_id)
{
    if (device_id >= LWIP_MULTI_PING_DEVICE_NUM)
    {
        return NULL;
    }

    return &sg_ping_multi_t.device[device_id];
}

uint8_t lwip_ping_multi_get_reply_count(uint8_t device_id)
{
    if (device_id >= LWIP_MULTI_PING_DEVICE_NUM)
    {
        return 0;
    }

    return sg_ping_multi_t.device[device_id].reply_count;
}

lwip_multi_ping_result_t lwip_ping_multi_get_result(uint8_t device_id)
{
    if (device_id >= LWIP_MULTI_PING_DEVICE_NUM)
    {
        return LWIP_MULTI_PING_RESULT_NONE;
    }

    return sg_ping_multi_t.device[device_id].result;
}

uint16_t lwip_ping_multi_get_packet_rtt(uint8_t device_id, uint8_t packet_index)
{
    if ((device_id >= LWIP_MULTI_PING_DEVICE_NUM) || (packet_index >= LWIP_MULTI_PING_PACKET_NUM))
    {
        return 0;
    }

    return sg_ping_multi_t.device[device_id].packet[packet_index].rtt_ms;
}

static uint8_t lwip_ping_multi_raw_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
    uint8_t ip_first = 0;
    uint8_t ip_header_len = 0;
    uint8_t src_ip[4] = {0};
    struct icmp_echo_hdr echo_hdr = {0};
    uint16_t packet_id = 0;
    uint16_t seq = 0;
    uint8_t device_id = 0;
    uint8_t packet_index = 0;
    lwip_multi_ping_device_t *device = NULL;
    lwip_multi_ping_packet_t *packet = NULL;

    (void)arg;
    (void)pcb;

    if ((p == NULL) || (addr == NULL))
    {
        return 0U;
    }

    if (pbuf_copy_partial(p, &ip_first, 1U, 0U) != 1U)
    {
        return 0U;
    }

    if ((ip_first >> 4) == 4U)
    {
        ip_header_len = (uint8_t)((ip_first & 0x0FU) * 4U);
    }

    if (p->tot_len < (uint16_t)(ip_header_len + sizeof(struct icmp_echo_hdr)))
    {
        return 0U;
    }

    if (pbuf_copy_partial(p, &echo_hdr, sizeof(echo_hdr), ip_header_len) != sizeof(echo_hdr))
    {
        return 0U;
    }

    if (echo_hdr.type != ICMP_ER)
    {
        return 0U;
    }

    packet_id = lwip_ntohs(echo_hdr.id);
    if (packet_id != LWIP_MULTI_PING_ID)
    {
        return 0U;
    }

    seq = lwip_ntohs(echo_hdr.seqno);
    lwip_ping_multi_get_source_ip(addr, src_ip);

    for (device_id = 0; device_id < LWIP_MULTI_PING_DEVICE_NUM; device_id++)
    {
        device = &sg_ping_multi_t.device[device_id];
        if ((device->round_active == 0U) || (memcmp(device->ip, src_ip, sizeof(src_ip)) != 0))
        {
            continue;
        }

        for (packet_index = 0; packet_index < LWIP_MULTI_PING_PACKET_NUM; packet_index++)
        {
            packet = &device->packet[packet_index];
            if ((packet->sent != 0U) && (packet->acked == 0U) && (packet->timed_out == 0U) && (packet->seq == seq))
            {
                packet->acked = 1U;
                packet->rtt_ms = (uint16_t)(sg_ping_multi_t.tick_ms - packet->send_tick_ms);
                if (device->reply_count < LWIP_MULTI_PING_PACKET_NUM)
                {
                    device->reply_count++;
                }

                if (device->reply_count >= LWIP_MULTI_PING_PACKET_NUM)
                {
                    lwip_ping_multi_finish_round(device_id);
                }
                return 0U;
            }
        }
    }

    return 0U;
}

static void lwip_ping_multi_reset_device(lwip_multi_ping_device_t *device)
{
    uint8_t enable = 0;
    uint8_t ip[4] = {0};

    if (device == NULL)
    {
        return;
    }

    enable = device->enable;
    memcpy(ip, device->ip, sizeof(ip));
    memset(device, 0, sizeof(*device));
    device->enable = enable;
    memcpy(device->ip, ip, sizeof(ip));
}

static void lwip_ping_multi_update_target(uint8_t device_id, uint8_t enable, const uint8_t ip[4], uint8_t clear_status_on_disable)
{
    lwip_multi_ping_device_t *device = NULL;

    if (device_id >= LWIP_MULTI_PING_DEVICE_NUM)
    {
        return;
    }

    device = &sg_ping_multi_t.device[device_id];

    device->enable = enable;
    memcpy(device->ip, ip, sizeof(device->ip));

    if (enable == 0U)
    {
        device->round_active = 0U;
        device->result = LWIP_MULTI_PING_RESULT_NONE;
        device->reply_count = 0U;
        device->send_count = 0U;
        device->finalised = 0U;
        device->round_start_ms = 0U;
        device->last_round_start_ms = 0U;
        memset(device->packet, 0, sizeof(device->packet));
        if (clear_status_on_disable != 0U)
        {
            lwip_ping_multi_apply_result(device_id, LWIP_MULTI_PING_RESULT_FAIL);
        }
        return;
    }

    if (device->round_active != 0U)
    {
        return;
    }
}

static void lwip_ping_multi_start_round(lwip_multi_ping_device_t *device)
{
    if (device == NULL)
    {
        return;
    }

    lwip_ping_multi_reset_device(device);
    device->round_active = 1U;
    device->round_start_ms = sg_ping_multi_t.tick_ms;
    device->last_round_start_ms = sg_ping_multi_t.tick_ms;
}

static int8_t lwip_ping_multi_send_packet(lwip_multi_ping_device_t *device, lwip_multi_ping_packet_t *packet)
{
    struct pbuf *p = NULL;
    struct icmp_echo_hdr *iecho = NULL;
    struct ip4_addr ipaddr = {0};
    err_t err = ERR_OK;
    uint16_t data_len = 0;
    uint8_t *payload = NULL;

    if ((device == NULL) || (packet == NULL) || (sg_ping_multi_t.pcb == NULL))
    {
        return -1;
    }

    memset(packet, 0, sizeof(*packet));
    packet->sent = 1U;
    packet->seq = sg_ping_multi_t.next_seq++;
    packet->send_tick_ms = sg_ping_multi_t.tick_ms;
    packet->deadline_tick_ms = sg_ping_multi_t.tick_ms + LWIP_MULTI_PING_PACKET_TIMEOUT_MS;

    if (g_lwipdev.netif_state != 1U)
    {
        packet->timed_out = 1U;
        return -2;
    }

    p = pbuf_alloc(PBUF_IP, LWIP_MULTI_PING_FRAME_SIZE, PBUF_RAM);
    if (p == NULL)
    {
        packet->timed_out = 1U;
        return -3;
    }

    iecho = (struct icmp_echo_hdr *)p->payload;
    data_len = (uint16_t)(LWIP_MULTI_PING_FRAME_SIZE - sizeof(struct icmp_echo_hdr));

    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0U;
    iecho->id = lwip_htons(LWIP_MULTI_PING_ID);
    iecho->seqno = lwip_htons(packet->seq);

    payload = (uint8_t *)iecho + sizeof(struct icmp_echo_hdr);
    memset(payload, 0x5AU, data_len);
    iecho->chksum = inet_chksum(iecho, LWIP_MULTI_PING_FRAME_SIZE);

    IP4_ADDR(&ipaddr, g_lwipdev.ip[0], g_lwipdev.ip[1], g_lwipdev.ip[2], g_lwipdev.ip[3]);
    ip_addr_set(&sg_ping_multi_t.pcb->local_ip, &ipaddr);

    IP4_ADDR(&ipaddr, device->ip[0], device->ip[1], device->ip[2], device->ip[3]);
    ip_addr_set(&sg_ping_multi_t.pcb->remote_ip, &ipaddr);

    err = raw_sendto(sg_ping_multi_t.pcb, p, &sg_ping_multi_t.pcb->remote_ip);
    pbuf_free(p);

    if (err != ERR_OK)
    {
        packet->timed_out = 1U;
        return -4;
    }

    return 0;
}

static void lwip_ping_multi_process_device(lwip_multi_ping_device_t *device)
{
    uint8_t packet_index = 0;
    lwip_multi_ping_packet_t *packet = NULL;
    uint32_t send_target_ms = 0;

    if (device == NULL)
    {
        return;
    }

    if ((device->round_active == 0U) && (device->enable != 0U))
    {
        if (((device->last_round_start_ms == 0U) && (device->finalised == 0U) &&
             (device->result == LWIP_MULTI_PING_RESULT_NONE)) ||
            ((uint32_t)(sg_ping_multi_t.tick_ms - device->last_round_start_ms) >= LWIP_MULTI_PING_ROUND_INTERVAL_MS))
        {
            lwip_ping_multi_start_round(device);
        }
    }

    if (device->round_active == 0U)
    {
        return;
    }

    while (device->send_count < LWIP_MULTI_PING_PACKET_NUM)
    {
        send_target_ms = device->round_start_ms + ((uint32_t)device->send_count * LWIP_MULTI_PING_SEND_INTERVAL_MS);
        if (lwip_ping_multi_time_reached(send_target_ms) == 0U)
        {
            break;
        }

        packet_index = device->send_count;
        packet = &device->packet[packet_index];
        (void)lwip_ping_multi_send_packet(device, packet);
        device->send_count++;
    }

    lwip_ping_multi_process_timeout(device);

    if (device->reply_count >= LWIP_MULTI_PING_PACKET_NUM)
    {
        lwip_ping_multi_finish_round((uint8_t)(device - &sg_ping_multi_t.device[0]));
        return;
    }

    if ((device->send_count >= LWIP_MULTI_PING_PACKET_NUM) && (lwip_ping_multi_round_finished(device) != 0U))
    {
        lwip_ping_multi_finish_round((uint8_t)(device - &sg_ping_multi_t.device[0]));
    }
}

static void lwip_ping_multi_process_timeout(lwip_multi_ping_device_t *device)
{
    uint8_t packet_index = 0;
    lwip_multi_ping_packet_t *packet = NULL;

    for (packet_index = 0; packet_index < LWIP_MULTI_PING_PACKET_NUM; packet_index++)
    {
        packet = &device->packet[packet_index];
        if ((packet->sent != 0U) && (packet->acked == 0U) && (packet->timed_out == 0U) &&
            (lwip_ping_multi_time_reached(packet->deadline_tick_ms) != 0U))
        {
            packet->timed_out = 1U;
        }
    }
}

static uint8_t lwip_ping_multi_packet_finished(const lwip_multi_ping_packet_t *packet)
{
    if (packet == NULL)
    {
        return 0U;
    }

    return (uint8_t)((packet->acked != 0U) || (packet->timed_out != 0U));
}

static uint8_t lwip_ping_multi_round_finished(const lwip_multi_ping_device_t *device)
{
    uint8_t packet_index = 0;

    if (device == NULL)
    {
        return 0U;
    }

    for (packet_index = 0; packet_index < LWIP_MULTI_PING_PACKET_NUM; packet_index++)
    {
        if (lwip_ping_multi_packet_finished(&device->packet[packet_index]) == 0U)
        {
            return 0U;
        }
    }

    return 1U;
}

static void lwip_ping_multi_finish_round(uint8_t device_id)
{
    lwip_multi_ping_device_t *device = NULL;

    if (device_id >= LWIP_MULTI_PING_DEVICE_NUM)
    {
        return;
    }

    device = &sg_ping_multi_t.device[device_id];
    if ((device->round_active == 0U) || (device->finalised != 0U))
    {
        return;
    }

    if (device->reply_count >= LWIP_MULTI_PING_PACKET_NUM)
    {
        device->result = LWIP_MULTI_PING_RESULT_OK;
    }
    else if (device->reply_count == 0U)
    {
        device->result = LWIP_MULTI_PING_RESULT_FAIL;
    }
    else
    {
        device->result = LWIP_MULTI_PING_RESULT_PARTIAL;
    }

    device->finalised = 1U;
    device->round_active = 0U;
    lwip_ping_multi_apply_result(device_id, device->result);
    det_set_ping_status(1);
}

static void lwip_ping_multi_apply_result(uint8_t device_id, lwip_multi_ping_result_t result)
{
    uint8_t camera_index = 0;

    switch (device_id)
    {
        case LWIP_MULTI_PING_DEV_MAIN:
            if (result == LWIP_MULTI_PING_RESULT_OK)
            {
                det_set_main_network_status(1);
            }
            else if (result == LWIP_MULTI_PING_RESULT_FAIL)
            {
                det_set_main_network_status(0);
            }
            break;

        case LWIP_MULTI_PING_DEV_MAIN_SUB:
            if (result == LWIP_MULTI_PING_RESULT_OK)
            {
                det_set_main_network_sub_status(1);
            }
            else if (result == LWIP_MULTI_PING_RESULT_FAIL)
            {
                det_set_main_network_sub_status(0);
            }
            break;

        default:
            camera_index = (uint8_t)(device_id - LWIP_MULTI_PING_DEV_CAMERA1);
            if (camera_index < LWIP_MULTI_PING_CAMERA_NUM)
            {
                if (result == LWIP_MULTI_PING_RESULT_OK)
                {
                    det_set_camera_status(camera_index, 1);
                }
                else if (result == LWIP_MULTI_PING_RESULT_FAIL)
                {
                    det_set_camera_status(camera_index, 0);
                }
            }
            break;
    }
}

static uint8_t lwip_ping_multi_ip_is_zero(const uint8_t ip[4])
{
    return (uint8_t)((ip[0] == 0U) && (ip[1] == 0U) && (ip[2] == 0U) && (ip[3] == 0U));
}

static uint8_t lwip_ping_multi_time_reached(uint32_t target_ms)
{
    return (uint8_t)(((int32_t)(sg_ping_multi_t.tick_ms - target_ms)) >= 0);
}

static void lwip_ping_multi_get_source_ip(const ip_addr_t *addr, uint8_t ip[4])
{
    const ip4_addr_t *addr4 = NULL;

    if ((addr == NULL) || (ip == NULL))
    {
        return;
    }

    addr4 = ip_2_ip4(addr);
    ip[0] = ip4_addr1(addr4);
    ip[1] = ip4_addr2(addr4);
    ip[2] = ip4_addr3(addr4);
    ip[3] = ip4_addr4(addr4);
}
