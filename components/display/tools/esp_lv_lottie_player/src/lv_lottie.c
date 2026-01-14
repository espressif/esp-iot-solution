/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "thorvg_capi.h"
#include "esp_lv_lottie.h"

#if LVGL_VERSION_MAJOR >= 9
#include "src/core/lv_obj_class_private.h"
#include "src/misc/cache/lv_cache.h"
#include "src/widgets/canvas/lv_canvas_private.h"
#endif

static const char *TAG = "lottie";

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS (&lv_lottie_class)

#if LVGL_VERSION_MAJOR >= 9
#define LV_LOTTIE_SRC_GET_TYPE(src)    lv_image_src_get_type(src)
#define LV_LOTTIE_SRC_TYPE_VARIABLE    LV_IMAGE_SRC_VARIABLE
#define LV_LOTTIE_SRC_TYPE_FILE        LV_IMAGE_SRC_FILE
#define LV_LOTTIE_OBJ_IMAGE_GET_SRC(o) lv_image_get_src(o)
#define LV_LOTTIE_CACHE_DROP(src)      lv_image_cache_drop(src)
#define LV_LOTTIE_ANIM_SET_TIME(a, t)  lv_anim_set_duration((a), (t))
#define LV_LOTTIE_MALLOC(size)         lv_malloc(size)
#define LV_LOTTIE_FREE(ptr)            lv_free(ptr)
#define LV_LOTTIE_ANIM_PAUSE(a)        lv_anim_pause(a)
#define LV_LOTTIE_ANIM_RESUME(a)       lv_anim_resume(a)
#else
#define LV_LOTTIE_SRC_GET_TYPE(src)    lv_img_src_get_type(src)
#define LV_LOTTIE_SRC_TYPE_VARIABLE    LV_IMG_SRC_VARIABLE
#define LV_LOTTIE_SRC_TYPE_FILE        LV_IMG_SRC_FILE
#define LV_LOTTIE_OBJ_IMAGE_GET_SRC(o) lv_img_get_src(o)
#define LV_LOTTIE_CACHE_DROP(src)      lv_img_cache_invalidate_src(src)
#define LV_LOTTIE_ANIM_SET_TIME(a, t)  lv_anim_set_time((a), (t))
#define LV_LOTTIE_MALLOC(size)         lv_mem_alloc(size)
#define LV_LOTTIE_FREE(ptr)            lv_mem_free(ptr)
/* LVGL v8 doesn't have lv_anim_pause/resume - these are no-ops,
 * the 'playing' flag controls behavior in lv_lottie_play/pause/stop */
#define LV_LOTTIE_ANIM_PAUSE(a)        ((void)0)
#define LV_LOTTIE_ANIM_RESUME(a)       ((void)0)
#endif

/**********************
 *      TYPEDEFS
 **********************/
typedef struct _lv_lottie_t {
    lv_canvas_t canvas;             /**< Base canvas object */
    Tvg_Paint * tvg_paint;          /**< ThorVG paint (picture) handle */
    Tvg_Canvas * tvg_canvas;        /**< ThorVG software canvas */
    Tvg_Animation * tvg_anim;       /**< ThorVG animation handle */
    lv_anim_t * anim;               /**< LVGL animation handle */

#if LVGL_VERSION_MAJOR >= 9
    lv_draw_buf_t * draw_buf;       /**< Current draw buffer pointer */
    lv_draw_buf_t * owned_draw_buf; /**< Internally allocated draw buffer */
    lv_draw_buf_t owned_draw_buf_inst; /**< Internal draw buffer wrapper */
    void * owned_draw_buf_data;     /**< Internal draw buffer data pointer */
    bool owned_draw_buf_inited;     /**< Internal draw buffer initialized flag */
    lv_draw_buf_t ext_draw_buf;     /**< External draw buffer wrapper */
    bool ext_draw_buf_inited;       /**< External draw buffer initialized flag */
#else
    lv_img_dsc_t * draw_buf;        /**< Draw buffer for LVGL v8 */
#endif

    void * buf;                     /**< Raw pixel buffer pointer */
    size_t buf_size;                /**< Buffer size in bytes */
    int32_t buf_w;                  /**< Buffer width in pixels */
    int32_t buf_h;                  /**< Buffer height in pixels */
    bool buf_owned;                 /**< True if buffer is internally allocated */

    uint8_t * src_data;             /**< Lottie JSON source data */
    size_t src_data_size;           /**< Source data size in bytes */
    bool src_data_owned;            /**< True if source data is internally allocated */

    bool loaded;                    /**< Animation loaded successfully */
    float total_frames;             /**< Total number of frames */
    uint32_t segment_start;         /**< Playback segment start frame */
    uint32_t segment_end;           /**< Playback segment end frame */
    uint32_t frame_delay_ms;        /**< Custom frame delay (0 = use animation FPS) */
    int32_t loop_count;             /**< Loop count (-1 = infinite) */
    bool loop_enabled;              /**< Loop playback enabled */
    bool playing;                   /**< Currently playing */
    int32_t current_frame;          /**< Current frame index */
} lv_lottie_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_lottie_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_lottie_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lottie_anim_exec_cb(void * var, int32_t v);
static void lottie_update(lv_lottie_t * lottie, int32_t v);
static void lottie_release_src(lv_lottie_t * lottie);
static bool lottie_load_from_file(lv_lottie_t * lottie, const char * path);
static bool lottie_set_target(lv_lottie_t * lottie, void * buf, int32_t w, int32_t h, uint32_t stride_px);
static void lottie_start_anim(lv_lottie_t * lottie);
static void lottie_reset_segment(lv_lottie_t * lottie);
static uint32_t lottie_get_frame_time(lv_lottie_t * lottie);
static bool lottie_refresh_canvas(lv_lottie_t * lottie);
static bool lottie_prepare_picture_size(lv_lottie_t * lottie);
static bool lottie_init_canvas_buffer(lv_lottie_t * lottie, int32_t w, int32_t h);
#if LVGL_VERSION_MAJOR >= 9
static lv_color_format_t lottie_get_color_format(void);
static void lottie_free_owned_draw_buf(lv_lottie_t * lottie);
#else
static lv_img_cf_t lottie_get_color_format(void);
#if LV_COLOR_DEPTH == 16
static void lottie_convert_argb8888_to_rgb565(uint8_t * buf, int32_t w, int32_t h);
#endif
#endif
static void * lottie_alloc_buf_prefer_psram(size_t size);

/**********************
 *  STATIC VARIABLES
 **********************/
static int s_tvg_ref_count = 0;

const lv_obj_class_t lv_lottie_class = {
    .constructor_cb = lv_lottie_constructor,
    .destructor_cb = lv_lottie_destructor,
    .width_def = LV_DPI_DEF,
    .height_def = LV_DPI_DEF,
    .instance_size = sizeof(lv_lottie_t),
    .base_class = &lv_canvas_class,
#if LVGL_VERSION_MAJOR >= 9
    .name = "lv_lottie",
#endif
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_lottie_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

/* Worker task related types */
typedef struct {
    lv_lottie_t * lottie;
    const void * src;
    SemaphoreHandle_t sem;
    StackType_t * stack_mem;
    StaticTask_t * tcb_mem;
    TaskHandle_t task_handle;
    bool task_is_static;
} lottie_set_src_job_t;

/*
 * PART 1: Cleanup and Preparation (MUST run in Caller Task holding LVGL Lock)
 *
 * This phase interacts with LVGL (invalidating objects, refreshing screen).
 * It CANNOT run in the Worker Task because the Caller Task holds the Lock, leading to Deadlock.
 */
static void lottie_cleanup_and_prepare(lv_lottie_t * lottie)
{
    /* 1. Sync outstanding tasks */
    if (lottie->tvg_canvas) {
        tvg_canvas_sync(lottie->tvg_canvas);
    }

    /* 2. Clear Buffer and Screen */
    /* This ensures no residue remains from the previous animation. */
    lottie_refresh_canvas(lottie);

    /* 3. Destroy ThorVG Data (NUCLEAR OPTION)
       We MUST destroy the canvas to clear internal SW engine caches (compositors).
       Tv_canvas_clear() is NOT sufficient to prevent ghosting/residue from previous animations. */
    if (lottie->tvg_paint) {
        tvg_animation_del(lottie->tvg_anim);
        lottie->tvg_anim = NULL;
        lottie->tvg_paint = NULL;
    }

    if (lottie->tvg_canvas) {
        tvg_canvas_destroy(lottie->tvg_canvas);
        lottie->tvg_canvas = NULL;
    }
}

/*
 * PART 2: Heavy Initialization (MUST run in Worker Task with Large Stack)
 *
 * This phase handles ThorVG object creation (if needed), JSON parsing, and initial setup.
 * It does NOT call LVGL APIs that require the lock.
 */
static bool lottie_load_heavy(lv_lottie_t * lottie, const void * src)
{
    /* 1. Create Canvas (Nuclear Re-init) */
    /* Since we destroyed it in Phase 1, we must recreate it here. */
    lottie->tvg_canvas = tvg_swcanvas_create();
    if (lottie->tvg_canvas == NULL) {
        ESP_LOGE(TAG, "Failed to create ThorVG canvas");
        return false;
    }
    tvg_swcanvas_set_mempool(lottie->tvg_canvas, TVG_MEMPOOL_POLICY_DEFAULT);

    /* 2. Create Animation */
    lottie->tvg_anim = tvg_animation_new();
    if (lottie->tvg_anim == NULL) {
        ESP_LOGE(TAG, "Failed to create ThorVG animation");
        return false;
    }

    lottie->tvg_paint = tvg_animation_get_picture(lottie->tvg_anim);
    if (lottie->tvg_paint == NULL) {
        ESP_LOGE(TAG, "Failed to get ThorVG picture");
        return false;
    }

    /* 3. Load Source Data */
    if (LV_LOTTIE_SRC_GET_TYPE(src) == LV_LOTTIE_SRC_TYPE_VARIABLE) {
        const lv_lottie_src_dsc_t * dsc = (const lv_lottie_src_dsc_t *)src;
        if (dsc->data == NULL || dsc->data_size == 0) {
            ESP_LOGE(TAG, "Invalid Lottie data descriptor");
            return false;
        }
        lottie->src_data = (uint8_t *)dsc->data;
        lottie->src_data_size = dsc->data_size;
        lottie->src_data_owned = false;
    } else if (LV_LOTTIE_SRC_GET_TYPE(src) == LV_LOTTIE_SRC_TYPE_FILE) {
        const char * path = (const char *)src;
        if (!lottie_load_from_file(lottie, path)) {
            return false;
        }
    } else {
        ESP_LOGE(TAG, "Unsupported source type");
        return false;
    }

    /* 4. Parse JSON */
    Tvg_Result load_res = tvg_picture_load_data(lottie->tvg_paint, (const char *)lottie->src_data,
                                                (uint32_t)lottie->src_data_size, "lottie", false);
    if (load_res != TVG_RESULT_SUCCESS) {
        ESP_LOGE(TAG, "Failed to load Lottie data (err=%d)", load_res);
        lottie_release_src(lottie);
        return false;
    }

    /* 5. Get Metadata */
    float total = 0.0f;
    tvg_animation_get_total_frame(lottie->tvg_anim, &total);
    if (total <= 0.0f) {
        ESP_LOGE(TAG, "Invalid total frame count");
        lottie_release_src(lottie);
        return false;
    }

    lottie->total_frames = total;
    lottie->loaded = true;

    /* 6. Initial Bind (No Drawing yet) */
    /* Only bind target if buffer is ready. Drawing happens in lottie_update() later. */
    if (lottie->buf_w > 0 && lottie->buf_h > 0 && lottie->buf) {
        uint32_t stride = (uint32_t)lottie->buf_w;
#if LVGL_VERSION_MAJOR >= 9
        if (lottie->draw_buf) {
            stride = lottie->draw_buf->header.stride / 4;
        }
#endif
        lottie_set_target(lottie, lottie->buf, lottie->buf_w, lottie->buf_h, stride);
    }

    return true;
}

static void lottie_set_src_impl_worker_body(lv_lottie_t * lottie, const void * src)
{
    /* Heavy Phase: Run in Worker Task */
    if (!lottie_load_heavy(lottie, src)) {
        return;
    }

    if (!lottie_prepare_picture_size(lottie)) {
        ESP_LOGW(TAG, "Failed to set picture size");
    }

    lottie_reset_segment(lottie);
    lottie_start_anim(lottie);

    /* Initial update: This runs ThorVG rasterizer (Stack heavy).
     * Safe here because we are in the Worker Task. */
    lottie_update(lottie, (int32_t)lottie->segment_start);
}

static void lottie_set_src_worker(void * arg)
{
    lottie_set_src_job_t * job = (lottie_set_src_job_t *)arg;
    lottie_set_src_impl_worker_body(job->lottie, job->src);
    xSemaphoreGive(job->sem);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

static TaskHandle_t lottie_create_worker_task(lottie_set_src_job_t * job, uint32_t stack_depth, UBaseType_t priority)
{
    job->task_is_static = false;
    job->stack_mem = NULL;
    job->tcb_mem = NULL;
    job->task_handle = NULL;

    job->stack_mem = (StackType_t *)heap_caps_malloc((size_t)stack_depth * sizeof(StackType_t),
                                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (job->stack_mem) {
        job->tcb_mem = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t),
                                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (job->tcb_mem) {
            job->task_handle = xTaskCreateStatic(lottie_set_src_worker, "lottie_load",
                                                 stack_depth, job, priority,
                                                 job->stack_mem, job->tcb_mem);
            if (job->task_handle) {
                job->task_is_static = true;
                return job->task_handle;
            }
        }
    }

    if (job->stack_mem) {
        heap_caps_free(job->stack_mem);
        job->stack_mem = NULL;
    }
    if (job->tcb_mem) {
        heap_caps_free(job->tcb_mem);
        job->tcb_mem = NULL;
    }

    if (xTaskCreate(lottie_set_src_worker, "lottie_load", stack_depth, job, priority, &job->task_handle) == pdPASS) {
        return job->task_handle;
    }

    return NULL;
}

void lv_lottie_set_src(lv_obj_t * obj, const void * src)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (src == NULL) {
        ESP_LOGE(TAG, "Source is NULL");
        return;
    }

    lv_lottie_t * lottie = (lv_lottie_t *)obj;

    /* PHASE 1: Safe Cleanup (Run in Caller Context)
     * Handles LVGL Lock-sensitive operations like invalidating objects and refreshing screen (lv_refr_now).
     */
    lottie_cleanup_and_prepare(lottie);

    /* PHASE 2: Heavy Initialization (Run in Worker Task)
     * Offload ThorVG parsing and initial rasterization to avoid Stack Overflow.
     */
    lottie_set_src_job_t job = {
        .lottie = lottie,
        .src = src,
        .sem = xSemaphoreCreateBinary(),
    };

    if (job.sem == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return;
    }

    /* Spawn worker task with sufficient stack (32KB), prefer PSRAM stack if allowed */
    if (lottie_create_worker_task(&job, 32 * 1024, uxTaskPriorityGet(NULL)) == NULL) {
        ESP_LOGE(TAG, "Failed to create worker task");
        vSemaphoreDelete(job.sem);
        return;
    }

    /* Wait for completion synchronously */
    xSemaphoreTake(job.sem, portMAX_DELAY);
    vSemaphoreDelete(job.sem);

    if (job.task_handle) {
        vTaskDelete(job.task_handle);
    }

    if (job.task_is_static) {
        if (job.stack_mem) {
            heap_caps_free(job.stack_mem);
        }
        if (job.tcb_mem) {
            heap_caps_free(job.tcb_mem);
        }
    }
}

void lv_lottie_set_src_data(lv_obj_t * obj, void * data, size_t len)
{
    if (data == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid Lottie data or length");
        return;
    }

    lv_lottie_src_dsc_t dsc;
    memset(&dsc, 0, sizeof(dsc));
#if LVGL_VERSION_MAJOR >= 9
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_RAW;
    dsc.header.flags = 0;
    dsc.header.w = 1;
    dsc.header.h = 1;
    dsc.header.stride = 0;
    dsc.header.reserved_2 = 0;
#else
    dsc.header.always_zero = 0;
    dsc.header.cf = LV_IMG_CF_RAW;
    dsc.header.w = 1;
    dsc.header.h = 1;
#endif
    dsc.data_size = len;
    dsc.data = data;

    lv_lottie_set_src(obj, &dsc);
}

void lv_lottie_set_size(lv_obj_t * obj, int32_t w, int32_t h)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (w <= 0 || h <= 0) {
        ESP_LOGE(TAG, "Invalid size: %d x %d", (int)w, (int)h);
        return;
    }

    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    if (!lottie_init_canvas_buffer(lottie, w, h)) {
        return;
    }

    if (!lottie_prepare_picture_size(lottie)) {
        ESP_LOGW(TAG, "Failed to set picture size");
    }

    if (lottie->loaded) {
        lottie_update(lottie, lottie->current_frame);
    }
}

void lv_lottie_set_buffer(lv_obj_t * obj, void * buf, int32_t w, int32_t h)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (buf == NULL || w <= 0 || h <= 0) {
        ESP_LOGE(TAG, "Invalid buffer or size");
        return;
    }

    lv_lottie_t * lottie = (lv_lottie_t *)obj;

#if LVGL_VERSION_MAJOR >= 9
    uint32_t stride = lv_draw_buf_width_to_stride(w, lottie_get_color_format());
    uint32_t required_size = stride * (uint32_t)h;

    lottie_free_owned_draw_buf(lottie);

    lottie->draw_buf = NULL;
    lottie->ext_draw_buf_inited = false;
    if (lv_draw_buf_init(&lottie->ext_draw_buf, w, h, lottie_get_color_format(), stride, buf, required_size) != LV_RESULT_OK) {
        ESP_LOGE(TAG, "Failed to init draw buffer");
        return;
    }
    lottie->ext_draw_buf_inited = true;
    lottie->draw_buf = &lottie->ext_draw_buf;
    lv_canvas_set_draw_buf(obj, lottie->draw_buf);
    lv_draw_buf_set_flag(lottie->draw_buf, LV_IMAGE_FLAGS_PREMULTIPLIED);

    lottie->buf = buf;
    lottie->buf_size = required_size;
    lottie->buf_w = w;
    lottie->buf_h = h;
    lottie->buf_owned = false;

    if (!lottie_set_target(lottie, lottie->draw_buf->data, w, h, lottie->draw_buf->header.stride / 4)) {
        return;
    }
#else
    if (lottie->buf_owned && lottie->buf) {
        heap_caps_free(lottie->buf);
    }
    lottie->buf = buf;
    lottie->buf_owned = false;
    lottie->buf_w = w;
    lottie->buf_h = h;
    lottie->buf_size = (size_t)w * (size_t)h * 4;

    lv_canvas_set_buffer(obj, buf, w, h, (lv_img_cf_t)lottie_get_color_format());
    if (!lottie_set_target(lottie, buf, w, h, (uint32_t)w)) {
        return;
    }
#endif

    if (!lottie_prepare_picture_size(lottie)) {
        ESP_LOGW(TAG, "Failed to set picture size");
    }

    if (lottie->loaded) {
        lottie_update(lottie, lottie->current_frame);
    }
}

void lv_lottie_set_draw_buf(lv_obj_t * obj, lv_lottie_draw_buf_t * draw_buf)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (draw_buf == NULL) {
        ESP_LOGE(TAG, "Draw buffer is NULL");
        return;
    }

    lv_lottie_t * lottie = (lv_lottie_t *)obj;

#if LVGL_VERSION_MAJOR >= 9
    if (draw_buf->header.cf != LV_COLOR_FORMAT_ARGB8888 &&
            draw_buf->header.cf != LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED) {
        ESP_LOGE(TAG, "Draw buffer format must be ARGB8888 or ARGB8888_PREMULTIPLIED");
        return;
    }

    lottie_free_owned_draw_buf(lottie);

    lottie->draw_buf = draw_buf;
    lottie->ext_draw_buf_inited = false;
    lv_canvas_set_draw_buf(obj, lottie->draw_buf);
    lv_draw_buf_set_flag(lottie->draw_buf, LV_IMAGE_FLAGS_PREMULTIPLIED);

    lottie->buf = draw_buf->data;
    lottie->buf_size = draw_buf->data_size;
    lottie->buf_w = draw_buf->header.w;
    lottie->buf_h = draw_buf->header.h;
    lottie->buf_owned = false;

    if (!lottie_set_target(lottie, draw_buf->data, draw_buf->header.w, draw_buf->header.h,
                           draw_buf->header.stride / 4)) {
        return;
    }
#else
    if (draw_buf->header.cf != LV_IMG_CF_TRUE_COLOR && draw_buf->header.cf != LV_IMG_CF_TRUE_COLOR_ALPHA) {
        ESP_LOGE(TAG, "Draw buffer format must be TRUE_COLOR or TRUE_COLOR_ALPHA");
        return;
    }

    lottie->draw_buf = draw_buf;
    lottie->buf = (void *)draw_buf->data;
    lottie->buf_size = draw_buf->data_size;
    lottie->buf_w = draw_buf->header.w;
    lottie->buf_h = draw_buf->header.h;
    lottie->buf_owned = false;

    lv_canvas_set_buffer(obj, (void *)draw_buf->data, draw_buf->header.w, draw_buf->header.h,
                         (lv_img_cf_t)lottie_get_color_format());
    if (!lottie_set_target(lottie, (void *)draw_buf->data, draw_buf->header.w, draw_buf->header.h,
                           (uint32_t)draw_buf->header.w)) {
        return;
    }
#endif

    if (!lottie_prepare_picture_size(lottie)) {
        ESP_LOGW(TAG, "Failed to set picture size");
    }

    if (lottie->loaded) {
        lottie_update(lottie, lottie->current_frame);
    }
}

bool lv_lottie_is_loaded(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    return lottie->loaded;
}

int32_t lv_lottie_get_total_frames(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    if (!lottie->loaded) {
        return -1;
    }
    return (int32_t)lottie->total_frames;
}

int32_t lv_lottie_get_current_frame(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    if (!lottie->loaded) {
        return -1;
    }
    return lottie->current_frame;
}

void lv_lottie_set_frame_delay(lv_obj_t * obj, uint32_t delay)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    lottie->frame_delay_ms = delay;
    if (lottie->loaded) {
        lottie_start_anim(lottie);
    }
}

uint32_t lv_lottie_get_frame_delay(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    return lottie->frame_delay_ms;
}

void lv_lottie_set_segment(lv_obj_t * obj, uint32_t begin, uint32_t end)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    if (!lottie->loaded) {
        ESP_LOGW(TAG, "No animation loaded");
        return;
    }

    uint32_t total = (uint32_t)lottie->total_frames;
    if (total == 0) {
        ESP_LOGW(TAG, "Invalid total frame count");
        return;
    }

    if (begin >= total) {
        begin = total - 1;
    }
    if (end >= total) {
        end = total - 1;
    }
    if (begin > end) {
        uint32_t tmp = begin;
        begin = end;
        end = tmp;
    }

    lottie->segment_start = begin;
    lottie->segment_end = end;
    lottie_start_anim(lottie);
}

void lv_lottie_get_segment(lv_obj_t * obj, uint32_t * begin, uint32_t * end)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    if (begin) {
        *begin = lottie->segment_start;
    }
    if (end) {
        *end = lottie->segment_end;
    }
}

void lv_lottie_set_loop_count(lv_obj_t * obj, int32_t count)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    if (count == 0) {
        count = 1;
    }
    lottie->loop_count = count;
    if (lottie->loaded) {
        lottie_start_anim(lottie);
    }
}

int32_t lv_lottie_get_loop_count(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    return lottie->loop_count;
}

void lv_lottie_set_loop_enabled(lv_obj_t * obj, bool enable)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    lottie->loop_enabled = enable;
    if (lottie->loaded) {
        lottie_start_anim(lottie);
    }
}

bool lv_lottie_get_loop_enabled(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    return lottie->loop_enabled;
}

void lv_lottie_play(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    if (!lottie->loaded) {
        return;
    }

    if (lottie->anim == NULL) {
        lottie_start_anim(lottie);
    } else {
        LV_LOTTIE_ANIM_RESUME(lottie->anim);
    }
    lottie->playing = true;
}

void lv_lottie_pause(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    if (lottie->anim) {
        LV_LOTTIE_ANIM_PAUSE(lottie->anim);
    }
    lottie->playing = false;
}

void lv_lottie_stop(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    if (!lottie->loaded) {
        return;
    }

    if (lottie->anim) {
        LV_LOTTIE_ANIM_PAUSE(lottie->anim);
    }
    lottie->playing = false;
    lottie->current_frame = (int32_t)lottie->segment_start;
    lottie_update(lottie, (int32_t)lottie->segment_start);
}

void lv_lottie_restart(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    if (!lottie->loaded) {
        return;
    }

    lottie_start_anim(lottie);
    lottie->playing = true;
}

lv_anim_t * lv_lottie_get_anim(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    return lottie->anim;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_lottie_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    lv_lottie_t * lottie = (lv_lottie_t *)obj;

    /* Initialize all pointers to NULL for safe cleanup */
    lottie->tvg_canvas = NULL;
    lottie->tvg_anim = NULL;
    lottie->tvg_paint = NULL;
    lottie->anim = NULL;
#if LVGL_VERSION_MAJOR >= 9
    lottie->draw_buf = NULL;
    lottie->owned_draw_buf = NULL;
    lottie->owned_draw_buf_data = NULL;
    lottie->owned_draw_buf_inited = false;
    lottie->ext_draw_buf_inited = false;
#else
    lottie->draw_buf = NULL;
#endif
    lottie->buf = NULL;
    lottie->buf_size = 0;
    lottie->buf_w = 0;
    lottie->buf_h = 0;
    lottie->buf_owned = false;
    lottie->src_data = NULL;
    lottie->src_data_size = 0;
    lottie->src_data_owned = false;
    lottie->loaded = false;
    lottie->total_frames = 0;
    lottie->segment_start = 0;
    lottie->segment_end = 0;
    lottie->frame_delay_ms = 0;
    lottie->loop_count = -1;
    lottie->loop_enabled = true;
    lottie->playing = true;
    lottie->current_frame = 0;

    /* Initialize ThorVG engine (reference counted) */
    if (s_tvg_ref_count == 0) {
        Tvg_Result res = tvg_engine_init(TVG_ENGINE_SW, 0);
        if (res != TVG_RESULT_SUCCESS) {
            ESP_LOGE(TAG, "Failed to init ThorVG engine (err=%d)", res);
            return;
        }
    }
    s_tvg_ref_count++;

    /* Create ThorVG canvas */
    lottie->tvg_canvas = tvg_swcanvas_create();
    if (lottie->tvg_canvas == NULL) {
        ESP_LOGE(TAG, "Failed to create ThorVG canvas");
        goto cleanup;
    }

    /* Create ThorVG animation */
    lottie->tvg_anim = tvg_animation_new();
    if (lottie->tvg_anim == NULL) {
        ESP_LOGE(TAG, "Failed to create ThorVG animation");
        goto cleanup;
    }

    /* Get picture from animation (will be pushed to canvas after target is set) */
    lottie->tvg_paint = tvg_animation_get_picture(lottie->tvg_anim);
    if (lottie->tvg_paint == NULL) {
        ESP_LOGE(TAG, "Failed to get ThorVG picture");
        goto cleanup;
    }

    LV_TRACE_OBJ_CREATE("finished");
    return;

cleanup:
    /* Clean up on failure */
    if (lottie->tvg_anim) {
        tvg_animation_del(lottie->tvg_anim);
        lottie->tvg_anim = NULL;
        lottie->tvg_paint = NULL;
    }
    if (lottie->tvg_canvas) {
        tvg_canvas_destroy(lottie->tvg_canvas);
        lottie->tvg_canvas = NULL;
    }
    s_tvg_ref_count--;
    if (s_tvg_ref_count == 0) {
        tvg_engine_term(TVG_ENGINE_SW);
    }
}

static void lv_lottie_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;

    if (lottie->anim) {
        lv_anim_del((void *)lottie, lottie_anim_exec_cb);
        lottie->anim = NULL;
    }

    lottie_release_src(lottie);

#if LVGL_VERSION_MAJOR >= 9
    lottie_free_owned_draw_buf(lottie);
#endif

    if (lottie->buf_owned && lottie->buf) {
        heap_caps_free(lottie->buf);
        lottie->buf = NULL;
    }

    if (lottie->tvg_anim) {
        tvg_animation_del(lottie->tvg_anim);
        lottie->tvg_anim = NULL;
    }
    if (lottie->tvg_canvas) {
        tvg_canvas_destroy(lottie->tvg_canvas);
        lottie->tvg_canvas = NULL;
    }

    if (s_tvg_ref_count > 0) {
        s_tvg_ref_count--;
        if (s_tvg_ref_count == 0) {
            tvg_engine_term(TVG_ENGINE_SW);
        }
    }
}

static void lottie_anim_exec_cb(void * var, int32_t v)
{
    lv_lottie_t * lottie = var;

    if (!lv_obj_is_visible((lv_obj_t *)var)) {
        return;
    }

#if LVGL_VERSION_MAJOR < 9
    /* In LVGL v8, we don't have lv_anim_pause/resume, so we check the playing
     * flag here to skip updates when paused. The animation timer still runs,
     * but we simply don't render new frames. */
    if (!lottie->playing) {
        return;
    }
#endif

    lottie_update(lottie, v);
}

static void lottie_update(lv_lottie_t * lottie, int32_t v)
{
    if (!lottie->loaded || lottie->tvg_anim == NULL || lottie->tvg_canvas == NULL || lottie->buf == NULL) {
        return;
    }

    if (!lottie_refresh_canvas(lottie)) {
        return;
    }

    int32_t frame = v;
    if (frame < (int32_t)lottie->segment_start) {
        frame = (int32_t)lottie->segment_start;
    } else if (frame > (int32_t)lottie->segment_end) {
        frame = (int32_t)lottie->segment_end;
    }
    lottie->current_frame = frame;

    tvg_animation_set_frame(lottie->tvg_anim, (float)frame);
    tvg_canvas_update(lottie->tvg_canvas);
    tvg_canvas_draw(lottie->tvg_canvas);
    tvg_canvas_sync(lottie->tvg_canvas);

#if LVGL_VERSION_MAJOR < 9 && LV_COLOR_DEPTH == 16
    /* ThorVG renders ARGB8888, convert to RGB565 in-place for 16-bit displays */
    lottie_convert_argb8888_to_rgb565(lottie->buf, lottie->buf_w, lottie->buf_h);
#endif

    LV_LOTTIE_CACHE_DROP(LV_LOTTIE_OBJ_IMAGE_GET_SRC((lv_obj_t *)lottie));
    lv_obj_invalidate((lv_obj_t *)lottie);
}

static void lottie_release_src(lv_lottie_t * lottie)
{
    lottie->loaded = false;
    lottie->total_frames = 0;
    lottie->segment_start = 0;
    lottie->segment_end = 0;

    if (lottie->src_data && lottie->src_data_owned) {
        LV_LOTTIE_FREE(lottie->src_data);
    }
    lottie->src_data = NULL;
    lottie->src_data_size = 0;
    lottie->src_data_owned = false;
}

static bool lottie_load_from_file(lv_lottie_t * lottie, const char * path)
{
    if (path == NULL || path[0] == '\0') {
        ESP_LOGE(TAG, "Invalid file path");
        return false;
    }

    lv_fs_file_t f;
    lv_fs_res_t res = lv_fs_open(&f, path, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        return false;
    }

    uint32_t file_size = 0;
    res = lv_fs_seek(&f, 0, LV_FS_SEEK_END);
    if (res != LV_FS_RES_OK) {
        ESP_LOGE(TAG, "Failed to seek file end: %s", path);
        lv_fs_close(&f);
        return false;
    }
    res = lv_fs_tell(&f, &file_size);
    if (res != LV_FS_RES_OK || file_size == 0) {
        ESP_LOGE(TAG, "Failed to get file size: %s", path);
        lv_fs_close(&f);
        return false;
    }
    res = lv_fs_seek(&f, 0, LV_FS_SEEK_SET);
    if (res != LV_FS_RES_OK) {
        ESP_LOGE(TAG, "Failed to rewind file: %s", path);
        lv_fs_close(&f);
        return false;
    }

    /* Allocate buffer with extra byte for null terminator (required by ThorVG) */
    uint8_t * data = LV_LOTTIE_MALLOC(file_size + 1);
    if (data == NULL) {
        ESP_LOGE(TAG, "No memory for Lottie file");
        lv_fs_close(&f);
        return false;
    }

    uint32_t read_size = 0;
    res = lv_fs_read(&f, data, file_size, &read_size);
    lv_fs_close(&f);
    if (res != LV_FS_RES_OK || read_size != file_size) {
        ESP_LOGE(TAG, "Failed to read file: %s", path);
        LV_LOTTIE_FREE(data);
        return false;
    }

    /* Null-terminate the JSON data (required by ThorVG when copy=false) */
    data[file_size] = '\0';

    lottie->src_data = data;
    lottie->src_data_size = file_size;
    lottie->src_data_owned = true;
    return true;
}

static bool lottie_set_target(lv_lottie_t * lottie, void * buf, int32_t w, int32_t h, uint32_t stride_px)
{
    if (lottie->tvg_canvas == NULL || lottie->tvg_paint == NULL) {
        ESP_LOGE(TAG, "Canvas or paint is NULL");
        return false;
    }

    if (buf == NULL || w <= 0 || h <= 0 || stride_px == 0) {
        ESP_LOGE(TAG, "Invalid target params: buf=%p, w=%d, h=%d, stride=%u", buf, (int)w, (int)h, (unsigned)stride_px);
        return false;
    }

    Tvg_Result res = tvg_swcanvas_set_target(lottie->tvg_canvas, buf, stride_px, w, h, TVG_COLORSPACE_ARGB8888);
    if (res != TVG_RESULT_SUCCESS) {
        ESP_LOGE(TAG, "Failed to set ThorVG target (err=%d, buf=%p, stride=%u, w=%d, h=%d)",
                 res, buf, (unsigned)stride_px, (int)w, (int)h);
        return false;
    }

    tvg_canvas_clear(lottie->tvg_canvas, false);
    if (lottie->tvg_paint) {
        tvg_canvas_push(lottie->tvg_canvas, lottie->tvg_paint);
    }

    return true;
}

static void lottie_start_anim(lv_lottie_t * lottie)
{
    if (!lottie->loaded) {
        return;
    }

    if (lottie->segment_end < lottie->segment_start) {
        return;
    }

    if (lottie->anim) {
        lv_anim_del((void *)lottie, lottie_anim_exec_cb);
        lottie->anim = NULL;
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, lottie_anim_exec_cb);
    lv_anim_set_var(&a, (void *)lottie);
    lv_anim_set_values(&a, (int32_t)lottie->segment_start, (int32_t)lottie->segment_end);
    LV_LOTTIE_ANIM_SET_TIME(&a, (uint32_t)((lottie->segment_end - lottie->segment_start + 1) * lottie_get_frame_time(lottie)));

    if (!lottie->loop_enabled) {
        lv_anim_set_repeat_count(&a, 0);
    } else if (lottie->loop_count < 0) {
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    } else if (lottie->loop_count <= 1) {
        lv_anim_set_repeat_count(&a, 0);
    } else {
        lv_anim_set_repeat_count(&a, (uint32_t)(lottie->loop_count - 1));
    }

    lottie->anim = lv_anim_start(&a);
    lottie->current_frame = (int32_t)lottie->segment_start;
    if (!lottie->playing) {
        LV_LOTTIE_ANIM_PAUSE(lottie->anim);
    }
}

static void lottie_reset_segment(lv_lottie_t * lottie)
{
    if (lottie->total_frames <= 0.0f) {
        lottie->segment_start = 0;
        lottie->segment_end = 0;
        return;
    }

    lottie->segment_start = 0;
    lottie->segment_end = (uint32_t)lottie->total_frames - 1;
}

static uint32_t lottie_get_frame_time(lv_lottie_t * lottie)
{
    if (lottie->frame_delay_ms > 0) {
        return lottie->frame_delay_ms;
    }

    float duration = 0.0f;
    tvg_animation_get_duration(lottie->tvg_anim, &duration);
    if (duration > 0.0f && lottie->total_frames > 0.0f) {
        uint32_t calc = (uint32_t)((duration * 1000.0f) / lottie->total_frames);
        return calc > 0 ? calc : 16;
    }

    return 16; /* Default to ~60 FPS */
}

static bool lottie_refresh_canvas(lv_lottie_t * lottie)
{
    /* Clear raw pixel buffer directly to ensure old frame data is completely removed.
     * Using memset on the raw buffer is more reliable than lv_draw_buf_clear
     * which may not zero out all pixel data in some LVGL versions. */
    if (lottie->buf && lottie->buf_size > 0) {
        memset(lottie->buf, 0, lottie->buf_size);
    }

    return true;
}

static bool lottie_prepare_picture_size(lv_lottie_t * lottie)
{
    if (lottie->tvg_paint == NULL) {
        return false;
    }

    if (lottie->buf_w > 0 && lottie->buf_h > 0) {
        tvg_picture_set_size(lottie->tvg_paint, lottie->buf_w, lottie->buf_h);
        return true;
    }

    float w = 0;
    float h = 0;
    if (tvg_picture_get_size(lottie->tvg_paint, &w, &h) != TVG_RESULT_SUCCESS) {
        return false;
    }

    if (w <= 0 || h <= 0) {
        return false;
    }

    return lottie_init_canvas_buffer(lottie, (int32_t)w, (int32_t)h);
}

static bool lottie_init_canvas_buffer(lv_lottie_t * lottie, int32_t w, int32_t h)
{
#if LVGL_VERSION_MAJOR >= 9
    lottie_free_owned_draw_buf(lottie);

    uint32_t stride = lv_draw_buf_width_to_stride(w, lottie_get_color_format());
    uint32_t required_size = stride * (uint32_t)h;
    void * data = lottie_alloc_buf_prefer_psram(required_size);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate draw buffer");
        return false;
    }

    if (lv_draw_buf_init(&lottie->owned_draw_buf_inst, w, h, lottie_get_color_format(),
                         stride, data, required_size) != LV_RESULT_OK) {
        ESP_LOGE(TAG, "Failed to init draw buffer");
        heap_caps_free(data);
        return false;
    }

    lottie->owned_draw_buf = &lottie->owned_draw_buf_inst;
    lottie->owned_draw_buf_data = data;
    lottie->owned_draw_buf_inited = true;
    lottie->draw_buf = lottie->owned_draw_buf;
    lottie->ext_draw_buf_inited = false;
    lv_canvas_set_draw_buf((lv_obj_t *)lottie, lottie->draw_buf);
    lv_draw_buf_set_flag(lottie->draw_buf, LV_IMAGE_FLAGS_PREMULTIPLIED);

    lottie->buf = lottie->draw_buf->data;
    lottie->buf_size = lottie->draw_buf->data_size;
    lottie->buf_w = w;
    lottie->buf_h = h;
    lottie->buf_owned = false;

    return lottie_set_target(lottie, lottie->draw_buf->data, w, h, lottie->draw_buf->header.stride / 4);
#else
    size_t size = (size_t)w * (size_t)h * 4;
    void * new_buf = lottie_alloc_buf_prefer_psram(size);
    if (new_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return false;
    }

    if (lottie->buf_owned && lottie->buf) {
        heap_caps_free(lottie->buf);
    }

    lottie->buf = new_buf;
    lottie->buf_size = size;
    lottie->buf_w = w;
    lottie->buf_h = h;
    lottie->buf_owned = true;

    lv_canvas_set_buffer((lv_obj_t *)lottie, new_buf, w, h, (lv_img_cf_t)lottie_get_color_format());
    return lottie_set_target(lottie, new_buf, w, h, (uint32_t)w);
#endif
}

#if LVGL_VERSION_MAJOR >= 9
static lv_color_format_t lottie_get_color_format(void)
{
    return LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED;
}
#else
static lv_img_cf_t lottie_get_color_format(void)
{
    /* Use TRUE_COLOR (not TRUE_COLOR_ALPHA) because ThorVG outputs
     * premultiplied ARGB8888. Using TRUE_COLOR_ALPHA would cause LVGL
     * to apply alpha blending again, resulting in ghosting artifacts. */
    return LV_IMG_CF_TRUE_COLOR;
}
#endif

#if LVGL_VERSION_MAJOR >= 9
static void lottie_free_owned_draw_buf(lv_lottie_t * lottie)
{
    if (!lottie->owned_draw_buf_inited) {
        return;
    }

    if (lottie->owned_draw_buf_data) {
        heap_caps_free(lottie->owned_draw_buf_data);
        lottie->owned_draw_buf_data = NULL;
    }

    lottie->owned_draw_buf_inited = false;
    lottie->owned_draw_buf = NULL;
}
#endif

static void * lottie_alloc_buf_prefer_psram(size_t size)
{
    void * buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf) {
        return buf;
    }

    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

#if LVGL_VERSION_MAJOR < 9 && LV_COLOR_DEPTH == 16
/**
 * Convert ARGB8888 buffer to RGB565 in-place.
 * ThorVG always renders to ARGB8888, but LVGL v8 with 16-bit color depth
 * expects RGB565 format. This function converts the buffer in-place,
 * overwriting the 32-bit pixels with 16-bit values.
 */
static void lottie_convert_argb8888_to_rgb565(uint8_t * buf, int32_t w, int32_t h)
{
    uint16_t * out = (uint16_t *)buf;
    int32_t num_pixels = w * h;

    for (int32_t i = 0; i < num_pixels; i++) {
        /* ARGB8888 layout: [B, G, R, A] in memory (little endian) */
        uint8_t b = buf[i * 4 + 0];
        uint8_t g = buf[i * 4 + 1];
        uint8_t r = buf[i * 4 + 2];
        /* Alpha is ignored for TRUE_COLOR format */

        /* Convert to RGB565: RRRRRGGG GGGBBBBB */
        uint16_t r565 = (r >> 3) & 0x1F;
        uint16_t g565 = (g >> 2) & 0x3F;
        uint16_t b565 = (b >> 3) & 0x1F;

        uint16_t rgb565 = (r565 << 11) | (g565 << 5) | b565;

#if LV_COLOR_16_SWAP
        /* Swap bytes for big-endian displays */
        rgb565 = ((rgb565 >> 8) | ((rgb565 & 0xFF) << 8));
#endif

        out[i] = rgb565;
    }
}
#endif
