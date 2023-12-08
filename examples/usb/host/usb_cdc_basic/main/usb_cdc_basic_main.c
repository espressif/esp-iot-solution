/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "iot_usbh_cdc.h"

static const char *TAG = "cdc_basic_demo";
/* USB PIN fixed in esp32-s2/s3, can not use io matrix */
#define BOARD_USB_DP_PIN 20
#define BOARD_USB_DN_PIN 19

/* ringbuffer size */
#define IN_RINGBUF_SIZE (1024 * 1)
#define OUT_RINGBUF_SIZE (1024 * 1)

/* enable interface num */
#define EXAMPLE_BULK_ITF_NUM 1
/* bulk endpoint address */
#define EXAMPLE_BULK_IN0_EP_ADDR 0x81
#define EXAMPLE_BULK_OUT0_EP_ADDR 0x01
#define EXAMPLE_BULK_IN1_EP_ADDR 0x82
#define EXAMPLE_BULK_OUT1_EP_ADDR 0x02
/* bulk endpoint max package size */
#define EXAMPLE_BULK_EP_MPS 64

/* choose if use user endpoint descriptors */
#define EXAMPLE_CONFIG_USER_EP_DESC

/*
the basic demo skip the standred get descriptors process,
users need to get params from cdc device descriptors from PC side,
eg. run `lsusb -v` in linux, then hardcode the related params below
*/
#ifdef EXAMPLE_CONFIG_USER_EP_DESC
static usb_ep_desc_t bulk_out_ep_desc = {
    .bLength = sizeof(usb_ep_desc_t),
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
    .bEndpointAddress = EXAMPLE_BULK_OUT0_EP_ADDR,
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
    .wMaxPacketSize = EXAMPLE_BULK_EP_MPS,
};

static usb_ep_desc_t bulk_in_ep_desc = {
    .bLength = sizeof(usb_ep_desc_t),
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
    .bEndpointAddress = EXAMPLE_BULK_IN0_EP_ADDR,
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
    .wMaxPacketSize = EXAMPLE_BULK_EP_MPS,
};
#endif

static void usb_receive_task(void *param)
{
    size_t data_len = 0;
    uint8_t buf[IN_RINGBUF_SIZE];

    while (1) {
        /* Polling USB receive buffer to get data */
        for (size_t i = 0; i < EXAMPLE_BULK_ITF_NUM; i++) {
            if (usbh_cdc_get_itf_state(i) == false) {
                continue;
            }
            usbh_cdc_itf_get_buffered_data_len(i, &data_len);
            if (data_len > 0) {
                usbh_cdc_itf_read_bytes(i, buf, data_len, 10);
                ESP_LOGI(TAG, "Itf %d, Receive len=%d: %.*s", i, data_len, data_len, buf);
            } else {
                vTaskDelay(1);
            }
        }
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
    /* install usbh cdc driver with bulk endpoint configs
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
        .bulk_in_ep_addr = EXAMPLE_BULK_IN0_EP_ADDR,
        .bulk_out_ep_addr = EXAMPLE_BULK_OUT0_EP_ADDR,
#endif
        .rx_buffer_size = IN_RINGBUF_SIZE,
        .tx_buffer_size = OUT_RINGBUF_SIZE,
        .conn_callback = usb_connect_callback,
        .disconn_callback = usb_disconnect_callback,
    };

#if (EXAMPLE_BULK_ITF_NUM > 1)
    ESP_LOGI(TAG, "itf %d using default bulk endpoint descriptor", 1);
    config.itf_num = 2;
    /* test config with only ep addr */
    config.bulk_in_ep_addrs[1] = EXAMPLE_BULK_IN1_EP_ADDR;
    config.bulk_out_ep_addrs[1] = EXAMPLE_BULK_OUT1_EP_ADDR;
    config.rx_buffer_sizes[1] = IN_RINGBUF_SIZE;
    config.tx_buffer_sizes[1] = OUT_RINGBUF_SIZE;
#endif

    /* install USB host CDC driver */
    ESP_ERROR_CHECK(usbh_cdc_driver_install(&config));
    /* Waiting for USB device connected */
    ESP_ERROR_CHECK(usbh_cdc_wait_connect(portMAX_DELAY));
    /* Create a task for USB data processing */
    xTaskCreate(usb_receive_task, "usb_rx", 4096, NULL, 2, NULL);

    /* Repeatly sent AT through USB */
    char buff[32] = "AT\r\n";
    while (1) {
        int len = usbh_cdc_write_bytes((uint8_t *)buff, strlen(buff));
        ESP_LOGI(TAG, "Send itf0 len=%d: %s", len, buff);
#if (EXAMPLE_BULK_ITF_NUM > 1)
        len = usbh_cdc_itf_write_bytes(1, (uint8_t *)buff, strlen(buff));
        ESP_LOGI(TAG, "Send itf1 len=%d: %s", len, buff);
#endif
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
