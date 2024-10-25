/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

#include "avifile.h"
#include "avi_player.h"

static const char *TAG = "avi player";

#define EVENT_FPS_TIME_UP     ((1 << 0))
#define EVENT_START_PLAY      ((1 << 1))
#define EVENT_STOP_PLAY       ((1 << 2))
#define EVENT_DEINIT          ((1 << 3))
#define EVENT_DEINIT_DONE     ((1 << 4))
#define EVENT_VIDEO_BUF_READY ((1 << 5))
#define EVENT_AUDIO_BUF_READY ((1 << 6))

#define EVENT_ALL          (EVENT_FPS_TIME_UP | EVENT_START_PLAY | EVENT_STOP_PLAY | EVENT_DEINIT)

typedef enum {
    PLAY_FILE,
    PLAY_MEMORY,
} play_mode_t;

typedef enum {
    AVI_PARSER_NONE,
    AVI_PARSER_HEADER,
    AVI_PARSER_DATA,
    AVI_PARSER_END,
} avi_play_state_t;

typedef struct {
    play_mode_t mode;
    union {
        struct {
            uint8_t *data;
            uint32_t size;
            uint32_t read_offset;
        } memory;
        struct {
            FILE *avi_file;
        } file;
    };
    uint8_t *pbuffer;
    uint32_t str_size;
    avi_play_state_t state;
    avi_typedef AVI_file;
} avi_data_t;

typedef struct {
    EventGroupHandle_t event_group;
    esp_timer_handle_t timer_handle;
    avi_player_config_t config;
    avi_data_t avi_data;
} avi_player_handle_t;

static avi_player_handle_t *s_avi = NULL;

static uint32_t _REV(uint32_t value)
{
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 |
           (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24;
}

static uint32_t read_frame(avi_data_t *avi, uint8_t *buffer, uint32_t length, uint32_t *fourcc)
{
    AVI_CHUNK_HEAD head;

    if (avi->mode == PLAY_MEMORY) {
        if (sizeof(AVI_CHUNK_HEAD) > (avi->memory.size - avi->memory.read_offset)) {
            ESP_LOGE(TAG, "not enough data for chunk head");
            return 0;
        }
        memcpy(&head, avi->memory.data + avi->memory.read_offset, sizeof(AVI_CHUNK_HEAD));
        avi->memory.read_offset += sizeof(AVI_CHUNK_HEAD);
    } else if (avi->mode == PLAY_FILE) {
        fread(&head, sizeof(AVI_CHUNK_HEAD), 1, avi->file.avi_file);
    }

    if (head.FourCC) {
        /* handle FourCC if needed */
    }
    *fourcc = head.FourCC;

    if (head.size % 2) {
        head.size++;    /*!< add a byte if size is odd */
    }

    if (avi->mode == PLAY_MEMORY) {
        if (head.size > (avi->memory.size - avi->memory.read_offset) || length < head.size) {
            ESP_LOGE(TAG, "frame size %"PRIu32" exceeds available data", head.size);
            return 0;
        }
        memcpy(buffer, avi->memory.data + avi->memory.read_offset, head.size);
        avi->memory.read_offset += head.size;
    } else if (avi->mode == PLAY_FILE) {
        if (length < head.size) {
            ESP_LOGE(TAG, "frame size %"PRIu32" exceeds available data", head.size);
            return 0;
        }
        fread(buffer, head.size, 1, avi->file.avi_file);
    }

    return head.size;
}

static esp_err_t avi_player(void)
{
    static size_t BytesRD;
    static uint32_t Strtype;
    uint32_t buffer_size = s_avi->config.buffer_size;
    int ret;

    switch (s_avi->avi_data.state) {
    case AVI_PARSER_HEADER: {
        if (s_avi->avi_data.mode == PLAY_MEMORY) {
            memcpy(s_avi->avi_data.pbuffer, s_avi->avi_data.memory.data, buffer_size);
            BytesRD = buffer_size;
        } else {
            BytesRD = fread(s_avi->avi_data.pbuffer, buffer_size, 1, s_avi->avi_data.file.avi_file);
        }

        ret = avi_parser(&s_avi->avi_data.AVI_file, s_avi->avi_data.pbuffer, BytesRD);
        if (0 > ret) {
            ESP_LOGE(TAG, "parse failed (%d)", ret);
            xEventGroupSetBits(s_avi->event_group, EVENT_STOP_PLAY);
            return ESP_FAIL;
        }

        /*!< Set the video callback */
        if (s_avi->config.audio_set_clock_cb) {
            s_avi->config.audio_set_clock_cb(
                             s_avi->avi_data.AVI_file.auds_sample_rate,
                             s_avi->avi_data.AVI_file.auds_bits,
                             s_avi->avi_data.AVI_file.auds_channels,
                             s_avi->config.user_data);
        }

        uint32_t fps_time = 1000 * 1000 / s_avi->avi_data.AVI_file.vids_fps;
        ESP_LOGD(TAG, "vids_fps=%d", s_avi->avi_data.AVI_file.vids_fps);
        esp_timer_start_periodic(s_avi->timer_handle, fps_time);

        if (s_avi->avi_data.mode == PLAY_MEMORY) {
            s_avi->avi_data.memory.read_offset = s_avi->avi_data.AVI_file.movi_start;
        } else {
            fseek(s_avi->avi_data.file.avi_file, s_avi->avi_data.AVI_file.movi_start, SEEK_SET);
        }

        s_avi->avi_data.state = AVI_PARSER_DATA;
        BytesRD = 0;
    }
    case AVI_PARSER_DATA: {
        /*!< clear event */
        xEventGroupClearBits(s_avi->event_group, EVENT_AUDIO_BUF_READY | EVENT_VIDEO_BUF_READY);
        while (1) {
            s_avi->avi_data.str_size = read_frame(&s_avi->avi_data, s_avi->avi_data.pbuffer, buffer_size, &Strtype);
            ESP_LOGD(TAG, "type=%"PRIu32", size=%"PRIu32"", Strtype, s_avi->avi_data.str_size);
            BytesRD += s_avi->avi_data.str_size + 8;

            if (BytesRD >= s_avi->avi_data.AVI_file.movi_size) {
                ESP_LOGI(TAG, "play end");
                s_avi->avi_data.state = AVI_PARSER_END;
                xEventGroupSetBits(s_avi->event_group, EVENT_STOP_PLAY);
                return ESP_OK;
            }

            if ((Strtype & 0xFFFF0000) == DC_ID) { // Display frame
                int64_t fr_end = esp_timer_get_time();
                if (s_avi->config.video_cb) {
                    frame_data_t data = {
                        .data = s_avi->avi_data.pbuffer,
                        .data_bytes = s_avi->avi_data.str_size,
                        .type = FRAME_TYPE_VIDEO,
                        .video_info.width = s_avi->avi_data.AVI_file.vids_width,
                        .video_info.height = s_avi->avi_data.AVI_file.vids_height,
                        .video_info.frame_format = s_avi->avi_data.AVI_file.vids_format,
                    };
                    s_avi->config.video_cb(&data, s_avi->config.user_data);
                }
                xEventGroupSetBits(s_avi->event_group, EVENT_VIDEO_BUF_READY);
                ESP_LOGD(TAG, "Draw %"PRIu32"ms", (uint32_t)((esp_timer_get_time() - fr_end) / 1000));
                break;
            } else if ((Strtype & 0xFFFF0000) == WB_ID) { // Audio output
                if (s_avi->config.audio_cb) {
                    frame_data_t data = {
                        .data = s_avi->avi_data.pbuffer,
                        .data_bytes = s_avi->avi_data.str_size,
                        .type = FRAME_TYPE_AUDIO,
                        .audio_info.channel = s_avi->avi_data.AVI_file.auds_channels,
                        .audio_info.bits_per_sample = s_avi->avi_data.AVI_file.auds_bits,
                        .audio_info.sample_rate = s_avi->avi_data.AVI_file.auds_sample_rate,
                        .audio_info.format = FORMAT_PCM,
                    };
                    s_avi->config.audio_cb(&data, s_avi->config.user_data);
                }
                xEventGroupSetBits(s_avi->event_group, EVENT_AUDIO_BUF_READY);
            } else {
                ESP_LOGE(TAG, "unknown frame %"PRIx32"", Strtype);
                xEventGroupSetBits(s_avi->event_group, EVENT_STOP_PLAY);
                return ESP_FAIL;
            }
        }
        break;
    }
    case AVI_PARSER_END:
        esp_timer_stop(s_avi->timer_handle);
        if (s_avi->avi_data.mode == PLAY_FILE) {
            fclose(s_avi->avi_data.file.avi_file);
        }

        s_avi->avi_data.state = AVI_PARSER_NONE;
        if (s_avi->config.avi_play_end_cb) {
            s_avi->config.avi_play_end_cb(s_avi->config.user_data);
        }

        break;
    default:
        break;
    }
    return ESP_OK;
}

static void avi_player_task(void *args)
{
    EventBits_t uxBits;
    bool exit = false;
    while (!exit) {
        uxBits = xEventGroupWaitBits(s_avi->event_group, EVENT_ALL, pdTRUE, pdFALSE, portMAX_DELAY);
        if (uxBits & EVENT_STOP_PLAY) {
            s_avi->avi_data.state = AVI_PARSER_END;
            esp_err_t ret = avi_player();
            if (ret != ESP_OK) {
                ESP_LOGI(TAG, "AVI Perse failed");
            }
        }

        if (uxBits & EVENT_START_PLAY) {
            s_avi->avi_data.state = AVI_PARSER_HEADER;
            esp_err_t ret = avi_player();
            if (ret != ESP_OK) {
                ESP_LOGI(TAG, "AVI Perse failed");
            }
        }

        if (uxBits & EVENT_FPS_TIME_UP) {
            esp_err_t ret = avi_player();
            if (ret != ESP_OK) {
                ESP_LOGI(TAG, "AVI Perse failed");
            }
        }

        if (uxBits & EVENT_DEINIT) {
            exit = true;
            break;
        }

    }
    xEventGroupSetBits(s_avi->event_group, EVENT_DEINIT_DONE);
    vTaskDelete(NULL);
}

esp_err_t avi_player_get_video_buffer(void **buffer, size_t *buffer_size, video_frame_info_t *info, TickType_t ticks_to_wait)
{
    ESP_RETURN_ON_FALSE(buffer != NULL, ESP_ERR_INVALID_ARG, TAG, "buffer can’t be NULL");
    ESP_RETURN_ON_FALSE(info != NULL, ESP_ERR_INVALID_ARG, TAG, "info can’t be NULL");
    ESP_RETURN_ON_FALSE(buffer_size != NULL, ESP_ERR_INVALID_ARG, TAG, "buffer_size can’t be 0");

    /*!< Get the EVENT_VIDEO_BUF_READY */
    EventBits_t uxBits = xEventGroupWaitBits(s_avi->event_group, EVENT_VIDEO_BUF_READY, pdTRUE, pdFALSE, ticks_to_wait);
    if (!(uxBits & EVENT_VIDEO_BUF_READY)) {
        return ESP_ERR_TIMEOUT;
    }

    if (*buffer_size < s_avi->avi_data.str_size) {
        ESP_LOGE(TAG, "buffer size is too small");
        return ESP_ERR_NO_MEM;
    }

    memcpy(*buffer, s_avi->avi_data.pbuffer, s_avi->avi_data.str_size);
    *buffer_size = s_avi->avi_data.str_size;
    info->width = s_avi->avi_data.AVI_file.vids_width;
    info->height = s_avi->avi_data.AVI_file.vids_height;
    info->frame_format = s_avi->avi_data.AVI_file.vids_format;
    return ESP_OK;
}

esp_err_t avi_player_get_audio_buffer(void **buffer, size_t *buffer_size, audio_frame_info_t *info, TickType_t ticks_to_wait)
{
    ESP_RETURN_ON_FALSE(buffer != NULL, ESP_ERR_INVALID_ARG, TAG, "buffer can’t be NULL");
    ESP_RETURN_ON_FALSE(info != NULL, ESP_ERR_INVALID_ARG, TAG, "info can’t be NULL");
    ESP_RETURN_ON_FALSE(buffer_size != NULL, ESP_ERR_INVALID_ARG, TAG, "buffer_size can’t be 0");

    /*!< Get the EVENT_AUDIO_BUF_READY */
    EventBits_t uxBits = xEventGroupWaitBits(s_avi->event_group, EVENT_AUDIO_BUF_READY, pdTRUE, pdFALSE, ticks_to_wait);
    if (!(uxBits & EVENT_AUDIO_BUF_READY)) {
        return ESP_ERR_TIMEOUT;
    }

    if (*buffer_size < s_avi->avi_data.str_size) {
        ESP_LOGE(TAG, "buffer size is too small");
        return ESP_ERR_NO_MEM;
    }

    memcpy(*buffer, s_avi->avi_data.pbuffer, s_avi->avi_data.str_size);
    *buffer_size = s_avi->avi_data.str_size;
    info->channel = s_avi->avi_data.AVI_file.auds_channels;
    info->bits_per_sample = s_avi->avi_data.AVI_file.auds_bits;
    info->sample_rate = s_avi->avi_data.AVI_file.auds_sample_rate;
    info->format = FORMAT_PCM;
    return ESP_OK;
}

esp_err_t avi_player_play_from_memory(uint8_t *avi_data, size_t avi_size)
{
    ESP_RETURN_ON_FALSE(s_avi->avi_data.state == AVI_PARSER_NONE, ESP_ERR_INVALID_STATE, TAG, "AVI player not ready");
    s_avi->avi_data.mode = PLAY_MEMORY;
    s_avi->avi_data.memory.data = avi_data;
    s_avi->avi_data.memory.size = avi_size;
    s_avi->avi_data.memory.read_offset = 0;
    xEventGroupSetBits(s_avi->event_group, EVENT_START_PLAY);
    return ESP_OK;
}

esp_err_t avi_player_play_from_file(const char *filename)
{
    ESP_RETURN_ON_FALSE(s_avi->avi_data.state == AVI_PARSER_NONE, ESP_ERR_INVALID_STATE, TAG, "AVI player not ready");

    s_avi->avi_data.mode = PLAY_FILE;
    s_avi->avi_data.file.avi_file = fopen(filename, "rb");
    if (s_avi->avi_data.file.avi_file == NULL) {
        ESP_LOGE(TAG, "Cannot open %s", filename);
        return ESP_FAIL;
    }
    xEventGroupSetBits(s_avi->event_group, EVENT_START_PLAY);
    return ESP_OK;
}

esp_err_t avi_player_play_stop(void)
{
    ESP_RETURN_ON_FALSE(s_avi->avi_data.state == AVI_PARSER_HEADER || s_avi->avi_data.state == AVI_PARSER_DATA,
                        ESP_ERR_INVALID_STATE, TAG, "AVI player not playing");
    xEventGroupSetBits(s_avi->event_group, EVENT_STOP_PLAY);
    return ESP_OK;
}

static void esp_timer_cb(void *arg)
{
    /*!< Give the Event */
    xEventGroupSetBits(s_avi->event_group, EVENT_FPS_TIME_UP);
}

esp_err_t avi_player_init(avi_player_config_t config)
{
    ESP_LOGI(TAG, "AVI Player Version: %d.%d.%d", AVI_PLAYER_VER_MAJOR, AVI_PLAYER_VER_MINOR, AVI_PLAYER_VER_PATCH);
    ESP_RETURN_ON_FALSE(s_avi == NULL, ESP_ERR_INVALID_STATE, TAG, "avi player already initialized");
    s_avi = (avi_player_handle_t *)calloc(1, sizeof(avi_player_handle_t));
    s_avi->config = config;

    if (s_avi->config.buffer_size == 0) {
        s_avi->config.buffer_size = 20 * 1024;
    }
    if (s_avi->config.priority == 0) {
        s_avi->config.priority = 5;
    }

    s_avi->avi_data.pbuffer = malloc(s_avi->config.buffer_size);
    ESP_RETURN_ON_FALSE(s_avi->avi_data.pbuffer != NULL, ESP_ERR_NO_MEM, TAG, "Cannot alloc memory for player");

    esp_timer_create_args_t timer = {0};
    timer.arg = NULL;
    timer.callback = esp_timer_cb;
    timer.dispatch_method = ESP_TIMER_TASK;
    timer.name = "avi_player_timer";
    s_avi->event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_avi->event_group != NULL, ESP_ERR_NO_MEM, TAG, "Cannot create event group");

    esp_timer_create(&timer, &s_avi->timer_handle);
    xTaskCreatePinnedToCore(avi_player_task, "avi_player", 4096, NULL, s_avi->config.priority, NULL, s_avi->config.coreID);
    return ESP_OK;
}

esp_err_t avi_player_deinit(void)
{
    if (s_avi == NULL) {
        return ESP_FAIL;
    }

    xEventGroupSetBits(s_avi->event_group, EVENT_DEINIT);
    EventBits_t uxBits = xEventGroupWaitBits(s_avi->event_group, EVENT_DEINIT_DONE, pdTRUE, pdTRUE, pdMS_TO_TICKS(1000));
    if (!(uxBits & EVENT_DEINIT_DONE)) {
        ESP_LOGE(TAG, "AVI player deinit timeout");
        return ESP_ERR_TIMEOUT;
    }

    if (s_avi->timer_handle != NULL) {
        esp_timer_stop(s_avi->timer_handle);
        esp_timer_delete(s_avi->timer_handle);
    }

    if (s_avi->avi_data.pbuffer != NULL) {
        free(s_avi->avi_data.pbuffer);
    }

    if (s_avi->event_group != NULL) {
        vEventGroupDelete(s_avi->event_group);
    }

    free(s_avi);
    s_avi = NULL;
    return ESP_OK;
}
