/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AVI_DEFINE_H_
#define _AVI_DEFINE_H_

#include "stdint.h"

 /**
  * reference links: https://www.cnblogs.com/songhe364826110/p/7619949.html
  * AVIFileFormat: https://web.archive.org/web/20170411001412/http://www.alexander-noe.com/video/documentation/avi.pdf
  * OpenDML AVI File Format Extensions: https://web.archive.org/web/20070112225112/http://www.the-labs.com/Video/odmlff2-avidef.pdf
  */

typedef struct {
    uint32_t FourCC;
    uint32_t size;   // Block size, equal to the size of subsequent data
    /* data uint8_t _data[size]; */
}AVI_CHUNK_HEAD;

typedef struct {
    uint32_t List;   // Fixed to "list" and "riff" if it is riff list
    uint32_t size;   // Block size, equal to the size of subsequent data
    uint32_t FourCC;
    /* data uint8_t _data[size-4]; */
}AVI_LIST_HEAD;

typedef struct {
    uint32_t FourCC;            // Block ID, fixed to 'avih'
    uint32_t size;              // Block size = size of struct avi_avih_chunk - the size of ID and size.
    uint32_t us_per_frame;      // Video frame interval (in microseconds)
    uint32_t max_bytes_per_sec; // Maximum data rate of AVI file
    uint32_t padding;           // Set to 0
    uint32_t flags;             // Global attributes of AVI file, such as whether it contains index blocks, whether audio and video data are cross stored, etc
    uint32_t total_frames;      // Total frames
    uint32_t init_frames;       // Specify the initial number of frames for interactive format (non interactive format should be specified as 0)
    uint32_t streams;           // The number of streams contained in the file. When there is only video stream, it is 1
    uint32_t suggest_buff_size; // Specifies the buffer size recommended for reading this file. It is usually the sum of the data required to store a frame image and synchronize sound. If it is not specified, it is set to 0
    uint32_t width;             // Width of video main window (unit: pixel)
    uint32_t height;            // Height of video main window (unit: pixel)
    uint32_t reserved[4];       // reserved space for dwScale,dwRate,dwStart,dwLength.
}AVI_AVIH_CHUNK;

typedef struct {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
}AVI_RECT_FRAME;

typedef struct {
    uint32_t FourCC;            // Block ID, set to Strh
    uint32_t size;              // Block size = size of struct struct avi_strh_chunk - the size of ID and size
    uint32_t fourcc_type;       // The type of stream. vids represents video stream and auds represents audio stream
    uint32_t fourcc_codec;      // Specify the decoder needed to process this stream, such as JPEG
    uint32_t flags;             // Flag, such as whether to allow this stream output, whether the palette changes, etc., is generally set to 0
    uint16_t priority;          // The priority of the video stream can be set to 0
    uint16_t language;          // Audio language code, video stream can be set to 0
    uint32_t init_frames;       // Specify the initial number of frames for interactive format (non interactive format should be specified as 0)
    uint32_t scale;             //
    uint32_t rate;              // For video streams，rate / scale = fps (frame rate)
    uint32_t start;             // For video streams, set to 0
    uint32_t length;            // For video streams, length is the total number of frames
    uint32_t suggest_buff_size; // The recommended buffer size for reading this stream data
    uint32_t quality;           // Quality index of stream data
    uint32_t sample_size;       // Audio sampling size, video stream can be set to 0
    AVI_RECT_FRAME rcFrame;     // The display position of this stream in the main video window can be set to {0,0, width, height}
}AVI_STRH_CHUNK;

/*For video streams, the STRF block structure is as follows*/
typedef struct {
    uint32_t FourCC;             // Block ID, set to 'strf'
    uint32_t size;               // Block size = size of struct struct avi_strh_chunk - the size of ID and size
    uint32_t size1;              // Size1 has the same meaning and value as 'size' above
    uint32_t width;              // Width of video main window (unit: pixel)
    uint32_t height;             // Height of video main window (unit: pixel)
    uint16_t planes;             // set to 1
    uint16_t bitcount;           // The number of bits occupied by each pixel can only be one of 1, 4, 8, 16, 24 and 32
    uint32_t fourcc_compression; // Video stream coding format, such as "JPEG", "mjpg", etc
    uint32_t image_size;         // Video image size = width * height * bitcount / 8
    uint32_t x_pixels_per_meter; // The horizontal resolution of the display device can be set to 0
    uint32_t y_pixels_per_meter; // The vertical resolution of the display device can be set to 0
    uint32_t num_colors;         // Set to 0
    uint32_t imp_colors;         // Set to 0
}AVI_VIDS_STRF_CHUNK;

/*For audio streams, the strf block structure is as follows*/
typedef struct __attribute__((packed))
{
    uint32_t FourCC;             // blcok ID, for audio it is 'strf'
    uint32_t size;               // Block size = size of struct struct avi_strh_chunk - the size of ID and size
    uint16_t format_tag;
    uint16_t channels;
    uint32_t samples_per_sec;
    uint32_t avg_bytes_per_sec;
    uint16_t block_align;
    uint32_t bits_per_sample;
}AVI_AUDS_STRF_CHUNK;

typedef struct {
    AVI_LIST_HEAD strl;
    AVI_STRH_CHUNK strh;
    AVI_VIDS_STRF_CHUNK strf;
}AVI_STRL_LIST;

typedef struct {
    AVI_LIST_HEAD hdrl;
    AVI_AVIH_CHUNK avih;
    AVI_STRL_LIST  strl;
}AVI_HDRL_LIST;

typedef struct {
    uint32_t FourCC; // Block ID，can be set to "idx1"
    uint32_t flags;
    uint32_t chunkoffset;
    uint32_t chunklength;
}AVI_IDX1;

/**
"db"：Uncompressed video frame (RGB data stream)；
"dc"：Compressed video frames；
"wb"：Audio uncompressed data (wave stream)；
"wc"：Audio compressed data (compressed wave data stream)；
"pc"：Use a new palette instead. (the new palette is defined by a data structure avipalchange. If the palette of a stream may change halfway, an avisf_video_palchanges tag should be included in the description of the stream format, that is, the dwflags of avistreamreader structure.)
*/

#endif