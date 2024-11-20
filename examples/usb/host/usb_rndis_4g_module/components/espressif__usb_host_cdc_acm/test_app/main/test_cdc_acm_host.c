/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED

#include <stdio.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "esp_private/usb_phy.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include <string.h>

#include "esp_intr_alloc.h"

#include "unity.h"
#include "soc/usb_wrap_struct.h"

static uint8_t tx_buf[] = "HELLO";
static uint8_t tx_buf2[] = "WORLD";
static int nb_of_responses;
static int nb_of_responses2;
static bool new_dev_cb_called = false;
static bool rx_overflow = false;
static usb_phy_handle_t phy_hdl = NULL;

static void force_conn_state(bool connected, TickType_t delay_ticks)
{
    TEST_ASSERT_NOT_EQUAL(NULL, phy_hdl);
    if (delay_ticks > 0) {
        //Delay of 0 ticks causes a yield. So skip if delay_ticks is 0.
        vTaskDelay(delay_ticks);
    }
    ESP_ERROR_CHECK(usb_phy_action(phy_hdl, (connected) ? USB_PHY_ACTION_HOST_ALLOW_CONN : USB_PHY_ACTION_HOST_FORCE_DISCONN));
}

void usb_lib_task(void *arg)
{
    // Initialize the internal USB PHY to connect to the USB OTG peripheral. We manually install the USB PHY for testing
    usb_phy_config_t phy_config = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_HOST,
        .otg_speed = USB_PHY_SPEED_UNDEFINED,   //In Host mode, the speed is determined by the connected device
    };
    TEST_ASSERT_EQUAL(ESP_OK, usb_new_phy(&phy_config, &phy_hdl));
    // Install USB Host driver. Should only be called once in entire application
    const usb_host_config_t host_config = {
        .skip_phy_setup = true,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    TEST_ASSERT_EQUAL(ESP_OK, usb_host_install(&host_config));
    printf("USB Host installed\n");
    xTaskNotifyGive(arg);

    bool all_clients_gone = false;
    bool all_dev_free = false;
    while (!all_clients_gone || !all_dev_free) {
        // Start handling system events
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            printf("No more clients\n");
            usb_host_device_free_all();
            all_clients_gone = true;
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            printf("All devices freed\n");
            all_dev_free = true;
        }
    }

    // Clean up USB Host
    vTaskDelay(10); // Short delay to allow clients clean-up
    TEST_ASSERT_EQUAL(ESP_OK, usb_host_uninstall());
    TEST_ASSERT_EQUAL(ESP_OK, usb_del_phy(phy_hdl)); //Tear down USB PHY
    phy_hdl = NULL;
    vTaskDelete(NULL);
}

void test_install_cdc_driver(void)
{
    // Create a task that will handle USB library events
    TEST_ASSERT_EQUAL(pdTRUE, xTaskCreatePinnedToCore(usb_lib_task, "usb_lib", 4 * 4096, xTaskGetCurrentTaskHandle(), 10, NULL, 0));
    ulTaskNotifyTake(false, 1000);

    printf("Installing CDC-ACM driver\n");
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_install(NULL));
}

/* ------------------------------- Callbacks -------------------------------- */
static bool handle_rx(const uint8_t *data, size_t data_len, void *arg)
{
    printf("Data received\n");
    nb_of_responses++;
    TEST_ASSERT_EQUAL_STRING_LEN(data, arg, data_len);
    return true;
}

static bool handle_rx2(const uint8_t *data, size_t data_len, void *arg)
{
    printf("Data received 2\n");
    nb_of_responses2++;
    TEST_ASSERT_EQUAL_STRING_LEN(data, arg, data_len);
    return true;
}

static bool handle_rx_advanced(const uint8_t *data, size_t data_len, void *arg)
{
    bool *process_data = (bool *)arg;
    return *process_data;
}

static void notif_cb(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case CDC_ACM_HOST_ERROR:
        printf("Error event %d\n", event->data.error);
        break;
    case CDC_ACM_HOST_SERIAL_STATE:
        if (event->data.serial_state.bOverRun) {
            rx_overflow = true;
        }
        break;
    case CDC_ACM_HOST_NETWORK_CONNECTION:
        break;
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        printf("Disconnection event\n");
        TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(event->data.cdc_hdl));
        xTaskNotifyGive(user_ctx);
        break;
    default:
        assert(false);
    }
}

static void new_dev_cb(usb_device_handle_t usb_dev)
{
    new_dev_cb_called = true;
    const usb_config_desc_t *config_desc;
    const usb_device_desc_t *device_desc;

    // Get descriptors
    TEST_ASSERT_EQUAL(ESP_OK, usb_host_get_device_descriptor(usb_dev, &device_desc));
    TEST_ASSERT_EQUAL(ESP_OK, usb_host_get_active_config_descriptor(usb_dev, &config_desc));

    printf("New device connected. VID = 0x%04X PID = %04X\n", device_desc->idVendor, device_desc->idProduct);
}

/* Basic test to check CDC communication:
 * open/read/write/close device
 * CDC-ACM specific commands: set/get_line_coding, set_control_line_state */
TEST_CASE("read_write", "[cdc_acm]")
{
    nb_of_responses = 0;
    cdc_acm_dev_hdl_t cdc_dev = NULL;

    test_install_cdc_driver();

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 500,
        .out_buffer_size = 64,
        .event_cb = notif_cb,
        .data_cb = handle_rx,
        .user_arg = tx_buf,
    };

    printf("Opening CDC-ACM device\n");
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev)); // 0x303A:0x4002 (TinyUSB Dual CDC device)
    TEST_ASSERT_NOT_NULL(cdc_dev);
    vTaskDelay(10);

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_data_tx_blocking(cdc_dev, tx_buf, sizeof(tx_buf), 1000));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_data_tx_blocking(cdc_dev, tx_buf, sizeof(tx_buf), 1000));
    vTaskDelay(100); // Wait until responses are processed

    // We sent two messages, should get two responses
    TEST_ASSERT_EQUAL(2, nb_of_responses);

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());
    vTaskDelay(20); //Short delay to allow task to be cleaned up
}

TEST_CASE("cdc_specific_commands", "[cdc_acm]")
{
    cdc_acm_dev_hdl_t cdc_dev = NULL;

    test_install_cdc_driver();

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 500,
        .out_buffer_size = 64
    };

    printf("Opening CDC-ACM device\n");
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev)); // 0x303A:0x4002 (TinyUSB Dual CDC device)
    TEST_ASSERT_NOT_NULL(cdc_dev);
    vTaskDelay(10);

    printf("Sending CDC specific commands\n");
    cdc_acm_line_coding_t line_coding_get;
    const cdc_acm_line_coding_t line_coding_set = {
        .dwDTERate = 9600,
        .bDataBits = 7,
        .bParityType = 1,
        .bCharFormat = 1,
    };
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_line_coding_set(cdc_dev, &line_coding_set));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_line_coding_get(cdc_dev, &line_coding_get));
    TEST_ASSERT_EQUAL_MEMORY(&line_coding_set, &line_coding_get, sizeof(cdc_acm_line_coding_t));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_set_control_line_state(cdc_dev, true, false));

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());
    vTaskDelay(20);
}

/* Test descriptor print function */
TEST_CASE("desc_print", "[cdc_acm]")
{
    cdc_acm_dev_hdl_t cdc_dev = NULL;

    test_install_cdc_driver();

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 500,
        .out_buffer_size = 64
    };

    printf("Opening CDC-ACM device\n");
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev)); // 0x303A:0x4002 (TinyUSB Dual CDC device)
    TEST_ASSERT_NOT_NULL(cdc_dev);
    cdc_acm_host_desc_print(cdc_dev);
    vTaskDelay(10);

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());
    vTaskDelay(20); //Short delay to allow task to be cleaned up
}

/* Test communication with multiple CDC-ACM devices from one thread */
TEST_CASE("multiple_devices", "[cdc_acm]")
{
    nb_of_responses = 0;
    nb_of_responses2 = 0;

    test_install_cdc_driver();

    printf("Opening 2 CDC-ACM devices\n");
    cdc_acm_dev_hdl_t cdc_dev1, cdc_dev2;
    cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 64,
        .event_cb = notif_cb,
        .data_cb = handle_rx,
        .user_arg = tx_buf,
    };
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev1)); // 0x303A:0x4002 (TinyUSB Dual CDC device)
    dev_config.data_cb = handle_rx2;
    dev_config.user_arg = tx_buf2;
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 2, &dev_config, &cdc_dev2)); // 0x303A:0x4002 (TinyUSB Dual CDC device)
    TEST_ASSERT_NOT_NULL(cdc_dev1);
    TEST_ASSERT_NOT_NULL(cdc_dev2);

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_data_tx_blocking(cdc_dev1, tx_buf, sizeof(tx_buf), 1000));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_data_tx_blocking(cdc_dev2, tx_buf2, sizeof(tx_buf2), 1000));

    vTaskDelay(100); // Wait for RX callbacks

    // We sent two messages, should get two responses
    TEST_ASSERT_EQUAL(1, nb_of_responses);
    TEST_ASSERT_EQUAL(1, nb_of_responses2);

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev1));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev2));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());

    //Short delay to allow task to be cleaned up
    vTaskDelay(20);
}

#define MULTIPLE_THREADS_TRANSFERS_NUM 5
#define MULTIPLE_THREADS_TASKS_NUM 4
void tx_task(void *arg)
{
    cdc_acm_dev_hdl_t cdc_dev = (cdc_acm_dev_hdl_t) arg;
    // Send multiple transfers to make sure that some of them will run at the same time
    for (int i = 0; i < MULTIPLE_THREADS_TRANSFERS_NUM; i++) {
        // BULK endpoints
        TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_data_tx_blocking(cdc_dev, tx_buf, sizeof(tx_buf), 1000));

        // CTRL endpoints
        cdc_acm_line_coding_t line_coding_get;
        TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_line_coding_get(cdc_dev, &line_coding_get));
        TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_set_control_line_state(cdc_dev, true, false));
    }
    vTaskDelete(NULL);
}

/**
 * @brief Multiple threads test
 *
 * In this test, one CDC device is accessed from multiple threads.
 * It has to be opened/closed just once, though.
 */
TEST_CASE("multiple_threads", "[cdc_acm]")
{
    nb_of_responses = 0;
    cdc_acm_dev_hdl_t cdc_dev;
    test_install_cdc_driver();

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 5000,
        .out_buffer_size = 64,
        .event_cb = notif_cb,
        .data_cb = handle_rx,
        .user_arg = tx_buf,
    };

    printf("Opening CDC-ACM device\n");
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev)); // 0x303A:0x4002 (TinyUSB Dual CDC device)
    TEST_ASSERT_NOT_NULL(cdc_dev);

    // Create two tasks that will try to access cdc_dev
    for (int i = 0; i < MULTIPLE_THREADS_TASKS_NUM; i++) {
        TEST_ASSERT_EQUAL(pdTRUE, xTaskCreate(tx_task, "CDC TX", 4096, cdc_dev, i + 3, NULL));
    }

    // Wait until all tasks finish
    vTaskDelay(pdMS_TO_TICKS(500));
    TEST_ASSERT_EQUAL(MULTIPLE_THREADS_TASKS_NUM * MULTIPLE_THREADS_TRANSFERS_NUM, nb_of_responses);

    // Clean-up
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());
    vTaskDelay(20);
}

/* Test CDC driver reaction to USB device sudden disconnection */
TEST_CASE("sudden_disconnection", "[cdc_acm]")
{
    test_install_cdc_driver();

    cdc_acm_dev_hdl_t cdc_dev;
    cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 64,
        .event_cb = notif_cb,
        .data_cb = handle_rx
    };
    dev_config.user_arg = xTaskGetCurrentTaskHandle();
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev));
    TEST_ASSERT_NOT_NULL(cdc_dev);

    force_conn_state(false, pdMS_TO_TICKS(10));                        // Simulate device disconnection
    TEST_ASSERT_EQUAL(1, ulTaskNotifyTake(false, pdMS_TO_TICKS(100))); // Notify will succeed only if CDC_ACM_HOST_DEVICE_DISCONNECTED notification was generated

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());
    vTaskDelay(20); //Short delay to allow task to be cleaned up
}

/**
 * @brief CDC-ACM error handling test
 *
 * There are multiple erroneous scenarios checked in this test:
 *
 * -# Install CDC-ACM driver without USB Host
 * -# Open device without installed driver
 * -# Uninstall driver before installing it
 * -# Open non-existent device
 * -# Open the same device twice
 * -# Uninstall driver with open devices
 * -# Send data that is too large
 * -# Send unsupported CDC request
 * -# Write to read-only device
 */
TEST_CASE("error_handling", "[cdc_acm]")
{
    cdc_acm_dev_hdl_t cdc_dev;
    cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 500,
        .out_buffer_size = 64,
        .event_cb = notif_cb,
        .data_cb = handle_rx
    };

    // Install CDC-ACM driver without USB Host
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, cdc_acm_host_install(NULL));

    // Open device without installed driver
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev));

    // Uninstall driver before installing it
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, cdc_acm_host_uninstall());

    // Properly install USB and CDC drivers
    test_install_cdc_driver();

    // Open non-existent device
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, cdc_acm_host_open(0x303A, 0x1234, 0, &dev_config, &cdc_dev)); // 0x303A:0x1234 this device is not connected to USB Host
    TEST_ASSERT_NULL(cdc_dev);

    // Open regular device
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev));
    TEST_ASSERT_NOT_NULL(cdc_dev);

    // Open one CDC-ACM device twice
    cdc_acm_dev_hdl_t cdc_dev_test;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev_test));
    TEST_ASSERT_NULL(cdc_dev_test);

    // Uninstall driver with open devices
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, cdc_acm_host_uninstall());

    // Send data that is too large and NULL data
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, cdc_acm_host_data_tx_blocking(cdc_dev, tx_buf, 1024, 1000));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cdc_acm_host_data_tx_blocking(cdc_dev, NULL, 10, 1000));

    // Change mode to read-only and try to write to it
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));
    dev_config.out_buffer_size = 0; // Read-only device
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev));
    TEST_ASSERT_NOT_NULL(cdc_dev);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, cdc_acm_host_data_tx_blocking(cdc_dev, tx_buf, sizeof(tx_buf), 1000));

    // Send unsupported CDC request (TinyUSB accepts SendBreak command, even though it doesn't support it)
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_send_break(cdc_dev, 100));

    // Clean-up
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());
    vTaskDelay(20);
}

TEST_CASE("custom_command", "[cdc_acm]")
{
    test_install_cdc_driver();

    // Open device with only CTRL endpoint (endpoint no 0)
    cdc_acm_dev_hdl_t cdc_dev;
    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 500,
        .out_buffer_size = 0,
        .event_cb = notif_cb,
        .data_cb = NULL
    };

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev));
    TEST_ASSERT_NOT_NULL(cdc_dev);

    // Corresponds to command: Set Control Line State, DTR on, RTS off
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_send_custom_request(cdc_dev, 0x21, 34, 1, 0, 0, NULL));

    // Clean-up
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());
    vTaskDelay(20);
}

TEST_CASE("new_device_connection_1", "[cdc_acm]")
{
    // Create a task that will handle USB library events
    TEST_ASSERT_EQUAL(pdTRUE, xTaskCreatePinnedToCore(usb_lib_task, "usb_lib", 4 * 4096, xTaskGetCurrentTaskHandle(), 10, NULL, 0));
    ulTaskNotifyTake(false, 1000);

    // Option 1: Register callback during driver install
    printf("Installing CDC-ACM driver\n");
    cdc_acm_host_driver_config_t driver_config = {
        .driver_task_priority = 11,
        .driver_task_stack_size = 2048,
        .xCoreID = 0,
        .new_dev_cb = new_dev_cb,
    };
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_install(&driver_config));

    vTaskDelay(50);
    TEST_ASSERT_TRUE_MESSAGE(new_dev_cb_called, "New device callback was not called\n");
    new_dev_cb_called = false;

    // Clean-up
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());
    vTaskDelay(20);
}

TEST_CASE("new_device_connection_2", "[cdc_acm]")
{
    test_install_cdc_driver();

    // Option 2: Register callback after driver install
    force_conn_state(false, 0);
    vTaskDelay(50);
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_register_new_dev_callback(new_dev_cb));
    force_conn_state(true, 0);
    vTaskDelay(50);
    TEST_ASSERT_TRUE_MESSAGE(new_dev_cb_called, "New device callback was not called\n");
    new_dev_cb_called = false;

    // Clean-up
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());
    vTaskDelay(20);
}

TEST_CASE("rx_buffer", "[cdc_acm]")
{
    test_install_cdc_driver();
    bool process_data = true; // This variable will determine return value of data_cb

    cdc_acm_dev_hdl_t cdc_dev;
    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 500,
        .out_buffer_size = 64,
        .in_buffer_size = 512,
        .event_cb = notif_cb,
        .data_cb = handle_rx_advanced,
        .user_arg = &process_data,
    };

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev));
    TEST_ASSERT_NOT_NULL(cdc_dev);

    // 1. Send > in_buffer_size bytes of data in normal operation: Expect no error
    uint8_t tx_data[64] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_data_tx_blocking(cdc_dev, tx_data, sizeof(tx_data), 1000));
        vTaskDelay(5);
    }
    TEST_ASSERT_FALSE_MESSAGE(rx_overflow, "RX overflowed");
    rx_overflow = false;

    // 2. Send < (in_buffer_size - IN_MPS) bytes of data in 'not processed' mode: Expect no error
    process_data = false;
    for (int i = 0; i < 7; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_data_tx_blocking(cdc_dev, tx_data, sizeof(tx_data), 1000));
        vTaskDelay(5);
    }
    TEST_ASSERT_FALSE_MESSAGE(rx_overflow, "RX overflowed");
    rx_overflow = false;

    // 3. Send >= (in_buffer_size - IN_MPS) bytes of data in 'not processed' mode: Expect error
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_data_tx_blocking(cdc_dev, tx_data, sizeof(tx_data), 1000));
    vTaskDelay(5);
    TEST_ASSERT_TRUE_MESSAGE(rx_overflow, "RX did not overflow");
    rx_overflow = false;

    // 4. Send more data to the EP: Expect no error
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_data_tx_blocking(cdc_dev, tx_data, sizeof(tx_data), 1000));
    vTaskDelay(5);
    TEST_ASSERT_FALSE_MESSAGE(rx_overflow, "RX overflowed");
    rx_overflow = false;

    // Clean-up
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());
    vTaskDelay(20);
}

TEST_CASE("functional_descriptor", "[cdc_acm]")
{
    test_install_cdc_driver();

    cdc_acm_dev_hdl_t cdc_dev;
    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 500,
        .out_buffer_size = 64,
        .event_cb = notif_cb,
        .data_cb = NULL
    };

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev));
    TEST_ASSERT_NOT_NULL(cdc_dev);

    // Request various CDC functional descriptors
    // Following are present in the TinyUSB CDC device: Header, Call management, ACM, Union
    const cdc_header_desc_t *header_desc;
    const cdc_acm_call_desc_t *call_desc;
    const cdc_acm_acm_desc_t *acm_desc;
    const cdc_union_desc_t *union_desc;

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_cdc_desc_get(cdc_dev, USB_CDC_DESC_SUBTYPE_HEADER, (const usb_standard_desc_t **)&header_desc));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_cdc_desc_get(cdc_dev, USB_CDC_DESC_SUBTYPE_CALL, (const usb_standard_desc_t **)&call_desc));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_cdc_desc_get(cdc_dev, USB_CDC_DESC_SUBTYPE_ACM, (const usb_standard_desc_t **)&acm_desc));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_cdc_desc_get(cdc_dev, USB_CDC_DESC_SUBTYPE_UNION, (const usb_standard_desc_t **)&union_desc));
    TEST_ASSERT_NOT_NULL(header_desc);
    TEST_ASSERT_NOT_NULL(call_desc);
    TEST_ASSERT_NOT_NULL(acm_desc);
    TEST_ASSERT_NOT_NULL(union_desc);
    TEST_ASSERT_EQUAL(USB_CDC_DESC_SUBTYPE_HEADER, header_desc->bDescriptorSubtype);
    TEST_ASSERT_EQUAL(USB_CDC_DESC_SUBTYPE_CALL, call_desc->bDescriptorSubtype);
    TEST_ASSERT_EQUAL(USB_CDC_DESC_SUBTYPE_ACM, acm_desc->bDescriptorSubtype);
    TEST_ASSERT_EQUAL(USB_CDC_DESC_SUBTYPE_UNION, union_desc->bDescriptorSubtype);

    // Check few errors
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, cdc_acm_host_cdc_desc_get(cdc_dev, USB_CDC_DESC_SUBTYPE_OBEX, (const usb_standard_desc_t **)&header_desc));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cdc_acm_host_cdc_desc_get(cdc_dev, USB_CDC_DESC_SUBTYPE_MAX, (const usb_standard_desc_t **)&header_desc));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cdc_acm_host_cdc_desc_get(NULL, USB_CDC_DESC_SUBTYPE_HEADER, (const usb_standard_desc_t **)&header_desc));

    // Clean-up
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());
    vTaskDelay(20);
}

/**
 * @brief Closing procedure test
 *
 * -# Close already closed device
 */
TEST_CASE("closing", "[cdc_acm]")
{
    cdc_acm_dev_hdl_t cdc_dev = NULL;

    test_install_cdc_driver();

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 500,
        .out_buffer_size = 64,
        .event_cb = notif_cb,
        .data_cb = handle_rx,
    };

    printf("Opening CDC-ACM device\n");
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev)); // 0x303A:0x4002 (TinyUSB Dual CDC device)
    TEST_ASSERT_NOT_NULL(cdc_dev);
    vTaskDelay(10);

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));
    printf("Closing already closed device \n");
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());

    vTaskDelay(20); //Short delay to allow task to be cleaned up
}

/* Basic test to check CDC driver reaction to TX timeout */
TEST_CASE("tx_timeout", "[cdc_acm]")
{
    cdc_acm_dev_hdl_t cdc_dev = NULL;

    test_install_cdc_driver();

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 500,
        .out_buffer_size = 64,
        .event_cb = notif_cb,
        .data_cb = handle_rx,
        .user_arg = tx_buf,
    };

    printf("Opening CDC-ACM device\n");
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, 0x4002, 0, &dev_config, &cdc_dev)); // 0x303A:0x4002 (TinyUSB Dual CDC device)
    TEST_ASSERT_NOT_NULL(cdc_dev);
    vTaskDelay(10);

    // TX some data with timeout_ms=0. This will cause a timeout
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, cdc_acm_host_data_tx_blocking(cdc_dev, tx_buf, sizeof(tx_buf), 0));
    vTaskDelay(100); // Wait before trying new TX

    // TX some data again with greater timeout. This will check normal operation
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_data_tx_blocking(cdc_dev, tx_buf, sizeof(tx_buf), 1000));
    vTaskDelay(100);

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());

    vTaskDelay(20); //Short delay to allow task to be cleaned up
}

/**
 * @brief Test: Opening with any VID/PID
 *
 * #. Try to open a device with all combinations of any VID/PID
 * #. Try to open a non-existing device with all combinations of any VID/PID
 */
TEST_CASE("any_vid_pid", "[cdc_acm]")
{
    cdc_acm_dev_hdl_t cdc_dev = NULL;
    test_install_cdc_driver();

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 500,
        .out_buffer_size = 64,
        .event_cb = notif_cb,
        .data_cb = handle_rx,
        .user_arg = tx_buf,
    };

    printf("Opening existing CDC-ACM devices with any VID/PID\n");
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(CDC_HOST_ANY_VID, CDC_HOST_ANY_PID, 0, &dev_config, &cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(0x303A, CDC_HOST_ANY_PID, 0, &dev_config, &cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_open(CDC_HOST_ANY_VID, 0x4002, 0, &dev_config, &cdc_dev));
    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_close(cdc_dev));

    printf("Opening non-existing CDC-ACM devices with any VID/PID\n");
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, cdc_acm_host_open(0x1234, CDC_HOST_ANY_PID, 0, &dev_config, &cdc_dev));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, cdc_acm_host_open(CDC_HOST_ANY_VID, 0x1234, 0, &dev_config, &cdc_dev));

    TEST_ASSERT_EQUAL(ESP_OK, cdc_acm_host_uninstall());
    vTaskDelay(20); //Short delay to allow task to be cleaned up
}

/* Following test case implements dual CDC-ACM USB device that can be used as mock device for CDC-ACM Host tests */
void run_usb_dual_cdc_device(void);
TEST_CASE("mock_device_app", "[cdc_acm_device][ignore]")
{
    run_usb_dual_cdc_device();
    while (1) {
        vTaskDelay(10);
    }
}

#endif // SOC_USB_OTG_SUPPORTED
