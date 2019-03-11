#ifndef _STREAM_H_
#define _STREAM_H_

#include <linux/if_packet.h>

typedef struct _d2d_hdr_
{
    int seq;
    uint8_t snack:1;    //Selective Negative ACK
}d2d_hdr_t;

#define MAX_PKT_LOST    512
typedef struct _stream_
{
    struct sockaddr_ll remaddr;
    int     fd;
    int     last_seq;
    size_t  rx_tot_bytes; //includes d2d hdr, retry and data bytes
    size_t  rx_data_bytes;//only non-retried data bytes for app
    int     rx_num_pkts;
    int     lost[MAX_PKT_LOST];
    int     lost_cnt;
    int     tx_snack;
}stream_info_t;

void stream_init(stream_info_t *si, int fd);
int  stream_handle_pkt(stream_info_t *si, const uint8_t *buf, int n);
int  stream_getstats(stream_info_t *si, struct timeval *stv, struct timeval *etv);
int  stream_send_snack(stream_info_t *si);

#endif // _STREAM_H_
