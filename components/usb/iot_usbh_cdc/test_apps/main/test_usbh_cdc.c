/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "iot_usbh_cdc.h"
#include "esp_log.h"

#define IN_RINGBUF_SIZE (256 * 1)
#define OUT_RINGBUF_SIZE (256 * 1)
#define READ_TASK_KILL_BIT BIT1

#define TAG "cdc_test"
EventGroupHandle_t s_event_group_hdl = NULL;
#define TEST_ITF_NUM   2
#define TEST_EP_IN0  0x82
#define TEST_EP_OUT0 0x02
#define TEST_EP_IN1  0x84
#define TEST_EP_OUT1 0x04

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

static void usb_communication(uint8_t loop_count, size_t data_length, usbh_cdc_handle_t handle)
{
    uint8_t buff[512] = {0};
    for (int i = 0; i < data_length; i++) {
        buff[i] = i;
    }

    while (loop_count--) {
        size_t length = data_length;

        /*!< Send data */
        usbh_cdc_write_bytes(handle, buff, length, pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Send data len: %d", length);
        ESP_LOG_BUFFER_HEXDUMP(TAG, buff, length, ESP_LOG_INFO);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        /*!< Receive data */
        length = data_length;
        usbh_cdc_read_bytes(handle, buff, &length, pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Recv data len: %d", length);
        ESP_LOG_BUFFER_HEXDUMP(TAG, buff, length, ESP_LOG_INFO);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

static void cdc_new_dev_cb(usb_device_handle_t usb_dev, void *arg)
{
    ESP_LOGI(TAG, "cdc new device found");
}

static void cdc_connect_cb(usbh_cdc_handle_t handle, void *arg)
{
    usbh_cdc_desc_print(handle);
    ESP_LOGI(TAG, "cdc device connect");
}

static void cdc_disconnect_cb(usbh_cdc_handle_t handle, void *arg)
{
    ESP_LOGI(TAG, "cdc device disconnect");
}

TEST_CASE("usb cdc R/W", "[iot_usbh_cdc][read-write][auto]")
{
    esp_log_level_set("USBH_CDC", ESP_LOG_DEBUG);

    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
        .new_dev_cb = cdc_new_dev_cb,
    };

    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_install(&config));

    usbh_cdc_device_config_t dev_config = {
        .vid = 0,
        .pid = 0,
        .itf_num = 0,
        .rx_buffer_size = 0,
        .tx_buffer_size = 0,
        .cbs = {
            .connect = cdc_connect_cb,
            .disconnect = cdc_disconnect_cb,
            .user_data = NULL
        },
    };

    usbh_cdc_handle_t handle = NULL;
    usbh_cdc_create(&dev_config, &handle);

    // Add connect event
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    usb_communication(5, 128, handle);
    usb_communication(5, 64, handle);
    usb_communication(5, 32, handle);

    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_delete(handle));
    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_uninstall());
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}

TEST_CASE("usb dual cdc R/W", "[iot_usbh_cdc][read-write]")
{
    esp_log_level_set("USBH_CDC", ESP_LOG_DEBUG);

    // for (size_t i = 0; i < 5; i++) {
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
        .new_dev_cb = cdc_new_dev_cb,
    };

    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_install(&config));

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    usbh_cdc_device_config_t dev_config = {
        .vid = 0,
        .pid = 0,
        .itf_num = 1,
        .rx_buffer_size = 0,
        .tx_buffer_size = 0,
        .cbs = {
            .connect = cdc_connect_cb,
            .disconnect = cdc_disconnect_cb,
            .user_data = NULL
        },
    };

    usbh_cdc_handle_t handle = NULL;
    usbh_cdc_create(&dev_config, &handle);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    dev_config.itf_num = 3;

    usbh_cdc_handle_t handle2 = NULL;
    usbh_cdc_create(&dev_config, &handle2);

    // Add connect event
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    usb_communication(5, 128, handle);
    usb_communication(5, 128, handle2);
    usb_communication(5, 64, handle);
    usb_communication(5, 64, handle2);
    usb_communication(5, 32, handle);
    usb_communication(5, 32, handle2);

    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_delete(handle));
    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_delete(handle2));
    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_uninstall());
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}

TEST_CASE("usbh cdc driver memory leak", "[iot_usbh_cdc][read-write][auto]")
{
    esp_log_level_set("USBH_CDC", ESP_LOG_DEBUG);
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_install(&config));
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    usbh_cdc_driver_uninstall();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}

TEST_CASE("usbh cdc device memory leak", "[iot_usbh_cdc][read-write][auto]")
{
    esp_log_level_set("USBH_CDC", ESP_LOG_DEBUG);
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_install(&config));

    usbh_cdc_device_config_t dev_config = {
        .itf_num = 1,
        .rx_buffer_size = 0,
        .tx_buffer_size = 0,
    };

    usbh_cdc_handle_t handle = NULL;
    usbh_cdc_create(&dev_config, &handle);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_delete(handle));
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    usbh_cdc_driver_uninstall();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("IOT USBH CDC TEST \n");
    unity_run_menu();
}
