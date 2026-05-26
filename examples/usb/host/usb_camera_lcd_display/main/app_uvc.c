/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/queue.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb/uvc_host.h"
#include "app_uvc.h"

#if CONFIG_SPIRAM
#define NUMBER_OF_FRAME_BUFFERS 6 // Number of frames from the camera
#define UVC_FRAME_HEAP_CAPS MALLOC_CAP_SPIRAM
#else
#define NUMBER_OF_FRAME_BUFFERS 3 // Number of frames from the camera
#define UVC_FRAME_HEAP_CAPS 0
#endif

#define JPG_MIN_SIZE 40000
#define JPG_COMPRESSION_RATIO 6
#define UVC_FRAME_QUEUE_LEN 2
#define max(a, b) ((a) > (b) ? (a) : (b))
#define UVC_ESTIMATE_JPG_SIZE_B(width, height, bpp) (max(((width) * (height) * (bpp) / 8) / JPG_COMPRESSION_RATIO, JPG_MIN_SIZE))
#define UVC_DESC_DWFRAMEINTERVAL_TO_FPS(dwFrameInterval) (((dwFrameInterval) != 0) ? 10000000 / ((float)(dwFrameInterval)) : 0)

#define EVENT_DEVICE_CONNECTED BIT0

static const char *TAG = "uvc";

typedef struct {
    uvc_host_stream_hdl_t stream;
    uvc_host_frame_t *frame;
} uvc_frame_queue_item_t;

typedef struct {
    uint8_t dev_addr;
    uint8_t uvc_stream_index;
    bool if_streaming;
    uvc_host_stream_hdl_t stream;
    QueueHandle_t frame_queue;
    EventGroupHandle_t event_group;
    uint16_t max_width;
    uint16_t max_height;
    app_uvc_frame_cb_t frame_cb;
    void *frame_cb_user_ctx;
    size_t active_frame_index;
    size_t frame_info_num;
    uvc_host_frame_info_t *frame_info;
} uvc_dev_t;

static uvc_dev_t s_uvc_dev = {0};

static esp_err_t uvc_stream_start_with_index(size_t frame_index);
static esp_err_t uvc_stream_close(void);

static void usb_lib_task(void *arg)
{
    // Install USB Host driver. Should only be called once in entire application.
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(arg);

    bool has_clients = true;
    bool has_devices = false;
    while (has_clients) {
        uint32_t event_flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "Get FLAGS_NO_CLIENTS");
            if (usb_host_device_free_all() == ESP_OK) {
                ESP_LOGI(TAG, "All devices marked as free, no need to wait FLAGS_ALL_FREE event");
                has_clients = false;
            } else {
                ESP_LOGI(TAG, "Wait for the FLAGS_ALL_FREE");
                has_devices = true;
            }
        }
        if (has_devices && event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "Get FLAGS_ALL_FREE");
            has_clients = false;
        }
    }

    // Clean up USB Host.
    vTaskDelay(pdMS_TO_TICKS(100));
    usb_host_uninstall();
    ESP_LOGD(TAG, "USB Host library is uninstalled");
    vTaskDelete(NULL);
}

static void uvc_return_queued_frames(uvc_host_stream_hdl_t stream)
{
    uvc_frame_queue_item_t item = {0};
    while (xQueueReceive(s_uvc_dev.frame_queue, &item, 0) == pdTRUE) {
        if (item.frame == NULL) {
            continue;
        }

        esp_err_t err = uvc_host_frame_return(item.stream ? item.stream : stream, item.frame);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "UVC queued frame return failed: %s", esp_err_to_name(err));
        }
    }
}

static void uvc_disconnected_cleanup_task(void *arg)
{
    uvc_host_stream_hdl_t stream = (uvc_host_stream_hdl_t)arg;
    if (stream != NULL) {
        esp_err_t err = ESP_FAIL;
        for (int i = 0; i < 3; i++) {
            err = uvc_host_stream_close(stream);
            if (err == ESP_OK) {
                break;
            }
            ESP_LOGW(TAG, "UVC disconnected stream close retry %d failed: %s", i + 1, esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "UVC disconnected stream close failed permanently: %s", esp_err_to_name(err));
        }
    }
    vTaskDelete(NULL);
}

static void uvc_frame_task(void *arg)
{
    uvc_frame_queue_item_t item = {0};

    while (1) {
        if (xQueueReceive(s_uvc_dev.frame_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (item.frame != NULL) {
            if (s_uvc_dev.frame_cb != NULL) {
                s_uvc_dev.frame_cb(item.frame, s_uvc_dev.frame_cb_user_ctx);
            }
            esp_err_t err = uvc_host_frame_return(item.stream, item.frame);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "UVC frame return failed: %s", esp_err_to_name(err));
            }
        }
    }
}

static void stream_callback(const uvc_host_stream_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case UVC_HOST_TRANSFER_ERROR:
        ESP_LOGE(TAG, "UVC transfer error: %s", esp_err_to_name(event->transfer_error.error));
        break;
    case UVC_HOST_DEVICE_DISCONNECTED: {
        ESP_LOGW(TAG, "UVC device disconnected");
        s_uvc_dev.if_streaming = false;
        xEventGroupClearBits(s_uvc_dev.event_group, EVENT_DEVICE_CONNECTED);
        free(s_uvc_dev.frame_info);
        s_uvc_dev.frame_info = NULL;
        s_uvc_dev.frame_info_num = 0;
        uvc_host_stream_hdl_t disconnected_stream = event->device_disconnected.stream_hdl;
        if (disconnected_stream != NULL) {
            s_uvc_dev.stream = NULL;
            uvc_return_queued_frames(disconnected_stream);
            BaseType_t task_created = xTaskCreate(uvc_disconnected_cleanup_task, "uvc_disconnect", 4096, disconnected_stream, 5, NULL);
            if (task_created != pdPASS) {
                ESP_LOGE(TAG, "Create UVC disconnect cleanup task failed");
                (void)uvc_host_stream_close(disconnected_stream);
            }
        }
        break;
    }
    case UVC_HOST_FRAME_BUFFER_OVERFLOW:
        ESP_LOGW(TAG, "UVC frame buffer overflow");
        break;
    case UVC_HOST_FRAME_BUFFER_UNDERFLOW:
        ESP_LOGD(TAG, "UVC frame buffer underflow");
        break;
    default:
        ESP_LOGW(TAG, "Unhandled UVC stream event: %d", event->type);
        break;
    }
}

static bool frame_callback(const uvc_host_frame_t *frame, void *user_ctx)
{
    if (frame->vs_format.format != UVC_VS_FORMAT_MJPEG) {
        ESP_LOGW(TAG, "Drop unsupported UVC frame format: %d", frame->vs_format.format);
        return true;
    }

    if (s_uvc_dev.frame_queue == NULL || s_uvc_dev.stream == NULL) {
        ESP_LOGW(TAG, "Drop UVC frame because stream queue is not ready");
        return true;
    }

    uvc_frame_queue_item_t item = {
        .stream = s_uvc_dev.stream,
        .frame = (uvc_host_frame_t *)frame,
    };

    if (xQueueSend(s_uvc_dev.frame_queue, &item, 0) != pdPASS) {
        // Keep the callback short; drop the frame when the consumer task is still busy.
        return true;
    }

    // The frame is owned by uvc_frame_task now and must be returned with uvc_host_frame_return().
    return false;
}

static esp_err_t uvc_stream_close(void)
{
    if (s_uvc_dev.stream == NULL) {
        s_uvc_dev.if_streaming = false;
        return ESP_OK;
    }

    esp_err_t final_err = ESP_OK;
    if (s_uvc_dev.if_streaming) {
        esp_err_t err = uvc_host_stream_stop(s_uvc_dev.stream);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "UVC stream stop failed: %s", esp_err_to_name(err));
            final_err = err;
        }
    }

    uvc_return_queued_frames(s_uvc_dev.stream);

    esp_err_t close_err = ESP_FAIL;
    for (int i = 0; i < 3; i++) {
        close_err = uvc_host_stream_close(s_uvc_dev.stream);
        if (close_err == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "UVC stream close retry %d failed: %s", i + 1, esp_err_to_name(close_err));
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    if (close_err != ESP_OK) {
        ESP_LOGE(TAG, "UVC stream close failed permanently: %s", esp_err_to_name(close_err));
        return close_err;
    }

    s_uvc_dev.stream = NULL;
    s_uvc_dev.if_streaming = false;
    return final_err;
}

static size_t find_current_resolution(const app_uvc_frame_size_t *frame_size)
{
    if (s_uvc_dev.frame_info == NULL || s_uvc_dev.frame_info_num == 0) {
        ESP_LOGE(TAG, "No UVC frame list available");
        return SIZE_MAX;
    }

    if (frame_size == NULL || frame_size->width == 0 || frame_size->height == 0) {
        return 0;
    }

    size_t i = 0;
    while (i < s_uvc_dev.frame_info_num) {
        if (frame_size->width >= s_uvc_dev.frame_info[i].h_res && frame_size->height >= s_uvc_dev.frame_info[i].v_res) {
            break;
        } else if (i == s_uvc_dev.frame_info_num - 1) {
            break;
        }
        i++;
    }
    ESP_LOGI(TAG, "Current resolution is %ux%u", s_uvc_dev.frame_info[i].h_res, s_uvc_dev.frame_info[i].v_res);
    return i;
}

static esp_err_t uvc_stream_start_with_index(size_t frame_index)
{
    ESP_RETURN_ON_FALSE(s_uvc_dev.frame_info != NULL, ESP_ERR_INVALID_STATE, TAG, "UVC frame list is empty");
    ESP_RETURN_ON_FALSE(frame_index < s_uvc_dev.frame_info_num, ESP_ERR_INVALID_ARG, TAG, "Invalid UVC frame index: %u", frame_index);

    esp_err_t err = uvc_stream_close();
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to close previous UVC stream");

    const uvc_host_frame_info_t *frame_info = &s_uvc_dev.frame_info[frame_index];
    uvc_host_stream_config_t stream_config = {
        .event_cb = stream_callback,
        .frame_cb = frame_callback,
        .user_ctx = NULL,
        .usb = {
            .dev_addr = s_uvc_dev.dev_addr,
            .vid = UVC_HOST_ANY_VID,
            .pid = UVC_HOST_ANY_PID,
            .uvc_stream_index = s_uvc_dev.uvc_stream_index,
        },
        .vs_format = {
            .h_res = frame_info->h_res,
            .v_res = frame_info->v_res,
            .fps = UVC_DESC_DWFRAMEINTERVAL_TO_FPS(frame_info->default_interval),
            .format = frame_info->format,
        },
        .advanced = {
            .number_of_frame_buffers = NUMBER_OF_FRAME_BUFFERS,
            .frame_size = UVC_ESTIMATE_JPG_SIZE_B(frame_info->h_res, frame_info->v_res, 16),
            .frame_heap_caps = UVC_FRAME_HEAP_CAPS,
            .number_of_urbs = 3,
            .urb_size = 10 * 1024,
        },
    };

    ESP_LOGI(TAG, "Opening UVC stream %ux%u@%.1ffps", frame_info->h_res, frame_info->v_res, stream_config.vs_format.fps);
    err = uvc_host_stream_open(&stream_config, pdMS_TO_TICKS(5000), &s_uvc_dev.stream);
    ESP_RETURN_ON_ERROR(err, TAG, "UVC stream open failed");

    err = uvc_host_stream_format_select(s_uvc_dev.stream, &stream_config.vs_format);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UVC format select failed: %s", esp_err_to_name(err));
        (void)uvc_stream_close();
        return err;
    }

    err = uvc_host_stream_start(s_uvc_dev.stream);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UVC stream start failed: %s", esp_err_to_name(err));
        (void)uvc_stream_close();
        return err;
    }

    s_uvc_dev.if_streaming = true;
    s_uvc_dev.active_frame_index = frame_index;
    return ESP_OK;
}

static void driver_event_cb(const uvc_host_driver_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case UVC_HOST_DRIVER_EVENT_DEVICE_CONNECTED: {
        ESP_LOGI(TAG, "UVC device connected");
        if (xEventGroupGetBits(s_uvc_dev.event_group) & EVENT_DEVICE_CONNECTED) {
            ESP_LOGW(TAG, "Only one UVC camera is supported by this example");
            return;
        }

        s_uvc_dev.dev_addr = event->device_connected.dev_addr;
        s_uvc_dev.uvc_stream_index = event->device_connected.uvc_stream_index;
        size_t frame_info_num = event->device_connected.frame_info_num;
        uvc_host_frame_info_t *frame_info = (uvc_host_frame_info_t *)calloc(frame_info_num, sizeof(uvc_host_frame_info_t));
        if (frame_info == NULL) {
            ESP_LOGE(TAG, "No memory for UVC frame list");
            return;
        }

        esp_err_t err = uvc_host_get_frame_list(s_uvc_dev.dev_addr, s_uvc_dev.uvc_stream_index, (uvc_host_frame_info_t (*)[])frame_info, &frame_info_num);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Get UVC frame list failed: %s", esp_err_to_name(err));
            free(frame_info);
            return;
        }

        free(s_uvc_dev.frame_info);
        s_uvc_dev.frame_info = (uvc_host_frame_info_t *)calloc(frame_info_num, sizeof(uvc_host_frame_info_t));
        if (s_uvc_dev.frame_info == NULL) {
            ESP_LOGE(TAG, "No memory for filtered UVC frame list");
            free(frame_info);
            return;
        }

        size_t pick_frame_num = 0;
        for (size_t i = 0; i < frame_info_num; i++) {
            if (frame_info[i].format == UVC_VS_FORMAT_MJPEG && frame_info[i].h_res <= s_uvc_dev.max_width && frame_info[i].v_res <= s_uvc_dev.max_height) {
                s_uvc_dev.frame_info[pick_frame_num++] = frame_info[i];
                ESP_LOGI(TAG, "\tpick frame[%u] = %ux%u@%.1ffps", i, frame_info[i].h_res, frame_info[i].v_res, UVC_DESC_DWFRAMEINTERVAL_TO_FPS(frame_info[i].default_interval));
            } else {
                ESP_LOGI(TAG, "\tdrop frame[%u] = %ux%u format=%d", i, frame_info[i].h_res, frame_info[i].v_res, frame_info[i].format);
            }
        }
        free(frame_info);

        s_uvc_dev.frame_info_num = pick_frame_num;
        if (pick_frame_num == 0) {
            ESP_LOGE(TAG, "No supported MJPEG resolution not larger than display");
            free(s_uvc_dev.frame_info);
            s_uvc_dev.frame_info = NULL;
            return;
        }

        xEventGroupSetBits(s_uvc_dev.event_group, EVENT_DEVICE_CONNECTED);
        break;
    }
    default:
        ESP_LOGW(TAG, "Unhandled UVC driver event: %d", event->type);
        break;
    }
}

esp_err_t app_uvc_init(const app_uvc_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid UVC config");
    ESP_RETURN_ON_FALSE(config->frame_cb != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid UVC frame callback");

    s_uvc_dev.max_width = config->max_width;
    s_uvc_dev.max_height = config->max_height;
    s_uvc_dev.frame_cb = config->frame_cb;
    s_uvc_dev.frame_cb_user_ctx = config->user_ctx;

    s_uvc_dev.event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_uvc_dev.event_group != NULL, ESP_ERR_NO_MEM, TAG, "UVC event group create failed");

    s_uvc_dev.frame_queue = xQueueCreate(UVC_FRAME_QUEUE_LEN, sizeof(uvc_frame_queue_item_t));
    ESP_RETURN_ON_FALSE(s_uvc_dev.frame_queue != NULL, ESP_ERR_NO_MEM, TAG, "UVC frame queue create failed");

    BaseType_t task_created = xTaskCreate(uvc_frame_task, "uvc_frame", 4096, NULL, 4, NULL);
    ESP_RETURN_ON_FALSE(task_created == pdPASS, ESP_FAIL, TAG, "Create UVC frame task failed");

    task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), 15, NULL);
    ESP_RETURN_ON_FALSE(task_created == pdPASS, ESP_FAIL, TAG, "Create USB host lib task failed");
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // UVC driver install.
    const uvc_host_driver_config_t driver_config = {
        .driver_task_stack_size = 5 * 1024,
        .driver_task_priority = configMAX_PRIORITIES - 1,
        .xCoreID = 1,
        .create_background_task = true,
        .event_cb = driver_event_cb,
    };
    return uvc_host_install(&driver_config);
}

esp_err_t app_uvc_start(const app_uvc_frame_size_t *preferred_frame_size, app_uvc_frame_size_t *current_frame_size)
{
    xEventGroupWaitBits(s_uvc_dev.event_group, EVENT_DEVICE_CONNECTED, false, true, portMAX_DELAY);

    size_t frame_index = find_current_resolution(preferred_frame_size);
    ESP_RETURN_ON_FALSE(frame_index != SIZE_MAX, ESP_FAIL, TAG, "Find current resolution failed");

    esp_err_t err = uvc_stream_start_with_index(frame_index);
    ESP_RETURN_ON_ERROR(err, TAG, "Start UVC stream failed");

    if (current_frame_size != NULL) {
        current_frame_size->width = s_uvc_dev.frame_info[frame_index].h_res;
        current_frame_size->height = s_uvc_dev.frame_info[frame_index].v_res;
    }
    return ESP_OK;
}

esp_err_t app_uvc_switch_resolution(app_uvc_frame_size_t *current_frame_size)
{
    ESP_RETURN_ON_FALSE(s_uvc_dev.frame_info != NULL && s_uvc_dev.frame_info_num > 0, ESP_ERR_INVALID_STATE, TAG, "UVC camera is not ready");

    size_t frame_index = s_uvc_dev.active_frame_index + 1;
    if (frame_index >= s_uvc_dev.frame_info_num) {
        frame_index = 0;
    }

    esp_err_t err = uvc_stream_start_with_index(frame_index);
    ESP_RETURN_ON_ERROR(err, TAG, "Switch UVC resolution failed");

    if (current_frame_size != NULL) {
        current_frame_size->width = s_uvc_dev.frame_info[frame_index].h_res;
        current_frame_size->height = s_uvc_dev.frame_info[frame_index].v_res;
    }
    return ESP_OK;
}
