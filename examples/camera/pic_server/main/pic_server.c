/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_http_server.h"
#include "img_converters.h"
#include "sdkconfig.h"
#include "esp_log.h"

static httpd_handle_t pic_httpd = NULL;

static const char *TAG = "pic_s";

/* Handler to download a file kept on the server */
static esp_err_t pic_get_handler(httpd_req_t *req)
{
    camera_fb_t *frame = NULL;
    esp_err_t res = ESP_OK;
    size_t image_data_buf_len = 0;
    uint8_t *image_data_buf = NULL;

#if CONFIG_IMAGE_JPEG_FORMAT
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
#elif CONFIG_IMAGE_BMP_FORMAT
    httpd_resp_set_type(req, "image/bmp");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.bmp");
#endif
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    esp_camera_fb_return(esp_camera_fb_get());
    frame = esp_camera_fb_get();

    if (frame) {
#if CONFIG_IMAGE_JPEG_FORMAT
        if (frame->format == PIXFORMAT_JPEG) {
            image_data_buf = frame->buf;
            image_data_buf_len = frame->len;
        } else if (!frame2jpg(frame, 60, &image_data_buf, &image_data_buf_len)) {
            ESP_LOGE(TAG, "JPEG compression failed");
            res = ESP_FAIL;
        }
#elif CONFIG_IMAGE_BMP_FORMAT
        if (frame2bmp(frame, &image_data_buf, &image_data_buf_len) != true) {
            res = ESP_FAIL;
        }
#endif
    } else {
        res = ESP_FAIL;
    }

    if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, (const char *)image_data_buf, image_data_buf_len);
#if CONFIG_IMAGE_JPEG_FORMAT
        if (frame->format != PIXFORMAT_JPEG) {
            free(image_data_buf);
            image_data_buf = NULL;
        }
#elif CONFIG_IMAGE_BMP_FORMAT
        free(image_data_buf);
        image_data_buf = NULL;
#endif
        esp_camera_fb_return(frame);
        ESP_LOGI(TAG, "pic len %d", image_data_buf_len);
    } else {
        ESP_LOGW(TAG, "exit pic server");
        return ESP_FAIL;
    }
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t start_pic_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 5120;

    httpd_uri_t pic_uri = {
        .uri = "/pic",
        .method = HTTP_GET,
        .handler = pic_get_handler,
        .user_ctx = NULL
    };

    ESP_LOGI(TAG, "Starting pic server on port: '%d'", config.server_port);
    if (httpd_start(&pic_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(pic_httpd, &pic_uri);
        return ESP_OK;
    }
    return ESP_FAIL;
}
