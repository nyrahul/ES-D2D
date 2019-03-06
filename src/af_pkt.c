#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "common.h"
#include "af_pkt.h"
#include "stream.h"

#define D2D_PROTO   0x9898

int g_ifindex = -1;
int g_l2_proto = ETH_P_ALL;// D2D_PROTO;
extern int g_mtu;

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

int fill_buf(uint8_t *buf, int mtu)
{
    static int seq = 0;
    d2d_hdr_t hdr = { 0 };

    hdr.seq = ++seq;
    memcpy(buf, &hdr, sizeof(hdr));
    return mtu;
}

FILE *g_readfp = NULL;
pthread_mutex_t g_sender_mutex;

int send_pkt_from_file(int fd, FILE *fp, int seq, struct sockaddr_ll *lladdr)
{
    uint8_t buf[MAX_MAC_MTU];
    int ret, len;
    d2d_hdr_t *hdr = (d2d_hdr_t *)buf;

    ret = fseek(fp, (seq-1)*(g_mtu-sizeof(d2d_hdr_t)), SEEK_SET);
    if(ret) return FAILURE;

    hdr->seq = seq;
    len = fread(buf+sizeof(d2d_hdr_t), 1, g_mtu-sizeof(d2d_hdr_t), fp);
    if(len <= 0)
    {
        ERROR("fread failed seq=%d, len=%d\n", seq, len);
        return FAILURE;
    }
    ret = sendto(fd, buf, len+sizeof(d2d_hdr_t), 0, (struct sockaddr*)lladdr, sizeof(*lladdr));
    if(ret <= 0)
    {
        ERROR("sendto failed %m ret=%d, fd=%d len=%d\n", ret, fd, len);
        return FAILURE;
    }
    return SUCCESS;
}

void *snack_receiver(void *arg)
{
    struct sockaddr_ll remaddr;
    socklen_t slen = sizeof(remaddr);
    int fd = (int)(uintptr_t)arg, n;
    uint8_t buf[MAX_MAC_MTU];
    d2d_hdr_t *hdr = (d2d_hdr_t *)buf;
    uint8_t *ptr = buf + sizeof(d2d_hdr_t);
    long org_loc;
    int ret;

    while(1)
    {
        n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*)&remaddr, &slen);
        if(n <= 0)
        {
            ERROR("rcvd n=%d in snack thread\n", n);
            continue;
        }
        if(!hdr->snack)
        {
            ERROR("rcvd non-snack packet in the snack-thread\n");
            continue;
        }
        ptr = buf + sizeof(d2d_hdr_t);
        n -= sizeof(d2d_hdr_t);
        pthread_mutex_lock(&g_sender_mutex);
        org_loc = ftell(g_readfp);
        while(n > 0)
        {
            int seq;
            memcpy(&seq, ptr, sizeof(seq));
            ptr += sizeof(seq);
            n -= sizeof(seq);
            send_pkt_from_file(fd, g_readfp, seq, &remaddr);
            printf("%d ", seq);
        }
        fseek(g_readfp, org_loc, SEEK_SET);
        pthread_mutex_unlock(&g_sender_mutex);
    }
    return NULL;
}

int sender(int fd, FILE *fp, const uint8_t *mac, size_t maclen, const int mtu)
{
    struct sockaddr_ll lladdr = {0};
    uint8_t buf[MAX_MAC_MTU];
    int len, ret, i, seq;

    if(fp)
    {
        pthread_t tid;
        g_readfp = fp;
        ret = pthread_create(&tid, NULL, snack_receiver, (void*)(uintptr_t)fd);
    }

    lladdr.sll_family = AF_PACKET;
    lladdr.sll_ifindex = g_ifindex;
    lladdr.sll_halen = maclen;
    lladdr.sll_protocol = htons(D2D_PROTO);
    memcpy(lladdr.sll_addr, mac, maclen);

    if(fp)
    {
        int nmemb = mtu-sizeof(d2d_hdr_t);
        INFO("sending from file mtu=%d d2d_hdr_sz=%zu, nmemb=%d...\n",
                mtu, sizeof(d2d_hdr_t), nmemb);
        ret = SUCCESS;
        seq = 1;
        while(ret == SUCCESS)
        {
            pthread_mutex_lock(&g_sender_mutex);
            ret = send_pkt_from_file(fd, fp, seq, &lladdr);
            pthread_mutex_unlock(&g_sender_mutex);
            seq++;
        }
    }
    else
    {
        for(i=0;i<100000;i++)
        {
            len = fill_buf(buf, mtu);
            ret = sendto(fd, buf, len, 0, (struct sockaddr*)&lladdr, sizeof(lladdr));
            if(ret <= 0)
            {
                ERROR("sendto failed ret=%d %m\n", ret);
            }
            if(i%1000 == 0) usleep(0);
        }
    }
    return SUCCESS;
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
    int ret;
    struct timeval end_tv, start_tv;
    stream_info_t strinfo, *si = &strinfo;

    lladdr.sll_family   = AF_PACKET;
    lladdr.sll_ifindex  = g_ifindex;
    lladdr.sll_protocol = htons(D2D_PROTO);

    ret = bind(fd, (struct sockaddr*)&lladdr, sizeof(lladdr));
    ASSERT(ret != -1, "bind failed %m");

    while(1)
    {
        stream_init(si, fd);
        while(1)
        {
            n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*)&si->remaddr, &slen);
            if(n <= 0)
            {
                break; //timedout
            }
            if(!si->rx_num_pkts)
            {
                gettimeofday(&start_tv, NULL);
                set_timeout(fd, 200);
            }
            stream_handle_pkt(si, buf, n);
            gettimeofday(&end_tv, NULL);
        }
        set_timeout(fd, 0);
        stream_getstats(si, &start_tv, &end_tv);
    }
    return SUCCESS;
}
