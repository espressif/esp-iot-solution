/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_system.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "tcpip_adapter.h"

#include "esp_alink.h"
#include "alink_platform.h"
#include "esp_alink_log.h"

#define SOMAXCONN            5
#define ALINK_SOCKET_TIMEOUT 20
static const char *TAG = "alink_network";

static alink_err_t network_create_socket(pplatform_netaddr_t netaddr, int type, struct sockaddr_in *paddr, int *psock)
{
    ALINK_PARAM_CHECK(netaddr);
    ALINK_PARAM_CHECK(paddr);
    ALINK_PARAM_CHECK(psock);

    struct hostent *hp;
    struct in_addr in;
    uint32_t ip;
    alink_err_t ret;

    if (NULL == netaddr->host) {
        ip = htonl(INADDR_ANY);
    } else {
        ALINK_LOGI("alink server host: %s", netaddr->host);

        if (inet_aton(netaddr->host, &in)) {
            ip = *(uint32_t *)&in;
        } else {
            hp = gethostbyname(netaddr->host);
            ALINK_ERROR_CHECK(hp == NULL, ALINK_ERR, "gethostbyname ret:%p", hp);
            ip = *(uint32_t *)(hp->h_addr);
        }
    }

    *psock = socket(AF_INET, type, 0);

    if (*psock < 0) {
        return ALINK_ERR;
    }

    memset(paddr, 0, sizeof(struct sockaddr_in));

    int opt_val = 1;
    ret = setsockopt(*psock, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
    ALINK_ERROR_CHECK(ret != 0, ALINK_ERR, "setsockopt SO_REUSEADDR errno: %d", errno);

    if (type == SOCK_DGRAM) {
        ret = setsockopt(*psock, SOL_SOCKET, SO_BROADCAST, &opt_val, sizeof(opt_val));

        if (ret != 0) {
            close(*psock);
        }

        ALINK_ERROR_CHECK(ret != 0, ALINK_ERR, "setsockopt SO_BROADCAST errno: %d", errno);
    }

    struct timeval timeout = {ALINK_SOCKET_TIMEOUT, 0};

    ret = setsockopt((int) * psock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
    ALINK_ERROR_CHECK(ret != 0, ALINK_ERR, "setsockopt SO_RCVTIMEO errno: %d", errno);
    ALINK_LOGD("setsockopt: recv timeout %ds", ALINK_SOCKET_TIMEOUT);

    ret = setsockopt((int) * psock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(struct timeval));
    ALINK_ERROR_CHECK(ret != 0, ALINK_ERR, "setsockopt SO_SNDTIMEO errno: %d", errno);
    ALINK_LOGD("setsockopt: send timeout %ds", ALINK_SOCKET_TIMEOUT);

    paddr->sin_addr.s_addr = ip;
    paddr->sin_family = AF_INET;
    paddr->sin_port = htons(netaddr->port);

    ALINK_LOGV("create sock: %d", *psock);

    return ALINK_OK;
}

void *platform_udp_server_create(_IN_ uint16_t port)
{
    struct sockaddr_in addr;
    int server_socket;
    alink_err_t ret;
    platform_netaddr_t netaddr = {NULL, port};

    ret = network_create_socket(&netaddr, SOCK_DGRAM, &addr, &server_socket);
    ALINK_ERROR_CHECK(ret != ALINK_OK, PLATFORM_INVALID_FD, "network_create_socket");

    if (-1 == bind(server_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) {
        perror("socket bind");
        platform_udp_close((void *)server_socket);
        return PLATFORM_INVALID_FD;
    }

    return (void *)server_socket;
}

void *platform_udp_client_create(void)
{
    struct sockaddr_in addr;
    int sock;
    platform_netaddr_t netaddr = {NULL, 0};
    alink_err_t ret;

    ret = network_create_socket(&netaddr, SOCK_DGRAM, &addr, &sock);
    ALINK_ERROR_CHECK(ret != ALINK_OK, PLATFORM_INVALID_FD, "network_create_socket");

    return (void *)sock;
}

void *platform_udp_multicast_server_create(pplatform_netaddr_t netaddr)
{
    ALINK_ASSERT(netaddr);

    int option = 1;
    struct sockaddr_in addr;
    int sock;
    struct ip_mreq mreq;
    alink_err_t ret = 0;

    platform_netaddr_t netaddr_client = {NULL, netaddr->port};

    memset(&addr, 0, sizeof(addr));
    memset(&mreq, 0, sizeof(mreq));

    ret = network_create_socket(&netaddr_client, SOCK_DGRAM, &addr, &sock);
    ALINK_ERROR_CHECK(ret != ALINK_OK, PLATFORM_INVALID_FD, "network_create_socket");

    /* allow multiple sockets to use the same PORT number */
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option)) < 0) {
        ALINK_LOGE("setsockopt");
        platform_udp_close((void *)sock);
        return PLATFORM_INVALID_FD;
    }

    if (-1 == bind(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) {
        ALINK_LOGE("socket bind");
        platform_udp_close((void *)sock);
        return PLATFORM_INVALID_FD;
    }

    mreq.imr_multiaddr.s_addr = inet_addr(netaddr->host);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof(mreq)) < 0) {
        ALINK_LOGE("setsockopt IP_ADD_MEMBERSHIP");
        platform_udp_close((void *)sock);
        return PLATFORM_INVALID_FD;
    }

    return (void *)sock;
}

void platform_udp_close(void *handle)
{
    if ((int)handle >= 0) {
        close((int)handle);
    }
}

int platform_udp_sendto(
    _IN_ void *handle,
    _IN_ const char *buffer,
    _IN_ uint32_t length,
    _IN_ pplatform_netaddr_t netaddr)
{
    ALINK_PARAM_CHECK((int)handle >= 0);
    ALINK_PARAM_CHECK(buffer);
    ALINK_PARAM_CHECK(netaddr);

    int ret;
    uint32_t ip;
    struct in_addr in;
    struct hostent *hp;
    struct sockaddr_in addr;

    if (inet_aton(netaddr->host, &in)) {
        ip = *(uint32_t *)&in;
    } else {
        hp = gethostbyname(netaddr->host);
        ALINK_ERROR_CHECK(!hp, ALINK_ERR, "gethostbyname Can't resolute the host address hp:%p", hp);
        ip = *(uint32_t *)(hp->h_addr);
    }

    addr.sin_addr.s_addr = ip;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(netaddr->port);

    ret = sendto((int)handle,
                 buffer,
                 length,
                 0,
                 (struct sockaddr *)&addr,
                 sizeof(struct sockaddr_in));
    ALINK_ERROR_CHECK(ret <= 0, ALINK_ERR, "sendto ret:%d, errno:%d", ret, errno);

    return ret;
}

int platform_udp_recvfrom(
    _IN_ void *handle,
    _OUT_ char *buffer,
    _IN_ uint32_t length,
    _OUT_OPT_ pplatform_netaddr_t netaddr)
{
    ALINK_PARAM_CHECK((int)handle >= 0);
    ALINK_PARAM_CHECK(buffer);

    int ret;
    struct sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    ret = recvfrom((int)handle, buffer, length, 0, (struct sockaddr *)&addr, &addr_len);
    ALINK_ERROR_CHECK(ret <= 0, ALINK_ERR, "recvfrom ret:%d, errno:%d", ret, errno);

    if (NULL != netaddr) {
        netaddr->port = ntohs(addr.sin_port);

        if (NULL != netaddr->host) {
            strcpy(netaddr->host, inet_ntoa(addr.sin_addr));
        }
    }

    return ret;
}


void *platform_tcp_server_create(_IN_ uint16_t port)
{
    struct sockaddr_in addr;
    int server_socket;
    alink_err_t ret = 0;
    platform_netaddr_t netaddr = {NULL, port};

    ret = network_create_socket(&netaddr, SOCK_STREAM, &addr, &server_socket);
    ALINK_ERROR_CHECK(ret != ALINK_OK, PLATFORM_INVALID_FD, "network_create_socket");

    if (-1 == bind(server_socket, (struct sockaddr *)&addr, sizeof(addr))) {
        ALINK_LOGE("bind");
        platform_tcp_close((void *)server_socket);
        return PLATFORM_INVALID_FD;
    }

    if (0 != listen(server_socket, SOMAXCONN)) {
        ALINK_LOGE("listen");
        platform_tcp_close((void *)server_socket);
        return PLATFORM_INVALID_FD;
    }

    return (void *)server_socket;
}

void *platform_tcp_server_accept(_IN_ void *server)
{
    ALINK_ASSERT(server);

    struct sockaddr_in addr;
    unsigned int addr_length = sizeof(addr);
    int new_client;

    new_client = accept((int)server, (struct sockaddr *)&addr, &addr_length);
    ALINK_ERROR_CHECK(new_client <= 0, PLATFORM_INVALID_FD, "accept errno:%d", errno);

    return (void *)new_client;
}

void *platform_tcp_client_connect(_IN_ pplatform_netaddr_t netaddr)
{
    ALINK_ASSERT(netaddr);

    struct sockaddr_in addr;
    int sock;
    alink_err_t ret = 0;

    ret = network_create_socket(netaddr, SOCK_STREAM, &addr, &sock);
    ALINK_ERROR_CHECK(ret != ALINK_OK, PLATFORM_INVALID_FD, "network_create_socket");

    if (-1 == connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) {
        ALINK_LOGE("connect errno: %d", errno);
        perror("connect");
        platform_tcp_close((void *)sock);
        return PLATFORM_INVALID_FD;
    }

    return (void *)sock;
}


int platform_tcp_send(_IN_ void *handle, _IN_ const char *buffer, _IN_ uint32_t length)
{
    ALINK_PARAM_CHECK((int)handle >= 0);
    ALINK_PARAM_CHECK(buffer);
    int bytes_sent;

    bytes_sent = send((int)handle, buffer, length, 0);
    ALINK_ERROR_CHECK(bytes_sent <= 0, ALINK_ERR, "send ret:%d", bytes_sent);
    return bytes_sent;
}

int platform_tcp_recv(_IN_ void *handle, _OUT_ char *buffer, _IN_ uint32_t length)
{
    ALINK_PARAM_CHECK((int)handle >= 0);
    ALINK_PARAM_CHECK(buffer);

    int bytes_received = recv((int)handle, buffer, length, 0);
    ALINK_ERROR_CHECK(bytes_received <= 0, ALINK_ERR, "recv ret:%d", bytes_received);

    return bytes_received;
}

void platform_tcp_close(_IN_ void *handle)
{
    if ((int)handle >= 0) {
        close((int)handle);
    }
}

int platform_select(void *read_fds[PLATFORM_SOCKET_MAXNUMS],
                    void *write_fds[PLATFORM_SOCKET_MAXNUMS],
                    int timeout_ms)
{
    ALINK_PARAM_CHECK(read_fds || write_fds);

    int i, ret = -1;
    struct timeval timeout_value;
    struct timeval *ptimeval = &timeout_value;
    fd_set r_set, w_set;
    int max_fd = -1;

    if (PLATFORM_WAIT_INFINITE == timeout_ms) {
        ptimeval = NULL;
    } else {
        ptimeval->tv_sec = timeout_ms / 1000;
        ptimeval->tv_usec = (timeout_ms % 1000) * 1000;
    }

    FD_ZERO(&r_set);
    FD_ZERO(&w_set);

    if (read_fds) {
        for (i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i) {
            if (PLATFORM_INVALID_FD != read_fds[i]) {
                FD_SET((int)read_fds[i], &r_set);
            }

            if ((int)read_fds[i] > max_fd) {
                max_fd = (int)read_fds[i];
            }
        }
    }

    if (write_fds) {
        for (i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i) {
            if (PLATFORM_INVALID_FD != write_fds[i]) {
                FD_SET((int)write_fds[i], &w_set);
            }

            if ((int)write_fds[i] > max_fd) {
                max_fd = (int)write_fds[i];
            }
        }
    }

    ret = select(max_fd + 1, &r_set, &w_set, NULL, ptimeval);

    if (ret > 0) {
        if (read_fds) {
            for (i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i) {
                if (PLATFORM_INVALID_FD != read_fds[i]
                        && !FD_ISSET((int)read_fds[i], &r_set)) {
                    read_fds[i] = PLATFORM_INVALID_FD;
                }
            }
        }

        if (write_fds) {
            for (i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i) {
                if (PLATFORM_INVALID_FD != write_fds[i]
                        && !FD_ISSET((int)write_fds[i], &w_set)) {
                    write_fds[i] = PLATFORM_INVALID_FD;
                }
            }
        }
    } else {/* clear all fd */
        if (read_fds) {
            for (i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i) {
                read_fds[i] = PLATFORM_INVALID_FD;
            }
        }

        if (write_fds) {
            for (i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i) {
                write_fds[i] = PLATFORM_INVALID_FD;
            }
        }
    }

    ALINK_ERROR_CHECK(ret < 0, ret, "select max_fd: %d, ret:%d, errno: %d", max_fd, ret, errno);
    return ret;
}
