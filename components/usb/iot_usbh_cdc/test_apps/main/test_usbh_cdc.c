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
#include "driver/gpio.h"
#include "iot_usbh_cdc.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_timer.h"

#define TAG "cdc_test"

extern int force_link;
ssize_t TEST_MEMORY_LEAK_THRESHOLD = 0;

#define UPDATE_LEAK_THRESHOLD(first_val) \
static bool is_first = true; \
if (is_first) { \
    TEST_MEMORY_LEAK_THRESHOLD = first_val; \
} else { \
    TEST_MEMORY_LEAK_THRESHOLD = 0; \
}

#define EVENT_CONNECT BIT0
#define EVENT_CONNECT2 BIT1
#define EVENT_DISCONNECT BIT2

// Global variables for testing
static usbh_cdc_port_handle_t test_port1 = NULL;
static usbh_cdc_port_handle_t test_port2 = NULL;
static EventGroupHandle_t test_event_group = NULL;
static int device_connect_count = 0;

// Default port configuration macro
#define USBH_CDC_PORT_DEFAULT_CONFIG(_dev_addr, _itf_num, _notif_cb, _recv_data_cb, _closed_cd) \
    { \
        .dev_addr = _dev_addr, \
        .itf_num = _itf_num, \
        .in_ringbuf_size = 0, \
        .out_ringbuf_size = 0, \
        .in_transfer_buffer_size = 512, \
        .out_transfer_buffer_size = 512, \
        .cbs = { \
            .closed = _closed_cd, \
            .notif_cb = _notif_cb, \
            .recv_data = _recv_data_cb, \
            .user_data = NULL, \
        }, \
        .flags = 0, \
    }

// Port configuration macro with Ringbuffer
#define USBH_CDC_PORT_RINGBUF_CONFIG(_dev_addr, _itf_num, _notif_cb, _recv_data_cb, _closed_cd) \
    { \
        .dev_addr = _dev_addr, \
        .itf_num = _itf_num, \
        .in_ringbuf_size = 1024, \
        .out_ringbuf_size = 1024, \
        .in_transfer_buffer_size = 512, \
        .out_transfer_buffer_size = 512, \
        .cbs = { \
            .closed = _closed_cd, \
            .notif_cb = _notif_cb, \
            .recv_data = _recv_data_cb, \
            .user_data = NULL, \
        }, \
        .flags = 0, \
    }

// Callback functions
static void cdc_notif_cb(usbh_cdc_port_handle_t port_handle, iot_cdc_notification_t *notif, void *user_data)
{
    ESP_LOGI(TAG, "CDC notification received on port %p", port_handle);
}

static void cdc_recv_data_cb(usbh_cdc_port_handle_t port_handle, void *user_data)
{
    size_t rx_length = 0;
    uint8_t buf[512] = {0};

    esp_err_t ret = usbh_cdc_get_rx_buffer_size(port_handle, &rx_length);
    if (ret == ESP_OK && rx_length > 0) {
        if (rx_length > sizeof(buf)) {
            rx_length = sizeof(buf);
        }
        usbh_cdc_read_bytes(port_handle, buf, &rx_length, 0);
        ESP_LOGI(TAG, "USB received data len: %u", rx_length);
    }
}

static void cdc_port_closed_cb(usbh_cdc_port_handle_t port_handle, void *user_data)
{
    ESP_LOGI(TAG, "CDC port closed callback: %p", port_handle);
}

// Driver event callback
static void driver_event_cb(usbh_cdc_device_event_t event, usbh_cdc_device_event_data_t *event_data, void *user_ctx)
{
    switch (event) {
    case CDC_HOST_DEVICE_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Device connected: dev_addr=%d, matched_intf_num=%d",
                 event_data->new_dev.dev_addr, event_data->new_dev.matched_intf_num);
        device_connect_count++;
        if (test_event_group) {
            xEventGroupSetBits(test_event_group, EVENT_CONNECT);
            xEventGroupClearBits(test_event_group, EVENT_DISCONNECT);
        }
        break;

    case CDC_HOST_DEVICE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Device disconnected: dev_addr=%d, dev_hdl=%p",
                 event_data->dev_gone.dev_addr, event_data->dev_gone.dev_hdl);
        if (test_event_group) {
            xEventGroupSetBits(test_event_group, EVENT_DISCONNECT);
            xEventGroupClearBits(test_event_group, EVENT_CONNECT);
        }
        break;

    default:
        break;
    }
}

// Helper function: install driver
static esp_err_t install_driver(usbh_cdc_device_event_callback_t event_cb, void *user_ctx)
{
    const static usb_device_match_id_t match_id_list[] = {
        {
            .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT,
            .idVendor = USB_DEVICE_VENDOR_ANY,
            .idProduct = USB_DEVICE_PRODUCT_ANY,
        },
        { 0 }
    };

    usbh_cdc_driver_config_t config = {
        .task_stack_size = 4096,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };

    esp_err_t ret = usbh_cdc_driver_install(&config);
    if (ret != ESP_OK) {
        return ret;
    }
    return usbh_cdc_register_dev_event_cb(match_id_list, event_cb, user_ctx);
}

// Helper function: uninstall driver
static esp_err_t uninstall_driver(void)
{
    esp_err_t ret = usbh_cdc_driver_uninstall();
    vTaskDelay(pdMS_TO_TICKS(1000));
    return ret;
}

// ==================== Test Cases: Driver Install/Uninstall ====================

TEST_CASE("usbh_cdc_driver_install_null_config", "[iot_usbh_cdc][auto][install][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);
    esp_err_t ret = usbh_cdc_driver_install(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

TEST_CASE("usbh_cdc_driver_install_uninstall_basic", "[iot_usbh_cdc][auto][install-uninstall]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    // Normal installation
    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Normal uninstallation
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_driver_install_duplicate", "[iot_usbh_cdc][auto][install][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    // First installation should succeed
    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Second installation should fail (already installed)
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 4096,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    esp_err_t ret = usbh_cdc_driver_install(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    // Cleanup
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_driver_uninstall_not_installed", "[iot_usbh_cdc][auto][uninstall][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    // Uninstallation should fail when driver is not installed
    esp_err_t ret = usbh_cdc_driver_uninstall();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

TEST_CASE("usbh_cdc_register_unregister_dev_event_cb", "[iot_usbh_cdc][auto][install][register]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    // Install driver (not using helper function, for independent verification of register/unregister)
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 4096,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_install(&config));

    const static usb_device_match_id_t match_id_list[] = {
        {
            .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT,
            .idVendor = USB_DEVICE_VENDOR_ANY,
            .idProduct = USB_DEVICE_PRODUCT_ANY,
        },
        { 0 }
    };

    // Boundary: NULL callback should return invalid argument
    esp_err_t ret = usbh_cdc_register_dev_event_cb(match_id_list, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    // Normal registration
    ret = usbh_cdc_register_dev_event_cb(match_id_list, driver_event_cb, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Boundary: NULL unregister parameter
    ret = usbh_cdc_unregister_dev_event_cb(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    // Unregister unregistered callback
    ret = usbh_cdc_unregister_dev_event_cb((usbh_cdc_device_event_callback_t)0xdeadbeef);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ret);

    // Normal unregistration
    ret = usbh_cdc_unregister_dev_event_cb(driver_event_cb);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Uninstall driver
    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_uninstall());
    vTaskDelay(pdMS_TO_TICKS(1500));
}

// ==================== Test Cases: Port Open/Close Boundary Conditions ====================

TEST_CASE("usbh_cdc_port_open_null_args", "[iot_usbh_cdc][auto][port][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    usbh_cdc_port_config_t config = USBH_CDC_PORT_DEFAULT_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);

    // Test NULL port_config
    esp_err_t ret = usbh_cdc_port_open(NULL, &test_port1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    // Test NULL cdc_port_out
    ret = usbh_cdc_port_open(&config, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    // Test both parameters are NULL
    ret = usbh_cdc_port_open(NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_port_open_zero_buffer_size", "[iot_usbh_cdc][auto][port][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    usbh_cdc_port_config_t config = USBH_CDC_PORT_DEFAULT_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);

    // Test in_transfer_buffer_size is 0
    config.in_transfer_buffer_size = 0;
    esp_err_t ret = usbh_cdc_port_open(&config, &test_port1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    // Test out_transfer_buffer_size is 0
    config.in_transfer_buffer_size = 512;
    config.out_transfer_buffer_size = 0;
    ret = usbh_cdc_port_open(&config, &test_port1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_port_open_driver_not_installed", "[iot_usbh_cdc][auto][port][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    // Try to open port without installing driver
    usbh_cdc_port_config_t config = USBH_CDC_PORT_DEFAULT_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);
    esp_err_t ret = usbh_cdc_port_open(&config, &test_port1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

TEST_CASE("usbh_cdc_port_close_null_handle", "[iot_usbh_cdc][auto][port][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Test NULL handle
    esp_err_t ret = usbh_cdc_port_close(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_port_close_invalid_handle", "[iot_usbh_cdc][auto][port][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Create an invalid handle pointer
    usbh_cdc_port_handle_t invalid_handle = (usbh_cdc_port_handle_t)0xDEADBEEF;
    esp_err_t ret = usbh_cdc_port_close(invalid_handle);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_port_close_driver_not_installed", "[iot_usbh_cdc][auto][port][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    // Try to close port without installing driver
    usbh_cdc_port_handle_t fake_handle = (usbh_cdc_port_handle_t)0x12345678;
    esp_err_t ret = usbh_cdc_port_close(fake_handle);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

// ==================== Test Cases: Read/Write Operation Boundary Conditions ====================

TEST_CASE("usbh_cdc_write_bytes_null_args", "[iot_usbh_cdc][auto][write][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));

    // Wait for device connection
    test_event_group = xEventGroupCreate();
    EventBits_t bits = xEventGroupWaitBits(test_event_group, EVENT_CONNECT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & EVENT_CONNECT) {
        usbh_cdc_port_config_t config = USBH_CDC_PORT_DEFAULT_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);
        if (usbh_cdc_port_open(&config, &test_port1) == ESP_OK) {
            uint8_t data[64] = {0};
            size_t length = sizeof(data);

            // Test NULL handle
            esp_err_t ret = usbh_cdc_write_bytes(NULL, data, length, pdMS_TO_TICKS(1000));
            TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

            // Test NULL buffer
            ret = usbh_cdc_write_bytes(test_port1, NULL, length, pdMS_TO_TICKS(1000));
            TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

            // Test zero length
            length = 0;
            ret = usbh_cdc_write_bytes(test_port1, data, length, pdMS_TO_TICKS(1000));
            TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

            usbh_cdc_port_close(test_port1);
        }
    }

    vEventGroupDelete(test_event_group);
    test_event_group = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_read_bytes_null_args", "[iot_usbh_cdc][auto][read][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));

    test_event_group = xEventGroupCreate();
    EventBits_t bits = xEventGroupWaitBits(test_event_group, EVENT_CONNECT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & EVENT_CONNECT) {
        usbh_cdc_port_config_t config = USBH_CDC_PORT_DEFAULT_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);
        if (usbh_cdc_port_open(&config, &test_port1) == ESP_OK) {
            uint8_t data[64] = {0};
            size_t length = sizeof(data);

            // Test NULL handle
            esp_err_t ret = usbh_cdc_read_bytes(NULL, data, &length, pdMS_TO_TICKS(1000));
            TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

            // Test NULL buffer
            ret = usbh_cdc_read_bytes(test_port1, NULL, &length, pdMS_TO_TICKS(1000));
            TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

            // Test NULL length pointer
            ret = usbh_cdc_read_bytes(test_port1, data, NULL, pdMS_TO_TICKS(1000));
            TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

            usbh_cdc_port_close(test_port1);
        }
    }

    vEventGroupDelete(test_event_group);
    test_event_group = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_read_write_invalid_handle", "[iot_usbh_cdc][auto][read-write][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    uint8_t data[64] = {0};
    size_t length = sizeof(data);
    usbh_cdc_port_handle_t invalid_handle = (usbh_cdc_port_handle_t)0xDEADBEEF;

    // Test write operation with invalid handle
    esp_err_t ret = usbh_cdc_write_bytes(invalid_handle, data, length, pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    // Test read operation with invalid handle
    ret = usbh_cdc_read_bytes(invalid_handle, data, &length, pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

// ==================== Test Cases: Buffer Operations ====================

TEST_CASE("usbh_cdc_get_rx_buffer_size_null_args", "[iot_usbh_cdc][auto][buffer][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    size_t size = 0;

    // Test NULL handle
    esp_err_t ret = usbh_cdc_get_rx_buffer_size(NULL, &size);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    // Test NULL size pointer
    usbh_cdc_port_handle_t fake_handle = (usbh_cdc_port_handle_t)0x12345678;
    ret = usbh_cdc_get_rx_buffer_size(fake_handle, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_flush_buffers_null_handle", "[iot_usbh_cdc][auto][buffer][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Test flush operation with NULL handle
    esp_err_t ret = usbh_cdc_flush_rx_buffer(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = usbh_cdc_flush_tx_buffer(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

// ==================== Test Cases: Descriptor and Handle Operations ====================

TEST_CASE("usbh_cdc_get_dev_handle_null_args", "[iot_usbh_cdc][auto][descriptor][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    usb_device_handle_t dev_handle = NULL;

    // Test NULL port handle
    esp_err_t ret = usbh_cdc_get_dev_handle(NULL, &dev_handle);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    // Test NULL dev_handle pointer
    usbh_cdc_port_handle_t fake_handle = (usbh_cdc_port_handle_t)0x12345678;
    ret = usbh_cdc_get_dev_handle(fake_handle, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_desc_print_null_handle", "[iot_usbh_cdc][auto][descriptor][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    // Test NULL handle
    esp_err_t ret = usbh_cdc_desc_print(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_port_get_intf_desc_null_args", "[iot_usbh_cdc][auto][descriptor][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    const usb_intf_desc_t *notif_intf = NULL;
    const usb_intf_desc_t *data_intf = NULL;

    // Test NULL port handle
    esp_err_t ret = usbh_cdc_port_get_intf_desc(NULL, &notif_intf, &data_intf);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    // Test NULL pointer parameters (if allowed)
    usbh_cdc_port_handle_t fake_handle = (usbh_cdc_port_handle_t)0x12345678;
    ret = usbh_cdc_port_get_intf_desc(fake_handle, NULL, NULL);
    // This may be allowed, depending on implementation
    ESP_LOGI(TAG, "get_intf_desc with NULL pointers returned: %d", ret);

    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

// ==================== Test Cases: Custom Requests ====================

TEST_CASE("usbh_cdc_send_custom_request_null_args", "[iot_usbh_cdc][auto][request][boundary]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    uint8_t data[64] = {0};

    // Test NULL handle
    esp_err_t ret = usbh_cdc_send_custom_request(NULL, 0, 0, 0, 0, 64, data);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    // Test NULL data buffer
    usbh_cdc_port_handle_t fake_handle = (usbh_cdc_port_handle_t)0x12345678;
    ret = usbh_cdc_send_custom_request(fake_handle, 0, 0, 0, 0, 64, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

// ==================== Test Cases: Normal Functional Tests ====================

TEST_CASE("usbh_cdc_port_open_close_valid_device", "[iot_usbh_cdc][auto][port][functional]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    test_event_group = xEventGroupCreate();
    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));

    EventBits_t bits = xEventGroupWaitBits(test_event_group, EVENT_CONNECT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & EVENT_CONNECT) {
        usbh_cdc_port_config_t config = USBH_CDC_PORT_DEFAULT_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);

        // Open port
        esp_err_t ret = usbh_cdc_port_open(&config, &test_port1);
        if (ret == ESP_OK) {
            TEST_ASSERT_NOT_NULL(test_port1);

            // Attempting to open the same port again should fail
            ret = usbh_cdc_port_open(&config, &test_port1);
            TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

            // Normal close
            ret = usbh_cdc_port_close(test_port1);
            TEST_ASSERT_EQUAL(ESP_OK, ret);

            // Closing again should fail
            ret = usbh_cdc_port_close(test_port1);
            TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
        }
    }

    vEventGroupDelete(test_event_group);
    test_event_group = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_read_write_with_ringbuffer", "[iot_usbh_cdc][auto][read-write][functional]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    test_event_group = xEventGroupCreate();
    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));

    EventBits_t bits = xEventGroupWaitBits(test_event_group, EVENT_CONNECT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & EVENT_CONNECT) {
        // Use ringbuffer configuration
        usbh_cdc_port_config_t config = USBH_CDC_PORT_RINGBUF_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);

        if (usbh_cdc_port_open(&config, &test_port1) == ESP_OK) {
            uint8_t tx_data[128] = {0};
            uint8_t rx_data[128] = {0};
            size_t tx_len = sizeof(tx_data);
            size_t rx_len = sizeof(rx_data);

            // Fill test data
            for (int i = 0; i < tx_len; i++) {
                tx_data[i] = i & 0xFF;
            }

            // Write data
            esp_err_t ret = usbh_cdc_write_bytes(test_port1, tx_data, tx_len, pdMS_TO_TICKS(1000));
            ESP_LOGI(TAG, "Write returned: %d", ret);

            vTaskDelay(pdMS_TO_TICKS(500));

            // Get receive buffer size
            size_t rx_buf_size = 0;
            ret = usbh_cdc_get_rx_buffer_size(test_port1, &rx_buf_size);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "RX buffer size: %zu", rx_buf_size);
            }

            // Try to read data
            if (rx_buf_size > 0) {
                rx_len = (rx_buf_size < sizeof(rx_data)) ? rx_buf_size : sizeof(rx_data);
                ret = usbh_cdc_read_bytes(test_port1, rx_data, &rx_len, pdMS_TO_TICKS(1000));
                ESP_LOGI(TAG, "Read returned: %d, length: %zu", ret, rx_len);
            }

            // Test flush operations (only supported with ringbuffer)
            ret = usbh_cdc_flush_rx_buffer(test_port1);
            ESP_LOGI(TAG, "Flush RX returned: %d", ret);

            ret = usbh_cdc_flush_tx_buffer(test_port1);
            ESP_LOGI(TAG, "Flush TX returned: %d", ret);

            usbh_cdc_port_close(test_port1);
        }
    }

    vEventGroupDelete(test_event_group);
    test_event_group = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_read_write_without_ringbuffer", "[iot_usbh_cdc][auto][read-write][functional]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    test_event_group = xEventGroupCreate();
    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));

    EventBits_t bits = xEventGroupWaitBits(test_event_group, EVENT_CONNECT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & EVENT_CONNECT) {
        // Use configuration without ringbuffer
        usbh_cdc_port_config_t config = USBH_CDC_PORT_DEFAULT_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);

        if (usbh_cdc_port_open(&config, &test_port1) == ESP_OK) {
            uint8_t tx_data[64] = "AT+GMR\r\n";
            size_t tx_len = strlen((char *)tx_data);

            // Write data
            esp_err_t ret = usbh_cdc_write_bytes(test_port1, tx_data, tx_len, pdMS_TO_TICKS(1000));
            ESP_LOGI(TAG, "Write returned: %d", ret);

            vTaskDelay(pdMS_TO_TICKS(1000));

            // In no ringbuffer mode, flush should return not supported
            ret = usbh_cdc_flush_rx_buffer(test_port1);
            if (ret == ESP_ERR_NOT_SUPPORTED) {
                ESP_LOGI(TAG, "Flush RX not supported (expected for no ringbuffer mode)");
            }

            ret = usbh_cdc_flush_tx_buffer(test_port1);
            if (ret == ESP_ERR_NOT_SUPPORTED) {
                ESP_LOGI(TAG, "Flush TX not supported (expected for no ringbuffer mode)");
            }

            usbh_cdc_port_close(test_port1);
        }
    }

    vEventGroupDelete(test_event_group);
    test_event_group = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_write_speed_test", "[iot_usbh_cdc][write][speed]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    test_event_group = xEventGroupCreate();
    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));

    EventBits_t bits = xEventGroupWaitBits(test_event_group, EVENT_CONNECT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & EVENT_CONNECT) {
        usbh_cdc_port_config_t config = USBH_CDC_PORT_DEFAULT_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);

        if (usbh_cdc_port_open(&config, &test_port1) == ESP_OK) {
            const size_t data_size = 1024;
            const size_t total_bytes = 1000 * 1024; // 1000KB
            uint8_t *tx_data = (uint8_t *)malloc(data_size);
            TEST_ASSERT_NOT_NULL(tx_data);

            for (size_t i = 0; i < data_size; i++) {
                tx_data[i] = (uint8_t)(i & 0xFF);
            }

            int64_t start_us = esp_timer_get_time();
            size_t bytes_written = 0;

            for (size_t i = 0; i < total_bytes / data_size; i++) {
                size_t write_len = data_size;
                if (usbh_cdc_write_bytes(test_port1, tx_data, write_len, pdMS_TO_TICKS(5000)) == ESP_OK) {
                    bytes_written += write_len;
                } else {
                    break;
                }
            }

            int64_t elapsed_us = esp_timer_get_time() - start_us;
            float speed_kbps = (bytes_written * 1000000.0f) / (elapsed_us * 1024.0f);

            ESP_LOGI(TAG, "Write: %zu bytes, %.2f ms, %.2f KB/s",
                     bytes_written, elapsed_us / 1000.0f, speed_kbps);

            free(tx_data);
            usbh_cdc_port_close(test_port1);
        }
    }

    vEventGroupDelete(test_event_group);
    test_event_group = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_read_speed_test", "[iot_usbh_cdc][read][speed]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    test_event_group = xEventGroupCreate();
    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));

    EventBits_t bits = xEventGroupWaitBits(test_event_group, EVENT_CONNECT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & EVENT_CONNECT) {
        usbh_cdc_port_config_t config = USBH_CDC_PORT_RINGBUF_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);

        if (usbh_cdc_port_open(&config, &test_port1) == ESP_OK) {
            const size_t read_buffer_size = 1024;
            const size_t target_bytes = 50 * 1024; // 50KB
            uint8_t *rx_data = (uint8_t *)malloc(read_buffer_size);
            TEST_ASSERT_NOT_NULL(rx_data);

            vTaskDelay(pdMS_TO_TICKS(1000));
            int64_t start_us = esp_timer_get_time();
            int64_t timeout_us = start_us + 30000000; // 30 seconds
            size_t bytes_read = 0;

            while (bytes_read < target_bytes && esp_timer_get_time() < timeout_us) {
                size_t read_len = read_buffer_size;
                esp_err_t ret = usbh_cdc_read_bytes(test_port1, rx_data, &read_len, pdMS_TO_TICKS(1000));

                if (ret == ESP_OK && read_len > 0) {
                    bytes_read += read_len;
                } else if (ret == ESP_ERR_TIMEOUT) {
                    size_t rx_buf_size = 0;
                    if (usbh_cdc_get_rx_buffer_size(test_port1, &rx_buf_size) == ESP_OK && rx_buf_size == 0) {
                        break;
                    }
                } else {
                    break;
                }
            }

            int64_t elapsed_us = esp_timer_get_time() - start_us;
            float speed_kbps = (bytes_read * 1000000.0f) / (elapsed_us * 1024.0f);

            ESP_LOGI(TAG, "Read: %zu bytes, %.2f ms, %.2f KB/s",
                     bytes_read, elapsed_us / 1000.0f, speed_kbps);

            free(rx_data);
            usbh_cdc_port_close(test_port1);
        }
    }

    vEventGroupDelete(test_event_group);
    test_event_group = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_descriptor_operations", "[iot_usbh_cdc][auto][descriptor][functional]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    test_event_group = xEventGroupCreate();
    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));

    EventBits_t bits = xEventGroupWaitBits(test_event_group, EVENT_CONNECT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & EVENT_CONNECT) {
        usbh_cdc_port_config_t config = USBH_CDC_PORT_DEFAULT_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);

        if (usbh_cdc_port_open(&config, &test_port1) == ESP_OK) {
            // Print descriptor
            esp_err_t ret = usbh_cdc_desc_print(test_port1);
            ESP_LOGI(TAG, "desc_print returned: %d", ret);

            // Get device handle
            usb_device_handle_t dev_handle = NULL;
            ret = usbh_cdc_get_dev_handle(test_port1, &dev_handle);
            if (ret == ESP_OK) {
                TEST_ASSERT_NOT_NULL(dev_handle);
                ESP_LOGI(TAG, "Device handle: %p", dev_handle);
            }

            // Get interface descriptor
            const usb_intf_desc_t *notif_intf = NULL;
            const usb_intf_desc_t *data_intf = NULL;
            ret = usbh_cdc_port_get_intf_desc(test_port1, &notif_intf, &data_intf);
            if (ret == ESP_OK) {
                if (notif_intf) {
                    ESP_LOGI(TAG, "Notification interface: %d", notif_intf->bInterfaceNumber);
                }
                if (data_intf) {
                    ESP_LOGI(TAG, "Data interface: %d", data_intf->bInterfaceNumber);
                }
            }

            usbh_cdc_port_close(test_port1);
        }
    }

    vEventGroupDelete(test_event_group);
    test_event_group = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_custom_request", "[iot_usbh_cdc][auto][request][functional]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    test_event_group = xEventGroupCreate();
    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));

    EventBits_t bits = xEventGroupWaitBits(test_event_group, EVENT_CONNECT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & EVENT_CONNECT) {
        usbh_cdc_port_config_t config = USBH_CDC_PORT_DEFAULT_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);

        if (usbh_cdc_port_open(&config, &test_port1) == ESP_OK) {
            // Send GET_DESCRIPTOR request
            uint8_t bmRequestType = USB_BM_REQUEST_TYPE_DIR_IN | USB_BM_REQUEST_TYPE_TYPE_STANDARD | USB_BM_REQUEST_TYPE_RECIP_DEVICE;
            uint8_t bRequest = USB_B_REQUEST_GET_DESCRIPTOR;
            uint16_t wValue = (USB_W_VALUE_DT_STRING << 8) | (1 & 0xFF);
            uint16_t wIndex = 0x0409; // English
            uint8_t str[64] = {0};
            uint16_t wLength = 64;

            esp_err_t ret = usbh_cdc_send_custom_request(test_port1, bmRequestType, bRequest, wValue, wIndex, wLength, str);
            ESP_LOGI(TAG, "Custom request returned: %d", ret);
            if (ret == ESP_OK) {
                ESP_LOG_BUFFER_HEXDUMP(TAG, str, wLength, ESP_LOG_INFO);
            }

            usbh_cdc_port_close(test_port1);
        }
    }

    vEventGroupDelete(test_event_group);
    test_event_group = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_flags_disable_notification", "[iot_usbh_cdc][auto][flags][functional]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    test_event_group = xEventGroupCreate();
    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));

    EventBits_t bits = xEventGroupWaitBits(test_event_group, EVENT_CONNECT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & EVENT_CONNECT) {
        usbh_cdc_port_config_t config = USBH_CDC_PORT_DEFAULT_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);
        // Set disable notification flag
        config.flags |= USBH_CDC_FLAGS_DISABLE_NOTIFICATION;

        if (usbh_cdc_port_open(&config, &test_port1) == ESP_OK) {
            ESP_LOGI(TAG, "Port opened with DISABLE_NOTIFICATION flag");

            // Perform some basic operations
            uint8_t data[32] = "TEST";
            size_t len = strlen((char *)data);
            usbh_cdc_write_bytes(test_port1, data, len, pdMS_TO_TICKS(1000));

            vTaskDelay(pdMS_TO_TICKS(500));

            usbh_cdc_port_close(test_port1);
        }
    }

    vEventGroupDelete(test_event_group);
    test_event_group = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

TEST_CASE("usbh_cdc_multiple_ports", "[iot_usbh_cdc][auto][multi-port][functional]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    test_event_group = xEventGroupCreate();
    TEST_ASSERT_EQUAL(ESP_OK, install_driver(driver_event_cb, NULL));

    EventBits_t bits = xEventGroupWaitBits(test_event_group, EVENT_CONNECT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & EVENT_CONNECT) {
        // Open first port
        usbh_cdc_port_config_t config1 = USBH_CDC_PORT_DEFAULT_CONFIG(1, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);
        esp_err_t ret1 = usbh_cdc_port_open(&config1, &test_port1);

        // Open second port (if device supports multiple interfaces)
        usbh_cdc_port_config_t config2 = USBH_CDC_PORT_DEFAULT_CONFIG(1, 3, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);
        esp_err_t ret2 = usbh_cdc_port_open(&config2, &test_port2);

        if (ret1 == ESP_OK) {
            ESP_LOGI(TAG, "Port 1 opened successfully");
            if (ret2 == ESP_OK) {
                ESP_LOGI(TAG, "Port 2 opened successfully");

                // Operate both ports simultaneously
                uint8_t data1[32] = "PORT1";
                uint8_t data2[32] = "PORT2";
                usbh_cdc_write_bytes(test_port1, data1, strlen((char *)data1), pdMS_TO_TICKS(1000));
                usbh_cdc_write_bytes(test_port2, data2, strlen((char *)data2), pdMS_TO_TICKS(1000));

                vTaskDelay(pdMS_TO_TICKS(500));

                usbh_cdc_port_close(test_port2);
            }
            usbh_cdc_port_close(test_port1);
        }
    }

    vEventGroupDelete(test_event_group);
    test_event_group = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

static uint32_t random_range(uint32_t min, uint32_t max)
{
    if (min > max) {
        return min;
    }

    uint32_t range = max - min + 1;
    uint32_t limit = UINT32_MAX - (UINT32_MAX % range);

    uint32_t r;
    do {
        r = esp_random();
    } while (r >= limit);

    return min + (r % range);
}

static void _write_read_task(void *arg)
{
    usbh_cdc_port_handle_t port = (usbh_cdc_port_handle_t)arg;
    uint8_t tx_data[512] = "AT+GMR\r\n"; // Example data
    size_t tx_len = sizeof(tx_data);

    while (1) {

        // Write data
        esp_err_t ret = usbh_cdc_write_bytes(port, tx_data, tx_len, pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Task Write returned: %s", esp_err_to_name(ret));

        vTaskDelay(pdMS_TO_TICKS(random_range(1, 100)));

        EventBits_t bits = xEventGroupGetBits(test_event_group);
        if (!(bits & EVENT_CONNECT)) {
            break;
        }
    }
    ESP_LOGI(TAG, "Write/Read task exiting for port %p", port);
    vTaskDelete(NULL);
}

static void _random_poweroff_task(void *arg)
{
    const int POWER_GPIO = 47;
    gpio_config_t _io_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT64(POWER_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&_io_cfg);
    while (1) {
        ESP_LOGW(TAG, "Simulating power on");
        gpio_set_level(POWER_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(random_range(2000, 9000)));
        ESP_LOGW(TAG, "Simulating power off");
        gpio_set_level(POWER_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(2500));
    }
    vTaskDelete(NULL);
}

static void hot_plugging_driver_event_cb(usbh_cdc_device_event_t event, usbh_cdc_device_event_data_t *event_data, void *user_ctx)
{
    switch (event) {
    case CDC_HOST_DEVICE_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Device connected: dev_addr=%d, matched_intf_num=%d",
                 event_data->new_dev.dev_addr, event_data->new_dev.matched_intf_num);
        device_connect_count++;
        if (test_event_group) {
            xEventGroupSetBits(test_event_group, EVENT_CONNECT);
        }
        // Open first port
        usbh_cdc_port_config_t config1 = USBH_CDC_PORT_DEFAULT_CONFIG(event_data->new_dev.dev_addr, 0, cdc_notif_cb, cdc_recv_data_cb, cdc_port_closed_cb);
        config1.flags |= USBH_CDC_FLAGS_DISABLE_NOTIFICATION;
        esp_err_t ret1 = usbh_cdc_port_open(&config1, &test_port1);
        if (ret1 == ESP_OK) {
            ESP_LOGI(TAG, "Port 1 opened successfully");
            xTaskCreate(_write_read_task, "WriteReadTask1", 4096, test_port1, 2, NULL);
        }

        // Open second port (if device supports multiple interfaces)
        // usbh_cdc_port_config_t config2 = USBH_CDC_PORT_DEFAULT_CONFIG(event_data->new_dev.dev_addr, 0, cdc_notif_cb, NULL);
        // esp_err_t ret2 = usbh_cdc_port_open(&config2, &test_port2);
        // if (ret2 == ESP_OK) {
        //     ESP_LOGI(TAG, "Port 2 opened successfully");
        //     xTaskCreate(_write_read_task, "WriteReadTask2", 4096, test_port2, 5, NULL);
        // }
        break;

    case CDC_HOST_DEVICE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Device disconnected: dev_addr=%d, dev_hdl=%p",
                 event_data->dev_gone.dev_addr, event_data->dev_gone.dev_hdl);
        if (test_event_group) {
            xEventGroupClearBits(test_event_group, EVENT_CONNECT);
        }
        break;

    default:
        break;
    }
}

TEST_CASE("usbh_cdc_multiple_ports", "[iot_usbh_cdc][hot-plugging][functional]")
{
    UPDATE_LEAK_THRESHOLD(-40);

    test_event_group = xEventGroupCreate();
    TEST_ASSERT_EQUAL(ESP_OK, install_driver(hot_plugging_driver_event_cb, NULL));
    xTaskCreate(_random_poweroff_task, "PowerTask", 4096, NULL, 5, NULL);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vEventGroupDelete(test_event_group);
    test_event_group = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, uninstall_driver());
}

// ==================== Memory Leak Check ====================

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before: %u bytes free, After: %u bytes free (delta:%d)\n",
           type, before_free, after_free, delta);
    if (!(delta >= TEST_MEMORY_LEAK_THRESHOLD)) {
        ESP_LOGE(TAG, "Memory leak detected, delta: %d bytes, threshold: %d bytes",
                 delta, TEST_MEMORY_LEAK_THRESHOLD);
    }
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    device_connect_count = 0;
    test_port1 = NULL;
    test_port2 = NULL;
    test_event_group = NULL;
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
    force_link = 1;
    printf("IOT USBH CDC TEST - Comprehensive Test Suite\n");
    unity_run_menu();
}
