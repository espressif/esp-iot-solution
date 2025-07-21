/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "esp_private/usb_phy.h"
#include "device/usbd.h"

static const char *TAG = "tusb_cdc_acm";
static usb_phy_handle_t phy_hdl =  NULL;
static TaskHandle_t uvc_task_hdl = NULL;
static TaskHandle_t cdc_task_hdl = NULL;

//****************************************************TinyUSB callbacks*/
// CDC-ACM
//**********************************************************************/

static void cdc_task(void *arg)
{
    uint8_t itf;
    while (1) {
        for (itf = 0; itf < CFG_TUD_CDC; itf++) {
            if (tud_cdc_n_available(itf)) {
                uint8_t buf[512];

                uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

                // echo back to serial ports
                tud_cdc_n_write(itf, buf, count);
                tud_cdc_n_write_flush(itf);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void tud_cdc_rx_cb(uint8_t itf)
{
    (void) itf;
}

//--------------------------------------------------------------------+
// USB PHY config
//--------------------------------------------------------------------+
static void usb_phy_init(void)
{
    // Configure USB PHY
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .target = USB_PHY_TARGET_INT,
#if CONFIG_TINYUSB_RHPORT_HS
        .otg_speed = USB_PHY_SPEED_HIGH,
#endif
    };
    usb_new_phy(&phy_conf, &phy_hdl);
}

static void tusb_device_task(void *arg)
{
    while (1) {
        tud_task();
    }
}

esp_err_t device_cdc_init(void)
{
    esp_err_t ret = ESP_OK;

    usb_phy_init();
    bool usb_init = tusb_init();
    if (!usb_init) {
        ESP_LOGE(TAG, "USB Device Stack Init Fail");
        return ESP_FAIL;
    }

    xTaskCreate(tusb_device_task, "tusb_device_task", 4096, NULL, 5, &uvc_task_hdl);
    xTaskCreate(cdc_task, "cdc_task", 4096, NULL, 5, &cdc_task_hdl);
    ESP_LOGI(TAG, "USB Device CDC Init");
    return ret;
}

esp_err_t device_cdc_deinit(void)
{
    esp_err_t ret = ESP_OK;
    tusb_teardown();
    vTaskDelete(uvc_task_hdl);
    vTaskDelete(cdc_task_hdl);
    usb_del_phy(phy_hdl);
    ESP_LOGI(TAG, "USB Device CDC Deinit");
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
