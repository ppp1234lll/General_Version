/*******************************************************************************
*
* @File name	: sadp.c
* @Description	: 海康 SADP 二层发现(EtherType 0x8033), 与 UDP 37020 并行
*
*******************************************************************************/
#include "./onvif/sadp.h"
#include "./onvif/onvif.h"
#include "lwip_comm.h"
#include "netif/etharp.h"
#include "string.h"
#include "stdio.h"

#define SADP_DEBUG           0
#define SADP_RX_Q_SIZE       2     /* 与 IPC_NUM_MAX 匹配, 任务侧及时消费 */
#define SADP_ETH_HDR_LEN     14
#define SADP_SN_TMP_SIZE     64    /* 仅拷贝 SN 区用于型号截取, 无需整包缓冲 */
#define SADP_CLIENT_PREFIX   0x42   /* Inquiry 负载长度, 兼作校验入参 */
#define SADP_SERVER_PREFIX   0xF6   /* InquiryResponse 负载长度标识 */
#define SADP_PKT_INQUIRY     0x03
#define SADP_PKT_INQUIRY_RSP 0x04
#define SADP_PKT_INQUIRY_RSP_LEGACY 0x0B  /* 部分旧版固件 Inquiry 应答 */
#define SADP_OFF_CHECKSUM    12
#define SADP_OFF_PKT_TYPE    10
#define SADP_OFF_SRC_MAC     14
#define SADP_OFF_SRC_IP      20
#define SADP_OFF_DST_MAC     24
#define SADP_OFF_DST_IP      30     /* Inquiry 中一般为 0.0.0.0 */
#define SADP_OFF_SUBNET      34     /* 子网掩码; 应答常见 ff ff ff 00 */
#define SADP_OFF_INQ_IPV6    38     /* Inquiry: IPv6(fe80::/64) 紧跟 subnet, 无 gateway 字段 */
#define SADP_OFF_RSP_SN      38     /* InquiryResponse: 型号+序列号起始(与 Inquiry IPv6 同偏移) */
#define SADP_OFF_RSP_MODEL   354    /* InquiryResponse: 独立型号字段(32B) */

extern struct netif g_lwip_netif;
extern IPC_Info_t ipcInfo;

typedef struct
{
	uint8_t used;
	uint8_t ip[4];
	uint8_t mac[6];
	char    model[32];
} sadp_rx_item_t;

static sadp_rx_item_t s_sadp_rx_q[SADP_RX_Q_SIZE];
static uint8_t        s_sadp_rx_w;
static uint16_t       s_sadp_counter = 0x1000;

/************************************************************
* SADP 校验: 16-bit one's complement (hiktools/checksum.cpp)
* 按 LE uint16 累加; 线上 checksum 两字节为大端存放
* prefix 为待校验字节数, Inquiry 固定 0x42(66)
************************************************************/
static uint16_t sadp_read_le16(const uint8_t *p)
{
	return (uint16_t)(p[0] | (p[1] << 8));
}

static uint16_t sadp_checksum_safe(const uint8_t *body, uint32_t prefix)
{
	uint32_t checksum = 0;
	uint32_t var1 = 0;
	uint32_t var2 = 0;
	uint32_t index;
	uint32_t p = 0;

	if(3 < (prefix & 0xFFFFFFFEU))
	{
		index = ((prefix - 4) >> 2) + 1;
		do
		{
			prefix -= 4;
			var1 += sadp_read_le16(body + p);
			var2 += sadp_read_le16(body + p + 2);
			p += 4;
			index--;
		} while(index != 0);
	}

	if(1 < prefix)
	{
		checksum = sadp_read_le16(body + p);
		p += 2;
		prefix -= 2;
	}

	checksum += var1 + var2;
	if(prefix != 0)
		checksum += body[p];

	checksum = (checksum >> 16) + (checksum & 0xFFFF);
	return (uint16_t)(~((checksum >> 16) + checksum));
}

static uint8_t sadp_is_printable_ascii(char c)
{
	return (c >= 0x20 && c <= 0x7E) ? 1 : 0;
}

static void sadp_extract_model_str(const uint8_t *buf, uint16_t blen, char *model, uint8_t mlen)
{
	uint16_t i;
	uint16_t n = 0;

	if(buf == NULL || model == NULL || mlen == 0)
		return;
	model[0] = 0;

	for(i = 0; i < blen && n < (uint8_t)(mlen - 1); i++)
	{
		char c = (char)buf[i];
		if(c == 0)
			break;
		if(!sadp_is_printable_ascii(c))
			break;
		model[n++] = c;
	}
	model[n] = 0;
}

static void sadp_extract_model_from_sn_buf(const uint8_t *buf, uint16_t blen, char *model, uint8_t mlen)
{
	uint16_t i;
	uint16_t n = 0;

	if(buf == NULL || model == NULL || mlen == 0 || blen == 0)
		return;
	model[0] = 0;

	for(i = 0; i < blen && n < (uint8_t)(mlen - 1); i++)
	{
		char c = (char)buf[i];
		uint16_t j;

		if(c == 0)
			break;
		if(!sadp_is_printable_ascii(c))
			break;

		if(n > 4 && c >= '0' && c <= '9' && (c == '1' || c == '2'))
		{
			for(j = 0; j < 8; j++)
			{
				if((i + j) >= blen)
					break;
				if(buf[i + j] < '0' || buf[i + j] > '9')
					break;
			}
			if(j == 8)
				break;
		}

		model[n++] = c;
	}
	model[n] = 0;
}

static int sadp_parse_response_pbuf(struct pbuf *p, uint16_t body_off, uint16_t plen, sadp_rx_item_t *item)
{
	uint8_t hdr[14];
	uint8_t sn_tmp[SADP_SN_TMP_SIZE];
	uint16_t sn_len;

	if(p == NULL || item == NULL)
		return -1;
	if(plen < SADP_OFF_SRC_IP + 4)
		return -2;
	if(pbuf_copy_partial(p, hdr, 14, body_off) != 14)
		return -1;
	if(hdr[0] != 0x21 || hdr[3] != SADP_SERVER_PREFIX)
		return -3;
	if(hdr[SADP_OFF_PKT_TYPE] == SADP_PKT_INQUIRY)
		return -4;
	if(hdr[SADP_OFF_PKT_TYPE] != SADP_PKT_INQUIRY_RSP &&
	   hdr[SADP_OFF_PKT_TYPE] != SADP_PKT_INQUIRY_RSP_LEGACY)
		return -4;

	pbuf_copy_partial(p, item->mac, 6, body_off + SADP_OFF_SRC_MAC);
	pbuf_copy_partial(p, item->ip,  4, body_off + SADP_OFF_SRC_IP);
	item->model[0] = 0;

	if(item->model[0] == 0 && plen > SADP_OFF_RSP_SN)
	{
		sn_len = (uint16_t)(plen - SADP_OFF_RSP_SN);
		if(sn_len > SADP_SN_TMP_SIZE)
			sn_len = SADP_SN_TMP_SIZE;
		pbuf_copy_partial(p, sn_tmp, sn_len, body_off + SADP_OFF_RSP_SN);
		sadp_extract_model_from_sn_buf(sn_tmp, sn_len, item->model, (uint8_t)sizeof(item->model));
	}

	if(item->model[0] == 0 && plen > SADP_OFF_RSP_MODEL + 4)
	{
		pbuf_copy_partial(p, (uint8_t *)item->model, 31, body_off + SADP_OFF_RSP_MODEL);
		item->model[31] = 0;
		sadp_extract_model_str((uint8_t *)item->model, 31, item->model, (uint8_t)sizeof(item->model));
	}

	if((item->ip[0] | item->ip[1] | item->ip[2] | item->ip[3]) == 0)
		return -5;

	return 0;
}

err_t sadp_ethernet_input(struct pbuf *p, struct netif *netif)
{
	struct eth_hdr *ethhdr;
	uint8_t hdr0;
	uint8_t hdr3;
	uint16_t plen;
	uint8_t idx;
	sadp_rx_item_t item;

	LWIP_UNUSED_ARG(netif);

	if(p == NULL)
		return ERR_ARG;

	if(p->tot_len < SADP_ETH_HDR_LEN + SADP_OFF_SRC_IP + 4)
	{
		pbuf_free(p);
		return ERR_OK;
	}

	ethhdr = (struct eth_hdr *)p->payload;
	if(ethhdr->type != PP_HTONS(ETHTYPE_SADP))
	{
		pbuf_free(p);
		return ERR_OK;
	}

	plen = (uint16_t)(p->tot_len - SADP_ETH_HDR_LEN);
	if(pbuf_copy_partial(p, &hdr0, 1, SADP_ETH_HDR_LEN) != 1 ||
	   pbuf_copy_partial(p, &hdr3, 1, SADP_ETH_HDR_LEN + 3) != 1)
	{
		pbuf_free(p);
		return ERR_OK;
	}

	/* 忽略本机 Inquiry; 仅处理摄像机 InquiryResponse */
	if(hdr0 != 0x21 || hdr3 == SADP_CLIENT_PREFIX)
	{
		pbuf_free(p);
		return ERR_OK;
	}

	memset(&item, 0, sizeof(item));
	if(sadp_parse_response_pbuf(p, SADP_ETH_HDR_LEN, plen, &item) != 0)
	{
		pbuf_free(p);
		return ERR_OK;
	}

	idx = s_sadp_rx_w;
	memcpy(s_sadp_rx_q[idx].mac, item.mac, 6);
	memcpy(s_sadp_rx_q[idx].ip,  item.ip,  4);
	memcpy(s_sadp_rx_q[idx].model, item.model, sizeof(s_sadp_rx_q[idx].model));
	s_sadp_rx_q[idx].used = 1;
	s_sadp_rx_w = (uint8_t)((idx + 1) % SADP_RX_Q_SIZE);

	if(SADP_DEBUG)
	{
		printf("sadp rsp %d.%d.%d.%d %02x:%02x:%02x:%02x:%02x:%02x %s\r\n",
			item.ip[0], item.ip[1], item.ip[2], item.ip[3],
			item.mac[0], item.mac[1], item.mac[2],
			item.mac[3], item.mac[4], item.mac[5],
			item.model);
	}

	pbuf_free(p);
	return ERR_OK;
}

static void sadp_fill_linklocal_ipv6(uint8_t *ipv6, const uint8_t *mac)
{
	memset(ipv6, 0, 16);
	ipv6[0]  = 0xFE;
	ipv6[1]  = 0x80;
	ipv6[8]  = (uint8_t)(mac[0] ^ 0x02);
	ipv6[9]  = mac[1];
	ipv6[10] = mac[2];
	ipv6[11] = 0xFF;
	ipv6[12] = 0xFE;
	ipv6[13] = mac[3];
	ipv6[14] = mac[4];
	ipv6[15] = mac[5];
}

int sadp_send_inquiry(void)
{
	struct pbuf *p;
	uint8_t *frame;
	uint8_t *body;
	uint16_t csum;
	err_t err;

	if(g_lwipdev.netif_state == 0)
		return -1;

	p = pbuf_alloc(PBUF_RAW, SADP_ETH_HDR_LEN + SADP_INQUIRY_LEN, PBUF_RAM);
	if(p == NULL)
		return -2;

	frame = (uint8_t *)p->payload;
	memset(frame, 0, SADP_ETH_HDR_LEN + SADP_INQUIRY_LEN);

	/* Ethernet II */
	memset(&frame[0], 0xFF, 6);
	memcpy(&frame[6], g_lwipdev.mac, 6);
	frame[12] = (uint8_t)(ETHTYPE_SADP >> 8);
	frame[13] = (uint8_t)(ETHTYPE_SADP & 0xFF);

	body = &frame[SADP_ETH_HDR_LEN];
	body[0] = 0x21;
	body[1] = 0x02;
	body[2] = 0x01;
	body[3] = SADP_CLIENT_PREFIX;
	body[4] = 0x00;
	body[5] = 0x00;
	body[6] = (uint8_t)(s_sadp_counter >> 8);
	body[7] = (uint8_t)(s_sadp_counter & 0xFF);
	s_sadp_counter++;
	body[8]  = 0x06;
	body[9]  = 0x04;
	body[10] = 0x03;                /* Inquiry */
	body[11] = 0x00;
	body[12] = 0x00;
	body[13] = 0x00;
	memcpy(&body[SADP_OFF_SRC_MAC], g_lwipdev.mac, 6);
	memcpy(&body[SADP_OFF_SRC_IP],  g_lwipdev.ip,  4);
	memset(&body[SADP_OFF_DST_MAC], 0xFF, 6);
	sadp_fill_linklocal_ipv6(&body[SADP_OFF_INQ_IPV6], g_lwipdev.mac);

	csum = sadp_checksum_safe(body, SADP_CLIENT_PREFIX);
	body[SADP_OFF_CHECKSUM]     = (uint8_t)(csum >> 8);
	body[SADP_OFF_CHECKSUM + 1] = (uint8_t)(csum & 0xFF);

	err = g_lwip_netif.linkoutput(&g_lwip_netif, p);
	pbuf_free(p);

	if(SADP_DEBUG)
		printf("sadp inquiry send %d\r\n", (int)err);

	return (err == ERR_OK) ? 0 : -3;
}

int sadp_recv_process(void)
{
	uint8_t i;
	uint8_t cnt = 0;
	uint8_t ip[4];
	uint8_t mac[6];
	char model[32];
	uint8_t used;

	for(i = 0; i < SADP_RX_Q_SIZE; i++)
	{
		used = s_sadp_rx_q[i].used;
		if(!used)
			continue;

		memcpy(ip, s_sadp_rx_q[i].ip, 4);
		memcpy(mac, s_sadp_rx_q[i].mac, 6);
		memcpy(model, s_sadp_rx_q[i].model, sizeof(model));
		s_sadp_rx_q[i].used = 0;

		if((ip[0] | ip[1] | ip[2] | ip[3]) == 0)
			continue;
		if((mac[0]|mac[1]|mac[2]|mac[3]|mac[4]|mac[5]) == 0)
			continue;
		if(memcmp(ip, g_lwipdev.ip, 4) == 0)
			continue;

		if(onvif_match_camera_ip(ip) != 0)
			continue;

		if(ipcInfo.ipc_num >= IPC_NUM_MAX)
			break;

		memcpy(ipcInfo.ipc_param[ipcInfo.ipc_num].ip, ip, 4);
		memcpy(ipcInfo.ipc_param[ipcInfo.ipc_num].mac, mac, 6);
		if(model[0] != 0)
			sprintf(ipcInfo.ipc_param[ipcInfo.ipc_num].model, "%s", model);
		else
			sprintf(ipcInfo.ipc_param[ipcInfo.ipc_num].model, "%s", "HIKVISION");
		ipcInfo.ipc_param[ipcInfo.ipc_num].brand = HIKVISION;
		ipcInfo.ipc_num++;
		cnt++;

		if(SADP_DEBUG)
		{
			printf("sadp add brand=HIKVISION model=%s ip=%d.%d.%d.%d mac=%02x:%02x:%02x:%02x:%02x:%02x\r\n",
				model, ip[0], ip[1], ip[2], ip[3],
				mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
	}
	return cnt;
}

void sadp_reset(void)
{
	uint8_t i;

	for(i = 0; i < SADP_RX_Q_SIZE; i++)
		s_sadp_rx_q[i].used = 0;
	s_sadp_rx_w = 0;
}
