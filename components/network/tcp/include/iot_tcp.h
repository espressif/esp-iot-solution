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

#ifndef _IOT_TCP_CLIENT_H_
#define _IOT_TCP_CLIENT_H_

#ifdef __cplusplus
#include "tcpip_adapter.h"
class CTcpConn
{
friend class CTcpServer;
private:
    /**
     * prevent copy constructing
     */
    CTcpConn(const CTcpConn&);
    CTcpConn& operator = (const CTcpConn&);
    int sockfd;
    int tout;

public:

    /**
     * @brief constructor of CTcpConn
     * 
     * @param sock socket to communicate. Set -1, by default, to create a new socket,
     *        otherwise the object will use the given socket directly.
     */
    CTcpConn(int sock = -1);
    ~CTcpConn();
#undef connect
    /**
     * @brief Create a socket, if not given, and connect to the specific target(ip, port)
     * @param ip ip address to connect
     * @param port port to connect
     * @return connect result
     *     0 if success
     *     <0 if fail
     */
    int connect(const char* ip, uint16_t port);

    /**
     * @brief Create a socket, if not given, and connect to the specific target(ip, port)
     * @param ip ip address to connect
     * @param port port to connect
     * @return connect result
     *     0 if success
     *     <0 if fail
     */
    int connect(uint32_t ip, uint16_t port);

    /**
     * @brief set socket timeout
     * @param timeout timeout time for socket
     * @return
     *     0 if success
     *     <0 if fail to set timeout
     */
    int set_timeout(int timeout);

    /**
     * @brief read data from socket
     * @param data buffer to receive data from socket
     * @param length max length for receive buffer
     * @param timeout for reading data
     * @return
     *     <0 if fail to read
     *     otherwise data length that read from socket
     */
    int read(uint8_t *data, int length, int timeout);

    /**
     * @brief write data to socket
     * @param data pointer to data buffer to send
     * @param length data length to send to socket
     * return
     *     <0 if fail to write
     *     otherwise data length that written to socket
     */
    int write(const uint8_t *data, int length);

    /**
     * @brief disconnect and close the TCP connection
     * @return
     *     <0 if error found
     *     otherwise success
     */
    int disconnect();
};

class CTcpServer: public CTcpConn
{
private:
    /**
     * prevent copy constructing
     */
    CTcpServer(const CTcpServer&);
    CTcpServer& operator = (const CTcpServer&);
public:
    /**
     * @brief constructor of CTcpServer
     */
    CTcpServer();
    ~CTcpServer();
#undef listen
    /**
     * @brief start TCP listening at given port
     * @param port listening port
     * @param max_connection max connection number
     * @return
     *     <0 if error occur
     *     otherwise success
     */
    int listen(uint16_t port, int max_connection);
#undef accept

    /**
     * @brief Wait client to connect and return a CTcpConn object
     * @return
     *     NULL if error occur
     *     otherwise the pointer to a CTcpConn object
     */
    CTcpConn* accept();

    /**
     * @brief Stop listening the port and close the listening socket
     */
    void stop();
};

#endif
#endif
