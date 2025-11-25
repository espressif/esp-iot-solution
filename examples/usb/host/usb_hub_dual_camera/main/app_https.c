/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "app_uvc.h"
#include "esp_spiffs.h"

#define TAG "HTTP_SERVER"

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

#define STATIC_PATH "/spiffs"

typedef struct {
    char id[9];
    uvc_dev_info_t info;
} camera_t;

typedef struct {
    camera_t *cameras;
    int camera_count;
    int active_camera_count;
} camera_system_t;

static camera_system_t camera_system = {0};

static void update_camera_system(void)
{
    int connected_num = 0;
    app_uvc_get_connect_dev_num(&connected_num);
    if (camera_system.cameras == NULL) {
        camera_system.cameras = calloc(connected_num, sizeof(camera_t));
        if (camera_system.cameras == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for camera system");
            return;
        }
    }
    camera_system.camera_count = connected_num;
    camera_system.active_camera_count = 0;
    for (int i = 0; i < connected_num; ++i) {
        if (app_uvc_get_dev_frame_info(i, &camera_system.cameras[i].info) == ESP_OK) {
            snprintf(camera_system.cameras[i].id, sizeof(camera_system.cameras[i].id), "%d", camera_system.cameras[i].info.index);
            if (camera_system.cameras[i].info.if_streaming) {
                camera_system.active_camera_count++;
            }
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
    char response[1024];
    int len = snprintf(response, sizeof(response),
                       "{\"limit\":%d,\"cameras\":[", camera_system.camera_count);

    for (int i = 0; i < camera_system.camera_count; ++i) {
        camera_t *cam = &camera_system.cameras[i];
        len += snprintf(response + len, sizeof(response) - len,
                        "{\"id\":\"%s\",\"resolutions\":[",
                        cam->id);
        for (int j = 0; j < cam->info.resolution_count; ++j) {
            len += snprintf(response + len, sizeof(response) - len,
                            "{\"width\":%d,\"height\":%d,\"index\":%d}%s",
                            cam->info.resolution[j].width, cam->info.resolution[j].height, j,
                            (j < cam->info.resolution_count - 1) ? "," : "");
        }
        len += snprintf(response + len, sizeof(response) - len, "]}%s",
                        (i < camera_system.camera_count - 1) ? "," : "");
    }
    len += snprintf(response + len, sizeof(response) - len,
                    "],\"activated\":[");

    for (int i = 0; i < camera_system.camera_count; ++i) {
        if (camera_system.cameras[i].info.if_streaming) {
            camera_t *cam = &camera_system.cameras[i];

            len += snprintf(response + len, sizeof(response) - len,
                            "{\"id\":\"%s\",\"resolution\":{\"width\":%d,\"height\":%d, \"index\":%d}}%s",
                            cam->id,
                            cam->info.resolution[cam->info.active_resolution].width,
                            cam->info.resolution[cam->info.active_resolution].height,
                            cam->info.active_resolution,
                            (i < camera_system.active_camera_count - 1) ? "," : "");
        }
    }
    len += snprintf(response + len, sizeof(response) - len, "]}");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// API: POST /api/active
static esp_err_t post_active_handler(httpd_req_t *req)
{
    char content[128];
    int ret = httpd_req_recv(req, content, sizeof(content));
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read request body");
        return ESP_FAIL;
    }

    int id = 0, width = 0, height = 0, resolution_index = 0;
    sscanf(content, "{\"id\":\"%d\",\"resolution\":{\"width\":%d,\"height\":%d,\"index\":%d}}", &id, &width, &height, &resolution_index);

    // Open streaming
    printf("id: %d, resolution_index %d\n", id, resolution_index);
    app_uvc_control_dev_by_index(id, true, resolution_index);
    camera_system.active_camera_count++;
    // Wait for camera to start streaming
    vTaskDelay(200 / portTICK_PERIOD_MS);

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"code\":200}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nAccess-Control-Allow-Origin: *\r\nIndex: %d\r\nContent-Length: %u\r\n\r\n";

static esp_err_t transfer_stream_frame(httpd_req_t *req, uvc_host_frame_t *frame, int stream_id)
{
    esp_err_t res = ESP_OK;
    char *part_buf[128];
    if (frame == NULL) {
        ESP_LOGE(TAG, "Camera capture failed");
        res = ESP_FAIL;
    }

    if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }

    if (res == ESP_OK) {
        size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, stream_id, frame->data_len);
        res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }

    if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, (const char *)frame->data, frame->data_len);
    }

    return res;
}

// API: GET /api/stream/*
static esp_err_t get_stream_handler(httpd_req_t *req)
{
    esp_err_t res = ESP_OK;
    const char *uri = req->uri;
    ESP_LOGI(TAG, "Received request for URI: %s", uri);

    const char *last_slash = strrchr(uri, '/');
    if (last_slash == NULL || *(last_slash + 1) == '\0') {
        ESP_LOGE(TAG, "Invalid URI format");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Invalid stream ID");
        return ESP_FAIL;
    }

    int stream_id = atoi(last_slash + 1);
    ESP_LOGI(TAG, "Extracted stream ID: %d", stream_id);

    httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    int frame_null_cnt = 0;
    int frame_cnt = 0;

    while (true) {
        uvc_host_frame_t *frame = app_uvc_get_frame_by_index(stream_id);
        if (frame != NULL) {
            frame_null_cnt = 0;
            // Discard the first 5 frames to avoid poor-quality frames generated by the camera.
            if (frame_cnt < 5) {
                ++frame_cnt;
            } else {
                res = transfer_stream_frame(req, frame, stream_id);
            }
            app_uvc_return_frame_by_index(stream_id, frame);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "Stream transfer failed");
                break;
            }
        } else {
            ESP_LOGI(TAG, "Camera #%d: frame is NULL", stream_id);
            frame_null_cnt++;
            if (frame_null_cnt > 10) {
                break;
            }
        }
    }
    ESP_LOGE(TAG, "trans error\n");
    res = httpd_resp_send_chunk(req, NULL, 0);

    app_uvc_control_dev_by_index(stream_id, false, -1);
    camera_system.active_camera_count--;

    return res;
}

static esp_err_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 1024 * 6;
    config.task_priority = 10;
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

    httpd_handle_t server_80 = NULL;

    config.core_id = 0;
    if (httpd_start(&server_80, &config) == ESP_OK) {
        httpd_register_uri_handler(server_80, &active_post);
        httpd_register_uri_handler(server_80, &cameras_get);
        httpd_register_uri_handler(server_80, &api_404_get);
        httpd_register_uri_handler(server_80, &stream_get);
        httpd_register_uri_handler(server_80, &static_handler);
    } else {
        goto exit;
    }

    config.task_priority = 10;
    config.core_id = 1;
    config.server_port = 8080;
    config.ctrl_port += 1;
    httpd_handle_t server_8080 = NULL;

    if (httpd_start(&server_8080, &config) == ESP_OK) {
        httpd_register_uri_handler(server_8080, &active_post);
        httpd_register_uri_handler(server_8080, &cameras_get);
        httpd_register_uri_handler(server_8080, &api_404_get);
        httpd_register_uri_handler(server_8080, &stream_get);
        httpd_register_uri_handler(server_8080, &static_handler);
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
