#include "main.h"
#include "./Task/inc/gprs_rx.h"


/* �����������? */
StreamBufferHandle_t g_gprs_rx_streambuf = NULL;

/* ���� gprs_rx_buff / gprs_rx_status / gprs_rx_take_point ������ */
SemaphoreHandle_t g_gprs_rx_mutex = NULL;

/* ��������֡���� */
static TaskHandle_t s_gprs_rx_task_handler = NULL;
static uint8_t      s_gprs_rx_frame[GSM_RX_BUFF_SIZE + 1]; /* ����1�����DMA֡����'\0' */

/* ��֡����ݴ�?: DMA �߽���ܽ�һ�? URC ��ʼ "\r\n+MIPURC:..." �зֵ����� chunk,
 * ���µ� chunk �� strncmp ƥ��ʧ�ܡ�֡��ʧ�����ۼ�������δ����������?,
 * ÿ���� chunk ����ʱ��ƴ���ٰ� \r\n �߽�(�� rtcp payload ����)��ȡ����֡���ɡ�
 * ���ۼ���������Ϊ GPRS_RX_ACC_BUF_SIZE,���� DMA ֡(2048) + ���δ�ɷ���URC(Լ1500) */
static uint8_t  s_rx_acc_buf[GSM_RX_BUFF_SIZE];
static uint16_t s_rx_acc_len = 0;

/* ���ۼӻ����������� 2 �ֽ�ģʽ(�����? TU ���õ� static my_memmem) */
static void *rx_acc_memmem(const void *haystack, uint16_t hs_len,
                           const void *needle,   uint16_t nd_len)
{
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *e;
    if(!nd_len || hs_len < nd_len) { return NULL; }
    e = h + hs_len - nd_len;
    while(h <= e)
    {
        if(h[0] == ((const unsigned char *)needle)[0] && memcmp(h, needle, nd_len) == 0)
            return (void *)h;
        h++;
    }
    return NULL;
}

/* �ۼ��� pos ���Ƿ�Ϊ rtcp URC ͷ; ���� +MIPURC: ���пո�/���е����? */
static int rx_acc_rtcp_prefix_len(uint16_t pos)
{
    uint16_t p = pos;
    int prefix_len = 0;

    if((pos + 10U) <= s_rx_acc_len
        && !strncmp((char *)(s_rx_acc_buf + pos), "\r\n+MIPURC:", 10))
    {
        prefix_len = 10;
    }
    else if((pos + 8U) <= s_rx_acc_len
        && !strncmp((char *)(s_rx_acc_buf + pos), "+MIPURC:", 8))
    {
        prefix_len = 8;
    }
    else
    {
        return 0;
    }

    p = (uint16_t)(pos + (uint16_t)prefix_len);
    while(p < s_rx_acc_len &&
          (s_rx_acc_buf[p] == ' ' || s_rx_acc_buf[p] == '\t' ||
           s_rx_acc_buf[p] == '\r' || s_rx_acc_buf[p] == '\n'))
    {
        p++;
    }

    if((p + 6U) <= s_rx_acc_len && !strncmp((char *)(s_rx_acc_buf + p), "\"rtcp\"", 6))
    {
        return prefix_len;
    }
    if((p + 9U) <= s_rx_acc_len && !strncmp((char *)(s_rx_acc_buf + p), "\"disconn\"", 9))
    {
        return 0;
    }

    return prefix_len; /* rtcp ���Ʊ����ڷ�Ƭ�ض�,�����ȴ���һ chunk */
}

/* �� \r\n ���׵��� AT �첽 URC(+MIPOPEN/+MIPCLOSE/+MIPSEND/+CME ERROR) */
static int rx_acc_bare_at_urc_prefix_len(uint16_t pos)
{
    if((pos + 9U) <= s_rx_acc_len && !strncmp((char *)(s_rx_acc_buf + pos), "+MIPOPEN:", 9))
    {
        return 9;
    }
    if((pos + 10U) <= s_rx_acc_len && !strncmp((char *)(s_rx_acc_buf + pos), "+MIPCLOSE:", 10))
    {
        return 10;
    }
    if((pos + 9U) <= s_rx_acc_len && !strncmp((char *)(s_rx_acc_buf + pos), "+MIPSEND:", 9))
    {
        return 9;
    }
    if((pos + 11U) <= s_rx_acc_len && !strncmp((char *)(s_rx_acc_buf + pos), "+CME ERROR:", 11))
    {
        return 11;
    }
    return 0;
}

/* AT/MIPSEND �ڼ�: �� acc ��������ȡ���ַ� '>' ��ʾ, ���ⱻ rtcp ��֡������
 * ���ڳ��� g_gprs_rx_mutex ʱ����; �ɹ����д s_rx_acc_buf/s_rx_acc_len ������ 1 */
static int rx_acc_try_dispatch_mipsend_prompt(void)
{
    uint16_t i;
    uint16_t prompt_at;
    uint16_t prompt_len;
    uint16_t remove_from;
    uint16_t remove_len;

    if(!gprs_at_cmdon_active())
    {
        return 0;
    }

    for(i = 0U; (i + 2U) < s_rx_acc_len; i++)
    {
        if(s_rx_acc_buf[i] != '\r' || s_rx_acc_buf[i + 1U] != '\n' || s_rx_acc_buf[i + 2U] != '>')
        {
            continue;
        }

        prompt_at = (uint16_t)(i + 2U);
        prompt_len = 1U;
        if((prompt_at + 1U) < s_rx_acc_len && s_rx_acc_buf[prompt_at + 1U] == ' ')
        {
            prompt_len = 2U;
        }

        remove_from = i;
        remove_len = (uint16_t)(2U + prompt_len);
        if((remove_from + remove_len + 1U) < s_rx_acc_len
            && s_rx_acc_buf[remove_from + remove_len] == '\r'
            && s_rx_acc_buf[remove_from + remove_len + 1U] == '\n')
        {
            remove_len = (uint16_t)(remove_len + 2U);
        }

        gprs_get_receive_data_function(s_rx_acc_buf + prompt_at, prompt_len);
        memmove(s_rx_acc_buf + remove_from,
                s_rx_acc_buf + remove_from + remove_len,
                (size_t)(s_rx_acc_len - remove_from - remove_len));
        s_rx_acc_len = (uint16_t)(s_rx_acc_len - remove_len);
        return 1;
    }

    if(s_rx_acc_len >= 1U && s_rx_acc_buf[0] == '>')
    {
        prompt_at = 0U;
        prompt_len = 1U;
        if(s_rx_acc_len >= 2U && s_rx_acc_buf[1] == ' ')
        {
            prompt_len = 2U;
        }

        remove_from = 0U;
        remove_len = prompt_len;
        if((remove_len + 1U) < s_rx_acc_len
            && s_rx_acc_buf[remove_len] == '\r'
            && s_rx_acc_buf[remove_len + 1U] == '\n')
        {
            remove_len = (uint16_t)(remove_len + 2U);
        }

        gprs_get_receive_data_function(s_rx_acc_buf + prompt_at, prompt_len);
        if(remove_len < s_rx_acc_len)
        {
            memmove(s_rx_acc_buf, s_rx_acc_buf + remove_len, (size_t)(s_rx_acc_len - remove_len));
        }
        s_rx_acc_len = (uint16_t)(s_rx_acc_len - remove_len);
        return 1;
    }

    return 0;
}

/* ���ۼӻ���������ȡ��������������֡,δ�����ѵ�β�������� s_rx_acc_buf[0..len-1]
 * ���÷����ѳ��� g_gprs_rx_mutex */
static void gprs_rx_dispatch_complete_frames(void)
{
    uint16_t pos = 0;

    while(pos < s_rx_acc_len)
    {
        /* MIPSEND ����ʾ�� '>' / '> ': �� \r\n �а�װ,���н��������в����� */
        if(s_rx_acc_buf[pos] == '>')
        {
            uint16_t prompt_len = 1;
            if((pos + 1U) < s_rx_acc_len && s_rx_acc_buf[pos + 1] == ' ')
            {
                prompt_len = 2;
            }
            gprs_get_receive_data_function(s_rx_acc_buf + pos, prompt_len);
            pos += prompt_len;
            continue;
        }

        /* �� AT �첽 URC(�� \r\n ǰ׺): ML307 ���������? +MIPOPEN: id,0\r\n */
        if(rx_acc_bare_at_urc_prefix_len(pos))
        {
            char *rn = (char *)rx_acc_memmem(s_rx_acc_buf + pos,
                                               (uint16_t)(s_rx_acc_len - pos),
                                               "\r\n", 2);
            if(!rn)
            {
                if(rx_acc_try_dispatch_mipsend_prompt()) { pos = 0U; continue; }
                return;
            }

            uint16_t line_len = (uint16_t)((rn + 2) - (char *)(s_rx_acc_buf + pos));
            /* ML307 �� URC �� \r\n ǰ׺; ���ǰ����, �� gprs_wait_feedback �� \r\n+URC ƥ�� */
            if((line_len + 2U) <= 128U)
            {
                uint8_t norm_line[128];
                norm_line[0] = '\r';
                norm_line[1] = '\n';
                memcpy(norm_line + 2, s_rx_acc_buf + pos, line_len);
                gprs_get_receive_data_function(norm_line, (uint16_t)(line_len + 2U));
            }
            pos += line_len;
            continue;
        }

        /* rtcp URC: ֡���� GPRS.c �� gprs_parse_rtcp_urc ����һ��,���ⶨ��ƫ�β�� */
        {
            int rtcp_pfx = rx_acc_rtcp_prefix_len(pos);
            if(rtcp_pfx)
            {
                int frame_len = gprs_rtcp_urc_frame_size(s_rx_acc_buf + pos,
                                                         (uint16_t)(s_rx_acc_len - pos));
                if(frame_len <= 0)
                {
                    if(rx_acc_try_dispatch_mipsend_prompt()) { pos = 0U; continue; }
                    return;
                }
                if((pos + (uint16_t)frame_len) > s_rx_acc_len)
                {
                    if(rx_acc_try_dispatch_mipsend_prompt()) { pos = 0U; continue; }
                    return;
                }

                gprs_get_receive_data_function(s_rx_acc_buf + pos, (uint16_t)frame_len);
                pos += (uint16_t)frame_len;
                continue;
            }
        }

        if((s_rx_acc_len - pos) < 4)  /* ������Ҫ��β��һ�� \r\n */
        {
            if(rx_acc_try_dispatch_mipsend_prompt()) { pos = 0U; continue; }
            return; /* ��βδ����,�����ȴ���һ chunk */
        }

        /* ������ÿ�о��� \r\n ��ͷ(��������β������),��ǰλ�ñ���λ���б߽� */
        if(s_rx_acc_buf[pos] != '\r' || s_rx_acc_buf[pos+1] != '\n')
        {
            /* ��ͬ��: �����ں����ֽ����� +MIPURC ���¶���,�������� HTTP URC ����ɾ */
            char *sync = (char *)rx_acc_memmem(s_rx_acc_buf + pos + 1U,
                                               (uint16_t)(s_rx_acc_len - pos - 1U),
                                               "+MIPURC:", 8);
            if(sync)
            {
                pos = (uint16_t)(sync - (char *)s_rx_acc_buf);
                continue;
            }
            sync = (char *)rx_acc_memmem(s_rx_acc_buf + pos + 1U,
                                         (uint16_t)(s_rx_acc_len - pos - 1U),
                                         "+MIPURC: \"rtcp\"", 14);
            if(sync)
            {
                pos = (uint16_t)(sync - (char *)s_rx_acc_buf);
                continue;
            }
            sync = (char *)rx_acc_memmem(s_rx_acc_buf + pos + 1U,
                                         (uint16_t)(s_rx_acc_len - pos - 1U),
                                         "+MIPOPEN:", 9);
            if(sync)
            {
                pos = (uint16_t)(sync - (char *)s_rx_acc_buf);
                continue;
            }
            sync = (char *)rx_acc_memmem(s_rx_acc_buf + pos + 1U,
                                         (uint16_t)(s_rx_acc_len - pos - 1U),
                                         "+MIPCLOSE:", 10);
            if(sync)
            {
                pos = (uint16_t)(sync - (char *)s_rx_acc_buf);
                continue;
            }
            /* ����δ����β��,�ȴ���һ chunk ��ȫ,���� HTTP �� URC ���������? */
            if(pos > 0U)
            {
                memmove(s_rx_acc_buf, s_rx_acc_buf + pos, (size_t)(s_rx_acc_len - pos));
                s_rx_acc_len -= pos;
            }
            if(rx_acc_try_dispatch_mipsend_prompt()) { pos = 0U; continue; }
            return;
        }

        /* ������ \r\n ֮��������β \r\n */
        char *rn = (char *)rx_acc_memmem(s_rx_acc_buf + pos + 2,
                                         (uint16_t)(s_rx_acc_len - pos - 2),
                                         "\r\n", 2);
        if(!rn)
        {
            if(rx_acc_try_dispatch_mipsend_prompt()) { pos = 0U; continue; }
            return;
        }  /* ��βδ����,�����ȴ���һ chunk */

        uint16_t line_len = (uint16_t)((rn + 2) - (char *)(s_rx_acc_buf + pos));

        /* --- �ı� URC ͷ�� DMA �зֵ� chunk �߽�ı���? ---
         * line_len < 18 ���� "\r\n+MIPURC: \"" ��ͷ(13 �ֽ�),
         * ������ rtcp/disconn ����Ƭͷ,�����ȴ���һ chunk ��ȫ�� */
        if(line_len < 18 && line_len >= 13 &&
           !strncmp((char *)(s_rx_acc_buf + pos), "\r\n+MIPURC: \"", 13))
        {
            if(rx_acc_try_dispatch_mipsend_prompt()) { pos = 0U; continue; }
            return;
        }

        /* --- ��ͨ�ı���(AT ���� / ����Ӧ�� / ���� disconn URC) --- */
        gprs_get_receive_data_function(s_rx_acc_buf + pos, line_len);
        pos += line_len;
    }

    if(pos > 0)
    {
        if(pos < s_rx_acc_len)
            memmove(s_rx_acc_buf, s_rx_acc_buf + pos, (size_t)(s_rx_acc_len - pos));
        s_rx_acc_len -= pos;
    }
}

/* ǰ������ */
static void gprs_rx_task_function(void *pvParameters);

/*
*********************************************************************************************************
*    �� �� ��: gprs_rx_streambuf_init_function
*    ����˵��: ����GPRS����������������մ�������?
*             ������һ��;���ڴ��ڽ���ʹ��֮ǰ����,��ȷ���ж����õĻ������Ѿ���
*    ��    ��: ��
*    �� �� ֵ: ��
*********************************************************************************************************
*/
void gprs_rx_streambuf_init_function(void)
{
    static uint8_t s_inited = 0;
    if(s_inited) { return; }
    s_inited = 1;

    /* �����ֽ���=1:�������ֽڼ���������,��֤AT��Ӧ/URC���ӳٴ��� */
    g_gprs_rx_streambuf = xStreamBufferCreate(GPRS_RX_STREAMBUF_SIZE, 1);
    configASSERT(g_gprs_rx_streambuf);

    g_gprs_rx_mutex = xSemaphoreCreateMutex();
    configASSERT(g_gprs_rx_mutex);

    xTaskCreate((TaskFunction_t  )gprs_rx_task_function,
                (const char *    )"gprs_rx_task",
                (uint16_t        )GPRS_RX_STK_SIZE,
                (void *          )NULL,
                (UBaseType_t     )GPRS_RX_TASK_PRIO,
                (TaskHandle_t *  )&s_gprs_rx_task_handler);
}

/*
*********************************************************************************************************
*    �� �� ��: gprs_rx_task_function
*    ����˵��: GPRS�������ݴ�������
*             ����������ȡ�������ж�д����ֽ���?(����Ϊ����ֶ�?,����֤֡�߽�),
*             �ڱ����������������ԭgprs_get_receive_data_function�Ľ�������
*             (memcpy/�ַ�������/���ݻ���),������Щ��ʱ����ռ�ô����жϡ�
*             ���ȼ�����gsm����,��֤ URC ����(rx_buf/ota/file)��
 *             gsm �����дǰ�ѱ�����·��?,ά��ԭ�д���ʱ��
*             ע:HTTP�ļ��շ�/�Զ���Э�����ʽ���������ڴ�������������չʵ�֡�?
*    ��    ��: pvParameters : δʹ��
*    �� �� ֵ: ��
*********************************************************************************************************
*/
static void gprs_rx_task_function(void *pvParameters)
{
    size_t chunk_len = 0;

    (void)pvParameters;

    for(;;)
    {
        /* �����ȴ������ж�Ͷ�ݵ��ֽ���;�����ݼ�����(����һ֡����),����������
         * buffer�ǿ�ʱ�����÷���������,�Ӷ���Ȼ��ɻ������ſ�? */
        chunk_len = xStreamBufferReceive(g_gprs_rx_streambuf,
                                        (void *)s_gprs_rx_frame,
                                        sizeof(s_gprs_rx_frame) - 1,
                                        portMAX_DELAY);
        if(chunk_len == 0) { continue; }

        /* ���ַ���������,��֤�����ַ�������(strncmp/strchr/atoi)��ȫ */
        s_gprs_rx_frame[chunk_len] = 0;

        /* ���Ᵽ�� gprs_rx_buff / gprs_rx_status / gprs_rx_take_point
         * gsm����(���ȼ�8)ͬʱ�ڶ�(gprs_wait_feedback) */
        xSemaphoreTake(g_gprs_rx_mutex, portMAX_DELAY);
        {
            /* ׷�ӵ���֡�ۼ��������ۼ������?(����:��һ��β����Ƭ + ����
             * ������֡��������),�����ۼ�����������Ƭ,�����ڴ�Խ�硣 */
            if((uint16_t)(s_rx_acc_len + chunk_len) > sizeof(s_rx_acc_buf))
            {
                s_rx_acc_len = 0;
            }
            memcpy(s_rx_acc_buf + s_rx_acc_len, s_gprs_rx_frame, chunk_len);
            s_rx_acc_len += (uint16_t)chunk_len;

            /* ��ȡ��������֡��һ dispatch;δ��ɵ�β�����?�������ۼ����� */
            gprs_rx_dispatch_complete_frames();
        }
        xSemaphoreGive(g_gprs_rx_mutex);
    }
}
