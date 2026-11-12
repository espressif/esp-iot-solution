/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pixel/present_executor.h"
#ifdef PRESENT_HOST
#ifndef PRESENT_HOST_DMA2D
#define PRESENT_HOST_DMA2D 1
#endif
#define PRESENT_PLUGIN_ENABLED PRESENT_HOST_DMA2D
#else
#include "soc/soc_caps.h"
#define PRESENT_PLUGIN_ENABLED SOC_DMA2D_SUPPORTED
#endif
#if PRESENT_PLUGIN_ENABLED
#include "pixel/executor/present_executor_hw.h"
#include "esp_private/async_memcpy_dma2d.h"
typedef struct {
    esp_display_present_executor_t base;
    present_mutex_t *gate;
    present_os_event_t *done;
    async_memcpy_dma2d_handle_t dma;
} plugin_t;
static bool PRESENT_ISR_ATTR dma2d_completed(async_memcpy_dma2d_handle_t driver, async_memcpy_dma2d_event_data_t *event, void *context)
{
    (void)driver;
    (void)event;
    plugin_t *plugin = context;
    return present_event_signal(plugin->done, true);
}
static esp_err_t dma2d_caps(void *context, esp_display_present_executor_caps_t *out)
{
    (void)context;
    *out = (esp_display_present_executor_caps_t) {
        .name = "dma2d",
        .format_mask = 3,
        .rotation_mask = 1,
        .byte_swap = false,
        .base_alignment = 64,
        .dst_cap_align = 64,
        .stride_align = 1,
        .max_stride_pixels = 0x3fff,
        .max_height = 0x3fff,
        .addr_constraints = true,
        .full_stride_cap = true
    };
    return ESP_OK;
}
static esp_err_t dma2d_probe(void *context, const esp_display_present_copy_request_t *request, esp_display_present_executor_reason_t *reason)
{
    (void)context;
    void *band;
    size_t bytes;
    esp_err_t error = hw_probe(request, reason, &band, &bytes);
    if (error != ESP_OK) {
        return error;
    }
    if (request->rotation != ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || request->src.layout.byte_order != request->dst.layout.byte_order) {
        return unsupported(reason, ESP_DISPLAY_PRESENT_EXEC_TRANSFORM);
    }
    size_t bpp = request->src.layout.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    size_t si = dma2d_get_buffer_alignment_constraint(request->src.pixels);
    size_t di = dma2d_get_buffer_alignment_constraint(request->dst.pixels);
    if ((si > 1 && ((uintptr_t)request->src.pixels % si || request->src.layout.stride_bytes % si ||
                    (request->src_area.x * bpp) % si || (request->src_area.width * bpp) % si)) ||
            (di > 1 && ((uintptr_t)request->dst.pixels % di || request->dst.layout.stride_bytes % di ||
                        (request->dst_x * bpp) % di || (request->src_area.width * bpp) % di))) {
        return unsupported(reason, ESP_DISPLAY_PRESENT_EXEC_ALIGNMENT);
    }
    return ESP_OK;
}
static esp_err_t dma2d_execute(void *context, const esp_display_present_copy_request_t *request)
{
    plugin_t *plugin = context;
    esp_display_present_executor_reason_t reason;
    esp_err_t error = dma2d_probe(context, request, &reason);
    if (error != ESP_OK) {
        return error;
    }
    void *band;
    size_t band_bytes;
    error = hw_probe(request, &reason, &band, &band_bytes);
    if (error != ESP_OK) {
        return error;
    }
    size_t bpp = request->src.layout.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    (void)present_mutex_take(plugin->gate, UINT32_MAX);
    error = hw_sync_source(request);
    if (error != ESP_OK) {
        goto done;
    }
    error = hw_sync(band, band_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    if (error != ESP_OK) {
        goto done;
    }
    async_memcpy_dma2d_trans_desc_t op = {.src_buffer = request->src.pixels,
                                          .src_stride = request->src.layout.stride_bytes / bpp,
                                          .src_height = request->src.layout.height,
                                          .src_x = request->src_area.x,
                                          .src_y = request->src_area.y,
                                          .dst_buffer = request->dst.pixels,
                                          .dst_stride = request->dst.layout.stride_bytes / bpp,
                                          .dst_height = request->dst.layout.height,
                                          .dst_x = request->dst_x,
                                          .dst_y = request->dst_y,
                                          .copy_width = request->src_area.width,
                                          .copy_height = request->src_area.height,
                                          .pixel_format =
                                              bpp == 2 ? ESP_COLOR_FOURCC_RGB16 : ESP_COLOR_FOURCC_BGR24
                                         };
    (void)present_event_wait(plugin->done, 0);
    /* IDF signals our ISR callback before recycling its descriptor. A
     * following worker may arrive in that brief window. INVALID_STATE is
     * documented definite rejection (no free transaction), safe to retry. */
    uint64_t start = present_now_ms();
    do {
        error = esp_async_memcpy_dma2d(plugin->dma, &op, dma2d_completed, plugin);
        if (error != ESP_ERR_INVALID_STATE || present_expired(start, 1000)) {
            break;
        }
        present_pause();
    } while (true);
    if (error == ESP_OK) {
        (void)present_event_wait(plugin->done, UINT32_MAX);
    }
    if (error == ESP_OK) {
        error = hw_sync(band, band_bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    }
done:
    present_mutex_give(plugin->gate);
    return error;
}
static esp_err_t dma2d_destroy(void *context)
{
    plugin_t *plugin = context;
    esp_err_t error = esp_async_memcpy_uninstall_dma2d(plugin->dma);
    if (error != ESP_OK) {
        return error;
    }
    present_event_destroy(plugin->done);
    present_mutex_destroy(plugin->gate);
    present_free(plugin);
    return ESP_OK;
}
static const esp_display_present_executor_ops_t dma2d_ops = {dma2d_caps, dma2d_probe, dma2d_execute, dma2d_destroy};
esp_err_t esp_display_present_executor_dma2d_create(esp_display_present_executor_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    plugin_t *plugin = present_alloc(sizeof(*plugin));
    if (!plugin) {
        return ESP_ERR_NO_MEM;
    }
    plugin->gate = present_mutex_create();
    if (!plugin->gate) {
        present_free(plugin);
        return ESP_ERR_NO_MEM;
    }
    plugin->done = present_event_create();
    if (!plugin->done) {
        present_mutex_destroy(plugin->gate);
        present_free(plugin);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t error;
    async_memcpy_dma2d_config_t cfg = {.backlog = 1};
    error = esp_async_memcpy_install_dma2d(&cfg, &plugin->dma);
    if (error != ESP_OK) {
        present_event_destroy(plugin->done);
        present_mutex_destroy(plugin->gate);
        present_free(plugin);
        return error;
    }
    plugin->base = (esp_display_present_executor_t) {
        &dma2d_ops, plugin
    };
    *out = &plugin->base;
    return ESP_OK;
}
#else
esp_err_t esp_display_present_executor_dma2d_create(esp_display_present_executor_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
