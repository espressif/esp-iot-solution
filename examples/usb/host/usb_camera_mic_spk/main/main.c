// Copyright 2022-2024 Espressif Systems (Shanghai) PTE LTD
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
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_stream.h"

static const char *TAG = "uvc_mic_spk_demo";
/****************** configure the example working mode *******************************/
#define ENABLE_UVC_CAMERA_FUNCTION        1        /* enable uvc function */
#define ENABLE_UAC_MIC_SPK_FUNCTION       1        /* enable uac mic+spk function */
#if (ENABLE_UVC_CAMERA_FUNCTION)
#define ENABLE_UVC_FRAME_RESOLUTION_ANY   1        /* Using any resolution found from the camera */
#define ENABLE_UVC_WIFI_XFER              0        /* transfer uvc frame to wifi http */
#endif
#if (ENABLE_UAC_MIC_SPK_FUNCTION)
#define ENABLE_UAC_FEATURE_CONTROL        0        /* enable feature control(volume, mute) if the module support*/
#define ENABLE_UAC_MIC_SPK_LOOPBACK       0        /* transfer mic data to speaker */
#endif

#if (ENABLE_UVC_CAMERA_FUNCTION)
#if (ENABLE_UVC_FRAME_RESOLUTION_ANY)
#define DEMO_UVC_FRAME_WIDTH        FRAME_RESOLUTION_ANY
#define DEMO_UVC_FRAME_HEIGHT       FRAME_RESOLUTION_ANY
#else
#define DEMO_UVC_FRAME_WIDTH        480
#define DEMO_UVC_FRAME_HEIGHT       320
#endif
#define DEMO_UVC_XFER_BUFFER_SIZE   (35 * 1024) //Double buffer
#if (ENABLE_UVC_WIFI_XFER)
#include "app_wifi.h"
#include "app_httpd.h"
#include "esp_camera.h"

#define BIT0_FRAME_START     (0x01 << 0)
#define BIT1_NEW_FRAME_START (0x01 << 1)
#define BIT2_NEW_FRAME_END   (0x01 << 2)
static EventGroupHandle_t s_evt_handle;
static camera_fb_t s_fb = {0};

camera_fb_t* esp_camera_fb_get()
{
    xEventGroupSetBits(s_evt_handle, BIT0_FRAME_START);
    xEventGroupWaitBits(s_evt_handle, BIT1_NEW_FRAME_START, true, true, portMAX_DELAY);
    return &s_fb;
}

void esp_camera_fb_return(camera_fb_t * fb)
{
    xEventGroupSetBits(s_evt_handle, BIT2_NEW_FRAME_END);
    return;
}

static void camera_frame_cb(uvc_frame_t *frame, void *ptr)
{
    ESP_LOGI(TAG, "uvc callback! frame_format = %d, seq = %"PRIu32", width = %"PRIu32", height = %"PRIu32", length = %u, ptr = %d",
            frame->frame_format, frame->sequence, frame->width, frame->height, frame->data_bytes, (int) ptr);
    if (!(xEventGroupGetBits(s_evt_handle) & BIT0_FRAME_START)) {
        return;
    }

    switch (frame->frame_format) {
        case UVC_FRAME_FORMAT_MJPEG:
            s_fb.buf = frame->data;
            s_fb.len = frame->data_bytes;
            s_fb.width = frame->width;
            s_fb.height = frame->height;
            s_fb.buf = frame->data;
            s_fb.format = PIXFORMAT_JPEG;
            s_fb.timestamp.tv_sec = frame->sequence;
            xEventGroupSetBits(s_evt_handle, BIT1_NEW_FRAME_START);
            ESP_LOGV(TAG, "send frame = %"PRIu32"",frame->sequence);
            xEventGroupWaitBits(s_evt_handle, BIT2_NEW_FRAME_END, true, true, portMAX_DELAY);
            ESP_LOGV(TAG, "send frame done = %"PRIu32"",frame->sequence);
            break;
        default:
            ESP_LOGW(TAG, "Format not supported");
            assert(0);
            break;
    }
}
#else
static void camera_frame_cb(uvc_frame_t *frame, void *ptr)
{
    ESP_LOGI(TAG, "uvc callback! frame_format = %d, seq = %"PRIu32", width = %"PRIu32", height = %"PRIu32", length = %u, ptr = %d",
            frame->frame_format, frame->sequence, frame->width, frame->height, frame->data_bytes, (int) ptr);
}
#endif //ENABLE_UVC_WIFI_XFER
#endif //ENABLE_UVC_CAMERA_FUNCTION

#if (ENABLE_UAC_MIC_SPK_FUNCTION)
static void mic_frame_cb(mic_frame_t *frame, void *ptr)
{
    // We should using higher baudrate here, to reduce the blocking time here
    ESP_LOGD(TAG, "mic callback! bit_resolution = %u, samples_frequence = %"PRIu32", data_bytes = %"PRIu32,
            frame->bit_resolution, frame->samples_frequence, frame->data_bytes);
    // We should never block in mic callback!
#if (ENABLE_UAC_MIC_SPK_LOOPBACK)
    uac_spk_streaming_write(frame->data, frame->data_bytes, 0);
#endif //ENABLE_UAC_MIC_SPK_LOOPBACK
}
#endif //ENABLE_UAC_MIC_SPK_FUNCTION

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("httpd_txrx", ESP_LOG_INFO);
    esp_err_t ret = ESP_FAIL;

#if (ENABLE_UVC_CAMERA_FUNCTION)
#if (ENABLE_UVC_WIFI_XFER)
    s_evt_handle = xEventGroupCreate();
    if (s_evt_handle == NULL) {
        ESP_LOGE(TAG, "line-%u event group create failed", __LINE__);
        assert(0);
    }
    app_wifi_main();
    app_httpd_main();
#endif //ENABLE_UVC_WIFI_XFER
    /* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
    uint8_t *xfer_buffer_a = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(xfer_buffer_a != NULL);
    uint8_t *xfer_buffer_b = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(xfer_buffer_b != NULL);

    /* malloc frame buffer for a jpeg frame*/
    uint8_t *frame_buffer = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(frame_buffer != NULL);

    uvc_config_t uvc_config = {
        .frame_width = DEMO_UVC_FRAME_WIDTH,
        .frame_height = DEMO_UVC_FRAME_HEIGHT,
        .frame_interval = FPS2INTERVAL(15),
        .xfer_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
        .frame_buffer = frame_buffer,
        .frame_cb = &camera_frame_cb,
        .frame_cb_arg = NULL,
    };

    ret = uvc_streaming_config(&uvc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uvc streaming config failed");
    }
#endif

#if (ENABLE_UAC_MIC_SPK_FUNCTION)
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
    ret = uac_streaming_config(&uac_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uac streaming config failed");
    }
#endif

    /* Start stream with pre-configs, usb stream driver will create multi-tasks internal
    to handle usb data from different pipes, and user's callback will be called after new frame ready. */
    ret = usb_streaming_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "usb streaming start failed");
    }
    ESP_LOGI(TAG, "usb streaming start succeed");

// IF not enable loopback, play default sound
#if (ENABLE_UAC_MIC_SPK_FUNCTION && !ENABLE_UAC_MIC_SPK_LOOPBACK)
    // set the speaker volume to 10%
#if (ENABLE_UAC_FEATURE_CONTROL)
    usb_streaming_control(STREAM_UAC_SPK, CTRL_UAC_VOLUME, (void *)60);
#endif
    extern const uint8_t wave_array_32000_16_1[];
    extern const uint32_t s_buffer_size;
    int freq_offsite_step = 32000 / uac_config.spk_samples_frequence;
    int downsampling_bits = 16 - uac_config.spk_bit_resolution;
    const int buffer_ms = 400;
    const int buffer_size = buffer_ms * (uac_config.spk_bit_resolution / 8) * (uac_config.spk_samples_frequence / 1000);
    // if 8bit spk, declare uint8_t *d_buffer
    uint16_t *s_buffer = (uint16_t *)wave_array_32000_16_1;
    uint16_t *d_buffer = calloc(1, buffer_size);
    size_t offset_size = buffer_size / (uac_config.spk_bit_resolution / 8);

    while (1) {
        if ((uint32_t)(s_buffer + offset_size) >= (uint32_t)(wave_array_32000_16_1 + s_buffer_size)) {
            s_buffer = (uint16_t *)wave_array_32000_16_1;
            // mute the speaker
#if (ENABLE_UAC_FEATURE_CONTROL)
            usb_streaming_control(STREAM_UAC_SPK, CTRL_UAC_MUTE, (void *)1);
#endif
            vTaskDelay(pdMS_TO_TICKS(1000));
            // un-mute the speaker
#if (ENABLE_UAC_FEATURE_CONTROL)
            usb_streaming_control(STREAM_UAC_SPK, CTRL_UAC_MUTE, (void *)0);
#endif
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
#endif

    while(1) {
        vTaskDelay(100);
    }
}
