/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "app_uvc.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "HTTP_SERVER"

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

#define STATIC_PATH "/spiffs"

typedef struct {
    char id[9];
    uvc_dev_info_t *info;
} camera_t;

typedef struct {
    camera_t *cameras;
    int camera_count;
} camera_system_t;

static camera_system_t camera_system = {0};
static httpd_handle_t s_server = NULL;

static void clear_camera_system(void)
{
    if (camera_system.cameras == NULL) {
        camera_system.camera_count = 0;
        return;
    }

    // Release dynamically allocated per-camera resolution lists before rebuilding the snapshot.
    for (int i = 0; i < camera_system.camera_count; ++i) {
        app_uvc_free_dev_frame_info(camera_system.cameras[i].info);
    }
    free(camera_system.cameras);
    camera_system.cameras = NULL;
    camera_system.camera_count = 0;
}

static void update_camera_system(void)
{
    int connected_num = 0;
    app_uvc_get_connect_dev_num(&connected_num);

    clear_camera_system();
    if (connected_num <= 0) {
        return;
    }

    camera_system.cameras = calloc(connected_num, sizeof(camera_t));
    if (camera_system.cameras == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for camera system");
        return;
    }

    camera_system.camera_count = connected_num;
    for (int i = 0; i < connected_num; ++i) {
        if (app_uvc_get_dev_frame_info(i, &camera_system.cameras[i].info) == ESP_OK) {
            snprintf(camera_system.cameras[i].id, sizeof(camera_system.cameras[i].id), "%d", camera_system.cameras[i].info->index);
        } else {
            ESP_LOGE(TAG, "Failed to get camera info for index %d", i);
        }
    }
}

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    } else if (IS_FILE_EXT(filename, ".css")) {
        return httpd_resp_set_type(req, "text/css");
    } else if (IS_FILE_EXT(filename, ".js")) {
        return httpd_resp_set_type(req, "text/javascript");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

static esp_err_t static_file_handler(httpd_req_t *req)
{
    const char *uri = req->uri;
    char filepath[1024];
    char filepath_gz[1024];

    snprintf(filepath, sizeof(filepath), "/webserver%s", req->uri);
    snprintf(filepath_gz, sizeof(filepath_gz), "/webserver%s.gz", req->uri);

    if (strcmp(uri, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", STATIC_PATH);
        snprintf(filepath_gz, sizeof(filepath_gz), "%s/index.html.gz", STATIC_PATH);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", STATIC_PATH, uri);
        snprintf(filepath_gz, sizeof(filepath_gz), "%s%s.gz", STATIC_PATH, uri);
    }

    FILE *file = fopen(filepath, "r");
    FILE *file_gz = fopen(filepath_gz, "r");

    if (!file_gz && !file) {
        ESP_LOGE(TAG, "File not found: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    if (file_gz) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        fclose(file);
        file = file_gz;
    }

    set_content_type_from_file(req, filepath);

    char buffer[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, read_bytes);
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// API: GET /api/404
static esp_err_t get_404_handler(httpd_req_t *req)
{
    httpd_resp_send_404(req);
    return ESP_OK;
}

// API: GET /api/cameras
static esp_err_t get_cameras_handler(httpd_req_t *req)
{
    update_camera_system();
    size_t resolution_count = 0;
    for (int i = 0; i < camera_system.camera_count; ++i) {
        if (camera_system.cameras[i].info != NULL) {
            resolution_count += camera_system.cameras[i].info->resolution_count;
        }
    }

    size_t response_size = 128 + camera_system.camera_count * 128 + resolution_count * 80;
    char *response = (char *)calloc(1, response_size);
    if (response == NULL) {
        ESP_LOGE(TAG, "Failed to allocate camera response");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_ERR_NO_MEM;
    }

    int len = snprintf(response, response_size, "{\"limit\":%d,\"cameras\":[", camera_system.camera_count);

    for (int i = 0; i < camera_system.camera_count; ++i) {
        camera_t *cam = &camera_system.cameras[i];
        uvc_dev_info_t *info = cam->info;
        if (info == NULL) {
            len += snprintf(response + len, response_size - len, "{\"id\":\"%s\",\"resolutions\":[]}%s", cam->id,
                            (i < camera_system.camera_count - 1) ? "," : "");
            continue;
        }

        len += snprintf(response + len, response_size - len, "{\"id\":\"%s\",\"resolutions\":[", cam->id);
        for (int j = 0; j < info->resolution_count; ++j) {
            len += snprintf(response + len, response_size - len,
                            "{\"width\":%d,\"height\":%d,\"index\":%d}%s",
                            info->resolution[j].width, info->resolution[j].height, j,
                            (j < info->resolution_count - 1) ? "," : "");
        }
        len += snprintf(response + len, response_size - len, "]}%s",
                        (i < camera_system.camera_count - 1) ? "," : "");
    }
    len += snprintf(response + len, response_size - len, "],\"activated\":[");

    bool first_activated = true;
    for (int i = 0; i < camera_system.camera_count; ++i) {
        camera_t *cam = &camera_system.cameras[i];
        uvc_dev_info_t *info = cam->info;
        if (info != NULL && info->if_streaming && info->active_resolution >= 0 && info->active_resolution < info->resolution_count) {
            if (!first_activated) {
                len += snprintf(response + len, response_size - len, ",");
            }

            len += snprintf(response + len, response_size - len,
                            "{\"id\":\"%s\",\"resolution\":{\"width\":%d,\"height\":%d, \"index\":%d}}",
                            cam->id,
                            info->resolution[info->active_resolution].width,
                            info->resolution[info->active_resolution].height,
                            info->active_resolution);
            first_activated = false;
        }
    }
    len += snprintf(response + len, response_size - len, "]}");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    free(response);
    return ESP_OK;
}

// API: POST /api/active
static esp_err_t post_active_handler(httpd_req_t *req)
{
    char content[128];
    int received = 0;
    int remaining = req->content_len;

    if (remaining <= 0 || remaining >= sizeof(content)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request body");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        int ret = httpd_req_recv(req, content + received, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read request body");
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    content[received] = '\0';

    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read request body");
        return ESP_FAIL;
    }

    int id = 0, resolution_index = 0;
    sscanf(content, "{\"id\":\"%d\",\"resolution\":{\"width\":%*d,\"height\":%*d,\"index\":%d}}", &id, &resolution_index);

    // Open streaming
    printf("id: %d, resolution_index %d\n", id, resolution_index);
    esp_err_t err = app_uvc_control_dev_by_index(id, true, resolution_index);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start stream");
        return ESP_FAIL;
    }
    // Wait for camera to start streaming
    vTaskDelay(200 / portTICK_PERIOD_MS);

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"code\":200}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// API: POST /api/deactivate
static esp_err_t post_deactivate_handler(httpd_req_t *req)
{
    char content[64];
    int received = 0;
    int remaining = req->content_len;

    if (remaining <= 0 || remaining >= sizeof(content)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request body");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        int ret = httpd_req_recv(req, content + received, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read request body");
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    content[received] = '\0';

    int id = 0;
    if (sscanf(content, "{\"id\":\"%d\"}", &id) != 1 && sscanf(content, "{\"id\":%d}", &id) != 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid camera id");
        return ESP_FAIL;
    }

    esp_err_t err = app_uvc_control_dev_by_index(id, false, -1);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Camera not found");
        return ESP_FAIL;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"code\":200}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace; boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "--" PART_BOUNDARY "\r\n";
static const char *_STREAM_BOUNDARY_CONT = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Stream-Index: %d\r\n\r\n";

static esp_err_t transfer_stream_frame(httpd_req_t *req, uvc_host_frame_t *frame, int stream_id, bool first_frame)
{
    esp_err_t res = ESP_OK;
    char part_buf[128];
    if (frame == NULL) {
        ESP_LOGE(TAG, "Camera capture failed");
        res = ESP_FAIL;
    }

    if (res == ESP_OK) {
        const char *boundary = first_frame ? _STREAM_BOUNDARY : _STREAM_BOUNDARY_CONT;
        res = httpd_resp_send_chunk(req, boundary, strlen(boundary));
    }

    if (res == ESP_OK) {
        size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, frame->data_len, stream_id);
        res = httpd_resp_send_chunk(req, part_buf, hlen);
    }

    if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, (const char *)frame->data, frame->data_len);
    }

    return res;
}

// API: GET /api/stream/*
static esp_err_t stream_worker(httpd_req_t *req, int stream_id)
{
    esp_err_t res = ESP_OK;
    ESP_LOGI(TAG, "Start stream worker, stream ID: %d", stream_id);

    httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Connection", "close");

    int frame_null_cnt = 0;
    int frame_cnt = 0;
    bool first_sent_frame = true;

    while (true) {
        uvc_host_frame_t *frame = app_uvc_get_frame_by_index(stream_id);
        if (frame != NULL) {
            frame_null_cnt = 0;
            // Discard the first 5 frames to avoid poor-quality frames generated by the camera.
            if (frame_cnt < 5) {
                ++frame_cnt;
            } else {
                res = transfer_stream_frame(req, frame, stream_id, first_sent_frame);
                first_sent_frame = false;
            }
            app_uvc_return_frame_by_index(stream_id, frame);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "Stream transfer failed");
                break;
            }
        } else {
            ESP_LOGI(TAG, "Camera #%d: frame is NULL", stream_id);
            frame_null_cnt++;
            if (frame_null_cnt > 30) {
                break;
            }
        }
    }
    ESP_LOGE(TAG, "trans error\n");
    res = httpd_resp_send_chunk(req, NULL, 0);

    app_uvc_control_dev_by_index(stream_id, false, -1);
    return res;
}

typedef struct {
    httpd_req_t *req;
    int stream_id;
} stream_req_ctx_t;

static void stream_task(void *arg)
{
    stream_req_ctx_t *ctx = (stream_req_ctx_t *)arg;
    if (ctx == NULL || ctx->req == NULL) {
        vTaskDelete(NULL);
        return;
    }

    stream_worker(ctx->req, ctx->stream_id);
    if (httpd_req_async_handler_complete(ctx->req) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to complete async stream request");
    }
    free(ctx);
    vTaskDelete(NULL);
}

static esp_err_t get_stream_handler(httpd_req_t *req)
{
    const char *uri = req->uri;
    ESP_LOGI(TAG, "Received request for URI: %s", uri);

    const char *last_slash = strrchr(uri, '/');
    if (last_slash == NULL || *(last_slash + 1) == '\0') {
        ESP_LOGE(TAG, "Invalid URI format");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Invalid stream ID");
        return ESP_FAIL;
    }

    int stream_id = atoi(last_slash + 1);
    httpd_req_t *async_req = NULL;
    esp_err_t err = httpd_req_async_handler_begin(req, &async_req);
    if (err != ESP_OK || async_req == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Server busy");
        return ESP_OK;
    }

    stream_req_ctx_t *ctx = calloc(1, sizeof(stream_req_ctx_t));
    if (ctx == NULL) {
        httpd_req_async_handler_complete(async_req);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_OK;
    }
    ctx->req = async_req;
    ctx->stream_id = stream_id;

    if (xTaskCreate(stream_task, "stream_task", 4096, ctx, 5, NULL) != pdPASS) {
        free(ctx);
        httpd_req_async_handler_complete(async_req);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start stream worker");
        return ESP_OK;
    }

    return ESP_OK;
}

static esp_err_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 1024 * 6;
    config.task_priority = 10;
    config.lru_purge_enable = true;
    config.max_open_sockets = 7;
    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_uri_t cameras_get = {
        .uri = "/api/cameras",
        .method = HTTP_GET,
        .handler = get_cameras_handler,
        .user_ctx = NULL
    };

    httpd_uri_t active_post = {
        .uri = "/api/active",
        .method = HTTP_POST,
        .handler = post_active_handler,
        .user_ctx = NULL
    };

    httpd_uri_t deactivate_post = {
        .uri = "/api/deactivate",
        .method = HTTP_POST,
        .handler = post_deactivate_handler,
        .user_ctx = NULL
    };

    httpd_uri_t static_handler = {
        .uri = "*",
        .method = HTTP_GET,
        .handler = static_file_handler,
        .user_ctx = NULL
    };

    httpd_uri_t stream_get = {
        .uri = "/api/stream/*",
        .method = HTTP_GET,
        .handler = get_stream_handler,
        .user_ctx = NULL
    };

    httpd_uri_t api_404_get = {
        .uri = "/api/404",
        .method = HTTP_GET,
        .handler = get_404_handler,
        .user_ctx = NULL
    };

    config.core_id = tskNO_AFFINITY;
    if (httpd_start(&s_server, &config) == ESP_OK) {
        httpd_register_uri_handler(s_server, &active_post);
        httpd_register_uri_handler(s_server, &deactivate_post);
        httpd_register_uri_handler(s_server, &cameras_get);
        httpd_register_uri_handler(s_server, &api_404_get);
        httpd_register_uri_handler(s_server, &stream_get);
        httpd_register_uri_handler(s_server, &static_handler);
    } else {
        goto exit;
    }

    return ESP_OK;
exit:
    return ESP_FAIL;
}

void app_https_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = STATIC_PATH,
        .partition_label = "storage",
        .max_files = 5,   // This decides the maximum number of files that can be created on the storage
        .format_if_mount_failed = false
    };

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
    ESP_ERROR_CHECK(start_webserver());
}
