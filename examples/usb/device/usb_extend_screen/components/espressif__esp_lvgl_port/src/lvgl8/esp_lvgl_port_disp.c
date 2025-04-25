/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "esp_lvgl_port_priv.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#if CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_lcd_panel_rgb.h"
#endif

#if (CONFIG_IDF_TARGET_ESP32P4 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))
#include "esp_lcd_mipi_dsi.h"
#endif

#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 4)) || (ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 0, 0))
#define LVGL_PORT_HANDLE_FLUSH_READY 0
#else
#define LVGL_PORT_HANDLE_FLUSH_READY 1
#endif

static const char *TAG = "LVGL";

/*******************************************************************************
* Types definitions
*******************************************************************************/

typedef struct {
    lvgl_port_disp_type_t     disp_type;    /* Display type */
    esp_lcd_panel_io_handle_t io_handle;    /* LCD panel IO handle */
    esp_lcd_panel_handle_t    panel_handle; /* LCD panel handle */
    esp_lcd_panel_handle_t    control_handle; /* LCD panel control handle */
    lvgl_port_rotation_cfg_t  rotation;     /* Default values of the screen rotation */
    lv_disp_drv_t             disp_drv;     /* LVGL display driver */
    lv_color_t                *trans_buf;   /* Buffer send to driver */
    uint32_t                  trans_size;   /* Maximum size for one transport */
    SemaphoreHandle_t         trans_sem;    /* Idle transfer mutex */
} lvgl_port_display_ctx_t;

/*******************************************************************************
* Function definitions
*******************************************************************************/
static lv_disp_t *lvgl_port_add_disp_priv(const lvgl_port_display_cfg_t *disp_cfg, const lvgl_port_disp_priv_cfg_t *priv_cfg);
static lvgl_port_display_ctx_t *lvgl_port_get_display_ctx(lv_disp_t *disp);
#if LVGL_PORT_HANDLE_FLUSH_READY
static bool lvgl_port_flush_io_ready_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
#if CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static bool lvgl_port_flush_vsync_ready_callback(esp_lcd_panel_handle_t panel_io, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx);
#endif
#if (CONFIG_IDF_TARGET_ESP32P4 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))
static bool lvgl_port_flush_panel_ready_callback(esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx);
#endif
#endif
static void lvgl_port_flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
static void lvgl_port_update_callback(lv_disp_drv_t *drv);
static void lvgl_port_pix_monochrome_callback(lv_disp_drv_t *drv, uint8_t *buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y, lv_color_t color, lv_opa_t opa);

/*******************************************************************************
* Public API functions
*******************************************************************************/

lv_disp_t *lvgl_port_add_disp(const lvgl_port_display_cfg_t *disp_cfg)
{
    lv_disp_t *disp = lvgl_port_add_disp_priv(disp_cfg, NULL);

    if (disp != NULL) {
        lvgl_port_display_ctx_t *disp_ctx = lvgl_port_get_display_ctx(disp);
        /* Set display type */
        disp_ctx->disp_type = LVGL_PORT_DISP_TYPE_OTHER;

        assert(disp_ctx->io_handle != NULL);

#if LVGL_PORT_HANDLE_FLUSH_READY
        const esp_lcd_panel_io_callbacks_t cbs = {
            .on_color_trans_done = lvgl_port_flush_io_ready_callback,
        };
        /* Register done callback */
        esp_lcd_panel_io_register_event_callbacks(disp_ctx->io_handle, &cbs, &disp_ctx->disp_drv);
#endif
    }

    return disp;
}

lv_display_t *lvgl_port_add_disp_dsi(const lvgl_port_display_cfg_t *disp_cfg, const lvgl_port_display_dsi_cfg_t *dsi_cfg)
{
    lv_disp_t *disp = lvgl_port_add_disp_priv(disp_cfg, NULL);

    if (disp != NULL) {
        lvgl_port_display_ctx_t *disp_ctx = lvgl_port_get_display_ctx(disp);
        /* Set display type */
        disp_ctx->disp_type = LVGL_PORT_DISP_TYPE_DSI;

#if (CONFIG_IDF_TARGET_ESP32P4 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))
        const esp_lcd_dpi_panel_event_callbacks_t cbs = {
            .on_color_trans_done = lvgl_port_flush_panel_ready_callback,
        };
        /* Register done callback */
        esp_lcd_dpi_panel_register_event_callbacks(disp_ctx->panel_handle, &cbs, &disp_ctx->disp_drv);
#else
        ESP_RETURN_ON_FALSE(false, NULL, TAG, "MIPI-DSI is supported only on ESP32P4 and from IDF 5.3!");
#endif
    }

    return disp;
}

lv_display_t *lvgl_port_add_disp_rgb(const lvgl_port_display_cfg_t *disp_cfg, const lvgl_port_display_rgb_cfg_t *rgb_cfg)
{
    assert(rgb_cfg != NULL);
    const lvgl_port_disp_priv_cfg_t priv_cfg = {
        .avoid_tearing = rgb_cfg->flags.avoid_tearing,
    };
    lv_disp_t *disp = lvgl_port_add_disp_priv(disp_cfg, &priv_cfg);

    if (disp != NULL) {
        lvgl_port_display_ctx_t *disp_ctx = lvgl_port_get_display_ctx(disp);
        /* Set display type */
        disp_ctx->disp_type = LVGL_PORT_DISP_TYPE_RGB;

#if (CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
        /* Register done callback */
        const esp_lcd_rgb_panel_event_callbacks_t vsync_cbs = {
            .on_vsync = lvgl_port_flush_vsync_ready_callback,
        };

        const esp_lcd_rgb_panel_event_callbacks_t bb_cbs = {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 2)
            .on_bounce_frame_finish = lvgl_port_flush_vsync_ready_callback,
#endif
        };

        if (rgb_cfg->flags.bb_mode && (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 2))) {
            ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(disp_ctx->panel_handle, &bb_cbs, &disp_ctx->disp_drv));
        } else {
            ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(disp_ctx->panel_handle, &vsync_cbs, &disp_ctx->disp_drv));
        }
#else
        ESP_RETURN_ON_FALSE(false, NULL, TAG, "RGB is supported only on ESP32S3 and from IDF 5.0!");
#endif
    }

    return disp;
}

esp_err_t lvgl_port_remove_disp(lv_disp_t *disp)
{
    assert(disp);
    lv_disp_drv_t *disp_drv = disp->driver;
    assert(disp_drv);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)disp_drv->user_data;

    lv_disp_remove(disp);

    if (disp_drv) {
        if (disp_drv->draw_buf && disp_drv->draw_buf->buf1) {
            free(disp_drv->draw_buf->buf1);
            disp_drv->draw_buf->buf1 = NULL;
        }
        if (disp_drv->draw_buf && disp_drv->draw_buf->buf2) {
            free(disp_drv->draw_buf->buf2);
            disp_drv->draw_buf->buf2 = NULL;
        }
        if (disp_drv->draw_buf) {
            free(disp_drv->draw_buf);
            disp_drv->draw_buf = NULL;
        }
    }

    free(disp_ctx);

    return ESP_OK;
}

void lvgl_port_flush_ready(lv_disp_t *disp)
{
    assert(disp);
    assert(disp->driver);
    lv_disp_flush_ready(disp->driver);
}

/*******************************************************************************
* Private functions
*******************************************************************************/

static lvgl_port_display_ctx_t *lvgl_port_get_display_ctx(lv_disp_t *disp)
{
    assert(disp);
    lv_disp_drv_t *disp_drv = disp->driver;
    assert(disp_drv);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)disp_drv->user_data;
    return disp_ctx;
}

static lv_disp_t *lvgl_port_add_disp_priv(const lvgl_port_display_cfg_t *disp_cfg, const lvgl_port_disp_priv_cfg_t *priv_cfg)
{
    esp_err_t ret = ESP_OK;
    lv_disp_t *disp = NULL;
    lv_color_t *buf1 = NULL;
    lv_color_t *buf2 = NULL;
    lv_color_t *buf3 = NULL;
    uint32_t buffer_size = 0;
    SemaphoreHandle_t trans_sem = NULL;
    assert(disp_cfg != NULL);
    assert(disp_cfg->panel_handle != NULL);
    assert(disp_cfg->buffer_size > 0);
    assert(disp_cfg->hres > 0);
    assert(disp_cfg->vres > 0);

    /* Display context */
    lvgl_port_display_ctx_t *disp_ctx = malloc(sizeof(lvgl_port_display_ctx_t));
    ESP_GOTO_ON_FALSE(disp_ctx, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for display context allocation!");
    memset(disp_ctx, 0, sizeof(lvgl_port_display_ctx_t));
    disp_ctx->io_handle = disp_cfg->io_handle;
    disp_ctx->panel_handle = disp_cfg->panel_handle;
    disp_ctx->control_handle = disp_cfg->control_handle;
    disp_ctx->rotation.swap_xy = disp_cfg->rotation.swap_xy;
    disp_ctx->rotation.mirror_x = disp_cfg->rotation.mirror_x;
    disp_ctx->rotation.mirror_y = disp_cfg->rotation.mirror_y;
    disp_ctx->trans_size = disp_cfg->trans_size;

    buffer_size = disp_cfg->buffer_size;

    /* Use RGB internal buffers for avoid tearing effect */
    if (priv_cfg && priv_cfg->avoid_tearing) {
#if CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        buffer_size = disp_cfg->hres * disp_cfg->vres;
        ESP_GOTO_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(disp_cfg->panel_handle, 2, (void *)&buf1, (void *)&buf2), err, TAG, "Get RGB buffers failed");
#endif
    } else {
        uint32_t buff_caps = MALLOC_CAP_DEFAULT;
        if (disp_cfg->flags.buff_dma && disp_cfg->flags.buff_spiram && (0 == disp_cfg->trans_size)) {
            ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "Alloc DMA capable buffer in SPIRAM is not supported!");
        } else if (disp_cfg->flags.buff_dma) {
            buff_caps = MALLOC_CAP_DMA;
        } else if (disp_cfg->flags.buff_spiram) {
            buff_caps = MALLOC_CAP_SPIRAM;
        }

        if (disp_cfg->trans_size) {
            buf3 = heap_caps_malloc(disp_cfg->trans_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
            ESP_GOTO_ON_FALSE(buf3, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for buffer(transport) allocation!");
            disp_ctx->trans_buf = buf3;

            trans_sem = xSemaphoreCreateCounting(1, 0);
            ESP_GOTO_ON_FALSE(trans_sem, ESP_ERR_NO_MEM, err, TAG, "Failed to create transport counting Semaphore");
            disp_ctx->trans_sem = trans_sem;
        }

        /* alloc draw buffers used by LVGL */
        /* it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized */
        buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), buff_caps);
        ESP_GOTO_ON_FALSE(buf1, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for LVGL buffer (buf1) allocation!");
        if (disp_cfg->double_buffer) {
            buf2 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), buff_caps);
            ESP_GOTO_ON_FALSE(buf2, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for LVGL buffer (buf2) allocation!");
        }
    }

    lv_disp_draw_buf_t *disp_buf = malloc(sizeof(lv_disp_draw_buf_t));
    ESP_GOTO_ON_FALSE(disp_buf, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for LVGL display buffer allocation!");

    /* initialize LVGL draw buffers */
    lv_disp_draw_buf_init(disp_buf, buf1, buf2, buffer_size);

    ESP_LOGD(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_ctx->disp_drv);
    disp_ctx->disp_drv.hor_res = disp_cfg->hres;
    disp_ctx->disp_drv.ver_res = disp_cfg->vres;
    disp_ctx->disp_drv.flush_cb = lvgl_port_flush_callback;
    disp_ctx->disp_drv.draw_buf = disp_buf;
    disp_ctx->disp_drv.user_data = disp_ctx;

    disp_ctx->disp_drv.sw_rotate = disp_cfg->flags.sw_rotate;
    if (disp_ctx->disp_drv.sw_rotate == false) {
        disp_ctx->disp_drv.drv_update_cb = lvgl_port_update_callback;
    }

    /* Monochrome display settings */
    if (disp_cfg->monochrome) {
        /* When using monochromatic display, there must be used full buffer! */
        ESP_GOTO_ON_FALSE((disp_cfg->hres * disp_cfg->vres == buffer_size), ESP_ERR_INVALID_ARG, err, TAG, "Monochromatic display must using full buffer!");

        disp_ctx->disp_drv.full_refresh = 1;
        disp_ctx->disp_drv.set_px_cb = lvgl_port_pix_monochrome_callback;
    } else if (disp_cfg->flags.direct_mode) {
        /* When using direct_mode, there must be used full buffer! */
        ESP_GOTO_ON_FALSE((disp_cfg->hres * disp_cfg->vres == buffer_size), ESP_ERR_INVALID_ARG, err, TAG, "Direct mode must using full buffer!");

        disp_ctx->disp_drv.direct_mode = 1;
    } else if (disp_cfg->flags.full_refresh) {
        /* When using full_refresh, there must be used full buffer! */
        ESP_GOTO_ON_FALSE((disp_cfg->hres * disp_cfg->vres == buffer_size), ESP_ERR_INVALID_ARG, err, TAG, "Full refresh must using full buffer!");

        disp_ctx->disp_drv.full_refresh = 1;
    }

    disp = lv_disp_drv_register(&disp_ctx->disp_drv);

    /* Apply rotation from initial display configuration */
    lvgl_port_update_callback(&disp_ctx->disp_drv);

err:
    if (ret != ESP_OK) {
        if (buf1) {
            free(buf1);
        }
        if (buf2) {
            free(buf2);
        }
        if (buf3) {
            free(buf3);
        }
        if (trans_sem) {
            vSemaphoreDelete(trans_sem);
        }
        if (disp_ctx) {
            free(disp_ctx);
        }
    }

    return disp;
}

#if LVGL_PORT_HANDLE_FLUSH_READY
static bool lvgl_port_flush_io_ready_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t taskAwake = pdFALSE;

    lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_ctx;
    assert(disp_drv != NULL);
    lvgl_port_display_ctx_t *disp_ctx = disp_drv->user_data;
    assert(disp_ctx != NULL);
    lv_disp_flush_ready(disp_drv);

    if (disp_ctx->trans_size && disp_ctx->trans_sem) {
        xSemaphoreGiveFromISR(disp_ctx->trans_sem, &taskAwake);
    }

    return false;
}

#if (CONFIG_IDF_TARGET_ESP32P4 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))
static bool lvgl_port_flush_panel_ready_callback(esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    BaseType_t taskAwake = pdFALSE;

    lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_ctx;
    assert(disp_drv != NULL);
    lvgl_port_display_ctx_t *disp_ctx = disp_drv->user_data;
    assert(disp_ctx != NULL);
    lv_disp_flush_ready(disp_drv);

    if (disp_ctx->trans_size && disp_ctx->trans_sem) {
        xSemaphoreGiveFromISR(disp_ctx->trans_sem, &taskAwake);
    }

    return false;
}
#endif

#if CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static bool lvgl_port_flush_vsync_ready_callback(esp_lcd_panel_handle_t panel_io, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;

    lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_ctx;
    assert(disp_drv != NULL);
    need_yield = lvgl_port_task_notify(ULONG_MAX);

    return (need_yield == pdTRUE);
}
#endif
#endif

static void lvgl_port_flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    assert(drv != NULL);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)drv->user_data;
    assert(disp_ctx != NULL);

    int x_draw_start;
    int x_draw_end;
    int y_draw_start;
    int y_draw_end;

    int y_start_tmp;
    int y_end_tmp;

    int trans_count;
    int trans_line;
    int max_line;

    const int x_start = area->x1;
    const int x_end = area->x2;
    const int y_start = area->y1;
    const int y_end = area->y2;
    const int width = x_end - x_start + 1;
    const int height = y_end - y_start + 1;

    lv_color_t *from = color_map;
    lv_color_t *to = NULL;

    if (disp_ctx->trans_size == 0) {
        if (disp_ctx->disp_type == LVGL_PORT_DISP_TYPE_RGB && (drv->direct_mode || drv->full_refresh)) {
            if (lv_disp_flush_is_last(drv)) {
                /* If the interface is I80 or SPI, this step cannot be used for drawing. */
                esp_lcd_panel_draw_bitmap(disp_ctx->panel_handle, x_start, y_start, x_end + 1, y_end + 1, color_map);
                /* Waiting for the last frame buffer to complete transmission */
                ulTaskNotifyValueClear(NULL, ULONG_MAX);
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            }
        } else {
            esp_lcd_panel_draw_bitmap(disp_ctx->panel_handle, x_start, y_start, x_end + 1, y_end + 1, color_map);
        }

        if (disp_ctx->disp_type == LVGL_PORT_DISP_TYPE_RGB) {
            lv_disp_flush_ready(drv);
        }
    } else {
        y_start_tmp = y_start;
        max_line = ((disp_ctx->trans_size / width) > height) ? (height) : (disp_ctx->trans_size / width);
        trans_count = height / max_line + (height % max_line ? (1) : (0));

        for (int i = 0; i < trans_count; i++) {
            trans_line = (y_end - y_start_tmp + 1) > max_line ? max_line : (y_end - y_start_tmp + 1);
            y_end_tmp = (y_end - y_start_tmp + 1) > max_line ? (y_start_tmp + max_line - 1) : y_end;

            to = disp_ctx->trans_buf;
            for (int y = 0; y < trans_line; y++) {
                for (int x = 0; x < width; x++) {
                    *(to + y * (width) + x) = *(from + y * (width) + x);
                }
            }
            x_draw_start = x_start;
            x_draw_end = x_end;
            y_draw_start = y_start_tmp;
            y_draw_end = y_end_tmp;
            esp_lcd_panel_draw_bitmap(disp_ctx->panel_handle, x_draw_start, y_draw_start, x_draw_end + 1, y_draw_end + 1, to);

            from += max_line * width;
            y_start_tmp += max_line;
            xSemaphoreTake(disp_ctx->trans_sem, portMAX_DELAY);
        }
    }
}

static void lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    assert(drv);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)drv->user_data;
    assert(disp_ctx != NULL);
    esp_lcd_panel_handle_t control_handle = (disp_ctx->control_handle ? disp_ctx->control_handle : disp_ctx->panel_handle);

    /* Solve rotation screen and touch */
    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(control_handle, disp_ctx->rotation.swap_xy);
        esp_lcd_panel_mirror(control_handle, disp_ctx->rotation.mirror_x, disp_ctx->rotation.mirror_y);
        break;
    case LV_DISP_ROT_90:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(control_handle, !disp_ctx->rotation.swap_xy);
        if (disp_ctx->rotation.swap_xy) {
            esp_lcd_panel_mirror(control_handle, !disp_ctx->rotation.mirror_x, disp_ctx->rotation.mirror_y);
        } else {
            esp_lcd_panel_mirror(control_handle, disp_ctx->rotation.mirror_x, !disp_ctx->rotation.mirror_y);
        }
        break;
    case LV_DISP_ROT_180:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(control_handle, disp_ctx->rotation.swap_xy);
        esp_lcd_panel_mirror(control_handle, !disp_ctx->rotation.mirror_x, !disp_ctx->rotation.mirror_y);
        break;
    case LV_DISP_ROT_270:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(control_handle, !disp_ctx->rotation.swap_xy);
        if (disp_ctx->rotation.swap_xy) {
            esp_lcd_panel_mirror(control_handle, disp_ctx->rotation.mirror_x, !disp_ctx->rotation.mirror_y);
        } else {
            esp_lcd_panel_mirror(control_handle, !disp_ctx->rotation.mirror_x, disp_ctx->rotation.mirror_y);
        }
        break;
    }
}

static void lvgl_port_pix_monochrome_callback(lv_disp_drv_t *drv, uint8_t *buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y, lv_color_t color, lv_opa_t opa)
{
    if (drv->rotated == LV_DISP_ROT_90 || drv->rotated == LV_DISP_ROT_270) {
        lv_coord_t tmp_x = x;
        x = y;
        y = tmp_x;
    }

    /* Write to the buffer as required for the display.
    * It writes only 1-bit for monochrome displays mapped vertically.*/
    buf += drv->hor_res * (y >> 3) + x;
    if (lv_color_to1(color)) {
        (*buf) &= ~(1 << (y % 8));
    } else {
        (*buf) |= (1 << (y % 8));
    }
}
