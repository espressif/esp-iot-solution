/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_lv_decoder.h"
#include "esp_lv_decoder_config.h"

#include "lvgl.h"

#include "png.h"
#include "esp_jpeg_dec.h"
#define QOI_IMPLEMENTATION
#include "qoi.h"

#if ESP_LV_ENABLE_HW_JPEG
#include "driver/jpeg_decode.h"
#include "esp_jpeg_common.h"
#endif

#include "esp_cache.h"

#if ESP_LV_ENABLE_PJPG
#define PJPG_MAGIC "_PJPG__"
#define PJPG_MAGIC_LEN 7
#define PJPG_HEADER_SIZE 22
typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t rgb_jpeg_size;
    uint32_t alpha_jpeg_size;
} pjpg_info_t;
#endif

/*********************
 *      DEFINES
 *********************/

/* Shared SP image container tags and offsets */
#define SP_TAG_SPNG        "_SPNG__"
#define SP_TAG_SQOI        "_SQOI__"
#define SP_TAG_SJPG        "_SJPG__"
#define SP_TAG_LEN         7
#define SP_META_OFFSET     14
#define SP_FILE_HEADER_SIZE 22
#if LV_COLOR_DEPTH == 32
#define DEC_RGBA_BPP 4
#define DEC_RGB_BPP 3
#define DEC_JPEG_SW_PIXEL_FORMAT JPEG_PIXEL_FORMAT_RGB888
#define DEC_JPEG_HW_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB888
#define DEC_JPEG_HW_RGB_ORDER JPEG_DEC_RGB_ELEMENT_ORDER_RGB
#elif LV_COLOR_DEPTH == 16
#define DEC_RGBA_BPP 3
#define DEC_RGB_BPP 2
#if  LV_BIG_ENDIAN_SYSTEM == 1 || LV_COLOR_16_SWAP == 1
#define DEC_JPEG_SW_PIXEL_FORMAT JPEG_PIXEL_FORMAT_RGB565_BE
#else
#define DEC_JPEG_SW_PIXEL_FORMAT JPEG_PIXEL_FORMAT_RGB565_LE
#endif
#define DEC_JPEG_HW_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB565
#define DEC_JPEG_HW_RGB_ORDER JPEG_DEC_RGB_ELEMENT_ORDER_BGR
#elif LV_COLOR_DEPTH == 8
#define DEC_RGBA_BPP 2
#define DEC_RGB_BPP 2
#elif LV_COLOR_DEPTH == 1
#define DEC_RGBA_BPP 2
#define DEC_RGB_BPP 2
#else
#error Unsupported LV_COLOR_DEPTH
#endif
/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_fs_file_t lv_file;
} io_source_t;

typedef struct {
    uint8_t *dsc_data;
    uint32_t dsc_size;
    int img_x_res;
    int img_y_res;
    int img_total_frames;
    int img_single_frame_height;
    int img_cache_frame_index;
    uint8_t **frame_base_array;        //to save base address of each split frames upto img_total_frames.
    int *frame_base_offset;            //to save base offset for fseek
    uint8_t *frame_cache;
    bool frame_cache_is_hw;            // whether frame_cache is allocated by jpeg_alloc_decoder_mem
    uint8_t img_depth;
    io_source_t io;
} image_decoder_t;

static lv_res_t decoder_info(struct _lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header);
static lv_res_t decoder_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc);
static lv_res_t decoder_read_line(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t len, uint8_t *buf);
static void decoder_close(lv_img_decoder_t *dec, lv_img_decoder_dsc_t *dsc);
static void convert_color_depth(uint8_t *img, uint32_t px_cnt);
static int is_png(const uint8_t *raw_data, size_t len);
static int is_qoi(const uint8_t *raw_data, size_t len);
static int is_jpg(const uint8_t *raw_data, size_t len);
static void decoder_cleanup(image_decoder_t *img_dec);
static void decoder_free(image_decoder_t *img_dec);

static lv_res_t libpng_decode32(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, size_t insize);
static lv_res_t qoi_decode32(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, size_t insize);
static lv_res_t jpeg_decode(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, uint32_t insize);
static lv_res_t jpeg_decode_header(uint32_t *w, uint32_t *h, const uint8_t *in, uint32_t insize);
static lv_res_t jpeg_decode_hw_or_sw(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, uint32_t insize, bool *out_is_hw);
static int is_pjpg(const uint8_t *raw_data, size_t len);
#if ESP_LV_ENABLE_HW_JPEG
static esp_err_t safe_cache_sync(void *addr, size_t size, int flags);
#endif
#if ESP_LV_ENABLE_PJPG
static lv_res_t pjpg_parse_header(const uint8_t *buf, size_t len, pjpg_info_t *info);
static lv_res_t pjpg_decode(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, uint32_t insize);
#endif

static const char *TAG = "lv_decoder_v8";

#if ESP_LV_ENABLE_HW_JPEG
static jpeg_decoder_handle_t jpgd_handle;
#endif

#if ESP_LV_ENABLE_PJPG
static uint8_t *s_pjpg_rx_rgb = NULL;
static size_t s_pjpg_rx_rgb_size = 0;
static uint8_t *s_pjpg_rx_a = NULL;
static size_t s_pjpg_rx_a_size = 0;
static SemaphoreHandle_t s_pjpg_mutex = NULL;
#endif

static inline uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline int sp_is_sp_tag(const uint8_t *hdr8)
{
    return (!strncmp((const char *)hdr8, SP_TAG_SPNG, SP_TAG_LEN) ||
            !strncmp((const char *)hdr8, SP_TAG_SQOI, SP_TAG_LEN) ||
            !strncmp((const char *)hdr8, SP_TAG_SJPG, SP_TAG_LEN));
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

esp_err_t esp_lv_decoder_init(esp_lv_decoder_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(ret_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    lv_img_decoder_t *dec = lv_img_decoder_create();
    ESP_RETURN_ON_FALSE(dec, ESP_ERR_NO_MEM, TAG, "failed to create LVGL decoder");
    lv_img_decoder_set_info_cb(dec, decoder_info);
    lv_img_decoder_set_open_cb(dec, decoder_open);
    lv_img_decoder_set_close_cb(dec, decoder_close);
    lv_img_decoder_set_read_line_cb(dec, decoder_read_line);

#if ESP_LV_ENABLE_PJPG
    if (!s_pjpg_mutex) {
        s_pjpg_mutex = xSemaphoreCreateMutex();
        if (!s_pjpg_mutex) {
            lv_img_decoder_delete(dec);
            return ESP_ERR_NO_MEM;
        }
    }
#endif

    *ret_handle = dec;
#if ESP_LV_ENABLE_HW_JPEG
    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .timeout_ms = 200,
    };
    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&decode_eng_cfg, &jpgd_handle));
#endif
    return ESP_OK;
}

esp_err_t esp_lv_decoder_deinit(esp_lv_decoder_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid decoder handle pointer");

    lv_img_decoder_delete(handle);

#if ESP_LV_ENABLE_HW_JPEG
    if (jpgd_handle) {
        ESP_ERROR_CHECK(jpeg_del_decoder_engine(jpgd_handle));
        jpgd_handle = NULL;
    }
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

static lv_res_t libpng_decode32(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, size_t insize)
{
    if (!in || !out || !w || !h) {
        return LV_RES_INV;
    }

    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_memory(&image, in, insize)) {
        return LV_RES_INV;
    }

    image.format = PNG_FORMAT_RGBA;

    *w = image.width;
    *h = image.height;
    *out = malloc(PNG_IMAGE_SIZE(image));
    if (*out == NULL) {
        png_image_free(&image);
        return LV_RES_INV;
    }

    if (!png_image_finish_read(&image, NULL, *out, 0, NULL)) {
        free(*out);
        png_image_free(&image);
        return LV_RES_INV;
    }

    return LV_RES_OK;
}

static lv_res_t qoi_decode32(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, size_t insize)
{
    if (!in || !out || !w || !h) {
        return LV_RES_INV;
    }

    qoi_desc image;
    memset(&image, 0, sizeof(image));

    unsigned char *pixels = qoi_decode(in, insize, &image, 0);

    *w = image.width;
    *h = image.height;
    *out = pixels;
    if (*out == NULL) {
        return LV_RES_INV;
    }
    return LV_RES_OK;
}

static lv_res_t jpeg_decode(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, uint32_t insize)
{
    jpeg_error_t ret;
    jpeg_dec_config_t config = {
        .output_type = DEC_JPEG_SW_PIXEL_FORMAT,
        .rotate = JPEG_ROTATE_0D,
    };

    jpeg_dec_handle_t jpeg_dec;
    jpeg_dec_open(&config, &jpeg_dec);
    ESP_GOTO_ON_ERROR(jpeg_dec ? ESP_OK : ESP_FAIL, cleanup0, TAG, "open jpeg decoder failed");

    jpeg_dec_io_t *jpeg_io = malloc(sizeof(jpeg_dec_io_t));
    jpeg_dec_header_info_t *out_info = malloc(sizeof(jpeg_dec_header_info_t));
    ESP_GOTO_ON_ERROR((jpeg_io && out_info) ? ESP_OK : ESP_ERR_NO_MEM, cleanup1, TAG, "alloc jpeg io/header failed");

    jpeg_io->inbuf = (unsigned char *)in;
    jpeg_io->inbuf_len = insize;

    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret == JPEG_ERR_OK) {

        *w = out_info->width;
        *h = out_info->height;
        if (out) {
            *out = (uint8_t *)heap_caps_aligned_alloc(16, out_info->height * out_info->width * DEC_RGB_BPP, MALLOC_CAP_DEFAULT);
            ESP_GOTO_ON_ERROR(*out ? ESP_OK : ESP_ERR_NO_MEM, cleanup2, TAG, "alloc jpeg out buffer failed");

            jpeg_io->outbuf = *out;
            ret = jpeg_dec_process(jpeg_dec, jpeg_io);
            ESP_GOTO_ON_ERROR((ret == JPEG_ERR_OK) ? ESP_OK : ESP_FAIL, cleanup3, TAG, "jpeg process failed[%d]", ret);
        }
    } else {
        ESP_GOTO_ON_ERROR(ESP_FAIL, cleanup1, TAG, "parse jpeg header failed");
    }

    free(jpeg_io);
    free(out_info);
    jpeg_dec_close(jpeg_dec);

    return LV_RES_OK;

cleanup3:
    if (out && *out) {
        free(*out);
        *out = NULL;
    }
cleanup2:
    free(jpeg_io);
    free(out_info);
    jpeg_dec_close(jpeg_dec);
    return LV_RES_INV;

cleanup1:
    if (jpeg_io) {
        free(jpeg_io);
    }
    if (out_info) {
        free(out_info);
    }
    jpeg_dec_close(jpeg_dec);
    return LV_RES_INV;

cleanup0:
    return LV_RES_INV;
}

static lv_fs_res_t load_image_file(const char *filename, uint8_t **buffer, size_t *size, bool read_head)
{
    uint32_t len;
    lv_fs_file_t f;
    lv_fs_res_t res = lv_fs_open(&f, filename, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        ESP_LOGE(TAG, "Failed to open file %s", filename);
        return res;
    }

    lv_fs_seek(&f, 0, LV_FS_SEEK_END);
    lv_fs_tell(&f, &len);
    lv_fs_seek(&f, 0, LV_FS_SEEK_SET);

    if (read_head && len > 1024) {
        len = 1024;
    } else if (len <= 0) {
        lv_fs_close(&f);
        return LV_FS_RES_FS_ERR;
    }

    *buffer = malloc(len);
    if (!*buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for file %s", filename);
        lv_fs_close(&f);
        return LV_FS_RES_OUT_OF_MEM;
    }

    uint32_t rn = 0;
    res = lv_fs_read(&f, *buffer, len, &rn);
    lv_fs_close(&f);

    if (res != LV_FS_RES_OK || rn != len) {
        free(*buffer);
        *buffer = NULL;
        ESP_LOGE(TAG, "Failed to read file %s", filename);
        return LV_FS_RES_UNKNOWN;
    }
    *size = len;

    return LV_FS_RES_OK;
}

static lv_res_t decoder_info(struct _lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{
    (void) decoder; /*Unused*/
    lv_img_src_t src_type = lv_img_src_get_type(src);          /*Get the source type*/

    lv_res_t lv_ret = LV_RES_OK;

    if (src_type == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t *img_dsc = src;
        uint8_t *img_dsc_data = (uint8_t *)img_dsc->data;
        const uint32_t img_dsc_size = img_dsc->data_size;

        if (!strncmp((char *)img_dsc_data, SP_TAG_SPNG, SP_TAG_LEN) ||
                !strncmp((char *)img_dsc_data, SP_TAG_SQOI, SP_TAG_LEN) ||
                !strncmp((char *)img_dsc_data, SP_TAG_SJPG, SP_TAG_LEN)) {
            if (!strncmp((char *)img_dsc_data, SP_TAG_SJPG, SP_TAG_LEN)) {
                header->cf = LV_IMG_CF_RAW;
            } else {
                header->cf = LV_IMG_CF_RAW_ALPHA;
            }
            img_dsc_data += SP_META_OFFSET; //seek to res info ... refer spng format

            header->always_zero = 0;
            header->w = read_le16(img_dsc_data); img_dsc_data += 2;
            header->h = read_le16(img_dsc_data); img_dsc_data += 2;

            return lv_ret;
        } else if (is_pjpg(img_dsc_data, img_dsc_size) == true) {
#if ESP_LV_ENABLE_PJPG
            pjpg_info_t info;
            if (pjpg_parse_header(img_dsc_data, img_dsc_size, &info) != LV_RES_OK) {
                return LV_RES_INV;
            }
            header->always_zero = 0;
            header->cf = LV_IMG_CF_RGB565A8;
            header->w = info.width;
            header->h = info.height;
            return lv_ret;
#else
            return LV_RES_INV;
#endif
        } else if (is_png(img_dsc_data, img_dsc_size) == true) {
            const uint32_t *size = ((uint32_t *)img_dsc->data) + 4;

            header->always_zero = 0;

            if (img_dsc->header.cf) {
                header->cf = img_dsc->header.cf;       /*Save the color format*/
            } else {
                header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
            }

            if (img_dsc->header.w) {
                header->w = img_dsc->header.w;         /*Save the image width*/
            } else {
                header->w = (lv_coord_t)((size[0] & 0xff000000) >> 24) + ((size[0] & 0x00ff0000) >> 8);
            }

            if (img_dsc->header.h) {
                header->h = img_dsc->header.h;         /*Save the color height*/
            } else {
                header->h = (lv_coord_t)((size[1] & 0xff000000) >> 24) + ((size[1] & 0x00ff0000) >> 8);
            }

            return lv_ret;
        } else if (is_qoi(img_dsc_data, img_dsc_size) == true) {
            const uint8_t *size = ((uint8_t *)img_dsc->data) + 4;

            header->always_zero = 0;

            if (img_dsc->header.cf) {
                header->cf = img_dsc->header.cf;       /*Save the color format*/
            } else {
                header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
            }

            if (img_dsc->header.w) {
                header->w = img_dsc->header.w;         /*Save the image width*/
            } else {
                header->w = (lv_coord_t)((size[0] << 24) + (size[1] << 16) + (size[2] << 8) + (size[3] << 0));
            }

            if (img_dsc->header.h) {
                header->h = img_dsc->header.h;         /*Save the color height*/
            } else {
                header->h = (lv_coord_t)((size[4] << 24) + (size[5] << 16) + (size[6] << 8) + (size[7] << 0));
            }

            return lv_ret;
        } else if (is_jpg(img_dsc_data, img_dsc_size) == true) {
            uint32_t width, height;
            lv_ret = jpeg_decode_header(&width, &height, img_dsc_data, img_dsc_size);

            header->w = width;
            header->h = height;
            header->always_zero = 0;
            header->cf = LV_IMG_CF_RAW;

            return lv_ret;
        } else {
            return LV_RES_INV;
        }
    } else if (src_type == LV_IMG_SRC_FILE) {
        const char *fn = src;
        uint8_t *load_img_data = NULL;  /*Pointer to the loaded data. Same as the original file just loaded into the RAM*/
        size_t load_img_size;           /*Size of `load_img_data` in bytes*/

        if (!strcmp(lv_fs_get_ext(fn), "png")) {

            if (load_image_file(fn, &load_img_data, &load_img_size, true) != LV_FS_RES_OK) {
                if (load_img_data) {
                    free(load_img_data);
                }
                return LV_RES_INV;
            }

            const uint32_t *size = ((uint32_t *)load_img_data) + 4;
            header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
            header->w = (lv_coord_t)((size[0] & 0xff000000) >> 24) + ((size[0] & 0x00ff0000) >> 8);
            header->h = (lv_coord_t)((size[1] & 0xff000000) >> 24) + ((size[1] & 0x00ff0000) >> 8);
            free(load_img_data);
            return lv_ret;
        } else if (!strcmp(lv_fs_get_ext(fn), "qoi")) {

            if (load_image_file(fn, &load_img_data, &load_img_size, true) != LV_FS_RES_OK) {
                if (load_img_data) {
                    free(load_img_data);
                }
                return LV_RES_INV;
            }

            const uint8_t *size = ((uint8_t *)load_img_data) + 4;
            header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
            header->w = (lv_coord_t)((size[0] << 24) + (size[1] << 16) + (size[2] << 8) + (size[3] << 0));
            header->h = (lv_coord_t)((size[4] << 24) + (size[5] << 16) + (size[6] << 8) + (size[7] << 0));
            free(load_img_data);
            return lv_ret;
        } else if (strcmp(lv_fs_get_ext(fn), "jpg") == 0) {

            if (load_image_file(fn, &load_img_data, &load_img_size, true) != LV_FS_RES_OK) {
                if (load_img_data) {
                    free(load_img_data);
                }
                return LV_RES_INV;
            }

            uint32_t width, height;
            lv_ret = jpeg_decode_header(&width, &height, load_img_data, load_img_size);

            header->cf = LV_IMG_CF_TRUE_COLOR;
            header->w = width;
            header->h = height;
            free(load_img_data);
            return lv_ret;
        } else if (!strcmp(lv_fs_get_ext(fn), "pjpg")) {

            if (load_image_file(fn, &load_img_data, &load_img_size, true) != LV_FS_RES_OK) {
                if (load_img_data) {
                    free(load_img_data);
                }
                return LV_RES_INV;
            }

#if ESP_LV_ENABLE_PJPG
            pjpg_info_t info;
            if (pjpg_parse_header(load_img_data, load_img_size, &info) != LV_RES_OK) {
                free(load_img_data);
                return LV_RES_INV;
            }
            header->cf = LV_IMG_CF_RGB565A8;
            header->w = info.width;
            header->h = info.height;
            free(load_img_data);
            return lv_ret;
#else
            free(load_img_data);
            return LV_RES_INV;
#endif
        } else if (!strcmp(lv_fs_get_ext(fn), "spng") ||
                   !strcmp(lv_fs_get_ext(fn), "sqoi") ||
                   !strcmp(lv_fs_get_ext(fn), "sjpg")) {

            uint8_t buff[22];
            memset(buff, 0, sizeof(buff));

            lv_fs_file_t file;
            lv_fs_res_t res = lv_fs_open(&file, fn, LV_FS_MODE_RD);
            if (res != LV_FS_RES_OK) {
                return 78;
            }

            uint32_t rn;
            res = lv_fs_read(&file, buff, 8, &rn);
            if (res != LV_FS_RES_OK || rn != 8) {
                lv_fs_close(&file);
                return LV_RES_INV;
            }

            if (sp_is_sp_tag(buff)) {

                if (!strncmp((char *)buff, SP_TAG_SJPG, SP_TAG_LEN)) {
                    header->cf = LV_IMG_CF_TRUE_COLOR;
                } else {
                    header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
                }

                lv_fs_seek(&file, SP_META_OFFSET, LV_FS_SEEK_SET);
                res = lv_fs_read(&file, buff, 4, &rn);
                if (res != LV_FS_RES_OK || rn != 4) {
                    lv_fs_close(&file);
                    return LV_RES_INV;
                }
                uint8_t *data = buff;
                header->always_zero = 0;
                header->w = read_le16(data); data += 2;
                header->h = read_le16(data); data += 2;
                lv_fs_close(&file);
                return lv_ret;
            }
        } else {
            return LV_RES_INV;
        }
    }

    return LV_RES_INV;         /*If didn't succeeded earlier then it's an error*/
}

static lv_res_t decoder_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{

    (void) decoder; /*Unused*/
    lv_res_t lv_ret = LV_RES_OK;        /*For the return values of PNG decoder functions*/

    uint8_t *img_data = NULL;
    uint32_t img_width;
    uint32_t img_height;

    if (dsc->src_type == LV_IMG_SRC_VARIABLE) {

        const lv_img_dsc_t *img_dsc = dsc->src;

        uint8_t *data;
        image_decoder_t *img_dec = (image_decoder_t *) dsc->user_data;
        if (img_dec == NULL) {
            img_dec =  malloc(sizeof(image_decoder_t));
            if (!img_dec) {
                return LV_RES_INV;
            }

            memset(img_dec, 0, sizeof(image_decoder_t));

            dsc->user_data = img_dec;
            img_dec->dsc_data = (uint8_t *)((lv_img_dsc_t *)(dsc->src))->data;
            img_dec->dsc_size = ((lv_img_dsc_t *)(dsc->src))->data_size;
        }

        if (!strncmp((char *) img_dec->dsc_data, SP_TAG_SPNG, SP_TAG_LEN) ||
                !strncmp((char *) img_dec->dsc_data, SP_TAG_SQOI, SP_TAG_LEN) ||
                !strncmp((char *) img_dec->dsc_data, SP_TAG_SJPG, SP_TAG_LEN)) {
            data = img_dec->dsc_data;
            data += SP_META_OFFSET;

            img_dec->img_x_res = read_le16(data); data += 2;
            img_dec->img_y_res = read_le16(data); data += 2;
            img_dec->img_total_frames = read_le16(data); data += 2;
            img_dec->img_single_frame_height = read_le16(data); data += 2;

            img_dec->frame_base_array = malloc(sizeof(uint8_t *) * img_dec->img_total_frames);
            if (! img_dec->frame_base_array) {
                ESP_LOGE(TAG, "Not enough memory for frame_base_array allocation");
                decoder_cleanup(img_dec);
                img_dec = NULL;
                return LV_RES_INV;
            }

            uint8_t *img_frame_base = data + img_dec->img_total_frames * 2;
            img_dec->frame_base_array[0] = img_frame_base;

            for (int i = 1; i <  img_dec->img_total_frames; i++) {
                int offset = (int)read_le16(data); data += 2;
                img_dec->frame_base_array[i] = img_dec->frame_base_array[i - 1] + offset;
            }
            img_dec->img_cache_frame_index = -1;
            // Allocate frame cache with maximum possible size to prevent buffer overflow
            size_t frame_cache_size = img_dec->img_x_res * img_dec->img_single_frame_height * DEC_RGBA_BPP;
            img_dec->frame_cache = (void *)malloc(frame_cache_size);
            if (! img_dec->frame_cache) {
                ESP_LOGE(TAG, "Not enough memory for frame_cache allocation (size: %zu)", frame_cache_size);
                decoder_cleanup(img_dec);
                img_dec = NULL;
                return LV_RES_INV;
            }
            dsc->img_data = NULL;

            return lv_ret;
        } else if (is_pjpg(img_dec->dsc_data, img_dec->dsc_size) == true) {
#if ESP_LV_ENABLE_PJPG
            uint32_t w, h;

            lv_ret = pjpg_decode(&img_data, &w, &h, img_dec->dsc_data, img_dec->dsc_size);

            if (lv_ret != LV_RES_OK) {
                if (img_data != NULL) {
                    free(img_data);
                }
                decoder_cleanup(img_dec);
                img_dec = NULL;
                return LV_RES_INV;
            } else {
                dsc->img_data = img_data;
                img_dec->frame_cache = img_data;
            }
            return lv_ret;
#else
            return LV_RES_INV;
#endif
        } else if (is_png(img_dec->dsc_data, img_dec->dsc_size) == true ||
                   is_qoi(img_dec->dsc_data, img_dec->dsc_size) == true ||
                   is_jpg(img_dec->dsc_data, img_dec->dsc_size) == true) {
            /*Decode the image in ARGB8888 */
            bool out_is_hw = false;
            if (is_png(img_dec->dsc_data, img_dec->dsc_size) == true) {
                lv_ret = libpng_decode32(&img_data, &img_width, &img_height, img_dsc->data, img_dsc->data_size);
            } else if (is_qoi(img_dec->dsc_data, img_dec->dsc_size) == true) {
                lv_ret = qoi_decode32(&img_data, &img_width, &img_height, img_dsc->data, img_dsc->data_size);
            } else if (is_jpg(img_dec->dsc_data, img_dec->dsc_size) == true) {
                lv_ret = jpeg_decode_hw_or_sw(&img_data, &img_width, &img_height, img_dsc->data, img_dsc->data_size, &out_is_hw);

            }
            if (lv_ret != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode error:%d", lv_ret);
                if (img_data != NULL) {
                    free(img_data);
                }
                decoder_cleanup(img_dec);
                img_dec = NULL;
                return LV_RES_INV;
            } else {
                if (is_jpg(img_dec->dsc_data, img_dec->dsc_size) == false) {
                    /*Convert the image to the system's color depth*/
                    convert_color_depth(img_data,  img_width * img_height);
                }
                dsc->img_data = img_data;
                img_dec->frame_cache = img_data;
                img_dec->frame_cache_is_hw = out_is_hw;
            }
            return lv_ret;
        } else {
            return LV_RES_INV;
        }
        return LV_RES_OK;     /*Return with its pointer*/
    } else if (dsc->src_type == LV_IMG_SRC_FILE) {
        const char *fn = dsc->src;

        if (!strcmp(lv_fs_get_ext(fn), "png") ||
                !strcmp(lv_fs_get_ext(fn), "qoi") ||
                !strcmp(lv_fs_get_ext(fn), "jpg") ||
                !strcmp(lv_fs_get_ext(fn), "pjpg")) {
            uint8_t *load_img_data = NULL;  /*Pointer to the loaded data. Same as the original file just loaded into the RAM*/
            size_t load_img_size;           /*Size of `load_img_data` in bytes*/

            image_decoder_t *img_dec = (image_decoder_t *) dsc->user_data;
            if (img_dec == NULL) {
                img_dec = malloc(sizeof(image_decoder_t));
                if (!img_dec) {
                    ESP_LOGE(TAG, "Failed to allocate memory for png");
                    return LV_RES_INV;
                }

                memset(img_dec, 0, sizeof(image_decoder_t));
                dsc->user_data = img_dec;
            }

            if (load_image_file(fn, &load_img_data, &load_img_size, false) != LV_FS_RES_OK) {
                if (load_img_data) {
                    free(load_img_data);
                    decoder_cleanup(img_dec);
                }
                return LV_RES_INV;
            }

            bool out_is_hw = false;
            if (!strcmp(lv_fs_get_ext(fn), "png")) {
                lv_ret = libpng_decode32(&img_data, &img_width, &img_height, load_img_data, load_img_size);
            } else if (!strcmp(lv_fs_get_ext(fn), "qoi")) {
                lv_ret = qoi_decode32(&img_data, &img_width, &img_height, load_img_data, load_img_size);
            } else if (!strcmp(lv_fs_get_ext(fn), "jpg")) {
                lv_ret = jpeg_decode_hw_or_sw(&img_data, &img_width, &img_height, load_img_data, load_img_size, &out_is_hw);

            } else if (!strcmp(lv_fs_get_ext(fn), "pjpg")) {
#if ESP_LV_ENABLE_PJPG

                lv_ret = pjpg_decode(&img_data, &img_width, &img_height, load_img_data, load_img_size);

#else
                lv_ret = LV_RES_INV;
#endif
            }
            free(load_img_data);
            if (lv_ret != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode error:%d", lv_ret);
                if (img_data != NULL) {
                    free(img_data);
                }
                decoder_cleanup(img_dec);
                img_dec = NULL;
                return LV_RES_INV;
            } else {
                if (strcmp(lv_fs_get_ext(fn), "jpg")) {
                    /*Convert the image to the system's color depth*/
                    convert_color_depth(img_data,  img_width * img_height);
                }
                dsc->img_data = img_data;
                img_dec->frame_cache = img_data;
                img_dec->frame_cache_is_hw = out_is_hw;
            }
            return lv_ret;
        } else if (!strcmp(lv_fs_get_ext(fn), "spng") ||
                   !strcmp(lv_fs_get_ext(fn), "sqoi") ||
                   !strcmp(lv_fs_get_ext(fn), "sjpg")) {
            uint8_t *data;
            uint8_t buff[22];
            memset(buff, 0, sizeof(buff));

            lv_fs_file_t lv_file;
            lv_fs_res_t res = lv_fs_open(&lv_file, fn, LV_FS_MODE_RD);
            if (res != LV_FS_RES_OK) {
                return 78;
            }

            uint32_t rn;
            res = lv_fs_read(&lv_file, buff, SP_FILE_HEADER_SIZE, &rn);
            if (res != LV_FS_RES_OK || rn != SP_FILE_HEADER_SIZE) {
                lv_fs_close(&lv_file);
                return LV_RES_INV;
            }

            if (sp_is_sp_tag(buff)) {

                image_decoder_t *img_dec = (image_decoder_t *)dsc->user_data;
                if (img_dec == NULL) {
                    img_dec = malloc(sizeof(image_decoder_t));
                    if (! img_dec) {
                        ESP_LOGE(TAG, "Failed to allocate memory for img_dec");
                        lv_fs_close(&lv_file);
                        return LV_RES_INV;
                    }
                    memset(img_dec, 0, sizeof(image_decoder_t));
                    dsc->user_data = img_dec;
                }
                data = buff;
                data += SP_META_OFFSET;

                img_dec->img_x_res = read_le16(data); data += 2;
                img_dec->img_y_res = read_le16(data); data += 2;
                img_dec->img_total_frames = read_le16(data); data += 2;
                img_dec->img_single_frame_height = read_le16(data); data += 2;

                img_dec->frame_base_offset = malloc(sizeof(int *) * img_dec->img_total_frames);
                if (! img_dec->frame_base_offset) {
                    ESP_LOGE(TAG, "Not enough memory for frame_base_offset allocation");
                    decoder_cleanup(img_dec);
                    img_dec = NULL;
                    return LV_RES_INV;
                }

                int img_frame_start_offset = SP_FILE_HEADER_SIZE + img_dec->img_total_frames * 2;
                img_dec->frame_base_offset[0] = img_frame_start_offset;

                for (int i = 1; i <  img_dec->img_total_frames; i++) {
                    res = lv_fs_read(&lv_file, buff, 2, &rn);
                    if (res != LV_FS_RES_OK || rn != 2) {
                        lv_fs_close(&lv_file);
                        return LV_RES_INV;
                    }

                    data = buff;
                    int offset = (int)read_le16(data); data += 2;
                    img_dec->frame_base_offset[i] = img_dec->frame_base_offset[i - 1] + offset;
                }

                img_dec->img_cache_frame_index = -1; //INVALID AT BEGINNING for a forced compare mismatch at first time.
                // Allocate frame cache with maximum possible size to prevent buffer overflow
                size_t frame_cache_size = img_dec->img_x_res * img_dec->img_single_frame_height * DEC_RGBA_BPP;
                img_dec->frame_cache = (void *)malloc(frame_cache_size);
                if (!img_dec->frame_cache) {
                    ESP_LOGE(TAG, "Not enough memory for frame_cache allocation (size: %zu)", frame_cache_size);
                    lv_fs_close(&lv_file);
                    decoder_cleanup(img_dec);
                    img_dec = NULL;
                    return LV_RES_INV;
                }

                uint32_t total_size;
                lv_fs_seek(&lv_file, 0, LV_FS_SEEK_END);
                lv_fs_tell(&lv_file, &total_size);
                img_dec->dsc_size = total_size;

                img_dec->io.lv_file = lv_file;
                dsc->img_data = NULL;
                return lv_ret;
            }
        } else {
            return LV_RES_INV;
        }
    }

    return LV_RES_INV;    /*If not returned earlier then it failed*/
}

static lv_res_t decoder_read_line(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t len, uint8_t *buf)
{
    LV_UNUSED(decoder);

    lv_res_t error = LV_RES_INV;
    uint8_t *img_data = NULL;

    uint8_t RGBA_depth = DEC_RGBA_BPP;
    uint8_t RGB_depth = DEC_RGB_BPP;

    uint32_t img_width;
    uint32_t img_height;

    uint8_t *img_block_data;
    uint32_t img_block_size;

    (void)RGBA_depth;
    (void)RGB_depth;

    if (dsc->src_type == LV_IMG_SRC_FILE) {
        uint32_t rn = 0;
        lv_fs_res_t res;

        image_decoder_t *img_dec = (image_decoder_t *) dsc->user_data;

        lv_fs_file_t *lv_file_p = &(img_dec->io.lv_file);
        if (!lv_file_p) {
            return LV_RES_INV;
        }

        int png_req_frame_index = y / img_dec->img_single_frame_height;
        /*If line not from cache, refresh cache */
        if (png_req_frame_index != img_dec->img_cache_frame_index) {

            if (png_req_frame_index == (img_dec->img_total_frames - 1)) {
                /*This is the last frame. */
                img_block_size = img_dec->dsc_size - (uint32_t)(img_dec->frame_base_offset[png_req_frame_index]);
            } else {
                img_block_size =
                    (uint32_t)(img_dec->frame_base_offset[png_req_frame_index + 1] - (uint32_t)(img_dec->frame_base_offset[png_req_frame_index]));
            }

            int next_read_pos = (int)(img_dec->frame_base_offset [ png_req_frame_index ]);
            lv_fs_seek(lv_file_p, next_read_pos, LV_FS_SEEK_SET);
            res = lv_fs_read(lv_file_p, img_dec->frame_cache, img_block_size, &rn);
            if (res != LV_FS_RES_OK || rn != img_block_size) {
                lv_fs_close(lv_file_p);
                return LV_RES_INV;
            }

            if (is_png(img_dec->frame_cache, rn) == true) {
                error = libpng_decode32(&img_data, &img_width, &img_height, img_dec->frame_cache, rn);
            } else if (is_qoi(img_dec->frame_cache, rn) == true) {
                error = qoi_decode32(&img_data, &img_width, &img_height, img_dec->frame_cache, rn);
            } else if (is_jpg(img_dec->frame_cache, rn) == true) {
                error = jpeg_decode(&img_data, &img_width, &img_height, img_dec->frame_cache, rn);
            }
            if (error != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode error:%d", error);
                if (img_data != NULL) {
                    free(img_data);
                }
                return LV_RES_INV;
            } else {
                if (is_jpg(img_dec->frame_cache, rn) == false) {
                    convert_color_depth(img_data,  img_width * img_height);
                    img_dec->img_depth = RGBA_depth;
                } else {
                    img_dec->img_depth = RGB_depth;
                }
                // Boundary check to prevent buffer overflow
                size_t copy_size = img_width * img_height * img_dec->img_depth;
                size_t max_size = img_dec->img_x_res * img_dec->img_single_frame_height * DEC_RGBA_BPP;
                if (copy_size > max_size) {
                    ESP_LOGE(TAG, "Buffer overflow prevented: copy_size=%zu > max_size=%zu", copy_size, max_size);
                    free(img_data);
                    return LV_RES_INV;
                }
                memcpy(img_dec->frame_cache, img_data, copy_size);
                if (img_data != NULL) {
                    free(img_data);
                }
            }
            img_dec->img_cache_frame_index = png_req_frame_index;
        }

        uint8_t *cache = (uint8_t *)img_dec->frame_cache + x * img_dec->img_depth + (y % img_dec->img_single_frame_height) * img_dec->img_x_res * img_dec->img_depth;
        memcpy(buf, cache, img_dec->img_depth * len);
        return LV_RES_OK;
    } else if (dsc->src_type == LV_IMG_SRC_VARIABLE) {
        image_decoder_t *img_dec = (image_decoder_t *) dsc->user_data;

        int spng_req_frame_index = y / img_dec->img_single_frame_height;

        /*If line not from cache, refresh cache */
        if (spng_req_frame_index != img_dec->img_cache_frame_index) {
            img_block_data = img_dec->frame_base_array[ spng_req_frame_index ];
            if (spng_req_frame_index == (img_dec->img_total_frames - 1)) {
                /*This is the last frame. */
                const uint32_t frame_offset = (uint32_t)(img_block_data - img_dec->dsc_data);
                img_block_size = img_dec->dsc_size - frame_offset;
            } else {
                img_block_size =
                    (uint32_t)(img_dec->frame_base_array[spng_req_frame_index + 1] - img_block_data);
            }

            if (is_png(img_block_data, img_block_size) == true) {
                error = libpng_decode32(&img_data, &img_width, &img_height, img_block_data, img_block_size);
            } else if (is_qoi(img_block_data, img_block_size) == true) {
                error = qoi_decode32(&img_data, &img_width, &img_height, img_block_data, img_block_size);
            } else if (is_jpg(img_block_data, img_block_size) == true) {
                error = jpeg_decode(&img_data, &img_width, &img_height, img_block_data, img_block_size);
            }
            if (error != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode error:%d", error);
                if (img_data != NULL) {
                    free(img_data);
                }
                return LV_RES_INV;
            } else {
                if (is_jpg(img_block_data, img_block_size) == false) {
                    convert_color_depth(img_data,  img_width * img_height);
                    img_dec->img_depth = RGBA_depth;
                } else {
                    img_dec->img_depth = RGB_depth;
                }
                // Boundary check to prevent buffer overflow
                size_t copy_size = img_dec->img_single_frame_height * img_dec->img_x_res * img_dec->img_depth;
                size_t max_size = img_dec->img_x_res * img_dec->img_single_frame_height * DEC_RGBA_BPP;
                if (copy_size > max_size) {
                    ESP_LOGE(TAG, "Buffer overflow prevented: copy_size=%zu > max_size=%zu", copy_size, max_size);
                    free(img_data);
                    return LV_RES_INV;
                }
                memcpy(img_dec->frame_cache, img_data, copy_size);
                if (img_data != NULL) {
                    free(img_data);
                }
            }
            img_dec->img_cache_frame_index = spng_req_frame_index;
        }

        uint8_t *cache = (uint8_t *)img_dec->frame_cache + x * img_dec->img_depth + (y % img_dec->img_single_frame_height) * img_dec->img_x_res * img_dec->img_depth;
        memcpy(buf, cache, img_dec->img_depth * len);
        return LV_RES_OK;
    }

    return LV_RES_INV;
}

static void decoder_close(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{
    LV_UNUSED(decoder);
    /*Free all allocated data*/
    image_decoder_t *img_dec = (image_decoder_t *) dsc->user_data;
    if (!img_dec) {
        return;
    }

    switch (dsc->src_type) {
    case LV_IMG_SRC_FILE:
        if (img_dec->io.lv_file.file_d) {
            lv_fs_close(&(img_dec->io.lv_file));
        }
        decoder_cleanup(img_dec);
        break;

    case LV_IMG_SRC_VARIABLE:
        decoder_cleanup(img_dec);
        break;

    default:
        ;
    }
}

static void convert_color_depth(uint8_t *img, uint32_t px_cnt)
{
    /*
     * Convert ARGB888/decoded data to the system's color depth.
     * The conversion paths below are identical to the original logic,
     * preserved to avoid any behavior change.
     */
#if LV_COLOR_DEPTH == 32
    lv_color32_t *img_argb = (lv_color32_t *)img;
    lv_color_t c;
    lv_color_t *img_c = (lv_color_t *) img;
    uint32_t i;
    for (i = 0; i < px_cnt; i++) {
        c = lv_color_make(img_argb[i].ch.red, img_argb[i].ch.green, img_argb[i].ch.blue);
        img_c[i].ch.red = c.ch.blue;
        img_c[i].ch.blue = c.ch.red;
    }
#elif LV_COLOR_DEPTH == 16
    lv_color32_t *img_argb = (lv_color32_t *)img;
    lv_color_t c;
    uint32_t i;
    for (i = 0; i < px_cnt; i++) {
        c = lv_color_make(img_argb[i].ch.blue, img_argb[i].ch.green, img_argb[i].ch.red);
        img[i * 3 + 2] = img_argb[i].ch.alpha;
        img[i * 3 + 1] = c.full >> 8;
        img[i * 3 + 0] = c.full & 0xFF;
    }
#elif LV_COLOR_DEPTH == 8
    lv_color32_t *img_argb = (lv_color32_t *)img;
    lv_color_t c;
    uint32_t i;
    for (i = 0; i < px_cnt; i++) {
        c = lv_color_make(img_argb[i].ch.red, img_argb[i].ch.green, img_argb[i].ch.blue);
        img[i * 2 + 1] = img_argb[i].ch.alpha;
        img[i * 2 + 0] = c.full;
    }
#elif LV_COLOR_DEPTH == 1
    lv_color32_t *img_argb = (lv_color32_t *)img;
    uint8_t b;
    uint32_t i;
    for (i = 0; i < px_cnt; i++) {
        b = img_argb[i].ch.red | img_argb[i].ch.green | img_argb[i].ch.blue;
        img[i * 2 + 1] = img_argb[i].ch.alpha;
        img[i * 2 + 0] = b > 128 ? 1 : 0;
    }
#endif
}

static int is_jpg(const uint8_t *raw_data, size_t len)
{
    const uint8_t jpg_signature_JFIF[] = {0xFF, 0xD8, 0xFF,  0xE0,  0x00,  0x10, 0x4A,  0x46, 0x49, 0x46};
    const uint8_t jpg_signature_Adobe[] = {0xFF, 0xD8, 0xFF,  0xEE,  0x00, 0x0E,  0x41, 0x64, 0x6F, 0x62};
    if (len < sizeof(jpg_signature_JFIF)) {
        return false;
    }
    return ((memcmp(jpg_signature_JFIF, raw_data, sizeof(jpg_signature_JFIF)) == 0) | (memcmp(jpg_signature_Adobe, raw_data, sizeof(jpg_signature_Adobe)) == 0));
}

static int is_png(const uint8_t *raw_data, size_t len)
{
    const uint8_t magic[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    if (len < sizeof(magic)) {
        return false;
    }
    return memcmp(magic, raw_data, sizeof(magic)) == 0;
}

static int is_qoi(const uint8_t *raw_data, size_t len)
{
    const uint8_t magic[] = {0x71, 0x6F, 0x69, 0x66};
    if (len < sizeof(magic)) {
        return false;
    }
    return memcmp(magic, raw_data, sizeof(magic)) == 0;
}

static lv_res_t jpeg_decode_header(uint32_t *w, uint32_t *h, const uint8_t *in, uint32_t insize)
{
    jpeg_error_t ret;
    jpeg_dec_config_t config = {
        .output_type = JPEG_PIXEL_FORMAT_RGB888,
        .rotate = JPEG_ROTATE_0D,
    };
    jpeg_dec_handle_t jpeg_dec;
    jpeg_dec_open(&config, &jpeg_dec);
    if (!jpeg_dec) {
        return LV_RES_INV;
    }
    jpeg_dec_io_t *jpeg_io = malloc(sizeof(jpeg_dec_io_t));
    jpeg_dec_header_info_t *out_info = malloc(sizeof(jpeg_dec_header_info_t));
    ESP_GOTO_ON_ERROR((jpeg_io && out_info) ? ESP_OK : ESP_ERR_NO_MEM, cleanup, TAG, "alloc jpeg header IO/info failed");
    jpeg_io->inbuf = (unsigned char *)in;
    jpeg_io->inbuf_len = insize;
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    ESP_GOTO_ON_ERROR((ret == JPEG_ERR_OK) ? ESP_OK : ESP_FAIL, cleanup, TAG, "parse jpeg header failed");
    *w = out_info->width;
    *h = out_info->height;
    free(jpeg_io);
    free(out_info);
    jpeg_dec_close(jpeg_dec);
    return LV_RES_OK;

cleanup:
    if (jpeg_io) {
        free(jpeg_io);
    }
    if (out_info) {
        free(out_info);
    }
    jpeg_dec_close(jpeg_dec);
    return LV_RES_INV;
}

static lv_res_t jpeg_decode_hw_or_sw(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, uint32_t insize, bool *out_is_hw)
{
#if ESP_LV_ENABLE_HW_JPEG
    if (out_is_hw) {
        *out_is_hw = false;
    }
    if (jpeg_decode_header(w, h, in, insize) == LV_RES_OK) {
        if (((*w) % 16 == 0) && ((*h) % 16 == 0) && (*w >= 64) && (*h >= 64) && jpgd_handle) {
            size_t req_size = (*w) * (*h) * DEC_RGB_BPP;
            jpeg_decode_memory_alloc_cfg_t mem_cfg = { .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER };
            size_t out_size = 0;
            *out = (uint8_t *)jpeg_alloc_decoder_mem(req_size, &mem_cfg, &out_size);
            if (!*out) {
                return LV_RES_INV;
            }
            jpeg_decode_cfg_t cfg = {
                .output_format = DEC_JPEG_HW_OUT_FORMAT,
                .rgb_order = DEC_JPEG_HW_RGB_ORDER,
            };
            uint32_t used = 0;

            safe_cache_sync((void *)in, insize, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
            safe_cache_sync(*out, out_size, ESP_CACHE_MSYNC_FLAG_INVALIDATE);
            int r = jpeg_decoder_process(jpgd_handle, &cfg, in, insize, *out, out_size, &used);

            if (r == ESP_OK) {
                safe_cache_sync(*out, out_size, ESP_CACHE_MSYNC_FLAG_INVALIDATE);
                if (out_is_hw) {
                    *out_is_hw = true;
                }
                return LV_RES_OK;
            }
            jpeg_free_align(*out);
            *out = NULL;
        } else { }
    } else {
        ESP_LOGE(TAG, "HW JPG header parse fail");
    }
#endif
    {
        lv_res_t sw_r = jpeg_decode(out, w, h, in, insize);

        return sw_r;
    }
}

#if ESP_LV_ENABLE_PJPG
static lv_res_t pjpg_parse_header(const uint8_t *buf, size_t len, pjpg_info_t *info)
{
    if (len < PJPG_HEADER_SIZE) {
        ESP_LOGE(TAG, "PJPG hdr too small len=%u", (unsigned)len);
        return LV_RES_INV;
    }
    if (memcmp(buf, PJPG_MAGIC, PJPG_MAGIC_LEN) != 0) {
        ESP_LOGE(TAG, "PJPG magic mismatch: %02X %02X %02X %02X %02X %02X %02X", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);
        return LV_RES_INV;
    }
    const uint8_t *p = buf + PJPG_MAGIC_LEN + 1;
    info->width = *(uint16_t *)p; p += 2;
    info->height = *(uint16_t *)p; p += 2;
    info->rgb_jpeg_size = *(uint32_t *)p; p += 4;
    info->alpha_jpeg_size = *(uint32_t *)p;

    return LV_RES_OK;
}

#if ESP_LV_ENABLE_HW_JPEG
static esp_err_t ensure_jpeg_output_buffer(uint8_t **buf, size_t *buf_size, size_t min_need)
{
    if (*buf_size < min_need) {
        if (*buf) {
            jpeg_free_align(*buf);
            *buf = NULL;
            *buf_size = 0;
        }
        size_t out_size = 0;
        uint8_t *new_buf = jpeg_alloc_decoder_mem(min_need, &(jpeg_decode_memory_alloc_cfg_t) {
            .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER
        }, &out_size);
        if (!new_buf) {
            return ESP_ERR_NO_MEM;
        }
        *buf = new_buf;
        *buf_size = out_size;
    }
    return ESP_OK;
}

static esp_err_t decode_jpeg_plane_to_dst(const uint8_t *jpeg,
                                          uint32_t jpeg_size,
                                          const jpeg_decode_cfg_t *cfg,
                                          uint8_t **rx_buf,
                                          size_t *rx_buf_size,
                                          uint32_t aligned_w,
                                          uint32_t aligned_h,
                                          uint32_t out_w,
                                          uint32_t out_h,
                                          uint32_t bytes_per_pixel,
                                          uint8_t *dst_plane,
                                          uint32_t dst_stride)
{
    size_t min_need = (size_t)aligned_w * aligned_h * bytes_per_pixel;
    esp_err_t err = ensure_jpeg_output_buffer(rx_buf, rx_buf_size, min_need);
    if (err != ESP_OK) {
        return err;
    }

    uint32_t used = 0;
    safe_cache_sync(*rx_buf, *rx_buf_size, ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    int r = jpeg_decoder_process(jpgd_handle, cfg, jpeg, jpeg_size, *rx_buf, *rx_buf_size, &used);
    safe_cache_sync(*rx_buf, *rx_buf_size, ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    if (r != ESP_OK) {
        return ESP_FAIL;
    }

    uint32_t row_bytes = out_w * bytes_per_pixel;
    for (uint32_t y = 0; y < out_h; y++) {
        const uint8_t *src = *rx_buf + (size_t)y * aligned_w * bytes_per_pixel;
        uint8_t *dst = dst_plane + (size_t)y * dst_stride;
        memcpy(dst, src, row_bytes);
    }
    return ESP_OK;
}
#endif

static lv_res_t pjpg_decode(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, uint32_t insize)
{
    lv_res_t res = LV_RES_INV;
    pjpg_info_t info;
    bool mutex_locked = false;
#if LV_COLOR_DEPTH == 16
    uint8_t *dst = NULL;
#endif
#if ESP_LV_ENABLE_PJPG
    if (s_pjpg_mutex) {
        if (xSemaphoreTake(s_pjpg_mutex, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire PJPG mutex");
            return LV_RES_INV;
        }
        mutex_locked = true;
    }
#endif

    if (pjpg_parse_header(in, insize, &info) != LV_RES_OK) {
        goto exit;
    }
    size_t need = PJPG_HEADER_SIZE + info.rgb_jpeg_size + info.alpha_jpeg_size;
    if (insize < need) {
        goto exit;
    }
    *w = info.width;
    *h = info.height;
#if LV_COLOR_DEPTH == 16
    size_t final_size = (size_t)(*w) * (*h) * 3;
    dst = (uint8_t *)heap_caps_aligned_alloc(16, final_size, MALLOC_CAP_DEFAULT);
    if (!dst) {
        goto exit;
    }
    uint32_t aligned_w = ((*w) + 15) & ~15;
    uint32_t aligned_h = ((*h) + 15) & ~15;

    const uint8_t *rgb_jpeg = in + PJPG_HEADER_SIZE;

    safe_cache_sync((void *)in, insize, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    if (!jpgd_handle) {
        ESP_LOGE(TAG, "PJPG no handle");
        free(dst);
        dst = NULL;
        goto exit;
    }

    jpeg_decode_cfg_t rgb_cfg = { .output_format = JPEG_DECODE_OUT_FORMAT_RGB565, .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR };
    uint8_t *rgb_plane = dst;
    size_t rgb_plane_size = (size_t)(*w) * (*h) * 2;
    if (decode_jpeg_plane_to_dst(rgb_jpeg, info.rgb_jpeg_size, &rgb_cfg,
                                 &s_pjpg_rx_rgb, &s_pjpg_rx_rgb_size,
                                 aligned_w, aligned_h, *w, *h, 2,
                                 rgb_plane, (*w) * 2) != ESP_OK) {
        free(dst);
        dst = NULL;
        goto exit;
    }

    uint8_t *alpha_plane = dst + rgb_plane_size;
    size_t alpha_plane_size = (size_t)(*w) * (*h);
    if (info.alpha_jpeg_size > 0) {
        jpeg_decode_cfg_t a_cfg = { .output_format = JPEG_DECODE_OUT_FORMAT_GRAY };
        const uint8_t *a_jpeg = rgb_jpeg + info.rgb_jpeg_size;
        if (decode_jpeg_plane_to_dst(a_jpeg, info.alpha_jpeg_size, &a_cfg,
                                     &s_pjpg_rx_a, &s_pjpg_rx_a_size,
                                     aligned_w, aligned_h, *w, *h, 1,
                                     alpha_plane, (*w)) != ESP_OK) {
            free(dst);
            dst = NULL;
            goto exit;
        }
    } else {
        memset(alpha_plane, 0xFF, alpha_plane_size);
    }

    safe_cache_sync(dst, final_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    *out = dst;
    res = LV_RES_OK;
#else
    LV_UNUSED(out);
    LV_UNUSED(w);
    LV_UNUSED(h);
    LV_UNUSED(in);
    LV_UNUSED(insize);
    goto exit;
#endif

exit:
#if LV_COLOR_DEPTH == 16
    if (res != LV_RES_OK && dst) {
        free(dst);
        dst = NULL;
    }
#endif
#if ESP_LV_ENABLE_PJPG
    if (mutex_locked) {
        xSemaphoreGive(s_pjpg_mutex);
    }
#endif
    return res;
}
#endif

static int is_pjpg(const uint8_t *raw_data, size_t len)
{
#if ESP_LV_ENABLE_PJPG
    if (len < PJPG_HEADER_SIZE) {
        return false;
    }
    return memcmp(raw_data, PJPG_MAGIC, PJPG_MAGIC_LEN) == 0;
#else
    return false;
#endif
}

#if ESP_LV_ENABLE_HW_JPEG
static esp_err_t safe_cache_sync(void *addr, size_t size, int flags)
{
    if (!addr || size == 0) {
        return ESP_OK;
    }
    const size_t CACHE_LINE_SIZE = 128;
    uintptr_t start_addr = (uintptr_t)addr;
    uintptr_t aligned_start = start_addr & ~(CACHE_LINE_SIZE - 1);
    size_t end_addr = start_addr + size;
    size_t aligned_end = (end_addr + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
    size_t aligned_size = aligned_end - aligned_start;
    return esp_cache_msync((void*)aligned_start, aligned_size, flags);
}
#endif

static void decoder_free(image_decoder_t *img_dec)
{
    if (img_dec->frame_cache) {
        if (img_dec->frame_cache_is_hw) {
#if ESP_LV_ENABLE_HW_JPEG
            jpeg_free_align(img_dec->frame_cache);
#else
            free(img_dec->frame_cache);
#endif
        } else {
            free(img_dec->frame_cache);
        }
    }
    if (img_dec->frame_base_array) {
        free(img_dec->frame_base_array);
    }
    if (img_dec->frame_base_offset) {
        free(img_dec->frame_base_offset);
    }
}

static void decoder_cleanup(image_decoder_t *img_dec)
{
    if (! img_dec) {
        return;
    }

    decoder_free(img_dec);
    free(img_dec);
}
