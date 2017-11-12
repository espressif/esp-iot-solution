/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
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

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "unity.h"
#include "esp_log.h"
#include "tcpip_adapter.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#include "iot_wifi_conn.h"
#include "iot_udp.h"

#define AP_SSID     CONFIG_AP_SSID
#define AP_PASSWORD CONFIG_AP_PASSWORD
#define SERVER_PORT CONFIG_TCP_SERVER_PORT
#define SERVER_MAX_CONNECTION  CONFIG_TCP_SERVER_MAX_CONNECTION

static const char* TAG_CLI = "UDP CLI";
static const char* TAG_SRV = "UDP SRV";
static const char* TAG_WIFI = "WIFI";

static void wifi_connect()
{
    CWiFi *my_wifi = CWiFi::GetInstance(WIFI_MODE_STA);
    ESP_LOGI(TAG_WIFI, "connect WiFi");
    my_wifi->Connect(AP_SSID, AP_PASSWORD, portMAX_DELAY);
    ESP_LOGI(TAG_WIFI, "WiFi connected...");
}

extern "C" void udp_client_obj_test()
{
    CUdpConn client;
    const char* data = "test1234567";
    uint8_t recv_buf[100];

    tcpip_adapter_ip_info_t ipconfig;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipconfig);

    struct sockaddr_in remoteAddr;
    size_t nAddrLen = sizeof(remoteAddr);
    client.SetTimeout(1);
    while (1) {
        client.SendTo(data, strlen(data), ipconfig.ip.addr, 7777);
        int len = client.RecvFrom(recv_buf, sizeof(recv_buf), (struct sockaddr*) &remoteAddr, &nAddrLen);
        if (len > 0) {
            ESP_LOGI(TAG_CLI, "recv len: %d", len);
            ESP_LOGI(TAG_CLI, "data: %s", recv_buf);
            ESP_LOGI(TAG_CLI, "ip: %s", inet_ntoa(remoteAddr.sin_addr));
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

extern "C" void udp_server_obj_test(void* arg)
{
    CUdpConn server;

    server.Bind(7777);
    server.SetTimeout(1);

    uint8_t recv_data[100];
    struct sockaddr_in remoteAddr;
    size_t nAddrLen = sizeof(remoteAddr);
    while (1) {
        int ret = server.RecvFrom(recv_data, sizeof(recv_data), (struct sockaddr*) &remoteAddr, &nAddrLen);
        if (ret > 0) {
            ESP_LOGI(TAG_SRV, "recv: %s", recv_data);
            ESP_LOGI(TAG_CLI, "ip: %s: %d", inet_ntoa(remoteAddr.sin_addr), remoteAddr.sin_port);

            const char* resp = "server reponse...\n";
            server.SendTo(resp, strlen(resp), 0, (struct sockaddr*) &remoteAddr);
        } else {
            ESP_LOGI(TAG_SRV, "timeout...");
        }
        ESP_LOGI(TAG_SRV, "heap: %d", esp_get_free_heap_size());
    }
    vTaskDelete(NULL);
}

TEST_CASE("UDP cpp test", "[udp_cpp][iot]")
{
    wifi_connect();
    xTaskCreate(udp_server_obj_test, "udp_server_obj_test", 2048, NULL, 5, NULL);
    while (1) {
        udp_client_obj_test();
    }
}

