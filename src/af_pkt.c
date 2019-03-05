#include <unistd.h>
#include <sys/time.h>
#include "common.h"
#include "af_pkt.h"

#define D2D_PROTO   0x9898

int g_ifindex = -1;
int g_l2_proto = ETH_P_ALL;// D2D_PROTO;

int get_ifindex(int fd, const char *if_name)
{
    int ret;
    struct ifreq ifr;
    size_t len = strlen(if_name);

    ASSERT(len < sizeof(ifr.ifr_name), "interface name <%s> too big", if_name);
    memcpy(ifr.ifr_name, if_name, len);
    ifr.ifr_name[len] = 0;

    ret = ioctl(fd, SIOCGIFINDEX, &ifr);
    ASSERT(ret != -1, "ioctl failed");

    return ifr.ifr_ifindex;
}

int get_mac_addr(const char *addr, uint8_t *mac, size_t len)
{
    char addrstr[128];
    char *ptr = addrstr, *state = NULL;
    int i = 0;

    strncpy(addrstr, addr, sizeof(addrstr));
    while((ptr = strtok_r(ptr, ":", &state)))
    {
        printf("ptr=[%s]\n", ptr);
        mac[i++] = strtol(ptr, NULL, 16);
        if(i >= len) break;
        ptr = NULL;
    }
    ASSERT(i == len, "get_mac_addr failed");
    return SUCCESS;
}

int create_sock(const char *if_name)
{
    int fd;

    fd = socket(AF_PACKET, SOCK_DGRAM, htons(g_l2_proto));
    ASSERT(fd >= 0, "socket failed %m");

    g_ifindex = get_ifindex(fd, if_name);

    return fd;
}

typedef struct _d2d_hdr_
{
    int seq;
}d2d_hdr_t;

int g_seq = 0;
int fill_buf(uint8_t *buf, int mtu)
{
    d2d_hdr_t hdr = { 0 };

    hdr.seq = g_seq++;
    memcpy(buf, &hdr, sizeof(hdr));
    return mtu;
}

int sender(int fd, const uint8_t *mac, size_t maclen, const int mtu)
{
    struct sockaddr_ll lladdr = {0};
    uint8_t buf[MAX_MAC_MTU] = { 1 };
    int len, ret, i;

    lladdr.sll_family = AF_PACKET;
    lladdr.sll_ifindex = g_ifindex;
    lladdr.sll_halen = maclen;
    lladdr.sll_protocol = htons(D2D_PROTO);
    memcpy(lladdr.sll_addr, mac, maclen);

    for(i=0;i<1000;i++)
    {
        len = fill_buf(buf, mtu);
        ret = sendto(fd, buf, len, 0, (struct sockaddr*)&lladdr, sizeof(lladdr));
        usleep(0);
    }
    return SUCCESS;
}

int calc_rate(struct timeval *stv, struct timeval *etv, size_t bytes_rcvd)
{
    int ms;
    double MBps;

    ms = (((etv->tv_sec - stv->tv_sec)*1000) + ((etv->tv_usec - stv->tv_usec)/1000));
    MBps = (double)(bytes_rcvd/(1024*1024))/(double)(ms/1000);
    INFO("Statistics:\n");
    INFO("Bytes rcvd=%zu\n", bytes_rcvd);
    INFO("time in ms=%d\n", ms);
    INFO("MBps=%.2f\n", MBps);
}

void set_timeout(int fd, int ms)
{
    int ret;
    struct timeval tv;

    tv.tv_sec = ms/1000;
    tv.tv_usec = (ms%1000)*1000;
    ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ASSERT(ret != -1, "setsockopt failed %m");
}

int receiver(int fd)
{
    struct sockaddr_ll lladdr;
    socklen_t slen = sizeof(lladdr);
    uint8_t buf[MAX_MAC_MTU];
    ssize_t n;
    int ret, last_seq = -1;
    size_t tot_bytes = 0;
    d2d_hdr_t *hdr;
    struct timeval end_tv, start_tv;

    memset(&lladdr, 0, sizeof(lladdr));
    lladdr.sll_family = AF_PACKET;
    lladdr.sll_ifindex = g_ifindex;
    lladdr.sll_protocol = htons(D2D_PROTO);

    ret = bind(fd, (struct sockaddr*)&lladdr, sizeof(lladdr));
    ASSERT(ret != -1, "bind failed %m");

    while(1)
    {
        tot_bytes = 0;
        last_seq = -1;
        gettimeofday(&start_tv, NULL);
        while(1)
        {
            n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*)&lladdr, &slen);
            if(n <= 0)
            {
                usleep(1000);
                break;
            }
            if(!tot_bytes) set_timeout(fd, 200);
            hdr = (d2d_hdr_t *)buf;
            if(last_seq != -1 && (last_seq + 1 != hdr->seq))
            {
                INFO("got out of seq, exp=%d got=%d\n", last_seq+1, hdr->seq);
            }
            last_seq = hdr->seq;
            tot_bytes += (size_t)n;
            gettimeofday(&end_tv, NULL);
        }
        set_timeout(fd, 0);
        calc_rate(&start_tv, &end_tv, tot_bytes);
    }
    return SUCCESS;
}
