/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_msc_host.h"
#include "esp_msc_ota.h"
#include "usb/usb_host.h"
#ifdef CONFIG_ESP32_S3_USB_OTG
#include "bsp/esp-bsp.h"
#endif

static const char *TAG = "usb_msc_ota";

#define OTA_FILE_NAME "/usb/" CONFIG_MSC_OTA_BIN_NAME

void print_progressbar(float progress, float total)
{
    const int bar_width = 50;
    int filled_width = progress * bar_width / total;

    printf("%s[", LOG_COLOR_I);
    for (int i = 0; i < bar_width; ++i) {
        if (i < filled_width) {
            printf(">");
        } else {
            printf(" ");
        }
    }
    printf(" ]%s%d%%\r", LOG_RESET_COLOR, filled_width * 100 / bar_width);
}

static void msc_ota_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ESP_MSC_OTA_START:
        ESP_LOGI(TAG, "ESP_MSC_OTA_START");
        break;
    case ESP_MSC_OTA_READY_UPDATE:
        ESP_LOGI(TAG, "ESP_MSC_OTA_READY_UPDATE");
        break;
    case ESP_MSC_OTA_WRITE_FLASH:
        float progress = *(float *)event_data;
        print_progressbar(progress, 1.0);
        break;
    case ESP_MSC_OTA_FAILED:
        ESP_LOGI(TAG, "ESP_MSC_OTA_FAILED");
        break;
    case ESP_MSC_OTA_GET_IMG_DESC:
        ESP_LOGI(TAG, "ESP_MSC_OTA_GET_IMG_DESC");
        break;
    case ESP_MSC_OTA_VERIFY_CHIP_ID:
        esp_chip_id_t chip_id = *(esp_chip_id_t *)event_data;
        ESP_LOGI(TAG, "ESP_MSC_OTA_VERIFY_CHIP_ID, chip_id: %08x", chip_id);
        break;
    case ESP_MSC_OTA_UPDATE_BOOT_PARTITION:
        esp_partition_subtype_t subtype = *(esp_partition_subtype_t *)event_data;
        ESP_LOGI(TAG, "ESP_MSC_OTA_UPDATE_BOOT_PARTITION, subtype: %d", subtype);
        break;
    case ESP_MSC_OTA_FINISH:
        ESP_LOGI(TAG, "ESP_MSC_OTA_FINISH");
        break;
    case ESP_MSC_OTA_ABORT:
        ESP_LOGI(TAG, "ESP_MSC_OTA_ABORT");
        break;
    }
}

void app_main(void)
{
#ifdef CONFIG_ESP32_S3_USB_OTG
    bsp_usb_mode_select_host();
    bsp_usb_host_power_mode(BSP_USB_HOST_POWER_MODE_USB_DEV, true);
#endif
    esp_event_loop_create_default();
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_MSC_OTA_EVENT, ESP_EVENT_ANY_ID, &msc_ota_event_handler, NULL));
    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG()
    };
    esp_msc_host_handle_t host_handle = NULL;
    esp_msc_host_install(&msc_host_config, &host_handle);

    esp_msc_ota_config_t config = {
        .ota_bin_path = OTA_FILE_NAME,
        .wait_msc_connect = portMAX_DELAY,
    };
#if CONFIG_SIMPLE_MSC_OTA
    esp_err_t ret = esp_msc_ota(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_msc_ota failed, ret: %d", ret);
    }
#else
    esp_msc_ota_handle_t msc_ota_handle = NULL;

    esp_err_t ret = esp_msc_ota_begin(&config, &msc_ota_handle);
    ESP_GOTO_ON_ERROR(ret, fail, TAG, "esp_msc_ota_begin failed, err: %d", ret);

    do {
        ret = esp_msc_ota_perform(msc_ota_handle);
        if (ret != ESP_OK) {
            break;
            ESP_LOGE(TAG, "esp_msc_ota_perform: (%s)\n", esp_err_to_name(ret));
        }
    } while (!esp_msc_ota_is_complete_data_received(msc_ota_handle));

    if (esp_msc_ota_is_complete_data_received(msc_ota_handle)) {
        esp_msc_ota_end(msc_ota_handle);
        ESP_LOGI(TAG, "esp msc ota complete");
    } else {
        esp_msc_ota_abort(msc_ota_handle);
        ESP_LOGE(TAG, "esp msc ota failed");
    }
fail:
#endif
    esp_msc_host_uninstall(host_handle);
}
