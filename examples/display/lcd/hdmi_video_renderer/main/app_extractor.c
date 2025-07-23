/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>      // Add support for open, O_RDONLY, etc.
#include <unistd.h>     // Add support for read, close, etc.
#include <sys/types.h>  // Add support for lseek, etc.
#include <math.h>
#include <errno.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "app_extractor.h"
#include "esp_extractor.h"
#include "esp_extractor_reg.h"
#include "esp_mp4_extractor.h"
#include "esp_ts_extractor.h"
#include "esp_avi_extractor.h"
#include "esp_flv_extractor.h"
#include "esp_wav_extractor.h"
#include "esp_hls_extractor.h"
#include "esp_audio_es_extractor.h"
#include "esp_ogg_extractor.h"
#include "mem_pool.h"

// Include correct audio codec headers
#include "simple_dec/esp_audio_simple_dec.h"
#include "decoder/esp_audio_dec.h"
#include "esp_audio_types.h"
#include "esp_timer.h"

// Required decoder headers
#include "decoder/impl/esp_aac_dec.h"
#include "decoder/impl/esp_mp3_dec.h"

// Optional decoder headers (Kconfig controlled)
#if defined(CONFIG_AUDIO_DEC_FLAC_ENABLE)
#include "decoder/impl/esp_flac_dec.h"
#endif
#if defined(CONFIG_AUDIO_DEC_OPUS_ENABLE)
#include "decoder/impl/esp_opus_dec.h"
#endif
#if defined(CONFIG_AUDIO_DEC_VORBIS_ENABLE)
#include "decoder/impl/esp_vorbis_dec.h"
#endif
#if defined(CONFIG_AUDIO_DEC_ADPCM_ENABLE)
#include "decoder/impl/esp_adpcm_dec.h"
#endif

static const char *TAG = "app_extractor";

#define RETURN_ON_FAIL(ret) do { \
    if (ret != ESP_OK) { \
        ESP_LOGE(TAG, "Error at line %d: %d", __LINE__, ret); \
        return ret; \
    } \
} while (0)

#define GOTO_ON_FAIL(ret, label) do { \
    if (ret != ESP_OK) { \
        ESP_LOGE(TAG, "Error at line %d: %d", __LINE__, ret); \
        goto label; \
    } \
} while (0)

/* Audio frame item for queue */
typedef struct {
    uint8_t *buffer;
    uint32_t size;
    uint32_t pts;
} audio_frame_item_t;

/**
 * @brief App extractor context structure
 */
typedef struct app_extractor_t {
    esp_extractor_handle_t  extractor;
    int                     file;
    app_extractor_frame_cb_t frame_cb;
    bool                    extract_video;
    bool                    extract_audio;
    bool                    has_video;
    bool                    has_audio;
    uint32_t                video_width;
    uint32_t                video_height;
    uint32_t                video_fps;
    uint32_t                video_duration;
    uint32_t                audio_sample_rate;
    uint8_t                 audio_channels;
    uint8_t                 audio_bits;
    uint32_t                audio_duration;
    uint32_t                last_video_pts;
    uint32_t                last_audio_pts;
    extractor_video_format_t video_format;
    extractor_audio_format_t audio_format;
    bool                    eos_reached;

    // A/V sync support (Kconfig controlled)
    bool                   sync_enabled;
    uint32_t               sync_threshold_ms;
    SemaphoreHandle_t      sync_mutex;
    int64_t                base_time;

    // Audio decoder
    esp_audio_simple_dec_handle_t audio_decoder;
    bool                   audio_decoder_open;
    uint8_t                *audio_buffer;
    uint32_t               audio_buffer_size;
    esp_codec_dev_handle_t audio_dev;

    // Audio task and queue
    TaskHandle_t           audio_task_handle;
    QueueHandle_t          audio_queue;
    bool                   audio_task_running;

    // Frame rate control
    uint32_t               frame_interval_ms;
    int64_t                last_frame_time;
} app_extractor_t;

/* Forward declarations */
static esp_err_t process_audio_frame(app_extractor_t *extractor, uint8_t *buffer, uint32_t buffer_size, uint32_t pts);

/**
 * @brief File I/O wrapper functions for ESP Extractor
 */
static void *_file_open(char *url, void *ctx)
{
    int fd = open(url, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open file: %s", url);
        return NULL;
    }
    return (void*)(intptr_t)fd;
}

static int _file_read(void *data, uint32_t size, void *ctx)
{
    int fd = (int)(intptr_t)ctx;
    ssize_t bytes_read = read(fd, data, size);
    if (bytes_read < 0) {
        ESP_LOGE(TAG, "File read error: %d", errno);
        return 0;
    }
    return (int)bytes_read;
}

static int _file_seek(uint32_t position, void *ctx)
{
    int fd = (int)(intptr_t)ctx;
    off_t result = lseek(fd, position, SEEK_SET);
    if (result < 0) {
        ESP_LOGE(TAG, "File seek error: %d", errno);
        return -1;
    }
    return 0;
}

static int _file_close(void *ctx)
{
    int fd = (int)(intptr_t)ctx;
    int result = close(fd);
    if (result < 0) {
        ESP_LOGE(TAG, "File close error: %d", errno);
        return -1;
    }
    return 0;
}

static uint32_t _file_size(void *ctx)
{
    int fd = (int)(intptr_t)ctx;
    off_t current = lseek(fd, 0, SEEK_CUR);
    off_t end = lseek(fd, 0, SEEK_END);
    lseek(fd, current, SEEK_SET);
    return end <= 0 ? 0 : (uint32_t)end;
}

/**
 * @brief Audio processing task with optimized queue handling
 */
static void audio_task(void *arg)
{
    app_extractor_t *extractor = (app_extractor_t *)arg;
    audio_frame_item_t *frame_item;
    uint32_t processed_frames = 0;

    while (extractor->audio_task_running) {
        if (xQueueReceive(extractor->audio_queue, &frame_item,
                          pdMS_TO_TICKS(AUDIO_QUEUE_TIMEOUT_MS))) {

            esp_err_t ret = process_audio_frame(extractor, frame_item->buffer,
                                                frame_item->size, frame_item->pts);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to process audio frame: %d", ret);
            }

            free(frame_item);  // Free entire structure
            processed_frames++;

            // Log every 100 frames to reduce overhead
            if (processed_frames % 100 == 0) {
                ESP_LOGD(TAG, "Audio processed %" PRIu32 " frames", processed_frames);
            }
        }
    }

    ESP_LOGI(TAG, "Audio task stopped, processed %" PRIu32 " frames", processed_frames);
    extractor->audio_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Start audio processing task
 */
static esp_err_t start_audio_task(app_extractor_t *extractor)
{
    if (extractor->audio_task_running || !extractor->extract_audio) {
        return ESP_OK;
    }

    extractor->audio_task_running = true;

    BaseType_t ret = xTaskCreate(audio_task, "audio_task",
                                 AUDIO_TASK_STACK_SIZE, extractor,
                                 AUDIO_TASK_PRIORITY,
                                 &extractor->audio_task_handle);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio task");
        extractor->audio_task_running = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Stop audio processing task and clean up queue
 */
static void stop_audio_task(app_extractor_t *extractor)
{
    if (!extractor->audio_task_running) {
        return;
    }

    extractor->audio_task_running = false;

    // Wait for task to exit
    while (extractor->audio_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Clean up remaining audio frames in queue
    audio_frame_item_t *frame_item;
    while (xQueueReceive(extractor->audio_queue, &frame_item, 0)) {
        free(frame_item);
    }
}

/**
 * @brief Control frame rate for proper playback timing
 */
static void control_frame_rate(app_extractor_t *extractor)
{
    int64_t current_time = esp_timer_get_time() / 1000;

    // Calculate target time for this frame
    int64_t target_time = extractor->last_frame_time + extractor->frame_interval_ms;

    if (current_time < target_time) {
        uint32_t delay = target_time - current_time;
        if (delay > 0 && delay < 1000) { // Sanity check: delay should be reasonable
            vTaskDelay(pdMS_TO_TICKS(delay));
        }
    }

    extractor->last_frame_time = esp_timer_get_time() / 1000;
}

/**
 * @brief Register all supported extractors for JPEG decoding
 *
 * @return ESP_OK on success, or an error code
 */
static esp_err_t register_all_extractors(void)
{
    esp_err_t ret;

    ret = esp_mp4_extractor_register();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MP4 extractor: %d", ret);
        return ret;
    }

    ret = esp_avi_extractor_register();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register AVI extractor: %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "Extractors registered successfully");
    return ESP_OK;
}

/**
 * @brief Register audio decoders
 *
 * @return ESP_OK on success, or error code
 */
static esp_err_t register_audio_decoders(void)
{
    esp_err_t ret = ESP_OK;

    // Register AAC decoder (critical for MP4 files)
    ret = esp_aac_dec_register();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register AAC decoder: %d", ret);
        return ret;
    }

    ret = esp_mp3_dec_register();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register MP3 decoder: %d", ret);
    }

    // Register optional decoders based on Kconfig
#if defined(CONFIG_AUDIO_DEC_FLAC_ENABLE)
    ret = esp_flac_dec_register();
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Failed to register FLAC decoder: %d", ret);
    }
#endif

#if defined(CONFIG_AUDIO_DEC_OPUS_ENABLE)
    ret = esp_opus_dec_register();
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Failed to register OPUS decoder: %d", ret);
    }
#endif

#if defined(CONFIG_AUDIO_DEC_VORBIS_ENABLE)
    ret = esp_vorbis_dec_register();
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Failed to register VORBIS decoder: %d", ret);
    }
#endif

#if defined(CONFIG_AUDIO_DEC_ADPCM_ENABLE)
    ret = esp_adpcm_dec_register();
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Failed to register ADPCM decoder: %d", ret);
    }
#endif

    ESP_LOGI(TAG, "Audio decoders registered successfully");
    return ESP_OK;
}

/**
 * @brief Initialize audio decoder based on audio format
 *
 * @param extractor App extractor handle
 * @return ESP_OK on success, or error code
 */
static esp_err_t init_audio_decoder(app_extractor_t *extractor)
{
    if (extractor->audio_decoder != NULL && extractor->audio_decoder_open) {
        return ESP_OK;
    }

    // Close existing decoder if any
    if (extractor->audio_decoder != NULL) {
        if (extractor->audio_decoder_open) {
            esp_audio_dec_close(extractor->audio_decoder);
        }
        extractor->audio_decoder = NULL;
        extractor->audio_decoder_open = false;
    }

    esp_err_t ret = ESP_OK;

    // Set decoder type based on audio format
    switch (extractor->audio_format) {
    case EXTRACTOR_AUDIO_FORMAT_AAC: {
        ESP_LOGI(TAG, "Using AAC decoder (sample_rate=%" PRIu32 ", channel=%u, bits=%u)",
                 extractor->audio_sample_rate, extractor->audio_channels, extractor->audio_bits);

        // AAC decoder specific configuration
        esp_aac_dec_cfg_t aac_cfg = {
            .sample_rate = extractor->audio_sample_rate,
            .channel = extractor->audio_channels,
            .bits_per_sample = extractor->audio_bits,
            .no_adts_header = false,  // Assuming ADTS header is present
            .aac_plus_enable = true   // Enable AAC+ support for better quality
        };

        // Decoder configuration structure
        esp_audio_dec_cfg_t dec_cfg = {
            .type = ESP_AUDIO_TYPE_AAC,
            .cfg = &aac_cfg,
            .cfg_sz = sizeof(aac_cfg)
        };

        // Try unregistering all decoders first to ensure clean state
        esp_audio_dec_unregister_all();

        // Re-register required decoder
        ret = esp_aac_dec_register();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register AAC decoder: %d", ret);
            return ret;
        }

        // Open decoder with esp_audio_dec API
        ret = esp_audio_dec_open(&dec_cfg, &extractor->audio_decoder);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open AAC decoder: %d", ret);
            return ret;
        }

        extractor->audio_decoder_open = true;
        ESP_LOGI(TAG, "AAC decoder initialized with parameters: sample_rate=%" PRIu32 ", channel=%u, bits=%u",
                 extractor->audio_sample_rate, extractor->audio_channels, extractor->audio_bits);
        break;
    }

    case EXTRACTOR_AUDIO_FORMAT_MP3: {
        ESP_LOGI(TAG, "Using MP3 decoder");

        // For MP3, we'll use the basic configuration without specific settings
        esp_audio_dec_cfg_t dec_cfg = {
            .type = ESP_AUDIO_TYPE_MP3,
            .cfg = NULL,
            .cfg_sz = 0
        };

        // Register MP3 decoder
        ret = esp_mp3_dec_register();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register MP3 decoder: %d", ret);
            return ret;
        }

        // Open decoder
        ret = esp_audio_dec_open(&dec_cfg, &extractor->audio_decoder);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open MP3 decoder: %d", ret);
            return ret;
        }

        extractor->audio_decoder_open = true;
        break;
    }

#if defined(CONFIG_AUDIO_DEC_FLAC_ENABLE)
    case EXTRACTOR_AUDIO_FORMAT_FLAC: {
        esp_audio_dec_cfg_t dec_cfg = {
            .type = ESP_AUDIO_TYPE_FLAC,
            .cfg = NULL,
            .cfg_sz = 0
        };

        ret = esp_flac_dec_register();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register FLAC decoder: %d", ret);
            return ret;
        }

        ret = esp_audio_dec_open(&dec_cfg, &extractor->audio_decoder);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open FLAC decoder: %d", ret);
            return ret;
        }

        extractor->audio_decoder_open = true;
        break;
    }
#endif

    default:
        ESP_LOGE(TAG, "Unsupported audio format: %d", extractor->audio_format);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

/**
 * @brief Process audio frame with optimized timing control
 */
static esp_err_t process_audio_frame(app_extractor_t *extractor, uint8_t *buffer, uint32_t buffer_size, uint32_t pts)
{
    if (extractor->audio_dev == NULL) {
        return ESP_OK;
    }

    // PCM direct playback
    if (extractor->audio_format == EXTRACTOR_AUDIO_FORMAT_PCM) {
        static bool first_frame = true;
        if (first_frame) {
            esp_codec_dev_sample_info_t fs = {
                .sample_rate = extractor->audio_sample_rate,
                .channel = extractor->audio_channels,
                .bits_per_sample = extractor->audio_bits
            };
            esp_codec_dev_open(extractor->audio_dev, &fs);
            first_frame = false;
        }

        esp_err_t ret = esp_codec_dev_write(extractor->audio_dev, buffer, buffer_size);

        // Precise timing control based on audio frame duration
        if (ret == ESP_OK) {
            uint32_t bytes_per_sample = extractor->audio_channels * (extractor->audio_bits / 8);
            uint32_t frame_duration_ms = (buffer_size * 1000) / (extractor->audio_sample_rate * bytes_per_sample);

            // Use 60% of frame duration to prevent audio buffer underrun while maintaining timing
            if (frame_duration_ms > 0 && frame_duration_ms < 50) {
                vTaskDelay(pdMS_TO_TICKS(frame_duration_ms * 6 / 10));
            }
        }
        return ret;
    }

    // Initialize decoder for compressed audio
    if (!extractor->audio_decoder_open) {
        esp_err_t ret = init_audio_decoder(extractor);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    // Ensure audio buffer exists
    if (extractor->audio_buffer == NULL) {
        extractor->audio_buffer_size = HDMI_AUDIO_BUFFER_SIZE;
        extractor->audio_buffer = heap_caps_aligned_calloc(64, 1, extractor->audio_buffer_size, MALLOC_CAP_INTERNAL);
        if (extractor->audio_buffer == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    esp_audio_dec_in_raw_t raw = { .buffer = buffer, .len = buffer_size };
    uint32_t total_decoded = 0;

    // Decode and play audio data
    while (raw.len > 0) {
        esp_audio_dec_out_frame_t out_frame = {
            .buffer = extractor->audio_buffer,
            .len = extractor->audio_buffer_size
        };

        esp_err_t ret = esp_audio_dec_process(extractor->audio_decoder, &raw, &out_frame);

        // Handle buffer resize if needed
        if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
            uint32_t new_size = out_frame.needed_size + 1024;
            uint8_t *new_buffer = heap_caps_aligned_calloc(64, 1, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (new_buffer == NULL) {
                return ESP_ERR_NO_MEM;
            }

            if (extractor->audio_buffer != NULL) {
                heap_caps_free(extractor->audio_buffer);
            }
            extractor->audio_buffer = new_buffer;
            extractor->audio_buffer_size = new_size;
            continue;
        }

        if (ret != ESP_OK) {
            if (ret == ESP_AUDIO_ERR_FAIL) {
                break;
            }
            raw.len = 0;
        }

        if (out_frame.decoded_size > 0) {
            // Configure audio device on first decoded frame
            static bool first_decoded = true;
            if (first_decoded) {
                esp_codec_dev_sample_info_t fs = {
                    .sample_rate = extractor->audio_sample_rate,
                    .channel = extractor->audio_channels,
                    .bits_per_sample = extractor->audio_bits
                };
                esp_codec_dev_open(extractor->audio_dev, &fs);
                first_decoded = false;
            }

            esp_err_t write_ret = esp_codec_dev_write(extractor->audio_dev, out_frame.buffer, out_frame.decoded_size);
            total_decoded += out_frame.decoded_size;

            // Optimized timing control for decoded audio
            if (write_ret == ESP_OK) {
                uint32_t bytes_per_sample = extractor->audio_channels * (extractor->audio_bits / 8);
                uint32_t decoded_duration_ms = (out_frame.decoded_size * 1000) / (extractor->audio_sample_rate * bytes_per_sample);

                // Use 40% delay for decoded audio to balance timing and responsiveness
                if (decoded_duration_ms > 0 && decoded_duration_ms < 50) {
                    vTaskDelay(pdMS_TO_TICKS(decoded_duration_ms * 4 / 10));
                }
            }
        }

        // Advance buffer pointer
        if (raw.consumed > 0) {
            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;
        } else {
            raw.len = 0;
        }
    }

    return (total_decoded > 0) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Process extracted frame with optimized routing
 */
static esp_err_t process_frame(extractor_frame_info_t *frame, app_extractor_t *extractor)
{
    if (frame->eos) {
        extractor->eos_reached = true;
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t ret = ESP_OK;

    switch (frame->stream_type) {
    case EXTRACTOR_STREAM_TYPE_VIDEO:
        if (frame->pts) {
            extractor->last_video_pts = frame->pts;
        }

        if (extractor->extract_video && frame->frame_buffer &&
                frame->frame_size > 0 && extractor->frame_cb) {

            control_frame_rate(extractor);
            ret = extractor->frame_cb(frame->frame_buffer, frame->frame_size, true, frame->pts);
        }
        break;

    case EXTRACTOR_STREAM_TYPE_AUDIO:
        if (frame->pts) {
            extractor->last_audio_pts = frame->pts;
        }

        if (extractor->extract_audio && extractor->audio_dev &&
                frame->frame_buffer && frame->frame_size > 0) {

            // Optimized audio frame queuing
            audio_frame_item_t *frame_item = malloc(sizeof(audio_frame_item_t) + frame->frame_size);
            if (frame_item) {
                frame_item->buffer = (uint8_t*)(frame_item + 1);  // Buffer follows struct
                frame_item->size = frame->frame_size;
                frame_item->pts = frame->pts;
                memcpy(frame_item->buffer, frame->frame_buffer, frame->frame_size);

                if (xQueueSend(extractor->audio_queue, &frame_item, 0) != pdTRUE) {
                    free(frame_item);  // Drop frame if queue full
                }
            } else {
                ESP_LOGW(TAG, "Audio frame memory allocation failed");
            }
        }
        break;

    default:
        break;
    }

    // Release frame buffer back to output pool (prevent memory leak)
    if (frame->frame_buffer) {
        mem_pool_free(esp_extractor_get_output_pool(extractor->extractor), frame->frame_buffer);
        frame->frame_buffer = NULL;
    }

    return ret;
}

/**
 * @brief Validate MPEG format compatibility for ESP32-P4
 *
 * This function ensures that only MPEG-compatible formats are processed:
 * - Video: MJPEG only (ESP32-P4 hardware JPEG decoder limitation)
 * - Audio: MPEG-related formats (AAC, MP3, etc.)
 *
 * @param extractor App extractor handle
 * @return ESP_OK if compatible, ESP_ERR_NOT_SUPPORTED if incompatible
 */
static esp_err_t validate_mpeg_compatibility(app_extractor_t *extractor)
{
    if (extractor->has_video) {
        switch (extractor->video_format) {
        case EXTRACTOR_VIDEO_FORMAT_MJPEG:
            ESP_LOGI(TAG, "Video: MJPEG format - ESP32-P4 hardware accelerated");
            break;

        case EXTRACTOR_VIDEO_FORMAT_H264:
            ESP_LOGE(TAG, "Video: H.264 format not supported - use MJPEG instead");
            return ESP_ERR_NOT_SUPPORTED;

        default:
            ESP_LOGE(TAG, "Video: Unsupported format %d", extractor->video_format);
            return ESP_ERR_NOT_SUPPORTED;
        }
    }

    if (!extractor->has_video && !extractor->has_audio) {
        ESP_LOGE(TAG, "No valid audio or video streams found");
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

/**
 * @brief Retrieve stream information
 *
 * @param extractor App extractor handle
 * @return ESP_OK on success, or an error code
 */
static esp_err_t get_stream_info(app_extractor_t *extractor)
{
    esp_err_t ret;
    uint16_t audio_num = 0;
    uint16_t video_num = 0;
    extractor_stream_info_t stream_info = {0};

    // Get number of audio streams - tolerate failure for video-only files
    ret = esp_extractor_get_stream_num(extractor->extractor,
                                       EXTRACTOR_STREAM_TYPE_AUDIO, &audio_num);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio stream query failed (OK for video-only files): %d", ret);
        audio_num = 0;
        extractor->has_audio = false;
    }

    // Get number of video streams - must succeed
    ret = esp_extractor_get_stream_num(extractor->extractor,
                                       EXTRACTOR_STREAM_TYPE_VIDEO, &video_num);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get video stream number: %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "Found %d audio and %d video streams", audio_num, video_num);

    // Get audio stream info if available
    if (audio_num > 0) {
        ret = esp_extractor_get_stream_info(extractor->extractor,
                                            EXTRACTOR_STREAM_TYPE_AUDIO, 0, &stream_info);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get audio stream info (ignoring): %d", ret);
            extractor->has_audio = false;
        } else {
            extractor_audio_stream_info_t *audio_info = &stream_info.stream_info.audio_info;
            extractor->has_audio = true;
            // For AVI files, if format is NONE, set it to PCM
            if (audio_info->format == EXTRACTOR_AUDIO_FORMAT_NONE) {
                extractor->audio_format = EXTRACTOR_AUDIO_FORMAT_PCM;
            } else {
                extractor->audio_format = audio_info->format;
            }
            extractor->audio_sample_rate = audio_info->sample_rate;
            extractor->audio_channels = audio_info->channel;
            extractor->audio_bits = audio_info->bits_per_sample;
            extractor->audio_duration = stream_info.duration;

            ESP_LOGI(TAG, "Audio: format=%d, %" PRIu32 "Hz, %dch, %dbits",
                     (int)extractor->audio_format, audio_info->sample_rate,
                     audio_info->channel, audio_info->bits_per_sample);
        }
    } else {
        extractor->has_audio = false;
    }

    // Get video stream info if available - must succeed
    if (video_num > 0) {
        ret = esp_extractor_get_stream_info(extractor->extractor,
                                            EXTRACTOR_STREAM_TYPE_VIDEO, 0, &stream_info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get video stream info: %d", ret);
            return ret;
        }

        extractor_video_stream_info_t *video_info = &stream_info.stream_info.video_info;
        extractor->has_video = true;
        extractor->video_format = video_info->format;
        extractor->video_width = video_info->width;
        extractor->video_height = video_info->height;
        extractor->video_fps = video_info->fps;
        extractor->video_duration = stream_info.duration;

        ESP_LOGI(TAG, "Video: format=%d, %" PRIu32 "x%" PRIu32 ", %" PRIu32 "fps",
                 (int)video_info->format, video_info->width, video_info->height,
                 (uint32_t)video_info->fps);
    } else {
        ESP_LOGE(TAG, "No video streams found");
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t app_extractor_init(app_extractor_frame_cb_t frame_cb,
                             esp_codec_dev_handle_t audio_dev,
                             app_extractor_handle_t *ret_extractor)
{
    if (ret_extractor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_extractor_t *extractor = calloc(1, sizeof(app_extractor_t));
    if (extractor == NULL) {
        ESP_LOGE(TAG, "Failed to allocate extractor context");
        return ESP_ERR_NO_MEM;
    }

    extractor->frame_cb = frame_cb;
    extractor->file = -1;
    extractor->extractor = NULL;
    extractor->eos_reached = false;

    extractor->audio_dev = audio_dev;

    // Initialize A/V sync if enabled
#if CONFIG_HDMI_VIDEO_SYNC_ENABLED
    extractor->sync_enabled = true;
    extractor->sync_threshold_ms = HDMI_SYNC_THRESHOLD_MS;
    extractor->base_time = esp_timer_get_time() / 1000;
    extractor->sync_mutex = xSemaphoreCreateMutex();
#else
    extractor->sync_enabled = false;
    extractor->sync_mutex = NULL;
#endif

    extractor->audio_decoder = NULL;
    extractor->audio_decoder_open = false;
    extractor->audio_buffer = NULL;
    extractor->audio_buffer_size = 0;

    // Initialize audio task variables
    extractor->audio_task_handle = NULL;
    extractor->audio_task_running = false;
    extractor->audio_queue = NULL;

    // Initialize frame rate control
    extractor->frame_interval_ms = 1000 / DEFAULT_VIDEO_FPS;
    extractor->last_frame_time = 0;

    // Create audio queue if audio device is provided
    if (audio_dev) {
        extractor->audio_queue = xQueueCreate(AUDIO_QUEUE_SIZE, sizeof(audio_frame_item_t*));
        if (extractor->audio_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create audio queue");
#if CONFIG_HDMI_VIDEO_SYNC_ENABLED
            if (extractor->sync_mutex) {
                vSemaphoreDelete(extractor->sync_mutex);
            }
#endif
            free(extractor);
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t ret = register_all_extractors();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register extractors: %d", ret);
        if (extractor->audio_queue) {
            vQueueDelete(extractor->audio_queue);
        }
#if CONFIG_HDMI_VIDEO_SYNC_ENABLED
        if (extractor->sync_mutex) {
            vSemaphoreDelete(extractor->sync_mutex);
        }
#endif
        free(extractor);
        return ret;
    }

    if (audio_dev != NULL) {
        ret = register_audio_decoders();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register audio decoders: %d", ret);
            if (extractor->audio_queue) {
                vQueueDelete(extractor->audio_queue);
            }
#if CONFIG_HDMI_VIDEO_SYNC_ENABLED
            if (extractor->sync_mutex) {
                vSemaphoreDelete(extractor->sync_mutex);
            }
#endif
            free(extractor);
            return ret;
        }
    }

    ESP_LOGI(TAG, "App extractor initialized%s", audio_dev ? " with audio" : "");
    *ret_extractor = extractor;
    return ESP_OK;
}

esp_err_t app_extractor_start(app_extractor_handle_t handle,
                              const char *filename,
                              bool extract_video,
                              bool extract_audio)
{
    if (handle == NULL || filename == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_extractor_t *extractor = (app_extractor_t *)handle;
    esp_err_t ret;

    // Close any existing extractor
    if (extractor->extractor != NULL) {
        esp_extractor_close(extractor->extractor);
        extractor->extractor = NULL;
    }

    // Stop audio task if running
    stop_audio_task(extractor);

    // Set extraction flags
    extractor->extract_video = extract_video;
    extractor->extract_audio = extract_audio && (extractor->audio_dev != NULL);
    extractor->eos_reached = false;
    extractor->last_video_pts = 0;
    extractor->last_audio_pts = 0;
    extractor->last_frame_time = esp_timer_get_time() / 1000;

    // Set extraction mask based on what we want to extract
    uint8_t extract_mask = 0;
    if (extract_video) {
        extract_mask |= ESP_EXTRACT_MASK_VIDEO;
    }
    if (extractor->extract_audio) {
        extract_mask |= ESP_EXTRACT_MASK_AUDIO;
    }

    // Configure extractor with larger pool size
    esp_extractor_config_t config = {
        .open = _file_open,
        .read = _file_read,
        .seek = _file_seek,
        .file_size = _file_size,
        .close = _file_close,
        .extract_mask = extract_mask,
        .url = (char *)filename,  // Cast to match the API
        .input_ctx = extractor,
        .output_pool_size = EXTRACTOR_POOL_SIZE,     // Set output pool size
        .cache_block_num = EXTRACTOR_POOL_BLOCKS,    // Set number of cache blocks
        .cache_block_size = EXTRACTOR_POOL_SIZE / EXTRACTOR_POOL_BLOCKS  // Set cache block size
    };

    // Open extractor
    ret = esp_extractor_open(&config, &extractor->extractor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open extractor: %d", ret);
        return ret;
    }

    // Parse stream information
    ret = esp_extractor_parse_stream_info(extractor->extractor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse stream info: %d", ret);
        return ret;
    }

    // Get stream information
    ret = get_stream_info(extractor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get stream info: %d", ret);
        return ret;
    }

    // Validate MPEG format compatibility
    ret = validate_mpeg_compatibility(extractor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MPEG compatibility validation failed: %d", ret);
        return ret;
    }

    // Calculate optimal frame interval based on video fps
    if (extractor->has_video && extractor->video_fps > 0) {
        extractor->frame_interval_ms = 1000 / extractor->video_fps;
    } else {
        extractor->frame_interval_ms = 1000 / DEFAULT_VIDEO_FPS;
    }

    // Start audio processing if needed
    if (extractor->extract_audio) {
        ret = start_audio_task(extractor);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Audio task start failed: %d", ret);
            return ret;
        }
    }

    ESP_LOGI(TAG, "Extraction started: fps=%" PRIu32 "ms, audio=%s",
             extractor->frame_interval_ms, extractor->extract_audio ? "yes" : "no");
    return ESP_OK;
}

esp_err_t app_extractor_read_frame(app_extractor_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_extractor_t *extractor = (app_extractor_t *)handle;

    // Check if we already reached the end of stream
    if (extractor->eos_reached) {
        return ESP_ERR_NOT_FOUND;
    }

    // Read the next frame
    extractor_frame_info_t frame = {0};
    esp_err_t ret = esp_extractor_read_frame(extractor->extractor, &frame);

    if (ret == ESP_OK) {
        // Process the frame
        ret = process_frame(&frame, extractor);
    } else {
        ESP_LOGW(TAG, "Failed to read frame: %d", ret);
        extractor->eos_reached = true;
    }

    return ret;
}

esp_err_t app_extractor_get_video_info(app_extractor_handle_t handle,
                                       uint32_t *width,
                                       uint32_t *height,
                                       uint32_t *fps,
                                       uint32_t *duration)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_extractor_t *extractor = (app_extractor_t *)handle;

    if (!extractor->has_video) {
        return ESP_ERR_NOT_FOUND;
    }

    if (width) {
        *width = extractor->video_width;
    }
    if (height) {
        *height = extractor->video_height;
    }
    if (fps) {
        *fps = extractor->video_fps;
    }
    if (duration) {
        *duration = extractor->video_duration;
    }

    return ESP_OK;
}

esp_err_t app_extractor_get_audio_info(app_extractor_handle_t handle,
                                       uint32_t *sample_rate,
                                       uint8_t *channels,
                                       uint8_t *bits,
                                       uint32_t *duration)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_extractor_t *extractor = (app_extractor_t *)handle;

    if (!extractor->has_audio) {
        return ESP_ERR_NOT_FOUND;
    }

    if (sample_rate) {
        *sample_rate = extractor->audio_sample_rate;
    }
    if (channels) {
        *channels = extractor->audio_channels;
    }
    if (bits) {
        *bits = extractor->audio_bits;
    }
    if (duration) {
        *duration = extractor->audio_duration;
    }

    return ESP_OK;
}

esp_err_t app_extractor_seek(app_extractor_handle_t handle, uint32_t position)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_extractor_t *extractor = (app_extractor_t *)handle;

    if (extractor->extractor == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Reset EOS flag when seeking
    extractor->eos_reached = false;

    // Seek to the specified position (in milliseconds)
    return esp_extractor_seek(extractor->extractor, position);
}

esp_err_t app_extractor_stop(app_extractor_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_extractor_t *extractor = (app_extractor_t *)handle;

    // Stop audio task first
    stop_audio_task(extractor);

    if (extractor->extractor != NULL) {
        esp_extractor_close(extractor->extractor);
        extractor->extractor = NULL;
    }

    extractor->eos_reached = true;

    return ESP_OK;
}

esp_err_t app_extractor_deinit(app_extractor_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_extractor_t *extractor = (app_extractor_t *)handle;

    // Stop extraction if in progress
    app_extractor_stop(handle);

    // Close audio decoder
    if (extractor->audio_decoder != NULL) {
        esp_audio_dec_close(extractor->audio_decoder);
        extractor->audio_decoder = NULL;
    }

    // Free audio buffer
    if (extractor->audio_buffer != NULL) {
        free(extractor->audio_buffer);
        extractor->audio_buffer = NULL;
    }

    // Delete audio queue
    if (extractor->audio_queue != NULL) {
        vQueueDelete(extractor->audio_queue);
        extractor->audio_queue = NULL;
    }

    // Delete sync mutex
#if CONFIG_HDMI_VIDEO_SYNC_ENABLED
    if (extractor->sync_mutex != NULL) {
        vSemaphoreDelete(extractor->sync_mutex);
        extractor->sync_mutex = NULL;
    }
#endif

    // Unregister all extractors
    esp_extractor_unregister_all();

    // Free context
    free(extractor);

    ESP_LOGI(TAG, "App extractor deinitialized");
    return ESP_OK;
}

esp_err_t app_extractor_probe_video_info(const char *filename,
                                         uint32_t *width, uint32_t *height,
                                         uint32_t *fps, uint32_t *duration)
{
    if (filename == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_extractor_handle_t probe;
    esp_err_t ret = app_extractor_init(NULL, NULL, &probe);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = app_extractor_start(probe, filename, true, false);
    if (ret != ESP_OK) {
        app_extractor_deinit(probe);
        return ret;
    }

    ret = app_extractor_get_video_info(probe, width, height, fps, duration);

    app_extractor_stop(probe);
    app_extractor_deinit(probe);

    return ret;
}
