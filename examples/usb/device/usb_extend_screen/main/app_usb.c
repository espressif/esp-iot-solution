/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_private/usb_phy.h"
#include "usb_descriptors.h"
#include "device/usbd.h"
#include "app_usb.h"

static const char *TAG = "app_usb";

//--------------------------------------------------------------------+
// USB PHY config
//--------------------------------------------------------------------+
static void usb_phy_init(void)
{
    usb_phy_handle_t phy_hdl;
    // Configure USB PHY
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
    };
    phy_conf.target = USB_PHY_TARGET_INT;
    usb_new_phy(&phy_conf, &phy_hdl);
}

static void tusb_device_task(void *arg)
{
    while (1) {
        tud_task();
    }
}

esp_err_t app_usb_init(void)
{
    esp_err_t ret = ESP_OK;

    usb_phy_init();
    bool usb_init = tusb_init();
    if (!usb_init) {
        ESP_LOGE(TAG, "USB Device Stack Init Fail");
        return ESP_FAIL;
    }

#if CFG_TUD_VENDOR
    ret = app_vendor_init();
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "app_vendor_init failed");
#endif

#if CFG_TUD_HID
    ret = app_hid_init();
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "app_hid_init failed");
#endif

#if CFG_TUD_AUDIO
    ret =  app_uac_init();
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "app_uac_init failed");
#endif

    xTaskCreate(tusb_device_task, "tusb_device_task", 4096, NULL, CONFIG_USB_TASK_PRIORITY, NULL);
    return ret;
}

/************************************************** TinyUSB callbacks ***********************************************/
// Invoked when device is mounted
void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "USB Mount");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "USB Un-Mount");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    ESP_LOGI(TAG, "USB Suspend");
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "USB Resume");
}
