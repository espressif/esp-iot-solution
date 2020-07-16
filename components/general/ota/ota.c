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
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"

#include "esp_system.h"
#include "lwip/sockets.h"
#include "iot_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

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

static portMUX_TYPE ota_spinlock = portMUX_INITIALIZER_UNLOCKED;
#define IOT_OTA_ENTER_CRITICAL()        portENTER_CRITICAL(&ota_spinlock)
#define IOT_OTA_EXIT_CRITICAL()         portEXIT_CRITICAL(&ota_spinlock)

static xSemaphoreHandle g_ota_mux = NULL;
static const char* TAG = "OTA";
static bool g_ota_timeout;
#define HTTP_STARTER_STR "http://"
#define HTTPS_STARTER_STR "https://"
static int ota_cur_len = 0;
static int ota_total_length = 0;

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
    if (socket_id >= 0) {
        close(socket_id);
    }
}

#define CONTENT_LEN_STR  "Content-Length: "
static int ota_get_content_length(uint8_t* data_buf)
{
    char* ptr = strstr((const char*)data_buf, CONTENT_LEN_STR);
    int length = -1;
    if (ptr) {
        ptr = ptr + strlen(CONTENT_LEN_STR);
        length = atoi(ptr);
    }
    return length;
}

static esp_err_t ota_download(int socket_id, esp_ota_handle_t ota_handle)
{
    IOT_CHECK(TAG, socket_id != -1, ESP_ERR_INVALID_ARG);
    esp_err_t ret = ESP_OK;
    bool resp_start = false;
    uint8_t* data_buff = (uint8_t*) calloc(1, BUFF_SIZE);
    if (data_buff == NULL) {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "OTA no enought memory for data buffer");
        goto EXIT;
    }
    int total_len = 0;
    for (;;) {
        memset(data_buff, 0, BUFF_SIZE);
        int recv_len = recv(socket_id, data_buff, BUFF_SIZE, 0);
        if (recv_len < 0) {
            ESP_LOGE(TAG, "recv_len < 0");
            ret = ESP_FAIL;
        }
        if (recv_len > 0 && !resp_start) {
            int data_len = 0;
            int content_len = ota_get_content_length(data_buff);
            if (content_len > 0) {
                ESP_LOGD(TAG, "Content length: %d", content_len);
                ota_total_length = content_len;
            } else {
                continue;
            }
            resp_start = erase_http_header(data_buff, recv_len, &data_len);
            ret = esp_ota_write(ota_handle, data_buff, data_len);
            if (ret != ESP_OK) {
                ret = ESP_FAIL;
                ESP_LOGE(TAG, "OTA write data error.");
                goto EXIT;
            }
            total_len += data_len;
        } else if (recv_len > 0 && resp_start) {
            ret = esp_ota_write(ota_handle, data_buff, recv_len);
            if (ret != ESP_OK) {
                ret = ESP_FAIL;
                ESP_LOGE(TAG, "OTA write data error.");
                goto EXIT;
            }
            total_len += recv_len;
            ota_cur_len = total_len;
        } else if (recv_len == 0 && ota_cur_len == ota_total_length && ota_total_length > 0) {
            if (!g_ota_timeout) {
                ESP_LOGD(TAG, "all packets received, total length: %d / %d", total_len, ota_total_length);
            } else {
                ESP_LOGE(TAG, "OTA timeout!");
                ret = ESP_ERR_TIMEOUT;
                goto EXIT;
            }
            break;
        } else {
            ESP_LOGE(TAG, "Unexpected recv result");
        }
        ESP_LOGD(TAG, "Have written image length %d / %d", total_len, ota_total_length);
    }
    EXIT:
    if (data_buff) {
        free(data_buff);
        data_buff = NULL;
    }
    return ret;
}

int iot_ota_get_ratio()
{
    int ret = -1;
    IOT_OTA_ENTER_CRITICAL();
    if (ota_total_length > 0) {
        ret = ota_cur_len * 100 / ota_total_length;
    }
    IOT_OTA_EXIT_CRITICAL();
    return ret;
}

esp_err_t iot_ota_start(const char *server_ip, uint16_t server_port, const char *file_dir, uint32_t ticks_to_wait)
{
    ota_cur_len = 0;
    ota_total_length = 0;
    TimerHandle_t ota_timer = NULL;
    int socket_id = -1;
    esp_err_t ret = ESP_FAIL;
    esp_ota_handle_t upgrade_handle = 0;
    POINT_ASSERT(TAG, server_ip, ESP_ERR_INVALID_ARG);
    POINT_ASSERT(TAG, file_dir, ESP_ERR_INVALID_ARG);
    if (g_ota_mux == NULL) {
        g_ota_mux = xSemaphoreCreateMutex();
    }
    if (g_ota_mux == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto OTA_FINISH;
    }
    if (pdTRUE != xSemaphoreTake(g_ota_mux, 0)){
        ret = ESP_ERR_INVALID_STATE;
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


    /* Erase flash */
    upgrade_part = esp_ota_get_next_update_partition(NULL);
    if (upgrade_part == NULL) {
        ret = ESP_FAIL;
        goto OTA_FINISH;
    }
    ESP_LOGI(TAG, "Erasing to partition subtype %d at offset 0x%x",
                upgrade_part->subtype, upgrade_part->address);
    ret = esp_ota_begin(upgrade_part, OTA_SIZE_UNKNOWN, &upgrade_handle);
    OTA_CHECK(TAG, "ota begin error!", ret, OTA_FINISH);



    ESP_LOGI(TAG, "OTA DNS running...");
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    ESP_LOGI(TAG, "Running DNS lookup for %s...", server_ip);
    char *port_str = NULL;
    asprintf(&port_str, "%d", server_port);

    int err = getaddrinfo(server_ip, port_str, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGI(TAG, "DNS lookup failed err=%d res=%p", err, res);
        if (res) {
            freeaddrinfo(res);
        }
        ret = ESP_FAIL;
        goto OTA_FINISH;
    }
    /* Start a timer for checking timeout */
    if (ticks_to_wait != portMAX_DELAY) {
        ota_timer = xTimerCreate("ota_timer", ticks_to_wait, pdFALSE, (void*)socket_id, download_timer_cb);
        if (ota_timer == NULL) {
            ret = ESP_ERR_NO_MEM;
            goto OTA_FINISH;
        }
        //todo: to check the result of RTOS timer APIs.
        if (pdTRUE != xTimerStart(ota_timer, ticks_to_wait / portTICK_PERIOD_MS)) {
            ret = ESP_ERR_TIMEOUT;
            goto OTA_FINISH;
        }
    }

    /* Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
    struct in_addr *addr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
    ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));
    char *server_addr = NULL;
    asprintf(&server_addr, "%s", inet_ntoa(*addr));
    server_ip = server_addr;

    socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_id == -1) {
        ESP_LOGI(TAG, "create socket error!");
        ret = ESP_ERR_NO_MEM;
        goto OTA_FINISH;
    }

    ret = connect_http_server(server_ip, server_port, socket_id);
    OTA_CHECK(TAG, "connect http server error!", ret, OTA_FINISH);
    /*send GET request to http server*/
    const char *GET_FORMAT =
        "GET %s HTTP/1.0\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: esp-idf/1.0 esp32\r\n\r\n";
    char *http_request = NULL;
    int get_len = asprintf(&http_request, GET_FORMAT, file_dir, server_ip, server_port);
    if (get_len < 0) {
        ESP_LOGE(TAG, "Failed to allocate memory for GET request buffer");
        ret = ESP_ERR_NO_MEM;
        goto OTA_FINISH;
    }
    ret = send(socket_id, http_request, get_len, 0);
    free(http_request);

    if (ret == -1) {
        ESP_LOGI(TAG, "send request to server  error!");
        ret = ESP_FAIL;
        goto OTA_FINISH;
    }

    ret = ota_download(socket_id, upgrade_handle);
    OTA_CHECK(TAG, "ota data download error!", ret, OTA_FINISH);
    //Stop timer since downloading has finished.
    if (ota_timer) {
        xTimerStop(ota_timer, ticks_to_wait / portTICK_PERIOD_MS);
    }
    ret = esp_ota_end(upgrade_handle);
    upgrade_handle = 0;
    OTA_CHECK(TAG, "ota end error!", ret, OTA_FINISH);
    ret = esp_ota_set_boot_partition(upgrade_part);
    OTA_CHECK(TAG, "set boot partition error!", ret, OTA_FINISH);
    ESP_LOGI(TAG, "ota succeed");

OTA_FINISH:
    if (server_addr) {
        free(server_addr);
        server_addr = NULL;
    }
    if (port_str) {
        free(port_str);
        port_str = NULL;
    }
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

esp_err_t iot_ota_start_url(const char *url, uint32_t ticks_to_wait)
{
    esp_err_t ret = ESP_FAIL;
    char *server_addr = NULL;
    uint16_t port = 0;
    int url_len = strlen(url);
    char* p_start = NULL;
    char* p_end = NULL;

    p_start = strstr(url, HTTP_STARTER_STR);
    if (p_start == NULL) {
        p_start = strstr(url, HTTPS_STARTER_STR);
        p_start = (p_start != NULL) ? (p_start + strlen(HTTPS_STARTER_STR)) : (char*)url;
    } else {
        p_start += strlen(HTTP_STARTER_STR);
    }

    p_end = (char*) strchr(p_start, ':');
    if (p_end == NULL) {
        port = 80;
        p_end = (char*) strchr(p_start, '/');
        if (p_end == NULL) {
            ESP_LOGE(TAG, "URL parse address error0");
            ret = ESP_ERR_INVALID_ARG;
            goto error;
        }
    }

    if (p_end > p_start) {
        server_addr = (char*) calloc(p_end - p_start + 1, 1);
        memcpy(server_addr, p_start, p_end - p_start);
        ESP_LOGI(TAG, "Address: %s\n", server_addr);
    } else {
        ESP_LOGE(TAG, "URL parse address error1");
        ret = ESP_ERR_INVALID_ARG;
        goto error;
    }

    if (port == 0) {
        p_start = p_end + 1;
        p_end = strchr(p_start, '/');
        if (p_end > p_start) {
            port = atoi(p_start);
        } else {
            ESP_LOGE(TAG, "URL parse port error \n");
            ret = ESP_ERR_INVALID_ARG;
            goto error;
        }
    }
    ESP_LOGI(TAG, "port: %d", port);
    p_start = p_end;
    char* file_path = NULL;
    if (p_start != NULL && (url_len > (int) (p_start - url))) {
        asprintf(&file_path, "%s", p_start);
        ESP_LOGI(TAG, "file path: %s", file_path);
    } else {
        ESP_LOGI(TAG, "URL parse file path error");
        ret = ESP_ERR_INVALID_ARG;
        goto error;
    }
    ret = iot_ota_start(server_addr, port, file_path, ticks_to_wait);

    error: if (server_addr) {
        free(server_addr);
        server_addr = NULL;
    }
    if (file_path) {
        free(file_path);
        file_path = NULL;
    }
    return ret;
}
