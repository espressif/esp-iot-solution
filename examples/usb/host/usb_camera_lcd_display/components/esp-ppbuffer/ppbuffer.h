/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

typedef struct {
    void *buffer[2];            /*!< Pointer to the ping-pong buffer structure >*/
    uint8_t writeIndex;         /*!< Write index >*/
    uint8_t readIndex;          /*!< Read index >*/
    uint8_t readAvailable[2];   /*!< Available to read >*/
} PingPongBuffer_t;

/**
 * @brief Ping-pong buffer initialization.
 *
 * @param ppbuf Pointer to the ping-pong buffer structure.
 * @param buf0  Pointer to the first buffer.
 * @param buf1  Pointer to the second buffer.
 * @return
 *    - ESP_OK: Create successfully.
 *    - ESP_ERR_INVALID_ARG: handle is invalid.
 */
esp_err_t ppbuffer_create(PingPongBuffer_t *ppbuf, void *buf0, void *buf1);

/**
 * @brief  Get a readable buffer.
 *
 * @param ppbuf    Pointer to the ping-pong buffer structure.
 * @param pReadBuf Pointer to the pointer to the buffer to be read.
 * @return
 *    - ESP_OK: Get a readable buffer successfully.
 *    - ESP_ERR_INVALID_ARG: handle is invalid.
 *    - ESP_ERR_INVALID_STATE: No readable buffer.
 */
esp_err_t ppbuffer_get_read_buf(PingPongBuffer_t *ppbuf, void **pReadBuf);

/**
 * @brief Notify buffer read completion.
 *
 * @param ppbuf Pointer to the ping-pong buffer structure.
 * @return
 *    - ESP_OK: Notify buffer read completion successfully.
 *    - ESP_ERR_INVALID_ARG: handle is invalid.
 */
esp_err_t ppbuffer_set_read_done(PingPongBuffer_t *ppbuf);

/**
 * @brief Get writable buffer.
 *
 * @param ppbuf     Pointer to the ping-pong buffer structure.
 * @param pWriteBuf Pointer to the pointer to the buffer to be write.
 * @return
 *    - ESP_OK: Get a writeable buffer successfully.
 *    - ESP_ERR_INVALID_ARG: handle is invalid.
 */
esp_err_t ppbuffer_get_write_buf(PingPongBuffer_t *ppbuf, void **pWriteBuf);

/**
 * @brief Notify buffer write completion.
 *
 * @param ppbuf Pointer to the ping-pong buffer structure.
 * @return
 *    - ESP_OK: Notify buffer write completion successfully.
 *    - ESP_ERR_INVALID_ARG: handle is invalid.
 */
esp_err_t ppbuffer_set_write_done(PingPongBuffer_t *ppbuf);

#ifdef __cplusplus
}
#endif
