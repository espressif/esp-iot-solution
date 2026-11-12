/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "platform/lcd/present_lcd_fb_scanout.h"
#include "platform/present_os.h"
#include "platform/lcd/present_lcd_rgb_info.h"
#include "esp_cache.h"
#include <limits.h>
#ifdef PRESENT_HOST
#define PRESENT_DSI_SUPPORTED 1
#define PRESENT_RGB_SUPPORTED 1
#else
#include "soc/soc_caps.h"
#define PRESENT_DSI_SUPPORTED SOC_MIPI_DSI_SUPPORTED
#define PRESENT_RGB_SUPPORTED SOC_LCD_RGB_SUPPORTED
#endif
#if PRESENT_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#endif
#if PRESENT_RGB_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif
typedef enum {
    SCANOUT_PERIODIC,
    SCANOUT_REQUESTED,
} scanout_protocol_t;
struct esp_display_present_lcd_fb_scanout {
    present_lock_t lock;
    esp_display_present_lcd_fb_scanout_config_t config;
    scanout_protocol_t protocol;
    void *buffers[3];
    esp_display_present_panel_event_fn callback;
    void *callback_user;
    struct {
        uint64_t cookie;
        unsigned boundaries, required_boundaries;
        bool armed;
        struct {
            bool submitting, early_complete;
        } link;
    } pending;
    size_t active_callbacks;
    bool attached, closing, src_stopped, stopped, callbacks_removed;
    bool initial_started, initial_ready, single_vsync;
};
typedef struct {
    esp_display_present_panel_event_fn callback;
    void *user;
    uint64_t cookie;
} scanout_completion_t;

/* Caller holds the lock. Claim once before making it visible to the backend. */
static scanout_completion_t PRESENT_ISR_ATTR claim_completion(esp_display_present_lcd_fb_scanout_t *scanout)
{
    scanout_completion_t completion = {scanout->callback, scanout->callback_user, scanout->pending.cookie};
    scanout->pending.cookie = 0;
    scanout->pending.armed = false;
    scanout->pending.link.early_complete = false;
    ++scanout->active_callbacks;
    return completion;
}

static bool PRESENT_ISR_ATTR dispatch_completion(esp_display_present_lcd_fb_scanout_t *scanout,
                                                 const scanout_completion_t *completion, bool from_isr)
{
    if (!completion->callback) {
        return false;
    }
    esp_display_present_backend_event_t event = {
        .kind = ESP_DISPLAY_PRESENT_EVENT_SCANOUT_CONFIRMED,
        .transaction_cookie = completion->cookie,
    };
    bool need_yield = completion->callback(completion->user, &event, from_isr);
    present_lock_enter(&scanout->lock, from_isr);
    --scanout->active_callbacks;
    present_lock_exit(&scanout->lock, from_isr);
    return need_yield;
}

static bool PRESENT_ISR_ATTR frame_boundary(esp_display_present_lcd_fb_scanout_t *scanout, bool from_isr)
{
    /* IDF unregister updates callback and user_ctx separately. An ISR that
     * loaded the old function may already observe the cleared context. */
    if (!scanout) {
        return false;
    }
    present_lock_enter(&scanout->lock, from_isr);
    if (scanout->attached && scanout->pending.cookie && scanout->pending.link.submitting) {
        /* draw_bitmap has not resolved yet. Retain exactly one completion;
         * publishing it now could release buffers on an uncertain submit. */
        scanout->pending.link.early_complete = true;
        present_lock_exit(&scanout->lock, from_isr);
        return false;
    }
    scanout_completion_t completion = {0};
    unsigned needed = scanout->pending.required_boundaries;
    if (scanout->attached && scanout->pending.armed && (!scanout->closing || scanout->config.retain_scanout) &&
            ++scanout->pending.boundaries == needed) {
        completion = claim_completion(scanout);
    }
    present_lock_exit(&scanout->lock, from_isr);
    return dispatch_completion(scanout, &completion, from_isr);
}
static bool PRESENT_ISR_ATTR vsync(esp_display_present_lcd_fb_scanout_t *scanout)
{
    /* IDF unregister updates callback and user_ctx separately. An ISR that
     * loaded the old function may already observe the cleared context. */
    if (!scanout) {
        return false;
    }
    present_lock_enter(&scanout->lock, true);
    esp_display_present_scanout_vsync_fn callback = scanout->attached && !scanout->closing ? scanout->config.on_vsync : NULL;
    if (callback) {
        ++scanout->active_callbacks;
    }
    present_lock_exit(&scanout->lock, true);
    if (!callback) {
        return false;
    }
    bool yield = callback(scanout->config.vsync_user, present_now_us(), true);
    present_lock_enter(&scanout->lock, true);
    --scanout->active_callbacks;
    present_lock_exit(&scanout->lock, true);
    return yield;
}
#if PRESENT_DSI_SUPPORTED
static bool PRESENT_ISR_ATTR dsi_vsync(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *data, void *user)
{
    (void)panel;
    (void)data;
    return vsync(user);
}
static bool PRESENT_ISR_ATTR dsi_done(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *data, void *user)
{
    (void)panel;
    (void)data;
    return frame_boundary(user, true);
}
#endif
#if PRESENT_RGB_SUPPORTED
static bool PRESENT_ISR_ATTR rgb_vsync(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *data, void *user)
{
    (void)panel;
    (void)data;
    esp_display_present_lcd_fb_scanout_t *scanout = user;
    bool yield = vsync(scanout);
    /* A sole driver buffer never switches allocations. Explicit active-write
     * mode confirms exposure at VSYNC, without claiming old-FB retirement. */
    if (scanout && scanout->single_vsync) {
        yield |= frame_boundary(scanout, true);
    }
    return yield;
}
static bool PRESENT_ISR_ATTR rgb_done(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *data, void *user)
{
    (void)panel;
    (void)data;
    return frame_boundary(user, true);
}
#endif
static esp_err_t scanout_attach(void *context, esp_display_present_panel_event_fn callback, void *user)
{
    esp_display_present_lcd_fb_scanout_t *scanout = context;
    if (!scanout || !callback) {
        return ESP_ERR_INVALID_ARG;
    }
    if (scanout->attached || scanout->closing) {
        return ESP_ERR_INVALID_STATE;
    }
    scanout->callback = callback;
    scanout->callback_user = user;
    esp_err_t error = ESP_ERR_NOT_SUPPORTED;
#if PRESENT_DSI_SUPPORTED
    if (scanout->config.kind == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_DSI) {
        esp_lcd_dpi_panel_event_callbacks_t callbacks = {
            .on_frame_buf_complete = dsi_done,
            .on_vsync = scanout->config.on_vsync ? dsi_vsync : NULL,
        };
        error = esp_lcd_dpi_panel_register_event_callbacks(scanout->config.panel, &callbacks, scanout);
    }
#endif
#if PRESENT_RGB_SUPPORTED
    if (scanout->config.kind == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB) {
        esp_lcd_rgb_panel_event_callbacks_t callbacks = {
            .on_frame_buf_complete = scanout->single_vsync ? NULL : rgb_done,
            .on_vsync = (scanout->config.on_vsync || scanout->single_vsync) ? rgb_vsync : NULL,
        };
        error = esp_lcd_rgb_panel_register_event_callbacks(scanout->config.panel, &callbacks, scanout);
    }
#endif
    present_lock_enter(&scanout->lock, false);
    if (error == ESP_OK) {
        scanout->attached = true;
    }
    present_lock_exit(&scanout->lock, false);
    return error;
}

static esp_display_present_tx_result_t scanout_submit(void *context, const esp_display_present_surface_t *surface, const esp_display_present_rect_t *area, uint64_t cookie)
{
    esp_display_present_lcd_fb_scanout_t *scanout = context;
    if (!scanout || !surface || !area || !cookie || area->x || area->y || area->width != scanout->config.width ||
            area->height != scanout->config.height || surface->layout.width != area->width ||
            surface->layout.height != area->height || surface->layout.format != scanout->config.format ||
            surface->layout.byte_order != scanout->config.byte_order)
        return (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_ERR_INVALID_ARG
    };
    size_t bytes = (size_t)scanout->config.width * scanout->config.height * (scanout->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3);
    if (surface->layout.stride_bytes != bytes / scanout->config.height || surface->capacity_bytes < bytes)
        return (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_ERR_INVALID_SIZE
    };
    bool known = false;
    for (size_t i = 0; i < scanout->config.buffer_count; ++i) {
        known |= scanout->buffers[i] == surface->pixels;
    }
    if (!known)
        return (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_ERR_INVALID_ARG
    };
    present_lock_enter(&scanout->lock, false);
    bool busy = !scanout->attached || scanout->closing || scanout->pending.cookie;
    present_lock_exit(&scanout->lock, false);
    if (busy)
        return (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_ERR_INVALID_STATE
    };
    esp_err_t error =
        esp_cache_msync(surface->pixels, bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    if (error != ESP_OK && error != ESP_ERR_NOT_SUPPORTED)
        return (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_REJECTED, error
    };
    present_lock_enter(&scanout->lock, false);
    /* Reserve atomically after cache maintenance as well: another caller must
     * not overwrite an outstanding transaction. */
    if (!scanout->attached || scanout->closing || scanout->pending.cookie) {
        present_lock_exit(&scanout->lock, false);
        return (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_ERR_INVALID_STATE
        };
    }
    bool link_switch = scanout->protocol == SCANOUT_REQUESTED;
    scanout->pending.cookie = cookie;
    scanout->pending.armed = false;
    scanout->pending.link.submitting = link_switch;
    scanout->pending.link.early_complete = false;
    scanout->pending.boundaries = 0;
    scanout->pending.required_boundaries = 1;
    present_lock_exit(&scanout->lock, false);
    error = esp_lcd_panel_draw_bitmap(scanout->config.panel, 0, 0, area->width, area->height, surface->pixels);
    if (link_switch) {
        scanout_completion_t completion = {0};
        present_lock_enter(&scanout->lock, false);
        scanout->pending.link.submitting = false;
        scanout->pending.armed = error == ESP_OK;
        if (error == ESP_OK && scanout->pending.link.early_complete) {
            /* Consume under the same lock that resolves submission. A later
             * ISR must not consume this cookie (or the next submit) instead. */
            completion = claim_completion(scanout);
        }
        if (error != ESP_OK) {
            scanout->closing = true;
        }
        present_lock_exit(&scanout->lock, false);
        (void)dispatch_completion(scanout, &completion, false);
        return (esp_display_present_tx_result_t) {
            error == ESP_OK ? ESP_DISPLAY_PRESENT_TX_ACCEPTED : ESP_DISPLAY_PRESENT_TX_UNCERTAIN, error
        };
    }
    /* Keep the gate closed through submission and the ISR join. RGB bounce
     * finishes copying the old FB before selecting cur_fb_index; DPI finishes
     * the old DMA source before installing the selected FB's descriptor list.
     * An ISR can have sampled the old selection before draw_bitmap returned,
     * then enter our callback on another CPU afterwards. Join it before arming.
     *
     * After a successful join, the next frame-complete retires old FB reads.
     * Previously copied bounce/FIFO pixels may still reach the panel, but do
     * not borrow the old FB allocation. Waiting another frame for those pixels
     * would unnecessarily serialize both double- and triple-buffer output.
     * If joining is unavailable or fails, two serialized completions cover one
     * old ISR in flight. This relies on the audited continuous-scan drivers and
     * exclusive panel ownership; raw VSYNC/color-done are not retirement.
     */
    /* Single-buffer exposure has the same pre-submit ISR race. Joining it
     * permits the next boundary without the unnecessary two-period fallback. */
    esp_err_t joined = ESP_ERR_NOT_SUPPORTED;
    if (error == ESP_OK) {
        joined = present_isr_barrier(1000);
    }
    present_lock_enter(&scanout->lock, false);
    scanout->pending.boundaries = 0;
    scanout->pending.required_boundaries = (joined == ESP_OK || scanout->config.single_retire) ? 1u : 2u;
    scanout->pending.armed = error == ESP_OK;
    if (error != ESP_OK) {
        scanout->closing = true;
    }
    present_lock_exit(&scanout->lock, false);
    return (esp_display_present_tx_result_t) {
        error == ESP_OK ? ESP_DISPLAY_PRESENT_TX_ACCEPTED : ESP_DISPLAY_PRESENT_TX_UNCERTAIN, error
    };
}

static esp_err_t detach_retained(esp_display_present_lcd_fb_scanout_t *scanout, uint32_t timeout)
{
    uint64_t start = present_now_ms();
    for (;;) {
        present_lock_enter(&scanout->lock, false);
        bool busy = (scanout->pending.cookie && (scanout->pending.armed || scanout->pending.link.submitting)) || scanout->active_callbacks;
        present_lock_exit(&scanout->lock, false);
        if (!busy) {
            break;
        }
        if (present_expired(start, timeout)) {
            return ESP_ERR_TIMEOUT;
        }
        present_pause();
    }
    if (!scanout->callbacks_removed) {
        esp_err_t error = ESP_ERR_NOT_SUPPORTED;
#if PRESENT_DSI_SUPPORTED
        if (scanout->config.kind == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_DSI) {
            esp_lcd_dpi_panel_event_callbacks_t empty = {0};
            error = esp_lcd_dpi_panel_register_event_callbacks(scanout->config.panel, &empty, NULL);
        }
#endif
#if PRESENT_RGB_SUPPORTED
        if (scanout->config.kind == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB) {
            esp_lcd_rgb_panel_event_callbacks_t empty = {0};
            error = esp_lcd_rgb_panel_register_event_callbacks(scanout->config.panel, &empty, NULL);
        }
#endif
        if (error != ESP_OK) {
            return error;
        }
        scanout->callbacks_removed = true;
    }
    uint64_t elapsed = present_now_ms() - start;
    uint32_t left = timeout == UINT32_MAX ? UINT32_MAX : elapsed >= timeout ? 0 : timeout - (uint32_t)elapsed;
    esp_err_t error = present_isr_barrier(left);
    if (error != ESP_OK) {
        return error;
    }
    present_lock_enter(&scanout->lock, false);
    bool busy = scanout->active_callbacks != 0;
    present_lock_exit(&scanout->lock, false);
    return busy ? ESP_ERR_TIMEOUT : ESP_OK;
}

static esp_err_t scanout_quiesce(void *context, uint32_t timeout)
{
    esp_display_present_lcd_fb_scanout_t *scanout = context;
    if (!scanout) {
        return ESP_ERR_INVALID_ARG;
    }
    if (scanout->stopped) {
        return ESP_OK;
    }
    uint64_t start = present_now_ms();
    present_lock_enter(&scanout->lock, false);
    scanout->closing = true;
    present_lock_exit(&scanout->lock, false);
    if (!scanout->src_stopped) {
        esp_err_t error = scanout->config.retain_scanout ? detach_retained(scanout, timeout) :
                          scanout->config.delete_panel ? esp_lcd_panel_del(scanout->config.panel) :
                          scanout->config.stop_scanout(scanout->config.user, timeout);
        if (error != ESP_OK) {
            return error;
        }
        scanout->src_stopped = true;
    }
    for (;;) {
        present_lock_enter(&scanout->lock, false);
        bool busy = scanout->active_callbacks != 0;
        if (!busy) {
            scanout->attached = false;
            scanout->stopped = true;
            scanout->pending.cookie = 0;
        }
        present_lock_exit(&scanout->lock, false);
        if (!busy) {
            return ESP_OK;
        }
        if (present_expired(start, timeout)) {
            return ESP_ERR_TIMEOUT;
        }
        present_pause();
    }
}

esp_err_t esp_display_present_lcd_fb_scanout_create(const esp_display_present_lcd_fb_scanout_config_t *config, esp_display_present_lcd_fb_scanout_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!config || !config->panel || (!config->stop_scanout && !config->delete_panel && !config->retain_scanout) || !config->width ||
            !config->height || config->width > INT_MAX || config->height > INT_MAX || config->buffer_count < 1 ||
            config->buffer_count > 3 || (config->format != ESP_DISPLAY_PRESENT_FORMAT_RGB565 && config->format != ESP_DISPLAY_PRESENT_FORMAT_RGB888) ||
            config->kind < ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_DSI || config->kind > ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->byte_order < ESP_DISPLAY_PRESENT_ORDER_NATIVE || config->byte_order > ESP_DISPLAY_PRESENT_ORDER_SWAPPED ||
            (config->format == ESP_DISPLAY_PRESENT_FORMAT_RGB888 && config->byte_order != ESP_DISPLAY_PRESENT_ORDER_NATIVE)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->retain_scanout && (config->delete_panel || config->stop_scanout)) {
        return ESP_ERR_INVALID_ARG;
    }
    /* A plain frame-complete callback does not identify the FB or submission
     * selected by the driver. One callback cannot prove an old FB retired. */
    if (config->single_retire && config->buffer_count > 1) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    bool single_vsync = false;
    scanout_protocol_t protocol = SCANOUT_PERIODIC;
    if (config->kind == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB) {
        present_lcd_rgb_facts_t facts;
        esp_err_t error = present_lcd_rgb_get_facts(config->panel, &facts);
        if (error == ESP_OK) {
            const present_lcd_rgb_info_t *info = &facts.panel;
            size_t bpp = config->format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
            if (info->width != config->width || info->height != config->height ||
                    !info->continuous_refresh || info->custom_bounce_callback || info->transformed ||
                    info->bits_per_pixel != bpp * 8 || info->frame_buffer_count < config->buffer_count ||
                    info->frame_buffer_bytes != (uint64_t)config->width * config->height * bpp) {
                return ESP_ERR_NOT_SUPPORTED;
            }
            single_vsync = config->buffer_count == 1 && info->frame_buffer_count == 1 && !facts.retirement;
            if (!facts.retirement && !single_vsync) {
                return ESP_ERR_NOT_SUPPORTED;
            }
            if (!info->bounce_buffer_bytes && !single_vsync) {
                if (config->buffer_count < 2) {
                    return ESP_ERR_NOT_SUPPORTED;
                }
                protocol = SCANOUT_REQUESTED;
            }
        } else if (error != ESP_ERR_NOT_FOUND) {
            return error; /* Known conflicting/unsupported facts cannot fall back. */
        } else if (!config->bounce) {
            return ESP_ERR_NOT_SUPPORTED;
        }
    }
    size_t bpp = config->format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    if (config->width > SIZE_MAX / config->height / bpp) {
        return ESP_ERR_INVALID_SIZE;
    }
    esp_display_present_lcd_fb_scanout_t *scanout = present_alloc(sizeof(*scanout));
    if (!scanout) {
        return ESP_ERR_NO_MEM;
    }
    if (!present_lock_init(&scanout->lock)) {
        present_free(scanout);
        return ESP_ERR_NO_MEM;
    }
    scanout->config = *config;
    scanout->protocol = protocol;
    scanout->single_vsync = single_vsync;
    esp_err_t error = ESP_ERR_NOT_SUPPORTED;
#if PRESENT_DSI_SUPPORTED
    if (config->kind == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_DSI)
        error = esp_lcd_dpi_panel_get_frame_buffer(config->panel, config->buffer_count, &scanout->buffers[0], &scanout->buffers[1],
                                                   &scanout->buffers[2]);
#endif
#if PRESENT_RGB_SUPPORTED
    if (config->kind == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB)
        error = esp_lcd_rgb_panel_get_frame_buffer(config->panel, config->buffer_count, &scanout->buffers[0], &scanout->buffers[1],
                                                   &scanout->buffers[2]);
#endif
    for (size_t i = 0; error == ESP_OK && i < config->buffer_count; ++i) {
        if (!scanout->buffers[i] || (uintptr_t)scanout->buffers[i] % 64) {
            error = ESP_ERR_INVALID_ARG;
        }
        for (size_t j = 0; j < i; ++j)
            if (scanout->buffers[i] == scanout->buffers[j]) {
                error = ESP_ERR_INVALID_ARG;
            }
    }
    if (error != ESP_OK) {
        present_lock_destroy(&scanout->lock);
        present_free(scanout);
        return error;
    }
    *out = scanout;
    return ESP_OK;
}

/* Compatibility factory calls this before its first producer lease. A new
 * presenter may borrow a panel still scanning a previous presenter's last FB.
 * Establish FB0 explicitly instead of guessing which buffer is initially safe.
 * Timeout retains this adapter/presenter for retry; create never submits pixels. */
esp_err_t present_lcd_fb_scanout_initialize(esp_display_present_lcd_fb_scanout_t *scanout, uint32_t timeout)
{
    if (!scanout || !scanout->attached || scanout->closing) {
        return ESP_ERR_INVALID_STATE;
    }
    if (scanout->initial_ready) {
        return ESP_OK;
    }
    uint64_t start = present_now_ms();
    if (!scanout->initial_started) {
        size_t bytes = (size_t)scanout->config.width * scanout->config.height * (scanout->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3);
        esp_display_present_surface_t surface = {scanout->buffers[0], bytes,
            {
                scanout->config.width, scanout->config.height, bytes / scanout->config.height,
                scanout->config.format, scanout->config.byte_order
            }
        };
        esp_display_present_rect_t area = {0, 0, scanout->config.width, scanout->config.height};
        uint64_t cookie = present_new_token();
        if (!cookie) {
            return ESP_ERR_NO_MEM;
        }
        esp_display_present_tx_result_t tx = scanout_submit(scanout, &surface, &area, cookie);
        if (tx.acceptance != ESP_DISPLAY_PRESENT_TX_REJECTED) {
            scanout->initial_started = true;
        }
        if (tx.result != ESP_OK) {
            return tx.result;
        }
    }
    for (;;) {
        present_lock_enter(&scanout->lock, false);
        bool busy = scanout->pending.cookie || scanout->active_callbacks;
        present_lock_exit(&scanout->lock, false);
        if (!busy) {
            scanout->initial_ready = true;
            return ESP_OK;
        }
        if (present_expired(start, timeout)) {
            return ESP_ERR_TIMEOUT;
        }
        present_pause();
    }
}

esp_err_t esp_display_present_lcd_fb_scanout_get_buffers(esp_display_present_lcd_fb_scanout_t *scanout, void **buffers, size_t capacity, size_t *count)
{
    if (!scanout || !count || (capacity && !buffers)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (scanout->stopped) {
        return ESP_ERR_INVALID_STATE;
    }
    *count = scanout->config.buffer_count;
    if (capacity < *count) {
        return ESP_ERR_INVALID_SIZE;
    }
    for (size_t i = 0; i < *count; ++i) {
        buffers[i] = scanout->buffers[i];
    }
    return ESP_OK;
}

esp_err_t esp_display_present_lcd_fb_scanout_get_interface(esp_display_present_lcd_fb_scanout_t *scanout, esp_display_present_panel_adapter_t *out)
{
    if (!scanout || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = (esp_display_present_panel_adapter_t) {
        scanout, scanout_attach, scanout_submit, scanout_quiesce
    };
    return ESP_OK;
}

esp_err_t esp_display_present_lcd_fb_scanout_destroy(esp_display_present_lcd_fb_scanout_t *scanout)
{
    if (!scanout) {
        return ESP_ERR_INVALID_ARG;
    }
    if (scanout->attached || (scanout->closing && !scanout->stopped)) {
        return ESP_ERR_INVALID_STATE;
    }
    present_lock_destroy(&scanout->lock);
    present_free(scanout);
    return ESP_OK;
}

esp_err_t esp_display_present_lcd_fb_scanout_get_output_caps(esp_display_present_lcd_fb_scanout_t *scanout, esp_display_present_output_caps_t *out)
{
    if (!scanout || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = (esp_display_present_output_caps_t) {
        .x_alignment = 1,
        .y_alignment = 1,
        .w_align = 1,
        .h_align = 1,
        .base_alignment = 64,
        .stride_align = 1,
        .tight_stride = true,
        .full_only = true,
        .ext_bufs_required = true,
        .internal_dma = true,
        .ext_mem = true,
        .max_inflight = 1
    };
    return ESP_OK;
}

esp_err_t present_lcd_fb_scanout_probe_buffers(esp_lcd_panel_handle_t panel,
                                               esp_display_present_lcd_fb_scanout_kind_t kind,
                                               void **buffers, size_t *count)
{
    if (!panel || !buffers || !count) {
        return ESP_ERR_INVALID_ARG;
    }
    *count = 0;
    for (size_t n = 3; n; --n) {
        esp_err_t error = ESP_ERR_NOT_SUPPORTED;
#if PRESENT_DSI_SUPPORTED
        if (kind == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_DSI) {
            error = esp_lcd_dpi_panel_get_frame_buffer(panel, n, &buffers[0], &buffers[1], &buffers[2]);
        }
#endif
#if PRESENT_RGB_SUPPORTED
        if (kind == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB) {
            error = esp_lcd_rgb_panel_get_frame_buffer(panel, n, &buffers[0], &buffers[1], &buffers[2]);
        }
#endif
        if (error == ESP_OK) {
            *count = n;
            return ESP_OK;
        }
        if (error != ESP_ERR_INVALID_ARG) {
            return error;
        }
    }
    return ESP_ERR_NOT_SUPPORTED;
}
