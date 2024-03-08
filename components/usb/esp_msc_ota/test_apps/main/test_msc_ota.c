/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "unity.h"
#include "esp_msc_host.h"
#include "esp_msc_ota.h"
#include "usb/usb_host.h"

static const char *TAG = "esp_msc_ota_test";

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
        ESP_LOGI(TAG, "ESP_MSC_OTA_WRITE_FLASH %.2f%%", progress * 100);
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

esp_msc_ota_handle_t handle;

TEST_CASE("SIMPLE MSC OTA", "[MSC OTA]")
{
    esp_event_loop_create_default();
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_MSC_OTA_EVENT, ESP_EVENT_ANY_ID, &msc_ota_event_handler, NULL));
    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG()
    };
    esp_msc_host_handle_t host_handle = NULL;
    TEST_ESP_OK(esp_msc_host_install(&msc_host_config, &host_handle));
    esp_msc_ota_config_t config = {
        .ota_bin_path = "/usb/ota_test.bin",
        .wait_msc_connect = pdMS_TO_TICKS(5000),
    };
    while (1) {
        if (esp_msc_ota(&config) == ESP_OK) {
            break;
        }
    }
    TEST_ESP_OK(esp_msc_host_uninstall(host_handle));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

TEST_CASE("Test memory leaks", "[memory leaks][MSC OTA]")
{
    // esp_event_loop_create_default();
    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG()
    };
    esp_msc_host_handle_t host_handle = NULL;
    TEST_ESP_OK(esp_msc_host_install(&msc_host_config, &host_handle));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    TEST_ESP_OK(esp_msc_host_uninstall(host_handle));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

/* Power Selector */
#define BOARD_IO_HOST_BOOST_EN 13
#define BOARD_IO_DEV_VBUS_EN 12
#define BOARD_IO_USB_SEL 18
#define BOARD_IO_IDEV_LIMIT_EN 17

#define BOARD_IO_PIN_SEL_OUTPUT ((1ULL<<BOARD_IO_HOST_BOOST_EN)\
                                |(1ULL<<BOARD_IO_DEV_VBUS_EN)\
                                |(1ULL<<BOARD_IO_USB_SEL)\
                                |(1ULL<<BOARD_IO_IDEV_LIMIT_EN))

typedef enum {
    USB_DEVICE_MODE,
    USB_HOST_MODE,
} usb_mode_t;

esp_err_t iot_board_usb_device_set_power(bool on_off, bool from_battery)
{
    if (from_battery) {
        gpio_set_level(BOARD_IO_HOST_BOOST_EN, on_off); //BOOST_EN
        gpio_set_level(BOARD_IO_DEV_VBUS_EN, !on_off);//DEV_VBUS_EN
        gpio_set_level(BOARD_IO_IDEV_LIMIT_EN, on_off);//DEV_VBUS_EN
    } else {
        gpio_set_level(BOARD_IO_HOST_BOOST_EN, !on_off); //BOOST_EN
        gpio_set_level(BOARD_IO_DEV_VBUS_EN, on_off);//DEV_VBUS_EN
        gpio_set_level(BOARD_IO_IDEV_LIMIT_EN, on_off);//DEV_VBUS_EN
    }
    return ESP_OK;
}

esp_err_t iot_board_usb_set_mode(usb_mode_t mode)
{
    switch (mode) {
    case USB_DEVICE_MODE:
        gpio_set_level(BOARD_IO_USB_SEL, false); //USB_SEL
        break;
    case USB_HOST_MODE:
        gpio_set_level(BOARD_IO_USB_SEL, true); //USB_SEL
        break;
    default:
        assert(0);
        break;
    }
    return ESP_OK;
}

usb_mode_t iot_board_usb_get_mode(void)
{
    int value = gpio_get_level(BOARD_IO_USB_SEL);
    if (value) {
        return USB_HOST_MODE;
    }
    return USB_DEVICE_MODE;
}

static esp_err_t board_gpio_init(void)
{
    esp_err_t ret;
    gpio_config_t io_conf = {0};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = BOARD_IO_PIN_SEL_OUTPUT;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    ret = gpio_config(&io_conf);
    return ret;
}

TEST_CASE("Auto MSC OTA", "[MSC OTA][Auto]")
{
    esp_event_loop_create_default();

    TEST_ASSERT_EQUAL(ESP_OK, board_gpio_init());
    TEST_ASSERT_EQUAL(ESP_OK, iot_board_usb_set_mode(USB_HOST_MODE));
    TEST_ASSERT_EQUAL(ESP_OK, iot_board_usb_device_set_power(true, false));

    ESP_ERROR_CHECK(esp_event_handler_register(ESP_MSC_OTA_EVENT, ESP_EVENT_ANY_ID, &msc_ota_event_handler, NULL));
    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG()
    };
    esp_msc_host_handle_t host_handle = NULL;
    TEST_ESP_OK(esp_msc_host_install(&msc_host_config, &host_handle));

    esp_msc_ota_handle_t msc_ota_handle = NULL;
    esp_msc_ota_config_t config = {
        .ota_bin_path = "/usb/ota_test.bin",
        .wait_msc_connect = pdMS_TO_TICKS(5000),
    };
    TEST_ESP_OK(esp_msc_ota_begin(&config, &msc_ota_handle));
    TEST_ESP_OK(esp_msc_ota_abort(msc_ota_handle));

    TEST_ASSERT_EQUAL(ESP_OK, iot_board_usb_set_mode(USB_DEVICE_MODE));
    TEST_ASSERT_EQUAL(ESP_OK, iot_board_usb_device_set_power(false, false));

    TEST_ESP_OK(esp_msc_host_uninstall(host_handle));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
