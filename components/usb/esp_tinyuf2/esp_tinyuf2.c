#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_private/usb_phy.h"
#include "esp_log.h"
#include "board_flash.h"
#include "tusb.h"
#include "esp_tinyuf2.h"
#include "uf2.h"

const static char* TAG = "TUF2";

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

esp_err_t tinyuf2_updater_install(tinyuf2_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid Parameter, config can’t be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (config->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MIN || (config->subtype > ESP_PARTITION_SUBTYPE_APP_OTA_MAX && config->subtype != ESP_PARTITION_SUBTYPE_ANY)) {
        ESP_LOGE(TAG, "Invalid partition type");
        return ESP_ERR_INVALID_ARG;
    }

    if (config->if_restart) {
        ESP_LOGW(TAG, "Enable restart, SoC will restart after update complete");
    }

    board_flash_init(config->subtype, config->label, config->complete_cb, config->if_restart);
    uf2_init();
    tinyusb_init();
    ESP_LOGI(TAG, "UF2 Updater install succeed, Version: %d.%d.%d", ESP_TINYUF2_VER_MAJOR, ESP_TINYUF2_VER_MINOR, ESP_TINYUF2_VER_PATCH);

    return ESP_OK;
}
