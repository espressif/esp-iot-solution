/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "test_utils.h"
#include "iot_usbh_cdc.h"
#include "esp_log.h"

#define IN_RINGBUF_SIZE (256 * 1)
#define OUT_RINGBUF_SIZE (256 * 1)
#define READ_TASK_KILL_BIT BIT1

#define TAG "cdc_test"
EventGroupHandle_t s_event_group_hdl = NULL;
#define TEST_ITF_NUM   2
#define TEST_EP_IN0  0x87
#define TEST_EP_OUT0 0x07
#define TEST_EP_IN1  0x86
#define TEST_EP_OUT1 0x06

static usb_ep_desc_t bulk_out_ep_desc = {
    .bLength = sizeof(usb_ep_desc_t),
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
    .bEndpointAddress = TEST_EP_OUT0,       //EP 1 OUT
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
    .wMaxPacketSize = 64,           //MPS of 64 bytes
    .bInterval = 0,
};

static usb_ep_desc_t bulk_in_ep_desc = {
    .bLength = sizeof(usb_ep_desc_t),
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
    .bEndpointAddress = TEST_EP_IN0,       //EP 2 IN
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
    .wMaxPacketSize = 64,           //MPS of 64 bytes
    .bInterval = 0,
};

static void usb_read_task(void *param)
{
    size_t data_len = 0;
    uint8_t buf[IN_RINGBUF_SIZE];
    while (!(xEventGroupGetBits(s_event_group_hdl) & READ_TASK_KILL_BIT)) {
        for (size_t i = 0; i < TEST_ITF_NUM; i++) {
            if(usbh_cdc_get_itf_state(i) == false) continue;
            usbh_cdc_itf_get_buffered_data_len(i, &data_len);
            if (data_len > 0) {
                usbh_cdc_itf_read_bytes(i, buf, data_len, 10);
                ESP_LOGI(TAG, "Itf %d, rcv len=%d: %.*s", i, data_len, data_len, buf);
            } else {
                vTaskDelay(1);
            }
        }
    }
    xEventGroupClearBits(s_event_group_hdl, READ_TASK_KILL_BIT);
    ESP_LOGW(TAG, "CDC read task deleted");
    vTaskDelete(NULL);
}

TEST_CASE("usb cdc R/W", "[iot_usbh_cdc]")
{
    for (size_t i = 0; i < 5; i++) {
        s_event_group_hdl = xEventGroupCreate();
        TEST_ASSERT(s_event_group_hdl);
        static usbh_cdc_config_t config = {
            .bulk_in_ep = &bulk_in_ep_desc,
            .bulk_out_ep = &bulk_out_ep_desc,
            .rx_buffer_size = IN_RINGBUF_SIZE,
            .tx_buffer_size = OUT_RINGBUF_SIZE,
        };

        TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_install(&config));
        /* Waiting for USB device connected */
        TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_wait_connect(portMAX_DELAY));
        xTaskCreate(usb_read_task, "usb_read", 4096, NULL, 2, NULL);
        uint8_t buff[] = "AT\r\n";
        uint8_t loop_num = 10;
        while (--loop_num) {
            int len = usbh_cdc_write_bytes(buff, 4);
            ESP_LOGI(TAG, "Send len=%d: %s", len, buff);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        xEventGroupSetBits(s_event_group_hdl, READ_TASK_KILL_BIT);
        ESP_LOGW(TAG, "Waiting CDC read task delete");
        while (xEventGroupGetBits(s_event_group_hdl) & READ_TASK_KILL_BIT) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        vEventGroupDelete(s_event_group_hdl);
        TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_delete());
    }
}

TEST_CASE("usb dual cdc R/W", "[iot_usbh_cdc]")
{
    for (size_t i = 0; i < 5; i++) {
        s_event_group_hdl = xEventGroupCreate();
        TEST_ASSERT(s_event_group_hdl);
        static usbh_cdc_config_t config = {
            .bulk_in_ep = &bulk_in_ep_desc,
            .bulk_out_ep = &bulk_out_ep_desc,
            .rx_buffer_size = IN_RINGBUF_SIZE,
            .tx_buffer_size = OUT_RINGBUF_SIZE,
        };
        config.itf_num = 2;
        /* test config with only ep addr */
        config.bulk_in_ep_addrs[1] = TEST_EP_IN1;
        config.bulk_out_ep_addrs[1] = TEST_EP_OUT1;
        config.rx_buffer_sizes[1] = IN_RINGBUF_SIZE;
        config.tx_buffer_sizes[1] = OUT_RINGBUF_SIZE;
        TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_install(&config));
        /* Waiting for USB device connected */
        TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_wait_connect(portMAX_DELAY));
        xTaskCreate(usb_read_task, "usb_read", 4096, NULL, 2, NULL);
        uint8_t buff[] = "AT\r\n";
        uint8_t loop_num = 100;
        while (--loop_num) {
            int len = usbh_cdc_itf_write_bytes(0, buff, sizeof(buff));
            ESP_LOGI(TAG, "Send itf0 len=%d: %s", len, buff);
            len = usbh_cdc_itf_write_bytes(1, buff, sizeof(buff));
            ESP_LOGI(TAG, "Send itf1 len=%d: %s", len, buff);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        xEventGroupSetBits(s_event_group_hdl, READ_TASK_KILL_BIT);
        ESP_LOGW(TAG, "Waiting CDC read task delete");
        while (xEventGroupGetBits(s_event_group_hdl) & READ_TASK_KILL_BIT) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        vEventGroupDelete(s_event_group_hdl);
        TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_delete());
    }
}