/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AVI_DEFINE_H_
#define _AVI_DEFINE_H_

#include "stdint.h"

/**
 * AVIFileFormat: https://web.archive.org/web/20170411001412/http://www.alexander-noe.com/video/documentation/avi.pdf
 * OpenDML AVI File Format Extensions: https://web.archive.org/web/20070112225112/http://www.the-labs.com/Video/odmlff2-avidef.pdf
 */

typedef struct {
    uint32_t FourCC;   /*!< FourCC identifier for the chunk */
    uint32_t size;     /*!< Size of the chunk, equals the size of the subsequent data */
    /* Data uint8_t _data[size]; */
} __attribute__((packed)) AVI_CHUNK_HEAD;

typedef struct {
    uint32_t List;     /*!< Fixed as "LIST", if it is an RIFF List then "RIFF" */
    uint32_t size;     /*!< Size of the chunk, equals the size of the subsequent data */
    uint32_t FourCC;
    /* Data uint8_t _data[size-4]; */
} __attribute__((packed)) AVI_LIST_HEAD;

typedef struct {
    uint32_t FourCC;            /*!< Chunk ID, fixed as avih */
    uint32_t size;              /*!< Size of the chunk, equals the size of struct avi_avih_chunk minus the size of id and size */
    uint32_t us_per_frame;      /*!< Video frame interval time (in microseconds) */
    uint32_t max_bytes_per_sec; /*!< Maximum data rate of the AVI file */
    uint32_t padding;           /*!< Set to 0 */
    uint32_t flags;             /*!< Global properties of the AVI file, such as whether it contains index blocks, whether audio and video data are interleaved, etc. */
    uint32_t total_frames;      /*!< Total number of frames */
    uint32_t init_frames;       /*!< Specifies the initial number of frames for interactive format (set to 0 for non-interactive format) */
    uint32_t streams;           /*!< Number of streams in the file, 1 when only a video stream exists */
    uint32_t suggest_buff_size; /*!< Specifies the buffer size recommended for reading this file, usually the sum of the data required for storing a frame and synchronizing sound */
    uint32_t width;             /*!< Video main window width (in pixels) */
    uint32_t height;            /*!< Video main window height (in pixels) */
    uint32_t reserved[4];       /*!< Reserved values: dwScale, dwRate, dwStart, dwLength */
} __attribute__((packed)) AVI_AVIH_CHUNK;

typedef struct {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
} __attribute__((packed)) AVI_RECT_FRAME;

typedef struct {
    uint32_t FourCC;         /*!< Chunk ID, fixed as strh */
    uint32_t size;           /*!< Size of the chunk, equals the size of struct avi_strh_chunk minus the size of id and size */
    uint32_t fourcc_type;    /*!< Stream type, vids for video stream, auds for audio stream */
    uint32_t fourcc_codec;   /*!< Specifies the decoder required to process this stream, such as JPEG */
    uint32_t flags;          /*!< Flags, such as whether the stream output is allowed, whether the palette changes, etc. Usually set to 0 */
    uint16_t priority;       /*!< Priority of the stream, set to 0 for video stream */
    uint16_t language;       /*!< Audio language code, set to 0 for video stream */
    uint32_t init_frames;    /*!< Specifies the initial number of frames for interactive format (set to 0 for non-interactive format) */
    uint32_t scale;          /*!<  */
    uint32_t rate;           /*!< For video streams, rate / scale = frame rate (fps) */
    uint32_t start;          /*!< For video streams, set to 0 */
    uint32_t length;         /*!< For video streams, length is the total number of frames */
    uint32_t suggest_buff_size; /*!< Recommended buffer size for reading this stream's data */
    uint32_t quality;        /*!< Quality metric of the stream data */
    uint32_t sample_size;    /*!< Audio sample size, set to 0 for video stream */
    AVI_RECT_FRAME rcFrame;  /*!< Display position of this stream in the video main window, set to {0,0,width,height} */
} __attribute__((packed)) AVI_STRH_CHUNK;

/*!< For video streams, the strf block structure is as follows */
typedef struct {
    uint32_t FourCC;             /*!< Chunk ID, fixed as strf */
    uint32_t size;               /*!< Size of the chunk, equals the size of struct avi_strf_chunk minus the size of id and size */
    uint32_t size1;              /*!< Meaning and value are the same as size */
    uint32_t width;              /*!< Video main window width (in pixels) */
    uint32_t height;             /*!< Video main window height (in pixels) */
    uint16_t planes;             /*!< Always 1 */
    uint16_t bitcount;           /*!< Number of bits per pixel, can only be one of 1, 4, 8, 16, 24, and 32 */
    uint32_t fourcc_compression; /*!< Video stream encoding format, such as "JPEG", "MJPG", etc. */
    uint32_t image_size;         /*!< Video image size, equals width * height * bitcount / 8 */
    uint32_t x_pixels_per_meter; /*!< Horizontal resolution of the display device, set to 0 */
    uint32_t y_pixels_per_meter; /*!< Vertical resolution of the display device, set to 0 */
    uint32_t num_colors;         /*!< Unclear meaning, set to 0 */
    uint32_t imp_colors;         /*!< Unclear meaning, set to 0 */
} __attribute__((packed)) AVI_VIDS_STRF_CHUNK;

/*!< For audio streams, the strf block structure is as follows */
typedef struct __attribute__((packed))
{
    uint32_t FourCC;             /*!< Chunk ID, fixed as strf */
    uint32_t size;               /*!< Size of the chunk, equals the size of struct avi_strf_chunk minus the size of id and size */
    uint16_t format_tag;
    uint16_t channels;
    uint32_t samples_per_sec;
    uint32_t avg_bytes_per_sec;
    uint16_t block_align;
    uint32_t bits_per_sample;
} __attribute__((packed))  AVI_AUDS_STRF_CHUNK;

typedef struct {
    AVI_LIST_HEAD strl;
    AVI_STRH_CHUNK strh;
    AVI_VIDS_STRF_CHUNK strf;
} __attribute__((packed)) AVI_STRL_LIST;

typedef struct {
    AVI_LIST_HEAD hdrl;
    AVI_AVIH_CHUNK avih;
    AVI_STRL_LIST  strl;
} __attribute__((packed)) AVI_HDRL_LIST;

typedef struct {
    uint32_t FourCC; /*!< Chunk ID, fixed as "idx1" */
    uint32_t flags;
    uint32_t chunkoffset;
    uint32_t chunklength;
} __attribute__((packed)) AVI_IDX1;

#endif
