/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <esp_check.h>

#include "esp_xiaozhi_video.h"
#include "esp_xiaozhi_vision.h"

static const char *TAG = "ESP_XIAOZHI_VIDEO";

struct esp_xiaozhi_video_ctx {
    esp_xiaozhi_vision_t vision;
};

esp_err_t esp_xiaozhi_video_create(const esp_xiaozhi_video_config_t *config,
                                   esp_xiaozhi_video_handle_t **out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid output handle");

    *out_handle = NULL;
    esp_xiaozhi_video_handle_t *handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate handle");

    esp_err_t ret = esp_xiaozhi_vision_init(&handle->vision,
                                            config != NULL ? config->explain_url : NULL,
                                            config != NULL ? config->explain_token : NULL);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t esp_xiaozhi_video_destroy(esp_xiaozhi_video_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    free(handle);
    return ESP_OK;
}

esp_err_t esp_xiaozhi_video_set_explain_url(esp_xiaozhi_video_handle_t *handle,
                                            const char *url,
                                            const char *token)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    return esp_xiaozhi_vision_set_endpoint(&handle->vision, url, token);
}

esp_err_t esp_xiaozhi_video_explain(esp_xiaozhi_video_handle_t *handle,
                                    const esp_xiaozhi_video_frame_t *frame,
                                    const char *question,
                                    char *out_buf,
                                    size_t buf_size,
                                    size_t *out_len)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(frame != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid frame");
    ESP_RETURN_ON_FALSE(frame->data != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid frame data");
    ESP_RETURN_ON_FALSE(frame->len > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid frame length");
    ESP_RETURN_ON_FALSE(question != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid question");

    return esp_xiaozhi_vision_explain_jpeg(&handle->vision,
                                           TAG,
                                           "video.jpg",
                                           question,
                                           frame->data,
                                           frame->len,
                                           out_buf,
                                           buf_size,
                                           out_len);
}
