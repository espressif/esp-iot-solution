/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_extractor.h"
#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ESP32-P4 JPEG Hardware Configuration */
#define HDMI_AUDIO_BUFFER_SIZE          (64 * 1024)

/* A/V Sync Configuration */
#define HDMI_SYNC_THRESHOLD_MS          (50)

/* Memory pool configuration */
#define EXTRACTOR_POOL_SIZE             (256 * 1024)
#define EXTRACTOR_POOL_BLOCKS           (3)

/* Audio Task Configuration  */
#define AUDIO_TASK_PRIORITY             (7)
#define AUDIO_TASK_STACK_SIZE           (4 * 1024)
#define AUDIO_QUEUE_SIZE                (6)
#define AUDIO_QUEUE_TIMEOUT_MS          (50)

/* Frame Rate Control */
#define DEFAULT_VIDEO_FPS               (25)

/**
 * @brief Extractor handle
 */
typedef struct app_extractor_t* app_extractor_handle_t;

/**
 * @brief Frame callback function
 */
typedef esp_err_t (*app_extractor_frame_cb_t)(uint8_t *buffer, uint32_t buffer_size,
                                              bool is_video, uint32_t pts);

/**
 * @brief Initialize extractor with optional audio support
 */
esp_err_t app_extractor_init(app_extractor_frame_cb_t frame_cb,
                             esp_codec_dev_handle_t audio_dev,
                             app_extractor_handle_t *ret_extractor);

/**
 * @brief Start extracting from media file
 */
esp_err_t app_extractor_start(app_extractor_handle_t extractor,
                              const char *filename,
                              bool extract_video,
                              bool extract_audio);

/**
 * @brief Read next frame
 */
esp_err_t app_extractor_read_frame(app_extractor_handle_t extractor);

/**
 * @brief Get video stream info
 */
esp_err_t app_extractor_get_video_info(app_extractor_handle_t extractor,
                                       uint32_t *width, uint32_t *height,
                                       uint32_t *fps, uint32_t *duration);

/**
 * @brief Get audio stream info
 */
esp_err_t app_extractor_get_audio_info(app_extractor_handle_t extractor,
                                       uint32_t *sample_rate, uint8_t *channels,
                                       uint8_t *bits, uint32_t *duration);

esp_err_t app_extractor_probe_video_info(const char *filename,
                                         uint32_t *width, uint32_t *height,
                                         uint32_t *fps, uint32_t *duration);

/**
 * @brief Seek to position in milliseconds
 */
esp_err_t app_extractor_seek(app_extractor_handle_t extractor, uint32_t position);

/**
 * @brief Stop extraction
 */
esp_err_t app_extractor_stop(app_extractor_handle_t extractor);

/**
 * @brief Cleanup and free resources
 */
esp_err_t app_extractor_deinit(app_extractor_handle_t extractor);

#ifdef __cplusplus
}
#endif
