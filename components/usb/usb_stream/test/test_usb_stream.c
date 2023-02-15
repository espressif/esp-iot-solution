/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "usb_stream.h"
#include "esp_log.h"

static const char *TAG = "uvc_test";

/* USB PIN fixed in esp32-s2, can not use io matrix */
#define BOARD_USB_DP_PIN 20
#define BOARD_USB_DN_PIN 19
#define ENABLE_UVC_FRAME_RESOLUTION_ANY 1

#if (ENABLE_UVC_FRAME_RESOLUTION_ANY)
#define DEMO_UVC_FRAME_WIDTH        FRAME_RESOLUTION_ANY
#define DEMO_UVC_FRAME_HEIGHT       FRAME_RESOLUTION_ANY
#else
#define DEMO_UVC_FRAME_WIDTH        320
#define DEMO_UVC_FRAME_HEIGHT       240
#endif
#define DEMO_XFER_BUFFER_SIZE (35 * 1024)

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
    ESP_LOGI(TAG, "uvc callback! frame_format = %d, seq = %"PRIu32", width = %"PRIu32", height = %"PRIu32", length = %u, ptr = %d",
            frame->frame_format, frame->sequence, frame->width, frame->height, frame->data_bytes, (int) ptr);

    switch (frame->frame_format) {
        case UVC_FRAME_FORMAT_MJPEG:
            break;
        default:
            break;
    }
}

TEST_CASE("test uvc streaming", "[usb][usb_stream][uvc]")
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
    /* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
    uint8_t *xfer_buffer_a = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    TEST_ASSERT(xfer_buffer_a != NULL);
    uint8_t *xfer_buffer_b = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    TEST_ASSERT(xfer_buffer_b != NULL);

    /* malloc frame buffer for a jpeg frame*/
    uint8_t *frame_buffer = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    TEST_ASSERT(frame_buffer != NULL);

    uvc_config_t uvc_config = {
        .frame_width = DEMO_UVC_FRAME_WIDTH,
        .frame_height = DEMO_UVC_FRAME_HEIGHT,
        .frame_interval = FPS2INTERVAL(15),
        .xfer_buffer_size = DEMO_XFER_BUFFER_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = DEMO_XFER_BUFFER_SIZE,
        .frame_buffer = frame_buffer,
        .frame_cb = frame_cb,
        .frame_cb_arg = NULL,
    };
    size_t test_count = 5;
    for (size_t i = 0; i < test_count; i++) {
        /* pre-config UVC driver with params from known USB Camera Descriptors*/
        TEST_ASSERT_EQUAL(ESP_OK, uvc_streaming_config(&uvc_config));
        /* Start camera IN stream with pre-configs, uvc driver will create multi-tasks internal
        to handle usb data from different pipes, and user's callback will be called after new frame ready. */
        TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_start());
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        /* test streaming suspend */
        TEST_ASSERT_NOT_EQUAL(ESP_FAIL, usb_streaming_control(STREAM_UVC, CTRL_SUSPEND, NULL));
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        /* test streaming resume */
        TEST_ASSERT_NOT_EQUAL(ESP_FAIL, usb_streaming_control(STREAM_UVC, CTRL_RESUME, NULL));
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        /* test streaming stop */
        TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_stop());
    }

    _free(xfer_buffer_a);
    _free(xfer_buffer_b);
    _free(frame_buffer);
}

static void mic_frame_cb(mic_frame_t *frame, void *ptr)
{
    // We should using higher baudrate here, to reduce the blocking time here
    ESP_LOGV(TAG, "mic callback! bit_resolution = %u, samples_frequence = %"PRIu32", data_bytes = %"PRIu32,
            frame->bit_resolution, frame->samples_frequence, frame->data_bytes);
    // We should never block in mic callback!
    uac_spk_streaming_write(frame->data, frame->data_bytes, 0);
}

TEST_CASE("test uac mic spk loop", "[usb][usb_stream][uvc][isoc]")
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
    uac_config_t uac_config = {
        .mic_bit_resolution = 16,
        .mic_samples_frequence = 16000,
        .spk_bit_resolution = 16,
        .spk_samples_frequence = 16000,
        .spk_buf_size = 16000,
        .mic_min_bytes = 320,
        .mic_cb = &mic_frame_cb,
        .mic_cb_arg = NULL,
    };
    size_t test_count = 3;
    for (size_t i = 0; i < test_count; i++) {
        /* pre-config UAC driver with params from known USB Camera Descriptors*/
        TEST_ASSERT_EQUAL(ESP_OK, uac_streaming_config(&uac_config));
        /* Start camera IN stream with pre-configs, uvc driver will create multi-tasks internal
        to handle usb data from different pipes, and user's callback will be called after new frame ready. */
        TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_start());
        TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_control(STREAM_UAC_SPK, CTRL_UAC_VOLUME, (void *)(90/(i+1))));
        size_t test_count2 = 3;
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        while (--test_count2) {
            TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_control(STREAM_UAC_MIC, CTRL_SUSPEND, NULL));
            ESP_LOGI(TAG, "mic suspend");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_control(STREAM_UAC_SPK, CTRL_SUSPEND, NULL));
            ESP_LOGI(TAG, "speaker suspend");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_control(STREAM_UAC_SPK, CTRL_RESUME, NULL));
            ESP_LOGI(TAG, "speaker resume");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_control(STREAM_UAC_MIC, CTRL_RESUME, NULL));
            ESP_LOGI(TAG, "mic resume");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
        TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_stop());
        test_count--;
    }

}

TEST_CASE("test uac mic", "[usb][usb_stream][mic]")
{
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    uac_config_t uac_config = {
        .mic_bit_resolution = 16,
        .mic_samples_frequence = 16000,
        .mic_buf_size = 6400,
    };
    size_t loop_count = 5;
    for (size_t i = 0; i < loop_count; i++) {
        /* code */
        TEST_ASSERT_EQUAL(ESP_OK, uac_streaming_config(&uac_config));
        /* Start camera IN stream with pre-configs, uvc driver will create multi-tasks internal
        to handle usb data from different pipes, and user's callback will be called after new frame ready. */
        TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_start());
        size_t test_count = 10;
        size_t buffer_size = uac_config.mic_ep_mps * 50;
        size_t read_size = 0;
        uint8_t *buffer = malloc(buffer_size);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        for (size_t i = 0; i < test_count; i++) {
            /* pre-config UAC driver with params from known USB Camera Descriptors*/
            uac_mic_streaming_read(buffer, buffer_size, &read_size, portMAX_DELAY);
            vTaskDelay(49 / portTICK_PERIOD_MS);
            printf("rcv len: %u\n", read_size);
            test_count--;
        }
        free(buffer);
        TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_stop());
    }
}

TEST_CASE("test uac spk", "[usb][usb_stream][spk]")
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
    uac_config_t uac_config = {
        .spk_bit_resolution = 16,
        .spk_samples_frequence = 16000,
        .spk_buf_size = 16000,
    };
    extern const uint8_t wave_array_32000_16_1[];
    extern const uint32_t s_buffer_size;
    int freq_offsite_step = 32000 / uac_config.spk_samples_frequence;
    int downsampling_bits = 16 - uac_config.spk_bit_resolution;
    const int buffer_ms = 400;
    const int buffer_size = buffer_ms * (uac_config.spk_bit_resolution / 8) * (uac_config.spk_samples_frequence / 1000);
    // if 8bit spk, declare uint8_t *d_buffer
    size_t offset_size = buffer_size / (uac_config.spk_bit_resolution / 8);
    size_t test_count = 3;
    for (size_t i = 0; i < test_count; i++) {
        /* pre-config UAC driver with params from known USB Camera Descriptors*/
        TEST_ASSERT_EQUAL(ESP_OK, uac_streaming_config(&uac_config));
        /* Start camera IN stream with pre-configs, uvc driver will create multi-tasks internal
        to handle usb data from different pipes, and user's callback will be called after new frame ready. */
        TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_start());
        // if 8bit spk, declare uint8_t *d_buffer
        uint16_t *s_buffer = (uint16_t *)wave_array_32000_16_1;
        uint16_t *d_buffer = calloc(1, buffer_size);
        TEST_ASSERT_NOT_NULL(d_buffer);
        size_t spk_count = 3;
        while(spk_count) {
            TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_control(STREAM_UAC_SPK, CTRL_UAC_VOLUME, (void *)(90/spk_count)));
            TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_control(STREAM_UAC_SPK, CTRL_UAC_MUTE, (void *)0));
            while (1) {
                if ((uint32_t)(s_buffer + offset_size) >= (uint32_t)(wave_array_32000_16_1 + s_buffer_size)) {
                    s_buffer = (uint16_t *)wave_array_32000_16_1;
                    break;
                } else {
                    // fill to usb buffer
                    for (size_t i = 0; i < offset_size; i++) {
                        d_buffer[i] = *(s_buffer + i * freq_offsite_step) >> downsampling_bits;
                    }
                    // write to usb speaker
                    uac_spk_streaming_write(d_buffer, buffer_size, portMAX_DELAY);
                    s_buffer += offset_size * freq_offsite_step;
                }
            }
            spk_count--;
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_control(STREAM_UAC_SPK, CTRL_UAC_VOLUME, (void *)(0/spk_count)));
            TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_control(STREAM_UAC_SPK, CTRL_UAC_MUTE, (void *)1));
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        free(d_buffer);
        TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_stop());
        test_count--;
    }
}

TEST_CASE("test uvc+uac", "[usb][usb_stream][uvc][uac]")
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
    /* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
    uint8_t *xfer_buffer_a = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    TEST_ASSERT(xfer_buffer_a != NULL);
    uint8_t *xfer_buffer_b = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    TEST_ASSERT(xfer_buffer_b != NULL);

    /* malloc frame buffer for a jpeg frame*/
    uint8_t *frame_buffer = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    TEST_ASSERT(frame_buffer != NULL);

    uvc_config_t uvc_config = {
        .frame_width = DEMO_UVC_FRAME_WIDTH,
        .frame_height = DEMO_UVC_FRAME_HEIGHT,
        .frame_interval = FPS2INTERVAL(15),
        .xfer_buffer_size = DEMO_XFER_BUFFER_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = DEMO_XFER_BUFFER_SIZE,
        .frame_buffer = frame_buffer,
        .frame_cb = frame_cb,
        .frame_cb_arg = NULL,
    };

    uac_config_t uac_config = {
        .mic_bit_resolution = 16,
        .mic_samples_frequence = 16000,
        .spk_bit_resolution = 16,
        .spk_samples_frequence = 16000,
        .spk_buf_size = 16000,
        .mic_min_bytes = 320,
        .mic_cb = &mic_frame_cb,
        .mic_cb_arg = NULL,
    };

    size_t test_count = 3;
    for (size_t i = 0; i < test_count; i++) {
        /* pre-config UVC driver with params from known USB Camera Descriptors*/
        TEST_ASSERT_EQUAL(ESP_OK, uvc_streaming_config(&uvc_config));
        /* pre-config UAC driver with params from known USB Camera Descriptors*/
        TEST_ASSERT_EQUAL(ESP_OK, uac_streaming_config(&uac_config));
        /* Start camera IN stream with pre-configs, uvc driver will create multi-tasks internal
        to handle usb data from different pipes, and user's callback will be called after new frame ready. */
        TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_start());
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        /* test streaming suspend */
        TEST_ASSERT_NOT_EQUAL(ESP_FAIL, usb_streaming_control(STREAM_UVC, CTRL_SUSPEND, NULL));
        TEST_ASSERT_NOT_EQUAL(ESP_FAIL, usb_streaming_control(STREAM_UAC_SPK, CTRL_SUSPEND, NULL));
        TEST_ASSERT_NOT_EQUAL(ESP_FAIL, usb_streaming_control(STREAM_UAC_MIC, CTRL_SUSPEND, NULL));
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        /* test streaming resume */
        TEST_ASSERT_NOT_EQUAL(ESP_FAIL, usb_streaming_control(STREAM_UVC, CTRL_RESUME, NULL));
        TEST_ASSERT_NOT_EQUAL(ESP_FAIL, usb_streaming_control(STREAM_UAC_SPK, CTRL_RESUME, NULL));
        TEST_ASSERT_NOT_EQUAL(ESP_FAIL, usb_streaming_control(STREAM_UAC_MIC, CTRL_RESUME, NULL));
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        /* test streaming stop */
        TEST_ASSERT_EQUAL(ESP_OK, usb_streaming_stop());
    }

    _free(xfer_buffer_a);
    _free(xfer_buffer_b);
    _free(frame_buffer);
}
