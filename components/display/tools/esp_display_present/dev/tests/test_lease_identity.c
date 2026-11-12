/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include "core/present_producer.h"
#include "core/present_engine.h"
#include "test_panel.h"
#include <pthread.h>

typedef struct {
    esp_display_present_engine_t *engine;
    esp_display_present_lease_t lease;
} fixture_t;
static void *foreign(void *user)
{
    fixture_t *f = user;
    uint32_t rows;
    esp_display_present_engine_area_t area = {0, 0, 3, 1};
    assert(present_presenter_resolve_lease_rows(f->engine, f->lease.id, 8, 2, &rows) == ESP_ERR_INVALID_STATE);
    assert(present_presenter_submit_lease(f->engine, f->lease.id, f->lease.surface.pixels, &area, 8) == ESP_ERR_INVALID_STATE);
    return NULL;
}
int main(void)
{
    test_panel_t panel = {.width = 4, .height = 4, .bpp = 2, .immediate = true};
    esp_display_present_engine_config_t config = {.backend = {.width = 4, .height = 4, .tile_rows = 2,
                                                              .buffer_count = 1, .max_frames = 2, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                                              .panel = test_panel_interface(&panel)
                                                             }
                                                 };
    fixture_t f;
    assert(esp_display_present_engine_create(&config, &f.engine) == ESP_OK);
    const esp_display_present_frame_request_t request = {0};
    assert(esp_display_present_engine_begin_next_frame(f.engine, &request) == ESP_OK);
    assert(esp_display_present_engine_acquire_buffer(f.engine, &f.lease) == ESP_OK);
    esp_display_present_lease_id_t wrong = {.value = f.lease.id.value ^ (UINT64_C(1) << 32)};
    uint32_t rows = 99;
    assert(present_presenter_resolve_lease_rows(f.engine, wrong, 8, 2, &rows) == ESP_ERR_INVALID_STATE && rows == 0);
    assert(present_presenter_resolve_lease_rows(f.engine, wrong, 0, 2, &rows) == ESP_ERR_INVALID_ARG); /* Keep native argument precedence. */
    assert(present_presenter_resolve_lease_rows(f.engine, f.lease.id, 8, 9, &rows) == ESP_OK && rows == 2);
    pthread_t thread;
    assert(!pthread_create(&thread, NULL, foreign, &f));
    assert(!pthread_join(thread, NULL));
    esp_display_present_engine_area_t area = {0, 0, 3, 1};
    assert(present_presenter_submit_lease(f.engine, wrong, f.lease.surface.pixels, &area, 8) == ESP_ERR_INVALID_STATE);
    assert(present_presenter_submit_lease(f.engine, f.lease.id, (uint8_t *)f.lease.surface.pixels + 2, &area, 8) == ESP_ERR_INVALID_STATE);
    assert(present_presenter_submit_lease(f.engine, f.lease.id, f.lease.surface.pixels, &area, 0) == ESP_ERR_INVALID_ARG);
    memset(f.lease.surface.pixels, 0x5a, 16);
    assert(present_presenter_submit_lease(f.engine, f.lease.id, f.lease.surface.pixels, &area, 8) == ESP_OK);
    assert(panel.submitted == 1 && !panel.count && panel.pixels[0] == 0x5a);
    esp_display_present_lease_t next;
    assert(esp_display_present_engine_acquire_buffer(f.engine, &next) == ESP_OK);
    assert(next.surface.pixels == f.lease.surface.pixels && next.id.value != f.lease.id.value);
    assert(present_presenter_resolve_lease_rows(f.engine, f.lease.id, 8, 2, &rows) == ESP_ERR_INVALID_STATE);
    assert(present_presenter_submit_lease(f.engine, f.lease.id, next.surface.pixels, &area, 8) == ESP_ERR_INVALID_STATE);
    assert(present_presenter_resolve_lease_rows(f.engine, next.id, 8, 2, &rows) == ESP_OK);
    assert(esp_display_present_engine_cancel_frame(f.engine) == ESP_OK);
    assert(present_presenter_resolve_lease_rows(f.engine, next.id, 8, 2, &rows) == ESP_ERR_INVALID_STATE);
    assert(esp_display_present_engine_stop(f.engine, 1000) == ESP_OK);
    assert(esp_display_present_engine_destroy(f.engine) == ESP_OK);
    return 0;
}
