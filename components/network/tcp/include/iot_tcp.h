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
#ifndef _IOT_TCP_H_
#define _IOT_TCP_H_

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
    /**
     * @brief Create a socket, if not given, and connect to the specific target(ip, port)
     * @param ip ip address to connect
     * @param port port to connect
     * @return connect result
     *     0 if success
     *     <0 if fail
     */
    int Connect(const char* ip, uint16_t port);

    /**
     * @brief Create a socket, if not given, and connect to the specific target(ip, port)
     * @param ip ip address to connect
     * @param port port to connect
     * @return connect result
     *     0 if success
     *     <0 if fail
     */
    int Connect(uint32_t ip, uint16_t port);

    /**
     * @brief set socket timeout
     * @param timeout timeout time for socket
     * @return
     *     0 if success
     *     <0 if fail to set timeout
     */
    int SetTimeout(int timeout);

    /**
     * @brief read data from socket
     * @param data buffer to receive data from socket
     * @param length max length for receive buffer
     * @param timeout for reading data
     * @return
     *     <0 if fail to read
     *     otherwise data length that read from socket
     */
    int Read(uint8_t *data, int length, int timeout);

    /**
     * @brief write data to socket
     * @param data pointer to data buffer to send
     * @param length data length to send to socket
     * return
     *     <0 if fail to write
     *     otherwise data length that written to socket
     */
    int Write(const void *data, int length);

    /**
     * @brief disconnect and close the TCP connection
     * @return
     *     <0 if error found
     *     otherwise success
     */
    int Disconnect();
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
    /**
     * @brief start TCP listening at given port
     * @param port listening port
     * @param max_connection max connection number
     * @return
     *     <0 if error occur
     *     otherwise success
     */
    int Listen(uint16_t port, int max_connection);

    /**
     * @brief Wait client to connect and return a CTcpConn object
     * @return
     *     NULL if error occur
     *     otherwise the pointer to a CTcpConn object
     */
    CTcpConn* Accept();

    /**
     * @brief Stop listening the port and close the listening socket
     */
    void Stop();
};

#endif
#endif
