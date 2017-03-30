/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP32 only, in which case,
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

#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "freertos/timers.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "tcpip_adapter.h"

#include "joylink.h"

static char esp_ota_url_schema[8];
static char* esp_ota_server_ip = NULL;
static uint16_t esp_ota_server_port = 0;
static char esp_ota_file_name[64];
#define ESP_OTA_URL_SCHEMA   esp_ota_url_schema
#define ESP_OTA_SERVER_IP    esp_ota_server_ip
#define ESP_OTA_SERVER_PORT  esp_ota_server_port
#define ESP_OTA_FILE_NAME    esp_ota_file_name

#define TEXT_BUFFSIZE 1024

#define ESP_OTA_TIMEOUT_MS               (60*3*1000)

static xTimerHandle esp_joylink_timeout_timer = NULL;
static bool esp_at_ota_timeout_flag = false;
static int esp_at_ota_socket_id = -1;

#define ESP_JOYLINK_OTA_DEBUG  printf


typedef struct URLInfo {
    char schema[8];
    char host[256];
    char host_name[256];
    unsigned int port;
    char file[256];
} URLInfo;

bool parse_url(const char* url) {
    int i = 0, j = 0;
    char schema[8] = { 0 };
    char host[256] = { 0 };
    char port[8] = { 0 };
    char file[256] = { 0 };
    int32_t tmp_port = 0;

    while (url[i] != ':') {
        schema[j++] = url[i++];
    }
    for (i += 3, j = 0; url[i] != ':' && url[i] != '/' && url[i] != 0;) {
        host[j++] = url[i++];
    }
    if (url[i] == ':') {
        for (i += 1, j = 0; url[i] != '/';) {
            port[j++] = url[i++];
        }
        sscanf(port, "%d", &tmp_port);
        ESP_OTA_SERVER_PORT = tmp_port;
    } else {
        ESP_OTA_SERVER_PORT = 80;
    }
    if (url[i] != 0) {
        for (j = 0; url[i] != 0;) {
            file[j++] = url[i++];
        }
    } else {
        file[0] = '/';
    }

    if (ESP_OTA_SERVER_PORT == 0) {
        ESP_OTA_SERVER_PORT = 80;
    }
    strncpy(ESP_OTA_URL_SCHEMA, schema,sizeof(ESP_OTA_URL_SCHEMA));
    strncpy(ESP_OTA_FILE_NAME, file,sizeof(ESP_OTA_FILE_NAME));
    strncpy(ESP_OTA_SERVER_IP, host,256);

    return true;
}

static void esp_at_ota_timeout_callback( TimerHandle_t xTimer )
{
    ESP_JOYLINK_OTA_DEBUG("ota timeout!\r\n");
    esp_at_ota_timeout_flag = true;
    if (esp_at_ota_socket_id >= 0) {
        close(esp_at_ota_socket_id);
        esp_at_ota_socket_id = -1;
    }
}

bool esp_ota_upgrade_process(const char* url)
{
    bool pkg_body_start = false;
    struct sockaddr_in sock_info;
    ip_addr_t ip_address;
    struct hostent* hptr = NULL;
    uint8_t* http_request = NULL;
    uint8_t* data_buffer = NULL;
    uint8_t* pStr = NULL;
    esp_partition_t* partition_ptr = NULL;
    esp_partition_t partition;
    esp_ota_handle_t out_handle = 0;
    int buff_len = 0;
    int sockopt = 1;
    int total_len = 0;
    int recv_len = 0;
    bool ret = false;
    extern JLDevice_t  *_g_pdev;
    JLOtaUpload_t* otaUpload = 0;
    otaUpload = (JLOtaUpload_t*)malloc(sizeof(JLOtaUpload_t));

    if (otaUpload == NULL) {
        return false;
    }

    memset(otaUpload,0x0,sizeof(JLOtaUpload_t));
    memcpy(otaUpload->feedid, _g_pdev->jlp.feedid,JL_MAX_FEEDID_LEN);
    memcpy(otaUpload->productuuid, _g_pdev->jlp.uuid,JL_MAX_UUID_LEN);

    if (url == NULL) {
        strcpy(otaUpload->status_desc, "url error");
        goto OTA_ERROR;
    }

    ESP_OTA_SERVER_IP = (char*)malloc(256);
    if (ESP_OTA_SERVER_IP == NULL) {
        ESP_JOYLINK_OTA_DEBUG("server ip malloc fail\r\n");
        strcpy(otaUpload->status_desc, "memory lack");
        goto OTA_ERROR;
    }

    if (parse_url(url) == false) {
        ESP_JOYLINK_OTA_DEBUG("parse url fail\r\n");
        strcpy(otaUpload->status_desc, "parse url fail");
        goto OTA_ERROR;
    }
    if (esp_joylink_timeout_timer != NULL) {
        xTimerStop(esp_joylink_timeout_timer,portMAX_DELAY);
        xTimerDelete(esp_joylink_timeout_timer,portMAX_DELAY);
        esp_joylink_timeout_timer = NULL;
    }
    esp_at_ota_timeout_flag = false;
    esp_joylink_timeout_timer = xTimerCreate("ota_timer",
                ESP_OTA_TIMEOUT_MS/portTICK_PERIOD_MS,
                pdFAIL,
                NULL,
                esp_at_ota_timeout_callback);
    xTimerStart(esp_joylink_timeout_timer,portMAX_DELAY);
    ip_address.u_addr.ip4.addr = inet_addr(ESP_OTA_SERVER_IP);

    if ((ip_address.u_addr.ip4.addr == IPADDR_NONE) && (strcmp(ESP_OTA_SERVER_IP,"255.255.255.255") != 0)) {
        if((hptr = gethostbyname(ESP_OTA_SERVER_IP)) == NULL)
        {
            ESP_JOYLINK_OTA_DEBUG("gethostbyname fail\r\n");
            goto OTA_ERROR;
        }
        ip_address = *(ip_addr_t*)hptr->h_addr_list[0];
    }
    esp_at_ota_socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (esp_at_ota_socket_id < 0) {
        goto OTA_ERROR;
    }

    setsockopt(esp_at_ota_socket_id, SOL_SOCKET, SO_REUSEADDR,&sockopt, sizeof(sockopt));
    // set connect info
    memset(&sock_info, 0, sizeof(struct sockaddr_in));
    sock_info.sin_family = AF_INET;
    sock_info.sin_addr.s_addr = ip_address.u_addr.ip4.addr;
    sock_info.sin_port = htons(ESP_OTA_SERVER_PORT);

    // connect to http server
    if (connect(esp_at_ota_socket_id, (struct sockaddr*)&sock_info, sizeof(sock_info)) < 0)
    {
        ESP_JOYLINK_OTA_DEBUG("connect to server failed!\r\n");
        goto OTA_ERROR;
    }


    http_request = (uint8_t*)malloc(TEXT_BUFFSIZE);
    if (http_request == NULL) {
        goto OTA_ERROR;
    }
    data_buffer = (uint8_t*)malloc(TEXT_BUFFSIZE);
    if (data_buffer == NULL) {
        goto OTA_ERROR;
    }
//    snprintf((char*)http_request,TEXT_BUFFSIZE,"GET /v1/device/rom/?is_format_simple=true HTTP/1.0\r\nHost: "IPSTR":%d\r\n"pheadbuffer"",
//             IP2STR(&ip_address.u_addr.ip4),
//             ESP_OTA_SERVER_PORT, ESP_AT_BIN_KEY);
    snprintf((char*)http_request,TEXT_BUFFSIZE,"GET %s HTTP/1.1\nHost: %s\nAccept: */*\nReferer:http://%s\nUser-Agent: Mozilla/4.0 (compatible; MSIE 5.00; Windows 98)\nPragma: no-cache\nCache-Control: no-cache\nConnection: close\n\n",
            ESP_OTA_FILE_NAME, ESP_OTA_SERVER_IP,ESP_OTA_SERVER_IP);
    /*send GET request to http server*/
    if (send(esp_at_ota_socket_id, http_request, strlen((char*)http_request), 0) < 0)
    {
    	ESP_JOYLINK_OTA_DEBUG("send GET request to server failed\r\n");
        goto OTA_ERROR;
    }
#if 0
    memset(data_buffer,0x0,TEXT_BUFFSIZE);
    if (recv(esp_at_ota_socket_id, data_buffer, TEXT_BUFFSIZE, 0) < 0) {
        ESP_JOYLINK_OTA_DEBUG("recv data from server failed!\r\n");
    	goto OTA_ERROR;
    }
#endif

    // search ota partition
    partition_ptr = (esp_partition_t*)esp_ota_get_boot_partition();
    if (partition_ptr == NULL) {
        ESP_JOYLINK_OTA_DEBUG("boot partition NULL!\r\n");
        goto OTA_ERROR;
    }
    if (partition_ptr->type != ESP_PARTITION_TYPE_APP)
    {
        ESP_JOYLINK_OTA_DEBUG("esp_current_partition->type != ESP_PARTITION_TYPE_APP\r\n");
        goto OTA_ERROR;
    }

    /*choose which OTA image should we write to*/
    switch(partition_ptr->subtype)
    {
        case  ESP_PARTITION_SUBTYPE_APP_OTA_0:
            partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
            break;
        case ESP_PARTITION_SUBTYPE_APP_OTA_1:
            partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
            break;
        default:
            ESP_JOYLINK_OTA_DEBUG("subtype error!\r\n");
            goto OTA_ERROR;
    }
    partition.type = ESP_PARTITION_TYPE_APP;

    // partition = (esp_partition_t*)esp_partition_find_first(partition->type,partition->subtype,NULL);
    partition_ptr = (esp_partition_t*)esp_partition_find_first(partition.type,partition.subtype,NULL);
    if (partition_ptr == NULL) {
        ESP_JOYLINK_OTA_DEBUG("partition NULL!\r\n");
        goto OTA_ERROR;
    }
    memcpy(&partition,partition_ptr,sizeof(esp_partition_t));
    if(esp_ota_begin(&partition, OTA_SIZE_UNKNOWN, &out_handle) != ESP_OK)
    {
        ESP_JOYLINK_OTA_DEBUG("esp_ota_begin failed!\r\n");
        goto OTA_ERROR;
    }

    /*deal with all receive packet*/
    for (;;)
    {
        memset(data_buffer, 0x0, TEXT_BUFFSIZE);
        buff_len = recv(esp_at_ota_socket_id, data_buffer, TEXT_BUFFSIZE, 0);
        if (buff_len < 0) {
            ESP_JOYLINK_OTA_DEBUG("receive data error!\r\n");
            strcpy(otaUpload->status_desc, "receive data fail");
            goto OTA_ERROR;
        } else if (buff_len > 0 && !pkg_body_start) {
            if ((strstr((char*)data_buffer, "HTTP/1.1 206") == NULL)
                    && (strstr((char*)data_buffer, "HTTP/1.0 206") == NULL)
                    && (strstr((char*)data_buffer, "HTTP/1.1 200") == NULL)
                    && (strstr((char*)data_buffer, "HTTP/1.0 200") == NULL)) {
                break;
            }
            // search "\r\n\r\n"
            pStr = (uint8_t*)strstr((char*)data_buffer,"Content-Length: ");
            if (pStr == NULL) {
                break;
            }
            pStr += strlen("Content-Length: ");
            total_len = atoi((char*)pStr);
            ESP_JOYLINK_OTA_DEBUG("total_len=%d!\r\n",total_len);
            pStr = (uint8_t*)strstr((char*)data_buffer,"\r\n\r\n");
            if (pStr) {
                pkg_body_start = true;
                pStr += 4; // skip "\r\n"
                if (pStr[0] != 0xE9) {
                    ESP_JOYLINK_OTA_DEBUG("OTA Write Header format Check Failed! first byte is %02x ,second byte is %02x\r\n", pStr[0],pStr[1]);
                    strcpy(otaUpload->status_desc, "magic error");
                    goto OTA_ERROR;
                }
                // pStr += 2;
                buff_len -= (pStr - data_buffer);
                if(esp_ota_write(out_handle, (const void*)pStr, buff_len) != ESP_OK)
                {
                    ESP_JOYLINK_OTA_DEBUG("esp_ota_write failed!\r\n");
                    strcpy(otaUpload->status_desc, "write flash fail");
                    goto OTA_ERROR;
                }

                recv_len += buff_len;
                otaUpload->status = 0;
                otaUpload->progress = (((double)recv_len)/total_len) * 100;
                strcpy(otaUpload->status_desc, "downloading");
                joylink_server_ota_status_upload_req(otaUpload);
            }
        } else if (buff_len > 0 && pkg_body_start) {
            if(esp_ota_write( out_handle, (const void*)data_buffer, buff_len) != ESP_OK) {
                ESP_JOYLINK_OTA_DEBUG("esp_ota_write failed!\r\n");
                strcpy(otaUpload->status_desc, "write flash fail");
                goto OTA_ERROR;
            }

            recv_len += buff_len;
            otaUpload->status = 0;
            otaUpload->progress = (((double)recv_len)/total_len) * 100;
            strcpy(otaUpload->status_desc, "downloading");
            joylink_server_ota_status_upload_req(otaUpload);
        } else if (buff_len == 0) {
            ESP_JOYLINK_OTA_DEBUG("receive all packet over!\r\n");
            if (recv_len != total_len) {
                strcpy(otaUpload->status_desc, "receive packet fail");
                goto OTA_ERROR;
            }
        } else {
            ESP_JOYLINK_OTA_DEBUG("Warning: uncontolled event!\r\n");
        }
        ESP_JOYLINK_OTA_DEBUG("recv_len=%d,total_len=%d!\r\n",recv_len,total_len);
        if (recv_len == total_len) {
            break;
        }
    }

    if(esp_ota_end(out_handle) != ESP_OK)
    {
        ESP_JOYLINK_OTA_DEBUG("esp_ota_end failed!\r\n");
        strcpy(otaUpload->status_desc, "check fail");
        goto OTA_ERROR;
    }

    otaUpload->status = 1;
    otaUpload->progress = 100;
    strcpy(otaUpload->status_desc, "installing");
    joylink_server_ota_status_upload_req(otaUpload);
    if(esp_ota_set_boot_partition(&partition) != ESP_OK)
    {
        ESP_JOYLINK_OTA_DEBUG("esp_ota_set_boot_partition failed!\r\n");
        strcpy(otaUpload->status_desc, "set bin fail");
        goto OTA_ERROR;
    }

    ret = true;
OTA_ERROR:
    if (esp_joylink_timeout_timer != NULL) {
        xTimerStop(esp_joylink_timeout_timer,portMAX_DELAY);
        xTimerDelete(esp_joylink_timeout_timer,portMAX_DELAY);
        esp_joylink_timeout_timer = NULL;
    }
    
    if (esp_at_ota_socket_id >= 0) {
        close(esp_at_ota_socket_id);
        esp_at_ota_socket_id = -1;
    }
    
    if (http_request) {
        free(http_request);
        http_request = NULL;
    }

    if (data_buffer) {
        free(data_buffer);
        data_buffer = NULL;
    }
    
    if (ESP_OTA_SERVER_IP) {
        free(ESP_OTA_SERVER_IP);
        ESP_OTA_SERVER_IP = NULL;
    }

    if (ret == true) {
        otaUpload->status = 2;
        otaUpload->progress = 100;
        strcpy(otaUpload->status_desc, "installed");
    } else {
        otaUpload->status = 3;
        otaUpload->progress = 0;
    }
    joylink_server_ota_status_upload_req(otaUpload);

    if (otaUpload) {
        free(otaUpload);
        otaUpload = NULL;
    }
    return ret;
}

static void esp_ota_task(void* para)
{
    esp_ota_upgrade_process((const char*)para);
    free(para);
    vTaskDelete(NULL);
}
void esp_ota_task_start(char* url)
{
    if (url == NULL) {
        return;
    }
    char *data = (char*)malloc(strlen(url) + 1);
    if (data == NULL) {
        return;
    }
    memset(data,0x0,sizeof(strlen(url) + 1));
    strcpy(data,url);
    xTaskCreate(esp_ota_task, "ota", 1024*4, (void*)data, 2, NULL);
}

