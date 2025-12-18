/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "usb_camera.h"

#define JPG_MIN_SIZE 40000
#define JPG_COMPRESSION_RATIO 5
#define max(a, b) ((a) > (b) ? (a) : (b))
#define UVC_DESC_DWFRAMEINTERVAL_TO_FPS(dwFrameInterval) (((dwFrameInterval) != 0) ? 10000000 / ((float)(dwFrameInterval)) : 0)
#define UVC_ESTIMATE_JPG_SIZE_B(width, height, bpp) (max(((width) * (height) * (bpp) / 8) / JPG_COMPRESSION_RATIO, JPG_MIN_SIZE))

#define EVENT_DISCONNECT ((1 << 2))
#define EVENT_START      ((1 << 3))
#define EVENT_STOP       ((1 << 4))
#define EVENT_ALL        (EVENT_DISCONNECT | EVENT_START | EVENT_STOP)

static const char *FORMAT_STR[] = {
    "FORMAT_UNDEFINED",
    "FORMAT_MJPEG",
    "FORMAT_YUY2",
    "FORMAT_H264",
    "FORMAT_H265",
};

static const char *TAG = "usb_camera";

typedef struct {
    size_t desired_width;
    size_t desired_height;
} cam_config_t;

typedef struct {
    uint16_t dev_addr;
    uint8_t uvc_stream_index;
    uvc_host_stream_hdl_t stream;
    EventGroupHandle_t event_group;
    QueueHandle_t frame_queue;
    uvc_host_frame_info_t frame_info;
    int require_frame_index;
    TaskHandle_t uvc_task_handle;
    bool is_streaming;
} uvc_dev_t;

static uvc_dev_t *g_uvc_dev = NULL;

static void usb_lib_task(void *pvParameter)
{
    usb_host_config_t host_config = {};
    host_config.skip_phy_setup = false;
    host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(static_cast<TaskHandle_t>(pvParameter));

    bool has_clients = true;
    bool has_devices = false;
    while (has_clients) {
        uint32_t event_flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "Get FLAGS_NO_CLIENTS");
            if (ESP_OK == usb_host_device_free_all()) {
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
    ESP_LOGI(TAG, "No more clients and devices, uninstall USB Host library");

    // Clean up USB Host
    vTaskDelay(100 / portTICK_PERIOD_MS); // Short delay to allow clients clean-up
    usb_host_uninstall();
    ESP_LOGD(TAG, "USB Host library is uninstalled");
    vTaskDelete(NULL);
}

void stream_callback(const uvc_host_stream_event_data_t *event, void *user_ctx)
{
    uvc_dev_t *dev = (uvc_dev_t *)user_ctx;
    switch (event->type) {
    case UVC_HOST_TRANSFER_ERROR:
        ESP_LOGE(TAG, "Cam USB error");
        break;
    case UVC_HOST_DEVICE_DISCONNECTED:
        ESP_LOGW(TAG, "Cam disconnected");
        xEventGroupSetBits(dev->event_group, EVENT_DISCONNECT);
        break;
    case UVC_HOST_FRAME_BUFFER_OVERFLOW:
        ESP_LOGW(TAG, "Cam Frame buffer overflow");
        break;
    case UVC_HOST_FRAME_BUFFER_UNDERFLOW:
        ESP_LOGD(TAG, "Cam Frame buffer underflow");
        break;
    default:
        abort();
        break;
    }
}

bool frame_callback(const uvc_host_frame_t *frame, void *user_ctx)
{
    uvc_dev_t *dev = (uvc_dev_t *)user_ctx;
    switch (frame->vs_format.format) {
    case UVC_VS_FORMAT_YUY2:
    case UVC_VS_FORMAT_H264:
    case UVC_VS_FORMAT_H265:
    case UVC_VS_FORMAT_MJPEG:
        ESP_LOGD(TAG, "Cam Frame received, size: %d", frame->data_len);
        break;
    default:
        ESP_LOGI(TAG, "Unsupported format!");
        break;
    }
    if (xQueueSend(dev->frame_queue, &frame, 0) != pdPASS) {
        return true;
    }

    // If we return false from this callback, we must return the frame with uvc_host_frame_return(stream, frame);
    return false;
}

static esp_err_t uvc_open(uvc_dev_t *dev, int frame_index)
{
    uvc_host_stream_config_t stream_config = {};
    stream_config.event_cb = stream_callback;
    stream_config.frame_cb = frame_callback;
    stream_config.user_ctx = dev;
    stream_config.usb.dev_addr = dev->dev_addr;
    stream_config.usb.uvc_stream_index = dev->uvc_stream_index;
    stream_config.vs_format.h_res = dev->frame_info.h_res;
    stream_config.vs_format.v_res = dev->frame_info.v_res;
    stream_config.vs_format.fps = UVC_DESC_DWFRAMEINTERVAL_TO_FPS(dev->frame_info.default_interval);
    stream_config.vs_format.format = dev->frame_info.format;
#if CONFIG_SPIRAM
    stream_config.advanced.number_of_frame_buffers = 6;
    stream_config.advanced.frame_heap_caps = MALLOC_CAP_SPIRAM;
#else
    stream_config.advanced.number_of_frame_buffers = 3;
    stream_config.advanced.freame_heap_caps = 0;
#endif
    stream_config.advanced.frame_size = UVC_ESTIMATE_JPG_SIZE_B(dev->frame_info.h_res, dev->frame_info.v_res, 16);
    stream_config.advanced.number_of_urbs = 3;
    stream_config.advanced.urb_size = 4 * 1024;

    ESP_LOGI(TAG, "Opening the UVC...");
    ESP_LOGI(TAG, "Config: dev_addr=%d, stream_index=%d, format=%d, res=%dx%d@%.1ffps",
             stream_config.usb.dev_addr, stream_config.usb.uvc_stream_index,
             stream_config.vs_format.format, stream_config.vs_format.h_res,
             stream_config.vs_format.v_res, stream_config.vs_format.fps);
    ESP_LOGI(TAG, "frame_size.advanced.frame_size: %d", stream_config.advanced.frame_size);
    esp_err_t err = uvc_host_stream_open(&stream_config, pdMS_TO_TICKS(5000), &dev->stream);
    if (ESP_OK != err) {
        ESP_LOGE(TAG, "Failed to open UVC: %s (0x%x)", esp_err_to_name(err), err);
        return err;
    }
    ESP_LOGI(TAG, "UVC stream opened successfully");

    // Select the frame format after opening the stream
    uvc_host_stream_format_t vs_format = {
        .h_res = dev->frame_info.h_res,
        .v_res = dev->frame_info.v_res,
        .fps = UVC_DESC_DWFRAMEINTERVAL_TO_FPS(dev->frame_info.default_interval),
        .format = dev->frame_info.format
    };
    ESP_ERROR_CHECK(uvc_host_stream_format_select(dev->stream, &vs_format));
    ESP_LOGI(TAG, "Frame format selected: %dx%d@%.1ffps", vs_format.h_res, vs_format.v_res, vs_format.fps);

    return ESP_OK;
}

static void uvc_task(void *pvParameter)
{
    uvc_dev_t *dev = g_uvc_dev;
    if (dev == NULL) {
        ESP_LOGE(TAG, "uvc_task: g_uvc_dev is NULL, exiting");
        vTaskDelete(NULL);
        return;
    }

    // Save task handle for external cleanup
    dev->uvc_task_handle = xTaskGetCurrentTaskHandle();
    dev->is_streaming = false;

    // Create event group and frame queue
    dev->event_group = xEventGroupCreate();
    dev->frame_queue = xQueueCreate(2, sizeof(uvc_host_frame_t *));

    if (dev->event_group == NULL || dev->frame_queue == NULL) {
        ESP_LOGE(TAG, "uvc_task: Failed to create event_group or frame_queue");
        if (dev->event_group != NULL) {
            vEventGroupDelete(dev->event_group);
            dev->event_group = NULL;
        }
        if (dev->frame_queue != NULL) {
            vQueueDelete(dev->frame_queue);
            dev->frame_queue = NULL;
        }
        dev->uvc_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "uvc_task: Opening the UVC device");
    esp_err_t open_err = uvc_open(dev, dev->require_frame_index);
    if (open_err != ESP_OK) {
        ESP_LOGE(TAG, "uvc_task: Failed to open UVC device: %s", esp_err_to_name(open_err));
        // Clean up and exit
        vEventGroupDelete(dev->event_group);
        vQueueDelete(dev->frame_queue);
        dev->event_group = NULL;
        dev->frame_queue = NULL;
        dev->uvc_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Auto start the stream after successful opening
    ESP_LOGI(TAG, "uvc_task: Auto starting camera stream");
    xEventGroupSetBits(dev->event_group, EVENT_START);

    bool exit = false;
    while (!exit) {
        EventBits_t uxBits = xEventGroupWaitBits(dev->event_group, EVENT_ALL, pdTRUE, pdFALSE, portMAX_DELAY);

        if (uxBits & EVENT_START) {
            if (!dev->is_streaming) {
                ESP_LOGI(TAG, "uvc_task: Starting stream");
                esp_err_t ret = uvc_host_stream_start(dev->stream);
                if (ret == ESP_OK) {
                    dev->is_streaming = true;
                    ESP_LOGI(TAG, "uvc_task: Stream started successfully");
                } else {
                    ESP_LOGE(TAG, "uvc_task: Failed to start stream: %s", esp_err_to_name(ret));
                }
            }
        }

        if (uxBits & EVENT_STOP) {
            if (dev->is_streaming) {
                ESP_LOGI(TAG, "uvc_task: Stopping stream");
                esp_err_t ret = uvc_host_stream_stop(dev->stream);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "uvc_task: Failed to stop stream: %s", esp_err_to_name(ret));
                }
                dev->is_streaming = false;

                // Return all pending frames
                uvc_host_frame_t *frame = NULL;
                while (xQueueReceive(dev->frame_queue, &frame, 0) == pdTRUE) {
                    uvc_host_frame_return(dev->stream, frame);
                }
                ESP_LOGI(TAG, "uvc_task: Stream stopped and frames returned");
            }
        }

        if (uxBits & EVENT_DISCONNECT) {
            ESP_LOGI(TAG, "uvc_task: Received disconnect event, starting graceful shutdown");
            exit = true;
        }
    }

    // Step 1: Stop streaming if still active
    if (dev->is_streaming) {
        ESP_LOGI(TAG, "uvc_task: Stopping stream before cleanup");
        esp_err_t ret = uvc_host_stream_stop(dev->stream);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "uvc_task: Failed to stop stream during cleanup: %s", esp_err_to_name(ret));
        }
        dev->is_streaming = false;
        vTaskDelay(100 / portTICK_PERIOD_MS); // Give time for stream to stop
    }

    // Step 2: Return all pending frames
    ESP_LOGI(TAG, "uvc_task: Returning pending frames");
    uvc_host_frame_t *frame = NULL;
    int frame_count = 0;
    while (xQueueReceive(dev->frame_queue, &frame, 0) == pdTRUE) {
        uvc_host_frame_return(dev->stream, frame);
        frame_count++;
    }
    if (frame_count > 0) {
        ESP_LOGI(TAG, "uvc_task: Returned %d pending frames", frame_count);
    }

    // Step 3: Close the stream
    ESP_LOGI(TAG, "uvc_task: Closing stream");
    if (dev->stream != NULL) {
        uvc_host_stream_close(dev->stream);
        dev->stream = NULL;
        vTaskDelay(50 / portTICK_PERIOD_MS); // Give time for stream to close
    }

    // Step 4: Clean up event group and queue
    ESP_LOGI(TAG, "uvc_task: Cleaning up event group and queue");
    if (dev->event_group != NULL) {
        vEventGroupDelete(dev->event_group);
        dev->event_group = NULL;
    }
    if (dev->frame_queue != NULL) {
        vQueueDelete(dev->frame_queue);
        dev->frame_queue = NULL;
    }

    // Step 5: Clear task handle
    dev->uvc_task_handle = NULL;
    ESP_LOGI(TAG, "uvc_task: Graceful shutdown complete, exiting");
    vTaskDelete(NULL);
}

void uvc_host_driver_event_cb(const uvc_host_driver_event_data_t *event, void *user_ctx)
{
    cam_config_t *config = (cam_config_t *)user_ctx;

    switch (event->type) {
    case UVC_HOST_DRIVER_EVENT_DEVICE_CONNECTED: {
        ESP_LOGI(TAG, "Device connected");

        // Clean up previous device if exists
        if (g_uvc_dev != NULL) {
            ESP_LOGW(TAG, "Previous device still exists, initiating graceful cleanup");
            uvc_dev_t *old_dev = g_uvc_dev;
            g_uvc_dev = NULL; // Clear global pointer first to prevent new operations

            // Notify old uvc_task to exit gracefully
            if (old_dev->event_group != NULL) {
                ESP_LOGI(TAG, "Notifying old uvc_task to exit");
                xEventGroupSetBits(old_dev->event_group, EVENT_DISCONNECT);
            }

            // Wait for old uvc_task to exit (with timeout)
            bool task_exited = false;
            if (old_dev->uvc_task_handle != NULL) {
                ESP_LOGI(TAG, "Waiting for old uvc_task to exit gracefully");
                // Wait up to 3 seconds for task to exit
                for (int i = 0; i < 30; i++) {
                    if (old_dev->uvc_task_handle == NULL) {
                        ESP_LOGI(TAG, "Old uvc_task exited gracefully");
                        task_exited = true;
                        break;
                    }
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }

                // Check if task is still running
                if (old_dev->uvc_task_handle != NULL) {
                    eTaskState task_state = eTaskGetState(old_dev->uvc_task_handle);
                    if (task_state != eDeleted && task_state != eInvalid) {
                        ESP_LOGW(TAG, "Old uvc_task did not exit in time (state=%d), force deleting", task_state);
                        // Force delete the task if it's still running
                        vTaskDelete(old_dev->uvc_task_handle);
                        old_dev->uvc_task_handle = NULL;
                        vTaskDelay(50 / portTICK_PERIOD_MS); // Give time for task deletion to complete
                    } else {
                        // Task is already deleted or invalid, just clear the handle
                        ESP_LOGI(TAG, "Old uvc_task handle is invalid, clearing");
                        old_dev->uvc_task_handle = NULL;
                    }
                }
            } else {
                task_exited = true;
            }

            // Clean up any remaining resources
            // Close stream first (safe even if task was force deleted)
            if (old_dev->stream != NULL) {
                ESP_LOGW(TAG, "Stream still exists, closing it");
                uvc_host_stream_close(old_dev->stream);
                old_dev->stream = NULL;
                vTaskDelay(50 / portTICK_PERIOD_MS); // Give time for stream to close
            }

            // Clean up event group and queue (safe now that task is deleted)
            if (old_dev->event_group != NULL) {
                vEventGroupDelete(old_dev->event_group);
                old_dev->event_group = NULL;
            }
            if (old_dev->frame_queue != NULL) {
                // Drain any remaining frames before deleting queue
                uvc_host_frame_t *frame = NULL;
                while (xQueueReceive(old_dev->frame_queue, &frame, 0) == pdTRUE) {
                    // Frame cannot be returned without stream, just log
                    ESP_LOGW(TAG, "Dropping frame from queue during cleanup");
                }
                vQueueDelete(old_dev->frame_queue);
                old_dev->frame_queue = NULL;
            }

            free(old_dev);
            ESP_LOGI(TAG, "Previous device cleanup complete");
        }

        g_uvc_dev = (uvc_dev_t *)calloc(1, sizeof(uvc_dev_t));
        assert(g_uvc_dev != NULL);

        // Initialize all fields
        g_uvc_dev->dev_addr = event->device_connected.dev_addr;
        g_uvc_dev->uvc_stream_index = event->device_connected.uvc_stream_index;
        g_uvc_dev->stream = NULL;
        g_uvc_dev->event_group = NULL;
        g_uvc_dev->frame_queue = NULL;
        g_uvc_dev->uvc_task_handle = NULL;
        g_uvc_dev->is_streaming = false;
        ESP_LOGI(TAG, "Cam uvc_stream_index = %d", g_uvc_dev->uvc_stream_index);

        size_t frame_info_num = event->device_connected.frame_info_num;
        uvc_host_frame_info_t *frame_info = (uvc_host_frame_info_t *)calloc(frame_info_num, sizeof(uvc_host_frame_info_t));
        assert(frame_info);

        uvc_host_get_frame_list(g_uvc_dev->dev_addr, g_uvc_dev->uvc_stream_index, (uvc_host_frame_info_t (*)[])frame_info, &frame_info_num);
        bool found_matching_resolution = false;

        for (int i = 0; i < frame_info_num; i++) {
            if (frame_info[i].format == UVC_VS_FORMAT_MJPEG) {
                ESP_LOGI(TAG, "Pick Cam %s %d*%d@%.1ffps", FORMAT_STR[frame_info[i].format], frame_info[i].h_res, frame_info[i].v_res, UVC_DESC_DWFRAMEINTERVAL_TO_FPS(frame_info[i].default_interval));

                // Check if this frame format matches the desired resolution
                if (frame_info[i].h_res == config->desired_width && frame_info[i].v_res == config->desired_height) {
                    g_uvc_dev->frame_info = frame_info[i];
                    g_uvc_dev->require_frame_index = i;
                    found_matching_resolution = true;
                    ESP_LOGI(TAG, "Found matching resolution: %dx%d, require_frame_index = %d",
                             config->desired_width, config->desired_height, g_uvc_dev->require_frame_index);
                }
            } else {
                ESP_LOGI(TAG, "Drop Cam %s %d*%d@%.1ffps", FORMAT_STR[frame_info[i].format], frame_info[i].h_res, frame_info[i].v_res, UVC_DESC_DWFRAMEINTERVAL_TO_FPS(frame_info[i].default_interval));
            }
        }

        free(frame_info);

        if (!found_matching_resolution) {
            ESP_LOGE(TAG, "No matching resolution %dx%d found for cam, destroy device",
                     config->desired_width, config->desired_height);
            free(g_uvc_dev);
            g_uvc_dev = NULL;
            return;
        }

        break;
    }
    default:
        break;
    }

    if (g_uvc_dev != NULL) {
        ESP_LOGI(TAG, "Creating uvc_task for new device");
        // Create uvc_task on CPU 1 with priority 7 to ensure it can run
        BaseType_t ret = xTaskCreatePinnedToCore(uvc_task, "uvc_task", 4096, NULL, 7, NULL, 1);
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create uvc_task");
            free(g_uvc_dev);
            g_uvc_dev = NULL;
        } else {
            ESP_LOGI(TAG, "uvc_task created successfully");
            // Give the task a chance to start
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    } else {
        ESP_LOGW(TAG, "g_uvc_dev is NULL, cannot create uvc_task");
    }
}

esp_err_t usb_camera_init(size_t width, size_t height)
{
    static cam_config_t cam_config = {};
    cam_config.desired_width = width;
    cam_config.desired_height = height;

    BaseType_t task_created = xTaskCreatePinnedToCore(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), 15, NULL, 1);
    assert(task_created == pdPASS);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    uvc_host_driver_config_t default_driver_config = {};
    default_driver_config.driver_task_stack_size = 5 * 1024;
    default_driver_config.driver_task_priority = 16;
    default_driver_config.xCoreID = 1;
    default_driver_config.create_background_task = true;
    default_driver_config.event_cb = uvc_host_driver_event_cb;
    default_driver_config.user_ctx = &cam_config;
    ESP_ERROR_CHECK(uvc_host_install(&default_driver_config));
    return ESP_OK;
}

uvc_host_frame_t *usb_camera_get_frame(void)
{
    if (g_uvc_dev == NULL || g_uvc_dev->frame_queue == NULL) {
        return NULL;
    }

    uvc_host_frame_t *frame = NULL;
    // Use non-blocking receive to avoid blocking the calling task
    if (xQueueReceive(g_uvc_dev->frame_queue, &frame, pdMS_TO_TICKS(1000)) == pdTRUE) {
        return frame;
    }
    return NULL;
}

esp_err_t usb_camera_return_frame(uvc_host_frame_t *frame)
{
    if (g_uvc_dev == NULL || g_uvc_dev->stream == NULL || frame == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return uvc_host_frame_return(g_uvc_dev->stream, frame);
}

esp_err_t usb_camera_restart_stream(void)
{
    if (g_uvc_dev == NULL || g_uvc_dev->stream == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = uvc_host_stream_stop(g_uvc_dev->stream);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS); // Give time for stream to stop

    ret = uvc_host_stream_start(g_uvc_dev->stream);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}
