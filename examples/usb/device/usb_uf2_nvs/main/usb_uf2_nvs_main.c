/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_tinyuf2.h"

static const char *TAG = "uf2_nvs_example";
#define EXAMPLE_ESP_MAXIMUM_RETRY 5
#define NVS_MODIFIED_BIT          BIT0
static int s_retry_num = 0;
static bool s_no_ssid_pwd = false;
static EventGroupHandle_t s_event_group;

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            ESP_LOGE(TAG, "connect to the AP fail, retry times exceed");
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
    }
}

static void wifi_sta_start(const char *ssid, const char *password)
{
    static bool wifi_inited = false;

    if (!wifi_inited) {
        ESP_ERROR_CHECK(esp_netif_init());

        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_got_ip));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        wifi_inited = true;
    }

    if (ssid && password) {
        esp_wifi_stop();
        wifi_config_t wifi_config = {0};
        strlcpy((char *)wifi_config.sta.ssid, ssid, strlen(ssid) + 1);
        strlcpy((char *)wifi_config.sta.password, password, strlen(password) + 1);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "wifi start ssid:%s password:%s", ssid, password);
    } else {
        ESP_LOGI(TAG, "invalid ssid or password, wifi not start");
    }
}

static void uf2_nvs_modified_cb()
{
    ESP_LOGI(TAG, "uf2 nvs modified");
    xEventGroupSetBits(s_event_group, NVS_MODIFIED_BIT);
}

void app_main(void)
{
#ifdef CONFIG_ESP32_S3_USB_OTG
    const gpio_config_t io_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_18),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_18, 0));
#endif
    esp_err_t err = ESP_OK;
    const char *uf2_nvs_partition = "nvs";
    const char *uf2_nvs_namespace = "wifi_config";
    char ssid[32] = {0};
    char password[64] = {0};
    s_event_group = xEventGroupCreate();
    // Initialize NVS
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t my_handle;
    err = nvs_open_from_partition(uf2_nvs_partition, uf2_nvs_namespace, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        size_t buf_len_long = sizeof(ssid);
        err = nvs_get_str(my_handle, "ssid", ssid, &buf_len_long);
        if (err != ESP_OK || buf_len_long == 0) {
            /* give a init value */
            s_no_ssid_pwd = true;
            ESP_ERROR_CHECK(nvs_set_str(my_handle, "ssid", "myssid"));
            ESP_ERROR_CHECK(nvs_commit(my_handle));
            ESP_LOGI(TAG, "no ssid, give a init value to nvs");
        } else {
            ESP_LOGI(TAG, "stored ssid:%s", ssid);
        }
        buf_len_long = sizeof(password);
        err = nvs_get_str(my_handle, "password", password, &buf_len_long);
        if (err != ESP_OK || buf_len_long == 0) {
            /* give a init value */
            s_no_ssid_pwd = true;
            ESP_ERROR_CHECK(nvs_set_str(my_handle, "password", "mypassword"));
            ESP_ERROR_CHECK(nvs_commit(my_handle));
            ESP_LOGI(TAG, "no password, give a init value to nvs");
        } else {
            ESP_LOGI(TAG, "stored password:%s", password);
        }
    }
    nvs_close(my_handle);

    /* install UF2 NVS */
    tinyuf2_nvs_config_t nvs_config = DEFAULT_TINYUF2_NVS_CONFIG();
    nvs_config.part_name = uf2_nvs_partition;
    nvs_config.namespace_name = uf2_nvs_namespace;
    nvs_config.modified_cb = uf2_nvs_modified_cb;

    ESP_ERROR_CHECK(esp_tinyuf2_install(NULL, &nvs_config));

    if (s_no_ssid_pwd == false) {
        wifi_sta_start(ssid, password);
    }

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(s_event_group, NVS_MODIFIED_BIT,
                                               pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & NVS_MODIFIED_BIT) {
            nvs_open_from_partition(uf2_nvs_partition, uf2_nvs_namespace, NVS_READONLY, &my_handle);
            size_t buf_len_long = sizeof(ssid);
            ESP_ERROR_CHECK(nvs_get_str(my_handle, "ssid", ssid, &buf_len_long));
            buf_len_long = sizeof(password);
            ESP_ERROR_CHECK(nvs_get_str(my_handle, "password", password, &buf_len_long));
            nvs_close(my_handle);
            wifi_sta_start(ssid, password);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
