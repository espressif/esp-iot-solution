/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
#ifdef CONFIG_ESP32_S3_USB_OTG
#include "bsp/esp-bsp.h"
#endif

static const char *TAG = "uvc_mic_spk_demo";
/****************** configure the example working mode *******************************/
#define ENABLE_UVC_CAMERA_FUNCTION        1        /* enable uvc function */
#define ENABLE_UAC_MIC_SPK_FUNCTION       1        /* enable uac mic+spk function */
#if (ENABLE_UVC_CAMERA_FUNCTION)
#define ENABLE_UVC_FRAME_RESOLUTION_ANY   1        /* Using any resolution found from the camera */
#define ENABLE_UVC_WIFI_XFER              0        /* transfer uvc frame to wifi http */
#endif
#if (ENABLE_UAC_MIC_SPK_FUNCTION)
#define ENABLE_UAC_MIC_SPK_LOOPBACK       0        /* transfer mic data to speaker */
static uint32_t s_mic_samples_frequence = 0;
static uint32_t s_mic_ch_num = 0;
static uint32_t s_mic_bit_resolution = 0;
static uint32_t s_spk_samples_frequence = 0;
static uint32_t s_spk_ch_num = 0;
static uint32_t s_spk_bit_resolution = 0;
#endif

#define BIT0_FRAME_START     (0x01 << 0)
#define BIT1_NEW_FRAME_START (0x01 << 1)
#define BIT2_NEW_FRAME_END   (0x01 << 2)
#define BIT3_SPK_START       (0x01 << 3)
#define BIT4_SPK_RESET       (0x01 << 4)

static EventGroupHandle_t s_evt_handle;

#if (ENABLE_UVC_CAMERA_FUNCTION)
#if (ENABLE_UVC_FRAME_RESOLUTION_ANY)
#define DEMO_UVC_FRAME_WIDTH        FRAME_RESOLUTION_ANY
#define DEMO_UVC_FRAME_HEIGHT       FRAME_RESOLUTION_ANY
#else
#define DEMO_UVC_FRAME_WIDTH        480
#define DEMO_UVC_FRAME_HEIGHT       320
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S2
#define DEMO_UVC_XFER_BUFFER_SIZE (45 * 1024)
#else
#define DEMO_UVC_XFER_BUFFER_SIZE (55 * 1024)
#endif

#if (ENABLE_UVC_WIFI_XFER)
#include "app_wifi.h"
#include "app_httpd.h"
#include "esp_camera.h"

static camera_fb_t s_fb = {0};

camera_fb_t *esp_camera_fb_get()
{
    xEventGroupSetBits(s_evt_handle, BIT0_FRAME_START);
    xEventGroupWaitBits(s_evt_handle, BIT1_NEW_FRAME_START, true, true, portMAX_DELAY);
    return &s_fb;
}

void esp_camera_fb_return(camera_fb_t *fb)
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
        ESP_LOGV(TAG, "send frame = %"PRIu32"", frame->sequence);
        xEventGroupWaitBits(s_evt_handle, BIT2_NEW_FRAME_END, true, true, portMAX_DELAY);
        ESP_LOGV(TAG, "send frame done = %"PRIu32"", frame->sequence);
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

static void stream_state_changed_cb(usb_stream_state_t event, void *arg)
{
    switch (event) {
    case STREAM_CONNECTED: {
        size_t frame_size = 0;
        size_t frame_index = 0;
#if (ENABLE_UVC_CAMERA_FUNCTION)
        uvc_frame_size_list_get(NULL, &frame_size, &frame_index);
        if (frame_size) {
            ESP_LOGI(TAG, "UVC: get frame list size = %u, current = %u", frame_size, frame_index);
            uvc_frame_size_t *uvc_frame_list = (uvc_frame_size_t *)malloc(frame_size * sizeof(uvc_frame_size_t));
            uvc_frame_size_list_get(uvc_frame_list, NULL, NULL);
            for (size_t i = 0; i < frame_size; i++) {
                ESP_LOGI(TAG, "\tframe[%u] = %ux%u", i, uvc_frame_list[i].width, uvc_frame_list[i].height);
            }
            free(uvc_frame_list);
        } else {
            ESP_LOGW(TAG, "UVC: get frame list size = %u", frame_size);
        }
#endif
#if (ENABLE_UAC_MIC_SPK_FUNCTION)
        uac_frame_size_list_get(STREAM_UAC_MIC, NULL, &frame_size, &frame_index);
        if (frame_size) {
            ESP_LOGI(TAG, "UAC MIC: get frame list size = %u, current = %u", frame_size, frame_index);
            uac_frame_size_t *mic_frame_list = (uac_frame_size_t *)malloc(frame_size * sizeof(uac_frame_size_t));
            uac_frame_size_list_get(STREAM_UAC_MIC, mic_frame_list, NULL, NULL);
            for (size_t i = 0; i < frame_size; i++) {
                ESP_LOGI(TAG, "\t [%u] ch_num = %u, bit_resolution = %u, samples_frequence = %"PRIu32 ", samples_frequence_min = %"PRIu32 ", samples_frequence_max = %"PRIu32,
                         i, mic_frame_list[i].ch_num, mic_frame_list[i].bit_resolution, mic_frame_list[i].samples_frequence,
                         mic_frame_list[i].samples_frequence_min, mic_frame_list[i].samples_frequence_max);
            }
            s_mic_samples_frequence = mic_frame_list[frame_index].samples_frequence;
            s_mic_ch_num = mic_frame_list[frame_index].ch_num;
            s_mic_bit_resolution = mic_frame_list[frame_index].bit_resolution;
            if (s_mic_ch_num != 1) {
                ESP_LOGW(TAG, "UAC MIC: only support 1 channel in this example");
            }
            ESP_LOGI(TAG, "UAC MIC: use frame[%u] ch_num = %"PRIu32", bit_resolution = %"PRIu32", samples_frequence = %"PRIu32,
                     frame_index, s_mic_ch_num, s_mic_bit_resolution, s_mic_samples_frequence);
            free(mic_frame_list);
        } else {
            ESP_LOGW(TAG, "UAC MIC: get frame list size = %u", frame_size);
        }

        uac_frame_size_list_get(STREAM_UAC_SPK, NULL, &frame_size, &frame_index);
        if (frame_size) {
            ESP_LOGI(TAG, "UAC SPK: get frame list size = %u, current = %u", frame_size, frame_index);
            uac_frame_size_t *spk_frame_list = (uac_frame_size_t *)malloc(frame_size * sizeof(uac_frame_size_t));
            uac_frame_size_list_get(STREAM_UAC_SPK, spk_frame_list, NULL, NULL);
            for (size_t i = 0; i < frame_size; i++) {
                ESP_LOGI(TAG, "\t [%u] ch_num = %u, bit_resolution = %u, samples_frequence = %"PRIu32 ", samples_frequence_min = %"PRIu32 ", samples_frequence_max = %"PRIu32,
                         i, spk_frame_list[i].ch_num, spk_frame_list[i].bit_resolution, spk_frame_list[i].samples_frequence,
                         spk_frame_list[i].samples_frequence_min, spk_frame_list[i].samples_frequence_max);
            }
            if (s_spk_samples_frequence != spk_frame_list[frame_index].samples_frequence
                    || s_spk_ch_num != spk_frame_list[frame_index].ch_num
                    || s_spk_bit_resolution != spk_frame_list[frame_index].bit_resolution) {
                if (s_spk_samples_frequence) {
                    xEventGroupSetBits(s_evt_handle, BIT4_SPK_RESET);
                }
                s_spk_samples_frequence = spk_frame_list[frame_index].samples_frequence;
                s_spk_ch_num = spk_frame_list[frame_index].ch_num;
                s_spk_bit_resolution = spk_frame_list[frame_index].bit_resolution;
            }
            xEventGroupSetBits(s_evt_handle, BIT3_SPK_START);
            if (s_spk_ch_num != 1) {
                ESP_LOGW(TAG, "UAC SPK: only support 1 channel in this example");
            }
            ESP_LOGI(TAG, "UAC SPK: use frame[%u] ch_num = %"PRIu32", bit_resolution = %"PRIu32", samples_frequence = %"PRIu32,
                     frame_index, s_spk_ch_num, s_spk_bit_resolution, s_spk_samples_frequence);
            free(spk_frame_list);
        } else {
            ESP_LOGW(TAG, "UAC SPK: get frame list size = %u", frame_size);
        }
#endif
        ESP_LOGI(TAG, "Device connected");
        break;
    }
    case STREAM_DISCONNECTED:
        ESP_LOGI(TAG, "Device disconnected");
        break;
    default:
        ESP_LOGE(TAG, "Unknown event");
        break;
    }
}

void app_main(void)
{
#ifdef CONFIG_ESP32_S3_USB_OTG
    bsp_usb_mode_select_host();
    bsp_usb_host_power_mode(BSP_USB_HOST_POWER_MODE_USB_DEV, true);
#endif
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("httpd_txrx", ESP_LOG_INFO);
    esp_err_t ret = ESP_FAIL;
    s_evt_handle = xEventGroupCreate();
    if (s_evt_handle == NULL) {
        ESP_LOGE(TAG, "line-%u event group create failed", __LINE__);
        assert(0);
    }

#if (ENABLE_UVC_CAMERA_FUNCTION)
#if (ENABLE_UVC_WIFI_XFER)
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
        /* match the any resolution of current camera (first frame size as default) */
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
    /* config to enable uvc function */
    ret = uvc_streaming_config(&uvc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uvc streaming config failed");
    }
#endif

#if (ENABLE_UAC_MIC_SPK_FUNCTION)
    /* match any frequency of audio device we can found
     * call uac_frame_size_list_get to get the frame list of current audio device
     */
    uac_config_t uac_config = {
        .mic_bit_resolution = UAC_BITS_ANY,
        .mic_samples_frequence = UAC_FREQUENCY_ANY,
        .spk_bit_resolution = UAC_BITS_ANY,
        .spk_samples_frequence = UAC_FREQUENCY_ANY,
        .spk_buf_size = 16000,
        .mic_cb = &mic_frame_cb,
        .mic_cb_arg = NULL,
        /* Set flags to suspend speaker, user need call usb_streaming_control
        later to resume the speaker*/
        .flags = FLAG_UAC_SPK_SUSPEND_AFTER_START,
    };
    ret = uac_streaming_config(&uac_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uac streaming config failed");
    }
#endif
    /* register the state callback to get connect/disconnect event
    * in the callback, we can get the frame list of current device
    */
    ESP_ERROR_CHECK(usb_streaming_state_register(&stream_state_changed_cb, NULL));
    /* start usb streaming, UVC and UAC MIC will start streaming because SUSPEND_AFTER_START flags not set */
    ESP_ERROR_CHECK(usb_streaming_start());
    ESP_ERROR_CHECK(usb_streaming_connect_wait(portMAX_DELAY));
    // wait for speaker device ready
    xEventGroupWaitBits(s_evt_handle, BIT3_SPK_START, false, false, portMAX_DELAY);

    while (1) {
        xEventGroupWaitBits(s_evt_handle, BIT3_SPK_START, true, false, portMAX_DELAY);
        /* Manually resume the speaker because SUSPEND_AFTER_START flags is set */
        ESP_ERROR_CHECK(usb_streaming_control(STREAM_UAC_SPK, CTRL_RESUME, NULL));
        usb_streaming_control(STREAM_UAC_SPK, CTRL_UAC_VOLUME, (void *)80);
        usb_streaming_control(STREAM_UAC_MIC, CTRL_UAC_VOLUME, (void *)80);
        ESP_LOGI(TAG, "speaker resume");
#if (ENABLE_UAC_MIC_SPK_FUNCTION && !ENABLE_UAC_MIC_SPK_LOOPBACK)
        ESP_LOGI(TAG, "start to play default sound");
        extern const uint8_t wave_array_32000_16_1[];
        extern const uint32_t s_buffer_size;
        int freq_offsite_step = 32000 / s_spk_samples_frequence;
        int downsampling_bits = 16 - s_spk_bit_resolution;
        const int buffer_ms = 400;
        const int buffer_size = buffer_ms * (s_spk_bit_resolution / 8) * (s_spk_samples_frequence / 1000);
        // if 8bit spk, declare uint8_t *d_buffer
        uint16_t *s_buffer = (uint16_t *)wave_array_32000_16_1;
        uint16_t *d_buffer = calloc(1, buffer_size);
        size_t offset_size = buffer_size / (s_spk_bit_resolution / 8);
        while (1) {
            if ((uint32_t)(s_buffer + offset_size) >= (uint32_t)(wave_array_32000_16_1 + s_buffer_size)) {
                s_buffer = (uint16_t *)wave_array_32000_16_1;
                // mute the speaker
                vTaskDelay(pdMS_TO_TICKS(1000));
                // un-mute the speaker
            } else {
                // fill to usb buffer
                for (size_t i = 0; i < offset_size; i++) {
                    d_buffer[i] = *(s_buffer + i * freq_offsite_step) >> downsampling_bits;
                }
                // write to usb speaker
                uac_spk_streaming_write(d_buffer, buffer_size, pdMS_TO_TICKS(1000));
                s_buffer += offset_size * freq_offsite_step;
            }
            if (xEventGroupGetBits(s_evt_handle) & (BIT4_SPK_RESET | BIT3_SPK_START)) {
                // disconnect happens, we may need to reset the frequency of the speaker
                xEventGroupClearBits(s_evt_handle, BIT4_SPK_RESET);
                break;
            }
        }
        free(d_buffer);
#endif
    }

    while (1) {
        vTaskDelay(100);
    }
}
