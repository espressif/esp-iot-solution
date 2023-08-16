/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "hal/usb_hal.h"
#include "esp_private/usb_phy.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "camera_config.h"
#include "camera_pin.h"
#include "esp_camera.h"
#if CONFIG_CAMERA_MODULE_ESP_S3_EYE
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "freertos/event_groups.h"
#include "bsp/esp-bsp.h"
#include "show_eyes.h"
static EventGroupHandle_t s_event_group = NULL;
#define EYES_CLOSE_BIT BIT0
#define EYES_OPEN_BIT  BIT1
#else
#pragma message("ESP-S3-EYE lcd animation not supported in ESP-IDF < v5.0")
#endif
#endif

static const char *TAG = "usb_webcam";
static usb_phy_handle_t phy_hdl;

static esp_err_t camera_init(uint32_t xclk_freq_hz, pixformat_t pixel_format, framesize_t frame_size, int jpeg_quality, uint8_t fb_count)
{
    static bool inited = false;
    static uint32_t cur_xclk_freq_hz = 0;
    static pixformat_t cur_pixel_format = 0;
    static framesize_t cur_frame_size = 0;
    static int cur_jpeg_quality = 0;
    static uint8_t cur_fb_count = 0;

    if ((inited && cur_xclk_freq_hz == xclk_freq_hz && cur_pixel_format == pixel_format
        && cur_frame_size== frame_size && cur_fb_count == fb_count && cur_jpeg_quality == jpeg_quality)) {
        ESP_LOGD(TAG, "camera already inited");
        return ESP_OK;
    } else if (inited) {
        esp_camera_return_all();
        esp_camera_deinit();
        inited = false;
        ESP_LOGI(TAG, "camera RESTART");
    }

    camera_config_t camera_config = {
        .pin_pwdn = CAMERA_PIN_PWDN,
        .pin_reset = CAMERA_PIN_RESET,
        .pin_xclk = CAMERA_PIN_XCLK,
        .pin_sscb_sda = CAMERA_PIN_SIOD,
        .pin_sscb_scl = CAMERA_PIN_SIOC,

        .pin_d7 = CAMERA_PIN_D7,
        .pin_d6 = CAMERA_PIN_D6,
        .pin_d5 = CAMERA_PIN_D5,
        .pin_d4 = CAMERA_PIN_D4,
        .pin_d3 = CAMERA_PIN_D3,
        .pin_d2 = CAMERA_PIN_D2,
        .pin_d1 = CAMERA_PIN_D1,
        .pin_d0 = CAMERA_PIN_D0,
        .pin_vsync = CAMERA_PIN_VSYNC,
        .pin_href = CAMERA_PIN_HREF,
        .pin_pclk = CAMERA_PIN_PCLK,

        .xclk_freq_hz = xclk_freq_hz,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = pixel_format,
        .frame_size = frame_size,

        .jpeg_quality = jpeg_quality,
        .fb_count = fb_count, 
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
        .fb_location = CAMERA_FB_IN_PSRAM
    };

    // initialize the camera sensor
    esp_err_t ret = esp_camera_init(&camera_config);
    if (ret != ESP_OK) {
        return ret;
    }

    // Get the sensor object, and then use some of its functions to adjust the parameters when taking a photo.
    // Note: Do not call functions that set resolution, set picture format and PLL clock,
    // If you need to reset the appeal parameters, please reinitialize the sensor.
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1); // flip it back
    // initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
        s->set_brightness(s, 1); // up the blightness just a bit
        s->set_saturation(s, -2); // lower the saturation
    }

    if (s->id.PID == OV3660_PID || s->id.PID == OV2640_PID) {
        s->set_vflip(s, 1); // flip it back    
    } else if (s->id.PID == GC0308_PID) {
        s->set_hmirror(s, 0);
    } else if (s->id.PID == GC032A_PID) {
        s->set_vflip(s, 1);
    }

    // Get the basic information of the sensor.
    camera_sensor_info_t *s_info = esp_camera_sensor_get_info(&(s->id));

    if (ESP_OK == ret && PIXFORMAT_JPEG == pixel_format && s_info->support_jpeg == true) {
        cur_xclk_freq_hz = xclk_freq_hz;
        cur_pixel_format = pixel_format;
        cur_frame_size = frame_size;
        cur_jpeg_quality = jpeg_quality;
        cur_fb_count = fb_count;
        inited = true;
    } else {
#if UVC_FORMAT_JPEG
        ESP_LOGE(TAG, "JPEG format is not supported");
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }

    return ret;
}

static void usb_phy_init(void)
{
    // Configure USB PHY
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
    };
    phy_conf.target = USB_PHY_TARGET_INT;
    usb_new_phy(&phy_conf, &phy_hdl);
}

static inline uint32_t get_time_millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void tusb_device_task(void *arg)
{
    while (1) {
        tud_task();
    }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+
// Invoked when device is mounted
void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "Mount");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "UN-Mount");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    ESP_LOGI(TAG, "Suspend");
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#if CONFIG_CAMERA_MODULE_ESP_S3_EYE
    xEventGroupSetBits(s_event_group, EYES_CLOSE_BIT);
#endif
#endif
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "Resume");
}

#if (CFG_TUD_VIDEO)
//--------------------------------------------------------------------+
// USB Video
//--------------------------------------------------------------------+
static uint8_t *frame_buffer = NULL;
static uint32_t interval_ms = 1000 / UVC_FRAME_RATE;
static TaskHandle_t uvc_task_hdl = NULL;

void video_task(void *arg)
{
    uint32_t start_ms = 0;
    uint32_t frame_num = 0;
    uint32_t frame_len = 0;
    uint32_t already_start = 0;
    uint32_t tx_busy = 0;
    camera_fb_t *pic = NULL;

    while (1) {
        if (!tud_video_n_streaming(0, 0)) {
            already_start  = 0;
            frame_num     = 0;
            tx_busy = 0;
            vTaskDelay(1);
            continue;
        }

        if (!already_start) {
            already_start = 1;
            start_ms = get_time_millis();
        }

        uint32_t cur = get_time_millis();
        if (cur - start_ms < interval_ms) {
            vTaskDelay(1);
            continue;
        }

        if (tx_busy) {
            uint32_t xfer_done = ulTaskNotifyTake(pdTRUE, 1);
            if (xfer_done == 0) {
                continue;
            }
            ++frame_num;
            tx_busy = 0;
        }

        start_ms += interval_ms;
        ESP_LOGD(TAG, "frame %" PRIu32 " taking picture...", frame_num);
        pic = esp_camera_fb_get();
        if(pic){
            ESP_LOGD(TAG, "Picture taken! Its size was: %zu bytes", pic->len);
        } else {
            ESP_LOGE(TAG, "Failed to capture picture");
            continue;
        }

        if (pic->len > UVC_FRAME_BUF_SIZE) {
            ESP_LOGW(TAG, "frame size is too big, dropping frame");
            esp_camera_fb_return(pic);
            continue;
        }
        frame_len = pic->len;
        memcpy(frame_buffer, pic->buf, frame_len);
        esp_camera_fb_return(pic);
        tx_busy = 1;
        tud_video_n_frame_xfer(0, 0, (void *)frame_buffer, frame_len);
        ESP_LOGD(TAG, "frame %" PRIu32 " transfer start, size %" PRIu32, frame_num, frame_len);
    }
}

void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx)
{
    (void)ctl_idx;
    (void)stm_idx;
    /* flip buffer */
    xTaskNotifyGive(uvc_task_hdl);
}

int tud_video_commit_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx,
                        video_probe_and_commit_control_t const *parameters)
{
    (void)ctl_idx;
    (void)stm_idx;
    /* convert unit to ms from 100 ns */
    ESP_LOGI(TAG, "bFrameIndex: %u", parameters->bFrameIndex);
    ESP_LOGI(TAG, "dwFrameInterval: %" PRIu32 "", parameters->dwFrameInterval);
    interval_ms = parameters->dwFrameInterval / 10000;
    if (parameters->bFrameIndex > UVC_FRAME_NUM) {
        return VIDEO_ERROR_OUT_OF_RANGE;
    }
    int frame_index = parameters->bFrameIndex - 1;
    esp_err_t ret = camera_init(20000000, PIXFORMAT_JPEG, UVC_FRAMES_INFO[frame_index].frame_size, UVC_FRAMES_INFO[frame_index].jpeg_quality, 2);
    if (ret!= ESP_OK) {
        ESP_LOGE(TAG, "camera init failed");
        return VIDEO_ERROR_OUT_OF_RANGE;
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#if CONFIG_CAMERA_MODULE_ESP_S3_EYE
    xEventGroupSetBits(s_event_group, EYES_OPEN_BIT);
#endif
#endif
    return VIDEO_ERROR_NONE;
}
#endif

void app_main(void)
{
#if CONFIG_CAMERA_MODULE_ESP_S3_EYE
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    bsp_display_start();
    bsp_display_backlight_on();
    lv_obj_t* img = eyes_init();
    s_event_group = xEventGroupCreate();
    xEventGroupSetBits(s_event_group, EYES_CLOSE_BIT);
#else
    ESP_LOGW(TAG, "ESP-S3-EYE lcd animation not supported in ESP-IDF < v5.0");
#endif
#endif
    ESP_LOGI(TAG, "Selected Camera Board %s", CAMERA_MODULE_NAME);
    frame_buffer = (uint8_t *)malloc(UVC_FRAME_BUF_SIZE);
    if (frame_buffer == NULL) {
        ESP_LOGE(TAG, "malloc frame buffer fail");
        return;
    }

    usb_phy_init();
    // init device stack on configured roothub port
    bool usb_init = tud_init(BOARD_TUD_RHPORT);
    if (!usb_init) {
        ESP_LOGE(TAG, "tud_init fail");
        return;
    }
    ESP_LOGI(TAG, "Format List");
    ESP_LOGI(TAG, "\tFormat(1) = %s", "MJPEG");
    ESP_LOGI(TAG, "Frame List");
    ESP_LOGI(TAG, "\tFrame(1) = %d * %d @%dfps, Quality = %d", UVC_FRAMES_INFO[0].width, UVC_FRAMES_INFO[0].height, UVC_FRAMES_INFO[0].rate, UVC_FRAMES_INFO[0].jpeg_quality);
#if CONFIG_CAMERA_MULTI_FRAMESIZE
    ESP_LOGI(TAG, "\tFrame(2) = %d * %d @%dfps, Quality = %d", UVC_FRAMES_INFO[1].width, UVC_FRAMES_INFO[1].height, UVC_FRAMES_INFO[1].rate, UVC_FRAMES_INFO[1].jpeg_quality);
    ESP_LOGI(TAG, "\tFrame(3) = %d * %d @%dfps, Quality = %d", UVC_FRAMES_INFO[2].width, UVC_FRAMES_INFO[2].height, UVC_FRAMES_INFO[2].rate, UVC_FRAMES_INFO[2].jpeg_quality);
#endif

    xTaskCreatePinnedToCore(tusb_device_task, "TinyUSB", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(video_task, "uvc", 4096, NULL, 4, &uvc_task_hdl, 0);
    while (1) {
#if CONFIG_CAMERA_MODULE_ESP_S3_EYE
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        static uint32_t close_time_ms = 0;
        static uint32_t open_time_ms = 0;
        EventBits_t bits = xEventGroupWaitBits(s_event_group, EYES_CLOSE_BIT | EYES_OPEN_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10));
        if (bits & EYES_CLOSE_BIT) {
            eyes_close(img);
            close_time_ms = 10;
            open_time_ms = 0;
            ESP_LOGI(TAG, "EYES CLOSE");
        } else if (bits & EYES_OPEN_BIT) {
            eyes_open(img);
            close_time_ms = 0;
            open_time_ms = 10;
            ESP_LOGI(TAG, "EYES OPEN");
        } else if (open_time_ms) {
            open_time_ms += 10;
            if (open_time_ms > 1000) {
                open_time_ms = 0;
                eyes_blink(img);
                ESP_LOGI(TAG, "EYES BLINK");
            }
        } else if (close_time_ms) {
            close_time_ms += 10;
            if (close_time_ms > 1000) {
                close_time_ms = 0;
                eyes_static(img);
                ESP_LOGI(TAG, "EYES STATIC");
            }
        }
#else
        vTaskDelay(pdMS_TO_TICKS(100));
#endif
#else
        vTaskDelay(pdMS_TO_TICKS(100));
#endif
    }
}
