#include "lwip_ping_remote.h"

#include <string.h>

#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet.h"
#include "lwip/inet_chksum.h"
#include "lwip/def.h"
#include "lwip_comm.h"

#define LWIP_REMOTE_PING_ID            (0x5250U)
#define LWIP_REMOTE_PING_FRAME_SIZE    (32U)

typedef struct
{
    uint8_t active;
    uint8_t finalised;
    uint8_t ip[4];
    uint8_t send_count;
    uint8_t reply_count;
    lwip_remote_ping_result_t result;
    uint32_t round_start_ms;
    lwip_remote_ping_packet_t packet[LWIP_REMOTE_PING_PACKET_NUM];
} lwip_remote_ping_session_t;

typedef struct
{
    uint8_t init;
    uint32_t tick_ms;
    uint16_t next_seq;
    struct raw_pcb *pcb;
    lwip_remote_ping_session_t session;
} lwip_remote_ping_ctrl_t;

static lwip_remote_ping_ctrl_t sg_ping_remote_t = {0};
static uint8_t sg_ping_remote_task_active = 0U;

static uint8_t lwip_ping_remote_raw_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr);
static void lwip_ping_remote_start(const uint8_t ip[4]);
static void lwip_ping_remote_poll(void);
static uint8_t lwip_ping_remote_is_busy(void);
static const lwip_remote_ping_packet_t *lwip_ping_remote_get_packet(uint8_t packet_index);
static void lwip_ping_remote_fill_report(lwip_remote_ping_report_t *report);
static void lwip_ping_remote_start_round(lwip_remote_ping_session_t *session);
static int8_t lwip_ping_remote_send_packet(lwip_remote_ping_session_t *session, lwip_remote_ping_packet_t *packet);
static void lwip_ping_remote_process_session(lwip_remote_ping_session_t *session);
static void lwip_ping_remote_process_timeout(lwip_remote_ping_session_t *session);
static void lwip_ping_remote_finish_round(lwip_remote_ping_session_t *session);
static uint8_t lwip_ping_remote_packet_finished(const lwip_remote_ping_packet_t *packet);
static uint8_t lwip_ping_remote_round_finished(const lwip_remote_ping_session_t *session);
static uint8_t lwip_ping_remote_ip_is_zero(const uint8_t ip[4]);
static uint8_t lwip_ping_remote_time_reached(uint32_t target_ms);
static void lwip_ping_remote_get_source_ip(const ip_addr_t *addr, uint8_t ip[4]);

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_init
*    功能说明: 初始化远程指定IP Ping模块
*    形    参: 无
*    返 回 值: 0:成功 1:失败
*********************************************************************************************************
*/
uint8_t lwip_ping_remote_init(void)
{
    if (sg_ping_remote_t.init != 0U)
    {
        return 0;
    }

    memset(&sg_ping_remote_t, 0, sizeof(sg_ping_remote_t));
    sg_ping_remote_t.next_seq = 1U;

    sg_ping_remote_t.pcb = raw_new(IP_PROTO_ICMP);
    if (sg_ping_remote_t.pcb == NULL)
    {
        return 1;
    }

    raw_recv(sg_ping_remote_t.pcb, lwip_ping_remote_raw_recv, NULL);
    raw_bind(sg_ping_remote_t.pcb, IP_ADDR_ANY);

    sg_ping_remote_t.init = 1U;
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_deinit
*    功能说明: 反初始化远程指定IP Ping模块
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void lwip_ping_remote_deinit(void)
{
    if (sg_ping_remote_t.pcb != NULL)
    {
        raw_remove(sg_ping_remote_t.pcb);
    }

    memset(&sg_ping_remote_t, 0, sizeof(sg_ping_remote_t));
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_timer_1ms
*    功能说明: 1ms定时器回调
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void lwip_ping_remote_timer_1ms(void)
{
    sg_ping_remote_t.tick_ms++;
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_cancel
*    功能说明: 取消当前远程Ping任务
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void lwip_ping_remote_cancel(void)
{
    sg_ping_remote_task_active = 0U;
    memset(&sg_ping_remote_t.session, 0, sizeof(sg_ping_remote_t.session));
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_task
*    功能说明: 远程指定IP Ping任务入口
*    形    参: ip: 目标IP
*              run_flag: 1-执行 0-停止
*              report: 完成后的结果
*    返 回 值: -1-无网络/未初始化 0-进行中 1-完成
*********************************************************************************************************
*/
int8_t lwip_ping_remote_task(const uint8_t ip[4], uint8_t run_flag, lwip_remote_ping_report_t *report)
{
    if (run_flag == 0U)
    {
        sg_ping_remote_task_active = 0U;
        return 0;
    }

    if ((sg_ping_remote_t.init == 0U) || (g_lwipdev.netif_state != 1U))
    {
        sg_ping_remote_task_active = 0U;
        return -1;
    }

    if (sg_ping_remote_task_active == 0U)
    {
        lwip_ping_remote_start(ip);
        sg_ping_remote_task_active = 1U;
    }

    lwip_ping_remote_poll();

    if (lwip_ping_remote_is_busy() != 0U)
    {
        return 0;
    }

    if (report != NULL)
    {
        lwip_ping_remote_fill_report(report);
    }

    sg_ping_remote_task_active = 0U;
    return 1;
}

static void lwip_ping_remote_start(const uint8_t ip[4])
{
    lwip_remote_ping_session_t *session = &sg_ping_remote_t.session;

    memset(session, 0, sizeof(*session));

    if ((ip == NULL) || (lwip_ping_remote_ip_is_zero(ip) != 0U))
    {
        session->finalised = 1U;
        session->result = LWIP_REMOTE_PING_RESULT_FAIL;
        return;
    }

    memcpy(session->ip, ip, sizeof(session->ip));
    lwip_ping_remote_start_round(session);
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_poll
*    功能说明: 推进远程Ping状态机
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void lwip_ping_remote_poll(void)
{
    lwip_remote_ping_session_t *session = &sg_ping_remote_t.session;

    if ((sg_ping_remote_t.init == 0U) || (sg_ping_remote_t.pcb == NULL))
    {
        return;
    }

    if ((session->active == 0U) || (session->finalised != 0U))
    {
        return;
    }

    if (g_lwipdev.netif_state != 1U)
    {
        lwip_ping_remote_finish_round(session);
        return;
    }

    lwip_ping_remote_process_session(session);
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_is_busy
*    功能说明: 判断远程Ping是否仍在进行
*    形    参: 无
*    返 回 值: 1:进行中 0:空闲或已完成
*********************************************************************************************************
*/
static uint8_t lwip_ping_remote_is_busy(void)
{
    lwip_remote_ping_session_t *session = &sg_ping_remote_t.session;

    return (uint8_t)((session->active != 0U) && (session->finalised == 0U));
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_get_packet
*    功能说明: 获取指定序号的Ping包结果
*    形    参: packet_index: 包序号
*    返 回 值: 包结果指针，失败返回NULL
*********************************************************************************************************
*/
static const lwip_remote_ping_packet_t *lwip_ping_remote_get_packet(uint8_t packet_index)
{
    if (packet_index >= LWIP_REMOTE_PING_PACKET_NUM)
    {
        return NULL;
    }

    return &sg_ping_remote_t.session.packet[packet_index];
}

static void lwip_ping_remote_fill_report(lwip_remote_ping_report_t *report)
{
    uint8_t i = 0;
    const lwip_remote_ping_packet_t *packet = NULL;

    if (report == NULL)
    {
        return;
    }

    memset(report, 0, sizeof(*report));
    report->result = sg_ping_remote_t.session.result;

    for (i = 0; i < LWIP_REMOTE_PING_PACKET_NUM; i++)
    {
        packet = lwip_ping_remote_get_packet(i);
        if ((packet != NULL) && (packet->acked != 0U))
        {
            report->status[i] = 1U;
            report->times[i] = packet->rtt_ms;
        }
    }
}

static uint8_t lwip_ping_remote_raw_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
    uint8_t ip_first = 0;
    uint8_t ip_header_len = 0;
    uint8_t src_ip[4] = {0};
    struct icmp_echo_hdr echo_hdr = {0};
    uint16_t packet_id = 0;
    uint16_t seq = 0;
    uint8_t packet_index = 0;
    lwip_remote_ping_session_t *session = &sg_ping_remote_t.session;
    lwip_remote_ping_packet_t *packet = NULL;

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
    if (packet_id != LWIP_REMOTE_PING_ID)
    {
        return 0U;
    }

    seq = lwip_ntohs(echo_hdr.seqno);
    lwip_ping_remote_get_source_ip(addr, src_ip);

    if ((session->active == 0U) || (memcmp(session->ip, src_ip, sizeof(src_ip)) != 0))
    {
        return 0U;
    }

    for (packet_index = 0; packet_index < LWIP_REMOTE_PING_PACKET_NUM; packet_index++)
    {
        packet = &session->packet[packet_index];
        if ((packet->sent != 0U) && (packet->acked == 0U) && (packet->timed_out == 0U) && (packet->seq == seq))
        {
            packet->acked = 1U;
            packet->rtt_ms = (uint16_t)(sg_ping_remote_t.tick_ms - packet->send_tick_ms);
            if (session->reply_count < LWIP_REMOTE_PING_PACKET_NUM)
            {
                session->reply_count++;
            }

            if (session->reply_count >= LWIP_REMOTE_PING_PACKET_NUM)
            {
                lwip_ping_remote_finish_round(session);
            }
            return 0U;
        }
    }

    return 0U;
}

static void lwip_ping_remote_start_round(lwip_remote_ping_session_t *session)
{
    uint8_t ip[4] = {0};

    if (session == NULL)
    {
        return;
    }

    memcpy(ip, session->ip, sizeof(ip));
    memset(session, 0, sizeof(*session));
    memcpy(session->ip, ip, sizeof(session->ip));
    session->active = 1U;
    session->round_start_ms = sg_ping_remote_t.tick_ms;
}

static int8_t lwip_ping_remote_send_packet(lwip_remote_ping_session_t *session, lwip_remote_ping_packet_t *packet)
{
    struct pbuf *p = NULL;
    struct icmp_echo_hdr *iecho = NULL;
    struct ip4_addr ipaddr = {0};
    err_t err = ERR_OK;
    uint16_t data_len = 0;
    uint8_t *payload = NULL;

    if ((session == NULL) || (packet == NULL) || (sg_ping_remote_t.pcb == NULL))
    {
        return -1;
    }

    memset(packet, 0, sizeof(*packet));
    packet->sent = 1U;
    packet->seq = sg_ping_remote_t.next_seq++;
    packet->send_tick_ms = sg_ping_remote_t.tick_ms;
    packet->deadline_tick_ms = sg_ping_remote_t.tick_ms + LWIP_REMOTE_PING_PACKET_TIMEOUT_MS;

    if (g_lwipdev.netif_state != 1U)
    {
        packet->timed_out = 1U;
        return -2;
    }

    p = pbuf_alloc(PBUF_IP, LWIP_REMOTE_PING_FRAME_SIZE, PBUF_RAM);
    if (p == NULL)
    {
        packet->timed_out = 1U;
        return -3;
    }

    iecho = (struct icmp_echo_hdr *)p->payload;
    data_len = (uint16_t)(LWIP_REMOTE_PING_FRAME_SIZE - sizeof(struct icmp_echo_hdr));

    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0U;
    iecho->id = lwip_htons(LWIP_REMOTE_PING_ID);
    iecho->seqno = lwip_htons(packet->seq);

    payload = (uint8_t *)iecho + sizeof(struct icmp_echo_hdr);
    memset(payload, 0x5AU, data_len);
    iecho->chksum = inet_chksum(iecho, LWIP_REMOTE_PING_FRAME_SIZE);

    IP4_ADDR(&ipaddr, g_lwipdev.ip[0], g_lwipdev.ip[1], g_lwipdev.ip[2], g_lwipdev.ip[3]);
    ip_addr_set(&sg_ping_remote_t.pcb->local_ip, &ipaddr);

    IP4_ADDR(&ipaddr, session->ip[0], session->ip[1], session->ip[2], session->ip[3]);
    ip_addr_set(&sg_ping_remote_t.pcb->remote_ip, &ipaddr);

    err = raw_sendto(sg_ping_remote_t.pcb, p, &sg_ping_remote_t.pcb->remote_ip);
    pbuf_free(p);

    if (err != ERR_OK)
    {
        packet->timed_out = 1U;
        return -4;
    }

    return 0;
}

static void lwip_ping_remote_process_session(lwip_remote_ping_session_t *session)
{
    uint8_t packet_index = 0;
    lwip_remote_ping_packet_t *packet = NULL;
    uint32_t send_target_ms = 0;

    if (session == NULL)
    {
        return;
    }

    while (session->send_count < LWIP_REMOTE_PING_PACKET_NUM)
    {
        send_target_ms = session->round_start_ms + ((uint32_t)session->send_count * LWIP_REMOTE_PING_SEND_INTERVAL_MS);
        if (lwip_ping_remote_time_reached(send_target_ms) == 0U)
        {
            break;
        }

        packet_index = session->send_count;
        packet = &session->packet[packet_index];
        (void)lwip_ping_remote_send_packet(session, packet);
        session->send_count++;
    }

    lwip_ping_remote_process_timeout(session);

    if (session->reply_count >= LWIP_REMOTE_PING_PACKET_NUM)
    {
        lwip_ping_remote_finish_round(session);
        return;
    }

    if ((session->send_count >= LWIP_REMOTE_PING_PACKET_NUM) && (lwip_ping_remote_round_finished(session) != 0U))
    {
        lwip_ping_remote_finish_round(session);
    }
}

static void lwip_ping_remote_process_timeout(lwip_remote_ping_session_t *session)
{
    uint8_t packet_index = 0;
    lwip_remote_ping_packet_t *packet = NULL;

    for (packet_index = 0; packet_index < LWIP_REMOTE_PING_PACKET_NUM; packet_index++)
    {
        packet = &session->packet[packet_index];
        if ((packet->sent != 0U) && (packet->acked == 0U) && (packet->timed_out == 0U) &&
            (lwip_ping_remote_time_reached(packet->deadline_tick_ms) != 0U))
        {
            packet->timed_out = 1U;
        }
    }
}

static uint8_t lwip_ping_remote_packet_finished(const lwip_remote_ping_packet_t *packet)
{
    if (packet == NULL)
    {
        return 0U;
    }

    return (uint8_t)((packet->acked != 0U) || (packet->timed_out != 0U));
}

static uint8_t lwip_ping_remote_round_finished(const lwip_remote_ping_session_t *session)
{
    uint8_t packet_index = 0;

    if (session == NULL)
    {
        return 0U;
    }

    for (packet_index = 0; packet_index < LWIP_REMOTE_PING_PACKET_NUM; packet_index++)
    {
        if (lwip_ping_remote_packet_finished(&session->packet[packet_index]) == 0U)
        {
            return 0U;
        }
    }

    return 1U;
}

static void lwip_ping_remote_finish_round(lwip_remote_ping_session_t *session)
{
    if ((session == NULL) || (session->active == 0U) || (session->finalised != 0U))
    {
        return;
    }

    if (session->reply_count >= LWIP_REMOTE_PING_PACKET_NUM)
    {
        session->result = LWIP_REMOTE_PING_RESULT_OK;
    }
    else if (session->reply_count == 0U)
    {
        session->result = LWIP_REMOTE_PING_RESULT_FAIL;
    }
    else
    {
        session->result = LWIP_REMOTE_PING_RESULT_PARTIAL;
    }

    session->finalised = 1U;
    session->active = 0U;
}

static uint8_t lwip_ping_remote_ip_is_zero(const uint8_t ip[4])
{
    return (uint8_t)((ip[0] == 0U) && (ip[1] == 0U) && (ip[2] == 0U) && (ip[3] == 0U));
}

static uint8_t lwip_ping_remote_time_reached(uint32_t target_ms)
{
    return (uint8_t)(((int32_t)(sg_ping_remote_t.tick_ms - target_ms)) >= 0);
}

static void lwip_ping_remote_get_source_ip(const ip_addr_t *addr, uint8_t ip[4])
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
