/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb_uac.h"
#include "hal/usb_hal.h"
#include "esp_private/usb_phy.h"

static void tusb_device_task(void *arg)
{
    while (1) {
        tud_task();
    }
}

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

void app_main(void)
{
    usb_phy_init();
    tud_init(BOARD_TUD_RHPORT);
    xTaskCreate(tusb_device_task, "TinyUSB", 4096, NULL, 5, NULL);
}
