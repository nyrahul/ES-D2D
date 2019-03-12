#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define ASSERT(COND, ...)   \
if(!(COND)) \
{\
    printf("FATAL: line=%d, function:%s, file:%s\n", \
        __LINE__, __FUNCTION__, __FILE__); \
    printf(__VA_ARGS__);\
    printf("\n");\
    exit(1);\
}

#define INFO     printf
#define ERROR    printf
#define SUCCESS  0
#define FAILURE  -1

#define MAC_ADDR_LEN    6
#define MAX_MAC_MTU     32768

static inline int diffms(struct timeval *stv, struct timeval *etv)
{
    return (((etv->tv_sec - stv->tv_sec)*1000) +
            ((etv->tv_usec - stv->tv_usec)/1000));
}

#endif  // _COMMON_H_
