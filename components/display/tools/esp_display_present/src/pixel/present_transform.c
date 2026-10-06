/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_display_present_transform.h"

#include <stdlib.h>
#include <string.h>

#include "esp_display_present_cache.h"
#include "esp_display_present_ppa.h"
#include "esp_display_present_rotate.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_memory_utils.h"

typedef struct {
    esp_display_present_size_t size;
    size_t stride_bytes;
} present_plane_geometry_t;

struct esp_display_present_transform {
    uint8_t *logical_buffer;
    void *ppa_handle;
    esp_display_present_rotation_t rotation;
    present_plane_geometry_t logical;
    present_plane_geometry_t physical;
    uint8_t bytes_per_pixel;
};

static const char *TAG = "present_transform";

esp_err_t esp_display_present_transform_create(
    esp_display_present_target_t *target,
    uint8_t bytes_per_pixel,
    esp_display_present_transform_t **out_transform)
{
    if (out_transform != NULL) {
        *out_transform = NULL;
    }
    const esp_display_present_target_info_t *info =
        esp_display_present_target_get_info(target);
    if (info == NULL || out_transform == NULL ||
            info->hw.rotation == ESP_DISPLAY_PRESENT_ROTATE_0 ||
            bytes_per_pixel == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_present_transform_t *transform = heap_caps_calloc(
                                                     1, sizeof(*transform), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (transform == NULL) {
        return ESP_ERR_NO_MEM;
    }
    transform->rotation = info->hw.rotation;
    transform->logical.size.width = info->hw.width;
    transform->logical.size.height = info->hw.height;
    transform->physical.size.width = info->hw.rotation ==
                                     ESP_DISPLAY_PRESENT_ROTATE_180
                                     ? info->hw.width : info->hw.height;
    transform->physical.size.height = info->hw.rotation ==
                                      ESP_DISPLAY_PRESENT_ROTATE_180
                                      ? info->hw.height : info->hw.width;
    transform->bytes_per_pixel = bytes_per_pixel;
    transform->logical.stride_bytes =
        (size_t)info->hw.width * bytes_per_pixel;
    transform->physical.stride_bytes =
        (size_t)transform->physical.size.width * bytes_per_pixel;

    size_t logical_bytes = transform->logical.stride_bytes * info->hw.height;
    transform->logical_buffer = heap_caps_malloc(
                                    logical_bytes,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_CACHE_ALIGNED);
    if (transform->logical_buffer == NULL) {
        transform->logical_buffer = heap_caps_malloc(
                                        logical_bytes, MALLOC_CAP_8BIT | MALLOC_CAP_CACHE_ALIGNED);
    }
    if (transform->logical_buffer == NULL) {
        free(transform);
        return ESP_ERR_NO_MEM;
    }
    memset(transform->logical_buffer, 0, logical_bytes);
    if (esp_display_present_ppa_register_srm_client(
                2, &transform->ppa_handle) != ESP_OK) {
        transform->ppa_handle = NULL;
        ESP_LOGW(TAG, "PPA unavailable; using software rotation");
    }
    *out_transform = transform;
    return ESP_OK;
}

void esp_display_present_transform_delete(
    esp_display_present_transform_t *transform)
{
    if (transform == NULL) {
        return;
    }
    if (transform->ppa_handle != NULL) {
        esp_display_present_ppa_unregister_client(transform->ppa_handle);
    }
    free(transform->logical_buffer);
    free(transform);
}

esp_err_t esp_display_present_transform_get_logical_surface(
    const esp_display_present_transform_t *transform,
    esp_display_present_transform_surface_t *out_surface)
{
    if (transform == NULL || out_surface == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_surface = (esp_display_present_transform_surface_t) {
        .pixels = transform->logical_buffer,
        .stride_bytes = transform->logical.stride_bytes,
        .width = transform->logical.size.width,
        .height = transform->logical.size.height,
    };
    return ESP_OK;
}

esp_err_t esp_display_present_transform_get_physical_geometry(
    const esp_display_present_transform_t *transform,
    uint16_t *out_width,
    uint16_t *out_height,
    size_t *out_stride_bytes)
{
    if (transform == NULL || out_width == NULL || out_height == NULL ||
            out_stride_bytes == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_width = transform->physical.size.width;
    *out_height = transform->physical.size.height;
    *out_stride_bytes = transform->physical.stride_bytes;
    return ESP_OK;
}

esp_err_t esp_display_present_transform_copy(
    esp_display_present_transform_t *transform,
    const esp_display_present_transform_copy_request_t *request)
{
    if (transform == NULL || request == NULL ||
            request->source.pixels == NULL ||
            request->destination_pixels == NULL ||
            request->source.color_bytes != transform->bytes_per_pixel ||
            request->source.stride_bytes == 0 ||
            request->source.stride_bytes % transform->bytes_per_pixel != 0 ||
            request->logical_area.x1 < request->source_origin.x ||
            request->logical_area.y1 < request->source_origin.y ||
            request->logical_area.x2 < request->logical_area.x1 ||
            request->logical_area.y2 < request->logical_area.y1 ||
            request->logical_area.x2 >= transform->logical.size.width ||
            request->logical_area.y2 >= transform->logical.size.height ||
            request->logical_area.x2 - request->source_origin.x >=
            request->source.width ||
            request->logical_area.y2 - request->source_origin.y >=
            request->source.height) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_display_present_rotate_copy_request_t rotate_request = {
        .source = request->source,
        .source_origin = request->source_origin,
        .destination = {
            .pixels = request->destination_pixels,
            .width = transform->physical.size.width,
            .height = transform->physical.size.height,
            .stride_bytes = transform->physical.stride_bytes,
            .color_bytes = transform->bytes_per_pixel,
        },
        .logical_area = request->logical_area,
        .rotation = transform->rotation,
    };

    bool full_source = request->source.pixels == transform->logical_buffer &&
                       request->source_origin.x == 0 &&
                       request->source_origin.y == 0 &&
                       request->source.width == transform->logical.size.width &&
                       request->source.height == transform->logical.size.height &&
                       request->source.stride_bytes ==
                       transform->logical.stride_bytes;
    /* S31 RGB: PSRAM-to-PSRAM PPA SRM can lose completions / scramble pixels
     * while racing panel scanout. Keep that path on software rotation. */
    bool ppa_safe = transform->ppa_handle != NULL &&
                    !(esp_ptr_external_ram(request->source.pixels) &&
                      esp_ptr_external_ram(request->destination_pixels));
    if (full_source && ppa_safe) {
        esp_err_t ret = esp_display_present_ppa_rotate_copy(
                            transform->ppa_handle, &rotate_request);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
    }

    esp_display_present_rotate_copy(&rotate_request);
    return ESP_OK;
}
