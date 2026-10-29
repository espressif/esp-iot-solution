/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "app_web.h"
#include "app_uvc_manager.h"

#define APP_UVC_FRAME_QUEUE_LEN          2
#define APP_UVC_TASK_STACK_SIZE          4096
#define APP_UVC_FRAME_WAIT_MS            20
#define APP_UVC_DEFAULT_FRAME_BUFFERS    2
#define APP_UVC_JPG_MIN_SIZE             40000
#define APP_UVC_JPG_COMPRESSION_RATIO    6
#define APP_UVC_EVENT_START              BIT0
#define APP_UVC_EVENT_STOP               BIT1
#define APP_UVC_EVENT_DISCONNECT         BIT2
#define APP_UVC_EVENT_CONNECT            BIT3
#define APP_UVC_EVENT_ALL                (APP_UVC_EVENT_START | APP_UVC_EVENT_STOP | APP_UVC_EVENT_DISCONNECT | APP_UVC_EVENT_CONNECT)
#define APP_UVC_MAX(a, b)                ((a) > (b) ? (a) : (b))
#define APP_UVC_EST_JPG_SIZE(width, height, bpp) APP_UVC_MAX((((width) * (height) * (bpp) / 8) / APP_UVC_JPG_COMPRESSION_RATIO), APP_UVC_JPG_MIN_SIZE)
#define APP_UVC_INTERVAL_TO_FPS(interval) ((interval) != 0 ? 10000000.0f / (float)(interval) : 0.0f)

typedef struct {
    bool connected;
    bool enabled;
    bool streaming;
    uint8_t dev_addr;
    uint8_t stream_index;
    app_uvc_format_t selected_format;
    uint8_t selected_resolution;
    int active_resolution;
    uint8_t pending_dev_addr;
    uint8_t pending_stream_index;
    size_t pending_frame_info_num;
    size_t frame_info_num;
    uvc_host_frame_info_t *frame_info;
    uvc_host_stream_hdl_t stream;
    uint32_t frames_out;
    QueueHandle_t frame_queue;
    EventGroupHandle_t event_group;
    SemaphoreHandle_t lock;
} app_uvc_ctx_t;

static const char *TAG = "app_uvc";
static app_uvc_ctx_t s_cam = {
    .selected_format = APP_UVC_FORMAT_MJPEG,
    .active_resolution = -1,
};

static const char *uvc_format_name(app_uvc_format_t format)
{
    return format == APP_UVC_FORMAT_MJPEG ? "mjpeg" : "h264";
}

static int frame_buffer_count(size_t frame_size)
{
#if CONFIG_SPIRAM
    if (frame_size == 0) {
        ESP_LOGE(TAG, "Invalid UVC frame size");
        return APP_UVC_DEFAULT_FRAME_BUFFERS;
    }
    size_t free_spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t count = free_spiram / frame_size;
    if (count == 0) {
        ESP_LOGE(TAG, "Not enough SPIRAM for UVC frame buffer, free=%zu frame_size=%zu", free_spiram, frame_size);
        return 1;
    }
    return count > 8 ? 8 : (int)count;
#else
    (void)frame_size;
    return APP_UVC_DEFAULT_FRAME_BUFFERS;
#endif
}

static void free_frame_info_locked(void)
{
    free(s_cam.frame_info);
    s_cam.frame_info = NULL;
    s_cam.frame_info_num = 0;
    s_cam.selected_resolution = 0;
    s_cam.active_resolution = -1;
}

static uint8_t smallest_resolution_index(const uvc_host_frame_info_t *info, size_t num)
{
    uint8_t selected = 0;
    uint64_t selected_pixels = UINT64_MAX;
    for (size_t i = 0; i < num; i++) {
        uint64_t pixels = (uint64_t)info[i].h_res * info[i].v_res;
        if (pixels < selected_pixels) {
            selected_pixels = pixels;
            selected = (uint8_t)i;
        }
    }
    return selected;
}

static void drain_frame_queue(uvc_host_stream_hdl_t stream)
{
    if (!s_cam.frame_queue || !stream) {
        return;
    }
    uvc_host_frame_t *frame = NULL;
    while (xQueueReceive(s_cam.frame_queue, &frame, 0) == pdTRUE) {
        esp_err_t ret = uvc_host_frame_return(stream, frame);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Return queued UVC frame failed: %s", esp_err_to_name(ret));
        }
    }
}

static void wait_returned_frames(void)
{
    uint32_t waited_ms = 0;
    while (1) {
        xSemaphoreTake(s_cam.lock, portMAX_DELAY);
        uint32_t frames_out = s_cam.frames_out;
        xSemaphoreGive(s_cam.lock);
        if (frames_out == 0) {
            return;
        }

        vTaskDelay(pdMS_TO_TICKS(APP_UVC_FRAME_WAIT_MS));
        waited_ms += APP_UVC_FRAME_WAIT_MS;
        if (waited_ms > 5000) {
            ESP_LOGE(TAG, "Waited %" PRIu32 " ms for returned frames, but still have %" PRIu32 " frames not returned", waited_ms, frames_out);
            return;
        }
    }
}

static esp_err_t stop_stream(void)
{
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    uvc_host_stream_hdl_t stream = s_cam.stream;
    bool was_streaming = s_cam.streaming;
    s_cam.streaming = false;
    s_cam.enabled = false;
    s_cam.active_resolution = -1;
    xSemaphoreGive(s_cam.lock);

    if (!stream) {
        return ESP_OK;
    }

    esp_err_t final_ret = ESP_OK;
    if (was_streaming) {
        esp_err_t ret = uvc_host_stream_stop(stream);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Stop UVC stream failed: %s", esp_err_to_name(ret));
            final_ret = ret;
        }
    }
    drain_frame_queue(stream);
    wait_returned_frames();
    ESP_LOGI(TAG, "UVC stream stopped");
    return final_ret;
}

static esp_err_t close_stream(void)
{
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    uvc_host_stream_hdl_t stream = s_cam.stream;
    xSemaphoreGive(s_cam.lock);

    esp_err_t final_ret = stop_stream();
    if (!stream) {
        return final_ret;
    }

    esp_err_t ret = uvc_host_stream_close(stream);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Close UVC stream failed: %s", esp_err_to_name(ret));
        final_ret = ret;
        return final_ret;
    }
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    if (s_cam.stream == stream) {
        s_cam.stream = NULL;
    }
    xSemaphoreGive(s_cam.lock);
    ESP_LOGI(TAG, "UVC stream closed");
    return final_ret;
}

static bool frame_callback(const uvc_host_frame_t *frame, void *user_ctx)
{
    (void)user_ctx;
    if (!frame || !s_cam.frame_queue) {
        ESP_LOGE(TAG, "Invalid UVC frame callback argument");
        return true;
    }
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    bool streaming = s_cam.streaming;
    xSemaphoreGive(s_cam.lock);
    if (!streaming) {
        return true;
    }
    uvc_host_frame_t *queued_frame = (uvc_host_frame_t *)frame;
    if (xQueueSend(s_cam.frame_queue, &queued_frame, 0) != pdPASS) {
        return true;
    }
    return false;
}

static void stream_callback(const uvc_host_stream_event_data_t *event, void *user_ctx)
{
    (void)user_ctx;
    if (!event) {
        ESP_LOGE(TAG, "Invalid UVC stream event");
        return;
    }
    if (event->type == UVC_HOST_TRANSFER_ERROR) {
        ESP_LOGE(TAG, "UVC transfer error: %s", esp_err_to_name(event->transfer_error.error));
        app_web_send_error("uvc_transfer_error", esp_err_to_name(event->transfer_error.error));
    } else if (event->type == UVC_HOST_DEVICE_DISCONNECTED) {
        ESP_LOGI(TAG, "UVC device disconnected");
        xEventGroupSetBits(s_cam.event_group, APP_UVC_EVENT_DISCONNECT);
    } else if (event->type == UVC_HOST_FRAME_BUFFER_OVERFLOW) {
        ESP_LOGW(TAG, "UVC frame buffer overflow");
    }
}

static esp_err_t open_idle_stream(uint8_t resolution)
{
    // Keep an idle stream handle open so UVC disconnect events are delivered even when video is stopped.
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    if (!s_cam.connected || s_cam.stream || s_cam.selected_format != APP_UVC_FORMAT_MJPEG || resolution >= s_cam.frame_info_num) {
        xSemaphoreGive(s_cam.lock);
        ESP_LOGE(TAG, "Open UVC idle stream rejected, connected=%d stream=%p format=%s resolution=%u num=%u",
                 s_cam.connected, s_cam.stream, uvc_format_name(s_cam.selected_format), resolution, (unsigned)s_cam.frame_info_num);
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t dev_addr = s_cam.dev_addr;
    uint8_t stream_index = s_cam.stream_index;
    uvc_host_frame_info_t frame_info = s_cam.frame_info[resolution];
    xSemaphoreGive(s_cam.lock);

    size_t frame_size = APP_UVC_EST_JPG_SIZE(frame_info.h_res, frame_info.v_res, 16);
    const uvc_host_stream_config_t stream_config = {
        .event_cb = stream_callback,
        .frame_cb = frame_callback,
        .user_ctx = NULL,
        .usb = {
            .dev_addr = dev_addr,
            .vid = UVC_HOST_ANY_VID,
            .pid = UVC_HOST_ANY_PID,
            .uvc_stream_index = stream_index,
        },
        .vs_format = {
            .h_res = frame_info.h_res,
            .v_res = frame_info.v_res,
            .fps = APP_UVC_INTERVAL_TO_FPS(frame_info.default_interval),
            .format = frame_info.format,
        },
        .advanced = {
            .number_of_frame_buffers = frame_buffer_count(frame_size),
            .frame_size = frame_size,
#if CONFIG_SPIRAM
            .frame_heap_caps = MALLOC_CAP_SPIRAM,
#else
            .frame_heap_caps = 0,
#endif
            .number_of_urbs = 3,
            .urb_size = 10 * 1024,
        },
    };

    uvc_host_stream_hdl_t stream = NULL;
    esp_err_t ret = uvc_host_stream_open(&stream_config, pdMS_TO_TICKS(5000), &stream);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Open UVC stream failed: %s", esp_err_to_name(ret));
        return ret;
    }

    uvc_host_stream_format_t format = stream_config.vs_format;
    ret = uvc_host_stream_format_select(stream, &format);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Select UVC format failed: %s", esp_err_to_name(ret));
        (void)uvc_host_stream_close(stream);
        return ret;
    }

    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    s_cam.stream = stream;
    s_cam.streaming = false;
    s_cam.enabled = false;
    s_cam.active_resolution = resolution;
    xSemaphoreGive(s_cam.lock);
    ESP_LOGI(TAG, "UVC idle stream opened %ux%u@%.1ffps", frame_info.h_res, frame_info.v_res, APP_UVC_INTERVAL_TO_FPS(frame_info.default_interval));
    return ESP_OK;
}

static esp_err_t start_stream(void)
{
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    uvc_host_stream_hdl_t stream = s_cam.stream;
    int active_resolution = s_cam.active_resolution;
    if (!s_cam.connected || !stream || s_cam.selected_format != APP_UVC_FORMAT_MJPEG || active_resolution < 0 || (size_t)active_resolution >= s_cam.frame_info_num) {
        xSemaphoreGive(s_cam.lock);
        ESP_LOGE(TAG, "Start UVC stream rejected, connected=%d stream=%p format=%s active_resolution=%d num=%u",
                 s_cam.connected, stream, uvc_format_name(s_cam.selected_format), active_resolution, (unsigned)s_cam.frame_info_num);
        return ESP_ERR_INVALID_STATE;
    }
    uvc_host_frame_info_t frame_info = s_cam.frame_info[active_resolution];
    xSemaphoreGive(s_cam.lock);

    esp_err_t ret = uvc_host_stream_start(stream);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start UVC stream failed: %s", esp_err_to_name(ret));
        return ret;
    }

    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    s_cam.enabled = true;
    s_cam.streaming = true;
    xSemaphoreGive(s_cam.lock);
    ESP_LOGI(TAG, "UVC stream started %ux%u@%.1ffps", frame_info.h_res, frame_info.v_res, APP_UVC_INTERVAL_TO_FPS(frame_info.default_interval));
    return ESP_OK;
}

static void clear_camera_locked(void)
{
    s_cam.connected = false;
    s_cam.enabled = false;
    s_cam.streaming = false;
    s_cam.stream = NULL;
    s_cam.dev_addr = 0;
    s_cam.stream_index = 0;
    s_cam.selected_format = APP_UVC_FORMAT_MJPEG;
    s_cam.frames_out = 0;
    free_frame_info_locked();
}

static void handle_connect_event(void)
{
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    uint8_t dev_addr = s_cam.pending_dev_addr;
    uint8_t stream_index = s_cam.pending_stream_index;
    size_t total_num = s_cam.pending_frame_info_num;
    xSemaphoreGive(s_cam.lock);

    ESP_LOGI(TAG, "Handling UVC connect addr=%u stream_index=%u frame_num=%u", dev_addr, stream_index, (unsigned)total_num);
    (void)close_stream();

    uvc_host_frame_info_t *all_info = calloc(total_num, sizeof(uvc_host_frame_info_t));
    if (!all_info) {
        ESP_LOGE(TAG, "Allocate UVC frame list failed");
        xSemaphoreTake(s_cam.lock, portMAX_DELAY);
        clear_camera_locked();
        xSemaphoreGive(s_cam.lock);
        app_web_send_error("uvc_no_mem", "Allocate UVC frame list failed");
        app_web_broadcast_state();
        return;
    }

    esp_err_t ret = uvc_host_get_frame_list(dev_addr, stream_index, (uvc_host_frame_info_t (*)[])all_info, &total_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get UVC frame list failed: %s", esp_err_to_name(ret));
        free(all_info);
        xSemaphoreTake(s_cam.lock, portMAX_DELAY);
        clear_camera_locked();
        xSemaphoreGive(s_cam.lock);
        app_web_send_error("uvc_frame_list_failed", esp_err_to_name(ret));
        app_web_broadcast_state();
        return;
    }

    size_t mjpeg_num = 0;
    for (size_t i = 0; i < total_num; i++) {
        if (all_info[i].format == UVC_VS_FORMAT_MJPEG) {
            mjpeg_num++;
        }
    }

    if (mjpeg_num == 0) {
        ESP_LOGW(TAG, "Connected UVC device has no MJPEG frame");
        free(all_info);
        xSemaphoreTake(s_cam.lock, portMAX_DELAY);
        clear_camera_locked();
        xSemaphoreGive(s_cam.lock);
        app_web_send_error("uvc_no_mjpeg", "Connected UVC device has no MJPEG frame");
        app_web_broadcast_state();
        return;
    }

    uvc_host_frame_info_t *mjpeg_info = calloc(mjpeg_num, sizeof(uvc_host_frame_info_t));
    if (!mjpeg_info) {
        ESP_LOGE(TAG, "Allocate MJPEG frame list failed");
        free(all_info);
        xSemaphoreTake(s_cam.lock, portMAX_DELAY);
        clear_camera_locked();
        xSemaphoreGive(s_cam.lock);
        app_web_send_error("uvc_no_mem", "Allocate MJPEG frame list failed");
        app_web_broadcast_state();
        return;
    }

    size_t out = 0;
    for (size_t i = 0; i < total_num; i++) {
        if (all_info[i].format == UVC_VS_FORMAT_MJPEG) {
            mjpeg_info[out++] = all_info[i];
            ESP_LOGI(TAG, "Pick UVC MJPEG %ux%u@%.1ffps", all_info[i].h_res, all_info[i].v_res, APP_UVC_INTERVAL_TO_FPS(all_info[i].default_interval));
        }
    }
    free(all_info);

    uint8_t selected_resolution = smallest_resolution_index(mjpeg_info, mjpeg_num);
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    free_frame_info_locked();
    s_cam.connected = true;
    s_cam.enabled = false;
    s_cam.streaming = false;
    s_cam.dev_addr = dev_addr;
    s_cam.stream_index = stream_index;
    s_cam.selected_format = APP_UVC_FORMAT_MJPEG;
    s_cam.frame_info = mjpeg_info;
    s_cam.frame_info_num = mjpeg_num;
    s_cam.selected_resolution = selected_resolution;
    s_cam.active_resolution = -1;
    xSemaphoreGive(s_cam.lock);

    ret = open_idle_stream(selected_resolution);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Open UVC idle stream after connect failed: %s", esp_err_to_name(ret));
        xSemaphoreTake(s_cam.lock, portMAX_DELAY);
        clear_camera_locked();
        xSemaphoreGive(s_cam.lock);
        app_web_send_error("uvc_open_idle_failed", esp_err_to_name(ret));
    }
    app_web_broadcast_state();
}

static void uvc_task(void *arg)
{
    (void)arg;
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(s_cam.event_group, APP_UVC_EVENT_ALL, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & APP_UVC_EVENT_CONNECT) {
            handle_connect_event();
        }
        if (bits & APP_UVC_EVENT_STOP) {
            ESP_LOGI(TAG, "Stopping UVC stream");
            esp_err_t ret = stop_stream();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Stop UVC stream failed: %s", esp_err_to_name(ret));
            }
            app_web_broadcast_state();
        }
        if (bits & APP_UVC_EVENT_DISCONNECT) {
            ESP_LOGI(TAG, "Cleaning disconnected UVC device");
            (void)close_stream();
            xSemaphoreTake(s_cam.lock, portMAX_DELAY);
            clear_camera_locked();
            xSemaphoreGive(s_cam.lock);
            app_web_broadcast_state();
        }
        if (bits & APP_UVC_EVENT_START) {
            uint8_t resolution = 0;
            xSemaphoreTake(s_cam.lock, portMAX_DELAY);
            resolution = s_cam.selected_resolution;
            xSemaphoreGive(s_cam.lock);
            esp_err_t ret = ESP_OK;
            xSemaphoreTake(s_cam.lock, portMAX_DELAY);
            bool need_reopen = !s_cam.stream || s_cam.active_resolution != resolution;
            xSemaphoreGive(s_cam.lock);
            if (need_reopen) {
                ret = close_stream();
                if (ret == ESP_OK) {
                    ret = open_idle_stream(resolution);
                }
            }
            if (ret == ESP_OK) {
                ret = start_stream();
            }
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Start UVC by selected resolution failed: %s", esp_err_to_name(ret));
                app_web_send_error("uvc_start_failed", esp_err_to_name(ret));
            }
            app_web_broadcast_state();
        }
    }
}

static void driver_event_cb(const uvc_host_driver_event_data_t *event, void *user_ctx)
{
    (void)user_ctx;
    if (!event || event->type != UVC_HOST_DRIVER_EVENT_DEVICE_CONNECTED) {
        return;
    }

    // Defer control transfers to the UVC manager task. The UVC driver callback must return quickly.
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    s_cam.pending_dev_addr = event->device_connected.dev_addr;
    s_cam.pending_stream_index = event->device_connected.uvc_stream_index;
    s_cam.pending_frame_info_num = event->device_connected.frame_info_num;
    xSemaphoreGive(s_cam.lock);
    xEventGroupSetBits(s_cam.event_group, APP_UVC_EVENT_CONNECT);
}

esp_err_t app_uvc_manager_start(void)
{
    s_cam.lock = xSemaphoreCreateMutex();
    s_cam.event_group = xEventGroupCreate();
    s_cam.frame_queue = xQueueCreate(APP_UVC_FRAME_QUEUE_LEN, sizeof(uvc_host_frame_t *));
    if (!s_cam.lock || !s_cam.event_group || !s_cam.frame_queue) {
        ESP_LOGE(TAG, "Create UVC manager resources failed");
        return ESP_ERR_NO_MEM;
    }

    const uvc_host_driver_config_t config = {
        .driver_task_stack_size = 5 * 1024,
        .driver_task_priority = configMAX_PRIORITIES - 1,
        .xCoreID = 0,
        .create_background_task = true,
        .event_cb = driver_event_cb,
        .user_ctx = NULL,
    };
    esp_err_t ret = uvc_host_install(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UVC host install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if (xTaskCreatePinnedToCore(uvc_task, "uvc_mgr", APP_UVC_TASK_STACK_SIZE, NULL, 5, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "Create UVC manager task failed");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "UVC host installed");
    return ESP_OK;
}

esp_err_t app_uvc_get_state_json(cJSON *root)
{
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *camera = cJSON_AddObjectToObject(root, "camera");
    cJSON *formats = cJSON_AddArrayToObject(camera, "formats");
    cJSON *mjpeg = cJSON_CreateObject();
    cJSON *resolutions = cJSON_AddArrayToObject(mjpeg, "resolutions");
    if (!camera || !formats || !mjpeg || !resolutions) {
        ESP_LOGE(TAG, "Create UVC state JSON failed");
        cJSON_Delete(mjpeg);
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    cJSON_AddBoolToObject(camera, "connected", s_cam.connected);
    cJSON_AddBoolToObject(camera, "enabled", s_cam.enabled);
    cJSON_AddBoolToObject(camera, "streaming", s_cam.streaming);
    cJSON_AddStringToObject(camera, "selected_format", uvc_format_name(s_cam.selected_format));
    cJSON_AddNumberToObject(camera, "selected_resolution", s_cam.selected_resolution);
    cJSON_AddStringToObject(mjpeg, "format", "mjpeg");
    for (size_t i = 0; i < s_cam.frame_info_num; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            xSemaphoreGive(s_cam.lock);
            cJSON_Delete(mjpeg);
            ESP_LOGE(TAG, "Create UVC resolution JSON failed");
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddNumberToObject(item, "index", i);
        cJSON_AddNumberToObject(item, "width", s_cam.frame_info[i].h_res);
        cJSON_AddNumberToObject(item, "height", s_cam.frame_info[i].v_res);
        cJSON_AddNumberToObject(item, "fps", APP_UVC_INTERVAL_TO_FPS(s_cam.frame_info[i].default_interval));
        cJSON_AddItemToArray(resolutions, item);
    }
    xSemaphoreGive(s_cam.lock);
    cJSON_AddItemToArray(formats, mjpeg);
    return ESP_OK;
}

esp_err_t app_uvc_set_enabled(bool enabled)
{
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    bool connected = s_cam.connected;
    bool already = s_cam.enabled == enabled;
    bool has_frame = s_cam.frame_info_num > 0;
    xSemaphoreGive(s_cam.lock);
    if (!connected) {
        ESP_LOGW(TAG, "Set UVC enabled failed, camera is disconnected");
        return ESP_ERR_INVALID_STATE;
    }
    if (enabled && !has_frame) {
        ESP_LOGW(TAG, "Set UVC enabled failed, no MJPEG resolution");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (already) {
        app_web_broadcast_state();
        return ESP_OK;
    }
    xEventGroupSetBits(s_cam.event_group, enabled ? APP_UVC_EVENT_START : APP_UVC_EVENT_STOP);
    return ESP_OK;
}

esp_err_t app_uvc_set_format(app_uvc_format_t format, uint8_t resolution)
{
    if (format != APP_UVC_FORMAT_MJPEG) {
        ESP_LOGW(TAG, "UVC format is not supported yet: %s", uvc_format_name(format));
        return ESP_ERR_NOT_SUPPORTED;
    }
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    if (!s_cam.connected || resolution >= s_cam.frame_info_num) {
        xSemaphoreGive(s_cam.lock);
        ESP_LOGW(TAG, "Set UVC format failed format=%s resolution=%u", uvc_format_name(format), resolution);
        return ESP_ERR_INVALID_ARG;
    }
    if (s_cam.enabled) {
        xSemaphoreGive(s_cam.lock);
        ESP_LOGW(TAG, "Set UVC format failed while streaming");
        return ESP_ERR_INVALID_STATE;
    }
    s_cam.selected_format = format;
    s_cam.selected_resolution = resolution;
    xSemaphoreGive(s_cam.lock);
    esp_err_t ret = close_stream();
    if (ret == ESP_OK) {
        ret = open_idle_stream(resolution);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Reopen UVC idle stream failed format=%s resolution=%u: %s", uvc_format_name(format), resolution, esp_err_to_name(ret));
        app_web_send_error("uvc_reopen_idle_failed", esp_err_to_name(ret));
    }
    app_web_broadcast_state();
    return ret;
}

uvc_host_frame_t *app_uvc_get_frame(TickType_t wait_ticks)
{
    uvc_host_frame_t *frame = NULL;
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    bool can_read = s_cam.streaming && s_cam.selected_format == APP_UVC_FORMAT_MJPEG;
    xSemaphoreGive(s_cam.lock);
    if (!can_read || xQueueReceive(s_cam.frame_queue, &frame, wait_ticks) != pdTRUE) {
        return NULL;
    }
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    can_read = s_cam.streaming && s_cam.selected_format == APP_UVC_FORMAT_MJPEG && s_cam.stream != NULL;
    uvc_host_stream_hdl_t stream = s_cam.stream;
    if (!can_read) {
        xSemaphoreGive(s_cam.lock);
        if (stream) {
            esp_err_t ret = uvc_host_frame_return(stream, frame);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Return stale UVC frame failed: %s", esp_err_to_name(ret));
            }
        }
        return NULL;
    }
    s_cam.frames_out++;
    xSemaphoreGive(s_cam.lock);
    return frame;
}

esp_err_t app_uvc_return_frame(uvc_host_frame_t *frame)
{
    if (!frame) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    uvc_host_stream_hdl_t stream = s_cam.stream;
    xSemaphoreGive(s_cam.lock);
    if (!stream) {
        ESP_LOGW(TAG, "Drop UVC frame return because stream is closed");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = uvc_host_frame_return(stream, frame);
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    if (s_cam.frames_out > 0) {
        s_cam.frames_out--;
    }
    xSemaphoreGive(s_cam.lock);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Return UVC frame failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

bool app_uvc_is_streaming(void)
{
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    bool streaming = s_cam.streaming;
    xSemaphoreGive(s_cam.lock);
    return streaming;
}

app_uvc_format_t app_uvc_get_format(void)
{
    xSemaphoreTake(s_cam.lock, portMAX_DELAY);
    app_uvc_format_t format = s_cam.selected_format;
    xSemaphoreGive(s_cam.lock);
    return format;
}
