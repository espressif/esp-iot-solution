/******************************************************************************
 * Copyright (C) 2014 -2016  Espressif System
 *
 * FileName: app_main.c
 *
 * Description:
 *
 * Modification history:
 * 2016/11/16, v0.0.1 create this file.
*******************************************************************************/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "string.h"
#include "esp_alink.h"
static const char *TAG = "alink_connect_ap";

alink_err_t alink_read_wifi_config(_OUT_ wifi_config_t *wifi_config)
{
    ALINK_PARAM_CHECK(wifi_config == NULL);
    alink_err_t ret   = -1;
    nvs_handle handle = 0;
    size_t length = sizeof(wifi_config_t);
    memset(wifi_config, 0, length);

    ret = nvs_open("ALINK", NVS_READWRITE, &handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_open ret:%x", ret);
    ret = nvs_get_blob(handle, "wifi_config", wifi_config, &length);
    nvs_close(handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ALINK_LOGD("nvs_get_blob ret:%x,No data storage,the read data is empty", ret);
        return ALINK_ERR;
    }
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_get_blob ret:%x", ret);
    return ALINK_OK;
}

alink_err_t alink_write_wifi_config(_IN_ const wifi_config_t *wifi_config)
{
    ALINK_PARAM_CHECK(wifi_config == NULL);
    alink_err_t ret   = -1;
    nvs_handle handle = 0;

    ret = nvs_open("ALINK", NVS_READWRITE, &handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_open ret:%x", ret);

    ret = nvs_set_blob(handle, "wifi_config", wifi_config, sizeof(wifi_config_t));
    nvs_commit(handle);
    nvs_close(handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_set_blob ret:%x", ret);
    return ALINK_OK;
}

alink_err_t alink_erase_wifi_config()
{
    alink_err_t ret   = -1;
    nvs_handle handle = 0;

    ret = nvs_open("ALINK", NVS_READWRITE, &handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_open ret:%x", ret);
    ret = nvs_erase_key(handle, "wifi_config");
    nvs_commit(handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ret, "nvs_erase_key ret:%x", ret);
    return ALINK_OK;
}

alink_err_t alink_connect_ap()
{
    alink_err_t ret = ALINK_ERR;
    wifi_config_t wifi_config;

    ret = alink_read_wifi_config(&wifi_config);
    if (ret == ALINK_OK) {
        if (platform_awss_connect_ap(WIFI_WAIT_TIME, (char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password,
                                     0, 0, wifi_config.sta.bssid, 0) == ALINK_OK) {
            return ALINK_OK;
        }
    }

    ALINK_LOGI("*********************************");
    ALINK_LOGI("*    ENTER SAMARTCONFIG MODE    *");
    ALINK_LOGI("*********************************");
    ret = awss_start();
    awss_stop();
    if (ret != ALINK_OK) {
        ALINK_LOGI("awss_start is err ret: %d", ret);
        esp_restart();
    }
    return ALINK_OK;
}
