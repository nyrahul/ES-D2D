#include <unistd.h>
#include "common.h"
#include "af_pkt.h"

int g_fd = -1;
int g_tx_mode = 0;
int g_mtu = 1500;
uint8_t g_remaddr[MAC_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
FILE *g_fp2send = NULL;

void usage(const char *cmd)
{
    INFO("Usage: %s -t <TX/RX> -i <*interface> -r <remote_mac_addr> -m <mtu> -f <file>\n", cmd);
    exit(2);
}

char *get_remaddr_str(void)
{
    static char str[128];
    int i, len = 0;

    len = snprintf(str + len, sizeof(str) - len, ":%02x", g_remaddr[0]);
    for(i=1;i<MAC_ADDR_LEN;i++)
    {
        len += snprintf(str + len, sizeof(str) - len, ":%02x", g_remaddr[i]);
    }
    return str;
}

int handle_args(int argc, char *argv[])
{
    int opt;
    while((opt = getopt(argc, argv, "f:r:i:m:t:")) != -1)
    {
        switch(opt)
        {
            case 'f':
                g_fp2send = fopen(optarg, "rb");
                ASSERT(g_fp2send, "could not open file <%s>\n", optarg);
                break;
            case 'r':
                get_mac_addr(optarg, g_remaddr, MAC_ADDR_LEN);
                break;
            case 'i':
                g_fd = create_sock(optarg);
                break;
            case 'm':
                g_mtu = atoi(optarg);
                break;
            case 't':
                if(!strcmp(optarg, "TX")) {
                    g_tx_mode = 1;
                } else if(!strcmp(optarg, "RX")) {
                    g_tx_mode = 0;
                } else {
                    ERROR("Invalid -t value\n");
                    usage(argv[0]);
                }
                break;
            default:
                usage(argv[0]);
        }
    }
    if(g_fd == -1)
    {
        printf("Interface info needed\n");
        usage(argv[0]);
    }
    return SUCCESS;
}

int main(int argc, char *argv[])
{
    handle_args(argc, argv);


    if(g_tx_mode)
    {
        INFO("starting TX MTU=%d dest_addr=%s\n", g_mtu, get_remaddr_str());
        sender(g_fd, g_fp2send, g_remaddr, MAC_ADDR_LEN, g_mtu);
    }
    else
    {
        INFO("starting RX ...\n");
        receiver(g_fd);
    }

    return 0;
}

