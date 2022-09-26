// Copyright 2016-2021 Espressif Systems (Shanghai) PTE LTD
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "hal/usb_hal.h"
#include "uvc_stream.h"
#include "esp_log.h"

static const char *TAG = "uvc_test";

/* USB PIN fixed in esp32-s2, can not use io matrix */
#define BOARD_USB_DP_PIN 20
#define BOARD_USB_DN_PIN 19

/* USB Camera Descriptors Related MACROS,
the quick demo skip the standred get descriptors process,
users need to get params from camera descriptors from PC side,
eg. run `lsusb -v` in linux,
then hardcode the related MACROS below
*/
#define DESCRIPTOR_CONFIGURATION_INDEX 1
#define DESCRIPTOR_FORMAT_MJPEG_INDEX  2
#define DESCRIPTOR_FRAME_320_240_INDEX 3
#define DESCRIPTOR_FRAME_15FPS_INTERVAL 666666

#define DESCRIPTOR_STREAM_INTERFACE_INDEX   1
#define DESCRIPTOR_STREAM_INTERFACE_ALT_MPS_512 3

#define DESCRIPTOR_STREAM_ENDPOINT_ADDR 0x81

#define DEMO_FRAME_WIDTH 320
#define DEMO_FRAME_HEIGHT 240
#define DEMO_XFER_BUFFER_SIZE (35 * 1024)
#define DEMO_FRAME_INDEX DESCRIPTOR_FRAME_320_240_INDEX
#define DEMO_FRAME_INTERVAL DESCRIPTOR_FRAME_15FPS_INTERVAL

/* max packet size of esp32-s2 is 1*512, bigger is not supported*/
#define DEMO_ISOC_EP_MPS 512
#define DEMO_BULK_EP_MPS 64
#define DEMO_ISOC_INTERFACE_ALT DESCRIPTOR_STREAM_INTERFACE_ALT_MPS_512

static void *_malloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static void _free(void *ptr)
{
    heap_caps_free(ptr);
}

/* *******************************************************************************************
 * This callback function runs once per frame. Use it to perform any
 * quick processing you need, or have it put the frame into your application's
 * input queue. If this function takes too long, you'll start losing frames. */
static void frame_cb(uvc_frame_t *frame, void *ptr)
{
    ESP_LOGI(TAG, "callback! frame_format = %d, seq = %u, width = %d, height = %d, length = %u, ptr = %d",
            frame->frame_format, frame->sequence, frame->width, frame->height, frame->data_bytes, (int) ptr);

    switch (frame->frame_format) {
        case UVC_FRAME_FORMAT_MJPEG:
            break;
        default:
            break;
    }
}

TEST_CASE("test uvc isoc streaming", "[usb][uvc_stream]")
{
    /* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
    uint8_t *xfer_buffer_a = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    TEST_ASSERT(xfer_buffer_a != NULL);
    uint8_t *xfer_buffer_b = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    TEST_ASSERT(xfer_buffer_b != NULL);

    /* malloc frame buffer for a jpeg frame*/
    uint8_t *frame_buffer = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    TEST_ASSERT(frame_buffer != NULL);

    uvc_config_t uvc_config = {
        .xfer_type = UVC_XFER_ISOC,
        .dev_speed = USB_SPEED_FULL,
        .configuration = DESCRIPTOR_CONFIGURATION_INDEX,
        .format_index = DESCRIPTOR_FORMAT_MJPEG_INDEX,
        .frame_width = DEMO_FRAME_WIDTH,
        .frame_height = DEMO_FRAME_HEIGHT,
        .frame_index = DEMO_FRAME_INDEX,
        .frame_interval = DEMO_FRAME_INTERVAL,
        .interface = DESCRIPTOR_STREAM_INTERFACE_INDEX,
        .interface_alt = DEMO_ISOC_INTERFACE_ALT,
        .isoc_ep_addr = DESCRIPTOR_STREAM_ENDPOINT_ADDR,
        .isoc_ep_mps = DEMO_ISOC_EP_MPS,
        .xfer_buffer_size = DEMO_XFER_BUFFER_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = DEMO_XFER_BUFFER_SIZE,
        .frame_buffer = frame_buffer,
    };
    size_t test_count = 10;
    for (size_t i = 0; i < test_count; i++) {
        /* pre-config UVC driver with params from known USB Camera Descriptors*/
        TEST_ASSERT_EQUAL(ESP_OK, uvc_streaming_config(&uvc_config));

        /* Start camera IN stream with pre-configs, uvc driver will create multi-tasks internal
        to handle usb data from different pipes, and user's callback will be called after new frame ready. */
        TEST_ASSERT_EQUAL(ESP_OK, uvc_streaming_start(frame_cb, NULL));
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        /* test streaming suspend */
        TEST_ASSERT_EQUAL(ESP_OK, uvc_streaming_suspend());
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        /* test streaming resume */
        TEST_ASSERT_EQUAL(ESP_OK, uvc_streaming_resume());
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        /* test streaming stop */
        TEST_ASSERT_EQUAL(ESP_OK, uvc_streaming_stop());
    }

    _free(xfer_buffer_a);
    _free(xfer_buffer_b);
    _free(frame_buffer);
}

TEST_CASE("test uvc cdc streaming", "[usb][uvc_stream]")
{
    /* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
    uint8_t *xfer_buffer_a = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    TEST_ASSERT(xfer_buffer_a != NULL);
    uint8_t *xfer_buffer_b = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    TEST_ASSERT(xfer_buffer_b != NULL);

    /* malloc frame buffer for a jpeg frame*/
    uint8_t *frame_buffer = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    TEST_ASSERT(frame_buffer != NULL);

    uvc_config_t uvc_config = {
        .xfer_type = UVC_XFER_BULK,
        .dev_speed = USB_SPEED_FULL,
        .configuration = DESCRIPTOR_CONFIGURATION_INDEX,
        .format_index = DESCRIPTOR_FORMAT_MJPEG_INDEX,
        .frame_width = 640,
        .frame_height = 480,
        .frame_index = 2,
        .frame_interval = DEMO_FRAME_INTERVAL,
        .interface = DESCRIPTOR_STREAM_INTERFACE_INDEX,
        .interface_alt = 0,
        .isoc_ep_addr = DESCRIPTOR_STREAM_ENDPOINT_ADDR,
        .isoc_ep_mps = DEMO_BULK_EP_MPS,
        .xfer_buffer_size = DEMO_XFER_BUFFER_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = DEMO_XFER_BUFFER_SIZE,
        .frame_buffer = frame_buffer,
    };
    size_t test_count = 20;
    for (size_t i = 0; i < test_count; i++) {
        /* pre-config UVC driver with params from known USB Camera Descriptors*/
        TEST_ASSERT_EQUAL(ESP_OK, uvc_streaming_config(&uvc_config));

        /* Start camera IN stream with pre-configs, uvc driver will create multi-tasks internal
        to handle usb data from different pipes, and user's callback will be called after new frame ready. */
        TEST_ASSERT_EQUAL(ESP_OK, uvc_streaming_start(frame_cb, NULL));
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        /* test streaming stop */
        TEST_ASSERT_EQUAL(ESP_OK, uvc_streaming_stop());
    }

    _free(xfer_buffer_a);
    _free(xfer_buffer_b);
    _free(frame_buffer);
}