/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "esp_extractor.h"
#include "data_cache.h"
#include "mem_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXTRACTOR_RESERVE_FLAG_NEED_PARSING (1)

/**
 * @brief   Extractor base struct
 * @note    Common resource can be accessed from base directly
 *          To keep specified information, should store in `extractor_inst`
 *          And free related resource when close
 */
typedef struct {
    data_cache_t        *cache;           /*!< Data cache to read input data */
    mem_pool_handle_t    output_pool;     /*!< Output pool for output frame data */
    uint16_t             output_align;    /*!< Output alignment setting */
    bool                 wait_for_output; /*!< Wait for output buffer ready */
    uint8_t              extract_mask;    /*!< Extractor mask */
    esp_extractor_type_t type;            /*!< Extractor type */
    uint32_t             sub_type;        /*!< Sub extractor type */
    bool                 resume_case;     /*!< Resume playback case */
    bool                 no_accurate_seek; /*!< When enable seek to key frame only */
    void                *extractor_inst;   /*!< Extractor instance */
} extractor_t;

/**
 * @brief   Extractor open function
 */
typedef esp_extr_err_t (*_extractor_open)(extractor_t *extractor);

/**
 * @brief   Extractor close function
 */
typedef esp_extr_err_t (*_extractor_close)(extractor_t *extractor);

/**
 * @brief   Get stream number function
 */
typedef esp_extr_err_t (*_extractor_get_stream_num)(extractor_t *extractor, extractor_stream_type_t stream_type,
                                                    uint16_t *stream_num);

/**
 * @brief   Get stream number information function
 */
typedef esp_extr_err_t (*_extractor_get_stream_info)(extractor_t *extractor, extractor_stream_type_t stream_type,
                                                     uint16_t stream_index, extractor_stream_info_t *stream_info);

/**
 * @brief   Get resume information function
 */
typedef esp_extr_err_t (*_extractor_get_resume_info)(extractor_t *extractor, esp_extractor_resume_info_t *resume_info);

/**
 * @brief   Set resume function
 */
typedef esp_extr_err_t (*_extractor_set_resume_info)(extractor_t *extractor, esp_extractor_resume_info_t *resume_info);

/**
 * @brief   Enable stream function
 */
typedef esp_extr_err_t (*_extractor_enable_stream)(extractor_t *extractor, extractor_stream_type_t stream_type,
                                                   uint16_t stream_index);

/**
 * @brief   Disable stream function
 */
typedef esp_extr_err_t (*_extractor_disable_stream)(extractor_t *extractor, extractor_stream_type_t stream_type);

/**
 * @brief   Read frame function
 */
typedef esp_extr_err_t (*_extractor_read_frame)(extractor_t *extractor, extractor_frame_info_t *frame_info);

/**
 * @brief   Read abort function
 */
typedef esp_extr_err_t (*_extractor_read_abort)(extractor_t *extractor);

/**
 * @brief   Seek function
 */
typedef esp_extr_err_t (*_extractor_seek)(extractor_t *extractor, uint32_t time);

/**
 * @brief   Probe function to get sub extractor type
 */
typedef esp_extr_err_t (*_extractor_probe_func)(uint8_t *buffer, uint32_t size, uint32_t *sub_type);

/**
 * @brief   Check whether favorite file extension for this container
 */
typedef bool (*_extractor_favor_ext_func)(const char *file_ext);

/**
 * @brief   Extractor operation table
 */
typedef struct {
    _extractor_open            open;            /*!< Open function */
    _extractor_get_stream_num  get_stream_num;  /*!< Get stream number function */
    _extractor_get_stream_info get_stream_info; /*!< Get stream information function */
    _extractor_enable_stream   enable_stream;   /*!< Enable stream function */
    _extractor_disable_stream  disable_stream;  /*!< Disable stream function */
    _extractor_read_frame      read_frame;      /*!< Read frame function */
    _extractor_read_abort      read_abort;      /*!< Read abort function */
    _extractor_get_resume_info get_resume_info; /*!< Get resume information function */
    _extractor_set_resume_info set_resume_info; /*!< Set resume information function */
    _extractor_seek            seek;            /*!< Seek function */
    _extractor_close           close;           /*!< Close function */
} extractor_reg_table_t;

/**
 * @brief   Extractor register information
 */
typedef struct {
    extractor_reg_table_t     extract_table; /*!< Extractor function groups */
    _extractor_probe_func     probe_cb;      /*!< Callback to probe for this container */
    _extractor_favor_ext_func favor_cb;      /*!< Callback to check whether favorite extension */
} extractor_reg_info_t;

/**
 * @brief        Register extractor
 *
 * @param        type: Extractor type
 * @param        table: Register table, register table must be `static const` or life cycle over extractor
 *
 * @return       - ESP_EXTR_ERR_OK: On success
 *               - ESP_EXTR_ERR_NO_MEM: No enough memory
 */
esp_extr_err_t esp_extractor_register(esp_extractor_type_t type, const extractor_reg_info_t *table);

/**
 * @brief        Register all supported extractor type
 *
 * @return       - ESP_EXTR_ERR_OK: On success
 *               - ESP_EXTR_ERR_NO_MEM: No enough memory
 */
esp_extr_err_t esp_extractor_register_all(void);

/**
 * @brief      Unregister all registered extractor
 * @note       Do not unregister when extractor is running to avoid resource leakage
 *
 */
void esp_extractor_unregister_all(void);

#ifdef __cplusplus
}
#endif
