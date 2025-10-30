/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

#include "lvgl.h"
#include "lvgl_private.h"

#include "esp_lv_decoder.h"
#include "esp_lv_decoder_config.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include "esp_jpeg_dec.h"
#include "esp_cache.h"
#if ESP_LV_ENABLE_HW_JPEG
#include "driver/jpeg_decode.h"
#include "esp_jpeg_common.h"
#endif
#include "png.h"
#define QOI_IMPLEMENTATION
#include "qoi.h"

#define DECODER_NAME    "esp_decoder"
#define default_handlers &(LV_GLOBAL_DEFAULT()->draw_buf_handlers)
#define image_cache_draw_buf_handlers &(LV_GLOBAL_DEFAULT()->image_cache_draw_buf_handlers)

#if ESP_LV_ENABLE_PJPG
#define PJPG_MAGIC "_PJPG__"
#define PJPG_MAGIC_LEN 7
#define PJPG_HEADER_SIZE 22
#endif

#define SP_TAG_SPNG        "_SPNG__"
#define SP_TAG_SJPG        "_SJPG__"
#define SP_TAG_LEN         7
#define SP_META_OFFSET     14
#define SP_FILE_HEADER_SIZE 22

#if ESP_LV_ENABLE_PJPG
typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t rgb_jpeg_size;
    uint32_t alpha_jpeg_size;
} pjpg_info_t;
#endif

#if ESP_LV_ENABLE_PJPG
static uint8_t *s_pjpg_rx_rgb = NULL;
static size_t s_pjpg_rx_rgb_size = 0;
static uint8_t *s_pjpg_rx_a = NULL;
static size_t s_pjpg_rx_a_size = 0;
static SemaphoreHandle_t s_pjpg_mutex = NULL;  // Protect PJPG global buffers from concurrent access
#endif

typedef struct {
    bool is_file;
    bool is_sjpg;
    uint16_t w;
    uint16_t h;
    uint16_t splits;
    uint16_t split_h;
    uint16_t next_split;
    uint32_t next_y;
    uint32_t table_off;
    uint32_t data_off;
    uint32_t cursor;
    lv_fs_file_t file;
    const uint8_t * base;
    uint32_t base_size;
    lv_draw_buf_t * tile;
    uint8_t * slice_buf;
    lv_color_format_t out_cf;
    uint8_t bpp;
    uint32_t tile_width;
} sp_session_t;

#if ESP_LV_ENABLE_HW_JPEG
static jpeg_decoder_handle_t jpgd_handle;
static const jpeg_decode_cfg_t decode_cfg_rgb888 = {
    .output_format = JPEG_DECODE_OUT_FORMAT_RGB888,
    .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
};
static const jpeg_decode_cfg_t decode_cfg_rgb565 = {
    .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
    .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
};
#endif

static bool s_hw_output_allocation = false;

#if ESP_LV_ENABLE_HW_JPEG
typedef struct hw_buf_node {
    void *ptr;
    struct hw_buf_node *next;
} hw_buf_node_t;
static hw_buf_node_t *s_hw_buf_list = NULL;

static void hw_buf_register(void *p)
{
    if (!p) {
        return;
    }
    hw_buf_node_t *node = malloc(sizeof(hw_buf_node_t));
    if (!node) {
        return; /* Fallback: not tracked; buf_free will try standard free */
    }
    node->ptr = p;
    node->next = s_hw_buf_list;
    s_hw_buf_list = node;
}

static bool hw_buf_unregister_and_free(void *p)
{
    hw_buf_node_t *prev = NULL;
    hw_buf_node_t *cur = s_hw_buf_list;
    while (cur) {
        if (cur->ptr == p) {
            if (prev) {
                prev->next = cur->next;
            } else {
                s_hw_buf_list = cur->next;
            }
            jpeg_free_align(p);
            free(cur);
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false;
}
#endif

static lv_result_t decoder_info(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc, lv_image_header_t * header);
static lv_result_t decoder_open(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);
static void decoder_close(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);
static lv_result_t decoder_get_area(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc, const lv_area_t * full_area, lv_area_t * decoded_area);
static lv_draw_buf_t * jpeg_decode_jpg(lv_image_decoder_dsc_t * dsc);
static lv_draw_buf_t * png_decode_rgba(lv_image_decoder_dsc_t * dsc);
static lv_draw_buf_t * qoi_decode_qoi(lv_image_decoder_dsc_t * dsc);
static uint8_t * alloc_file(const char * filename, uint32_t * size);
static lv_res_t jpeg_decode_header(
    uint32_t *w,
    uint32_t *h,
    const uint8_t *in,
    uint32_t insize,
    jpeg_pixel_format_t out_fmt,
    jpeg_dec_handle_t *jpeg_dec_out,
    jpeg_dec_io_t **jpeg_io_out,
    jpeg_dec_header_info_t **header_info_out);

static lv_result_t sp_open_session(lv_image_decoder_dsc_t * dsc);

#if ESP_LV_ENABLE_PJPG
static esp_err_t pjpg_parse_header(const uint8_t *buf, size_t len, pjpg_info_t *info);
static lv_draw_buf_t * pjpg_decode_jpg(lv_image_decoder_dsc_t * dsc);
static esp_err_t load_source_data(lv_image_decoder_dsc_t * dsc, uint8_t **data, uint32_t *size);
#endif

static void buf_free(void * buf);
static void * buf_malloc(size_t size_bytes, lv_color_format_t color_format);
static void * buf_align(void * buf, lv_color_format_t color_format);
static uint32_t width_to_stride(uint32_t w, lv_color_format_t color_format);

static inline uint32_t read_be32(const uint8_t *p);
static inline bool sp_is_sp_tag(const uint8_t *hdr8, bool *is_sjpg);
static inline void sp_read_meta(const uint8_t *meta8, uint16_t *w, uint16_t *h, uint16_t *splits, uint16_t *split_h);

static const char *TAG = "lv_decoder_v9";

static lv_color_format_t get_target_rgb_format(void)
{
    lv_display_t *disp = lv_display_get_default();
    if (disp) {
        lv_color_format_t cf = lv_display_get_color_format(disp);
        if (cf == LV_COLOR_FORMAT_RGB565) {
            return LV_COLOR_FORMAT_RGB565;
        }
        if (cf == LV_COLOR_FORMAT_RGB888) {
            return LV_COLOR_FORMAT_RGB888;
        }
        if (cf == LV_COLOR_FORMAT_ARGB8888 || cf == LV_COLOR_FORMAT_XRGB8888) {
            return LV_COLOR_FORMAT_RGB888;
        }
    }
    return LV_COLOR_FORMAT_RGB888;
}

static inline uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

esp_err_t esp_lv_decoder_init(esp_lv_decoder_handle_t *ret_handle)
{
    lv_image_decoder_t * dec = lv_image_decoder_create();
    ESP_RETURN_ON_FALSE(dec, ESP_ERR_NO_MEM, TAG, "failed to create LVGL decoder");
    lv_image_decoder_set_info_cb(dec, decoder_info);
    lv_image_decoder_set_open_cb(dec, decoder_open);
    lv_image_decoder_set_get_area_cb(dec, decoder_get_area);
    lv_image_decoder_set_close_cb(dec, decoder_close);
#if ESP_LV_ENABLE_HW_JPEG
    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .timeout_ms = 40,
    };
    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&decode_eng_cfg, &jpgd_handle));
#endif

#if ESP_LV_ENABLE_PJPG
    if (!s_pjpg_mutex) {
        s_pjpg_mutex = xSemaphoreCreateMutex();
        if (!s_pjpg_mutex) {
            lv_image_decoder_delete(dec);
            return ESP_ERR_NO_MEM;
        }
    }
#endif

    lv_draw_buf_handlers_t * handlers = image_cache_draw_buf_handlers;
    handlers->buf_malloc_cb = buf_malloc;
    handlers->buf_free_cb = buf_free;
    handlers->align_pointer_cb = buf_align;
    handlers->width_to_stride_cb = width_to_stride;

    dec->name = DECODER_NAME;
    *ret_handle = dec;

    return ESP_OK;
}

esp_err_t esp_lv_decoder_deinit(esp_lv_decoder_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid decoder handle");
    lv_image_decoder_delete(handle);

#if ESP_LV_ENABLE_HW_JPEG
    ESP_ERROR_CHECK(jpeg_del_decoder_engine(jpgd_handle));
#endif
#if ESP_LV_ENABLE_PJPG
    if (s_pjpg_rx_rgb) {
        jpeg_free_align(s_pjpg_rx_rgb);
        s_pjpg_rx_rgb = NULL;
        s_pjpg_rx_rgb_size = 0;
    }
    if (s_pjpg_rx_a) {
        jpeg_free_align(s_pjpg_rx_a);
        s_pjpg_rx_a = NULL;
        s_pjpg_rx_a_size = 0;
    }
    if (s_pjpg_mutex) {
        vSemaphoreDelete(s_pjpg_mutex);
        s_pjpg_mutex = NULL;
    }
#endif
    return ESP_OK;
}

static lv_result_t decoder_info(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc, lv_image_header_t * header)
{
    /*
     * Lightweight inspection of the source to fill width/height/format.
     * No heavy decoding here; only read headers or minimal bytes.
     */
    LV_UNUSED(decoder);

    lv_image_src_t src_type = dsc->src_type;
    if (src_type == LV_IMAGE_SRC_FILE || src_type == LV_IMAGE_SRC_VARIABLE) {
#if ESP_LV_ENABLE_PJPG
        if (src_type == LV_IMAGE_SRC_FILE) {
            uint32_t rn;
            uint32_t file_size;
            lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_END);
            lv_fs_tell(&dsc->file, &file_size);
            lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);

            if (file_size >= PJPG_HEADER_SIZE) {
                uint8_t *pjpg_header = malloc(PJPG_HEADER_SIZE);
                if (pjpg_header) {
                    lv_fs_read(&dsc->file, pjpg_header, PJPG_HEADER_SIZE, &rn);
                    lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);

                    if (rn == PJPG_HEADER_SIZE && memcmp(pjpg_header, PJPG_MAGIC, PJPG_MAGIC_LEN) == 0) {
                        pjpg_info_t pjpg_info;
                        if (pjpg_parse_header(pjpg_header, PJPG_HEADER_SIZE, &pjpg_info) == ESP_OK) {
                            header->cf = LV_COLOR_FORMAT_RGB565A8;
                            header->w = pjpg_info.width;
                            header->h = pjpg_info.height;
                            // RGB565A8 is planar: stride is for RGB565 plane only
                            header->stride = pjpg_info.width * 2;
                            free(pjpg_header);
                            return LV_RESULT_OK;
                        }
                    }
                    free(pjpg_header);
                }
            }
            lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
        } else if (src_type == LV_IMAGE_SRC_VARIABLE) {
            const lv_image_dsc_t * img_dsc = dsc->src;
            if (img_dsc->data_size >= PJPG_HEADER_SIZE &&
                    memcmp(img_dsc->data, PJPG_MAGIC, PJPG_MAGIC_LEN) == 0) {
                pjpg_info_t pjpg_info;
                if (pjpg_parse_header(img_dsc->data, img_dsc->data_size, &pjpg_info) == ESP_OK) {
                    header->cf = LV_COLOR_FORMAT_RGB565A8;
                    header->w = pjpg_info.width;
                    header->h = pjpg_info.height;
                    // RGB565A8 is planar: stride is for RGB565 plane only
                    header->stride = pjpg_info.width * 2;
                    return LV_RESULT_OK;
                }
            }
        }
#endif
        bool is_png = false;
        bool is_qoi = false;
        uint8_t png_magic[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        uint8_t qoi_magic[4] = {0x71, 0x6F, 0x69, 0x66};
        bool is_sp = false;
        bool is_sjpg = false;
        if (src_type == LV_IMAGE_SRC_FILE) {
            uint8_t hdr8[8];
            uint32_t rn;
            lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
            if (lv_fs_read(&dsc->file, hdr8, 8, &rn) == LV_FS_RES_OK && rn == 8) {
                if (sp_is_sp_tag(hdr8, &is_sjpg)) {
                    is_sp = true;
                }
            }
            lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
        } else {
            const lv_image_dsc_t *img = dsc->src;
            if (img->data_size >= 8) {
                if (sp_is_sp_tag(img->data, &is_sjpg)) {
                    is_sp = true;
                }
            }
        }
        if (is_sp) {
            uint8_t wh[4];
            uint32_t rn2 = 0;
            if (src_type == LV_IMAGE_SRC_FILE) {
                lv_fs_seek(&dsc->file, SP_META_OFFSET, LV_FS_SEEK_SET);
                if (lv_fs_read(&dsc->file, wh, 4, &rn2) != LV_FS_RES_OK || rn2 != 4) {
                    lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
                    return LV_RESULT_INVALID;
                }
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
            } else {
                const lv_image_dsc_t *img = dsc->src;
                if (img->data_size < 18) {
                    return LV_RESULT_INVALID;
                }
                memcpy(wh, (const uint8_t *)img->data + SP_META_OFFSET, 4);
            }
            uint32_t w = read_le16(wh);
            uint32_t h = read_le16(wh + 2);
            if (w == 0 || h == 0) {
                return LV_RESULT_INVALID;
            }
            if (is_sjpg) {
                lv_color_format_t tgt = get_target_rgb_format();
                header->cf = (tgt == LV_COLOR_FORMAT_RGB565) ? LV_COLOR_FORMAT_RGB565 : LV_COLOR_FORMAT_RGB888;
                header->stride = (tgt == LV_COLOR_FORMAT_RGB565) ? (w * 2) : (w * 3);
            } else {
                header->cf = LV_COLOR_FORMAT_ARGB8888;
                header->stride = w * 4;
            }
            header->w = w;
            header->h = h;
            return LV_RESULT_OK;
        }
        if (src_type == LV_IMAGE_SRC_FILE) {
            const char *ext = lv_fs_get_ext(dsc->src);
            if (ext && lv_strcmp(ext, "png") == 0) {
                uint8_t hdr[8];
                uint32_t rn;
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
                lv_fs_read(&dsc->file, hdr, 8, &rn);
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
                if (rn == 8 && memcmp(hdr, png_magic, 8) == 0) {
                    is_png = true;
                }
            }
            if (ext && lv_strcmp(ext, "qoi") == 0) {
                uint8_t hdr[14];
                uint32_t rn;
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
                lv_fs_read(&dsc->file, hdr, sizeof(hdr), &rn);
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
                if (rn >= 14 && memcmp(hdr, qoi_magic, 4) == 0) {
                    is_qoi = true;
                }
            }
        } else {
            const lv_image_dsc_t *img_dsc = dsc->src;
            if (img_dsc->data_size >= 8 && memcmp(img_dsc->data, png_magic, 8) == 0) {
                is_png = true;
            }
            if (img_dsc->data_size >= 14 && memcmp(img_dsc->data, qoi_magic, 4) == 0) {
                is_qoi = true;
            }
        }

        if (is_png) {
            // Parse PNG IHDR directly to get width/height
            uint8_t hdr[24];
            if (src_type == LV_IMAGE_SRC_FILE) {
                uint32_t rn;
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
                if (lv_fs_read(&dsc->file, hdr, sizeof(hdr), &rn) != LV_FS_RES_OK || rn < sizeof(hdr)) {
                    lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
                    return LV_RESULT_INVALID;
                }
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
            } else {
                const lv_image_dsc_t *img_dsc = dsc->src;
                if (img_dsc->data_size < sizeof(hdr)) {
                    return LV_RESULT_INVALID;
                }
                memcpy(hdr, img_dsc->data, sizeof(hdr));
            }
            if (memcmp(hdr, png_magic, 8) != 0) {
                return LV_RESULT_INVALID;
            }
            uint32_t w = read_be32(&hdr[16]);
            uint32_t h = read_be32(&hdr[20]);
            if (w == 0 || h == 0) {
                return LV_RESULT_INVALID;
            }
            header->cf = LV_COLOR_FORMAT_ARGB8888;
            header->w = w;
            header->h = h;
            header->stride = w * 4;

            return LV_RESULT_OK;
        }

        if (is_qoi) {
            uint8_t hdr[14];
            if (src_type == LV_IMAGE_SRC_FILE) {
                uint32_t rn;
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
                if (lv_fs_read(&dsc->file, hdr, sizeof(hdr), &rn) != LV_FS_RES_OK || rn < sizeof(hdr)) {
                    lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
                    return LV_RESULT_INVALID;
                }
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
            } else {
                const lv_image_dsc_t *img_dsc = dsc->src;
                if (img_dsc->data_size < sizeof(hdr)) {
                    return LV_RESULT_INVALID;
                }
                memcpy(hdr, img_dsc->data, sizeof(hdr));
            }
            if (memcmp(hdr, qoi_magic, 4) != 0) {
                return LV_RESULT_INVALID;
            }
            uint32_t w = read_be32(&hdr[4]);
            uint32_t h = read_be32(&hdr[8]);
            uint8_t channels = hdr[12];
            if (w == 0 || h == 0 || (channels != 3 && channels != 4)) {
                return LV_RESULT_INVALID;
            }
            if (channels == 4) {
                header->cf = LV_COLOR_FORMAT_ARGB8888;
                header->stride = w * 4;
            } else {
                header->cf = LV_COLOR_FORMAT_RGB888;
                header->stride = w * 3;
            }
            header->w = w;
            header->h = h;
            return LV_RESULT_OK;
        }

        // Check for JPEG format
        static const uint8_t jpeg_magic[] = {0xFF, 0xD8};  // Standard JPEG magic bytes
        uint8_t *jpeg_data = NULL;
        uint32_t jpeg_size;
        bool is_jpeg = false;

        if (src_type == LV_IMAGE_SRC_FILE) {
            // Check file extension first
            const char* ext = lv_fs_get_ext(dsc->src);
            if (ext && (lv_strcmp(ext, "jpg") == 0 || lv_strcmp(ext, "jpeg") == 0)) {
                is_jpeg = true;
            }

            // If extension suggests JPEG, also check magic bytes
            if (is_jpeg) {
                uint32_t rn;
                uint8_t header[2];
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
                lv_fs_read(&dsc->file, header, 2, &rn);
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);

                if (rn == 2 && memcmp(header, jpeg_magic, 2) == 0) {
                    // Get file size for header parsing
                    lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_END);
                    lv_fs_tell(&dsc->file, &jpeg_size);
                    lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);

                    // Read first 1KB for header parsing
                    uint32_t read_size = (jpeg_size > 1024) ? 1024 : jpeg_size;
                    jpeg_data = malloc(read_size);
                    if (jpeg_data) {
                        lv_fs_read(&dsc->file, jpeg_data, read_size, &rn);
                        lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
                        if (rn == read_size) {
                            jpeg_size = read_size;
                        } else {
                            free(jpeg_data);
                            jpeg_data = NULL;
                            is_jpeg = false;
                        }
                    } else {
                        is_jpeg = false;
                    }
                } else {
                    is_jpeg = false;
                }
            }
        } else if (src_type == LV_IMAGE_SRC_VARIABLE) {
            const lv_image_dsc_t * img_dsc = dsc->src;
            jpeg_data = (uint8_t *)img_dsc->data;
            jpeg_size = img_dsc->data_size;

            // Check JPEG magic bytes
            if (jpeg_size >= 2 && memcmp(jpeg_data, jpeg_magic, 2) == 0) {
                is_jpeg = true;
            }
        }

        if (is_jpeg && jpeg_data) {
            uint32_t width = 0;
            uint32_t height = 0;
            lv_color_format_t tgt = get_target_rgb_format();
            jpeg_pixel_format_t out_fmt = (tgt == LV_COLOR_FORMAT_RGB565) ? JPEG_PIXEL_FORMAT_RGB565_LE : JPEG_PIXEL_FORMAT_RGB888;
            lv_res_t res = jpeg_decode_header(&width, &height, jpeg_data, jpeg_size, out_fmt, NULL, NULL, NULL);

            if (src_type == LV_IMAGE_SRC_FILE) {
                free(jpeg_data);
            }

            if (res == LV_RES_OK && width > 0 && height > 0) {
                header->cf = (tgt == LV_COLOR_FORMAT_RGB565) ? LV_COLOR_FORMAT_RGB565 : LV_COLOR_FORMAT_RGB888;
                header->w = width;
                header->h = height;
                header->stride = (tgt == LV_COLOR_FORMAT_RGB565) ? (width * 2) : (width * 3);
                return LV_RESULT_OK;
            }
        }
    }
    return LV_RESULT_INVALID;
}

static lv_result_t decoder_open(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder);
    lv_draw_buf_t * decoded = NULL;

#if ESP_LV_ENABLE_PJPG
    bool is_pjpg = false;
    if (dsc->src_type == LV_IMAGE_SRC_FILE) {
        const char *ext = lv_fs_get_ext(dsc->src);
        if (ext && lv_strcmp(ext, "pjpg") == 0) {
            is_pjpg = true;
        }
    } else if (dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
        const lv_image_dsc_t * img_dsc = dsc->src;
        if (img_dsc->data_size >= PJPG_HEADER_SIZE && memcmp(img_dsc->data, PJPG_MAGIC, PJPG_MAGIC_LEN) == 0) {
            is_pjpg = true;
        }
    }
    if (is_pjpg) {
        decoded = pjpg_decode_jpg(dsc);
        if (!decoded) {
            return LV_RESULT_INVALID;
        }
        goto post_process;
    }
#endif

    {
        bool is_sp = false;

        // First, try to detect by file extension (more reliable with mmap_assets)
        if (dsc->src_type == LV_IMAGE_SRC_FILE) {
            const char *ext = lv_fs_get_ext(dsc->src);
            if (ext && (lv_strcmp(ext, "spng") == 0 || lv_strcmp(ext, "sjpg") == 0)) {
                is_sp = true;
            }
        }

        // If not detected by extension, try reading header (for memory sources or validation)
        if (!is_sp) {
            uint8_t hdr8[8];
            if (dsc->src_type == LV_IMAGE_SRC_FILE) {
                uint32_t rn;
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
                if (lv_fs_read(&dsc->file, hdr8, 8, &rn) == LV_FS_RES_OK && rn == 8) {
                    bool dummy = false;
                    if (sp_is_sp_tag(hdr8, &dummy)) {
                        is_sp = true;
                    }
                }
                lv_fs_seek(&dsc->file, 0, LV_FS_SEEK_SET);
            } else if (dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
                const lv_image_dsc_t * img = dsc->src;
                if (img->data_size >= 8) {
                    bool dummy = false;
                    if (sp_is_sp_tag(img->data, &dummy)) {
                        is_sp = true;
                    }
                }
            }
        }

        if (is_sp) {
            if (sp_open_session(dsc) != LV_RESULT_OK) {
                return LV_RESULT_INVALID;
            }
            return LV_RESULT_OK;
        }
    }

    {
        bool try_qoi = false;
        if (dsc->src_type == LV_IMAGE_SRC_FILE) {
            const char *ext = lv_fs_get_ext(dsc->src);
            if (ext && lv_strcmp(ext, "qoi") == 0) {
                try_qoi = true;
            }
        } else if (dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
            const lv_image_dsc_t * img_dsc = dsc->src;
            if (img_dsc->data_size >= 4) {
                static const uint8_t qoi_magic[4] = {0x71, 0x6F, 0x69, 0x66};
                try_qoi = (memcmp(img_dsc->data, qoi_magic, 4) == 0);
            }
        }
        if (try_qoi) {
            decoded = qoi_decode_qoi(dsc);
            if (!decoded) {
                return LV_RESULT_INVALID;
            }
            goto post_process;
        }
    }

    {
        bool try_png = false;
        if (dsc->src_type == LV_IMAGE_SRC_FILE) {
            const char *ext = lv_fs_get_ext(dsc->src);
            if (ext && lv_strcmp(ext, "png") == 0) {
                try_png = true;
            }
        } else if (dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
            const lv_image_dsc_t * img_dsc = dsc->src;
            if (img_dsc->data_size >= 8) {
                static const uint8_t png_magic[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
                try_png = (memcmp(img_dsc->data, png_magic, 8) == 0);
            }
        }
        if (try_png) {
            decoded = png_decode_rgba(dsc);
            if (!decoded) {
                return LV_RESULT_INVALID;
            }
            goto post_process;
        }
    }

    {
        bool is_jpeg = false;
        if (dsc->src_type == LV_IMAGE_SRC_FILE) {
            const char *ext = lv_fs_get_ext(dsc->src);
            if (ext && (lv_strcmp(ext, "jpg") == 0 || lv_strcmp(ext, "jpeg") == 0)) {
                is_jpeg = true;
            }
        } else if (dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
            const lv_image_dsc_t * img_dsc = dsc->src;
            static const uint8_t jpeg_magic[] = {0xFF, 0xD8};
            if (img_dsc->data_size >= 2 && memcmp(img_dsc->data, jpeg_magic, 2) == 0) {
                is_jpeg = true;
            }
        }
        if (is_jpeg) {
            decoded = jpeg_decode_jpg(dsc);
            if (!decoded) {
                return LV_RESULT_INVALID;
            }
            goto post_process;
        }
    }

    return LV_RESULT_INVALID;

post_process: {
        lv_draw_buf_t * adjusted = lv_image_decoder_post_process(dsc, decoded);
        if (adjusted == NULL) {
            lv_draw_buf_destroy_user(image_cache_draw_buf_handlers, decoded);
            return LV_RESULT_INVALID;
        }
        if (adjusted != decoded) {
            lv_draw_buf_destroy_user(image_cache_draw_buf_handlers, decoded);
            decoded = adjusted;
        }
        dsc->decoded = decoded;
        if (dsc->args.no_cache) {
            return LV_RESULT_OK;
        }
        if (!lv_image_cache_is_enabled()) {
            return LV_RESULT_OK;
        }
        lv_image_cache_data_t search_key;
        search_key.src_type = dsc->src_type;
        search_key.src = dsc->src;
        search_key.slot.size = decoded->data_size;
        lv_cache_entry_t * entry = lv_image_decoder_add_to_cache(decoder, &search_key, decoded, NULL);
        if (entry == NULL) {
            lv_draw_buf_destroy_user(image_cache_draw_buf_handlers, decoded);
            return LV_RESULT_INVALID;
        }
        dsc->cache_entry = entry;
        return LV_RESULT_OK;
    }
}

static lv_result_t sp_open_session(lv_image_decoder_dsc_t * dsc)
{
    bool is_file = (dsc->src_type == LV_IMAGE_SRC_FILE);
    uint8_t hdr8[8];
    uint32_t rn = 0;
    const uint8_t * base = NULL;
    uint32_t base_size = 0;
    lv_fs_file_t f;
    if (is_file) {
        if (lv_fs_open(&f, dsc->src, LV_FS_MODE_RD) != LV_FS_RES_OK) {
            return LV_RESULT_INVALID;
        }
        if (lv_fs_read(&f, hdr8, 8, &rn) != LV_FS_RES_OK || rn != 8) {
            lv_fs_close(&f);
            return LV_RESULT_INVALID;
        }
    } else if (dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
        const lv_image_dsc_t *img = dsc->src;
        if (img->data_size < 8) {
            return LV_RESULT_INVALID;
        }
        memcpy(hdr8, img->data, 8);
        base = (const uint8_t *)img->data;
        base_size = img->data_size;
    } else {
        return LV_RESULT_INVALID;
    }
    bool is_sjpg = false;
    if (!sp_is_sp_tag(hdr8, &is_sjpg)) {
        if (is_file) {
            lv_fs_close(&f);
        }
        return LV_RESULT_INVALID;
    }
    uint8_t meta[8];
    if (is_file) {
        if (lv_fs_seek(&f, SP_META_OFFSET, LV_FS_SEEK_SET) != LV_FS_RES_OK) {
            lv_fs_close(&f);
            return LV_RESULT_INVALID;
        }
        if (lv_fs_read(&f, meta, 8, &rn) != LV_FS_RES_OK || rn != 8) {
            lv_fs_close(&f);
            return LV_RESULT_INVALID;
        }
    } else {
        if (base_size < SP_FILE_HEADER_SIZE) {
            return LV_RESULT_INVALID;
        }
        memcpy(meta, base + SP_META_OFFSET, 8);
    }
    uint16_t w = 0, h = 0, splits = 0, split_h = 0;
    sp_read_meta(meta, &w, &h, &splits, &split_h);
    if (w == 0 || h == 0 || splits == 0 || split_h == 0) {
        if (is_file) {
            lv_fs_close(&f);
        }
        return LV_RESULT_INVALID;
    }
    sp_session_t * ctx = lv_malloc(sizeof(sp_session_t));
    if (!ctx) {
        if (is_file) {
            lv_fs_close(&f);
        }
        return LV_RESULT_INVALID;
    }
    ctx->is_file = is_file;
    ctx->is_sjpg = is_sjpg;
    ctx->w = w;
    ctx->h = h;
    ctx->splits = splits;
    ctx->split_h = split_h;
    ctx->next_split = 0;
    ctx->next_y = 0;
    ctx->table_off = SP_FILE_HEADER_SIZE;
    ctx->data_off = SP_FILE_HEADER_SIZE + splits * 2;
    ctx->cursor = 0;
    ctx->base = base;
    ctx->base_size = base_size;
    ctx->tile = NULL;
    ctx->slice_buf = NULL;
    ctx->out_cf = is_sjpg ? get_target_rgb_format() : LV_COLOR_FORMAT_ARGB8888;
    ctx->bpp = (ctx->out_cf == LV_COLOR_FORMAT_RGB565) ? 2 : (ctx->out_cf == LV_COLOR_FORMAT_RGB888 ? 3 : 4);
    ctx->tile_width = 0;
    if (is_file) {
        ctx->file = f;
    }
    dsc->user_data = ctx;
    dsc->args.no_cache = true;
    dsc->decoded = NULL;
    return LV_RESULT_OK;
}

static uint16_t sp_read_len(sp_session_t *ctx, uint16_t idx)
{
    if (ctx->is_file) {
        uint8_t l[2];
        uint32_t rn1 = 0;
        lv_fs_seek(&ctx->file, ctx->table_off + idx * 2, LV_FS_SEEK_SET);
        if (lv_fs_read(&ctx->file, l, 2, &rn1) != LV_FS_RES_OK || rn1 != 2) {
            return 0;
        }
        return (uint16_t)((uint16_t)l[0] | ((uint16_t)l[1] << 8));
    } else {
        return (uint16_t)((uint16_t)ctx->base[ctx->table_off + idx * 2] | ((uint16_t)ctx->base[ctx->table_off + idx * 2 + 1] << 8));
    }
}

static lv_result_t sp_decode_split(sp_session_t *ctx, uint16_t idx, uint8_t *dst_buf)
{
    uint16_t len_i = sp_read_len(ctx, idx);
    if (len_i == 0) {
        return LV_RESULT_INVALID;
    }
    uint32_t y0 = (uint32_t)idx * ctx->split_h;
    uint32_t cur_h = ctx->split_h;
    if (y0 + cur_h > ctx->h) {
        cur_h = ctx->h - y0;
    }
    uint8_t *encoded = NULL;
    if (ctx->is_file) {
        encoded = lv_malloc(len_i);
        if (!encoded) {
            return LV_RESULT_INVALID;
        }
        uint32_t rn2 = 0;
        uint32_t off = ctx->data_off + ctx->cursor;
        lv_fs_seek(&ctx->file, off, LV_FS_SEEK_SET);
        if (lv_fs_read(&ctx->file, encoded, len_i, &rn2) != LV_FS_RES_OK || rn2 != len_i) {
            lv_free(encoded);
            return LV_RESULT_INVALID;
        }
    } else {
        if (ctx->data_off + ctx->cursor + len_i > ctx->base_size) {
            return LV_RESULT_INVALID;
        }
        encoded = (uint8_t *)(ctx->base + ctx->data_off + ctx->cursor);
    }
    if (!ctx->slice_buf) {
        // Allocate with padding for JPEG decoder alignment requirements
        // JPEG decoder may need 16-byte alignment and extra space
        uint32_t max_slice_bytes = (uint32_t)ctx->w * ctx->split_h * ctx->bpp;
        // Add 128 bytes padding for alignment and decoder internal buffer
        uint32_t alloc_size = max_slice_bytes + 128;
        ctx->slice_buf = heap_caps_aligned_alloc(16, alloc_size, MALLOC_CAP_DEFAULT);
        if (!ctx->slice_buf) {
            ESP_LOGE(TAG, "Failed to allocate aligned slice_buf: %lu bytes (w=%u, split_h=%u, bpp=%u)",
                     (unsigned long)alloc_size, ctx->w, ctx->split_h, ctx->bpp);
            if (ctx->is_file) {
                lv_free(encoded);
            }
            return LV_RESULT_INVALID;
        }
        ESP_LOGD(TAG, "Allocated aligned slice_buf: %lu bytes (requested: %lu)",
                 (unsigned long)alloc_size, (unsigned long)max_slice_bytes);
    }
    if (!ctx->is_sjpg) {
        png_image im;
        memset(&im, 0, sizeof(im));
        im.version = PNG_IMAGE_VERSION;
        if (!png_image_begin_read_from_memory(&im, encoded, len_i)) {
            if (ctx->is_file) {
                lv_free(encoded);
            }
            return LV_RESULT_INVALID;
        }
        im.format = PNG_FORMAT_BGRA;
        uint32_t need = (uint32_t)ctx->w * cur_h * 4;
        if (need > (uint32_t)ctx->w * ctx->split_h * 4) {
            png_image_free(&im);
            if (ctx->is_file) {
                lv_free(encoded);
            }
            return LV_RESULT_INVALID;
        }
        if (!png_image_finish_read(&im, NULL, ctx->slice_buf, 0, NULL)) {
            png_image_free(&im);
            if (ctx->is_file) {
                lv_free(encoded);
            }
            return LV_RESULT_INVALID;
        }
        png_image_free(&im);
    } else {
        uint32_t jw = 0, jh = 0;
        jpeg_dec_handle_t jdec = NULL;
        jpeg_dec_io_t *jio = NULL;
        jpeg_dec_header_info_t *hinfo = NULL;
        jpeg_pixel_format_t out_fmt = (ctx->out_cf == LV_COLOR_FORMAT_RGB565) ? JPEG_PIXEL_FORMAT_RGB565_LE : JPEG_PIXEL_FORMAT_RGB888;
        if (jpeg_decode_header(&jw, &jh, encoded, len_i, out_fmt, &jdec, &jio, &hinfo) != LV_RESULT_OK) {
            if (ctx->is_file) {
                lv_free(encoded);
            }
            return LV_RESULT_INVALID;
        }
        if (jw != ctx->w || jh != cur_h) {
            ESP_LOGE(TAG, "SJPG dimension mismatch: expected %" PRIu32 "x%" PRIu32 ", got %" PRIu32 "x%" PRIu32,
                     (uint32_t)ctx->w, cur_h, jw, jh);
            if (jio) {
                free(jio);
            }
            if (hinfo) {
                free(hinfo);
            }
            if (jdec) {
                jpeg_dec_close(jdec);
            }
            if (ctx->is_file) {
                lv_free(encoded);
            }
            return LV_RESULT_INVALID;
        }
        // Verify buffer size before JPEG decode
        uint32_t required_bytes = jw * jh * ctx->bpp;
        uint32_t max_slice_bytes = (uint32_t)ctx->w * ctx->split_h * ctx->bpp;
        if (required_bytes > max_slice_bytes) {
            ESP_LOGE(TAG, "SJPG buffer overflow risk: required %lu > allocated %lu",
                     (unsigned long)required_bytes, (unsigned long)max_slice_bytes);
            if (jio) {
                free(jio);
            }
            if (hinfo) {
                free(hinfo);
            }
            if (jdec) {
                jpeg_dec_close(jdec);
            }
            if (ctx->is_file) {
                lv_free(encoded);
            }
            return LV_RESULT_INVALID;
        }
        jio->outbuf = ctx->slice_buf;
        if (jpeg_dec_process(jdec, jio) != JPEG_ERR_OK) {
            if (jio) {
                free(jio);
            }
            if (hinfo) {
                free(hinfo);
            }
            if (jdec) {
                jpeg_dec_close(jdec);
            }
            if (ctx->is_file) {
                lv_free(encoded);
            }
            return LV_RESULT_INVALID;
        }
        if (ctx->out_cf == LV_COLOR_FORMAT_RGB888) {
            uint8_t *p = ctx->slice_buf;
            for (uint32_t k = 0; k < jw * jh; k++) {
                uint8_t r = p[0];
                uint8_t b = p[2];
                p[0] = b;
                p[2] = r;
                p += 3;
            }
        }
        if (jio) {
            free(jio);
        }
        if (hinfo) {
            free(hinfo);
        }
        if (jdec) {
            jpeg_dec_close(jdec);
        }
    }
    if (ctx->is_file) {
        lv_free(encoded);
    }
    ctx->cursor += len_i;
    return LV_RESULT_OK;
}

static lv_result_t decoder_get_area(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc, const lv_area_t * full_area, lv_area_t * decoded_area)
{
    LV_UNUSED(decoder);
    if (!dsc->user_data) {
        return LV_RESULT_INVALID;
    }
    sp_session_t *ctx = (sp_session_t *)dsc->user_data;
    if (ctx->w == 0 || ctx->h == 0) {
        return LV_RESULT_INVALID;
    }
    if (full_area->x1 > full_area->x2 || full_area->y1 > full_area->y2) {
        return LV_RESULT_INVALID;
    }
    uint32_t x1 = full_area->x1 < 0 ? 0 : (uint32_t)full_area->x1;
    uint32_t x2 = (uint32_t)full_area->x2;
    if (x2 >= ctx->w) {
        x2 = ctx->w - 1;
    }
    if (x1 > x2) {
        return LV_RESULT_INVALID;
    }
    uint32_t start_y = (decoded_area->y1 == LV_COORD_MIN) ? (uint32_t)full_area->y1 : (uint32_t)(decoded_area->y2 + 1);
    if (start_y < ctx->next_y) {
        start_y = ctx->next_y;
    }
    if (start_y >= (uint32_t)full_area->y2 + 1 || start_y >= ctx->h) {
        return LV_RESULT_INVALID;
    }
    while (ctx->next_split < ctx->splits) {
        uint32_t split_base_y = (uint32_t)ctx->next_split * ctx->split_h;
        uint32_t split_end_y = split_base_y + ctx->split_h;
        if (split_end_y > ctx->h) {
            split_end_y = ctx->h;
        }
        if (start_y >= split_end_y) {
            uint16_t len = sp_read_len(ctx, ctx->next_split);
            if (len == 0) {
                return LV_RESULT_INVALID;
            }
            ctx->cursor += len;
            ctx->next_split++;
            ctx->next_y = split_end_y;
            continue;
        }
        uint32_t y0_in_split = (start_y > split_base_y) ? (start_y - split_base_y) : 0;
        if (sp_decode_split(ctx, ctx->next_split, ctx->slice_buf) != LV_RESULT_OK) {
            return LV_RESULT_INVALID;
        }
        uint32_t remaining_in_split = split_end_y - start_y;
        uint32_t remaining_in_area = (uint32_t)full_area->y2 + 1 - start_y;
        uint32_t copy_h = remaining_in_split < remaining_in_area ? remaining_in_split : remaining_in_area;
        uint32_t out_w = x2 - x1 + 1;
        if (ctx->tile == NULL || ctx->tile_width != out_w || ctx->tile->header.h != (int32_t)copy_h || ctx->tile->header.cf != ctx->out_cf) {
            if (ctx->tile) {
                lv_draw_buf_destroy_user(image_cache_draw_buf_handlers, ctx->tile);
            }
            ctx->tile = lv_draw_buf_create_ex(image_cache_draw_buf_handlers, out_w, copy_h, ctx->out_cf, LV_STRIDE_AUTO);
            if (!ctx->tile) {
                return LV_RESULT_INVALID;
            }
            ctx->tile_width = out_w;
        }
        uint8_t *dst = (uint8_t *)ctx->tile->data;
        uint32_t row_bytes_full = ctx->w * ctx->bpp;
        uint32_t row_bytes_out = out_w * ctx->bpp;
        uint8_t *src_base = ctx->slice_buf + y0_in_split * row_bytes_full + x1 * ctx->bpp;
        for (uint32_t y = 0; y < copy_h; y++) {
            memcpy(dst + y * ctx->tile->header.stride, src_base + y * row_bytes_full, row_bytes_out);
        }
        dsc->decoded = ctx->tile;
        decoded_area->x1 = full_area->x1;
        decoded_area->x2 = full_area->x2;
        decoded_area->y1 = (lv_coord_t)start_y;
        decoded_area->y2 = (lv_coord_t)(start_y + copy_h - 1);
        ctx->next_y = decoded_area->y2 + 1;
        if (ctx->next_y >= split_end_y) {
            ctx->next_split++;
        }
        return LV_RESULT_OK;
    }
    return LV_RESULT_INVALID;
}

static void decoder_close(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder);
    sp_session_t *ctx = (sp_session_t *)dsc->user_data;
    if (ctx) {
        if (ctx->tile) {
            lv_draw_buf_destroy_user(image_cache_draw_buf_handlers, ctx->tile);
            ctx->tile = NULL;
        }
        if (ctx->slice_buf) {
            // Use free() not lv_free() for heap_caps_aligned_alloc()
            free(ctx->slice_buf);
            ctx->slice_buf = NULL;
        }
        if (ctx->is_file) {
            lv_fs_close(&ctx->file);
        }
        lv_free(ctx);
        dsc->user_data = NULL;
        dsc->decoded = NULL;
        return;
    }
    // Only free the buffer if it's NOT in cache (cache_entry == NULL)
    // If cache_entry is set, the cache system manages the buffer lifetime
    if (dsc->cache_entry == NULL && dsc->decoded) {
        lv_draw_buf_destroy_user(image_cache_draw_buf_handlers, (lv_draw_buf_t *)dsc->decoded);
    }
}

static uint8_t * alloc_file(const char * filename, uint32_t * size)
{
    uint8_t * data = NULL;
    lv_fs_file_t f;
    uint32_t data_size;
    uint32_t rn;
    lv_fs_res_t res;
    esp_err_t ret;
    *size = 0;
    res = lv_fs_open(&f, filename, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        ESP_LOGE(TAG, "can't open %s", filename);
        return NULL;
    }
    res = lv_fs_seek(&f, 0, LV_FS_SEEK_END);
    ESP_GOTO_ON_ERROR(res == LV_FS_RES_OK ? ESP_OK : ESP_FAIL, failed, TAG, "lv_fs_seek end failed");
    res = lv_fs_tell(&f, &data_size);
    ESP_GOTO_ON_ERROR(res == LV_FS_RES_OK ? ESP_OK : ESP_FAIL, failed, TAG, "lv_fs_tell failed");
    res = lv_fs_seek(&f, 0, LV_FS_SEEK_SET);
    ESP_GOTO_ON_ERROR(res == LV_FS_RES_OK ? ESP_OK : ESP_FAIL, failed, TAG, "lv_fs_seek set failed");
    data = malloc(data_size);
    ESP_GOTO_ON_ERROR(data ? ESP_OK : ESP_ERR_NO_MEM, failed, TAG, "malloc %uB failed", (unsigned)data_size);
    res = lv_fs_read(&f, data, data_size, &rn);
    if (res == LV_FS_RES_OK && rn == data_size) {
        *size = rn;
    } else {
        ESP_LOGE(TAG, "read file failed");
        free(data);
        data = NULL;
    }
failed:
    lv_fs_close(&f);
    (void)ret;
    return data;
}

static lv_res_t jpeg_decode_header(uint32_t *w, uint32_t *h, const uint8_t *in, uint32_t insize,
                                   jpeg_pixel_format_t out_fmt,
                                   jpeg_dec_handle_t *jpeg_dec_out, jpeg_dec_io_t **jpeg_io_out,
                                   jpeg_dec_header_info_t **header_info_out)
{
    jpeg_error_t ret;
    jpeg_dec_config_t config = {
        .output_type = out_fmt,
        .rotate = JPEG_ROTATE_0D,
    };
    jpeg_dec_handle_t jpeg_dec;
    jpeg_dec_open(&config, &jpeg_dec);
    if (!jpeg_dec) {
        ESP_LOGE(TAG, "Failed to open jpeg decoder");
        return LV_RESULT_INVALID;
    }

    jpeg_dec_io_t *jpeg_io = malloc(sizeof(jpeg_dec_io_t));
    jpeg_dec_header_info_t *header_info = malloc(sizeof(jpeg_dec_header_info_t));
    ESP_GOTO_ON_ERROR((jpeg_io && header_info) ? ESP_OK : ESP_ERR_NO_MEM, cleanup, TAG, "alloc jpeg io/header failed");

    jpeg_io->inbuf = (unsigned char *)in;
    jpeg_io->inbuf_len = insize;
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, header_info);
    ESP_GOTO_ON_ERROR((ret == JPEG_ERR_OK) ? ESP_OK : ESP_FAIL, cleanup, TAG, "parse jpeg header failed");
    *w = header_info->width;
    *h = header_info->height;
    if (jpeg_dec_out) {
        *jpeg_dec_out = jpeg_dec;
    } else {
        jpeg_dec_close(jpeg_dec);
    }

    if (jpeg_io_out) {
        *jpeg_io_out = jpeg_io;
    } else {
        free(jpeg_io);
    }

    if (header_info_out) {
        *header_info_out = header_info;
    } else {
        free(header_info);
    }

    return LV_RESULT_OK;

cleanup:
    if (jpeg_io) {
        free(jpeg_io);
    }
    if (header_info) {
        free(header_info);
    }
    jpeg_dec_close(jpeg_dec);
    return LV_RESULT_INVALID;
}

static lv_draw_buf_t * jpeg_decode_jpg(lv_image_decoder_dsc_t * dsc)
{
    /* Decode a standard JPEG (file or memory). Optionally use HW acceleration. */
    uint8_t * src_data = NULL;
    uint32_t src_data_size = 0;
    lv_draw_buf_t * decoded = NULL;
    uint32_t width = 0;
    uint32_t height = 0;

    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_dec_io_t *jpeg_io = NULL;
    jpeg_dec_header_info_t *header_info = NULL;
    esp_err_t ret;

    if (dsc->src_type == LV_IMAGE_SRC_FILE) {
        const char* ext = lv_fs_get_ext(dsc->src);
        if (!ext || (lv_strcmp(ext, "jpg") != 0 && lv_strcmp(ext, "jpeg") != 0)) {
            return NULL;
        }
        src_data = alloc_file(dsc->src, &src_data_size);
        if (src_data == NULL) {
            return NULL;
        }
    } else if (dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
        const lv_image_dsc_t * img_dsc = dsc->src;
        src_data = (uint8_t *)img_dsc->data;
        src_data_size = img_dsc->data_size;
    } else {
        return NULL;
    }

    lv_color_format_t tgt = get_target_rgb_format();
    jpeg_pixel_format_t out_fmt = (tgt == LV_COLOR_FORMAT_RGB565) ? JPEG_PIXEL_FORMAT_RGB565_LE : JPEG_PIXEL_FORMAT_RGB888;
    ESP_GOTO_ON_ERROR((jpeg_decode_header(&width, &height, src_data, src_data_size, out_fmt, &jpeg_dec, &jpeg_io, &header_info) == LV_RESULT_OK) ? ESP_OK : ESP_FAIL,
                      cleanup, TAG, "decode JPEG header '%s' failed", (const char*)dsc->src);

    /* Decide whether we will use HW JPEG path before allocating output buffer */
#if ESP_LV_ENABLE_HW_JPEG
    bool use_hw = ((width % 16 == 0) && (height % 16 == 0) && (width >= 64) && (height >= 64));
#else
    bool use_hw = false;
#endif
    s_hw_output_allocation = use_hw;
    decoded = lv_draw_buf_create_ex(image_cache_draw_buf_handlers, width, height,
                                    (tgt == LV_COLOR_FORMAT_RGB565) ? LV_COLOR_FORMAT_RGB565 : LV_COLOR_FORMAT_RGB888,
                                    LV_STRIDE_AUTO);
    s_hw_output_allocation = false;
    ESP_GOTO_ON_ERROR(decoded ? ESP_OK : ESP_ERR_NO_MEM, cleanup, TAG, "alloc image buffer '%s' failed", (const char*)dsc->src);

#if ESP_LV_ENABLE_HW_JPEG
    /* Use HW JPEG only for sizes >= 64x64 and 16-pixel aligned. Smaller images fall back to SW */
    if (use_hw) {
        uint32_t out_size = 0;
        const jpeg_decode_cfg_t *cfg = (tgt == LV_COLOR_FORMAT_RGB565) ? &decode_cfg_rgb565 : &decode_cfg_rgb888;
        esp_err_t hw_ret = jpeg_decoder_process(jpgd_handle, cfg, src_data, src_data_size, decoded->data, decoded->data_size, &out_size);
        if (hw_ret != ESP_OK) {
            lv_draw_buf_destroy_user(image_cache_draw_buf_handlers, decoded);
            decoded = NULL;
            ESP_GOTO_ON_ERROR(hw_ret, cleanup, TAG, "HW JPEG decode '%s' failed", (const char*)dsc->src);
        }
    } else
#endif
    {
        jpeg_io->outbuf = decoded->data;
        if (jpeg_dec_process(jpeg_dec, jpeg_io) != JPEG_ERR_OK) {
            lv_draw_buf_destroy_user(image_cache_draw_buf_handlers, decoded);
            decoded = NULL;
            ESP_GOTO_ON_ERROR(ESP_FAIL, cleanup, TAG, "SW JPEG decode '%s' failed", (const char*)dsc->src);
        }
        if (tgt == LV_COLOR_FORMAT_RGB888) {
            uint8_t *p = jpeg_io->outbuf;
            for (int i = 0; i < width * height; i++) {
                uint8_t r = p[0];
                uint8_t b = p[2];
                p[0] = b;
                p[2] = r;
                p += 3;
            }
        }
    }

cleanup:
    if (dsc->src_type == LV_IMAGE_SRC_FILE && src_data) {
        free(src_data);
    }
    if (jpeg_io) {
        free(jpeg_io);
    }
    if (header_info) {
        free(header_info);
    }
    if (jpeg_dec) {
        jpeg_dec_close(jpeg_dec);
    }
    (void)ret;
    return decoded;
}

static lv_draw_buf_t * png_decode_rgba(lv_image_decoder_dsc_t * dsc)
{
    /* Decode a PNG from file or memory into ARGB8888 draw buffer. */
    uint8_t * src_data = NULL;
    uint32_t src_data_size = 0;
    lv_draw_buf_t * decoded = NULL;
    esp_err_t ret;

    if (dsc->src_type == LV_IMAGE_SRC_FILE) {
        const char* ext = lv_fs_get_ext(dsc->src);
        if (!ext || lv_strcmp(ext, "png") != 0) {
            return NULL;
        }
        src_data = alloc_file(dsc->src, &src_data_size);
        if (!src_data) {
            return NULL;
        }
    } else if (dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
        const lv_image_dsc_t * img_dsc = dsc->src;
        src_data = (uint8_t *)img_dsc->data;
        src_data_size = img_dsc->data_size;
    } else {
        return NULL;
    }

    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    ESP_GOTO_ON_ERROR(png_image_begin_read_from_memory(&image, src_data, src_data_size) ? ESP_OK : ESP_FAIL,
                      cleanup, TAG, "png begin read failed");
    image.format = PNG_FORMAT_BGRA;

    uint32_t width = image.width;
    uint32_t height = image.height;
    lv_draw_buf_t * buf = lv_draw_buf_create_ex(image_cache_draw_buf_handlers, width, height, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
    ESP_GOTO_ON_ERROR(buf ? ESP_OK : ESP_ERR_NO_MEM, cleanup_img, TAG, "alloc png buf failed");

    ESP_GOTO_ON_ERROR(png_image_finish_read(&image, NULL, buf->data, 0, NULL) ? ESP_OK : ESP_FAIL,
                      cleanup_buf_img, TAG, "png finish read failed");

    png_image_free(&image);
    if (dsc->src_type == LV_IMAGE_SRC_FILE && src_data) {
        free(src_data);
    }
    decoded = buf;
    (void)ret;
    return decoded;

cleanup_buf_img:
    lv_draw_buf_destroy_user(image_cache_draw_buf_handlers, buf);
cleanup_img:
    png_image_free(&image);
cleanup:
    if (dsc->src_type == LV_IMAGE_SRC_FILE && src_data) {
        free(src_data);
    }
    return NULL;
}

static lv_draw_buf_t * qoi_decode_qoi(lv_image_decoder_dsc_t * dsc)
{
    /* Decode a QOI image (RGB or RGBA) and adapt channel order for LVGL. */
    uint8_t * src_data = NULL;
    uint32_t src_data_size = 0;
    lv_draw_buf_t * buf = NULL;
    qoi_desc desc;
    memset(&desc, 0, sizeof(desc));
    esp_err_t ret;

    if (dsc->src_type == LV_IMAGE_SRC_FILE) {
        const char* ext = lv_fs_get_ext(dsc->src);
        if (!ext || lv_strcmp(ext, "qoi") != 0) {
            return NULL;
        }
        src_data = alloc_file(dsc->src, &src_data_size);
        if (!src_data) {
            return NULL;
        }
    } else if (dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
        const lv_image_dsc_t * img_dsc = dsc->src;
        src_data = (uint8_t *)img_dsc->data;
        src_data_size = img_dsc->data_size;
    } else {
        return NULL;
    }

    unsigned char *pixels = qoi_decode(src_data, src_data_size, &desc, 0);
    ESP_GOTO_ON_ERROR(pixels ? ESP_OK : ESP_FAIL, cleanup, TAG, "qoi decode failed");

    if (desc.channels == 4) {
        buf = lv_draw_buf_create_ex(image_cache_draw_buf_handlers, desc.width, desc.height, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
        ESP_GOTO_ON_ERROR(buf ? ESP_OK : ESP_ERR_NO_MEM, cleanup_pixels, TAG, "alloc qoi buf failed");
        uint8_t *dst = (uint8_t *)buf->data;
        uint8_t *src = (uint8_t *)pixels;
        for (uint32_t i = 0; i < desc.width * desc.height; i++) {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
            src += 4;
            dst += 4;
        }

    } else if (desc.channels == 3) {
        buf = lv_draw_buf_create_ex(image_cache_draw_buf_handlers, desc.width, desc.height, LV_COLOR_FORMAT_RGB888, LV_STRIDE_AUTO);
        ESP_GOTO_ON_ERROR(buf ? ESP_OK : ESP_ERR_NO_MEM, cleanup_pixels, TAG, "alloc qoi buf failed");
        uint8_t *dst = (uint8_t *)buf->data;
        uint8_t *src = (uint8_t *)pixels;
        for (uint32_t i = 0; i < desc.width * desc.height; i++) {
            uint8_t r = src[0];
            uint8_t g = src[1];
            uint8_t b = src[2];
            dst[0] = b;
            dst[1] = g;
            dst[2] = r;
            src += 3;
            dst += 3;
        }

    } else {
        free(pixels);
        if (dsc->src_type == LV_IMAGE_SRC_FILE && src_data) {
            free(src_data);
        }
        return NULL;
    }

    free(pixels);
    if (dsc->src_type == LV_IMAGE_SRC_FILE && src_data) {
        free(src_data);
    }
    (void)ret;
    return buf;

cleanup_pixels:
    free(pixels);
cleanup:
    if (dsc->src_type == LV_IMAGE_SRC_FILE && src_data) {
        free(src_data);
    }
    return NULL;
}

#if ESP_LV_ENABLE_PJPG

static esp_err_t safe_cache_sync(void *addr, size_t size, int flags)
{
    /*
     * Align the cache sync range to cache line boundaries. This wrapper keeps
     * the call sites concise and centralizes the alignment logic.
     */
    if (!addr || size == 0) {
        return ESP_OK;
    }

    const size_t CACHE_LINE_SIZE = 128;
    uintptr_t start_addr = (uintptr_t)addr;
    uintptr_t aligned_start = start_addr & ~(CACHE_LINE_SIZE - 1);
    size_t end_addr = start_addr + size;
    size_t aligned_end = (end_addr + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
    size_t aligned_size = aligned_end - aligned_start;

    esp_err_t ret = esp_cache_msync((void*)aligned_start, aligned_size, flags);
    if (ret != ESP_OK) { }
    return ESP_OK;
}

static esp_err_t pjpg_parse_header(const uint8_t *buf, size_t len, pjpg_info_t *info)
{
    if (len < PJPG_HEADER_SIZE || memcmp(buf, PJPG_MAGIC, PJPG_MAGIC_LEN) != 0) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t *p = buf + PJPG_MAGIC_LEN + 1;
    info->width = *(uint16_t *)p;
    p += 2;
    info->height = *(uint16_t *)p;
    p += 2;
    info->rgb_jpeg_size = *(uint32_t *)p;
    p += 4;
    info->alpha_jpeg_size = *(uint32_t *)p;

    return ESP_OK;
}

static esp_err_t load_source_data(lv_image_decoder_dsc_t * dsc, uint8_t **data, uint32_t *size)
{
    if (dsc->src_type == LV_IMAGE_SRC_FILE) {
        lv_fs_file_t temp_file;
        lv_fs_res_t open_res = lv_fs_open(&temp_file, dsc->src, LV_FS_MODE_RD);

        if (open_res != LV_FS_RES_OK) {
            ESP_LOGE(TAG, "Failed to open file: %s", (const char*)dsc->src);
            return ESP_ERR_INVALID_STATE;
        }

        uint32_t file_size, read_size;
        lv_fs_seek(&temp_file, 0, LV_FS_SEEK_END);
        lv_fs_tell(&temp_file, &file_size);
        lv_fs_seek(&temp_file, 0, LV_FS_SEEK_SET);

        if (file_size == 0) {
            ESP_LOGE(TAG, "File size is 0");
            lv_fs_close(&temp_file);
            return ESP_ERR_INVALID_SIZE;
        }

        *data = malloc(file_size);
        if (!*data) {
            ESP_LOGE(TAG, "Failed to allocate %lu bytes", file_size);
            lv_fs_close(&temp_file);
            return ESP_ERR_NO_MEM;
        }

        lv_fs_res_t read_res = lv_fs_read(&temp_file, *data, file_size, &read_size);
        lv_fs_close(&temp_file);

        if (read_res != LV_FS_RES_OK || read_size != file_size) {
            ESP_LOGE(TAG, "Failed to read file: expected %lu bytes, got %lu", file_size, read_size);
            free(*data);
            *data = NULL;
            return ESP_ERR_INVALID_SIZE;
        }

        *size = file_size;

    } else if (dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
        const lv_image_dsc_t * img_dsc = dsc->src;
        *data = (uint8_t *)img_dsc->data;
        *size = img_dsc->data_size;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static lv_draw_buf_t * pjpg_decode_jpg(lv_image_decoder_dsc_t * dsc)
{
    uint8_t *src_data = NULL;
    uint32_t src_size = 0;
    lv_draw_buf_t * decoded = NULL;
    bool need_free_src = false;
    esp_err_t ret;

    // Acquire mutex to protect PJPG global buffers (s_pjpg_rx_rgb/s_pjpg_rx_a) from concurrent access
    if (s_pjpg_mutex) {
        if (xSemaphoreTake(s_pjpg_mutex, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire PJPG mutex");
            return NULL;
        }
    }

    esp_err_t load_ret = load_source_data(dsc, &src_data, &src_size);
    ESP_GOTO_ON_ERROR(load_ret, cleanup, TAG, "load PJPG source data failed");
    need_free_src = (dsc->src_type == LV_IMAGE_SRC_FILE);

    pjpg_info_t info;
    ESP_GOTO_ON_ERROR(pjpg_parse_header(src_data, src_size, &info), cleanup, TAG, "parse PJPG header failed");

    size_t expected_min_size = PJPG_HEADER_SIZE + info.rgb_jpeg_size + info.alpha_jpeg_size;
    ESP_GOTO_ON_FALSE(src_size >= expected_min_size, ESP_ERR_INVALID_SIZE, cleanup, TAG, "PJPG too small: need %zu got %lu", expected_min_size, src_size);

    decoded = lv_draw_buf_create_ex(image_cache_draw_buf_handlers,
                                    info.width, info.height,
                                    LV_COLOR_FORMAT_RGB565A8,
                                    LV_STRIDE_AUTO);
    ESP_GOTO_ON_FALSE(decoded, ESP_ERR_NO_MEM, cleanup, TAG, "alloc PJPG decode buffer failed");

    uint8_t *rgb_data = src_data + PJPG_HEADER_SIZE;
    uint16_t *rgb565_ptr = (uint16_t *)decoded->data;
    size_t rgb_output_size = info.width * info.height * 2;
    jpeg_decode_cfg_t rgb_cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
    };
    safe_cache_sync(rgb_data, info.rgb_jpeg_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    uint32_t aligned_w = (info.width + 15) & ~15;
    uint32_t aligned_h = (info.height + 15) & ~15;

    uint32_t rgb_out_size = 0;
    if (aligned_w == info.width && aligned_h == info.height) {
        safe_cache_sync((void*)rgb565_ptr, rgb_output_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
        esp_err_t rgb_ret = jpeg_decoder_process(jpgd_handle, &rgb_cfg,
                                                 rgb_data, info.rgb_jpeg_size,
                                                 (uint8_t *)rgb565_ptr, rgb_output_size,
                                                 &rgb_out_size);
        ESP_GOTO_ON_ERROR(rgb_ret, cleanup, TAG, "RGB HW decode failed");
        safe_cache_sync((void*)rgb565_ptr, rgb_output_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    } else {
        size_t rgb_aligned_output_size = (size_t)aligned_w * aligned_h * 2;
        jpeg_decode_memory_alloc_cfg_t rx_mem_cfg = { .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER };
        size_t rx_buf_size = 0;
        if (s_pjpg_rx_rgb_size < rgb_aligned_output_size) {
            // Free old buffer before allocating new one
            if (s_pjpg_rx_rgb) {
                jpeg_free_align(s_pjpg_rx_rgb);
                s_pjpg_rx_rgb = NULL;
                s_pjpg_rx_rgb_size = 0;
            }
            s_pjpg_rx_rgb = jpeg_alloc_decoder_mem(rgb_aligned_output_size, &rx_mem_cfg, &rx_buf_size);
            ESP_GOTO_ON_FALSE(s_pjpg_rx_rgb, ESP_ERR_NO_MEM, cleanup, TAG, "Failed to alloc PJPG RGB buffer");
            s_pjpg_rx_rgb_size = rx_buf_size;
        }
        esp_err_t rgb_ret = jpeg_decoder_process(jpgd_handle, &rgb_cfg,
                                                 rgb_data, info.rgb_jpeg_size,
                                                 s_pjpg_rx_rgb, s_pjpg_rx_rgb_size,
                                                 &rgb_out_size);
        ESP_GOTO_ON_ERROR(rgb_ret, cleanup, TAG, "RGB HW decode failed");
        for (uint32_t y = 0; y < info.height; y++) {
            memcpy((uint8_t *)rgb565_ptr + y * info.width * 2, s_pjpg_rx_rgb + y * aligned_w * 2, info.width * 2);
        }
        safe_cache_sync((void*)rgb565_ptr, rgb_output_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    }

    // Alpha channel decoding (if exists)
    if (info.alpha_jpeg_size > 0) {
        uint8_t *alpha_data = src_data + PJPG_HEADER_SIZE + info.rgb_jpeg_size;
        uint8_t *alpha_ptr = (uint8_t *)decoded->data + rgb_output_size;
        size_t alpha_output_size = info.width * info.height;
        jpeg_decode_cfg_t alpha_cfg = { .output_format = JPEG_DECODE_OUT_FORMAT_GRAY };
        safe_cache_sync(alpha_data, info.alpha_jpeg_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
        if (aligned_w == info.width && aligned_h == info.height) {
            safe_cache_sync(alpha_ptr, alpha_output_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_INVALIDATE);

            uint32_t alpha_out_size = 0;
            esp_err_t alpha_ret = jpeg_decoder_process(jpgd_handle, &alpha_cfg,
                                                       alpha_data, info.alpha_jpeg_size,
                                                       alpha_ptr, alpha_output_size,
                                                       &alpha_out_size);
            ESP_GOTO_ON_ERROR(alpha_ret, cleanup, TAG, "Alpha HW decode failed");
            safe_cache_sync(alpha_ptr, alpha_output_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        } else {

            uint32_t alpha_out_size = 0;
            size_t alpha_aligned_output_size = (size_t)aligned_w * aligned_h;
            jpeg_decode_memory_alloc_cfg_t rx_mem_cfg = { .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER };
            size_t rx_a_buf_size = 0;
            if (s_pjpg_rx_a_size < alpha_aligned_output_size) {
                // Free old buffer before allocating new one
                if (s_pjpg_rx_a) {
                    jpeg_free_align(s_pjpg_rx_a);
                    s_pjpg_rx_a = NULL;
                    s_pjpg_rx_a_size = 0;
                }
                s_pjpg_rx_a = jpeg_alloc_decoder_mem(alpha_aligned_output_size, &rx_mem_cfg, &rx_a_buf_size);
                ESP_GOTO_ON_FALSE(s_pjpg_rx_a, ESP_ERR_NO_MEM, cleanup, TAG, "Failed to alloc PJPG Alpha buffer");
                s_pjpg_rx_a_size = rx_a_buf_size;
            }
            esp_err_t alpha_ret = jpeg_decoder_process(jpgd_handle, &alpha_cfg,
                                                       alpha_data, info.alpha_jpeg_size,
                                                       s_pjpg_rx_a, s_pjpg_rx_a_size,
                                                       &alpha_out_size);
            ESP_GOTO_ON_ERROR(alpha_ret, cleanup, TAG, "Alpha HW decode failed");
            for (uint32_t y = 0; y < info.height; y++) {
                memcpy(alpha_ptr + y * info.width, s_pjpg_rx_a + y * aligned_w, info.width);
            }
            safe_cache_sync(alpha_ptr, alpha_output_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        }
    } else {
        uint8_t *alpha_ptr = (uint8_t *)decoded->data + rgb_output_size;
        memset(alpha_ptr, 255, info.width * info.height);
        safe_cache_sync(alpha_ptr, info.width * info.height, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    }

    safe_cache_sync(decoded->data, decoded->data_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    if (need_free_src && src_data) {
        free(src_data);
    }

    // Release mutex before returning
    if (s_pjpg_mutex) {
        xSemaphoreGive(s_pjpg_mutex);
    }

    return decoded;

cleanup:
    if (decoded) {
        lv_draw_buf_destroy_user(image_cache_draw_buf_handlers, decoded);
    }
    if (need_free_src && src_data) {
        free(src_data);
    }

    // Release mutex before returning
    if (s_pjpg_mutex) {
        xSemaphoreGive(s_pjpg_mutex);
    }

    (void)ret;
    return NULL;
}
#endif

static void * buf_malloc(size_t size_bytes, lv_color_format_t color_format)
{
    const size_t CACHE_LINE_SIZE = 128;
    size_t aligned_size = (size_bytes + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
    void *buf = NULL;

#if ESP_LV_ENABLE_HW_JPEG
    /* Only prefer HW JPEG dedicated memory when allocating output for HW decode */
    if (s_hw_output_allocation) {
        jpeg_decode_memory_alloc_cfg_t mem_cfg = {
            .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
        };
        size_t buffer_size = 0;
        buf = jpeg_alloc_decoder_mem(aligned_size, &mem_cfg, &buffer_size);
        if (buf) {
#if ESP_LV_ENABLE_HW_JPEG
            hw_buf_register(buf);
#endif
            return buf;
        }
    }
#endif

    /* Default: allocate from default heap (typically internal RAM) */
    buf = heap_caps_aligned_alloc(CACHE_LINE_SIZE, aligned_size, MALLOC_CAP_DEFAULT);
    if (buf) {
        memset(buf, 0, aligned_size);
        return buf;
    }

    /* Fallback: try PSRAM if available (large frames) */
    buf = heap_caps_aligned_alloc(CACHE_LINE_SIZE, aligned_size, MALLOC_CAP_SPIRAM);
    if (buf) {
        memset(buf, 0, aligned_size);
        return buf;
    }

    ESP_LOGE(TAG, "128-byte aligned allocation failed for size: %zu", aligned_size);
    return NULL;
}

static void buf_free(void * buf)
{
    if (buf) {
        /* If allocated from HW JPEG dedicated memory, free accordingly */
#if ESP_LV_ENABLE_HW_JPEG
        if (hw_buf_unregister_and_free(buf)) {
            return;
        }
#endif
        free(buf);
    }
}

static void * buf_align(void * buf, lv_color_format_t color_format)
{
    LV_UNUSED(color_format);

    if (!buf) {
        return NULL;
    }

    const size_t CACHE_LINE_SIZE = 128;
    uint8_t * buf_u8 = (uint8_t *)((uintptr_t)(buf + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1));

    return buf_u8;
}

static uint32_t width_to_stride(uint32_t w, lv_color_format_t color_format)
{
    uint32_t width_byte;
    width_byte = w * lv_color_format_get_bpp(color_format);
    width_byte = (width_byte + 7) >> 3;
    return LV_ROUND_UP(width_byte, LV_DRAW_BUF_STRIDE_ALIGN);
}

static inline uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline bool sp_is_sp_tag(const uint8_t *hdr8, bool *is_sjpg)
{
    if (memcmp(hdr8, SP_TAG_SPNG, SP_TAG_LEN) == 0) {
        if (is_sjpg) {
            *is_sjpg = false;
        }
        return true;
    }
    if (memcmp(hdr8, SP_TAG_SJPG, SP_TAG_LEN) == 0) {
        if (is_sjpg) {
            *is_sjpg = true;
        }
        return true;
    }
    return false;
}

static inline void sp_read_meta(const uint8_t *meta8, uint16_t *w, uint16_t *h, uint16_t *splits, uint16_t *split_h)
{
    if (w) {
        *w = (uint16_t)read_le16(meta8 + 0);
    }
    if (h) {
        *h = (uint16_t)read_le16(meta8 + 2);
    }
    if (splits) {
        *splits = (uint16_t)read_le16(meta8 + 4);
    }
    if (split_h) {
        *split_h = (uint16_t)read_le16(meta8 + 6);
    }
}
