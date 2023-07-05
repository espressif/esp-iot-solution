/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_private/usb_phy.h"
#include "esp_log.h"
#include "board_flash.h"
#include "tusb.h"
#include "esp_tinyuf2.h"
#include "uf2.h"

#ifdef CONFIG_ENABLE_UF2_USB_CONSOLE
#include "tusb_cdc_acm.h"
#include "tusb_console.h"
#endif

const static char* TAG = "TUF2";
static bool _if_init = false;

// USB Device Driver task
static void usb_phy_init(void)
{
    // Configure USB PHY
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
    };
    phy_conf.target = USB_PHY_TARGET_INT;
    usb_phy_handle_t phy_hdl;
    usb_new_phy(&phy_conf, &phy_hdl);
}

// This top level thread process all usb events and invoke callbacks
static void usb_device_task(void* param)
{
    (void) param;
    // RTOS forever loop
    while (1) {
        // put this thread to waiting state until there is new events
        tud_task();
    }
}

esp_err_t tinyusb_init()
{
    usb_phy_init();
    // init device stack on configured roothub port
    // This should be called after scheduler/kernel is started.
    // Otherwise it could cause kernel issue since USB IRQ handler does use RTOS queue API.
    tud_init(BOARD_TUD_RHPORT);
    xTaskCreatePinnedToCore(usb_device_task, "usbd", 4096, NULL, 5, NULL, 0);
    return ESP_OK;
}

esp_err_t esp_tinyuf2_install(tinyuf2_ota_config_t *ota_config, tinyuf2_nvs_config_t *nvs_config)
{
    if (_if_init) {
        ESP_LOGE(TAG, "Tinyuf2 already installed");
        return ESP_ERR_INVALID_STATE;
    }
    _if_init = true;

    if (!(ota_config || nvs_config)) {
        ESP_LOGE(TAG, "Invalid Parameter, config can’t be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (ota_config) {
        if (ota_config->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MIN || (ota_config->subtype > ESP_PARTITION_SUBTYPE_APP_OTA_MAX && ota_config->subtype != ESP_PARTITION_SUBTYPE_ANY)) {
            ESP_LOGE(TAG, "Invalid partition type");
            return ESP_ERR_INVALID_ARG;
        }
        if (ota_config->if_restart) {
            ESP_LOGW(TAG, "Enable restart, SoC will restart after update complete");
        }
        board_flash_init(ota_config->subtype, ota_config->label, ota_config->complete_cb, ota_config->if_restart);
    }

    if (nvs_config) {
        board_flash_nvs_init(nvs_config->part_name, nvs_config->namespace_name, nvs_config->modified_cb);
    }

    uf2_init();
    tinyusb_init();
#ifdef CONFIG_ENABLE_UF2_USB_CONSOLE
    tinyusb_config_cdcacm_t acm_cfg = { 0 }; // the configuration uses default values
    tusb_cdc_acm_init(&acm_cfg);
    esp_tusb_init_console(TINYUSB_CDC_ACM_0);
#endif
    ESP_LOGI(TAG, "UF2 Updater install succeed, Version: %d.%d.%d", ESP_TINYUF2_VER_MAJOR, ESP_TINYUF2_VER_MINOR, ESP_TINYUF2_VER_PATCH);

    return ESP_OK;
}
