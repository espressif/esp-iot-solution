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
#include "button.h"
#include "esp_log.h"
#include "wifi.h"
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
    CWiFi my_wifi(WIFI_MODE_STA);
    ESP_LOGI(TAG_WIFI, "connect WiFi");
    my_wifi.connect_start(AP_SSID, AP_PASSWORD, portMAX_DELAY);
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

        if (client.connect(ipconfig.ip.addr, SERVER_PORT) < 0) {
            ESP_LOGI(TAG_CLI, "fail to connect...");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG_CLI, "TCP connected");

        if (client.write((const uint8_t*)data, strlen(data)) < 0) {
            ESP_LOGI(TAG_CLI, "fail to send data...");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG_CLI, "TCP send data done");

        ESP_LOGI(TAG_CLI, "TCP wait data, timeout 10 sec");
        memset(recv_buf, 0, sizeof(recv_buf));
        if (client.read(recv_buf, sizeof(recv_buf), 10) > 0) {
            ESP_LOGI(TAG_CLI, "recv: %s", recv_buf);
        } else {
            ESP_LOGI(TAG_CLI, "recv timeout...");
        }
        ESP_LOGI(TAG_CLI, "TCP disconnect");
        client.disconnect();
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
            if (conn->write((const uint8_t*) data, strlen(data)) < 0) {
                conn->disconnect();
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
    server.listen(SERVER_PORT, SERVER_MAX_CONNECTION);
    while (1) {
        ESP_LOGI(TAG_SRV, "before accept...");
        CTcpConn* conn = server.accept();
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


