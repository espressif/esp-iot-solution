// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_usbh_cdc.h"

static const char *TAG = "cdc_basic_demo";
/* USB PIN fixed in esp32-s2/s3, can not use io matrix */
#define BOARD_USB_DP_PIN 20
#define BOARD_USB_DN_PIN 19

/* ringbuffer size */
#define IN_RINGBUF_SIZE (1024 * 1)
#define OUT_RINGBUF_SIZE (1024 * 1)

/* bulk endpoint address */
#define EXAMPLE_BULK_IN_EP_ADDR 0x81
#define EXAMPLE_BULK_OUT_EP_ADDR 0x01
/* bulk endpoint max package size */
#define EXAMPLE_BULK_EP_MPS 64
/* bulk endpoint transfer interval */
#define EXAMPLE_BULK_EP_INTERVAL 0

/* choose if use user endpoint descriptors */
#define EXAMPLE_CONFIG_USER_EP_DESC

#ifdef EXAMPLE_CONFIG_USER_EP_DESC
/*
the basic demo skip the standred get descriptors process,
users need to get params from cdc device descriptors from PC side,
eg. run `lsusb -v` in linux, then hardcode the related params below
*/
static usb_ep_desc_t bulk_out_ep_desc = {
    .bLength = sizeof(usb_ep_desc_t),
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
    .bEndpointAddress = EXAMPLE_BULK_OUT_EP_ADDR,
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
    .wMaxPacketSize = EXAMPLE_BULK_EP_MPS,
    .bInterval = EXAMPLE_BULK_EP_INTERVAL,
};

static usb_ep_desc_t bulk_in_ep_desc = {
    .bLength = sizeof(usb_ep_desc_t),
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
    .bEndpointAddress = EXAMPLE_BULK_IN_EP_ADDR,
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
    .wMaxPacketSize = EXAMPLE_BULK_EP_MPS,
    .bInterval = EXAMPLE_BULK_EP_INTERVAL,
};
#endif

static void usb_receive_task(void *param)
{
    size_t data_len = 0;
    uint8_t buf[IN_RINGBUF_SIZE];

    while (1) {
        /* Polling USB receive buffer to get data */
        usbh_cdc_get_buffered_data_len(&data_len);

        if (data_len == 0) {
            vTaskDelay(1);
            continue;
        }

        usbh_cdc_read_bytes(buf, data_len, 10);
        ESP_LOGI(TAG, "Receive len=%d: %.*s", data_len, data_len, buf);
    }
}

static void usb_connect_callback(void *arg)
{
    ESP_LOGI(TAG, "Device Connected!");
}

static void usb_disconnect_callback(void *arg)
{
    ESP_LOGW(TAG, "Device Disconnected!");
}

void app_main(void)
{
    /* @brief install usbh cdc driver with bulk endpoint configs
    and size of internal ringbuffer*/
#ifdef EXAMPLE_CONFIG_USER_EP_DESC
    ESP_LOGI(TAG, "using user bulk endpoint descriptor");
    static usbh_cdc_config_t config = {
    /* use user endpoint descriptor */
        .bulk_in_ep = &bulk_in_ep_desc,
        .bulk_out_ep = &bulk_out_ep_desc,
#else
    ESP_LOGI(TAG, "using default bulk endpoint descriptor");
    static usbh_cdc_config_t config = {
    /* use default endpoint descriptor with user address */
        .bulk_in_ep_addr = EXAMPLE_BULK_IN_EP_ADDR,
        .bulk_out_ep_addr = EXAMPLE_BULK_OUT_EP_ADDR,
#endif
        .rx_buffer_size = IN_RINGBUF_SIZE,
        .tx_buffer_size = OUT_RINGBUF_SIZE,
        .conn_callback = usb_connect_callback,
        .disconn_callback = usb_disconnect_callback,
    };
    /* install USB host CDC driver */
    esp_err_t ret = usbh_cdc_driver_install(&config);
    assert(ret == ESP_OK);
    /* Waitting for USB device connected */
    ret = usbh_cdc_wait_connect(portMAX_DELAY);
    assert(ret == ESP_OK);
    /* Create a task for USB data processing */
    xTaskCreate(usb_receive_task, "usb_rx", 4096, NULL, 2, NULL);

    /* Repeatly sent AT through USB */
    char buff[32] = "AT\r\n";
    while (1) {
        int len = usbh_cdc_write_bytes((uint8_t *)buff, strlen(buff));
        ESP_LOGI(TAG, "Send len=%d: %s", len, buff);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
