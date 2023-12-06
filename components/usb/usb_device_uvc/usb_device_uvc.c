/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_private/usb_phy.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "usb_device_uvc.h"

static const char *TAG = "usbd_uvc";

typedef struct {
    usb_phy_handle_t phy_hdl;
    uvc_format_t format;
    uvc_device_config_t user_config;
    TaskHandle_t uvc_task_hdl;
    uint32_t interval_ms;
} uvc_device_t;

static uvc_device_t s_uvc_device;

static void usb_phy_init(void)
{
    // Configure USB PHY
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
    };
    phy_conf.target = USB_PHY_TARGET_INT;
    usb_new_phy(&phy_conf, &s_uvc_device.phy_hdl);
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

void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "Mount");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "UN-Mount");
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    s_uvc_device.user_config.stop_cb(s_uvc_device.user_config.cb_ctx);
    ESP_LOGI(TAG, "Suspend");
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
static void video_task(void *arg)
{
    uint32_t start_ms = 0;
    uint32_t frame_num = 0;
    uint32_t frame_len = 0;
    uint32_t already_start = 0;
    uint32_t tx_busy = 0;
    uint8_t *uvc_buffer = s_uvc_device.user_config.uvc_buffer;
    uint32_t uvc_buffer_size = s_uvc_device.user_config.uvc_buffer_size;
    uvc_fb_t *pic = NULL;

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
        if (cur - start_ms < s_uvc_device.interval_ms) {
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

        start_ms += s_uvc_device.interval_ms;
        ESP_LOGD(TAG, "frame %" PRIu32 " taking picture...", frame_num);
        pic = s_uvc_device.user_config.fb_get_cb(s_uvc_device.user_config.cb_ctx);
        if (pic) {
            ESP_LOGD(TAG, "Picture taken! Its size was: %zu bytes", pic->len);
        } else {
            ESP_LOGE(TAG, "Failed to capture picture");
            continue;
        }

        if (pic->len > uvc_buffer_size) {
            ESP_LOGW(TAG, "frame size is too big, dropping frame");
            s_uvc_device.user_config.fb_return_cb(pic, s_uvc_device.user_config.cb_ctx);
            continue;
        }
        frame_len = pic->len;
        memcpy(uvc_buffer, pic->buf, frame_len);
        s_uvc_device.user_config.fb_return_cb(pic, s_uvc_device.user_config.cb_ctx);
        tx_busy = 1;
        tud_video_n_frame_xfer(0, 0, (void *)uvc_buffer, frame_len);
        ESP_LOGD(TAG, "frame %" PRIu32 " transfer start, size %" PRIu32, frame_num, frame_len);
    }
}

void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx)
{
    (void)ctl_idx;
    (void)stm_idx;
    xTaskNotifyGive(s_uvc_device.uvc_task_hdl);
}

int tud_video_commit_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx,
                        video_probe_and_commit_control_t const *parameters)
{
    (void)ctl_idx;
    (void)stm_idx;
    /* convert unit to ms from 100 ns */
    ESP_LOGI(TAG, "bFrameIndex: %u", parameters->bFrameIndex);
    ESP_LOGI(TAG, "dwFrameInterval: %" PRIu32 "", parameters->dwFrameInterval);
    s_uvc_device.interval_ms = parameters->dwFrameInterval / 10000;
    if (parameters->bFrameIndex > UVC_FRAME_NUM) {
        return VIDEO_ERROR_OUT_OF_RANGE;
    }
    int frame_index = parameters->bFrameIndex - 1;
    esp_err_t ret = s_uvc_device.user_config.start_cb(s_uvc_device.format, UVC_FRAMES_INFO[frame_index].width,
                                                      UVC_FRAMES_INFO[frame_index].height, UVC_FRAMES_INFO[frame_index].rate, s_uvc_device.user_config.cb_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "camera init failed");
        return VIDEO_ERROR_OUT_OF_RANGE;
    }
    return VIDEO_ERROR_NONE;
}
#endif

esp_err_t uvc_device_init(uvc_device_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ESP_RETURN_ON_FALSE(config->start_cb != NULL, ESP_ERR_INVALID_ARG, TAG, "start_cb is NULL");
    ESP_RETURN_ON_FALSE(config->fb_get_cb != NULL, ESP_ERR_INVALID_ARG, TAG, "fb_get_cb is NULL");
    ESP_RETURN_ON_FALSE(config->fb_return_cb != NULL, ESP_ERR_INVALID_ARG, TAG, "fb_return_cb is NULL");
    ESP_RETURN_ON_FALSE(config->stop_cb != NULL, ESP_ERR_INVALID_ARG, TAG, "stop_cb is NULL");
    ESP_RETURN_ON_FALSE(config->uvc_buffer != NULL, ESP_ERR_INVALID_ARG, TAG, "uvc_buffer is NULL");
    ESP_RETURN_ON_FALSE(config->uvc_buffer_size > 0, ESP_ERR_INVALID_ARG, TAG, "uvc_buffer_size is 0");
    s_uvc_device.user_config = *config;
    s_uvc_device.interval_ms = 1000 / UVC_FRAME_RATE;
#ifdef CONFIG_FORMAT_MJPEG
    s_uvc_device.format = UVC_FORMAT_JPEG;
#endif
    // init device stack on configured roothub port
    usb_phy_init();
    bool usb_init = tud_init(BOARD_TUD_RHPORT);
    if (!usb_init) {
        ESP_LOGE(TAG, "USB Device Stack Init Fail");
        return ESP_FAIL;
    }

    xTaskCreatePinnedToCore(tusb_device_task, "TinyUSB", 4096, NULL, 5, NULL, 0);
#if (CFG_TUD_VIDEO)
    xTaskCreatePinnedToCore(video_task, "UVC", 4096, NULL, 4, &s_uvc_device.uvc_task_hdl, 0);
#endif
    ESP_LOGI(TAG, "UVC Device Start, Version: %d.%d.%d", USB_DEVICE_UVC_VER_MAJOR, USB_DEVICE_UVC_VER_MINOR, USB_DEVICE_UVC_VER_PATCH);
    return ESP_OK;
}
