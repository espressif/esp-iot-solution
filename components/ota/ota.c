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
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "lwip/sockets.h"
#include "ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#define BUFF_SIZE   1024
#define OTA_CHECK(tag, info, a, b)  if((a) != ESP_OK) {                                             \
        ESP_LOGE(tag,"%s %s:%d (%s)", info,__FILE__, __LINE__, __FUNCTION__);      \
        goto b;                                                                   \
        }
#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                             \
        ESP_LOGE(tag,"check error! %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                                   \
        }
#define POINT_ASSERT(tag, param, ret)    IOT_CHECK(tag, (param) != NULL, (ret))
#define ERR_ASSERT(tag, param, ret)  IOT_CHECK(tag, (param) == ESP_OK, (ret))

static xSemaphoreHandle g_ota_mux = NULL;
static const char* TAG = "OTA";
static bool g_ota_timeout;

static esp_err_t connect_http_server(const char *server_ip, uint16_t server_port, int socket_id)
{
    if (socket_id == -1) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Server IP: %s Server Port: %d", server_ip, server_port);
    struct sockaddr_in sock_info;
    memset(&sock_info, 0, sizeof(struct sockaddr_in));
    sock_info.sin_family = AF_INET;
    sock_info.sin_addr.s_addr = inet_addr(server_ip);
    sock_info.sin_port = htons(server_port);
    IOT_CHECK(TAG, -1 != connect(socket_id, (struct sockaddr*)&sock_info, sizeof(sock_info)), ESP_FAIL);
    ESP_LOGI(TAG, "Connected to server");
    return ESP_OK;
}

static bool erase_http_header(uint8_t *buff, int total_len, int *data_len)
{
    char *pstr = strstr((char*)buff, "\r\n\r\n");
    if (pstr) {
        pstr += 4;
        int write_len = total_len - ((uint8_t*)pstr - buff);
        *data_len = write_len;
        int i = 0;
        while (i < write_len) {
            buff[i] = pstr[i];
            i++;
        }
        return true;
    }
    else {
        return false;
    }
}

static void download_timer_cb(TimerHandle_t xTimer)
{
    int socket_id = (int) pvTimerGetTimerID(xTimer);
    g_ota_timeout = true;
    if (socket_id != -1) {
        close(socket_id);
    }
}

static esp_err_t ota_download(int socket_id, esp_ota_handle_t ota_handle)
{
    IOT_CHECK(TAG, socket_id != -1, ESP_ERR_INVALID_ARG);
    bool resp_start = false;
    uint8_t data_buff[BUFF_SIZE];
    int total_len = 0;
    esp_err_t ret = ESP_OK;
    for (;;) {
        memset(data_buff, 0, BUFF_SIZE);
        int recv_len = recv(socket_id, data_buff, BUFF_SIZE, 0);
        if (recv_len < 0) {
            return ESP_FAIL;
        }
        if (recv_len > 0 && !resp_start) {
            int data_len = 0;
            resp_start = erase_http_header(data_buff, recv_len, &data_len);
            ret = esp_ota_write(ota_handle, data_buff, data_len);
            IOT_CHECK(TAG, ret == ESP_OK, ESP_FAIL);
            total_len += data_len;
        } else if (recv_len > 0 && resp_start) {
            ret = esp_ota_write(ota_handle, data_buff, recv_len);
            IOT_CHECK(TAG, ret == ESP_OK, ESP_FAIL);
            total_len += recv_len;
        } else if (recv_len == 0) {
            if (!g_ota_timeout) {
                ESP_LOGI(TAG, "all packets received, total length:%d", total_len);
            } else {
                return ESP_ERR_TIMEOUT;
            }
            break;
        } else {
            ESP_LOGE(TAG, "Unexpected recv result");
        }
        ESP_LOGI(TAG, "Have written image length %d", total_len);
    }
    return ESP_OK; 
}

esp_err_t ota_start(const char *server_ip, uint16_t server_port, const char *file_dir, uint32_t ticks_to_wait)
{
    TimerHandle_t ota_timer = NULL;
    int socket_id = -1;
    esp_err_t ret = ESP_FAIL;
    esp_ota_handle_t upgrade_handle = 0;
    POINT_ASSERT(TAG, server_ip, ESP_ERR_INVALID_ARG);
    POINT_ASSERT(TAG, server_port, ESP_ERR_INVALID_ARG);
    POINT_ASSERT(TAG, file_dir, ESP_ERR_INVALID_ARG);
    if (g_ota_mux == NULL) {
        g_ota_mux = xSemaphoreCreateMutex();
    }
    if (g_ota_mux == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto OTA_FINISH;
    }
    if (pdTRUE != xSemaphoreTake(g_ota_mux, ticks_to_wait)){
        ret = ESP_ERR_TIMEOUT;
        goto OTA_FINISH;
    }
    g_ota_timeout = false;

    ESP_LOGI(TAG, "ota starting");
    const esp_partition_t *upgrade_part = NULL;
    const esp_partition_t *part_confed = esp_ota_get_boot_partition();
    const esp_partition_t *part_running = esp_ota_get_running_partition();
    if (part_confed != part_running) {
        ESP_LOGI(TAG, "partition error");
        ret = ESP_FAIL;
        goto OTA_FINISH;
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
             part_confed->type, part_confed->subtype, part_confed->address);
    socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_id == -1) {
        ESP_LOGI(TAG, "create socket error!");
        goto OTA_FINISH;
    }
    if (ticks_to_wait != portMAX_DELAY) {
        ota_timer = xTimerCreate("ota_timer", ticks_to_wait, pdFALSE, (void*)socket_id, download_timer_cb);
        if (ota_timer == NULL) {
            ret = ESP_ERR_NO_MEM;
            goto OTA_FINISH;
        }
        //todo: to check the result of RTOS timer APIs.
        xTimerStart(ota_timer, ticks_to_wait / portTICK_PERIOD_MS);
    }
    ret = connect_http_server(server_ip, server_port, socket_id);
    OTA_CHECK(TAG, "connect http server error!", ret, OTA_FINISH);
    char http_request[64];
    sprintf(http_request, "GET %s HTTP/1.1\r\nHost: %s:%d \r\n\r\n", file_dir, server_ip, server_port);
    ret = send(socket_id, http_request, strlen(http_request), 0);
    if (ret == -1) {
        ESP_LOGI(TAG, "send request to server  error!");
        ret = ESP_FAIL;
        goto OTA_FINISH;
    }
    upgrade_part = esp_ota_get_next_update_partition(NULL);
    if (upgrade_part == NULL) {
        ret = ESP_FAIL;
        goto OTA_FINISH;
    }
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
                upgrade_part->subtype, upgrade_part->address);
    ret = esp_ota_begin(upgrade_part, OTA_SIZE_UNKNOWN, &upgrade_handle);
    OTA_CHECK(TAG, "ota begin error!", ret, OTA_FINISH);
    ret = ota_download(socket_id, upgrade_handle);
    OTA_CHECK(TAG, "ota data download error!", ret, OTA_FINISH);
    //Stop timer since downloading has finished.
    xTimerStop(ota_timer, ticks_to_wait / portTICK_PERIOD_MS);
    ret = esp_ota_end(upgrade_handle);
    upgrade_handle = 0;
    OTA_CHECK(TAG, "ota end error!", ret, OTA_FINISH);
    ret = esp_ota_set_boot_partition(upgrade_part);
    OTA_CHECK(TAG, "set boot partition error!", ret, OTA_FINISH);
    ESP_LOGI(TAG, "ota succeed");

OTA_FINISH:
    if (upgrade_handle != 0) {
        esp_ota_end(upgrade_handle);
        upgrade_handle = 0;
    }
    if (g_ota_timeout == true) {
        ESP_LOGI(TAG, "ota timeout");
        ret = ESP_ERR_TIMEOUT;
    } else if (socket_id != -1) {
        close(socket_id);
    }
    if (ticks_to_wait != portMAX_DELAY && ota_timer != NULL) {
        xTimerStop(ota_timer, 0);
        xTimerDelete(ota_timer, 0);
        ota_timer = NULL;
    }
    if (g_ota_mux != NULL) {
        xSemaphoreGive(g_ota_mux);
    }
    return ret;
}
