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

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_log.h"
#include "button.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "joylink_obj.h"
#include "virtual_device.h"

static SemaphoreHandle_t xSemWriteInfo = NULL;
CJoylink* p_joylink = NULL;
static const char* TAG = "alink_test";

static virtual_device_t virtual_device = {
    0x01, 0x30, 0x50, 0, 0x01
};

static const char *joylink_json_upload_str = \
"{\
\"code\":%d,\
\"data\":[\
{\"current_value\": \"%d\",\"stream_id\": \"power\"},\
{\"current_value\": \"%d\",\"stream_id\": \"brightness\"},\
{\"current_value\": \"%d\",\"stream_id\": \"colortemp\"},\
{\"current_value\": \"%d\",\"stream_id\": \"mode\"},\
{\"current_value\": \"%s\",\"stream_id\": \"color\"}\
]}";

static void read_task_test(void *arg)
{
    for (;;) {
        char *down_cmd = (char *) malloc(JOYLINK_DATA_LEN);
        p_joylink->read(down_cmd, JOYLINK_DATA_LEN, portMAX_DELAY);
        free(down_cmd);
        xSemaphoreGive(xSemWriteInfo);
    }
    vTaskDelete(NULL);
}

static void write_task_test(void *arg)
{
    esp_err_t ret;
    for (;;) {
        xSemaphoreTake(xSemWriteInfo, portMAX_DELAY);
        char *up_cmd = (char *)malloc(JOYLINK_DATA_LEN);
        memset(up_cmd, 0, JOYLINK_DATA_LEN);
        sprintf(up_cmd, joylink_json_upload_str, JOYLINK_RET_JSON_OK, virtual_device.power, virtual_device.light_value,
            virtual_device.temp_value, virtual_device.work_mode, "red");
        ret = p_joylink->write(up_cmd, JOYLINK_DATA_LEN, 1000);
        free(up_cmd);
        if (ret == ESP_FAIL) {
            ESP_LOGW(TAG, "esp_write is err");
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(NULL, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void initialise_key(void)
{
    button_handle_t btn_handle = button_create(GPIO_NUM_0, BUTTON_ACTIVE_LOW, BUTTON_SINGLE_TRIGGER, 1);
    button_add_cb(btn_handle, BUTTON_PUSH_CB, joylink_button_smnt_tap_cb, NULL, 50 / portTICK_PERIOD_MS);

    btn_handle = button_create(GPIO_NUM_15, BUTTON_ACTIVE_LOW, BUTTON_SINGLE_TRIGGER, 1);
    button_add_cb(btn_handle, BUTTON_PUSH_CB, joylink_button_reset_tap_cb, NULL, 50 / portTICK_PERIOD_MS);
}

extern "C" void joylink_test()
{
    if (xSemWriteInfo == NULL) {
        xSemWriteInfo = xSemaphoreCreateBinary();
    }
    initialise_wifi();
    initialise_key();
    printf("mode: json, free_heap: %u\n", esp_get_free_heap_size());
    joylink_info_t product_info;
    strcpy(product_info.innet_aes_key, JOYLINK_AES_KEY);
    product_info.jlp.version = JOYLINK_VERSION;
    strcpy(product_info.jlp.accesskey, JOYLINK_ACCESSKEY);
    strcpy(product_info.jlp.localkey, JOYLINK_LOCAL_KEY);
    strcpy(product_info.jlp.feedid, JOYLINK_FEEDID);
    product_info.jlp.devtype = JOYLINK_DEVTYPE;
    strcpy(product_info.jlp.joylink_server, JOYLINK_SERVER);
    product_info.jlp.server_port = JOYLINK_SERVER_PORT;
    strcpy(product_info.jlp.CID, JOYLINK_CID);
    strcpy(product_info.jlp.firmwareVersion, JOYLINK_FW_VERSION);
    strcpy(product_info.jlp.modelCode, JOYLINK_MODEL_CODE);
    strcpy(product_info.jlp.uuid, JOYLINK_UUID);
    product_info.jlp.lancon = JOYLINK_LAN_CTRL;
    product_info.jlp.cmd_tran_type = JOYLINK_CMD_TYPE;
    uint8_t dev_mac[6];
    char dev_mac_str[20];
    memset(dev_mac_str, 0, 20);
    esp_wifi_get_mac(ESP_IF_WIFI_STA, dev_mac);
    sprintf(dev_mac_str, ""MACSTR"", MAC2STR(dev_mac));
    strcpy(product_info.jlp.mac, dev_mac_str);
    printf("*********************************\r\n");
    printf("*         PRODUCT INFO          *\r\n");
    printf("*********************************\r\n");
    printf("UUID     : %s\r\n", product_info.jlp.uuid);
    printf("mac      : %s\r\n", product_info.jlp.mac);
    printf("type     : %d\r\n", product_info.jlp.devtype);
    printf("version  : %d\r\n", product_info.jlp.version);
    printf("fw_ver   : %s\r\n", product_info.jlp.firmwareVersion);
    printf("model    : %s\r\n", product_info.jlp.modelCode);
    printf("tran type: %d\r\n", product_info.jlp.cmd_tran_type);
    p_joylink = CJoylink::GetInstance(product_info);
    xTaskCreate(read_task_test, "read_task_test", 1024 * 8, NULL, 9, NULL);
    xTaskCreate(write_task_test, "write_task_test", 1024 * 8, NULL, 4, NULL);
}