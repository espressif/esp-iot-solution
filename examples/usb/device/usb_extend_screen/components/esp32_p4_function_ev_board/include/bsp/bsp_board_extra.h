/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"

#include "audio_player.h"
#include "file_iterator.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CODEC_DEFAULT_SAMPLE_RATE          (16000)
#define CODEC_DEFAULT_BIT_WIDTH            (16)
#define CODEC_DEFAULT_ADC_VOLUME           (24.0)
#define CODEC_DEFAULT_CHANNEL              (2)
#define CODEC_DEFAULT_VOLUME               (60)

/**
 * @brief Player set mute.
 *
 * @param enable: true or false
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t bsp_extra_codec_mute_set(bool enable);

/**
 * @brief Player set volume.
 *
 * @param volume: volume set
 * @param volume_set: volume set response
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set);

int bsp_extra_codec_volume_get(void);

/**
 * @brief Stop I2S function.
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t bsp_extra_codec_dev_stop(void);

/**
 * @brief Resume I2S function.
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t bsp_extra_codec_dev_resume(void);

/**
 * @brief Set I2S format to codec.
 *
 * @param rate: Sample rate of sample
 * @param bits_cfg: Bit lengths of one channel data
 * @param ch: Channels of sample
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch);

/**
 * @brief Read data from recoder.
 *
 * @param audio_buffer: The pointer of receiving data buffer
 * @param len: Max data buffer length
 * @param bytes_read: Byte number that actually be read, can be NULL if not needed
 * @param timeout_ms: Max block time
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms);

/**
 * @brief Write data to player.
 *
 * @param audio_buffer: The pointer of sent data buffer
 * @param len: Max data buffer length
 * @param bytes_written: Byte number that actually be sent, can be NULL if not needed
 * @param timeout_ms: Max block time
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t bsp_extra_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms);

/**
 * @brief Initialize codec play and record handle.
 *
 * @return
 *      - ESP_OK: Success
 *      - Others: Fail
 */
esp_err_t bsp_extra_codec_init();

/**
 * @brief Initialize audio player task.
 *
 * @param path file path
 *
 * @return
 *      - ESP_OK: Success
 *      - Others: Fail
 */
esp_err_t bsp_extra_player_init(void);

esp_err_t bsp_extra_player_del(void);

esp_err_t bsp_extra_file_instance_init(const char *path, file_iterator_instance_t **ret_instance);

esp_err_t bsp_extra_player_play_index(file_iterator_instance_t *instance, int index);

esp_err_t bsp_extra_player_play_file(const char *file_path);

void bsp_extra_player_register_callback(audio_player_cb_t cb, void *user_data);

bool bsp_extra_player_is_playing_by_path(const char *file_path);

bool bsp_extra_player_is_playing_by_index(file_iterator_instance_t *instance, int index);

#ifdef __cplusplus
}
#endif
