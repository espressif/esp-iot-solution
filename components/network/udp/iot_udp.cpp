// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "iot_udp.h"
static const char* TAG = "udp";

CUdpConn::CUdpConn(int sock)
{
    tout = 0;
    if (sock < 0) {
        sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    } else {
        sockfd = sock;
    }
}

CUdpConn::~CUdpConn()
{
    close(sockfd);
    sockfd = -1;
}

int CUdpConn::SetTimeout(int timeout)
{
    int ret = 0;
    if (sockfd < 0) {
        return -1;
    }
    tout = timeout;
    struct timeval receiving_timeout;
    receiving_timeout.tv_sec = timeout;
    receiving_timeout.tv_usec = 0;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout));
    if (ret < 0) {
        ESP_LOGE(TAG, "... failed to set socket receiving timeout");
        close(sockfd);
        sockfd = -1;
    }
    return ret;
}

int CUdpConn::RecvFrom(uint8_t* data, int length, struct sockaddr *from, size_t *fromlen, int timeout)
{
    int ret = -1;
    if (sockfd < 0) {
        return -1;
    }

    if (timeout > 0) {
        ret = SetTimeout(timeout);
        if (ret < 0) {
            return ret;
        }
    }
    ret = recvfrom(sockfd, data, length, 0, from, fromlen);
    return ret;
}

int CUdpConn::Bind(uint16_t port)
{
    if (sockfd < 0) {
        return -1;
    }
    sockaddr_in serAddr;
    serAddr.sin_family = AF_INET;
    serAddr.sin_port = htons(port);
    serAddr.sin_addr.s_addr = INADDR_ANY;

    int ret = bind(sockfd, (sockaddr * )&serAddr, sizeof(serAddr));
    if (ret != 0) {
        ESP_LOGE(TAG, "bind error ! port: %d", port);
        Close();
    }
    return ret;
}

int CUdpConn::SendTo(const void *data, size_t size, int flags, const struct sockaddr *to)
{
    socklen_t tolen = sizeof(*to);
    return sendto(sockfd, data, size, flags, to, tolen);
}

int CUdpConn::SendTo(const void *data, size_t length, const char* ip, uint16_t port)
{
    if (sockfd < 0) {
        ESP_LOGE(TAG, "... socket error");
        return -1;
    }
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = inet_addr(ip);
    int len = sizeof(sin);

    int ret = sendto(sockfd, data, length, 0, (sockaddr * )&sin, len);
    if (ret < 0) {
        ESP_LOGE(TAG, "... socket send failed");
        close(sockfd);
        sockfd = -1;
    }
    return ret;
}

int CUdpConn::SendTo(const void *data, size_t length, const in_addr_t ip, uint16_t port)
{
    if (sockfd < 0) {
        ESP_LOGE(TAG, "... socket error");
        return -1;
    }
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = ip;
    int len = sizeof(sin);

    int ret = sendto(sockfd, data, length, 0, (sockaddr * )&sin, len);
    if (ret < 0) {
        ESP_LOGE(TAG, "... socket send failed");
        close(sockfd);
        sockfd = -1;
    }
    return ret;
}

void CUdpConn::Close()
{
    close(sockfd);
    sockfd = -1;
}

