/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_lv_decoder.h"

#include "lvgl.h"

#include "png.h"
#include "esp_jpeg_dec.h"
#define QOI_IMPLEMENTATION
#include "qoi.h"

/*********************
 *      DEFINES
 *********************/

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
    uint8_t img_depth;
    io_source_t io;
} image_decoder_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
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

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG = "img_dec";

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Register the PNG decoder functions in LVGL
 */
esp_err_t esp_lv_decoder_init(esp_lv_decoder_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(ret_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    lv_img_decoder_t *dec = lv_img_decoder_create();
    lv_img_decoder_set_info_cb(dec, decoder_info);
    lv_img_decoder_set_open_cb(dec, decoder_open);
    lv_img_decoder_set_close_cb(dec, decoder_close);
    lv_img_decoder_set_read_line_cb(dec, decoder_read_line);

    *ret_handle = dec;
    ESP_LOGD(TAG, "new img_dec decoder @%p", dec);

    ESP_LOGD(TAG, "img_dec decoder create success, version: %d.%d.%d", ESP_LV_DECODER_VER_MAJOR, ESP_LV_DECODER_VER_MINOR, ESP_LV_DECODER_VER_PATCH);
    return ESP_OK;
}

esp_err_t esp_lv_decoder_deinit(esp_lv_decoder_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid decoder handle pointer");
    ESP_LOGD(TAG, "delete img_dec decoder @%p", handle);
    lv_img_decoder_delete(handle);

    return ESP_OK;

}

/**********************
 *   STATIC FUNCTIONS
 **********************/

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
#if  LV_COLOR_DEPTH == 32
        .output_type = JPEG_PIXEL_FORMAT_RGB888,
#elif  LV_COLOR_DEPTH == 16
#if  LV_BIG_ENDIAN_SYSTEM == 1 || LV_COLOR_16_SWAP == 1
        .output_type = JPEG_PIXEL_FORMAT_RGB565_BE,
#else
        .output_type = JPEG_PIXEL_FORMAT_RGB565_LE,
#endif
#else
#error Unsupported LV_COLOR_DEPTH
#endif
        .rotate = JPEG_ROTATE_0D,
    };

    jpeg_dec_handle_t jpeg_dec;
    jpeg_dec_open(&config, &jpeg_dec);
    if (!jpeg_dec) {
        ESP_LOGE(TAG, "Failed to open jpeg decoder");
        return LV_RES_INV;
    }

    jpeg_dec_io_t *jpeg_io = malloc(sizeof(jpeg_dec_io_t));
    jpeg_dec_header_info_t *out_info = malloc(sizeof(jpeg_dec_header_info_t));
    if (!jpeg_io || !out_info) {
        if (jpeg_io) {
            free(jpeg_io);
        }
        if (out_info) {
            free(out_info);
        }
        jpeg_dec_close(jpeg_dec);
        ESP_LOGE(TAG, "Failed to allocate memory for jpeg decoder");
        return LV_RES_INV;
    }

    jpeg_io->inbuf = (unsigned char *)in;
    jpeg_io->inbuf_len = insize;

    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret == JPEG_ERR_OK) {

        *w = out_info->width;
        *h = out_info->height;
        if (out) {
            *out = (uint8_t *)heap_caps_aligned_alloc(16, out_info->height * out_info->width * 2, MALLOC_CAP_DEFAULT);
            if (!*out) {
                free(jpeg_io);
                free(out_info);
                jpeg_dec_close(jpeg_dec);
                ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                return LV_RES_INV;
            }

            jpeg_io->outbuf = *out;
            ret = jpeg_dec_process(jpeg_dec, jpeg_io);
            if (ret != JPEG_ERR_OK) {
                free(*out);
                *out = NULL;
                free(jpeg_io);
                free(out_info);
                jpeg_dec_close(jpeg_dec);
                ESP_LOGE(TAG, "Failed to decode jpeg:[%d]", ret);
                return LV_RES_INV;
            }
        }
    } else {
        free(jpeg_io);
        free(out_info);
        jpeg_dec_close(jpeg_dec);
        ESP_LOGE(TAG, "Failed to parse jpeg header");
        return LV_RES_INV;
    }

    free(jpeg_io);
    free(out_info);
    jpeg_dec_close(jpeg_dec);

    return LV_RES_OK;
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

/**
 * Get info about a PNG image
 * @param src can be file name or pointer to a C array
 * @param header store the info here
 * @return LV_RES_OK: no error; LV_RES_INV: can't get the info
 */
static lv_res_t decoder_info(struct _lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{
    (void) decoder; /*Unused*/
    lv_img_src_t src_type = lv_img_src_get_type(src);          /*Get the source type*/

    lv_res_t lv_ret = LV_RES_OK;

    if (src_type == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t *img_dsc = src;
        uint8_t *img_dsc_data = (uint8_t *)img_dsc->data;
        const uint32_t img_dsc_size = img_dsc->data_size;

        if (!strncmp((char *)img_dsc_data, "_SPNG__", strlen("_SPNG__")) ||
                !strncmp((char *)img_dsc_data, "_SQOI__", strlen("_SQOI__")) ||
                !strncmp((char *)img_dsc_data, "_SJPG__", strlen("_SJPG__"))) {
            if (!strncmp((char *)img_dsc_data, "_SJPG__", strlen("_SJPG__"))) {
                header->cf = LV_IMG_CF_RAW;
            } else {
                header->cf = LV_IMG_CF_RAW_ALPHA;
            }
            img_dsc_data += 14; //seek to res info ... refer spng format

            header->always_zero = 0;
            header->w = *img_dsc_data++;
            header->w |= *img_dsc_data++ << 8;
            header->h = *img_dsc_data++;
            header->h |= *img_dsc_data++ << 8;

            return lv_ret;
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
            lv_ret = jpeg_decode(NULL, &width, &height, img_dsc_data, img_dsc_size);

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
            lv_ret = jpeg_decode(NULL, &width, &height, load_img_data, load_img_size);

            header->cf = LV_IMG_CF_TRUE_COLOR;
            header->w = width;
            header->h = height;
            free(load_img_data);
            return lv_ret;
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

            if (!strcmp((char *)buff, "_SPNG__") ||
                    !strcmp((char *)buff, "_SQOI__") ||
                    !strcmp((char *)buff, "_SJPG__")) {

                if (!strncmp((char *)buff, "_SJPG__", strlen("_SJPG__"))) {
                    header->cf = LV_IMG_CF_TRUE_COLOR;
                } else {
                    header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
                }

                lv_fs_seek(&file, 14, LV_FS_SEEK_SET);
                res = lv_fs_read(&file, buff, 4, &rn);
                if (res != LV_FS_RES_OK || rn != 4) {
                    lv_fs_close(&file);
                    return LV_RES_INV;
                }
                uint8_t *data = buff;
                header->always_zero = 0;
                header->w = *data++;
                header->w |= *data++ << 8;
                header->h = *data++;
                header->h |= *data++ << 8;
                lv_fs_close(&file);
                return lv_ret;
            }
        } else {
            return LV_RES_INV;
        }
    }

    return LV_RES_INV;         /*If didn't succeeded earlier then it's an error*/
}

/**
 * Open a PNG image and return the decided image
 * @param src can be file name or pointer to a C array
 * @param style style of the image object (unused now but certain formats might use it)
 * @return pointer to the decoded image or `LV_IMG_DECODER_OPEN_FAIL` if failed
 */
static lv_res_t decoder_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{

    (void) decoder; /*Unused*/
    lv_res_t lv_ret = LV_RES_OK;        /*For the return values of PNG decoder functions*/

    uint8_t *img_data = NULL;
    uint32_t png_width;
    uint32_t png_height;

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

        if (!strncmp((char *) img_dec->dsc_data, "_SPNG__", strlen("_SPNG__")) ||
                !strncmp((char *) img_dec->dsc_data, "_SQOI__", strlen("_SQOI__")) ||
                !strncmp((char *) img_dec->dsc_data, "_SJPG__", strlen("_SJPG__"))) {
            data = img_dec->dsc_data;
            data += 14;

            img_dec->img_x_res = *data++;
            img_dec->img_x_res |= *data++ << 8;

            img_dec->img_y_res = *data++;
            img_dec->img_y_res |= *data++ << 8;

            img_dec->img_total_frames = *data++;
            img_dec->img_total_frames |= *data++ << 8;

            img_dec->img_single_frame_height = *data++;
            img_dec->img_single_frame_height |= *data++ << 8;

            ESP_LOGD(TAG, "[%d,%d], frames:%d, height:%d", img_dec->img_x_res, img_dec->img_y_res, \
                     img_dec->img_total_frames, img_dec->img_single_frame_height);
            img_dec->frame_base_array = malloc(sizeof(uint8_t *) * img_dec->img_total_frames);
            if (! img_dec->frame_base_array) {
                ESP_LOGE(TAG, "Not enough memory for frame_base_array allocation");
                decoder_cleanup(img_dec);
                img_dec = NULL;
                return LV_RES_INV;
            }

            uint8_t *img_frame_base = data +  img_dec->img_total_frames * 2;
            img_dec->frame_base_array[0] = img_frame_base;

            for (int i = 1; i <  img_dec->img_total_frames; i++) {
                int offset = *data++;
                offset |= *data++ << 8;
                img_dec->frame_base_array[i] = img_dec->frame_base_array[i - 1] + offset;
            }
            img_dec->img_cache_frame_index = -1;
            img_dec->frame_cache = (void *)malloc(img_dec->img_x_res * img_dec->img_single_frame_height * 4);
            if (! img_dec->frame_cache) {
                ESP_LOGE(TAG, "Not enough memory for frame_cache allocation");
                decoder_cleanup(img_dec);
                img_dec = NULL;
                return LV_RES_INV;
            }
            dsc->img_data = NULL;

            return lv_ret;
        } else if (is_png(img_dec->dsc_data, img_dec->dsc_size) == true ||
                   is_qoi(img_dec->dsc_data, img_dec->dsc_size) == true ||
                   is_jpg(img_dec->dsc_data, img_dec->dsc_size) == true) {
            /*Decode the image in ARGB8888 */
            if (is_png(img_dec->dsc_data, img_dec->dsc_size) == true) {
                lv_ret = libpng_decode32(&img_data, &png_width, &png_height, img_dsc->data, img_dsc->data_size);
            } else if (is_qoi(img_dec->dsc_data, img_dec->dsc_size) == true) {
                lv_ret = qoi_decode32(&img_data, &png_width, &png_height, img_dsc->data, img_dsc->data_size);
            } else if (is_jpg(img_dec->dsc_data, img_dec->dsc_size) == true) {
                lv_ret = jpeg_decode(&img_data, &png_width, &png_height, img_dsc->data, img_dsc->data_size);
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
                    convert_color_depth(img_data,  png_width * png_height);
                }
                dsc->img_data = img_data;
                img_dec->frame_cache = img_data;
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
                !strcmp(lv_fs_get_ext(fn), "jpg")) {
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

            if (!strcmp(lv_fs_get_ext(fn), "png")) {
                lv_ret = libpng_decode32(&img_data, &png_width, &png_height, load_img_data, load_img_size);
            } else if (!strcmp(lv_fs_get_ext(fn), "qoi")) {
                lv_ret = qoi_decode32(&img_data, &png_width, &png_height, load_img_data, load_img_size);
            } else if (!strcmp(lv_fs_get_ext(fn), "jpg")) {
                lv_ret = jpeg_decode(&img_data, &png_width, &png_height, load_img_data, load_img_size);
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
                    convert_color_depth(img_data,  png_width * png_height);
                }
                dsc->img_data = img_data;
                img_dec->frame_cache = img_data;
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
            res = lv_fs_read(&lv_file, buff, 22, &rn);
            if (res != LV_FS_RES_OK || rn != 22) {
                lv_fs_close(&lv_file);
                return LV_RES_INV;
            }

            if (!strcmp((char *)buff, "_SPNG__") ||
                    !strcmp((char *)buff, "_SQOI__") ||
                    !strcmp((char *)buff, "_SJPG__")) {

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
                data += 14;

                img_dec->img_x_res = *data++;
                img_dec->img_x_res |= *data++ << 8;

                img_dec->img_y_res = *data++;
                img_dec->img_y_res |= *data++ << 8;

                img_dec->img_total_frames = *data++;
                img_dec->img_total_frames |= *data++ << 8;

                img_dec->img_single_frame_height = *data++;
                img_dec->img_single_frame_height |= *data++ << 8;

                ESP_LOGD(TAG, "[%d,%d], frames:%d, height:%d", img_dec->img_x_res, img_dec->img_y_res, \
                         img_dec->img_total_frames, img_dec->img_single_frame_height);
                img_dec->frame_base_offset = malloc(sizeof(int *) * img_dec->img_total_frames);
                if (! img_dec->frame_base_offset) {
                    ESP_LOGE(TAG, "Not enough memory for frame_base_offset allocation");
                    decoder_cleanup(img_dec);
                    img_dec = NULL;
                    return LV_RES_INV;
                }

                int img_frame_start_offset = 22 +  img_dec->img_total_frames * 2;
                img_dec->frame_base_offset[0] = img_frame_start_offset;

                for (int i = 1; i <  img_dec->img_total_frames; i++) {
                    res = lv_fs_read(&lv_file, buff, 2, &rn);
                    if (res != LV_FS_RES_OK || rn != 2) {
                        lv_fs_close(&lv_file);
                        return LV_RES_INV;
                    }

                    data = buff;
                    int offset = *data++;
                    offset |= *data++ << 8;
                    img_dec->frame_base_offset[i] = img_dec->frame_base_offset[i - 1] + offset;
                }

                img_dec->img_cache_frame_index = -1; //INVALID AT BEGINNING for a forced compare mismatch at first time.
                img_dec->frame_cache = (void *)malloc(img_dec->img_x_res * img_dec->img_single_frame_height * 4);
                if (!img_dec->frame_cache) {
                    ESP_LOGE(TAG, "Not enough memory for frame_cache allocation");
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

/**
 * Decode `len` pixels starting from the given `x`, `y` coordinates and store them in `buf`.
 * Required only if the "open" function can't open the whole decoded pixel array. (dsc->img_data == NULL)
 * @param decoder pointer to the decoder the function associated with
 * @param dsc pointer to decoder descriptor
 * @param x start x coordinate
 * @param y start y coordinate
 * @param len number of pixels to decode
 * @param buf a buffer to store the decoded pixels
 * @return LV_RES_OK: ok; LV_RES_INV: failed
 */

static lv_res_t decoder_read_line(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t len, uint8_t *buf)
{
    LV_UNUSED(decoder);

    lv_res_t error = LV_RES_INV;
    uint8_t *img_data = NULL;

    uint8_t RGBA_depth = 0;
    uint8_t RGB_depth = 0;

    uint32_t png_width;
    uint32_t png_height;

    uint8_t *img_block_data;
    uint32_t img_block_size;

#if LV_COLOR_DEPTH == 32
    RGBA_depth = 4;
    RGB_depth = 4;
#elif LV_COLOR_DEPTH == 16
    RGBA_depth = 3;
    RGB_depth = 2;
#elif LV_COLOR_DEPTH == 8
    RGBA_depth = 2;
#elif LV_COLOR_DEPTH == 1
    RGBA_depth = 2;
#endif

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
                error = libpng_decode32(&img_data, &png_width, &png_height, img_dec->frame_cache, rn);
            } else if (is_qoi(img_dec->frame_cache, rn) == true) {
                error = qoi_decode32(&img_data, &png_width, &png_height, img_dec->frame_cache, rn);
            } else if (is_jpg(img_dec->frame_cache, rn) == true) {
                error = jpeg_decode(&img_data, &png_width, &png_height, img_dec->frame_cache, rn);
            }
            if (error != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode error:%d", error);
                if (img_data != NULL) {
                    free(img_data);
                }
                return LV_RES_INV;
            } else {
                if (is_jpg(img_dec->frame_cache, rn) == false) {
                    convert_color_depth(img_data,  png_width * png_height);
                    img_dec->img_depth = RGBA_depth;
                } else {
                    img_dec->img_depth = RGB_depth;
                }
                memcpy(img_dec->frame_cache, img_data, png_width * png_height * img_dec->img_depth);
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
                error = libpng_decode32(&img_data, &png_width, &png_height, img_block_data, img_block_size);
            } else if (is_qoi(img_block_data, img_block_size) == true) {
                error = qoi_decode32(&img_data, &png_width, &png_height, img_block_data, img_block_size);
            } else if (is_jpg(img_block_data, img_block_size) == true) {
                error = jpeg_decode(&img_data, &png_width, &png_height, img_block_data, img_block_size);
            }
            if (error != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode error:%d", error);
                if (img_data != NULL) {
                    free(img_data);
                }
                return LV_RES_INV;
            } else {
                if (is_jpg(img_block_data, img_block_size) == false) {
                    convert_color_depth(img_data,  png_width * png_height);
                    img_dec->img_depth = RGBA_depth;
                } else {
                    img_dec->img_depth = RGB_depth;
                }
                memcpy(img_dec->frame_cache, img_data, img_dec->img_single_frame_height * img_dec->img_x_res * img_dec->img_depth);
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

/**
 * If the display is not in 32 bit format (ARGB888) then convert the image to the current color depth
 * @param img the ARGB888 image
 * @param px_cnt number of pixels in `img`
 */
static void convert_color_depth(uint8_t *img, uint32_t px_cnt)
{
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

static void decoder_free(image_decoder_t *img_dec)
{
    if (img_dec->frame_cache) {
        free(img_dec->frame_cache);
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
