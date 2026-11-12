/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pixel/present_pixel_ops.h"
#include <assert.h>
#include <string.h>

/* Links only the stateless implementation: no OS/worker/engine library. */
int main(void)
{
    const uint16_t source[] = {0x1234, 0x2345, 0x3456, 0x4567, 0x5678, 0x6789};
    uint16_t destination[6] = {0};
    esp_display_present_copy_request_t request = {
        .src = {
            .pixels = source, .capacity_bytes = sizeof(source),
            .layout = {3, 2, 6, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}
        },
        .dst = {
            .pixels = destination, .capacity_bytes = sizeof(destination),
            .layout = {2, 3, 4, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_SWAPPED}
        },
        .src_area = {0, 0, 3, 2}, .rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90,
    };
    const uint16_t expected[] = {0x6745, 0x3412, 0x7856, 0x4523, 0x8967, 0x5634};
    assert(esp_display_present_pixel_execute_cpu(&request) == ESP_OK);
    assert(!memcmp(destination, expected, sizeof(expected)));
    esp_display_present_rect_t physical;
    assert(esp_display_present_rotate_rect(3, 2, request.rotation, &request.src_area, &physical) == ESP_OK);
    assert(physical.x == 0 && physical.y == 0 && physical.width == 2 && physical.height == 3);
    uint32_t x, y;
    assert(esp_display_present_unrotate_point(3, 2, request.rotation, 0, 2, &x, &y) == ESP_OK);
    assert(x == 2 && y == 1);

    /* Rejection must preserve the destination; no speculative partial writes. */
    request.dst.capacity_bytes--;
    assert(esp_display_present_pixel_execute_cpu(&request) == ESP_ERR_INVALID_ARG);
    assert(!memcmp(destination, expected, sizeof(expected)));
    request.dst.capacity_bytes++;
    request.rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0;
    request.src = (esp_display_present_source_t) {
        destination, sizeof(destination), request.dst.layout
    };
    request.src_area = (esp_display_present_rect_t) {
        0, 0, 2, 3
    };
    request.dst.layout.byte_order = ESP_DISPLAY_PRESENT_ORDER_NATIVE;
    const uint16_t native[] = {0x4567, 0x1234, 0x5678, 0x2345, 0x6789, 0x3456};
    assert(esp_display_present_pixel_execute_cpu(&request) == ESP_OK);
    assert(!memcmp(destination, native, sizeof(native)));
    return 0;
}
