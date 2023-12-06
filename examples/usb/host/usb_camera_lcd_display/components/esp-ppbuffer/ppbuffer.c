/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "ppbuffer.h"

static const char *TAG = "PingPongBuffer";

#define PPBUFFER_CHECK(a, str, ret_val)                          \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

esp_err_t ppbuffer_create(PingPongBuffer_t *ppbuf, void *buf0, void *buf1)
{
    PPBUFFER_CHECK(ppbuf != NULL, "Invalid pointer", ESP_ERR_INVALID_ARG);
    PPBUFFER_CHECK(buf0 != NULL, "Invalid pointer", ESP_ERR_INVALID_ARG);
    PPBUFFER_CHECK(buf1 != NULL, "Invalid pointer", ESP_ERR_INVALID_ARG);

    memset(ppbuf, 0, sizeof(PingPongBuffer_t));
    ppbuf->buffer[0] = buf0;
    ppbuf->buffer[1] = buf1;
    return ESP_OK;
}

esp_err_t ppbuffer_get_read_buf(PingPongBuffer_t *ppbuf, void **pReadBuf)
{
    PPBUFFER_CHECK(ppbuf != NULL, "Invalid pointer", ESP_ERR_INVALID_ARG);
    if (ppbuf->readAvailable[0]) {
        ppbuf->readIndex = 0;
    } else if (ppbuf->readAvailable[1]) {
        ppbuf->readIndex = 1;
    } else {
        return ESP_ERR_INVALID_STATE;
    }

    *pReadBuf = ppbuf->buffer[ppbuf->readIndex];
    return ESP_OK;
}

esp_err_t ppbuffer_set_read_done(PingPongBuffer_t *ppbuf)
{
    PPBUFFER_CHECK(ppbuf != NULL, "Invalid pointer", ESP_ERR_INVALID_ARG);
    ppbuf->readAvailable[ppbuf->readIndex] = false;
    return ESP_OK;
}

esp_err_t ppbuffer_get_write_buf(PingPongBuffer_t *ppbuf, void **pWriteBuf)
{
    PPBUFFER_CHECK(ppbuf != NULL, "Invalid pointer", ESP_ERR_INVALID_ARG);
    if (ppbuf->writeIndex == ppbuf->readIndex) {
        ppbuf->writeIndex = !ppbuf->readIndex;
    }

    *pWriteBuf = ppbuf->buffer[ppbuf->writeIndex];
    return ESP_OK;
}

esp_err_t ppbuffer_set_write_done(PingPongBuffer_t *ppbuf)
{
    PPBUFFER_CHECK(ppbuf != NULL, "Invalid pointer", ESP_ERR_INVALID_ARG);
    ppbuf->readAvailable[ppbuf->writeIndex] = true;
    ppbuf->writeIndex = !ppbuf->writeIndex;
    return ESP_OK;
}
