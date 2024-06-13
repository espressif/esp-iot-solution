/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __AVIFILE_H
#define __AVIFILE_H

#include "avi_def.h"
#include "avi_player.h"

/** little_endian */
#define RIFF_ID     _REV(0x52494646)
#define AVI_ID      _REV(0x41564920)
#define LIST_ID     _REV(0x4c495354)
#define HDRL_ID     _REV(0x6864726c)
#define AVIH_ID     _REV(0x61766968)
#define STRL_ID     _REV(0x7374726c)
#define STRH_ID     _REV(0x73747268)
#define STRF_ID     _REV(0x73747266)
#define MOVI_ID     _REV(0x6d6f7669)
#define MJPG_ID     _REV(0x4D4A5047)
#define H264_ID     _REV(0x48323634)
#define VIDS_ID     _REV(0x76696473)
#define AUDS_ID     _REV(0x61756473)

/**
"db"：uncompressed video frame (RGB data stream);
"dc"：compressed video frame;
"wb"：uncompressed audio data (Wave data stream);
"wc"：compressed audio data (compressed Wave data stream);
"pc"：use new palette. (The new palette uses a data structure AVIPALCHANGE to define. If the palette of a stream may change during the stream, the AVISF_VIDEO_PALCHANGES flag should be included in the dwFlags member of the AVISTREAMHEADER structure.)
*/
#define DB_ID       _REV(0x00006462)  /*!< uncompressed video frame */
#define DC_ID       _REV(0x00006463)  /*!< compressed video frame */
#define WB_ID       _REV(0x00007762)  /*!< uncompressed audio data */
#define PC_ID       _REV(0x00007063)  /*!< use new palette */

typedef struct {
    uint32_t  RIFFchunksize;
    uint32_t  LISTchunksize;
    uint32_t  avihsize;
    uint32_t  strlsize;
    uint32_t  strhsize;

    uint32_t movi_start;
    uint32_t movi_size;

    uint16_t vids_fps;
    uint16_t vids_width;
    uint16_t vids_height;
    video_frame_format vids_format;

    uint16_t auds_channels;
    uint16_t auds_sample_rate;
    uint16_t auds_bits;
} avi_typedef;

/**
 * @brief Parse the AVI file buffer to extract essential information.
 *
 * @param AVI_file Pointer to the AVI file structure.
 * @param buffer Pointer to the AVI file buffer.
 * @param length Length of the AVI file buffer.
 *
 * @return
 *     -  0: Success
 *     - -1: Invalid RIFF or FourCC
 *     - -3: Invalid LIST or FourCC
 *     - -5: Invalid size or FourCC for avih or strf
 *     - -7: "movi" list not found
 *     - -8: Invalid "movi" list
 */
int avi_parser(avi_typedef *AVI_file, const uint8_t *buffer, uint32_t length);

#endif
