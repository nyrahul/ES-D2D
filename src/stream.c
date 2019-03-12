#include "common.h"
#include "stream.h"
#include <sys/socket.h>

// Stream currently handles lost packets but not unordered packets!
// Stream currently does not handle duplicate packets
// Stream sequence number always starts with 1

extern int g_mtu;

void stream_init(stream_info_t *si, int fd)
{
    memset(si, 0, sizeof(stream_info_t));
    si->fd = fd;
}

int stream_getstats(stream_info_t *si, struct timeval *stv, struct timeval *etv)
{
    int ms;
    double MBps;

    ms   = diffms(stv, etv);
    MBps = (double)(si->rx_data_bytes/(1024*1024))/(double)(ms/1000);

    INFO("\nRX Statistics:\n");
    INFO("Total_bytes rcvd=%zu\n", si->rx_tot_bytes);
    INFO("Data_bytes rcvd=%zu\n", si->rx_data_bytes);
    INFO("Lost pkts=%d\n", si->lost_cnt);
    INFO("SNACKs=%d\n", si->tx_snack);
    INFO("pkts rcvd=%d\n", si->rx_num_pkts);
    INFO("Avg pkt sz=%.2f\n", (float)(si->rx_tot_bytes/si->rx_num_pkts));
    INFO("time in ms=%d\n", ms);
    INFO("Thruput=%.2f MBps\n", MBps);
}


int stream_send_snack(stream_info_t *si)
{
    uint8_t buf[MAX_MAC_MTU];
    uint8_t *ptr = buf + sizeof(d2d_hdr_t);
    int len = sizeof(d2d_hdr_t);
    d2d_hdr_t *hdr = (d2d_hdr_t*)buf;
    int i, ret;

    hdr->seq = 0;
    hdr->snack = 1;

    for(i = 0; i < MAX_PKT_LOST; i++)
    {
        if(si->lost[i])
        {
            memcpy(ptr, &si->lost[i], sizeof(si->lost[i]));
            ptr += sizeof(si->lost[i]);
            len += sizeof(si->lost[i]);
            if(g_mtu - len < 10) break;
        }
    }

    if(len <= sizeof(d2d_hdr_t))
        return FAILURE;
    ret = sendto(si->fd, buf, len, 0,
            (struct sockaddr*)&si->remaddr, sizeof(si->remaddr));
    if(ret <= 0)
    {
        ERROR("SNACK sendto failed %m fd=%d ret=%d\n", si->fd, ret);
    }
    else
    {
        si->tx_snack++;
    }

    return SUCCESS;
}

int stream_handle_loss(stream_info_t *si, d2d_hdr_t *hdr)
{
    int i;
    int start_seq = si->last_seq + 1, end_seq = hdr->seq;

    si->lost_cnt += (end_seq - start_seq);
    for(i = 0; i < MAX_PKT_LOST; i++)
    {
        if(!si->lost[i])
        {
            si->lost[i] = start_seq++;
            if(start_seq == end_seq) break;
        }
    }
    if(start_seq != end_seq)
    {
        ERROR("pkts from seq=%d to %d could not be snacked\n",
                start_seq, end_seq);
    }
    return SUCCESS;
}

int handle_retry(stream_info_t *si, d2d_hdr_t *hdr)
{
    int i;
    for(i = 0; i < MAX_PKT_LOST; i++)
    {
        if(si->lost[i] == hdr->seq)
        {
            si->lost[i] = 0;
            break;
        }
    }
    if(i == MAX_PKT_LOST)
    {
        // Lost packet might be retried more than once and recvd more than once
        ERROR("RETRIED pkt not found in LOST list seq=%d\r", hdr->seq);
        return FAILURE;
    }

    return SUCCESS;
}

int stream_handle_pkt(stream_info_t *si, const uint8_t *buf, int n)
{
    d2d_hdr_t *hdr = NULL;

    si->rx_tot_bytes  += (size_t)n;
    si->rx_data_bytes += (size_t)(n - sizeof(d2d_hdr_t));
    si->rx_num_pkts++;
    hdr = (d2d_hdr_t *)buf;
    if(hdr->seq < si->last_seq)
    {
        if(handle_retry(si, hdr) == FAILURE)
        {
            // packet retried more than once and rcvd more than once
            si->rx_data_bytes -= (size_t)(n - sizeof(d2d_hdr_t));
        }
        return SUCCESS;
    }
    if(si->last_seq + 1 != hdr->seq)
    {
        stream_handle_loss(si, hdr);
    }
    si->last_seq = hdr->seq;
}

