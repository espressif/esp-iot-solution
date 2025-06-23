/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __VIDOPLAYER_H
#define __VIDOPLAYER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "esp_idf_version.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief video frame format
 *
 */
typedef enum  {
    FORMAT_MJEPG = 0,
    FORMAT_H264,
} video_frame_format;

/**
 * @brief video frame info
 *
 */
typedef struct {
    uint32_t width;                  /*!< Width of image in pixels */
    uint32_t height;                 /*!< Height of image in pixels */
    video_frame_format frame_format; /*!< Pixel data format */
} video_frame_info_t;

/**
 * @brief audio frame format
 *
 */
typedef enum  {
    FORMAT_PCM = 0,
} audio_frame_format;

/**
 * @brief audio frame info
 *
 */
typedef struct {
    uint8_t channel;                /*!< Audio output channel */
    uint8_t bits_per_sample;        /*!< Audio bits per sample */
    uint32_t sample_rate;           /*!< Audio sample rate */
    audio_frame_format format;      /*!< Audio format */
} audio_frame_info_t;

/**
 * @brief frame type: video or audio
 *
 */
typedef enum {
    FRAME_TYPE_VIDEO = 0,
    FRAME_TYPE_AUDIO
} frame_type_t;

/**
 * @brief frame data
 *
 */
typedef struct {
    uint8_t *data;                     /*!< Image data for this frame */
    size_t data_bytes;                 /*!< Size of image data buffer */
    frame_type_t type;                 /*!< Frame type: video or audio */
    /**
     * @brief frame info
     *
     */
    union {
        video_frame_info_t video_info; /*!< Video frame info */
        audio_frame_info_t audio_info; /*!< Audio frame info */
    };
} frame_data_t;

typedef void (*video_write_cb)(frame_data_t *data, void *arg);
typedef void (*audio_write_cb)(frame_data_t *data, void *arg);
typedef void (*audio_set_clock_cb)(uint32_t rate, uint32_t bits_cfg, uint32_t ch, void *arg);
typedef void (*avi_play_end_cb)(void *arg);

typedef void *avi_player_handle_t;

/**
 * @brief avi player config
 *
 */
typedef struct {
    size_t buffer_size;                      /*!< Internal buffer size */
    video_write_cb video_cb;                 /*!< Video frame callback */
    audio_write_cb audio_cb;                 /*!< Audio frame callback */
    audio_set_clock_cb audio_set_clock_cb;   /*!< Audio set clock callback */
    avi_play_end_cb avi_play_end_cb;         /*!< AVI play end callback */
    UBaseType_t priority;                    /*!< FreeRTOS task priority */
    BaseType_t coreID;                       /*!< ESP32 core ID */
    void *user_data;                         /*!< User data */
    int stack_size;                          /*!< Stack size for the player task */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    bool stack_in_psram;                     /*!< If you read file/data from flash, do not set true*/
#endif
} avi_player_config_t;

/**
 * @brief Plays an AVI file from memory. The buffer of the AVI will be passed through the set callback function.
 *
 * This function initializes and plays an AVI file from a memory buffer.
 *
 * @param[in] handle AVI player handle
 * @param[in] avi_data Pointer to the AVI file data in memory.
 * @param[in] avi_size Size of the AVI file data in bytes.
 * @return esp_err_t ESP_OK if successful, otherwise an error code.
 */
esp_err_t avi_player_play_from_memory(avi_player_handle_t handle, uint8_t *avi_data, size_t avi_size);

/**
 * @brief Plays an AVI file from the filesystem. The buffer of the AVI will be passed through the set callback function.
 *
 * This function initializes and plays an AVI file from the filesystem using its filename.
 *
 * @param[in] handle AVI player handle
 * @param[in] filename Path to the AVI file on the filesystem.
 * @return esp_err_t ESP_OK if successful, otherwise an error code.
 */
esp_err_t avi_player_play_from_file(avi_player_handle_t handle, const char *filename);

/**
 * @brief Get one video frame from AVI stream
 *
 * @param[in] handle AVI player handle
 * @param[out] buffer        Pointer to external buffer to hold one frame
 * @param[in,out] buffer_size Size of external buffer
 * @param[out] info          Information of the video frame
 * @param[in] ticks_to_wait  Maximum blocking time
 *
 * @return
 *      - ESP_OK   Success
 *      - ESP_ERR_TIMEOUT  Timeout
 *      - ESP_ERR_INVALID_ARG  NULL arguments
 *      - ESP_ERR_NO_MEM  External buffer not enough
 */
esp_err_t avi_player_get_video_buffer(avi_player_handle_t handle, void **buffer, size_t *buffer_size, video_frame_info_t *info, TickType_t ticks_to_wait);

/**
 * @brief Get the audio buffer from AVI file
 *
 * @param[in] handle AVI player handle
 * @param[out] buffer pointer to the audio buffer
 * @param[in] buffer_size size of the audio buffer
 * @param[out] info audio frame information
 * @param[in] ticks_to_wait maximum blocking time in ticks
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_TIMEOUT if semaphore is not acquired before timeout
 *      - ESP_ERR_INVALID_ARG if buffer or info is NULL or buffer_size is zero
 *      - ESP_ERR_NO_MEM if buffer size is not enough
 */
esp_err_t avi_player_get_audio_buffer(avi_player_handle_t handle, void **buffer, size_t *buffer_size, audio_frame_info_t *info, TickType_t ticks_to_wait);

/**
 * @brief Stop AVI player
 *
 * @param[in] handle AVI player handle
 * @return
 *      - ESP_OK: Stop AVI player successfully
 *      - ESP_ERR_INVALID_STATE: AVI player not playing
 */
esp_err_t avi_player_play_stop(avi_player_handle_t handle);

/**
 * @brief Initialize the AVI player
 *
 * @param[in] config Configuration of AVI player
 * @param[out] handle Pointer to store the AVI player handle
 *
 * @return
 *      - ESP_OK: succeed
 *      - ESP_ERR_NO_MEM: Cannot allocate memory for AVI player
 *      - ESP_ERR_INVALID_STATE: AVI player has already been initialized
 */
esp_err_t avi_player_init(avi_player_config_t config, avi_player_handle_t *handle);

/**
 * @brief Deinitializes the AVI player.
 *
 * This function deinitializes and cleans up resources used by the AVI player.
 *
 * @param[in] handle AVI player handle
 * @return esp_err_t ESP_OK if successful, otherwise an error code.
 */
esp_err_t avi_player_deinit(avi_player_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif
