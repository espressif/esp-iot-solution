/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_rom_gpio.h"
#include "soc/soc_caps.h"
#include "soc/gpio_pins.h"
#include "soc/gpio_sig_map.h"
#include "soc/rtc_cntl_struct.h"
#include "tusb.h"
#include "esp_tinyuf2.h"
#include "esp_private/usb_phy.h"
#include "board_flash.h"
#include "uf2.h"

#if SOC_USB_SERIAL_JTAG_SUPPORTED
#include "soc/usb_serial_jtag_reg.h"
#endif

#ifdef CONFIG_ENABLE_UF2_USB_CONSOLE
#include "tusb_cdc_acm.h"
#include "tusb_console.h"
#endif

const static char* TAG = "TUF2";
static tinyuf2_state_t _uf2_state = TINYUF2_STATE_NOT_INSTALLED;
static TaskHandle_t _task_handle = NULL;

#ifdef CONFIG_PM_ENABLE
static esp_pm_lock_handle_t pm_locks_cpu_max = NULL;
#endif

// Invoked when device is mounted
void tud_mount_cb(void)
{
#ifdef CONFIG_ENABLE_UF2_USB_CONSOLE
    ESP_LOGI(TAG, "USB Mounted");
    ESP_LOGI(TAG, "Enable USB console, log will be output to USB");
    tinyusb_config_cdcacm_t acm_cfg = { 0 }; // the configuration uses default values
    tusb_cdc_acm_init(&acm_cfg);
    esp_tusb_init_console(TINYUSB_CDC_ACM_0);
#endif
    _uf2_state = TINYUF2_STATE_MOUNTED;
    ESP_LOGI(TAG, "USB Mounted");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
#ifdef CONFIG_ENABLE_UF2_USB_CONSOLE
    ESP_LOGI(TAG, "USB Unmounted");
    esp_tusb_deinit_console(TINYUSB_CDC_ACM_0);
    //todo: tusb_cdc_acm_deinit() is not implemented
#endif
    _uf2_state = TINYUF2_STATE_INSTALLED;
    ESP_LOGI(TAG, "USB Unmounted");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allows us to perform remote wakeup
// USB Specs: Within 7ms, device must draw an average current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    _uf2_state = TINYUF2_STATE_INSTALLED;
    ESP_LOGI(TAG, "USB Suspended");
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    _uf2_state = TINYUF2_STATE_MOUNTED;
    ESP_LOGI(TAG, "USB Resumed");
}

// USB Device Driver task
static void _usb_otg_phy_init(bool enable)
{
    static usb_phy_handle_t phy_hdl = NULL;
    usb_phy_config_t phy_conf = {
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .otg_speed = USB_PHY_SPEED_FULL
    };
    if (phy_hdl) {
        usb_del_phy(phy_hdl);
        phy_hdl = NULL;
    }
    if (enable) {
#if SOC_USB_SERIAL_JTAG_SUPPORTED
        // due to the default usb function is usb-serial-jtag
        // quick switch may cause the host not re-enumerate the device
        // pull down DP to make host treat it as disconnect event
        SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PAD_PULL_OVERRIDE);
        CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);
        SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLDOWN);
        CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLDOWN);
        vTaskDelay(pdMS_TO_TICKS(10));
#endif
        // Configure USB OTG PHY
        phy_conf.controller = USB_PHY_CTRL_OTG;
        usb_new_phy(&phy_conf, &phy_hdl);
    } else {
        // Restore the USB PHY to default state
        // Configure USB JTAG PHY
#if SOC_USB_SERIAL_JTAG_SUPPORTED
        phy_conf.controller = USB_PHY_CTRL_SERIAL_JTAG;
        usb_new_phy(&phy_conf, &phy_hdl);
#endif
    }
}

// This top level thread process all usb events and invoke callbacks
static void usb_device_task(void* param)
{
    (void) param;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // RTOS forever loop
    while (1) {
        // put this thread to waiting state until there is new events
        tud_task();
        uint32_t from_task = ulTaskNotifyTake(pdTRUE, 0);
        if (from_task) {
            //we get a notify back, it means the task is going to exit
            xTaskNotifyGive((TaskHandle_t)from_task);
            break;
        }
    }
    ESP_LOGI(TAG, "USB Device task exit");
    vTaskDelete(NULL);
}

// To generate a disconnect event
static void usbd_vbus_enable(bool enable)
{
    esp_rom_gpio_connect_in_signal(enable ? GPIO_MATRIX_CONST_ONE_INPUT : GPIO_MATRIX_CONST_ZERO_INPUT, USB_OTG_VBUSVALID_IN_IDX, 0);
    esp_rom_gpio_connect_in_signal(enable ? GPIO_MATRIX_CONST_ONE_INPUT : GPIO_MATRIX_CONST_ZERO_INPUT, USB_SRP_BVALID_IN_IDX, 0);
    esp_rom_gpio_connect_in_signal(enable ? GPIO_MATRIX_CONST_ONE_INPUT : GPIO_MATRIX_CONST_ZERO_INPUT, USB_SRP_SESSEND_IN_IDX, 1);
    return;
}

static esp_err_t tinyusb_init()
{
#ifdef CONFIG_PM_ENABLE
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "usb_otg", &pm_locks_cpu_max);
    esp_pm_lock_acquire(pm_locks_cpu_max);
#endif
    _usb_otg_phy_init(true);
    // init device stack on configured roothub port
    // This should be called after scheduler/kernel is started.
    // Otherwise it could cause kernel issue since USB IRQ handler does use RTOS queue API.
    tud_init(BOARD_TUD_RHPORT);
    xTaskCreatePinnedToCore(usb_device_task, "usbd", 4096, NULL, 5, &_task_handle, 0);
    return ESP_OK;
}

// bool tusb_teardown(void); function is not implemented in tinyusb,
// here we just deinit hardware, the memory not released
__attribute__((weak)) bool tusb_teardown(void)
{
    ESP_LOGW(TAG, "tusb_teardown not implemented in tinyusb, memory not released");
    return true;
}

static esp_err_t tinyusb_deinit()
{
    //prepare to exit the task
    xTaskNotify(_task_handle, (uint32_t)xTaskGetCurrentTaskHandle(), eSetValueWithOverwrite);
    //we give a false signal to make disconnect event happen
    usbd_vbus_enable(false);
    //wait for the task exit
    uint32_t notify = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000));
    if (!notify) {
        ESP_LOGW(TAG, "USB Device task exit timeout");
        vTaskDelete(_task_handle);
    }
    _task_handle = NULL;
    tusb_teardown();
    _usb_otg_phy_init(false);
#ifdef CONFIG_PM_ENABLE
    esp_pm_lock_release(pm_locks_cpu_max);
    esp_pm_lock_delete(pm_locks_cpu_max);
#endif
    return ESP_OK;
}

esp_err_t esp_tinyuf2_install(tinyuf2_ota_config_t *ota_config, tinyuf2_nvs_config_t *nvs_config)
{
    if (_uf2_state != TINYUF2_STATE_NOT_INSTALLED) {
        ESP_LOGE(TAG, "Tinyuf2 already installed");
        return ESP_ERR_INVALID_STATE;
    }

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

    _uf2_state = TINYUF2_STATE_INSTALLED;
    xTaskNotifyGive(_task_handle);
    ESP_LOGI(TAG, "UF2 Updater install succeed, Version: %d.%d.%d", ESP_TINYUF2_VER_MAJOR, ESP_TINYUF2_VER_MINOR, ESP_TINYUF2_VER_PATCH);

    return ESP_OK;
}

esp_err_t esp_tinyuf2_uninstall(void)
{
    if (_uf2_state == TINYUF2_STATE_NOT_INSTALLED) {
        ESP_LOGE(TAG, "Tinyuf2 not installed");
        return ESP_ERR_INVALID_STATE;
    }

#ifdef CONFIG_ENABLE_UF2_USB_CONSOLE
    if (_uf2_state == TINYUF2_STATE_MOUNTED) {
        esp_tusb_deinit_console(TINYUSB_CDC_ACM_0);
        //todo: tusb_cdc_acm_deinit() is not implemented
    }
#endif

    tinyusb_deinit();
    board_flash_deinit();
    board_flash_nvs_deinit();
    ESP_LOGI(TAG, "UF2 Updater uninstall succeed");
    _uf2_state = TINYUF2_STATE_NOT_INSTALLED;
    return ESP_OK;
}

tinyuf2_state_t esp_tinyuf2_current_state(void)
{
    return _uf2_state;
}
