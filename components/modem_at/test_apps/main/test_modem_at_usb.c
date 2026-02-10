/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#ifdef CONFIG_SOC_USB_OTG_SUPPORTED
#include "iot_usbh_cdc.h"
#endif
#include "at_3gpp_ts_27_007.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "test_modem_at_usb";

ssize_t TEST_MEMORY_LEAK_THRESHOLD = 0;
extern int force_link;

#define UPDATE_LEAK_THRESHOLD(first_val) \
static bool is_first = true; \
if (is_first) { \
    TEST_MEMORY_LEAK_THRESHOLD = first_val; \
} else { \
    TEST_MEMORY_LEAK_THRESHOLD = 0; \
}

#ifdef CONFIG_SOC_USB_OTG_SUPPORTED
#define TEST_AT_INTERFACE_NUM 3

static EventGroupHandle_t event_group;
#define TEST_CONNECTED_BIT BIT(0)
#define TEST_DONE_BIT BIT(2)

typedef struct {
    usbh_cdc_port_handle_t cdc_port;      /*!< CDC port handle */
    at_handle_t at_handle;                /*!< AT command parser handle */
} at_ctx_t;
at_ctx_t g_at_ctx = {0};

static esp_err_t _usb_at_send_cmd(const char *cmd, size_t length, void *usr_data)
{
    at_ctx_t *at_ctx = (at_ctx_t *)usr_data;
    TEST_ASSERT_NOT_NULL(at_ctx);
    return usbh_cdc_write_bytes(at_ctx->cdc_port, (const uint8_t *)cmd, length, pdMS_TO_TICKS(500));
}

TEST_CASE("modem at memory leak", "[iot_usbh_modem][creat-destory][auto]")
{
    UPDATE_LEAK_THRESHOLD(0);
    // init the AT command parser
    modem_at_config_t at_config = {
        .recv_buffer_length = 128,
        .send_buffer_length = 256,
        .io = {
            .send_cmd = _usb_at_send_cmd,
            .usr_data = NULL,
        }
    };
    at_handle_t at_handle = modem_at_parser_create(&at_config);
    TEST_ASSERT_NOT_NULL(at_handle);

    // free the AT command parser
    modem_at_parser_destroy(at_handle);
}

static void _usb_recv_data_cb(usbh_cdc_port_handle_t cdc_port_handle, void *arg)
{
    at_ctx_t *at_ctx = (at_ctx_t *)arg;

    size_t length = 0;
    usbh_cdc_get_rx_buffer_size(cdc_port_handle, &length);
    printf("USB AT received data length: %d\n", length);

    char *buffer;
    size_t buffer_remain;
    modem_at_get_response_buffer(at_ctx->at_handle, &buffer, &buffer_remain);
    if (buffer_remain < length) {
        length = buffer_remain;
        ESP_LOGE(TAG, "data size is too big, truncated to %d", length);
    }
    usbh_cdc_read_bytes(cdc_port_handle, (uint8_t *)buffer, &length, 0);
    // Parse the AT command response
    modem_at_write_response_done(at_ctx->at_handle, length);
}

static void driver_event_cb(usbh_cdc_device_event_t event, usbh_cdc_device_event_data_t *event_data, void *user_ctx)
{
    switch (event) {
    case CDC_HOST_DEVICE_EVENT_CONNECTED: {
        at_ctx_t *at_ctx = (at_ctx_t *)user_ctx;
        const usb_device_desc_t *device_desc = event_data->new_dev.device_desc;
        ESP_LOGI(TAG, "USB CDC device connected, VID: 0x%04x, PID: 0x%04x", device_desc->idVendor, device_desc->idProduct);

        usbh_cdc_port_config_t config = {
            .dev_addr = event_data->new_dev.dev_addr,
            .itf_num = TEST_AT_INTERFACE_NUM,
            .in_transfer_buffer_size = 512 * 3,
            .out_transfer_buffer_size = 512 * 3,
            .cbs = {
                .recv_data = _usb_recv_data_cb,
                .notif_cb = NULL,
                .user_data = at_ctx,
            }
        };
        TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_port_open(&config, &(at_ctx->cdc_port)));
        xEventGroupSetBits(event_group, TEST_CONNECTED_BIT);
    }
    break;

    case CDC_HOST_DEVICE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Device disconnected: dev_addr=%d, dev_hdl=%p",
                 event_data->dev_gone.dev_addr, event_data->dev_gone.dev_hdl);
        at_ctx_t *at_ctx = (at_ctx_t *)user_ctx;
        at_ctx->cdc_port = NULL;
        break;

    default:
        break;
    }
}

TEST_CASE("modem at set command", "[modem_at][usb][auto]")
{
    UPDATE_LEAK_THRESHOLD(-20);

    event_group = xEventGroupCreate();

    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    ESP_ERROR_CHECK(usbh_cdc_driver_install(&config));

    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_register_dev_event_cb(ESP_USB_DEVICE_MATCH_ID_ANY, driver_event_cb, &g_at_ctx));

    // init the AT command parser
    modem_at_config_t at_config = {
        .recv_buffer_length = 128,
        .send_buffer_length = 256,
        .io = {
            .send_cmd = _usb_at_send_cmd,
            .usr_data = &g_at_ctx,
        }
    };
    g_at_ctx.at_handle = modem_at_parser_create(&at_config);
    TEST_ASSERT_NOT_NULL(g_at_ctx.at_handle);

    xEventGroupWaitBits(event_group, TEST_CONNECTED_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    TEST_ASSERT_EQUAL(ESP_OK, modem_at_start(g_at_ctx.at_handle));
    TEST_ASSERT_EQUAL(ESP_OK, at_cmd_at(g_at_ctx.at_handle));
    TEST_ASSERT_EQUAL(ESP_OK, at_cmd_set_echo(g_at_ctx.at_handle, true));

    char str[64] = {0};
    at_cmd_get_manufacturer_id(g_at_ctx.at_handle, str, sizeof(str));
    ESP_LOGI(TAG, "Modem manufacturer ID: \"%s\"", str);
    str[0] = '\0'; // clear the string buffer
    at_cmd_get_module_id(g_at_ctx.at_handle, str, sizeof(str));
    ESP_LOGI(TAG, "Modem module ID: \"%s\"", str);
    str[0] = '\0'; // clear the string buffer
    at_cmd_get_revision_id(g_at_ctx.at_handle, str, sizeof(str));
    ESP_LOGI(TAG, "Modem revision ID: \"%s\"", str);
    str[0] = '\0'; // clear the string buffer
    at_cmd_get_operator_name(g_at_ctx.at_handle, str, sizeof(str));
    ESP_LOGI(TAG, "Modem operator name: \"%s\"", str);

    esp_modem_at_csq_t result;
    at_cmd_get_signal_quality(g_at_ctx.at_handle, &result);
    ESP_LOGI(TAG, "Modem rssi: %d", result.rssi);

    esp_modem_pin_state_t pin_state;
    at_cmd_read_pin(g_at_ctx.at_handle, &pin_state);
    ESP_LOGI(TAG, "SIM pin state: %d", pin_state);

    TEST_ASSERT_EQUAL(ESP_OK, at_cmd_set_echo(g_at_ctx.at_handle, false));

    ESP_LOGI(TAG, "waiting for modem to disconnect");

    vTaskDelay(pdMS_TO_TICKS(500));
    // free the AT command parser
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, modem_at_parser_destroy(g_at_ctx.at_handle));
    TEST_ASSERT_EQUAL(ESP_OK, modem_at_stop(g_at_ctx.at_handle));
    TEST_ASSERT_EQUAL(ESP_OK, modem_at_parser_destroy(g_at_ctx.at_handle));

    g_at_ctx.at_handle = NULL;
    vEventGroupDelete(event_group);
    usbh_cdc_driver_uninstall();
    vTaskDelay(4000 / portTICK_PERIOD_MS); // wait for memory to be freed
    ESP_LOGI(TAG, "TEST_DONE");
}
#endif

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before: %u bytes free, After: %u bytes free (delta:%d)\n", type, before_free, after_free, delta);
    if (!(delta >= TEST_MEMORY_LEAK_THRESHOLD)) {
        ESP_LOGE(TAG, "Memory leak detected, delta: %d bytes, threshold: %d bytes", delta, TEST_MEMORY_LEAK_THRESHOLD);
    }
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
    force_link = 1;

    printf("IOT MODEM AT TEST \n");
    unity_run_menu();
}
