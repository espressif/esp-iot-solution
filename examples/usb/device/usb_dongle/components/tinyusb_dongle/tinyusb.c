/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_private/periph_ctrl.h"
#include "esp_private/usb_phy.h"
#include "tinyusb.h"
#include "descriptors_control.h"
#include "tusb.h"
#include "tusb_tasks.h"

const static char *TAG = "TinyUSB";
static usb_phy_handle_t phy_hdl;

void tud_mount_cb(void)
{
    ESP_EARLY_LOGI(TAG, "USB mounted");
}

void tud_umount_cb(void)
{
    ESP_EARLY_LOGI(TAG, "USB unmounted");
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    ESP_EARLY_LOGI(TAG, "USB suspended");
}

void tud_resume_cb(void)
{
    ESP_EARLY_LOGI(TAG, "USB resumed");
}

esp_err_t tinyusb_driver_install(const tinyusb_config_t *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Config can't be NULL");

    // Configure USB PHY
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .target = USB_PHY_TARGET_INT,
        .otg_speed = USB_PHY_SPEED_FULL,
    };
#if CONFIG_TINYUSB_RHPORT_HS
    phy_conf.otg_speed = USB_PHY_SPEED_HIGH;
#endif

    usb_phy_ext_io_conf_t ext_io_conf;
    if (config->external_phy) {
        ESP_RETURN_ON_FALSE(config->external_phy_io, ESP_ERR_INVALID_ARG, TAG,
                            "External PHY IO config can't be NULL");
        const tinyusb_ext_phy_io_config_t *io = config->external_phy_io;
        ext_io_conf = (usb_phy_ext_io_conf_t) {
            .vp_io_num = io->vp_io_num,
            .vm_io_num = io->vm_io_num,
            .rcv_io_num = io->rcv_io_num,
            .suspend_n_io_num = io->suspend_n_io_num,
            .oen_io_num = io->oen_io_num,
            .vpo_io_num = io->vpo_io_num,
            .vmo_io_num = io->vmo_io_num,
            .fs_edge_sel_io_num = io->fs_edge_sel_io_num,
        };
        phy_conf.target = USB_PHY_TARGET_EXT;
        phy_conf.ext_io_conf = &ext_io_conf;
    } else {
        phy_conf.target = USB_PHY_TARGET_INT;
    }

    // OTG IOs config
    const usb_phy_otg_io_conf_t otg_io_conf = USB_PHY_SELF_POWERED_DEVICE(config->vbus_monitor_io);
    if (config->self_powered) {
        phy_conf.otg_io_conf = &otg_io_conf;
    }
    ESP_RETURN_ON_ERROR(usb_new_phy(&phy_conf, &phy_hdl), TAG, "Install USB PHY failed");

    // Descriptors config
    ESP_RETURN_ON_ERROR(tinyusb_set_descriptors(config), TAG, "Descriptors config failed");

    // Init
#if !CONFIG_TINYUSB_INIT_IN_DEFAULT_TASK
    ESP_RETURN_ON_FALSE(tusb_init(), ESP_FAIL, TAG, "Init TinyUSB stack failed");
#endif
#if !CONFIG_TINYUSB_NO_DEFAULT_TASK
    ESP_RETURN_ON_ERROR(tusb_run_task(), TAG, "Run TinyUSB task failed");
#endif
    ESP_LOGI(TAG, "TinyUSB Driver installed");
    return ESP_OK;
}

esp_err_t tinyusb_driver_uninstall()
{
    tinyusb_free_descriptors();
    return usb_del_phy(phy_hdl);
}
