#ifndef _AF_PKT_H_
#define _AF_PKT_H_

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <net/ethernet.h> /* the L2 protocols */
#include <net/if.h>
#include <netinet/in.h>

int get_ifindex(int fd, const char *if_name);
int get_mac_addr(const char *addr, uint8_t *mac, size_t len);
int create_sock(const char *if_name);
int sender(int fd, const uint8_t *mac, size_t maclen, const int mtu);
int receiver(int fd);

#endif // _AF_PKT_H_
