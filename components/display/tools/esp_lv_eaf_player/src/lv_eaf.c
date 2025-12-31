/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file lv_eaf.c
 * @brief EAF (Emote Animation Format) player implementation for LVGL
 */

/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include "lv_eaf_private.h"
#include "lvgl.h"
#include "esp_eaf_dec.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"

#if LVGL_VERSION_MAJOR >= 9
#include "src/misc/lv_timer_private.h"
#include "src/misc/cache/lv_cache.h"
#include "src/core/lv_obj_class_private.h"

typedef lv_result_t lv_eaf_event_res_t;
#define LV_EAF_RESULT_OK LV_RESULT_OK
#define LV_EAF_SEND_EVENT(obj, code, param) lv_obj_send_event((obj), (code), (param))
#define LV_EAF_CACHE_DROP(src) lv_image_cache_drop((src))
#define LV_EAF_GET_SRC(obj) lv_image_get_src((obj))
#define LV_EAF_SET_SRC(obj, src) lv_image_set_src((obj), (src))
#define LV_EAF_SRC_GET_TYPE(src) lv_image_src_get_type(src)
#define LV_EAF_SRC_TYPE_VARIABLE LV_IMAGE_SRC_VARIABLE
#define LV_EAF_SRC_TYPE_FILE LV_IMAGE_SRC_FILE
#else
/* LVGL v8 compatibility */
typedef lv_res_t lv_eaf_event_res_t;
#define LV_EAF_RESULT_OK LV_RES_OK
#define LV_EAF_SEND_EVENT(obj, code, param) lv_event_send((obj), (code), (param))
#define LV_EAF_CACHE_DROP(src) lv_img_cache_invalidate_src((src))
#define LV_EAF_GET_SRC(obj) lv_img_get_src((obj))
#define LV_EAF_SET_SRC(obj, src) lv_img_set_src((obj), (src))
#define LV_EAF_SRC_GET_TYPE(src) lv_img_src_get_type(src)
#define LV_EAF_SRC_TYPE_VARIABLE LV_IMG_SRC_VARIABLE
#define LV_EAF_SRC_TYPE_FILE LV_IMG_SRC_FILE
#endif

#if LVGL_VERSION_MAJOR >= 9
typedef lv_image_dsc_t lv_eaf_src_dsc_t;
#else
typedef lv_img_dsc_t lv_eaf_src_dsc_t;
#endif

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS (&lv_eaf_class)
#define DEFAULT_FRAME_DELAY 100  /* Default frame delay in ms */

/**
 * @brief Align size up to alignment boundary
 */
#define ESP_EAF_ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

/**
 * @brief Get cache alignment for PSRAM
 */
static inline size_t esp_eaf_get_psram_cache_align(void)
{
    size_t cache_align = 0;
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
    esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &cache_align);
#endif
    /* Fallback to 4-byte alignment if cache alignment not available */
    if (cache_align == 0) {
        cache_align = 4;
    }
    return cache_align;
}

/**
 * @brief Allocate memory with PSRAM priority
 *
 * Tries to allocate from PSRAM first, falls back to internal RAM if unavailable.
 */
static inline void *esp_eaf_malloc_prefer_psram(size_t size)
{
    void *ptr = NULL;

    /* Try PSRAM first if available */
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
    ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif

    /* Fallback to default memory (internal RAM) */
    if (ptr == NULL) {
        ptr = malloc(size);
    }

    return ptr;
}

/**
 * @brief Allocate aligned memory with PSRAM priority
 *
 * Tries to allocate aligned memory from PSRAM first, falls back to internal RAM if unavailable.
 * This is required for hardware JPEG decoder output buffers.
 *
 * @param size Size to allocate
 * @param alignment Alignment requirement (0 means use cache line alignment)
 * @param allocated_size Pointer to store actual allocated size (may be larger due to alignment)
 * @return Pointer to allocated memory, or NULL on failure
 */
static inline void *esp_eaf_malloc_aligned_prefer_psram(size_t size, size_t alignment, size_t *allocated_size)
{
    void *ptr = NULL;
    size_t aligned_size = size;

    /* Use cache alignment if not specified */
    if (alignment == 0) {
        alignment = esp_eaf_get_psram_cache_align();
    }

    /* Align size up to alignment boundary */
    aligned_size = ESP_EAF_ALIGN_UP(size, alignment);

    /* Try PSRAM first if available */
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
    ptr = heap_caps_aligned_alloc(alignment, aligned_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif

    /* Fallback to default memory (internal RAM) with alignment */
    if (ptr == NULL) {
        ptr = heap_caps_aligned_alloc(alignment, aligned_size, MALLOC_CAP_DEFAULT);
    }

    if (allocated_size != NULL) {
        *allocated_size = (ptr != NULL) ? aligned_size : 0;
    }

    return ptr;
}

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_eaf_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_eaf_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void next_frame_task_cb(lv_timer_t * t);
static esp_err_t load_eaf_from_data(lv_eaf_t * eaf_obj, const uint8_t * data, size_t data_size);
static esp_err_t decode_current_frame(lv_eaf_t * eaf_obj);
static bool is_eaf_format(const uint8_t * data);
static bool eaf_has_alpha_palette(esp_eaf_format_handle_t handle);

/**********************
 *  STATIC VARIABLES
 **********************/

#if LVGL_VERSION_MAJOR >= 9
const lv_obj_class_t lv_eaf_class = {
    .constructor_cb = lv_eaf_constructor,
    .destructor_cb = lv_eaf_destructor,
    .instance_size = sizeof(lv_eaf_t),
    .base_class = LV_EAF_BASE_CLASS,
    .name = "lv_eaf",
};
#else
const lv_obj_class_t lv_eaf_class = {
    .base_class = LV_EAF_BASE_CLASS,
    .constructor_cb = lv_eaf_constructor,
    .destructor_cb = lv_eaf_destructor,
    .instance_size = sizeof(lv_eaf_t),
};
#endif

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_eaf_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void lv_eaf_set_src(lv_obj_t * obj, const void * src)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    /* Close previous EAF if any */
    if (eaf_obj->eaf_handle != NULL) {
        LV_EAF_CACHE_DROP(LV_EAF_GET_SRC(obj));

        esp_eaf_format_deinit(eaf_obj->eaf_handle);
        eaf_obj->eaf_handle = NULL;
        esp_eaf_free_header(&eaf_obj->eaf_header);

        if (eaf_obj->frame_buffer) {
            free(eaf_obj->frame_buffer);
            eaf_obj->frame_buffer = NULL;
        }
        eaf_obj->frame_buffer_size = 0;
        eaf_obj->color_buffer_size = 0;
        eaf_obj->use_alpha = false;
        if (eaf_obj->file_data) {
            free(eaf_obj->file_data);
            eaf_obj->file_data = NULL;
        }
        eaf_obj->imgdsc.data = NULL;
    }

    esp_err_t ret = ESP_FAIL;
    if (LV_EAF_SRC_GET_TYPE(src) == LV_EAF_SRC_TYPE_VARIABLE) {
        const lv_eaf_src_dsc_t * img_dsc = src;
        ret = load_eaf_from_data(eaf_obj, img_dsc->data, img_dsc->data_size);
    } else if (LV_EAF_SRC_GET_TYPE(src) == LV_EAF_SRC_TYPE_FILE) {
        const char *path = (const char *)src;

        /* Open file */
        lv_fs_file_t f;
        lv_fs_res_t res = lv_fs_open(&f, path, LV_FS_MODE_RD);
        if (res != LV_FS_RES_OK) {
            LV_LOG_ERROR("Failed to open file: %s", path);
            return;
        }

        /* Get file size */
        uint32_t file_size = 0;
        res = lv_fs_seek(&f, 0, LV_FS_SEEK_END);
        if (res != LV_FS_RES_OK) {
            LV_LOG_ERROR("Failed to seek file end: %s (err=%d)", path, res);
            lv_fs_close(&f);
            return;
        }
        res = lv_fs_tell(&f, &file_size);
        if (res != LV_FS_RES_OK) {
            LV_LOG_ERROR("Failed to get file size: %s (err=%d)", path, res);
            lv_fs_close(&f);
            return;
        }
        res = lv_fs_seek(&f, 0, LV_FS_SEEK_SET);
        if (res != LV_FS_RES_OK) {
            LV_LOG_ERROR("Failed to rewind file: %s (err=%d)", path, res);
            lv_fs_close(&f);
            return;
        }

        if (file_size == 0) {
            LV_LOG_ERROR("File is empty: %s", path);
            lv_fs_close(&f);
            return;
        }

        /* Allocate buffer and read file (prefer PSRAM for large files) */
        uint8_t *file_data = (uint8_t *)esp_eaf_malloc_prefer_psram(file_size);
        if (!file_data) {
            LV_LOG_ERROR("Failed to allocate buffer for file: %lu bytes", (unsigned long)file_size);
            lv_fs_close(&f);
            return;
        }

        uint32_t bytes_read;
        res = lv_fs_read(&f, file_data, file_size, &bytes_read);
        lv_fs_close(&f);

        if (res != LV_FS_RES_OK || bytes_read != file_size) {
            LV_LOG_ERROR("Failed to read file: %s", path);
            free(file_data);
            return;
        }

        /* Store file data pointer for cleanup in destructor */
        eaf_obj->file_data = file_data;

        ret = load_eaf_from_data(eaf_obj, file_data, file_size);
        if (ret != ESP_OK) {
            free(file_data);
            eaf_obj->file_data = NULL;
        }
    }

    if (ret != ESP_OK) {
        LV_LOG_WARN("Couldn't load the EAF source");
        return;
    }

    eaf_obj->last_call = lv_tick_get();
    LV_EAF_SET_SRC(obj, &eaf_obj->imgdsc);

    lv_timer_resume(eaf_obj->timer);
    lv_timer_reset(eaf_obj->timer);

    next_frame_task_cb(eaf_obj->timer);
}

void lv_eaf_restart(lv_obj_t * obj)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    if (eaf_obj->eaf_handle == NULL) {
        LV_LOG_WARN("EAF resource not loaded correctly");
        return;
    }

    eaf_obj->current_frame = 0;
    decode_current_frame(eaf_obj);
    lv_timer_resume(eaf_obj->timer);
    lv_timer_reset(eaf_obj->timer);
}

void lv_eaf_pause(lv_obj_t * obj)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;
    lv_timer_pause(eaf_obj->timer);
}

void lv_eaf_resume(lv_obj_t * obj)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    if (eaf_obj->eaf_handle == NULL) {
        LV_LOG_WARN("EAF resource not loaded correctly");
        return;
    }

    lv_timer_resume(eaf_obj->timer);
}

bool lv_eaf_is_loaded(lv_obj_t * obj)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    return (eaf_obj->eaf_handle != NULL);
}

int32_t lv_eaf_get_loop_count(lv_obj_t * obj)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    if (eaf_obj->eaf_handle == NULL) {
        return -1;
    }

    return eaf_obj->loop_count;
}

void lv_eaf_set_loop_count(lv_obj_t * obj, int32_t count)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    if (eaf_obj->eaf_handle == NULL) {
        LV_LOG_WARN("EAF resource not loaded correctly");
        return;
    }

    eaf_obj->loop_count = count;
}

int32_t lv_eaf_get_total_frames(lv_obj_t * obj)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    if (eaf_obj->eaf_handle == NULL) {
        return -1;
    }

    return eaf_obj->total_frames;
}

int32_t lv_eaf_get_current_frame(lv_obj_t * obj)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    if (eaf_obj->eaf_handle == NULL) {
        return -1;
    }

    return eaf_obj->current_frame;
}

void lv_eaf_set_frame_delay(lv_obj_t * obj, uint32_t delay)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    if (eaf_obj->eaf_handle == NULL) {
        LV_LOG_WARN("EAF resource not loaded correctly");
        return;
    }

    eaf_obj->frame_delay = delay;
    lv_timer_set_period(eaf_obj->timer, delay);
}

uint32_t lv_eaf_get_frame_delay(lv_obj_t * obj)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    if (eaf_obj->eaf_handle == NULL) {
        return 0;
    }

    return eaf_obj->frame_delay;
}

void lv_eaf_set_loop_enabled(lv_obj_t * obj, bool enable)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    if (eaf_obj->eaf_handle == NULL) {
        LV_LOG_WARN("EAF resource not loaded correctly");
        return;
    }

    eaf_obj->loop_enabled = enable;
}

bool lv_eaf_get_loop_enabled(lv_obj_t * obj)
{
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    if (eaf_obj->eaf_handle == NULL) {
        return false;
    }

    return eaf_obj->loop_enabled;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_eaf_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    memset(&eaf_obj->eaf_header, 0, sizeof(esp_eaf_header_t));
    eaf_obj->eaf_handle = NULL;
    eaf_obj->frame_buffer = NULL;
    eaf_obj->frame_buffer_size = 0;
    eaf_obj->color_buffer_size = 0;
    eaf_obj->use_alpha = false;
    eaf_obj->file_data = NULL;
    eaf_obj->current_frame = 0;
    eaf_obj->total_frames = 0;
    eaf_obj->frame_delay = DEFAULT_FRAME_DELAY;
    eaf_obj->loop_enabled = true;
    eaf_obj->loop_count = -1; /* Infinite loop by default */

    eaf_obj->timer = lv_timer_create(next_frame_task_cb, DEFAULT_FRAME_DELAY, obj);
    lv_timer_pause(eaf_obj->timer);
}

static void lv_eaf_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;

    LV_EAF_CACHE_DROP(LV_EAF_GET_SRC(obj));

    if (eaf_obj->eaf_handle) {
        esp_eaf_format_deinit(eaf_obj->eaf_handle);
    }

    esp_eaf_free_header(&eaf_obj->eaf_header);

    if (eaf_obj->frame_buffer) {
        free(eaf_obj->frame_buffer);
    }
    eaf_obj->frame_buffer_size = 0;
    eaf_obj->color_buffer_size = 0;
    eaf_obj->use_alpha = false;

    if (eaf_obj->file_data) {
        free(eaf_obj->file_data);
    }

#if LVGL_VERSION_MAJOR >= 9
    lv_timer_delete(eaf_obj->timer);
#else
    lv_timer_del(eaf_obj->timer);
#endif
}

static void next_frame_task_cb(lv_timer_t * t)
{
    lv_obj_t * obj = t->user_data;
    lv_eaf_t * eaf_obj = (lv_eaf_t *) obj;
    uint32_t elaps = lv_tick_elaps(eaf_obj->last_call);

    if (elaps < eaf_obj->frame_delay) {
        return;
    }

    eaf_obj->last_call = lv_tick_get();

    // Move to next frame
    eaf_obj->current_frame++;

    if (eaf_obj->current_frame >= eaf_obj->total_frames) {
        if (eaf_obj->loop_enabled && (eaf_obj->loop_count == -1 || eaf_obj->loop_count > 0)) {
            eaf_obj->current_frame = 0;
            if (eaf_obj->loop_count > 0) {
                eaf_obj->loop_count--;
            }
        } else {
            /*Animation finished*/
            lv_eaf_event_res_t res = LV_EAF_SEND_EVENT(obj, LV_EVENT_READY, NULL);
            lv_timer_pause(t);
            if (res != LV_EAF_RESULT_OK) {
                return;
            }
            return;
        }
    }

    decode_current_frame(eaf_obj);

    LV_EAF_CACHE_DROP(LV_EAF_GET_SRC(obj));
    lv_obj_invalidate(obj);
}

static bool is_eaf_format(const uint8_t * data)
{
    if (data == NULL) {
        return false;
    }

    // Check for EAF/AAF magic: 0x89 + "EAF" or 0x89 + "AAF"
    if (data[0] == 0x89) {
        if (memcmp(&data[1], "EAF", 3) == 0 || memcmp(&data[1], "AAF", 3) == 0) {
            return true;
        }
    }
    return false;
}

static bool eaf_has_alpha_palette(esp_eaf_format_handle_t handle)
{
    int total_frames = esp_eaf_format_get_total_frames(handle);
    if (total_frames <= 0) {
        LV_LOG_WARN("Alpha scan skipped: no frames available");
        return false;
    }
    for (int frame = 0; frame < total_frames; frame++) {
        const uint8_t *frame_data = esp_eaf_format_get_frame_data(handle, frame);
        int frame_size = esp_eaf_format_get_frame_size(handle, frame);
        if (!frame_data || frame_size <= 0) {
            LV_LOG_WARN("Alpha scan skipped for frame %d: data unavailable", frame);
            continue;
        }

        esp_eaf_header_t header;
        esp_eaf_format_t format = esp_eaf_header_parse(frame_data, frame_size, &header);
        if (format != ESP_EAF_FORMAT_VALID) {
            LV_LOG_WARN("Alpha scan skipped for frame %d: header parse failed", frame);
            continue;
        }

        bool found_alpha = false;
        if ((header.bit_depth == 4 || header.bit_depth == 8) && header.palette != NULL && header.num_colors > 0) {
            for (int i = 0; i < header.num_colors; i++) {
                const uint8_t *color_data = &header.palette[i * 4];
                if (color_data[3] < 0xFF) {
                    found_alpha = true;
                    break;
                }
            }
        }
        esp_eaf_free_header(&header);

        if (found_alpha) {
            return true;
        }
    }

    return false;
}

static esp_err_t load_eaf_from_data(lv_eaf_t * eaf_obj, const uint8_t * data, size_t data_size)
{
    if (!is_eaf_format(data)) {
        LV_LOG_ERROR("Invalid EAF/AAF format");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_eaf_format_init(data, data_size, &eaf_obj->eaf_handle);
    if (ret != ESP_OK) {
        LV_LOG_ERROR("Failed to initialize EAF parser");
        return ret;
    }

    eaf_obj->total_frames = esp_eaf_format_get_total_frames(eaf_obj->eaf_handle);
    eaf_obj->current_frame = 0;
    eaf_obj->use_alpha = eaf_has_alpha_palette(eaf_obj->eaf_handle);

    if (eaf_obj->total_frames <= 0) {
        LV_LOG_ERROR("No frames found in EAF");
        esp_eaf_format_deinit(eaf_obj->eaf_handle);
        eaf_obj->eaf_handle = NULL;
        return ESP_FAIL;
    }

    // Get first frame to determine dimensions
    const uint8_t *frame_data = esp_eaf_format_get_frame_data(eaf_obj->eaf_handle, 0);
    if (!frame_data) {
        LV_LOG_ERROR("Failed to get first frame data");
        esp_eaf_format_deinit(eaf_obj->eaf_handle);
        eaf_obj->eaf_handle = NULL;
        return ESP_FAIL;
    }

    // Parse first frame header to get image dimensions
    esp_eaf_format_t format = esp_eaf_header_parse(frame_data, esp_eaf_format_get_frame_size(eaf_obj->eaf_handle, 0), &eaf_obj->eaf_header);
    if (format != ESP_EAF_FORMAT_VALID) {
        LV_LOG_ERROR("Failed to parse EAF header");
        esp_eaf_format_deinit(eaf_obj->eaf_handle);
        eaf_obj->eaf_handle = NULL;
        return ESP_FAIL;
    }

    // Allocate frame buffer for RGB565 or RGB565A8 format (prefer PSRAM for large buffers)
    // Use aligned allocation for hardware JPEG decoder compatibility
    eaf_obj->color_buffer_size = eaf_obj->eaf_header.width * eaf_obj->eaf_header.height * 2;
    size_t alpha_buffer_size = eaf_obj->use_alpha ? (eaf_obj->eaf_header.width * eaf_obj->eaf_header.height) : 0;
    size_t required_size = eaf_obj->color_buffer_size + alpha_buffer_size;
    size_t allocated_size = 0;
    eaf_obj->frame_buffer = (uint8_t *)esp_eaf_malloc_aligned_prefer_psram(required_size, 0, &allocated_size);
    if (!eaf_obj->frame_buffer) {
        LV_LOG_ERROR("Failed to allocate frame buffer");
        esp_eaf_free_header(&eaf_obj->eaf_header);
        esp_eaf_format_deinit(eaf_obj->eaf_handle);
        eaf_obj->eaf_handle = NULL;
        return ESP_ERR_NO_MEM;
    }
    eaf_obj->frame_buffer_size = allocated_size;

    // Setup image descriptor
#if LVGL_VERSION_MAJOR >= 9
    eaf_obj->imgdsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    eaf_obj->imgdsc.header.flags = LV_IMAGE_FLAGS_MODIFIABLE;
    eaf_obj->imgdsc.header.cf = eaf_obj->use_alpha ? LV_COLOR_FORMAT_RGB565A8 : LV_COLOR_FORMAT_RGB565;
    eaf_obj->imgdsc.header.h = eaf_obj->eaf_header.height;
    eaf_obj->imgdsc.header.w = eaf_obj->eaf_header.width;
    eaf_obj->imgdsc.header.stride = eaf_obj->eaf_header.width * 2;
#else
    eaf_obj->imgdsc.header.always_zero = 0;
    eaf_obj->imgdsc.header.w = eaf_obj->eaf_header.width;
    eaf_obj->imgdsc.header.h = eaf_obj->eaf_header.height;
    eaf_obj->imgdsc.header.cf = eaf_obj->use_alpha ? LV_IMG_CF_RGB565A8 : LV_IMG_CF_TRUE_COLOR;
#endif
    eaf_obj->imgdsc.data = eaf_obj->frame_buffer;
    eaf_obj->imgdsc.data_size = required_size;

    // Decode first frame
    return decode_current_frame(eaf_obj);
}

static esp_err_t decode_current_frame(lv_eaf_t * eaf_obj)
{
    if (!eaf_obj->eaf_handle || !eaf_obj->frame_buffer) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t *frame_data = esp_eaf_format_get_frame_data(eaf_obj->eaf_handle, eaf_obj->current_frame);
    if (!frame_data) {
        LV_LOG_ERROR("Failed to get frame data for frame %d", eaf_obj->current_frame);
        return ESP_FAIL;
    }

    /* Free previous frame header and parse current frame header */
    esp_eaf_free_header(&eaf_obj->eaf_header);

    size_t frame_size = esp_eaf_format_get_frame_size(eaf_obj->eaf_handle, eaf_obj->current_frame);
    esp_eaf_format_t format = esp_eaf_header_parse(frame_data, frame_size, &eaf_obj->eaf_header);
    if (format != ESP_EAF_FORMAT_VALID) {
        LV_LOG_ERROR("Failed to parse header for frame %d", eaf_obj->current_frame);
        return ESP_FAIL;
    }

    /* Clear frame buffer */
    memset(eaf_obj->frame_buffer, 0, eaf_obj->frame_buffer_size);

    size_t block_stride = (size_t)eaf_obj->eaf_header.block_height * eaf_obj->eaf_header.width * 2;
    uint8_t *alpha_plane = NULL;
    if (eaf_obj->use_alpha) {
        alpha_plane = eaf_obj->frame_buffer + eaf_obj->color_buffer_size;
    }

    /* Initialize frame-level palette cache */
    esp_eaf_palette_cache_t palette_cache;
    memset(palette_cache.color, 0xFF, sizeof(palette_cache.color));

    /* Decode all blocks for this frame */
    for (int block = 0; block < eaf_obj->eaf_header.blocks; block++) {
        size_t block_offset = (size_t)block * block_stride;
        if (block_offset >= eaf_obj->color_buffer_size) {
            LV_LOG_WARN("Block %d exceeds frame buffer", block);
            break;
        }
        uint8_t *block_buffer = eaf_obj->frame_buffer + block_offset;

        esp_err_t ret = esp_eaf_block_decode(&eaf_obj->eaf_header, frame_data, block, block_buffer,
                                             alpha_plane, false, &palette_cache);
        if (ret != ESP_OK) {
            LV_LOG_WARN("Failed to decode block %d of frame %d", block, eaf_obj->current_frame);
        }
    }

    return ESP_OK;
}
