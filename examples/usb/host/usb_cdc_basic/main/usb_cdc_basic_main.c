/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "iot_usbh_cdc.h"

static const char *TAG = "cdc_basic_demo";

/* enable interface num */
#define EXAMPLE_BULK_ITF_NUM 1
#define EXAMPLE_INTF1_NUM 0
#define EXAMPLE_INTF2_NUM 1

#define USB_DEV_DISCONNECTED_BIT    BIT(0)

static usbh_cdc_port_handle_t g_port_handle[EXAMPLE_BULK_ITF_NUM] = {};
static EventGroupHandle_t g_event_group;

static void usb_demo_task(void *param)
{
    const usb_config_desc_t *config_desc = NULL;
    const usb_device_desc_t *device_desc = NULL;
    usb_device_handle_t usb_dev = NULL;
    usbh_cdc_get_dev_handle(g_port_handle[0], &usb_dev);
    ESP_RETURN_VOID_ON_ERROR(usb_host_get_device_descriptor(usb_dev, &device_desc), TAG, "Failed to get device descriptor");
    ESP_RETURN_VOID_ON_ERROR(usb_host_get_active_config_descriptor(usb_dev, &config_desc), TAG, "Failed to get active configuration descriptor");

    if (device_desc->iProduct != 0) {
        uint8_t bmRequestType = USB_BM_REQUEST_TYPE_DIR_IN | USB_BM_REQUEST_TYPE_TYPE_STANDARD | USB_BM_REQUEST_TYPE_RECIP_DEVICE;
        uint8_t bRequest = USB_B_REQUEST_GET_DESCRIPTOR;
        uint16_t wValue = (USB_W_VALUE_DT_STRING << 8) | (device_desc->iProduct & 0xFF);
#define LANG_ID_ENGLISH 0x409
        uint16_t wIndex = LANG_ID_ENGLISH;

        uint8_t str[64] = {0};
        uint16_t wLength = 64;
        esp_err_t ret = usbh_cdc_send_custom_request(g_port_handle[0], bmRequestType, bRequest, wValue, wIndex, wLength, str);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to send custom request");
        ESP_LOG_BUFFER_HEXDUMP("Received string descriptor", str, sizeof(str), ESP_LOG_INFO);
    }

    /* Repeatedly sent AT through USB */
    char buff[32] = "AT\r\n";
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(g_event_group, USB_DEV_DISCONNECTED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
        if (bits & USB_DEV_DISCONNECTED_BIT) {
            break;
        }
        size_t len = strlen(buff);
        usbh_cdc_write_bytes(g_port_handle[0], (uint8_t *)buff, len, pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "Send itf0 len=%d: %s", len, buff);
#if (EXAMPLE_BULK_ITF_NUM == 2)
        len = strlen(buff);
        usbh_cdc_write_bytes(g_port_handle[1], (uint8_t *)buff, len, pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "Send itf1 len=%d: %s", len, buff);
#endif
    }

err:
    ESP_LOGI(TAG, "USB demo task deleted");
    vTaskDelete(NULL);
}

static void _usbh_cdc_recv_data_cb(usbh_cdc_port_handle_t cdc_port_handle, void *arg)
{
    uint8_t buf[64];
    size_t data_len = 0;
    int itf_num = (int)arg;
    /* Polling USB receive buffer to get data */
    usbh_cdc_get_rx_buffer_size(cdc_port_handle, &data_len);
    if (data_len > 0) {
        data_len = data_len > sizeof(buf) ? sizeof(buf) : data_len;
        usbh_cdc_read_bytes(cdc_port_handle, buf, &data_len, 0);
        ESP_LOGI(TAG, "itf%d Receive len=%d", itf_num, data_len);
        ESP_LOG_BUFFER_HEXDUMP("data_recv", buf, data_len, ESP_LOG_INFO);
    }
}

static void _usbh_cdc_notif_cb(usbh_cdc_port_handle_t cdc_port_handle, iot_cdc_notification_t *notif, void *arg)
{
    switch (notif->bNotificationCode) {
    case USB_CDC_NOTIFY_NETWORK_CONNECTION:
        bool connected = notif->wValue;
        ESP_LOGI(TAG, "Notify - network connection changed: %s", connected ? "Connected" : "Disconnected");
        break;
    case USB_CDC_NOTIFY_SPEED_CHANGE: {
        uint32_t *speeds = (uint32_t *)notif->Data;
        ESP_LOGI(TAG, "Notify - link speeds: %lu kbps ↑, %lu kbps ↓", (speeds[0]) / 1000, (speeds[1]) / 1000);
        break;
    }
    case USB_CDC_NOTIFY_SERIAL_STATE:
        usb_cdc_serial_state_t *uart_state = (usb_cdc_serial_state_t *)notif->Data;
        ESP_LOGI(TAG, "Notify - serial state: intf(%d), dcd: %d, dsr: %d, break: %d, ring: %d, frame_err: %d, parity_err: %d, overrun: %d", notif->wIndex,
                 uart_state->dcd, uart_state->dsr, uart_state->break_det, uart_state->ring,
                 uart_state->framing_err, uart_state->parity_err, uart_state->overrun_err);
        break;
    default:
        ESP_LOGW(TAG, "unexpected notification 0x%02x!", notif->bNotificationCode);
        return;
    }
}

static void _usbh_dev_event_cb(usbh_cdc_device_event_t event, usbh_cdc_device_event_data_t *event_data, void *user_ctx)
{
    switch (event) {
    case CDC_HOST_DEVICE_EVENT_CONNECTED: {
        usbh_cdc_port_config_t config = {
            .dev_addr = event_data->new_dev.dev_addr,
            .itf_num = EXAMPLE_INTF1_NUM,
            .in_transfer_buffer_size = 1024,
            .out_transfer_buffer_size = 1024,
            .cbs = {
                .recv_data = _usbh_cdc_recv_data_cb,
                .notif_cb = _usbh_cdc_notif_cb,
                .user_data = (void*)EXAMPLE_INTF1_NUM,
            }
        };
        ESP_RETURN_VOID_ON_ERROR(usbh_cdc_port_open(&config, &(g_port_handle[0])), TAG, "Failed to open USB CDC port");
        usbh_cdc_desc_print(g_port_handle[0]);

#if (EXAMPLE_BULK_ITF_NUM == 2)
        config.dev_addr = event_data->new_dev.dev_addr;
        config.itf_num = EXAMPLE_INTF2_NUM;
        config.cbs.user_data = (void*)EXAMPLE_INTF2_NUM;
        ESP_RETURN_VOID_ON_ERROR(usbh_cdc_port_open(&config, &(g_port_handle[1])), TAG, "Failed to open USB CDC port");
#endif

        /* Create a task for USB data processing */
        xTaskCreate(usb_demo_task, "cdc_task", 4096, NULL, 2, NULL);
        ESP_LOGI(TAG, "Device Connected!");
    } break;

    case CDC_HOST_DEVICE_EVENT_DISCONNECTED: {
        ESP_LOGW(TAG, "Device Disconnected!");
        xEventGroupSetBits(g_event_group, USB_DEV_DISCONNECTED_BIT);
        g_port_handle[0] = NULL;
        g_port_handle[1] = NULL;
    } break;

    default:
        break;
    }
}

void app_main(void)
{
#ifdef CONFIG_ESP32_S3_USB_OTG
    // USB mode select host
    const gpio_config_t io_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_18),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_18, 1));

    // Set host usb dev power mode
    const gpio_config_t power_io_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_17) | BIT64(GPIO_NUM_12) | BIT64(GPIO_NUM_13),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&power_io_config));

    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_17, 1)); // Configure the limiter 500mA
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_12, 0));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_13, 0)); // Turn power off
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_12, 1)); // Turn on usb dev power mode
#endif
    /* install usbh cdc driver with skip_init_usb_host_driver */
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    /* install USB host CDC driver */
    usbh_cdc_driver_install(&config);
    g_event_group = xEventGroupCreate();
    ESP_RETURN_VOID_ON_FALSE(g_event_group != NULL, TAG, "Failed to create event group");
    ESP_ERROR_CHECK(usbh_cdc_register_dev_event_cb(ESP_USB_DEVICE_MATCH_ID_ANY, _usbh_dev_event_cb, xTaskGetCurrentTaskHandle()));
}
