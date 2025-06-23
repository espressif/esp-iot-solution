/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "avifile.h"

static const char *TAG = "avifile";

#define MAKE_FOURCC(a, b, c, d) ((uint32_t)(d)<<24 | (uint32_t)(c)<<16 | (uint32_t)(b)<<8 | (uint32_t)(a))
#define READ_WORD(a) ((uint32_t)*(a)<<24 | (uint32_t)*(a)<<16 | (uint32_t)*(a)<<8 | (uint32_t)*(a))

static uint32_t _REV(uint32_t value)
{
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 |
           (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24;
}

static int search_fourcc(uint32_t fourcc, const uint8_t *buffer, uint32_t length)
{
    uint32_t i, j;
    uint32_t *pdata;
    j = length - 4;
    for (i = 0; i < j; i++) {
        pdata = (uint32_t*)(buffer + i);
        if (fourcc == *pdata) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Parse the AVI stream list (strl) from the AVI file buffer.
 *
 * @param AVI_file Pointer to the AVI file structure.
 * @param buffer Pointer to the AVI file buffer.
 * @param length Length of the AVI file buffer.
 * @param list_length Pointer to store the length of the parsed list.
 *
 * @return
 *     -  0: Success
 *     - -1: Invalid list or FourCC
 *     - -5: Invalid size or FourCC for strh or strf
 */
static int strl_parser(avi_typedef *AVI_file, const uint8_t *buffer, uint32_t length, uint32_t *list_length)
{
    /**
     * TODO: how to deal with the list is not complete in the buffer
     */
    const uint8_t *pdata = buffer;
    // strl(stream list), include "strh" and "strf"
    AVI_LIST_HEAD *strl = (AVI_LIST_HEAD*)pdata;
    if (strl->List != LIST_ID || strl->FourCC != STRL_ID) {
        return -1;
    }
    pdata += sizeof(AVI_LIST_HEAD);
    *list_length = strl->size + 8; //return the entire size of list

    // strh
    AVI_STRH_CHUNK *strh = (AVI_STRH_CHUNK*)pdata;
    if (strh->FourCC != STRH_ID || strh->size + 8 != sizeof(AVI_STRH_CHUNK)) {
        return -5;
    }
#ifdef CONFIG_AVI_PLAYER_DEBUG_INFO
    printf("-----strh info------\r\n");
    printf("fourcc_type:0x%lx\r\n", strh->fourcc_type);
    printf("fourcc_codec:0x%"PRIx32"\r\n", strh->fourcc_codec);
    printf("flags:%"PRIu32"\r\n", strh->flags);
    printf("Priority:%d\r\n", strh->priority);
    printf("Language:%d\r\n", strh->language);
    printf("InitFrames:%"PRIu32"\r\n", strh->init_frames);
    printf("Scale:%"PRIu32"\r\n", strh->scale);
    printf("Rate:%"PRIu32"\r\n", strh->rate);
    printf("Start:%"PRIu32"\r\n", strh->start);
    printf("Length:%"PRIu32"\r\n", strh->length);
    printf("RefBufSize:%"PRIu32"\r\n", strh->suggest_buff_size);
    printf("Quality:%"PRIu32"\r\n", strh->quality);
    printf("SampleSize:%"PRIu32"\r\n", strh->sample_size);
    printf("FrameLeft:%d\r\n", strh->rcFrame.left);
    printf("FrameTop:%d\r\n", strh->rcFrame.top);
    printf("FrameRight:%d\r\n", strh->rcFrame.right);
    printf("FrameBottom:%d\r\n\n", strh->rcFrame.bottom);
#endif
    pdata += sizeof(AVI_STRH_CHUNK);

    if (VIDS_ID == strh->fourcc_type) {
        ESP_LOGI(TAG, "Find a video stream");
        if (MJPG_ID == strh->fourcc_codec) {
            AVI_file->vids_format = FORMAT_MJEPG;
        } else if (H264_ID == strh->fourcc_codec) {
            AVI_file->vids_format = FORMAT_H264;
        } else {
            ESP_LOGE(TAG, "only support mjpeg\\h264 decoder, but needed is 0x%"PRIx32"", strh->fourcc_codec);
            return -1;
        }
        AVI_VIDS_STRF_CHUNK *strf = (AVI_VIDS_STRF_CHUNK*)pdata;
        if (strf->FourCC != STRF_ID || strf->size + 8 != sizeof(AVI_VIDS_STRF_CHUNK)) {
            return -5;
        }
#ifdef CONFIG_AVI_PLAYER_DEBUG_INFO
        printf("-----video strf info------\r\n");
        printf("Size of this structure:%"PRIu32"\r\n", strf->size1);
        printf("Width of image:%"PRIu32"\r\n", strf->width);
        printf("Height of image:%"PRIu32"\r\n", strf->height);
        printf("Number of planes:%d\r\n", strf->planes);
        printf("Number of bits per pixel:%d\r\n", strf->bitcount);
        printf("Compression type:0x%"PRIx32"\r\n", strf->fourcc_compression);
        printf("Image size:%"PRIu32"\r\n", strf->image_size);
        printf("Horizontal resolution:%"PRIu32"\r\n", strf->x_pixels_per_meter);
        printf("Vertical resolution:%"PRIu32"\r\n", strf->y_pixels_per_meter);
        printf("Number of colors in palette:%"PRIu32"\r\n", strf->num_colors);
        printf("Number of important colors:%"PRIu32"\r\n\n", strf->imp_colors);
#endif
        AVI_file->vids_fps = strh->rate / strh->scale;
        AVI_file->vids_width = strf->width;
        AVI_file->vids_height = strf->height;
        pdata += sizeof(AVI_VIDS_STRF_CHUNK);
    } else if (AUDS_ID == strh->fourcc_type) {
        ESP_LOGI(TAG, "Find a audio stream");
        AVI_AUDS_STRF_CHUNK *strf = (AVI_AUDS_STRF_CHUNK*)pdata;
        if (strf->FourCC != STRF_ID || (strf->size + 8 != sizeof(AVI_AUDS_STRF_CHUNK) && strf->size + 10 != sizeof(AVI_AUDS_STRF_CHUNK))) {
            ESP_LOGE(TAG, "FourCC=0x%"PRIx32"|%"PRIx32", size=%"PRIu32"|%d", strf->FourCC, STRF_ID, strf->size, sizeof(AVI_AUDS_STRF_CHUNK));
            return -5;
        }
#ifdef CONFIG_AVI_PLAYER_DEBUG_INFO
        printf("-----audio strf info------\r\n");
        printf("strf data block info(audio stream):");
        printf("format tag:%d\r\n", strf->format_tag);
        printf("number of channels:%d\r\n", strf->channels);
        printf("sampling rate:%"PRIu32"\r\n", strf->samples_per_sec);
        printf("bitrate:%"PRIu32"\r\n", strf->avg_bytes_per_sec);
        printf("block align:%d\r\n", strf->block_align);
        printf("sample size:%"PRIu32"\r\n\n", strf->bits_per_sample);
#endif
        AVI_file->auds_channels = strf->channels;
        AVI_file->auds_sample_rate = strf->samples_per_sec;
        AVI_file->auds_bits = strf->bits_per_sample;
        pdata += sizeof(AVI_AUDS_STRF_CHUNK);
    } else {
        ESP_LOGW(TAG, "Unsupported stream 0x%"PRIu32"", strh->fourcc_type);
    }
    return 0;
}

int avi_parser(avi_typedef *AVI_file, const uint8_t *buffer, uint32_t length)
{
    const uint8_t *pdata = buffer;
    AVI_LIST_HEAD *riff = (AVI_LIST_HEAD*)pdata;
    if (riff->List != RIFF_ID || riff->FourCC != AVI_ID) {
        return -1;
    }
    /*!< data block length */
    AVI_file->RIFFchunksize = riff->size;
    pdata += sizeof(AVI_LIST_HEAD);

    /*!< LIST data block length */
    AVI_LIST_HEAD *list = (AVI_LIST_HEAD*)pdata;
    if (list->List != LIST_ID || list->FourCC != HDRL_ID) {
        return -3;
    }
    AVI_file->LISTchunksize = list->size;
    pdata += sizeof(AVI_LIST_HEAD);

    /*!< avih chunk */
    AVI_AVIH_CHUNK *avih = (AVI_AVIH_CHUNK*)pdata;
    if (avih->FourCC != AVIH_ID || avih->size + 8 != sizeof(AVI_AVIH_CHUNK)) {
        return -5;
    }
    /*!< avih data block length */
    AVI_file->avihsize = avih->size;

#ifdef CONFIG_AVI_PLAYER_DEBUG_INFO
    printf("-----avih info------\r\n");
    printf("us_per_frame:%"PRIu32"\r\n", avih->us_per_frame);
    printf("max_bytes_per_sec:%"PRIu32"\r\n", avih->max_bytes_per_sec);
    printf("padding:%"PRIu32"\r\n", avih->padding);
    printf("flags:%"PRIu32"\r\n", avih->flags);
    printf("total_frames:%"PRIu32"\r\n", avih->total_frames);
    printf("init_frames:%"PRIu32"\r\n", avih->init_frames);
    printf("streams:%"PRIu32"\r\n", avih->streams);
    printf("suggest_buff_size:%"PRIu32"\r\n", avih->suggest_buff_size);
    printf("Width:%"PRIu32"\r\n", avih->width);
    printf("Height:%"PRIu32"\r\n\n", avih->height);
#endif

    pdata += sizeof(AVI_AVIH_CHUNK);

    /*!< process all streams in turn */
    for (size_t i = 0; i < avih->streams; i++) {
        uint32_t strl_size = 0;
        int ret = strl_parser(AVI_file, pdata, length - (pdata - buffer), &strl_size);
        if (0 > ret) {
            ESP_LOGE(TAG, "strl of stream%d prase failed", i);
            break;
            /**
             * TODO: how to deal this error? maybe we should search for the next strl.
             */
        }
        pdata += strl_size;
    }
    int movi_offset = search_fourcc(MAKE_FOURCC('m', 'o', 'v', 'i'), pdata, length - (pdata - buffer));
    if (0 > movi_offset) {
        ESP_LOGE(TAG, "can't find \"movi\" list");
        return -7;
    }
    AVI_file->movi_start = movi_offset + 4  + pdata - buffer;
    /*!< back to the list head */
    pdata += movi_offset - 8;
    AVI_LIST_HEAD *movi = (AVI_LIST_HEAD*)pdata;
    if (movi->List != LIST_ID || movi->FourCC != MOVI_ID) {
        return -8;
    }
    AVI_file->movi_size = movi->size;
    pdata += sizeof(AVI_LIST_HEAD);
    ESP_LOGI(TAG, "movi pos:%"PRIu32", size:%"PRIu32"", AVI_file->movi_start, AVI_file->movi_size);

    return 0;
}
