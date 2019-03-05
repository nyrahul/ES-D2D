#include <unistd.h>
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

int receiver(int fd)
{
    struct sockaddr_ll lladdr;
    socklen_t slen = sizeof(lladdr);
    uint8_t buf[MAX_MAC_MTU];
    ssize_t n;
    int ret, last_seq = -1;
    d2d_hdr_t *hdr;

    memset(&lladdr, 0, sizeof(lladdr));
    lladdr.sll_family = AF_PACKET;
    lladdr.sll_ifindex = g_ifindex;
    lladdr.sll_protocol = htons(D2D_PROTO);

    ret = bind(fd, (struct sockaddr*)&lladdr, sizeof(lladdr));
    ASSERT(ret != -1, "bind failed %m");

    while(1)
    {
        n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*)&lladdr, &slen);
        if(n <= 0)
        {
            ERROR("recvfrom n=%ld %m\n", n);
            usleep(1000);
            continue;
        }
        hdr = (d2d_hdr_t *)buf;
        if(last_seq != -1 && (last_seq + 1 != hdr->seq))
        {
            INFO("got out of seq, exp=%d got=%d\n", last_seq+1, hdr->seq);
        }
        last_seq = hdr->seq;
    }
    return SUCCESS;
}
