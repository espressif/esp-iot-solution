/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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
