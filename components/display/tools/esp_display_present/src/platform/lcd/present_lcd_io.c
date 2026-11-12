/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "platform/lcd/present_lcd_io.h"
#include "platform/present_os.h"
#include "esp_cache.h"
#include "esp_memory_utils.h"
#include <limits.h>
#include <string.h>
#ifdef PRESENT_HOST
#define PRESENT_SPI_EXTERNAL_DMA 1
#else
#include "soc/soc_caps.h"
#define PRESENT_SPI_EXTERNAL_DMA SOC_PSRAM_DMA_CAPABLE
#endif

enum {
    PRESENT_SPI_BASE_ALIGNMENT = 64,
    PRESENT_SPI_EXTERNAL_LENGTH_ALIGNMENT = 64,
};

struct esp_display_present_lcd_io {
    present_lock_t lock;
    esp_display_present_lcd_io_config_t config;
    esp_display_present_panel_event_fn callback;
    void *user;
    uint64_t cookies[16];
    size_t head, count, active_callbacks;
    bool attached, closing, submitting;
    void *staging;
    present_os_event_t *staging_event;
    bool staging_active, staging_last, staging_abort, staging_waiting;
    esp_err_t staging_result;
};

static bool PRESENT_ISR_ATTR finish_color(esp_display_present_lcd_io_t *spi, bool isr)
{
    present_lock_enter(&spi->lock, isr);
    if (!spi->attached || !spi->count) {
        present_lock_exit(&spi->lock, isr);
        return false;
    }
    if (spi->staging_active) {
        spi->staging_waiting = false;
        if (!spi->staging_last && !spi->staging_abort) {
            ++spi->active_callbacks;
            present_lock_exit(&spi->lock, isr);
            bool yield = present_event_signal(spi->staging_event, isr);
            present_lock_enter(&spi->lock, isr);
            --spi->active_callbacks;
            present_lock_exit(&spi->lock, isr);
            return yield;
        }
        spi->staging_active = false;
    }
    /* Untagged color-done retires the oldest cookie. Requires exclusive IO,
     * serialized submits and one tx_color per draw_bitmap; IDF only signals
     * its final SPI chunk. Revision-specific audit: STATE_MACHINE.md,
     * "LCD IO completion ordering (SPI/QSPI)". */
    uint64_t cookie = spi->cookies[spi->head];
    spi->head = (spi->head + 1) % spi->config.max_inflight;
    --spi->count;
    ++spi->active_callbacks;
    esp_display_present_panel_event_fn callback = spi->callback;
    void *context = spi->user;
    esp_err_t result = spi->staging_result;
    present_lock_exit(&spi->lock, isr);

    const esp_display_present_backend_event_t event = {
        .kind = ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE,
        .transaction_cookie = cookie,
        .result = result,
    };
    bool need_yield = callback(context, &event, isr);

    if (spi->staging_event) {
        need_yield |= present_event_signal(spi->staging_event, isr);
    }
    present_lock_enter(&spi->lock, isr);
    --spi->active_callbacks;
    present_lock_exit(&spi->lock, isr);
    return need_yield;
}

static bool PRESENT_ISR_ATTR color_done(esp_lcd_panel_io_handle_t io,
                                        esp_lcd_panel_io_event_data_t *data, void *user)
{
    (void)io;
    (void)data;
    return user ? finish_color(user, true) : false;
}

static esp_err_t spi_attach(void *context, esp_display_present_panel_event_fn callback, void *user)
{
    esp_display_present_lcd_io_t *spi = context;
    if (!spi || !callback) {
        return ESP_ERR_INVALID_ARG;
    }
    present_lock_enter(&spi->lock, false);
    if (spi->attached || spi->closing) {
        present_lock_exit(&spi->lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    spi->callback = callback;
    spi->user = user;
    spi->attached = true;
    present_lock_exit(&spi->lock, false);
    const esp_lcd_panel_io_callbacks_t callbacks = { .on_color_trans_done = color_done };
    esp_err_t ret = esp_lcd_panel_io_register_event_callbacks(spi->config.io, &callbacks, spi);
    if (ret != ESP_OK) {
        present_lock_enter(&spi->lock, false);
        spi->attached = false;
        spi->callback = NULL;
        spi->user = NULL;
        present_lock_exit(&spi->lock, false);
    }
    return ret;
}

static esp_err_t validate_tx(esp_display_present_lcd_io_t *spi, const esp_display_present_surface_t *surface,
                             const esp_display_present_rect_t *area, uint64_t cookie, size_t *bytes)
{
    if (!surface || !area || !surface->pixels || !cookie || !area->width || !area->height ||
            surface->layout.width != area->width || surface->layout.height != area->height ||
            surface->layout.format != spi->config.format || surface->layout.byte_order != spi->config.byte_order ||
            area->x > INT_MAX || area->y > INT_MAX ||
            area->width > (uint32_t)INT_MAX - area->x || area->height > (uint32_t)INT_MAX - area->y) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t bpp = spi->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    if (area->width > SIZE_MAX / bpp) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t row = area->width * bpp;
    if (surface->layout.stride_bytes != row || area->height > SIZE_MAX / row) {
        return ESP_ERR_INVALID_SIZE;
    }
    *bytes = row * area->height;
    if (*bytes > surface->capacity_bytes || (uintptr_t)surface->pixels % PRESENT_SPI_BASE_ALIGNMENT ||
            (uintptr_t)surface->pixels > UINTPTR_MAX - *bytes) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (spi->staging) {
        return row <= spi->config.staging_bytes ? ESP_OK : ESP_ERR_INVALID_SIZE;
    }
    /* Check both ends: a valid base alone does not establish the entire range. */
    const uint8_t *last = (const uint8_t *)surface->pixels + *bytes - 1;
    bool valid = esp_ptr_internal(surface->pixels) && esp_ptr_internal(last) &&
                 esp_ptr_dma_capable(surface->pixels) && esp_ptr_dma_capable(last);
#if PRESENT_SPI_EXTERNAL_DMA
    if (!valid && spi->config.psram_dma_direct && *bytes % PRESENT_SPI_EXTERNAL_LENGTH_ALIGNMENT == 0) {
        valid = esp_ptr_dma_ext_capable(surface->pixels) && esp_ptr_dma_ext_capable(last);
    }
#endif
    if (!valid) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

/* One logical transaction, bounded scratch. Only the last real DMA completion
 * publishes success. Aborting between bands retires reads with an error; an
 * uncertain current band stays pinned until its real completion callback. */
static esp_err_t stage_transfer(esp_display_present_lcd_io_t *spi, const esp_display_present_surface_t *surface, const esp_display_present_rect_t *area)
{
    size_t row = surface->layout.stride_bytes;
    size_t rows = spi->config.staging_bytes / row;
    uint64_t start = present_now_ms();
    uint32_t timeout = spi->config.staging_timeout_ms ? spi->config.staging_timeout_ms : 1000;
    present_lock_enter(&spi->lock, false);
    spi->staging_active = true;
    spi->staging_abort = false;
    spi->staging_result = ESP_OK;
    present_lock_exit(&spi->lock, false);
    esp_err_t error = ESP_OK;
    for (uint32_t y = 0; y < area->height;) {
        uint32_t n = area->height - y < rows ? area->height - y : (uint32_t)rows;
        (void)present_event_wait(spi->staging_event, 0);
        memcpy(spi->staging, (const uint8_t *)surface->pixels + (size_t)y * row, (size_t)n * row);
        error = esp_cache_msync(spi->staging, (size_t)n * row,
                                ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
        if (error != ESP_OK && error != ESP_ERR_NOT_SUPPORTED) {
            break;
        }
        present_lock_enter(&spi->lock, false);
        spi->staging_last = y + n == area->height;
        spi->staging_waiting = true;
        present_lock_exit(&spi->lock, false);
        error = esp_lcd_panel_draw_bitmap(spi->config.panel, area->x, area->y + y,
                                          area->x + area->width, area->y + y + n, spi->staging);
        if (error != ESP_OK) {
            break;
        }
        for (;;) {
            present_lock_enter(&spi->lock, false);
            bool waiting = spi->staging_waiting;
            present_lock_exit(&spi->lock, false);
            if (!waiting) {
                break;
            }
            uint64_t elapsed = present_now_ms() - start;
            uint32_t left = timeout == UINT32_MAX ? UINT32_MAX : elapsed >= timeout ? 0 : timeout - (uint32_t)elapsed;
            if (!present_event_wait(spi->staging_event, left)) {
                error = ESP_ERR_TIMEOUT;
                break;
            }
        }
        if (error != ESP_OK) {
            break;
        }
        y += n;
    }
    if (error != ESP_OK) {
        present_lock_enter(&spi->lock, false);
        spi->staging_result = error;
        spi->staging_abort = true;
        bool retired = !spi->staging_waiting && spi->count != 0;
        present_lock_exit(&spi->lock, false);
        /* All actual accesses ended at the previous real band callback. */
        if (retired) {
            (void)finish_color(spi, false);
        }
    }
    return error;
}

static esp_display_present_tx_result_t spi_submit(void *context, const esp_display_present_surface_t *surface,
                                                  const esp_display_present_rect_t *area, uint64_t cookie)
{
    esp_display_present_lcd_io_t *spi = context;
    if (!spi) {
        return (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_ERR_INVALID_ARG
        };
    }
    size_t bytes;
    esp_err_t ret = validate_tx(spi, surface, area, cookie, &bytes);
    if (ret != ESP_OK) {
        return (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_REJECTED, ret
        };
    }
    present_lock_enter(&spi->lock, false);
    if (!spi->attached || spi->closing || spi->submitting) {
        present_lock_exit(&spi->lock, false);
        return (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_ERR_INVALID_STATE
        };
    }
    if (spi->count == spi->config.max_inflight) {
        present_lock_exit(&spi->lock, false);
        return (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_ERR_NO_MEM
        };
    }
    present_lock_exit(&spi->lock, false);

    ret = spi->staging ? ESP_OK : esp_cache_msync(surface->pixels, bytes,
                                                  ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    /* esp_cache_msync documents NOT_SUPPORTED for a non-cacheable range. */
    if (ret != ESP_OK && ret != ESP_ERR_NOT_SUPPORTED) {
        return (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_REJECTED, ret
        };
    }
    present_lock_enter(&spi->lock, false);
    spi->cookies[(spi->head + spi->count) % spi->config.max_inflight] = cookie;
    ++spi->count;
    spi->submitting = true;
    present_lock_exit(&spi->lock, false);

    if (spi->staging != NULL) {
        ret = stage_transfer(spi, surface, area);
    } else {
        ret = esp_lcd_panel_draw_bitmap(spi->config.panel, (int)area->x, (int)area->y,
                                        (int)(area->x + area->width), (int)(area->y + area->height),
                                        surface->pixels);
    }
    present_lock_enter(&spi->lock, false);
    spi->submitting = false;
    if (ret != ESP_OK) {
        spi->closing = true;
    }
    present_lock_exit(&spi->lock, false);
    /* A failed split transfer can still own the DMA source. Never pop it. */
    return (esp_display_present_tx_result_t) {
        ret == ESP_OK ? ESP_DISPLAY_PRESENT_TX_ACCEPTED : ESP_DISPLAY_PRESENT_TX_UNCERTAIN, ret
    };
}

static esp_err_t spi_quiesce(void *context, uint32_t timeout)
{
    esp_display_present_lcd_io_t *spi = context;
    if (!spi) {
        return ESP_ERR_INVALID_ARG;
    }
    uint64_t start = present_now_ms();
    for (;;) {
        present_lock_enter(&spi->lock, false);
        spi->closing = true;
        bool attached = spi->attached;
        bool busy = spi->count || spi->active_callbacks || spi->submitting;
        present_lock_exit(&spi->lock, false);
        if (!attached) {
            return ESP_OK;
        }
        if (!busy) {
            break;
        }
        if (present_expired(start, timeout)) {
            return ESP_ERR_TIMEOUT;
        }
        present_pause();
    }
    /* Exclusive SPI IO + all color-done callbacks returned means no later
     * color event can begin. Registering an empty callback does not drain DMA. */
    const esp_lcd_panel_io_callbacks_t callbacks = {0};
    esp_err_t ret = esp_lcd_panel_io_register_event_callbacks(spi->config.io, &callbacks, NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    present_lock_enter(&spi->lock, false);
    spi->attached = false;
    spi->callback = NULL;
    spi->user = NULL;
    present_lock_exit(&spi->lock, false);
    return ESP_OK;
}

esp_err_t esp_display_present_lcd_io_create(const esp_display_present_lcd_io_config_t *config, esp_display_present_lcd_io_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!config || !config->io || !config->panel || !config->max_inflight || config->max_inflight > 16 ||
            (config->format != ESP_DISPLAY_PRESENT_FORMAT_RGB565 && config->format != ESP_DISPLAY_PRESENT_FORMAT_RGB888) ||
            (config->byte_order != ESP_DISPLAY_PRESENT_ORDER_NATIVE && config->byte_order != ESP_DISPLAY_PRESENT_ORDER_SWAPPED) ||
            (config->format == ESP_DISPLAY_PRESENT_FORMAT_RGB888 && config->byte_order != ESP_DISPLAY_PRESENT_ORDER_NATIVE)) {
        return ESP_ERR_INVALID_ARG;
    }
#if !PRESENT_SPI_EXTERNAL_DMA
    if (config->psram_dma_direct) {
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif
    esp_display_present_lcd_io_t *spi = present_alloc(sizeof(*spi));
    if (!spi) {
        return ESP_ERR_NO_MEM;
    }
    if (!present_lock_init(&spi->lock)) {
        present_free(spi);
        return ESP_ERR_NO_MEM;
    }
    spi->config = *config;
    if (config->staging_bytes) {
        spi->config.max_inflight = 1;
        spi->staging = present_alloc_pixels(config->staging_bytes);
        spi->staging_event = present_event_create();
        if (!spi->staging || !spi->staging_event) {
            present_free(spi->staging);
            present_event_destroy(spi->staging_event);
            present_lock_destroy(&spi->lock);
            present_free(spi);
            return ESP_ERR_NO_MEM;
        }
    }
    *out = spi;
    return ESP_OK;
}

esp_err_t esp_display_present_lcd_io_get_interface(esp_display_present_lcd_io_t *spi, esp_display_present_panel_adapter_t *out)
{
    if (!spi || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = (esp_display_present_panel_adapter_t) {
        spi, spi_attach, spi_submit, spi_quiesce
    };
    return ESP_OK;
}

esp_err_t esp_display_present_lcd_io_destroy(esp_display_present_lcd_io_t *spi)
{
    if (!spi) {
        return ESP_ERR_INVALID_ARG;
    }
    if (spi->attached || spi->count || spi->active_callbacks || spi->submitting) {
        return ESP_ERR_INVALID_STATE;
    }
    present_event_destroy(spi->staging_event);
    present_free(spi->staging);
    present_lock_destroy(&spi->lock);
    present_free(spi);
    return ESP_OK;
}

esp_err_t esp_display_present_lcd_io_get_output_caps(esp_display_present_lcd_io_t *spi, esp_display_present_output_caps_t *out)
{
    if (!spi || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    bool direct_external_dma = spi->config.psram_dma_direct && spi->staging == NULL;
    *out = (esp_display_present_output_caps_t) {
        .x_alignment = 1,
        .y_alignment = 1,
        .w_align = 1,
        .h_align = 1,
        .base_alignment = PRESENT_SPI_BASE_ALIGNMENT,
        .stride_align = 1,
        .tight_stride = true,
        .full_only = false,
        .internal_dma = true,
        .ext_mem = spi->config.psram_dma_direct || spi->staging != NULL,
        .max_inflight = spi->config.max_inflight,
        .ext_len_align = direct_external_dma ? PRESENT_SPI_EXTERNAL_LENGTH_ALIGNMENT : 1,
    };
    return ESP_OK;
}
