/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_display_present_ppa.h"

#include "sdkconfig.h"
#include "soc/soc_caps.h"

#include "esp_log.h"

#if CONFIG_SOC_PPA_SUPPORTED
#include "driver/ppa.h"
#include "esp_heap_caps.h"

#include "esp_display_present_cache.h"
#endif

static const char *TAG = "present_ppa";

#if CONFIG_SOC_PPA_SUPPORTED
#define ESP_DISPLAY_PRESENT_PPA_SCALE_NO_CHANGE  (1.0f)
#define ESP_DISPLAY_PRESENT_PPA_SWAP_DISABLED    (0)
#define ESP_DISPLAY_PRESENT_COLOR_BYTES_RGB565   (2)
#define ESP_DISPLAY_PRESENT_COLOR_BYTES_RGB888   (3)

static size_t present_ppa_align_up(size_t value, size_t align)
{
    return (align > 0) ? (((value + align - 1) / align) * align) : value;
}

static ppa_srm_color_mode_t present_ppa_color_mode(uint8_t color_bytes)
{
    return (color_bytes == ESP_DISPLAY_PRESENT_COLOR_BYTES_RGB888) ?
           PPA_SRM_COLOR_MODE_RGB888 : PPA_SRM_COLOR_MODE_RGB565;
}

static size_t present_ppa_get_buffer_size(void *buffer, uint8_t color_bytes, uint16_t width, uint16_t height)
{
    size_t buffer_size = heap_caps_get_allocated_size(buffer);
    if (buffer_size != 0) {
        return buffer_size;
    }

    buffer_size = (size_t)color_bytes * width * height;
    size_t line_size = esp_display_present_get_cache_line_size_by_addr(buffer);
    if (line_size > 0) {
        buffer_size = present_ppa_align_up(buffer_size, line_size);
    }

    return buffer_size;
}

static ppa_srm_rotation_angle_t present_ppa_rotation_angle(esp_display_present_rotation_t rotation)
{
    switch (rotation) {
    case ESP_DISPLAY_PRESENT_ROTATE_90:
        return PPA_SRM_ROTATION_ANGLE_270;
    case ESP_DISPLAY_PRESENT_ROTATE_180:
        return PPA_SRM_ROTATION_ANGLE_180;
    case ESP_DISPLAY_PRESENT_ROTATE_270:
        return PPA_SRM_ROTATION_ANGLE_90;
    default:
        return PPA_SRM_ROTATION_ANGLE_0;
    }
}
#endif

#if CONFIG_SOC_PPA_SUPPORTED
static bool present_ppa_color_supported(uint8_t color_bytes)
{
    return color_bytes == ESP_DISPLAY_PRESENT_COLOR_BYTES_RGB565 ||
           color_bytes == ESP_DISPLAY_PRESENT_COLOR_BYTES_RGB888;
}
#endif

esp_err_t esp_display_present_ppa_register_srm_client(int max_pending_trans_num, void **out_handle)
{
#if CONFIG_SOC_PPA_SUPPORTED
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    ppa_client_config_t cfg = {
        .oper_type = PPA_OPERATION_SRM,
        .max_pending_trans_num = max_pending_trans_num,
    };
    return ppa_register_client(&cfg, (ppa_client_handle_t *)out_handle);
#else
    (void)max_pending_trans_num;
    (void)out_handle;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_display_present_ppa_unregister_client(void *handle)
{
#if CONFIG_SOC_PPA_SUPPORTED
    if (!handle) {
        return ESP_OK;
    }

    return ppa_unregister_client((ppa_client_handle_t)handle);
#else
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_display_present_ppa_try_open_tile_client(
    esp_display_present_rotation_t rotation,
    void **inout_handle)
{
    if (inout_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (*inout_handle != NULL) {
        return ESP_OK;
    }
    if (rotation != ESP_DISPLAY_PRESENT_ROTATE_90 &&
            rotation != ESP_DISPLAY_PRESENT_ROTATE_270) {
        return ESP_OK;
    }
    if (esp_display_present_ppa_register_srm_client(2, inout_handle) != ESP_OK) {
        *inout_handle = NULL;
        ESP_LOGW(TAG, "PPA unavailable; tile rotate uses software");
    }
    return ESP_OK;
}

void esp_display_present_ppa_close_tile_client(void **handle)
{
    if (handle == NULL || *handle == NULL) {
        return;
    }
    (void)esp_display_present_ppa_unregister_client(*handle);
    *handle = NULL;
}

esp_err_t esp_display_present_ppa_rotate_copy(
    void *ppa_handle,
    const esp_display_present_rotate_copy_request_t *request)
{
#if CONFIG_SOC_PPA_SUPPORTED
    if (request == NULL || !ppa_handle || request->source.pixels == NULL ||
            request->destination.pixels == NULL ||
            !present_ppa_color_supported(request->source.color_bytes) ||
            request->source.color_bytes != request->destination.color_bytes ||
            request->source_origin.x != 0 || request->source_origin.y != 0 ||
            request->logical_area.x1 < 0 || request->logical_area.y1 < 0 ||
            request->logical_area.x2 < request->logical_area.x1 ||
            request->logical_area.y2 < request->logical_area.y1 ||
            request->logical_area.x2 >= request->source.width ||
            request->logical_area.y2 >= request->source.height) {
        return ESP_ERR_INVALID_ARG;
    }

    const void *src = request->source.pixels;
    void *dst_fb = request->destination.pixels;
    uint16_t x_start = (uint16_t)request->logical_area.x1;
    uint16_t y_start = (uint16_t)request->logical_area.y1;
    uint16_t x_end = (uint16_t)request->logical_area.x2;
    uint16_t y_end = (uint16_t)request->logical_area.y2;
    uint16_t src_logical_w = request->source.width;
    uint16_t src_logical_h = request->source.height;
    uint16_t dst_physical_w = request->destination.width;
    uint16_t dst_physical_h = request->destination.height;
    esp_display_present_rotation_t rotation = request->rotation;
    uint8_t color_bytes = request->source.color_bytes;

    const uint16_t rect_w = x_end - x_start + 1;
    const uint16_t rect_h = y_end - y_start + 1;
    int x_offset = 0;
    int y_offset = 0;

    switch (rotation) {
    case ESP_DISPLAY_PRESENT_ROTATE_90:
        x_offset = dst_physical_w - y_end - 1;
        y_offset = x_start;
        break;
    case ESP_DISPLAY_PRESENT_ROTATE_180:
        x_offset = dst_physical_w - x_end - 1;
        y_offset = dst_physical_h - y_end - 1;
        break;
    case ESP_DISPLAY_PRESENT_ROTATE_270:
        x_offset = y_start;
        y_offset = dst_physical_h - x_end - 1;
        break;
    default:
        x_offset = x_start;
        y_offset = y_start;
        break;
    }

    size_t buffer_size = present_ppa_get_buffer_size(dst_fb, color_bytes, dst_physical_w, dst_physical_h);
    ppa_srm_oper_config_t oper_config = {
        .in.buffer = src,
        .in.pic_w = src_logical_w,
        .in.pic_h = src_logical_h,
        .in.block_w = rect_w,
        .in.block_h = rect_h,
        .in.block_offset_x = x_start,
        .in.block_offset_y = y_start,
        .in.srm_cm = present_ppa_color_mode(color_bytes),

           .out.buffer = dst_fb,
           .out.buffer_size = buffer_size,
           .out.pic_w = dst_physical_w,
           .out.pic_h = dst_physical_h,
           .out.block_offset_x = x_offset,
           .out.block_offset_y = y_offset,
           .out.srm_cm = present_ppa_color_mode(color_bytes),

           .rotation_angle = present_ppa_rotation_angle(rotation),
           .scale_x = ESP_DISPLAY_PRESENT_PPA_SCALE_NO_CHANGE,
           .scale_y = ESP_DISPLAY_PRESENT_PPA_SCALE_NO_CHANGE,
           .rgb_swap = ESP_DISPLAY_PRESENT_PPA_SWAP_DISABLED,
           .byte_swap = ESP_DISPLAY_PRESENT_PPA_SWAP_DISABLED,
           .mode = PPA_TRANS_MODE_BLOCKING,
    };

    return ppa_do_scale_rotate_mirror((ppa_client_handle_t)ppa_handle, &oper_config);
#else
    (void)ppa_handle;
    (void)request;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_display_present_ppa_rotate_copy_block(
    void *ppa_handle,
    const esp_display_present_ppa_rotate_block_request_t *request)
{
#if CONFIG_SOC_PPA_SUPPORTED
    if (request == NULL || !ppa_handle ||
            !present_ppa_color_supported(request->source.color_bytes) ||
            request->source.pixels == NULL ||
            request->destination.pixels == NULL ||
            request->source.color_bytes != request->destination.color_bytes ||
            request->source_area.x1 < 0 || request->source_area.y1 < 0 ||
            request->source_area.x2 < request->source_area.x1 ||
            request->source_area.y2 < request->source_area.y1 ||
            request->source_area.x2 >= request->source.width ||
            request->source_area.y2 >= request->source.height ||
            request->destination_origin.x < 0 ||
            request->destination_origin.y < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const void *src = request->source.pixels;
    void *dst_fb = request->destination.pixels;
    uint16_t src_stride_px = request->source.width;
    uint16_t src_height = request->source.height;
    uint16_t block_w =
        (uint16_t)(request->source_area.x2 - request->source_area.x1 + 1);
    uint16_t block_h =
        (uint16_t)(request->source_area.y2 - request->source_area.y1 + 1);
    uint16_t dst_offset_x = (uint16_t)request->destination_origin.x;
    uint16_t dst_offset_y = (uint16_t)request->destination_origin.y;
    uint16_t dst_physical_w = request->destination.width;
    uint16_t dst_physical_h = request->destination.height;
    esp_display_present_rotation_t rotation = request->rotation;
    uint8_t color_bytes = request->source.color_bytes;

    size_t buffer_size = present_ppa_get_buffer_size(
                             dst_fb, color_bytes, dst_physical_w, dst_physical_h);
    ppa_srm_oper_config_t oper_config = {
        .in.buffer = src,
        .in.pic_w = src_stride_px,
        .in.pic_h = src_height,
        .in.block_w = block_w,
        .in.block_h = block_h,
        .in.block_offset_x = request->source_area.x1,
        .in.block_offset_y = request->source_area.y1,
        .in.srm_cm = present_ppa_color_mode(color_bytes),

           .out.buffer = dst_fb,
           .out.buffer_size = buffer_size,
           .out.pic_w = dst_physical_w,
           .out.pic_h = dst_physical_h,
           .out.block_offset_x = dst_offset_x,
           .out.block_offset_y = dst_offset_y,
           .out.srm_cm = present_ppa_color_mode(color_bytes),

           .rotation_angle = present_ppa_rotation_angle(rotation),
           .scale_x = ESP_DISPLAY_PRESENT_PPA_SCALE_NO_CHANGE,
           .scale_y = ESP_DISPLAY_PRESENT_PPA_SCALE_NO_CHANGE,
           .rgb_swap = ESP_DISPLAY_PRESENT_PPA_SWAP_DISABLED,
           .byte_swap = ESP_DISPLAY_PRESENT_PPA_SWAP_DISABLED,
           .mode = PPA_TRANS_MODE_BLOCKING,
    };

    return ppa_do_scale_rotate_mirror(
               (ppa_client_handle_t)ppa_handle, &oper_config);
#else
    (void)ppa_handle;
    (void)request;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
