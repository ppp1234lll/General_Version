#include "lwip_ping_remote.h"

#include <string.h>

#include "lwip/raw.h"           /* lwip RAW PCB接口，用于收发原始ICMP报文 */
#include "lwip/icmp.h"          /* ICMP报文类型及echo头定义 */
#include "lwip/inet.h"          /* 字节序转换等网络辅助接口 */
#include "lwip/inet_chksum.h"   /* inet_chksum校验和计算 */
#include "lwip/def.h"           /* lwip通用宏定义 */
#include "lwip_comm.h"          /* g_lwipdev等本地网络设备信息 */

#define LWIP_REMOTE_PING_ID            (0x5250U)   /* ICMP echo请求的标识符(id)，用于过滤本模块自身发出的Ping回复 */
#define LWIP_REMOTE_PING_FRAME_SIZE    (32U)       /* 单个ICMP报文长度(含echo头+数据区) */

/* 一次远程Ping会话(一轮共LWIP_REMOTE_PING_PACKET_NUM个包)的运行状态 */
typedef struct
{
    uint8_t active;                                             /* 会话是否处于激活(进行中)状态 */
    uint8_t finalised;                                          /* 会话是否已结束并生成最终结果 */
    uint8_t ip[4];                                              /* 目标IP地址 */
    uint8_t send_count;                                         /* 已发送的Ping包数量 */
    uint8_t reply_count;                                        /* 已收到回复的Ping包数量 */
    lwip_remote_ping_result_t result;                           /* 本轮Ping的最终结果 */
    uint32_t round_start_ms;                                    /* 本轮起始时间戳(ms)，用于计算各包发送时刻 */
    lwip_remote_ping_packet_t packet[LWIP_REMOTE_PING_PACKET_NUM]; /* 各Ping包的收发记录 */
} lwip_remote_ping_session_t;

/* 远程Ping模块的全局控制块 */
typedef struct
{
    uint8_t init;                       /* 模块是否已初始化 */
    uint32_t tick_ms;                   /* 由1ms定时器累加的毫秒计时基准 */
    uint16_t next_seq;                  /* 下一个ICMP报文使用的序列号(seqno) */
    struct raw_pcb *pcb;                /* lwip RAW协议控制块 */
    lwip_remote_ping_session_t session; /* 当前Ping会话 */
} lwip_remote_ping_ctrl_t;

static lwip_remote_ping_ctrl_t sg_ping_remote_t = {0};  /* 远程Ping模块全局控制块实例 */
static uint8_t sg_ping_remote_task_active = 0U;         /* 任务入口的运行标志，避免重复启动会话 */

/* 内部函数声明 */
static uint8_t lwip_ping_remote_raw_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr); /* RAW收包回调 */
static void lwip_ping_remote_start(const uint8_t ip[4]);                                        /* 启动一次Ping会话 */
static void lwip_ping_remote_poll(void);                                                        /* 推进状态机 */
static uint8_t lwip_ping_remote_is_busy(void);                                                  /* 查询是否忙碌 */
static const lwip_remote_ping_packet_t *lwip_ping_remote_get_packet(uint8_t packet_index);      /* 获取指定包记录 */
static void lwip_ping_remote_fill_report(lwip_remote_ping_report_t *report);                    /* 填充结果报告 */
static void lwip_ping_remote_start_round(lwip_remote_ping_session_t *session);                  /* 开始新一轮 */
static int8_t lwip_ping_remote_send_packet(lwip_remote_ping_session_t *session, lwip_remote_ping_packet_t *packet); /* 发送单个ICMP包 */
static void lwip_ping_remote_process_session(lwip_remote_ping_session_t *session);              /* 处理会话(发送/超时/结束判定) */
static void lwip_ping_remote_process_timeout(lwip_remote_ping_session_t *session);              /* 处理各包超时 */
static void lwip_ping_remote_finish_round(lwip_remote_ping_session_t *session);                 /* 结束本轮并判定结果 */
static uint8_t lwip_ping_remote_packet_finished(const lwip_remote_ping_packet_t *packet);       /* 判断单个包是否有结论 */
static uint8_t lwip_ping_remote_round_finished(const lwip_remote_ping_session_t *session);      /* 判断本轮是否全部有结论 */
static uint8_t lwip_ping_remote_ip_is_zero(const uint8_t ip[4]);                                /* 判断IP是否为全0 */
static uint8_t lwip_ping_remote_time_reached(uint32_t target_ms);                               /* 判断目标时刻是否到达(处理回绕) */
static void lwip_ping_remote_get_source_ip(const ip_addr_t *addr, uint8_t ip[4]);               /* 提取源IP到字节数组 */

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
        return 0;   /* 已初始化，直接返回成功 */
    }

    memset(&sg_ping_remote_t, 0, sizeof(sg_ping_remote_t));
    sg_ping_remote_t.next_seq = 1U;     /* 序列号从1开始 */

    sg_ping_remote_t.pcb = raw_new(IP_PROTO_ICMP);  /* 创建ICMP协议的RAW PCB */
    if (sg_ping_remote_t.pcb == NULL)
    {
        return 1;   /* PCB创建失败 */
    }

    raw_recv(sg_ping_remote_t.pcb, lwip_ping_remote_raw_recv, NULL); /* 注册收包回调 */
    raw_bind(sg_ping_remote_t.pcb, IP_ADDR_ANY);                     /* 绑定到任意本地地址 */

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
        sg_ping_remote_task_active = 0U;    /* 请求停止，复位运行标志 */
        return 0;
    }

    /* 模块未初始化或网络未就绪，无法执行 */
    if ((sg_ping_remote_t.init == 0U) || (g_lwipdev.netif_state != 1U))
    {
        sg_ping_remote_task_active = 0U;
        return -1;
    }

    if (sg_ping_remote_task_active == 0U)
    {
        lwip_ping_remote_start(ip);         /* 首次调用，启动新的Ping会话 */
        sg_ping_remote_task_active = 1U;
    }

    lwip_ping_remote_poll();                /* 推进状态机(发送/超时/收包判定) */

    if (lwip_ping_remote_is_busy() != 0U)
    {
        return 0;   /* 仍在进行中 */
    }

    if (report != NULL)
    {
        lwip_ping_remote_fill_report(report);   /* 会话结束，输出结果报告 */
    }

    sg_ping_remote_task_active = 0U;
    return 1;   /* 完成 */
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_start
*    功能说明: 启动一次远程Ping会话，校验目标IP并开始第一轮
*    形    参: ip: 目标IP地址(4字节)
*    返 回 值: 无
*********************************************************************************************************
*/
static void lwip_ping_remote_start(const uint8_t ip[4])
{
    lwip_remote_ping_session_t *session = &sg_ping_remote_t.session;

    memset(session, 0, sizeof(*session));

    /* 目标IP无效(空指针或全0)时直接判定为失败 */
    if ((ip == NULL) || (lwip_ping_remote_ip_is_zero(ip) != 0U))
    {
        session->finalised = 1U;
        session->result = LWIP_REMOTE_PING_RESULT_FAIL;
        return;
    }

    memcpy(session->ip, ip, sizeof(session->ip));
    lwip_ping_remote_start_round(session);  /* 记录目标IP并开始一轮Ping */
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
        return; /* 模块未就绪 */
    }

    if ((session->active == 0U) || (session->finalised != 0U))
    {
        return; /* 无进行中的会话 */
    }

    if (g_lwipdev.netif_state != 1U)
    {
        lwip_ping_remote_finish_round(session); /* 网络中途掉线，立即结束本轮 */
        return;
    }

    lwip_ping_remote_process_session(session);  /* 正常推进会话 */
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

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_fill_report
*    功能说明: 将会话结果整理到外部报告结构中
*    形    参: report: 输出的结果报告
*    返 回 值: 无
*********************************************************************************************************
*/
static void lwip_ping_remote_fill_report(lwip_remote_ping_report_t *report)
{
    uint8_t i = 0;
    const lwip_remote_ping_packet_t *packet = NULL;

    if (report == NULL)
    {
        return;
    }

    memset(report, 0, sizeof(*report));
    report->result = sg_ping_remote_t.session.result;  /* 拷贝整体结果 */

    /* 逐包填充：仅对收到回复的包记录状态与往返时延 */
    for (i = 0; i < LWIP_REMOTE_PING_PACKET_NUM; i++)
    {
        packet = lwip_ping_remote_get_packet(i);
        if ((packet != NULL) && (packet->acked != 0U))
        {
            report->status[i] = 1U;             /* 该包成功 */
            report->times[i] = packet->rtt_ms;  /* 往返时延(ms) */
        }
    }
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_raw_recv
*    功能说明: RAW PCB收包回调，解析ICMP echo回复并匹配已发送的Ping包
*    形    参: arg:  用户参数(未使用)
*              pcb:  RAW控制块(未使用)
*              p:    收到的报文缓冲
*              addr: 源IP地址
*    返 回 值: 0-未消费该报文(交由lwip继续处理)
*********************************************************************************************************
*/
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

    /* 读取IP首字节，取得版本号与首部长度 */
    if (pbuf_copy_partial(p, &ip_first, 1U, 0U) != 1U)
    {
        return 0U;
    }

    if ((ip_first >> 4) == 4U)
    {
        ip_header_len = (uint8_t)((ip_first & 0x0FU) * 4U);  /* IHL字段以4字节为单位 */
    }

    /* 长度不足以容纳IP首部+ICMP echo头，丢弃 */
    if (p->tot_len < (uint16_t)(ip_header_len + sizeof(struct icmp_echo_hdr)))
    {
        return 0U;
    }

    /* 跳过IP首部，拷贝ICMP echo头 */
    if (pbuf_copy_partial(p, &echo_hdr, sizeof(echo_hdr), ip_header_len) != sizeof(echo_hdr))
    {
        return 0U;
    }

    if (echo_hdr.type != ICMP_ER)   /* 只处理echo reply */
    {
        return 0U;
    }

    packet_id = lwip_ntohs(echo_hdr.id);
    if (packet_id != LWIP_REMOTE_PING_ID)   /* 过滤非本模块发出的Ping */
    {
        return 0U;
    }

    seq = lwip_ntohs(echo_hdr.seqno);
    lwip_ping_remote_get_source_ip(addr, src_ip);

    /* 会话未激活或源IP与目标IP不符则忽略 */
    if ((session->active == 0U) || (memcmp(session->ip, src_ip, sizeof(src_ip)) != 0))
    {
        return 0U;
    }

    /* 按序列号匹配对应的待回复包 */
    for (packet_index = 0; packet_index < LWIP_REMOTE_PING_PACKET_NUM; packet_index++)
    {
        packet = &session->packet[packet_index];
        if ((packet->sent != 0U) && (packet->acked == 0U) && (packet->timed_out == 0U) && (packet->seq == seq))
        {
            packet->acked = 1U;
            packet->rtt_ms = (uint16_t)(sg_ping_remote_t.tick_ms - packet->send_tick_ms); /* 计算往返时延 */
            if (packet->rtt_ms == 0U)
            {
                packet->rtt_ms = 1U;    /* 同一tick内收到回复时，最小按1ms上报 */
            }
            if (session->reply_count < LWIP_REMOTE_PING_PACKET_NUM)
            {
                session->reply_count++;
            }

            if (session->reply_count >= LWIP_REMOTE_PING_PACKET_NUM)
            {
                lwip_ping_remote_finish_round(session);  /* 全部回复到齐，提前结束 */
            }
            return 0U;
        }
    }

    return 0U;
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_start_round
*    功能说明: 开始新一轮Ping：保留目标IP，清零其余状态并记录起始时间
*    形    参: session: Ping会话
*    返 回 值: 无
*********************************************************************************************************
*/
static void lwip_ping_remote_start_round(lwip_remote_ping_session_t *session)
{
    uint8_t ip[4] = {0};

    if (session == NULL)
    {
        return;
    }

    memcpy(ip, session->ip, sizeof(ip));        /* 暂存目标IP */
    memset(session, 0, sizeof(*session));       /* 清空会话状态 */
    memcpy(session->ip, ip, sizeof(session->ip)); /* 恢复目标IP */
    session->active = 1U;
    session->round_start_ms = sg_ping_remote_t.tick_ms; /* 记录本轮起始时刻 */
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_send_packet
*    功能说明: 构造并发送一个ICMP echo请求包
*    形    参: session: Ping会话
*              packet:  待发送的包记录(将被填充收发信息)
*    返 回 值: 0-成功 负值-失败(不同负值对应不同失败原因)
*********************************************************************************************************
*/
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

    /* 初始化包记录：序列号、发送时刻及超时截止时刻 */
    memset(packet, 0, sizeof(*packet));
    packet->sent = 1U;
    packet->seq = sg_ping_remote_t.next_seq++;
    packet->send_tick_ms = sg_ping_remote_t.tick_ms;
    packet->deadline_tick_ms = sg_ping_remote_t.tick_ms + LWIP_REMOTE_PING_PACKET_TIMEOUT_MS;

    if (g_lwipdev.netif_state != 1U)
    {
        packet->timed_out = 1U; /* 网络不可用，标记超时 */
        return -2;
    }

    p = pbuf_alloc(PBUF_IP, LWIP_REMOTE_PING_FRAME_SIZE, PBUF_RAM);
    if (p == NULL)
    {
        packet->timed_out = 1U; /* 缓冲分配失败，标记超时 */
        return -3;
    }

    /* 填充ICMP echo请求头 */
    iecho = (struct icmp_echo_hdr *)p->payload;
    data_len = (uint16_t)(LWIP_REMOTE_PING_FRAME_SIZE - sizeof(struct icmp_echo_hdr));

    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0U;
    iecho->id = lwip_htons(LWIP_REMOTE_PING_ID);
    iecho->seqno = lwip_htons(packet->seq);

    /* 填充数据区并计算校验和 */
    payload = (uint8_t *)iecho + sizeof(struct icmp_echo_hdr);
    memset(payload, 0x5AU, data_len);
    iecho->chksum = inet_chksum(iecho, LWIP_REMOTE_PING_FRAME_SIZE);

    /* 设置本地源IP与目标IP */
    IP4_ADDR(&ipaddr, g_lwipdev.ip[0], g_lwipdev.ip[1], g_lwipdev.ip[2], g_lwipdev.ip[3]);
    ip_addr_set(&sg_ping_remote_t.pcb->local_ip, &ipaddr);

    IP4_ADDR(&ipaddr, session->ip[0], session->ip[1], session->ip[2], session->ip[3]);
    ip_addr_set(&sg_ping_remote_t.pcb->remote_ip, &ipaddr);

    err = raw_sendto(sg_ping_remote_t.pcb, p, &sg_ping_remote_t.pcb->remote_ip);
    pbuf_free(p);   /* 发送后立即释放缓冲 */

    if (err != ERR_OK)
    {
        packet->timed_out = 1U; /* 发送失败，标记超时 */
        return -4;
    }

    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_process_session
*    功能说明: 按发送间隔发包、检查超时并判断本轮是否结束
*    形    参: session: Ping会话
*    返 回 值: 无
*********************************************************************************************************
*/
static void lwip_ping_remote_process_session(lwip_remote_ping_session_t *session)
{
    uint8_t packet_index = 0;
    lwip_remote_ping_packet_t *packet = NULL;
    uint32_t send_target_ms = 0;

    if (session == NULL)
    {
        return;
    }

    /* 按固定间隔依次发送尚未发出的包 */
    while (session->send_count < LWIP_REMOTE_PING_PACKET_NUM)
    {
        send_target_ms = session->round_start_ms + ((uint32_t)session->send_count * LWIP_REMOTE_PING_SEND_INTERVAL_MS);
        if (lwip_ping_remote_time_reached(send_target_ms) == 0U)
        {
            break;  /* 未到该包的发送时刻，稍后再试 */
        }

        packet_index = session->send_count;
        packet = &session->packet[packet_index];
        (void)lwip_ping_remote_send_packet(session, packet);
        session->send_count++;
    }

    lwip_ping_remote_process_timeout(session);  /* 标记已超时的包 */

    if (session->reply_count >= LWIP_REMOTE_PING_PACKET_NUM)
    {
        lwip_ping_remote_finish_round(session);     /* 全部回复到齐 */
        return;
    }

    /* 包已全部发出且每个包都有结论(回复或超时)，结束本轮 */
    if ((session->send_count >= LWIP_REMOTE_PING_PACKET_NUM) && (lwip_ping_remote_round_finished(session) != 0U))
    {
        lwip_ping_remote_finish_round(session);
    }
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_process_timeout
*    功能说明: 遍历所有已发送但未回复的包，超过截止时刻则标记为超时
*    形    参: session: Ping会话
*    返 回 值: 无
*********************************************************************************************************
*/
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

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_packet_finished
*    功能说明: 判断单个包是否已有结论(收到回复或超时)
*    形    参: packet: 包记录
*    返 回 值: 1-已有结论 0-仍在等待
*********************************************************************************************************
*/
static uint8_t lwip_ping_remote_packet_finished(const lwip_remote_ping_packet_t *packet)
{
    if (packet == NULL)
    {
        return 0U;
    }

    return (uint8_t)((packet->acked != 0U) || (packet->timed_out != 0U));
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_round_finished
*    功能说明: 判断本轮所有包是否都已有结论
*    形    参: session: Ping会话
*    返 回 值: 1-全部完成 0-仍有包在等待
*********************************************************************************************************
*/
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
            return 0U;  /* 存在未完成的包 */
        }
    }

    return 1U;
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_finish_round
*    功能说明: 结束本轮Ping，根据回复数量判定最终结果
*    形    参: session: Ping会话
*    返 回 值: 无
*********************************************************************************************************
*/
static void lwip_ping_remote_finish_round(lwip_remote_ping_session_t *session)
{
    if ((session == NULL) || (session->active == 0U) || (session->finalised != 0U))
    {
        return;
    }

    if (session->reply_count >= LWIP_REMOTE_PING_PACKET_NUM)
    {
        session->result = LWIP_REMOTE_PING_RESULT_OK;       /* 全部回复：成功 */
    }
    else if (session->reply_count == 0U)
    {
        session->result = LWIP_REMOTE_PING_RESULT_FAIL;     /* 无回复：失败 */
    }
    else
    {
        session->result = LWIP_REMOTE_PING_RESULT_PARTIAL;  /* 部分回复：部分成功 */
    }

    session->finalised = 1U;
    session->active = 0U;
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_ip_is_zero
*    功能说明: 判断IP地址是否为全0
*    形    参: ip: IP地址(4字节)
*    返 回 值: 1-全0 0-非全0
*********************************************************************************************************
*/
static uint8_t lwip_ping_remote_ip_is_zero(const uint8_t ip[4])
{
    return (uint8_t)((ip[0] == 0U) && (ip[1] == 0U) && (ip[2] == 0U) && (ip[3] == 0U));
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_time_reached
*    功能说明: 判断目标时刻是否到达，采用有符号差值以兼容计时器回绕
*    形    参: target_ms: 目标时刻(ms)
*    返 回 值: 1-已到达 0-未到达
*********************************************************************************************************
*/
static uint8_t lwip_ping_remote_time_reached(uint32_t target_ms)
{
    return (uint8_t)(((int32_t)(sg_ping_remote_t.tick_ms - target_ms)) >= 0);
}

/*
*********************************************************************************************************
*    函 数 名: lwip_ping_remote_get_source_ip
*    功能说明: 从lwip地址结构中提取IPv4地址到4字节数组
*    形    参: addr: 源地址
*              ip:   输出的IP地址(4字节)
*    返 回 值: 无
*********************************************************************************************************
*/
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
