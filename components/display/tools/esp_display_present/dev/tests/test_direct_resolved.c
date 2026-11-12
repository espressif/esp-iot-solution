/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include "pixel/present_pixel_ops.h"
#include "backend/present_backend_config.h"
#include "backend/present_backend.h"
#include "platform/present_os.h"
#include "scan_panel.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#define OK(x) assert((x) == ESP_OK)
#define W 7
#define H 5
static const present_backend_ops_t *ops = &present_fb_backend;
static atomic_size_t copied_pixels;
static atomic_bool fail_next_copy;
static esp_err_t counted_copy(void *user, const esp_display_present_copy_request_t *request)
{
    (void)user;
    if (atomic_exchange(&fail_next_copy, false)) {
        return ESP_FAIL;
    }
    atomic_fetch_add(&copied_pixels, request->src_area.width * request->src_area.height);
    return esp_display_present_pixel_execute_cpu(request);
}
static esp_display_present_lease_t acquire(present_backend_t *b, esp_display_present_frame_t f)
{
    esp_display_present_lease_t lease;
    uint64_t start = present_now_ms();
    esp_err_t error;
    while ((error = ops->acquire(b, f, NULL, 0, &lease)) == ESP_ERR_TIMEOUT) {
        assert(!present_expired(start, 1000));
        OK(present_fb_backend.service(b));
        present_pause();
    }
    OK(error);
    return lease;
}
static void write_area(present_backend_t *b, esp_display_present_frame_t f,
                       esp_display_present_rect_t area, size_t bpp, unsigned color, uint8_t *oracle)
{
    esp_display_present_lease_t lease = acquire(b, f);
    for (unsigned y = area.y; y < area.y + area.height; ++y)
        for (unsigned x = area.x; x < area.x + area.width; ++x) {
            uint8_t pixel[3] = {(uint8_t)color, (uint8_t)(color + 1), (uint8_t)(color + 2)};
            memcpy((uint8_t *)lease.surface.pixels + y * lease.surface.layout.stride_bytes + x * bpp, pixel, bpp);
            if (oracle) {
                memcpy(oracle + (y * W + x) * bpp, pixel, bpp);
            }
        }
    esp_display_present_region_submit_t submit = {.logical_area = area, .stride_bytes = W * bpp};
    OK(ops->submit_region_ex(b, f, lease.id, &submit));
}
static void check(scan_panel_t *panel, unsigned rotation, size_t bpp, bool swap, const uint8_t *oracle)
{
    unsigned pw = rotation & 1 ? H : W;
    assert(panel->current.layout.width == pw);
    assert(panel->current.layout.height == (rotation & 1 ? W : H));
    for (unsigned y = 0; y < H; ++y)
        for (unsigned x = 0; x < W; ++x) {
            unsigned px = x, py = y;
            switch (rotation) {
            case 1: px = H - 1 - y; py = x; break;
            case 2: px = W - 1 - x; py = H - 1 - y; break;
            case 3: px = y; py = W - 1 - x; break;
            }
            const uint8_t *actual = (uint8_t *)panel->current.pixels + (py * pw + px) * bpp;
            const uint8_t *expected = oracle + (y * W + x) * bpp;
            for (unsigned i = 0; i < bpp; ++i) {
                assert(actual[i] == expected[swap ? 1 - i : i]);
            }
        }
}
static void exercise(unsigned rotation, bool asynchronous, size_t buffers, size_t bpp, bool swap)
{
    scan_panel_t panel = {0};
    esp_display_present_backend_config_t cfg = {
        .width = W, .height = H, .tile_rows = 2, .buffer_count = buffers, .max_frames = 4,
        .format = bpp == 2 ? ESP_DISPLAY_PRESENT_FORMAT_RGB565 : ESP_DISPLAY_PRESENT_FORMAT_RGB888,
        .rotation = rotation, .render_contract = ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT,
        .async_copy = asynchronous, .copy_execute = counted_copy, .panel = scan_interface(&panel),
        .panel_order = swap ? ESP_DISPLAY_PRESENT_ORDER_SWAPPED : ESP_DISPLAY_PRESENT_ORDER_NATIVE
    };
    present_backend_t *b;
    OK(ops->create(&cfg, &b));
    uint8_t oracle[W * H * 3] = {0};
    for (unsigned step = 0; step < 26; ++step) {
        /* Mix a regular read-modify-write producer into the resolved stream.
         * Changing coordinate contracts must invalidate history safely. */
        bool resolved = step != 12;
        esp_display_present_frame_request_t request = {.resolved_pixels = resolved};
        size_t before = atomic_load(&copied_pixels);
        esp_display_present_frame_t frame;
        OK(ops->begin(b, &request, &frame));
        if (resolved) {
            esp_display_present_lease_t lease;
            OK(ops->acquire(b, frame, NULL, 0, &lease)); /* No prefill ticket. */
            assert(atomic_load(&copied_pixels) == before);
            OK(ops->cancel_region(b, frame, lease.id));
        }
        esp_display_present_rect_t area = {0, 0, W, H};
        if (step) {
            area.x = (step * 2) % (W - 1);
            area.y = step % H;
            area.width = 2;
            area.height = 1;
        }
        uint8_t snapshot[W * H * 3];
        if (panel.current.pixels) {
            memcpy(snapshot, panel.current.pixels, W * H * bpp);
        }
        if (step == 8) {
            write_area(b, frame, area, bpp, 0xe0, NULL);
            OK(ops->cancel_frame(b, frame, 1000));
            OK(ops->begin(b, &request, &frame));
            area = (esp_display_present_rect_t) {
                0, H - 1, 1, 1
            };
        }
        write_area(b, frame, area, bpp, step + 3, oracle);
        esp_display_present_receipt_t receipt;
        OK(ops->end(b, frame, &receipt));
        if (step == 6 && !asynchronous) {
            panel.reject = true;
            assert(present_fb_backend.service(b) == ESP_FAIL);
            assert(!panel.cookie);
            panel.reject = false; /* READY pixels remain available for retry. */
        }
        uint64_t start = present_now_ms();
        while (!panel.cookie) {
            OK(present_fb_backend.service(b));
            assert(!present_expired(start, 1000));
            present_pause();
        }
        if (panel.current.pixels) {
            assert(panel.pending.pixels != panel.current.pixels);
            assert(memcmp(snapshot, panel.current.pixels, W * H * bpp) == 0);
        }
        scan_confirm(&panel);
        esp_display_present_frame_status_t status;
        OK(ops->wait(b, receipt, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 0, &status));
        assert(status.terminal && status.result == ESP_OK);
        OK(ops->release_receipt(b, receipt));
        check(&panel, rotation, bpp, swap, oracle);
        /* Once pools/history are warm, small damage must not trigger a whole
         * canvas reconstruction or full-frame rotation. Allow contract/cancel
         * recovery to repair conservatively before asserting steady state. */
        if (step > 17) {
            assert(atomic_load(&copied_pixels) - before < W * H);
        }
    }
    OK(ops->stop(b, 1000));
    OK(ops->destroy(b));
}
static void failed_transform(bool asynchronous)
{
    scan_panel_t panel = {0};
    esp_display_present_backend_config_t cfg = {
        .width = W, .height = H, .buffer_count = 2, .max_frames = 4,
        .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90,
        .render_contract = ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT,
        .async_copy = asynchronous, .copy_execute = counted_copy, .panel = scan_interface(&panel)
    };
    present_backend_t *b;
    OK(ops->create(&cfg, &b));
    esp_display_present_frame_request_t request = {.resolved_pixels = true};
    esp_display_present_frame_t frame;
    OK(ops->begin(b, &request, &frame));
    atomic_store(&fail_next_copy, true);
    esp_display_present_lease_t lease = acquire(b, frame);
    memset(lease.surface.pixels, 0x55, W * H * 2);
    esp_display_present_region_submit_t region = {.logical_area = {0, 0, W, H}, .stride_bytes = W * 2};
    esp_err_t error = ops->submit_region_ex(b, frame, lease.id, &region);
    assert(error == (asynchronous ? ESP_OK : ESP_FAIL));
    esp_display_present_receipt_t receipt = {0};
    if (asynchronous) {
        error = ops->end(b, frame, &receipt);
        assert(error == ESP_OK || error == ESP_FAIL);
        esp_display_present_backend_fault_t fault;
        uint64_t start = present_now_ms();
        do {
            OK(present_fb_backend.service(b));
            OK(ops->get_fault(b, &fault));
            assert(!present_expired(start, 1000));
            present_pause();
        } while (fault.result == ESP_OK);
        assert(fault.result == ESP_FAIL);
    }
    assert(!panel.submissions && !panel.cookie);
    OK(ops->stop(b, 1000));
    if (receipt.value) {
        esp_display_present_frame_status_t status;
        OK(ops->query(b, receipt, &status));
        assert(status.terminal && status.result == ESP_FAIL);
        OK(ops->release_receipt(b, receipt));
    }
    OK(ops->destroy(b));
}

int main(void)
{
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned async = 0; async < 2; ++async)
            for (size_t count = 2; count <= 3; ++count) {
                exercise(r, async, count, 2, false);
                exercise(r, async, count, 2, true);
                exercise(r, async, count, 3, false);
            }
    failed_transform(false);
    failed_transform(true);
    puts("DIRECT resolved pixels: physical repair, rotations, cancellation, contract changes and bounded copy passed");
}
