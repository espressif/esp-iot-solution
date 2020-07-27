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
    const char* data = "message from client";
    uint8_t recv_buf[100];

    tcpip_adapter_ip_info_t ipconfig;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipconfig);

    struct sockaddr_in remoteAddr;
    size_t nAddrLen = sizeof(remoteAddr);
    client.SetTimeout(1);
    while (1) {
        client.SendTo(data, strlen(data)+1, ipconfig.ip.addr, 7777);
        int len = client.RecvFrom(recv_buf, sizeof(recv_buf), (struct sockaddr*) &remoteAddr, &nAddrLen);
        if (len > 0) {
            ESP_LOGI(TAG_CLI, "recv len: %d data: %s", len, recv_buf);
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
            ESP_LOGI(TAG_SRV, "recv len: %d data: %s", ret, recv_data);
            ESP_LOGI(TAG_SRV, "ip: %s: %d", inet_ntoa(remoteAddr.sin_addr), remoteAddr.sin_port);

            const char* resp = "message from server";
            server.SendTo(resp, strlen(resp)+1, 0, (struct sockaddr*) &remoteAddr);
        } else {
            ESP_LOGW(TAG_SRV, "timeout...");
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

