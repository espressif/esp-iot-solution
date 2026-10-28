/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_cache_private.h"
#include "esp_video_init.h"
#include "app_video.h"

#define MAX_BUFFER_COUNT        3
#define MIN_BUFFER_COUNT        2
#define VIDEO_TASK_STACK_SIZE   (4 * 1024)
#define VIDEO_TASK_PRIORITY     4

static const char *TAG = "app_video";

typedef struct {
    int video_fd;
    uint8_t *buffers[MAX_BUFFER_COUNT];
    size_t buffer_size;
    uint32_t buffer_count;
    int held_index;
    uint32_t width;
    uint32_t height;
    enum v4l2_memory memory;
    app_video_frame_operation_cb_t frame_cb;
    TaskHandle_t task_handle;
    int core_id;
    bool stop_requested;
    void *user_data;
} app_video_t;

static app_video_t app_camera_video;

static bool video_task_running(void)
{
    return __atomic_load_n(&app_camera_video.task_handle, __ATOMIC_ACQUIRE) != NULL;
}

esp_err_t app_video_main(i2c_master_bus_handle_t i2c_bus_handle)
{
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
    esp_video_init_csi_config_t csi_config = {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT,
                .scl_pin = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN,
                .sda_pin = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN,
            },
            .freq = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ,
        },
        .reset_pin = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN,
        .pwdn_pin = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN,
    };
    if (i2c_bus_handle) {
        csi_config.sccb_config.init_sccb = false;
        csi_config.sccb_config.i2c_handle = i2c_bus_handle;
    }
#endif
    const esp_video_init_config_t config = {
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
        .csi = &csi_config,
#endif
    };
    return esp_video_init(&config);
}

#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_VFLIP || CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_HFLIP
static void video_set_flip(int fd, uint32_t id, const char *name)
{
    struct v4l2_ext_control control = {.id = id, .value = 1};
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CTRL_CLASS_USER,
        .count = 1,
        .controls = &control,
    };
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to set %s flip: %s", name, strerror(errno));
    }
}
#endif

int app_video_open(char *dev, video_fmt_t init_fmt)
{
    if (video_task_running()) {
        ESP_LOGE(TAG, "capture task is already running");
        return -1;
    }
    struct v4l2_capability capability = {0};
    struct v4l2_format format = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open %s: %s", dev, strerror(errno));
        return -1;
    }
    if (ioctl(fd, VIDIOC_QUERYCAP, &capability) != 0) {
        ESP_LOGE(TAG, "failed to query video capabilities: %s", strerror(errno));
        goto fail;
    }
    ESP_LOGI(TAG, "%s v%u.%u.%u on %s", capability.driver,
             (unsigned)(capability.version >> 16),
             (unsigned)((capability.version >> 8) & 0xff),
             (unsigned)(capability.version & 0xff), capability.bus_info);

    if (ioctl(fd, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "failed to get capture format: %s", strerror(errno));
        goto fail;
    }
    if (format.fmt.pix.pixelformat != init_fmt) {
        struct v4l2_format requested = {
            .type = format.type,
            .fmt.pix.width = format.fmt.pix.width,
            .fmt.pix.height = format.fmt.pix.height,
            .fmt.pix.pixelformat = init_fmt,
        };
        if (ioctl(fd, VIDIOC_S_FMT, &requested) != 0 ||
                ioctl(fd, VIDIOC_G_FMT, &format) != 0) {
            ESP_LOGE(TAG, "failed to negotiate capture format: %s", strerror(errno));
            goto fail;
        }
    }
    if (format.fmt.pix.pixelformat != init_fmt || !format.fmt.pix.width ||
            !format.fmt.pix.height || !format.fmt.pix.sizeimage) {
        ESP_LOGE(TAG, "unsupported capture format or frame size");
        goto fail;
    }
    app_camera_video.width = format.fmt.pix.width;
    app_camera_video.height = format.fmt.pix.height;
    app_camera_video.buffer_size = format.fmt.pix.sizeimage;
    ESP_LOGI(TAG, "Capture: %" PRIu32 "x%" PRIu32 ", format=0x%08" PRIx32 ", sizeimage=%" PRIu32,
             format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.pixelformat, format.fmt.pix.sizeimage);

#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_VFLIP
    video_set_flip(fd, V4L2_CID_VFLIP, "vertical");
#endif
#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_HFLIP
    video_set_flip(fd, V4L2_CID_HFLIP, "horizontal");
#endif
    return fd;

fail:
    close(fd);
    return -1;
}

esp_err_t app_video_set_bufs(int video_fd, uint32_t fb_num, const void **fb, int held_index, size_t fb_capacity)
{
    if (video_task_running() || !app_camera_video.buffer_size || app_camera_video.buffer_count) {
        return ESP_ERR_INVALID_STATE;
    }
    if (fb_num < MIN_BUFFER_COUNT || fb_num > MAX_BUFFER_COUNT) {
        ESP_LOGE(TAG, "buffer count must be between %d and %d", MIN_BUFFER_COUNT, MAX_BUFFER_COUNT);
        return ESP_FAIL;
    }
    if (held_index < -1 || held_index >= (int)fb_num ||
            (held_index >= 0 && (!fb || fb_num != MAX_BUFFER_COUNT))) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t *buffers[MAX_BUFFER_COUNT] = {0};
    size_t buffer_size = 0, cache_alignment = 0;
#if CONFIG_EXAMPLE_USE_MEMORY_MAPPING
    enum v4l2_memory memory = fb ? V4L2_MEMORY_USERPTR : V4L2_MEMORY_MMAP;
#else
    enum v4l2_memory memory = V4L2_MEMORY_USERPTR;
#endif
    struct v4l2_requestbuffers request = {
        .count = fb_num,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = memory,
    };
    bool requested = false;
    if (ioctl(video_fd, VIDIOC_REQBUFS, &request) != 0) {
        ESP_LOGE(TAG, "failed to request buffers: %s", strerror(errno));
        goto fail;
    }
    requested = true;
    if (request.count != fb_num) {
        ESP_LOGE(TAG, "requested %" PRIu32 " buffers, received %" PRIu32, fb_num, request.count);
        goto fail;
    }
    for (uint32_t i = 0; i < fb_num; i++) {
        struct v4l2_buffer buffer = {
            .type = request.type,
            .memory = request.memory,
            .index = i,
        };
        if (ioctl(video_fd, VIDIOC_QUERYBUF, &buffer) != 0) {
            ESP_LOGE(TAG, "failed to query buffer: %s", strerror(errno));
            goto fail;
        }
        if (buffer.length < app_camera_video.buffer_size || (buffer_size && buffer.length != buffer_size)) {
            ESP_LOGE(TAG, "invalid queried buffer capacity");
            goto fail;
        }
        buffer_size = buffer.length;
        if (request.memory == V4L2_MEMORY_MMAP) {
            void *mapped = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, video_fd, buffer.m.offset);
            if (mapped == MAP_FAILED) {
                ESP_LOGE(TAG, "failed to map buffer: %s", strerror(errno));
                goto fail;
            }
            buffers[i] = mapped;
        } else {
            if (fb) {
                if (!fb[i] || fb_capacity < buffer_size) {
                    ESP_LOGE(TAG, "supplied frame buffer is too small or NULL");
                    goto fail;
                }
                buffers[i] = (uint8_t *)fb[i];
            } else {
                if (!cache_alignment && esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &cache_alignment) != ESP_OK) {
                    goto fail;
                }
                buffers[i] = heap_caps_aligned_calloc(cache_alignment, 1, buffer_size, MALLOC_CAP_SPIRAM);
                if (!buffers[i]) {
                    ESP_LOGE(TAG, "failed to allocate capture buffer");
                    goto fail;
                }
            }
            buffer.m.userptr = (unsigned long)buffers[i];
        }
        if ((int)i != held_index && ioctl(video_fd, VIDIOC_QBUF, &buffer) != 0) {
            ESP_LOGE(TAG, "failed to queue buffer: %s", strerror(errno));
            goto fail;
        }
    }
    memcpy(app_camera_video.buffers, buffers, sizeof(buffers));
    app_camera_video.buffer_size = buffer_size;
    app_camera_video.buffer_count = fb_num;
    app_camera_video.held_index = held_index;
    app_camera_video.memory = request.memory;
    return ESP_OK;

fail:
    for (uint32_t i = 0; i < fb_num; i++) {
        if (buffers[i]) {
            if (request.memory == V4L2_MEMORY_MMAP) {
                munmap(buffers[i], buffer_size);
            } else if (!fb) {
                free(buffers[i]);
            }
        }
    }
    if (requested) {
        request.count = 0;
        ioctl(video_fd, VIDIOC_REQBUFS, &request);
    }
    close(video_fd);
    memset(app_camera_video.buffers, 0, sizeof(app_camera_video.buffers));
    app_camera_video.buffer_count = 0;
    return ESP_FAIL;
}

esp_err_t app_video_get_bufs(int fb_num, void **fb)
{
    if (!fb || fb_num < MIN_BUFFER_COUNT || (uint32_t)fb_num > app_camera_video.buffer_count) {
        return ESP_FAIL;
    }
    for (int i = 0; i < fb_num; i++) {
        fb[i] = app_camera_video.buffers[i];
    }
    return ESP_OK;
}

uint32_t app_video_get_buf_size(void)
{
    return app_camera_video.buffer_size;
}

static esp_err_t video_dequeue_frame(int video_fd, struct v4l2_buffer *buffer)
{
    if (ioctl(video_fd, VIDIOC_DQBUF, buffer) != 0) {
        ESP_LOGE(TAG, "failed to dequeue frame: %s", strerror(errno));
        return ESP_FAIL;
    }
    if (buffer->index >= app_camera_video.buffer_count || buffer->bytesused > app_camera_video.buffer_size) {
        ESP_LOGE(TAG, "invalid dequeued frame");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static inline esp_err_t video_stream_start(int video_fd)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "failed to start stream: %s", strerror(errno));
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t video_stream_stop(int video_fd)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd, VIDIOC_STREAMOFF, &type) != 0) {
        ESP_LOGE(TAG, "failed to stop stream: %s", strerror(errno));
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void video_stream_task(void *arg)
{
    app_video_t *video = arg;
    const int video_fd = video->video_fd;
    for (;;) {
        struct v4l2_buffer buffer = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = video->memory,
        };
        ESP_ERROR_CHECK(video_dequeue_frame(video_fd, &buffer));
        int release_index = video->frame_cb(video->buffers[buffer.index], buffer.index, video->width,
                                            video->height, buffer.bytesused, video->user_data);
        int expected_index = video->held_index >= 0 ? video->held_index : (int)buffer.index;
        ESP_ERROR_CHECK(release_index == expected_index ? ESP_OK : ESP_ERR_INVALID_STATE);
        if (video->held_index >= 0) {
            video->held_index = buffer.index;
        }
        buffer.index = release_index;
        if (video->memory == V4L2_MEMORY_USERPTR) {
            buffer.m.userptr = (unsigned long)video->buffers[buffer.index];
            buffer.length = video->buffer_size;
        }
        if (ioctl(video_fd, VIDIOC_QBUF, &buffer) != 0) {
            ESP_LOGE(TAG, "failed to return frame: %s", strerror(errno));
            ESP_ERROR_CHECK(ESP_FAIL);
        }
        if (__atomic_load_n(&video->stop_requested, __ATOMIC_ACQUIRE)) {
            break;
        }
    }
    ESP_ERROR_CHECK(video_stream_stop(video_fd));
    __atomic_store_n(&video->stop_requested, false, __ATOMIC_RELAXED);
    __atomic_store_n(&video->task_handle, NULL, __ATOMIC_RELEASE);
    vTaskDelete(NULL);
}

esp_err_t app_video_stream_task_start(int video_fd, int core_id, void *user_data)
{
    if (!app_camera_video.frame_cb || !app_camera_video.buffer_count || video_task_running()) {
        return ESP_ERR_INVALID_STATE;
    }
    app_camera_video.video_fd = video_fd;
    app_camera_video.core_id = core_id;
    app_camera_video.user_data = user_data;
    __atomic_store_n(&app_camera_video.stop_requested, false, __ATOMIC_RELAXED);
    esp_err_t ret = video_stream_start(video_fd);
    if (ret != ESP_OK) {
        return ret;
    }
    if (xTaskCreatePinnedToCore(video_stream_task, "video stream task", VIDEO_TASK_STACK_SIZE,
                                &app_camera_video, VIDEO_TASK_PRIORITY, &app_camera_video.task_handle, core_id) != pdPASS) {
        ESP_LOGE(TAG, "failed to create video stream task");
        video_stream_stop(video_fd);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t app_video_stream_task_restart(int video_fd)
{
    if (video_task_running() || !app_camera_video.buffer_count || video_fd != app_camera_video.video_fd) {
        return ESP_ERR_INVALID_STATE;
    }
    // STREAMOFF clears the queues but keeps the allocated buffers.
    for (uint32_t i = 0; i < app_camera_video.buffer_count; i++) {
        if ((int)i == app_camera_video.held_index) {
            continue;
        }
        struct v4l2_buffer buffer = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = app_camera_video.memory,
            .index = i,
        };
        if (ioctl(video_fd, VIDIOC_QUERYBUF, &buffer) != 0) {
            return ESP_FAIL;
        }
        if (buffer.memory == V4L2_MEMORY_USERPTR) {
            buffer.m.userptr = (unsigned long)app_camera_video.buffers[i];
        }
        if (ioctl(video_fd, VIDIOC_QBUF, &buffer) != 0) {
            return ESP_FAIL;
        }
    }
    return app_video_stream_task_start(video_fd, app_camera_video.core_id, app_camera_video.user_data);
}

esp_err_t app_video_stream_task_stop(int video_fd)
{
    if (!video_task_running() || video_fd != app_camera_video.video_fd) {
        return ESP_ERR_INVALID_STATE;
    }
    __atomic_store_n(&app_camera_video.stop_requested, true, __ATOMIC_RELEASE);
    return ESP_OK;
}

esp_err_t app_video_register_frame_operation_cb(app_video_frame_operation_cb_t operation_cb)
{
    if (!operation_cb) {
        return ESP_ERR_INVALID_ARG;
    }
    if (video_task_running()) {
        return ESP_ERR_INVALID_STATE;
    }
    app_camera_video.frame_cb = operation_cb;
    return ESP_OK;
}
