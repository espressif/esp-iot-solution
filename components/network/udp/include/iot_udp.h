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
#ifndef _IOT_UDP_H_
#define _IOT_UDP_H_

#ifdef __cplusplus
#include "tcpip_adapter.h"
class CUdpConn
{
private:
    /**
     * prevent copy constructing
     */
    CUdpConn(const CUdpConn&);
    CUdpConn& operator = (const CUdpConn&);
    int sockfd;
    int tout;

public:

    /**
     * @brief constructor of CUdpConn
     * 
     * @param sock socket to communicate. Set -1, by default, to create a new socket,
     *        otherwise the object will use the given socket directly.
     */
    CUdpConn(int sock = -1);
    ~CUdpConn();

    /**
     * @brief set socket timeout
     * @param timeout timeout time for socket
     * @return
     *     0 if success
     *     <0 if fail to set timeout
     */
    int SetTimeout(int timeout);

    int Bind(uint16_t port);
    /**
     * @brief read data from socket
     * @param data buffer to receive data from socket
     * @param length max length for receive buffer
     * @param timeout for reading data
     * @return
     *     <0 if fail to read
     *     otherwise data length that read from socket
     */
    int RecvFrom(uint8_t* data, int length, struct sockaddr *from, size_t *fromlen, int timeout = -1);

    /**
     * @brief write data to socket
     * @param data pointer to data buffer to send
     * @param length data length to send to socket
     * @param ip ip address to send data
     * @param port target port to send data
     * return
     *     <0 if fail to write
     *     otherwise data length that written to socket
     */
    int SendTo(const void *data, size_t length, const char* ip, uint16_t port);

    /**
     * @brief write data to socket
     * @param data pointer to data buffer to send
     * @param length data length to send to socket
     * @param ip ip address to send data
     * @param port target port to send data
     * return
     *     <0 if fail to write
     *     otherwise data length that written to socket
     */
    int SendTo(const void *data, size_t length, const in_addr_t ip, uint16_t port);

    /**
     * @brief write data to socket
     * @param data pointer to data buffer to send
     * @param length data length to send to socket
     * @param flag protocol flag
     * @param to target address to send data
     * return
     *     <0 if fail to write
     *     otherwise data length that written to socket
     */
    int SendTo(const void *data, size_t size, int flags, const struct sockaddr *to);

    /**
     * @brief close the UDP socket
     */
    void Close();
};

#endif
#endif
