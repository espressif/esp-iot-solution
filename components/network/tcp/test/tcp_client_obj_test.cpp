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
#include "iot_wifi_conn.h"
#include "iot_tcp.h"
#include "tcpip_adapter.h"

#define AP_SSID     CONFIG_AP_SSID
#define AP_PASSWORD CONFIG_AP_PASSWORD
#define SERVER_PORT CONFIG_TCP_SERVER_PORT
#define SERVER_MAX_CONNECTION  CONFIG_TCP_SERVER_MAX_CONNECTION

static const char* TAG_CLI = "TCP_CLI";
static const char* TAG_SRV = "TCP_SRV";
static const char* TAG_WIFI = "WIFI";

static void wifi_connect()
{
    CWiFi *my_wifi = CWiFi::GetInstance(WIFI_MODE_STA);
    ESP_LOGI(TAG_WIFI, "connect WiFi");
    my_wifi->Connect(AP_SSID, AP_PASSWORD, portMAX_DELAY);
    ESP_LOGI(TAG_WIFI, "WiFi connected...");
}

extern "C" void tcp_client_obj_test()
{
    CTcpConn client;
    const char* data = "test1234567";
    uint8_t recv_buf[100];

    tcpip_adapter_ip_info_t ipconfig;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipconfig);


    while(1) {
        ESP_LOGI(TAG_CLI, "TCP connect...");
        ESP_LOGI(TAG_CLI, "connect to: %d.%d.%d.%d(%d)", ((uint8_t*)&ipconfig.ip.addr)[0], ((uint8_t*)&ipconfig.ip.addr)[1],
                                                          ((uint8_t*)&ipconfig.ip.addr)[2], ((uint8_t*)&ipconfig.ip.addr)[3], SERVER_PORT);

        if (client.Connect(ipconfig.ip.addr, SERVER_PORT) < 0) {
            ESP_LOGI(TAG_CLI, "fail to connect...");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG_CLI, "TCP connected");

        if (client.Write((const uint8_t*)data, strlen(data)) < 0) {
            ESP_LOGI(TAG_CLI, "fail to send data...");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG_CLI, "TCP send data done");

        ESP_LOGI(TAG_CLI, "TCP wait data, timeout 10 sec");
        memset(recv_buf, 0, sizeof(recv_buf));
        if (client.Read(recv_buf, sizeof(recv_buf), 10) > 0) {
            ESP_LOGI(TAG_CLI, "recv: %s", recv_buf);
        } else {
            ESP_LOGI(TAG_CLI, "recv timeout...");
        }
        ESP_LOGI(TAG_CLI, "TCP disconnect");
        client.Disconnect();
        ESP_LOGI(TAG_CLI, "heap: %d", esp_get_free_heap_size());
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void tcp_server_handle(void* arg)
{
    CTcpConn *conn = (CTcpConn*) arg;
    const char* data = "test from server";
    while (1) {
        if (conn) {
            ESP_LOGI(TAG_SRV, "send data...");
            if (conn->Write((const uint8_t*) data, strlen(data)) < 0) {
                conn->Disconnect();
                delete conn;
                ESP_LOGI(TAG_SRV, "connection error, close");
                break;
            }
        }
        ESP_LOGI(TAG_SRV, "free heap: %d", esp_get_free_heap_size());
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

extern "C" void tcp_server_obj_test(void* arg)
{
    CTcpServer server;
    server.Listen(SERVER_PORT, SERVER_MAX_CONNECTION);
    while (1) {
        ESP_LOGI(TAG_SRV, "before accept...");
        CTcpConn* conn = server.Accept();
        ESP_LOGI(TAG_SRV, "after accept...");
        xTaskCreate(tcp_server_handle, "tcp_server_handle", 2048, (void* )conn, 6, NULL);
    }
    ESP_LOGI(TAG_SRV, "end of server");
}

TEST_CASE("TCP cpp test", "[tcp_cpp][iot]")
{
    wifi_connect();
    xTaskCreate(tcp_server_obj_test, "tcp_server_obj_test", 2048, NULL, 5, NULL);
    while(1) {
        tcp_client_obj_test();
    }
}


