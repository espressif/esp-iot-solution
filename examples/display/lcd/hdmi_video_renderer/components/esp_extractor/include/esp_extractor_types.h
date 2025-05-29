/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Stream type
 */
typedef enum {
    EXTRACTOR_STREAM_TYPE_NONE,
    EXTRACTOR_STREAM_TYPE_AUDIO,
    EXTRACTOR_STREAM_TYPE_VIDEO,
    EXTRACTOR_STREAM_TYPE_MAX
} extractor_stream_type_t;

/**
 * @brief   Audio codec format
 */
typedef enum {
    EXTRACTOR_AUDIO_FORMAT_NONE,
    EXTRACTOR_AUDIO_FORMAT_PCM,
    EXTRACTOR_AUDIO_FORMAT_ADPCM,
    EXTRACTOR_AUDIO_FORMAT_AAC,
    EXTRACTOR_AUDIO_FORMAT_MP3,
    EXTRACTOR_AUDIO_FORMAT_AC3,
    EXTRACTOR_AUDIO_FORMAT_VORBIS,
    EXTRACTOR_AUDIO_FORMAT_OPUS,
    EXTRACTOR_AUDIO_FORMAT_FLAC,
    EXTRACTOR_AUDIO_FORMAT_AMRNB,
    EXTRACTOR_AUDIO_FORMAT_AMRWB,
    EXTRACTOR_AUDIO_FORMAT_G711A,
    EXTRACTOR_AUDIO_FORMAT_G711U,
    EXTRACTOR_AUDIO_FORMAT_ALAC,
} extractor_audio_format_t;

/**
 * @brief   Video codec format
 */
typedef enum {
    EXTRACTOR_VIDEO_FORMAT_NONE,
    EXTRACTOR_VIDEO_FORMAT_H264,
    EXTRACTOR_VIDEO_FORMAT_MJPEG,
} extractor_video_format_t;

/**
 * @brief   Video stream information
 */
typedef struct {
    extractor_video_format_t format; /*!< Video format */
    uint16_t                 fps;    /*!< Video frames per second */
    uint32_t                 width;  /*!< Video width */
    uint32_t                 height; /*!< Video height */
} extractor_video_stream_info_t;

/**
 * @brief   Audio stream information
 */
typedef struct {
    extractor_audio_format_t format;          /*!< Audio format */
    uint8_t                  channel;         /*!< Audio output channel */
    uint8_t                  bits_per_sample; /*!< Audio bits per sample */
    uint32_t                 sample_rate;     /*!< Audio sample rate */
} extractor_audio_stream_info_t;

/**
 * @brief   Stream information
 */
typedef struct {
    extractor_stream_type_t stream_type;          /*!< Stream type */
    uint16_t                stream_id;            /*!< Stream id */
    uint32_t                duration;             /*!< Stream total duration (unit milliseconds) */
    uint32_t                bitrate;              /*!< Stream bitrate */
    uint8_t                *spec_info;            /*!< Stream specified information */
    uint32_t                spec_info_len;        /*!< Stream specified information length */
    union {
        extractor_video_stream_info_t video_info; /*!< Video stream information */
        extractor_audio_stream_info_t audio_info; /*!< Audio stream information */
    } stream_info;
} extractor_stream_info_t;

/**
 * @brief   Output frame information
 */
typedef struct {
    extractor_stream_type_t stream_type;  /*!< Stream type */
    uint16_t                stream_idx;
    bool                    eos;          /*!< Flag for end of stream */
    uint8_t                 reserve_flag; /*!< Flag reserved for extractor */
    uint32_t                pts;          /*!< Stream PTS (unit milliseconds) */
    uint32_t                frame_size;   /*!< Frame data size */
    uint8_t                *frame_buffer; /*!< Frame data pointer (output only) */
} extractor_frame_info_t;

#ifdef __cplusplus
}
#endif
