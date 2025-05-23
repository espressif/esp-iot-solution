/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "iot_usbh_cdc.h"
#ifdef CONFIG_ESP32_S3_USB_OTG
#include "bsp/esp-bsp.h"
#endif

static const char *TAG = "cdc_basic_demo";

/* ringbuffer size */
#define IN_RINGBUF_SIZE (1024 * 1)
#define OUT_RINGBUF_SIZE (1024 * 1)

/* enable interface num */
#define EXAMPLE_BULK_ITF_NUM 1

/* choose if use user endpoint descriptors */
#define EXAMPLE_CONFIG_USER_EP_DESC

static void usb_receive_task(void *param)
{
    usbh_cdc_handle_t *handle = (usbh_cdc_handle_t *)param;
    size_t data_len = 0;
    uint8_t buf[IN_RINGBUF_SIZE];

    while (1) {
        for (size_t i = 0; i < EXAMPLE_BULK_ITF_NUM; i++) {
            /* Polling USB receive buffer to get data */
            usbh_cdc_get_rx_buffer_size(handle[i], &data_len);
            if (data_len > 0) {
                usbh_cdc_read_bytes(handle[i], buf, &data_len, pdMS_TO_TICKS(100));
                ESP_LOGI(TAG, "Device %d Receive len=%d: %.*s", i, data_len, data_len, buf);
            } else {
                vTaskDelay(1);
            }
        }
    }
}

static void usb_connect_callback(usbh_cdc_handle_t cdc_handle, void *user_data)
{
    usbh_cdc_desc_print(cdc_handle);
    TaskHandle_t task_hdl = (TaskHandle_t)user_data;
    vTaskResume(task_hdl);
    ESP_LOGI(TAG, "Device Connected!");
}

static void usb_disconnect_callback(usbh_cdc_handle_t cdc_handle, void *user_data)
{
    ESP_LOGW(TAG, "Device Disconnected!");
}

void app_main(void)
{
#ifdef CONFIG_ESP32_S3_USB_OTG
    bsp_usb_mode_select_host();
    bsp_usb_host_power_mode(BSP_USB_HOST_POWER_MODE_USB_DEV, true);
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

    usbh_cdc_handle_t handle[EXAMPLE_BULK_ITF_NUM] = {};

    usbh_cdc_device_config_t dev_config = {
        .vid = 0,
        .pid = 0,
        .itf_num = 1,
        .rx_buffer_size = IN_RINGBUF_SIZE,
        .tx_buffer_size = OUT_RINGBUF_SIZE,
        .cbs = {
            .connect = usb_connect_callback,
            .disconnect = usb_disconnect_callback,
            .user_data = xTaskGetCurrentTaskHandle(),
        },
    };

    usbh_cdc_create(&dev_config, &handle[0]);

#if (EXAMPLE_BULK_ITF_NUM > 1)
    dev_config.itf_num = 3;
    memset(&dev_config.cbs, 0, sizeof(dev_config.cbs));
    ESP_LOGI(TAG, "Open interface num: %d with first USB CDC Device", dev_config.itf_num);
    usbh_cdc_create(&dev_config, &handle[1]);
#endif
    /*!< Wait for the USB device to be connected */
    vTaskSuspend(NULL);

    /* Create a task for USB data processing */
    xTaskCreate(usb_receive_task, "usb_rx", 4096, (void *)handle, 2, NULL);

    /* Repeatedly sent AT through USB */
    char buff[32] = "AT\r\n";
    while (1) {
        size_t len = strlen(buff);
        usbh_cdc_write_bytes(handle[0], (uint8_t *)buff, len, pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "Send itf0 len=%d: %s", len, buff);
#if (EXAMPLE_BULK_ITF_NUM > 1)
        len = strlen(buff);
        usbh_cdc_write_bytes(handle[1], (uint8_t *)buff, len, pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "Send itf1 len=%d: %s", len, buff);
#endif
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
