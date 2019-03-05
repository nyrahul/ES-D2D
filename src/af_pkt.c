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

    fd = socket(AF_PACKET, SOCK_RAW, htons(g_l2_proto));
    ASSERT(fd >= 0, "socket failed %m");

    g_ifindex = get_ifindex(fd, if_name);

    return fd;
}

int send_packet(int fd, struct sockaddr_ll *lladdr, const uint8_t *buf, size_t buflen)
{
    int ret;

    ret = sendto(fd, buf, buflen, 0, (struct sockaddr*)lladdr, sizeof(*lladdr));
    INFO("sendto ret=%d %m fd=%d\n", ret, fd);
    return ret;
}

int sender(int fd, const uint8_t *mac, size_t maclen, const int mtu)
{
    struct sockaddr_ll lladdr = {0};
    uint8_t buf[MAX_MAC_MTU] = { 1 };

    lladdr.sll_family = AF_PACKET;
    lladdr.sll_ifindex = g_ifindex;
    lladdr.sll_halen = maclen;
    lladdr.sll_protocol = htons(g_l2_proto);
    memcpy(lladdr.sll_addr, mac, maclen);

    send_packet(fd, &lladdr, buf, mtu);
    return SUCCESS;
}

int receiver(int fd)
{
    struct sockaddr_ll lladdr;
    socklen_t slen = sizeof(lladdr);
    uint8_t buf[MAX_MAC_MTU];
    ssize_t n;

    while(1)
    {
        n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*)&lladdr, &slen);
        INFO("recvfrom n=%ld\n", n);
    }
    return SUCCESS;
}
