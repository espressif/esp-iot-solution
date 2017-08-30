#ifndef _UTILS_H
#define _UTILS_H

#ifdef __LINUX_UB2__ 
#include <stdint.h>
#endif

#include <unistd.h>
#include <netinet/in.h>
#include "lwip/sockets.h"

int 
joylink_util_cut_ip_port(const char *ipport, char *out_ip, int *out_port);

int 
joylink_util_byte2hexstr(const uint8_t *pBytes, int srcLen, uint8_t *pDstStr, int dstLen);

int 
joylink_util_hexStr2bytes(const char *hexStr, uint8_t *buf, int bufLen);

int 
joylink_util_get_ipstr(struct sockaddr_in* pPeerAddr, char* str);

void 
joylink_util_timer_reset(uint32_t *timestamp);

int 
joylink_util_is_time_out(uint32_t timestamp, uint32_t timeout);

#endif /* utils.h */
