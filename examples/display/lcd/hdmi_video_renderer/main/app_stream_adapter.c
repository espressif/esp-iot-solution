/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>  // For read/write functions
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "app_stream_adapter.h"
#include "app_extractor.h"
#include "driver/jpeg_decode.h"

static const char *TAG = "stream_adapter";

/* Task parameters */
#define EXTRACT_TASK_STACK_SIZE (4 * 1024)
#define EXTRACT_TASK_PRIORITY 5

/* Event group bits for task control */
#define EXTRACT_TASK_START_BIT      (1 << 0)  /*!< Start extraction task */
#define EXTRACT_TASK_STOP_BIT       (1 << 1)  /*!< Stop extraction task */
#define EXTRACT_TASK_STOPPED_BIT    (1 << 2)  /*!< Task has stopped */

/**
 * @brief Stream adapter context structure
 */
typedef struct app_stream_adapter_t {
    /* Common parameters */
    app_stream_frame_cb_t frame_cb;           /*!< Frame callback function */
    void *user_data;                          /*!< User data to be passed to frame callback */
    void **decode_buffers;                    /*!< Array of frame buffer pointers */
    uint32_t buffer_count;                    /*!< Number of frame buffers */
    uint32_t buffer_size;                     /*!< Size of each frame buffer */
    const char *filename;                     /*!< Current media filename */
    bool running;                             /*!< Running state flag */
    uint32_t current_buffer;                  /*!< Current buffer index */
    uint32_t frame_count;                     /*!< Number of frames processed */
    bool has_info;                            /*!< Flag indicating if stream info is available */
    uint32_t width;                           /*!< Frame width */
    uint32_t height;                          /*!< Frame height */
    uint32_t fps;                             /*!< Frames per second */
    uint32_t duration;                        /*!< Duration in milliseconds */

    /* Extractor specific members */
    app_extractor_handle_t extractor_handle;  /*!< Extractor handle */
    uint8_t *jpeg_buffer;                     /*!< Buffer for JPEG frames from extractor */
    uint32_t jpeg_buffer_size;                /*!< Size of JPEG buffer */
    jpeg_decoder_handle_t jpeg_handle;        /*!< JPEG hardware decoder handle */
    TaskHandle_t extract_task_handle;         /*!< Handle for extraction task */
    EventGroupHandle_t extract_event_group;   /*!< Event group for task control */
    SemaphoreHandle_t frame_mutex;            /*!< Mutex to protect frame buffer access */

    /* JPEG decoder configuration */
    app_stream_jpeg_config_t jpeg_config;     /*!< JPEG decoder configuration */

    // Audio support
    bool extract_audio;                       /*!< Flag to extract audio */
    esp_codec_dev_handle_t audio_dev;         /*!< Audio device handle */
} app_stream_adapter_t;

/**
 * Global adapter reference for callback
 */
static app_stream_adapter_t *g_adapter_instance = NULL;

/**
 * @brief Initialize JPEG hardware decoder
 *
 * @param adapter Stream adapter
 * @return ESP_OK on success, or an error code
 */
static esp_err_t jpeg_hw_init(app_stream_adapter_t *adapter)
{
    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .intr_priority = 0,
        .timeout_ms = 1000,
    };

    return jpeg_new_decoder_engine(&decode_eng_cfg, &adapter->jpeg_handle);
}

/**
 * @brief Decode a JPEG frame using hardware decoder
 *
 * @param adapter Stream adapter
 * @param input_buffer JPEG data buffer
 * @param input_size JPEG data size
 * @param out_width Pointer to store width
 * @param out_height Pointer to store height
 * @param out_size Pointer to store decoded size
 * @return ESP_OK on success, or an error code
 */
static esp_err_t decode_jpeg_frame(
    app_stream_adapter_t *adapter,
    const uint8_t *input_buffer,
    uint32_t input_size,
    uint32_t *out_width,
    uint32_t *out_height,
    uint32_t *out_size)
{
    esp_err_t ret;

    jpeg_decode_picture_info_t pic_info;
    ret = jpeg_decoder_get_info(input_buffer, input_size, &pic_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get JPEG info: %d", ret);
        return ret;
    }

    *out_width = pic_info.width;
    *out_height = pic_info.height;

    uint32_t bytes_per_pixel = (adapter->jpeg_config.output_format == APP_STREAM_JPEG_OUTPUT_RGB888) ? 3 : 2;
    uint32_t required_size = pic_info.width * pic_info.height * bytes_per_pixel;

    if (required_size > adapter->buffer_size) {
        ESP_LOGE(TAG, "Buffer too small: required=%u, available=%u", required_size, adapter->buffer_size);
        return ESP_ERR_NO_MEM;
    }

    // Select next buffer in round-robin fashion
    adapter->current_buffer = (adapter->current_buffer + 1) % adapter->buffer_count;
    void *current_decode_buffer = adapter->decode_buffers[adapter->current_buffer];

    jpeg_decode_cfg_t decode_cfg = {
        .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
    };

    // Set output format based on configuration
    switch (adapter->jpeg_config.output_format) {
    case APP_STREAM_JPEG_OUTPUT_RGB565:
        decode_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
        break;
    case APP_STREAM_JPEG_OUTPUT_RGB888:
        decode_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB888;
        break;
    default:
        decode_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
        break;
    }

    decode_cfg.rgb_order = adapter->jpeg_config.bgr_order ?
                           JPEG_DEC_RGB_ELEMENT_ORDER_BGR :
                           JPEG_DEC_RGB_ELEMENT_ORDER_RGB;

    ret = jpeg_decoder_process(adapter->jpeg_handle, &decode_cfg,
                               input_buffer, input_size,
                               current_decode_buffer, adapter->buffer_size,
                               out_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "JPEG decoding failed: %d", ret);
        return ret;
    }

    return ESP_OK;
}

// Extractor frame callback function
static esp_err_t extractor_frame_callback(uint8_t *buffer,
                                          uint32_t buffer_size,
                                          bool is_video,
                                          uint32_t pts)
{
    app_stream_adapter_t *adapter = g_adapter_instance;
    esp_err_t ret = ESP_OK;

    if (adapter == NULL) {
        ESP_LOGE(TAG, "Adapter not set for extractor callback");
        return ESP_ERR_INVALID_STATE;
    }

    // Process video frames only (audio handled in app_extractor)
    if (!is_video) {
        return ESP_OK;
    }

    if (buffer_size > adapter->jpeg_buffer_size) {
        ESP_LOGE(TAG, "JPEG frame too large: %" PRIu32 " > %" PRIu32, buffer_size, adapter->jpeg_buffer_size);
        return ESP_ERR_NO_MEM;
    }

    memcpy(adapter->jpeg_buffer, buffer, buffer_size);

    uint32_t width = 0, height = 0, decoded_size = 0;

    xSemaphoreTake(adapter->frame_mutex, portMAX_DELAY);

    ret = decode_jpeg_frame(adapter, adapter->jpeg_buffer, buffer_size,
                            &width, &height, &decoded_size);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to decode frame: %d", ret);
        xSemaphoreGive(adapter->frame_mutex);
        return ret;
    }

    // Store stream info if not available
    if (!adapter->has_info) {
        adapter->width = width;
        adapter->height = height;
        adapter->has_info = true;
    }

    adapter->frame_count++;

    if (adapter->frame_cb) {
        void *current_buffer = adapter->decode_buffers[adapter->current_buffer];
        uint32_t frame_index = (adapter->frame_count > 0) ? (adapter->frame_count - 1) : 0;
        ret = adapter->frame_cb(current_buffer, decoded_size, width, height, frame_index, adapter->user_data);
    }

    xSemaphoreGive(adapter->frame_mutex);
    return ret;
}

// Task that extracts and processes frames
static void extract_task(void *arg)
{
    app_stream_adapter_t *adapter = (app_stream_adapter_t *)arg;
    esp_err_t ret = ESP_OK;
    EventBits_t bits;

    ESP_LOGI(TAG, "Extract task started");

    while (1) {
        // Wait for start or stop events
        bits = xEventGroupWaitBits(adapter->extract_event_group,
                                   EXTRACT_TASK_START_BIT | EXTRACT_TASK_STOP_BIT,
                                   pdFALSE, pdFALSE, portMAX_DELAY);

        if (bits & EXTRACT_TASK_STOP_BIT) {
            ESP_LOGI(TAG, "Extract task received stop signal");
            break;
        }

        if (bits & EXTRACT_TASK_START_BIT) {
            ESP_LOGI(TAG, "Extract task processing frames");

            while (1) {
                // Check for stop signal with minimal timeout
                bits = xEventGroupWaitBits(adapter->extract_event_group,
                                           EXTRACT_TASK_STOP_BIT,
                                           pdFALSE, pdFALSE, 0);

                if (bits & EXTRACT_TASK_STOP_BIT) {
                    ESP_LOGI(TAG, "Extract task stopping frame processing");
                    break;
                }

                ret = app_extractor_read_frame(adapter->extractor_handle);

                if (ret != ESP_OK) {
                    if (ret == ESP_ERR_NOT_FOUND) {
                        ESP_LOGI(TAG, "End of stream reached");
                    } else {
                        ESP_LOGW(TAG, "Failed to read frame: %d", ret);
                    }
                    break;
                }
            }

            // Clear the start bit to indicate we've processed the start event
            xEventGroupClearBits(adapter->extract_event_group, EXTRACT_TASK_START_BIT);
        }
    }

    ESP_LOGI(TAG, "Extract task stopped");

    // Set stopped bit to indicate task has finished
    xEventGroupSetBits(adapter->extract_event_group, EXTRACT_TASK_STOPPED_BIT);

    adapter->extract_task_handle = NULL;
    vTaskDelete(NULL);
}

// Start extraction task
static esp_err_t start_extract_task(app_stream_adapter_t *adapter)
{
    if (adapter->extract_task_handle != NULL) {
        // Task is already running, just set start bit
        xEventGroupSetBits(adapter->extract_event_group, EXTRACT_TASK_START_BIT);
        return ESP_OK;
    }

    // Clear all event bits before starting
    xEventGroupClearBits(adapter->extract_event_group,
                         EXTRACT_TASK_START_BIT | EXTRACT_TASK_STOP_BIT | EXTRACT_TASK_STOPPED_BIT);

    BaseType_t ret = xTaskCreate(extract_task, "extract_task",
                                 EXTRACT_TASK_STACK_SIZE, adapter,
                                 EXTRACT_TASK_PRIORITY,
                                 &adapter->extract_task_handle);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create extract task");
        return ESP_FAIL;
    }

    // Set start bit to begin processing
    xEventGroupSetBits(adapter->extract_event_group, EXTRACT_TASK_START_BIT);

    return ESP_OK;
}

// Stop extraction task
static void stop_extract_task(app_stream_adapter_t *adapter)
{
    if (adapter->extract_task_handle == NULL) {
        return;
    }

    ESP_LOGI(TAG, "Stopping extract task");

    // Set stop bit to signal task to stop
    xEventGroupSetBits(adapter->extract_event_group, EXTRACT_TASK_STOP_BIT);

    // Wait for task to acknowledge it has stopped
    EventBits_t bits = xEventGroupWaitBits(adapter->extract_event_group,
                                           EXTRACT_TASK_STOPPED_BIT,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(2000));

    if (!(bits & EXTRACT_TASK_STOPPED_BIT)) {
        ESP_LOGW(TAG, "Extract task did not stop within timeout period");
    }

    // Clear stop bit for next time
    xEventGroupClearBits(adapter->extract_event_group, EXTRACT_TASK_STOP_BIT);
}

esp_err_t app_stream_adapter_init(const app_stream_adapter_config_t *config,
                                  app_stream_adapter_handle_t *ret_adapter)
{
    if (config == NULL || ret_adapter == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->buffer_count == 0) {
        ESP_LOGE(TAG, "buffer_count cannot be zero");
        return ESP_ERR_INVALID_ARG;
    }

    if (config->decode_buffers == NULL) {
        ESP_LOGE(TAG, "decode_buffers cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (config->buffer_size == 0) {
        ESP_LOGE(TAG, "buffer_size cannot be zero");
        return ESP_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < config->buffer_count; i++) {
        if (config->decode_buffers[i] == NULL) {
            ESP_LOGE(TAG, "decode_buffers[%d] is NULL", i);
            return ESP_ERR_INVALID_ARG;
        }
    }

    app_stream_adapter_t *adapter = (app_stream_adapter_t *)calloc(1, sizeof(app_stream_adapter_t));
    if (adapter == NULL) {
        ESP_LOGE(TAG, "Failed to allocate adapter context");
        return ESP_ERR_NO_MEM;
    }

    adapter->frame_cb = config->frame_cb;
    adapter->user_data = config->user_data;
    adapter->decode_buffers = config->decode_buffers;
    adapter->buffer_count = config->buffer_count;
    adapter->buffer_size = config->buffer_size;
    adapter->running = false;
    adapter->current_buffer = config->buffer_count - 1;
    adapter->frame_count = 0;
    adapter->has_info = false;

    adapter->jpeg_config = config->jpeg_config;
    adapter->audio_dev = config->audio_dev;
    adapter->extract_audio = (config->audio_dev != NULL);

    adapter->jpeg_buffer_size = APP_STREAM_JPEG_BUFFER_SIZE;
    adapter->jpeg_buffer = heap_caps_malloc(adapter->jpeg_buffer_size, MALLOC_CAP_SPIRAM);
    if (adapter->jpeg_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate JPEG buffer");
        free(adapter);
        return ESP_ERR_NO_MEM;
    }

    adapter->frame_mutex = xSemaphoreCreateMutex();
    if (adapter->frame_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create frame mutex");
        heap_caps_free(adapter->jpeg_buffer);
        free(adapter);
        return ESP_ERR_NO_MEM;
    }

    // Create event group for task control
    adapter->extract_event_group = xEventGroupCreate();
    if (adapter->extract_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create extract event group");
        vSemaphoreDelete(adapter->frame_mutex);
        heap_caps_free(adapter->jpeg_buffer);
        free(adapter);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = jpeg_hw_init(adapter);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize JPEG decoder: %d", ret);
        vEventGroupDelete(adapter->extract_event_group);
        vSemaphoreDelete(adapter->frame_mutex);
        heap_caps_free(adapter->jpeg_buffer);
        free(adapter);
        return ret;
    }

    g_adapter_instance = adapter;

    if (config->audio_dev) {
        ret = app_extractor_init(extractor_frame_callback, config->audio_dev, &adapter->extractor_handle);
    } else {
        ret = app_extractor_init(extractor_frame_callback, NULL, &adapter->extractor_handle);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize extractor: %d", ret);
        g_adapter_instance = NULL;
        jpeg_del_decoder_engine(adapter->jpeg_handle);
        vEventGroupDelete(adapter->extract_event_group);
        vSemaphoreDelete(adapter->frame_mutex);
        heap_caps_free(adapter->jpeg_buffer);
        free(adapter);
        return ret;
    }

    ESP_LOGI(TAG, "Stream adapter initialized%s with event group control", config->audio_dev ? " with audio" : "");
    *ret_adapter = adapter;
    return ESP_OK;
}

esp_err_t app_stream_adapter_set_file(app_stream_adapter_handle_t handle,
                                      const char *filename,
                                      bool extract_audio)
{
    if (handle == NULL || filename == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_stream_adapter_t *adapter = (app_stream_adapter_t *)handle;

    if (adapter->running) {
        app_stream_adapter_stop(handle);
    }

    adapter->filename = filename;
    adapter->frame_count = 0;
    adapter->has_info = false;
    adapter->width = 0;
    adapter->height = 0;
    adapter->fps = 0;
    adapter->duration = 0;
    adapter->extract_audio = extract_audio && (adapter->audio_dev != NULL);

    ESP_LOGI(TAG, "Set media file: %s, extract_audio: %d",
             filename, adapter->extract_audio);
    return ESP_OK;
}

esp_err_t app_stream_adapter_start(app_stream_adapter_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_stream_adapter_t *adapter = (app_stream_adapter_t *)handle;
    esp_err_t ret = ESP_OK;

    if (adapter->running) {
        return ESP_OK;
    }

    if (adapter->filename == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting playback of %s%s", adapter->filename,
             adapter->extract_audio ? " with audio" : "");

    ret = app_extractor_start(adapter->extractor_handle, adapter->filename,
                              true, adapter->extract_audio);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start extractor: %d", ret);
        return ret;
    }

    uint32_t width, height, fps, duration;
    ret = app_extractor_get_video_info(adapter->extractor_handle,
                                       &width, &height, &fps, &duration);
    if (ret == ESP_OK) {
        adapter->width = width;
        adapter->height = height;
        adapter->fps = fps;
        adapter->duration = duration;
        adapter->has_info = true;

        ESP_LOGI(TAG, "Video info: %" PRIu32 "x%" PRIu32 ", %" PRIu32 " fps, %" PRIu32 " ms",
                 width, height, fps, duration);
    }

    if (adapter->extract_audio) {
        uint32_t sample_rate, duration;
        uint8_t channels, bits;

        ret = app_extractor_get_audio_info(adapter->extractor_handle,
                                           &sample_rate, &channels, &bits, &duration);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Audio info: %" PRIu32 " Hz, %u ch, %u bits, %" PRIu32 " ms",
                     sample_rate, channels, bits, duration);
        }
    }

    ret = start_extract_task(adapter);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start extract task: %d", ret);
        app_extractor_stop(adapter->extractor_handle);
        return ret;
    }

    adapter->running = true;
    return ESP_OK;
}

esp_err_t app_stream_adapter_stop(app_stream_adapter_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_stream_adapter_t *adapter = (app_stream_adapter_t *)handle;

    if (!adapter->running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping playback");

    stop_extract_task(adapter);
    app_extractor_stop(adapter->extractor_handle);

    adapter->running = false;
    return ESP_OK;
}

esp_err_t app_stream_adapter_seek(app_stream_adapter_handle_t handle, uint32_t position)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_stream_adapter_t *adapter = (app_stream_adapter_t *)handle;
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Seeking to position %" PRIu32 " ms", position);

    bool was_running = adapter->extract_task_handle != NULL;
    if (was_running) {
        stop_extract_task(adapter);
    }

    ret = app_extractor_seek(adapter->extractor_handle, position);

    if (was_running) {
        start_extract_task(adapter);
    }

    return ret;
}

esp_err_t app_stream_adapter_get_info(app_stream_adapter_handle_t handle,
                                      uint32_t *width, uint32_t *height,
                                      uint32_t *fps, uint32_t *duration)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_stream_adapter_t *adapter = (app_stream_adapter_t *)handle;

    if (!adapter->has_info) {
        return ESP_ERR_NOT_FOUND;
    }

    if (width) {
        *width = adapter->width;
    }
    if (height) {
        *height = adapter->height;
    }
    if (fps) {
        *fps = adapter->fps;
    }
    if (duration) {
        *duration = adapter->duration;
    }

    return ESP_OK;
}

esp_err_t app_stream_adapter_get_stats(app_stream_adapter_handle_t handle,
                                       app_stream_stats_t *stats)
{
    if (handle == NULL || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_stream_adapter_t *adapter = (app_stream_adapter_t *)handle;

    memset(stats, 0, sizeof(app_stream_stats_t));
    stats->frames_processed = adapter->frame_count;

    return ESP_OK;
}

esp_err_t app_stream_adapter_deinit(app_stream_adapter_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_stream_adapter_t *adapter = (app_stream_adapter_t *)handle;

    if (adapter->running) {
        app_stream_adapter_stop(handle);
    }

    if (adapter->extractor_handle != NULL) {
        app_extractor_deinit(adapter->extractor_handle);
        adapter->extractor_handle = NULL;
    }

    if (adapter->jpeg_handle != NULL) {
        jpeg_del_decoder_engine(adapter->jpeg_handle);
        adapter->jpeg_handle = NULL;
    }

    if (adapter->frame_mutex != NULL) {
        vSemaphoreDelete(adapter->frame_mutex);
    }

    if (adapter->extract_event_group != NULL) {
        vEventGroupDelete(adapter->extract_event_group);
    }

    if (adapter->jpeg_buffer != NULL) {
        heap_caps_free(adapter->jpeg_buffer);
    }

    if (g_adapter_instance == adapter) {
        g_adapter_instance = NULL;
    }

    free(adapter);

    ESP_LOGI(TAG, "Stream adapter deinitialized");
    return ESP_OK;
}

esp_err_t app_stream_adapter_resize_buffers(app_stream_adapter_handle_t handle,
                                            void **decode_buffers,
                                            uint32_t buffer_count,
                                            uint32_t buffer_size)
{
    if (handle == NULL || decode_buffers == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_stream_adapter_t *adapter = (app_stream_adapter_t *)handle;

    if (adapter->running) {
        return ESP_ERR_INVALID_STATE;
    }

    adapter->decode_buffers = decode_buffers;
    adapter->buffer_count = buffer_count;
    adapter->buffer_size = buffer_size;
    adapter->current_buffer = buffer_count - 1;

    return ESP_OK;
}

esp_err_t app_stream_adapter_probe_video_info(const char *filename,
                                              uint32_t *width, uint32_t *height,
                                              uint32_t *fps, uint32_t *duration)
{
    return app_extractor_probe_video_info(filename, width, height, fps, duration);
}
