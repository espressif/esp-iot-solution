/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "esp_extractor_types.h"
#include "mem_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Extract mask
 */
#define ESP_EXTRACT_MASK_AUDIO (1 << 0) /*!< Filter audio data only */
#define ESP_EXTRACT_MASK_VIDEO (1 << 1) /*!< Filter video data only */
#define ESP_EXTRACT_MASK_AV    (ESP_EXTRACT_MASK_AUDIO | ESP_EXTRACT_MASK_VIDEO)

/**
 * @brief   Extractor error code
 */
typedef enum {
    ESP_EXTR_ERR_NEED_MORE_BUF = 1,
    ESP_EXTR_ERR_ALREADY_EXIST = 2,
    ESP_EXTR_ERR_STREAM_CHANGED = 3,
    ESP_EXTR_ERR_EOS = 4,
    ESP_EXTR_ERR_WAITING_NEW_URL = 5,
    ESP_EXTR_ERR_ALREADY_PARSED = 6,
    ESP_EXTR_ERR_WAITING_OUTPUT = 7,
    ESP_EXTR_ERR_OK = 0,
    ESP_EXTR_ERR_FAIL = -1,
    ESP_EXTR_ERR_INV_ARG = -2,
    ESP_EXTR_ERR_NOT_FOUND = -3,
    ESP_EXTR_ERR_NO_MEM = -4,
    ESP_EXTR_ERR_READ = -5,
    ESP_EXTR_ERR_ABORTED = -6,
    ESP_EXTR_ERR_INTERNAL = -7,
    ESP_EXTR_ERR_NOT_SUPPORT = -8,
    ESP_EXTR_ERR_WRONG_HEADER = -9,
    ESP_EXTR_ERR_SKIPPED = -10,
} esp_extr_err_t;

/**
 * @brief   Extractor container type
 */
typedef enum {
    ESP_EXTRACTOR_TYPE_NONE = 0,
    ESP_EXTRACTOR_TYPE_WAV,
    ESP_EXTRACTOR_TYPE_MP4,
    ESP_EXTRACTOR_TYPE_TS,
    ESP_EXTRACTOR_TYPE_OGG,
    ESP_EXTRACTOR_TYPE_AVI,
    ESP_EXTRACTOR_TYPE_AUDIO_ES,
    ESP_EXTRACTOR_TYPE_VIDEO_ES,
    ESP_EXTRACTOR_TYPE_HLS,
    ESP_EXTRACTOR_TYPE_FLV,
    ESP_EXTRACTOR_TYPE_CAF,
    ESP_EXTRACTOR_TYPE_CUSTOMIZED = 0x10,
} esp_extractor_type_t;

/**
 * @brief   Extractor handle
 */
typedef void *esp_extractor_handle_t;

/**
 * @brief   Function groups to get input data
 */
typedef void *(*_extractor_open_func)(char *url, void *input_ctx);
typedef int (*_extractor_read_func)(void *buffer, uint32_t size, void *ctx);
typedef int (*_extractor_read_abort_func)(void *ctx);
typedef int (*_extractor_seek_func)(uint32_t position, void *ctx);
typedef uint32_t (*_extractor_file_size_func)(void *ctx);
typedef int (*_extractor_close_func)(void *ctx);

/**
 * @brief   Configuration of extractor
 */
typedef struct {
    _extractor_open_func       open;         /*!< Input open callback (optional)
                                                 Open or close callback need provided in pair
                                                 When read from fifo, need not provided */
    _extractor_read_func       read;         /*!< Input read callback (required) */
    _extractor_read_abort_func read_abort;   /*!< Abort the read action to fast quit (optional) */
    _extractor_seek_func       seek;         /*!< Input seek callback (optional)
                                                  Will read data until destination position if not provided */
    _extractor_file_size_func  file_size;    /*!< Input get file size callback (optional) */
    _extractor_close_func      close;        /*!< Input close callback (optional) */
    void                      *input_ctx;    /*!< Input context (should not NULL) */
    char                      *url;          /*!< Input url
                                                 Used to probe according file extension
                                                 Or specially used for reload after content updated */
    uint8_t                    extract_mask; /*!< Extract mask: video, audio or both */

    /*!< Following configuration is optional for optimize or speed */
    uint16_t                   cache_block_num;  /*!< Total cache block number */
    uint32_t                   cache_block_size; /*!< Cache block size
                                                     If audio frame size is small, read big block data will cause long latency
                                                     To minimum the latency can set block size to small value
                                                     If input data is read from SDcard, big and aligned block size have better read
                                                    performance */
    uint32_t                   output_pool_size; /*!< Output pool size */
    mem_pool_handle_t          output_pool;      /*!< User created pool for output */
    uint16_t                   output_align;     /*!< Alignment for output */
    bool                       wait_for_output;  /*!< Wait for output buffer  */
    bool                       no_accurate_seek;  /*!< Seek from nearest key frame */
} esp_extractor_config_t;

/**
 * @brief  Resume stream information for interrupt play
 */
typedef struct {
    extractor_stream_type_t stream_type;          /*!< Stream type */
    bool                    enable;               /*!< Stream enable or not */
    uint16_t                stream_id;            /*!< Selected stream id */
    uint32_t                bitrate;              /*!< Bitrate */
    uint32_t                duration;             /*!< Stream duration (unit milliseconds) */
    uint32_t                frame_index;          /*!< Current play frame index */
    void                   *back_data;            /*!< Extra backup data */
    uint32_t                back_size;            /*!< Extra backup data size */
    union {
        extractor_video_stream_info_t video_info; /*!< Video stream information */
        extractor_audio_stream_info_t audio_info; /*!< Audio stream information */
    } stream_info;
} extractor_stream_resume_info_t;

/**
 * @brief  Struct for resume information of streams
 */
typedef struct {
    esp_extractor_type_t            extractor_type; /*!< Extractor type */
    uint32_t                        position;       /*!< Current extract byte position */
    uint32_t                        time;           /*!< Current extract time position (unit milliseconds)*/
    uint8_t                         stream_num;     /*!< Stream number */
    extractor_stream_resume_info_t *resume_streams; /*!< Kept resume stream information */
} esp_extractor_resume_info_t;

/**
 * @brief        Open extractor
 *
 * @note         Just allocate memory for cache and output buffer
 *
 * @param        config: Extractor configuration
 * @param[out]   extractor: Output extractor handle
 *
 * @return       - ESP_EXTR_ERR_OK: On success
 *               - ESP_EXTR_ERR_INV_ARG: Invalid input arguments
 *               - Others: Open fail
 */
esp_extr_err_t esp_extractor_open(esp_extractor_config_t *config, esp_extractor_handle_t *extractor);

/**
 * @brief        Parse stream information
 *
 * @note         It may takes long time for network case, can be canceled by `esp_extractor_close`
 *
 * @param        extractor: Extractor Handle
 *
 * @return       - ESP_EXTR_ERR_OK: On success
 *               - ESP_EXTR_ERR_INV_ARG: Invalid input arguments
 *               - Others: Fail to parse
 */
esp_extr_err_t esp_extractor_parse_stream_info(esp_extractor_handle_t extractor);

/**
 * @brief        Get total stream number of certain stream type
 *
 * @param        extractor: Extractor Handle
 * @param        stream_type: Stream type
 * @param[out]   stream_count: Output stream count
 *
 * @return       - ESP_EXTR_ERR_OK: On success
 *               - ESP_EXTR_ERR_INV_ARG: Invalid input arguments
 *               - ESP_EXTR_ERR_NOT_FOUND: not found track of such stream type
 */
esp_extr_err_t esp_extractor_get_stream_num(esp_extractor_handle_t extractor, extractor_stream_type_t stream_type,
                                            uint16_t *stream_count);

/**
 * @brief        Get stream information

 * @note         Stream information keeps some internal information to decrease the memory usage
 *               Never try to modify it, internal information will released after 'esp_extractor_close'.
 *               Don't rely on it once 'esp_extractor_close' called
 *
 * @param        extractor: Extractor Handle
 * @param        stream_type: Stream type
 * @param        stream_idx: Stream index
 * @param        info: Stream information to store
 *
 * @return       - ESP_EXTR_ERR_OK: On success
 *               - ESP_EXTR_ERR_INV_ARG: Invalid input arguments
 *               - Others: Fail to get stream information
 */
esp_extr_err_t esp_extractor_get_stream_info(esp_extractor_handle_t extractor, extractor_stream_type_t stream_type,
                                             uint16_t stream_idx, extractor_stream_info_t *info);

/**
* @brief        Enable stream

* @note         Use scenario: change audio track etc

* @param        extractor: Extractor Handle
* @param        stream_type: Stream type
* @param        stream_idx: Stream index

* @return       - ESP_EXTR_ERR_OK: On success
*               - ESP_EXTR_ERR_INV_ARG: Invalid input arguments
*               - Others: Fail to enable stream
*/
esp_extr_err_t esp_extractor_enable_stream(esp_extractor_handle_t extractor, extractor_stream_type_t stream_type,
                                           uint16_t stream_idx);

/**
* @brief        Disable stream

* @param        extractor: Extractor Handle
* @param        stream_type: Stream type

* @return       - ESP_EXTR_ERR_OK: On success
*               - ESP_EXTR_ERR_INV_ARG: Invalid input arguments
*               - Others: Fail to disable
*/
esp_extr_err_t esp_extractor_disable_stream(esp_extractor_handle_t extractor, extractor_stream_type_t stream_type);

/**
 * @brief        Get output memory pool instance
 *
 * @param        extractor: Extractor Handle
 *
 * @return       - NULL: Output memory pool not existed yet
 *               - Others: Output memory pool handle
 */
mem_pool_handle_t esp_extractor_get_output_pool(esp_extractor_handle_t extractor);

/**
 * @brief        Do extract stream data operation, output one or more frames after called once
 *
 * @note         `frame_buffer` in `frame_info` is allocated in memory pool
 *               Users need call `mem_pool_free` to free when not used anymore
 *               This API will block if memory is not enough in memory pool
 *
 * @param        extractor: Extractor Handle
 * @param[out]   frame_info: Output frame information
 *
 * @return       - ESP_EXTR_ERR_OK: On success
 *               - ESP_EXTR_ERR_INV_ARG: Invalid input arguments
 *               - ESP_EXTR_ERR_EOS: Reach file end
 *               - Others: Fail to extract data
 */
esp_extr_err_t esp_extractor_read_frame(esp_extractor_handle_t extractor, extractor_frame_info_t *frame_info);

/**
 * @brief        Abort ongoing read request using provided read abort API
 * @note         This API can be called before `esp_extractor_seek` to abort old read operations
 * @param        extractor: Extractor Handle
 * @return       - ESP_EXTR_ERR_OK: On success
 *               - ESP_EXTR_ERR_INV_ARG: Invalid input arguments
 *               - Others: Fail to abort
 */
esp_extr_err_t esp_extractor_read_abort(esp_extractor_handle_t handle);

/**
 * @brief        Extractor seek operation
 *
 * @param        extractor: Extractor Handle
 * @param        time_pos: Time position to seek (unit millisecond)
 *
 * @return       - ESP_EXTR_ERR_OK: On success
 *               - ESP_EXTR_ERR_INV_ARG: Invalid input arguments
 *               - ESP_EXTR_ERR_NOT_SUPPORT: Not support to do seek operation
 */
esp_extr_err_t esp_extractor_seek(esp_extractor_handle_t extractor, uint32_t time_pos);

/**
 * @brief        Get resume information
 *
 * @note         More memory will allocated and storaged into resume_info which maintained by extractor
 *               When not used need call `esp_extractor_free_resume_info` to release it
 *
 * @param        extractor: Extractor Handle
 * @param        resume_info: Struct pointer to store backup resume information
 *
 * @return       - ESP_EXTR_ERR_OK: On success
 *               - ESP_EXTR_ERR_INV_ARG: Invalid input arguments
 *               - ESP_EXTR_ERR_NO_MEM: No enough memory
 */
esp_extr_err_t esp_extractor_get_resume_info(esp_extractor_handle_t extractor,
                                             esp_extractor_resume_info_t *resume_info);

/**
 * @brief        Set resume info to restore playback
 *
 * @param        extractor: Extractor Handle
 * @param        resume_info: Resume information
 *
 * @return       - ESP_EXTR_ERR_OK: On success
 *               - ESP_EXTR_ERR_INV_ARG: Invalid input arguments
 *               - ESP_EXTR_ERR_NO_MEM:  memory alloc fail
 */
esp_extr_err_t esp_extractor_set_resume_info(esp_extractor_handle_t extractor,
                                             esp_extractor_resume_info_t *resume_info);

/**
 * @brief        Free backup resume information
 *
 * @param        resume_info: Resume information
 *
 * @return       - ESP_EXTR_ERR_OK: free success
 *               - ESP_EXTR_ERR_INV_ARG: input is NULL
 */
esp_extr_err_t esp_extractor_free_resume_info(esp_extractor_resume_info_t *resume_info);

/**
 * @brief      Get extractor type
 * @return     Current extractor type
 */
esp_extractor_type_t esp_extractor_get_extractor_type(esp_extractor_handle_t extractor);

/**
 * @brief        Close extractor
 *
 * @param        extractor: Extractor Handle
 *
 * @return        - ESP_EXTR_ERR_OK: On success
 *                - ESP_EXTR_ERR_INV_ARG: Extractor handle not valid
 */
esp_extr_err_t esp_extractor_close(esp_extractor_handle_t extractor);

#ifdef __cplusplus
}
#endif
