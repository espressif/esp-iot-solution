/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_check.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_log.h>

#include "esp_xiaozhi_board.h"
#include "esp_xiaozhi_vision.h"

#define ESP_XIAOZHI_VISION_BOUNDARY "----ESP32_CAMERA_BOUNDARY"

typedef struct {
    char *buf;
    size_t size;
    size_t len;
} esp_xiaozhi_vision_response_t;

static esp_err_t esp_xiaozhi_vision_http_event_handler(esp_http_client_event_t *event)
{
    esp_xiaozhi_vision_response_t *ctx = (esp_xiaozhi_vision_response_t *)event->user_data;
    if (ctx == NULL || ctx->buf == NULL || ctx->size == 0 || event->event_id != HTTP_EVENT_ON_DATA) {
        return ESP_OK;
    }

    size_t copy_len = event->data_len;
    if (ctx->len + copy_len >= ctx->size) {
        copy_len = ctx->size - ctx->len - 1;
    }
    if (copy_len > 0) {
        memcpy(ctx->buf + ctx->len, event->data, copy_len);
        ctx->len += copy_len;
        ctx->buf[ctx->len] = '\0';
    }

    return ESP_OK;
}

esp_err_t esp_xiaozhi_vision_init(esp_xiaozhi_vision_t *vision,
                                  const char *url,
                                  const char *token)
{
    ESP_RETURN_ON_FALSE(vision != NULL, ESP_ERR_INVALID_ARG, "ESP_XIAOZHI_VISION", "Invalid vision");

    memset(vision, 0, sizeof(*vision));
    return esp_xiaozhi_vision_set_endpoint(vision, url, token);
}

esp_err_t esp_xiaozhi_vision_set_endpoint(esp_xiaozhi_vision_t *vision,
                                          const char *url,
                                          const char *token)
{
    ESP_RETURN_ON_FALSE(vision != NULL, ESP_ERR_INVALID_ARG, "ESP_XIAOZHI_VISION", "Invalid vision");

    if (url != NULL) {
        strlcpy(vision->explain_url, url, sizeof(vision->explain_url));
    } else {
        vision->explain_url[0] = '\0';
    }

    if (token != NULL) {
        strlcpy(vision->explain_token, token, sizeof(vision->explain_token));
    } else {
        vision->explain_token[0] = '\0';
    }

    return ESP_OK;
}

esp_err_t esp_xiaozhi_vision_explain_jpeg(const esp_xiaozhi_vision_t *vision,
                                          const char *log_tag,
                                          const char *filename,
                                          const char *question,
                                          const uint8_t *jpeg_data,
                                          size_t jpeg_len,
                                          char *out_buf,
                                          size_t buf_size,
                                          size_t *out_len)
{
    ESP_RETURN_ON_FALSE(vision != NULL, ESP_ERR_INVALID_ARG, log_tag, "Invalid vision");
    ESP_RETURN_ON_FALSE(filename != NULL, ESP_ERR_INVALID_ARG, log_tag, "Invalid filename");
    ESP_RETURN_ON_FALSE(question != NULL, ESP_ERR_INVALID_ARG, log_tag, "Invalid question");
    ESP_RETURN_ON_FALSE(jpeg_data != NULL, ESP_ERR_INVALID_ARG, log_tag, "Invalid JPEG data");
    ESP_RETURN_ON_FALSE(vision->explain_url[0] != '\0', ESP_ERR_INVALID_STATE, log_tag, "Explain URL is not configured");

    if (out_len != NULL) {
        *out_len = 0;
    }
    if (out_buf != NULL && buf_size > 0) {
        out_buf[0] = '\0';
    }

    esp_xiaozhi_chat_board_info_t board_info = {0};
    esp_err_t ret = esp_xiaozhi_chat_get_board_info(&board_info);
    ESP_RETURN_ON_ERROR(ret, log_tag, "Failed to get board info");

    const size_t boundary_len = strlen(ESP_XIAOZHI_VISION_BOUNDARY);
    const size_t part1_len = snprintf(NULL, 0,
                                      "--%s\r\n"
                                      "Content-Disposition: form-data; name=\"question\"\r\n"
                                      "\r\n"
                                      "%s\r\n",
                                      ESP_XIAOZHI_VISION_BOUNDARY, question);
    const size_t part2_len = snprintf(NULL, 0,
                                      "--%s\r\n"
                                      "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
                                      "Content-Type: image/jpeg\r\n"
                                      "\r\n",
                                      ESP_XIAOZHI_VISION_BOUNDARY, filename);
    const size_t footer_len = boundary_len + 8;
    const size_t body_len = part1_len + part2_len + jpeg_len + footer_len;

    uint8_t *body = (uint8_t *)heap_caps_malloc(body_len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(body != NULL, ESP_ERR_NO_MEM, log_tag, "Failed to allocate HTTP body");

    uint8_t *cursor = body;
    int written = snprintf((char *)cursor, part1_len + 1,
                           "--%s\r\n"
                           "Content-Disposition: form-data; name=\"question\"\r\n"
                           "\r\n"
                           "%s\r\n",
                           ESP_XIAOZHI_VISION_BOUNDARY, question);
    if (written < 0 || (size_t)written != part1_len) {
        heap_caps_free(body);
        return ESP_FAIL;
    }
    cursor += part1_len;

    written = snprintf((char *)cursor, part2_len + 1,
                       "--%s\r\n"
                       "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
                       "Content-Type: image/jpeg\r\n"
                       "\r\n",
                       ESP_XIAOZHI_VISION_BOUNDARY, filename);
    if (written < 0 || (size_t)written != part2_len) {
        heap_caps_free(body);
        return ESP_FAIL;
    }
    cursor += part2_len;

    memcpy(cursor, jpeg_data, jpeg_len);
    cursor += jpeg_len;
    memcpy(cursor, "\r\n--", 4);
    cursor += 4;
    memcpy(cursor, ESP_XIAOZHI_VISION_BOUNDARY, boundary_len);
    cursor += boundary_len;
    memcpy(cursor, "--\r\n", 4);
    cursor += 4;

    esp_xiaozhi_vision_response_t resp_ctx = {
        .buf = out_buf,
        .size = buf_size,
        .len = 0,
    };

    esp_http_client_config_t config = {
        .url = vision->explain_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
        .event_handler = esp_xiaozhi_vision_http_event_handler,
        .user_data = (out_buf != NULL && buf_size > 0) ? &resp_ctx : NULL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    char content_type[96];
    char authorization[ESP_XIAOZHI_VISION_EXPLAIN_TOKEN_LEN + 8];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", ESP_XIAOZHI_VISION_BOUNDARY);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        heap_caps_free(body);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_http_client_set_header(client, "Device-Id", board_info.mac_address);
    esp_http_client_set_header(client, "Client-Id", board_info.uuid);
    if (vision->explain_token[0] != '\0') {
        snprintf(authorization, sizeof(authorization), "Bearer %s", vision->explain_token);
        esp_http_client_set_header(client, "Authorization", authorization);
    }
    esp_http_client_set_post_field(client, (const char *)body, (int)(cursor - body));

    ret = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    if (ret == ESP_OK && status_code == 200 && out_len != NULL) {
        *out_len = resp_ctx.len;
    } else if (ret == ESP_OK && status_code != 200) {
        ESP_LOGE(log_tag, "Explain HTTP status=%d", status_code);
        ret = ESP_FAIL;
    }

    esp_http_client_cleanup(client);
    heap_caps_free(body);

    return ret;
}
