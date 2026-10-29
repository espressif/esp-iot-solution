/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * GSP <-> LVGL handoff stress over one app-owned presenter.
 *
 * This loop deliberately runs in app_main's task. Creating another worker does
 * not make presenter ownership transitions safer and can reintroduce the
 * Display Off deadlock if one task waits while another owns the completion path.
 */

#include "handoff_stress_runner.h"

#include <inttypes.h>

#include "esp_gsp_esp_lcd.h"
#include "esp_log.h"
#include "esp_lv_present.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "handoff_lvgl_screen.h"
#include "hw_init.h"
#include "lvgl.h"

#include "bundle_gsp.h"

static const char *TAG = "gsp_lvgl_handoff";

#define HANDOFF_PAUSE_TIMEOUT_MS 1000
#define HANDOFF_LVGL_PUMP_MS 5
#define HANDOFF_QUIESCE_RETRY_MS 1000
#define HANDOFF_QUIESCE_MAX_ATTEMPTS 3
#define HANDOFF_RESUME_MAX_ATTEMPTS 10
#define HANDOFF_STOP_MAX_ATTEMPTS 3
#define HANDOFF_RETRY_DELAY_MS 100

static void hello_load_timer_cb(esp_gsp_handle_t gsp, void *user_ctx)
{
    static int32_t load;
    (void)user_ctx;
    load = (load + 5) % 101;
    esp_err_t ret = gsp_hello_load_set_value(gsp, load);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "load update failed: %s", esp_err_to_name(ret));
    }
}

static uint32_t elapsed_ms(int64_t start_us)
{
    return (uint32_t)((esp_timer_get_time() - start_us) / 1000);
}

static void pump_lvgl_until(int64_t end_us)
{
    while (esp_timer_get_time() < end_us) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(HANDOFF_LVGL_PUMP_MS));
    }
    lv_timer_handler();
}

static esp_err_t resume_gsp_with_retry(esp_gsp_esp_lcd_pause_t *pause,
                                       esp_gsp_handle_t *gsp,
                                       uint32_t *out_elapsed_ms)
{
    int64_t start = esp_timer_get_time();
    esp_err_t ret = ESP_FAIL;
    for (unsigned attempt = 1; attempt <= HANDOFF_RESUME_MAX_ATTEMPTS;
            ++attempt) {
        ret = esp_gsp_esp_lcd_resume_paused(pause, gsp);
        if (ret == ESP_OK) {
            if (out_elapsed_ms != NULL) {
                *out_elapsed_ms = elapsed_ms(start);
            }
            return ESP_OK;
        }
        ESP_LOGE(TAG, "resume attempt %u/%u failed: %s", attempt,
                 HANDOFF_RESUME_MAX_ATTEMPTS, esp_err_to_name(ret));
        if (attempt < HANDOFF_RESUME_MAX_ATTEMPTS) {
            vTaskDelay(pdMS_TO_TICKS(HANDOFF_RETRY_DELAY_MS));
        }
    }
    return ret;
}

static esp_err_t quiesce_lvgl_owner_with_retry(
    esp_display_presenter_t *presenter,
    uint32_t *cycle_errors,
    uint32_t *total_errors,
    uint32_t batch,
    int cycle)
{
    esp_err_t ret = ESP_FAIL;
    for (unsigned attempt = 1; attempt <= HANDOFF_QUIESCE_MAX_ATTEMPTS;
            ++attempt) {
        ret = esp_display_presenter_quiesce(presenter,
                                            HANDOFF_PAUSE_TIMEOUT_MS);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        ++(*cycle_errors);
        ++(*total_errors);
        ESP_LOGE(TAG,
                 "batch %" PRIu32 " cycle %d: LVGL quiesce failed: %s; "
                 "keeping LVGL active (attempt=%u/%u errors=%" PRIu32 ")",
                 batch, cycle, esp_err_to_name(ret), attempt,
                 HANDOFF_QUIESCE_MAX_ATTEMPTS, *total_errors);
        if (attempt < HANDOFF_QUIESCE_MAX_ATTEMPTS) {
            pump_lvgl_until(esp_timer_get_time() +
                            (int64_t)HANDOFF_QUIESCE_RETRY_MS * 1000);
        }
    }
    return ret;
}

static esp_err_t stop_lvgl_with_retry(void)
{
    esp_err_t ret = ESP_FAIL;
    for (unsigned attempt = 1; attempt <= HANDOFF_STOP_MAX_ATTEMPTS;
            ++attempt) {
        ret = esp_lv_present_stop();
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGE(TAG, "LVGL stop attempt %u/%u failed: %s", attempt,
                 HANDOFF_STOP_MAX_ATTEMPTS, esp_err_to_name(ret));
        if (attempt < HANDOFF_STOP_MAX_ATTEMPTS) {
            vTaskDelay(pdMS_TO_TICKS(HANDOFF_RETRY_DELAY_MS));
        }
    }
    return ret;
}

static esp_err_t run_lvgl_owner_phase(esp_display_presenter_t *presenter,
                                      int cycle,
                                      int cycles,
                                      uint32_t *out_frames)
{
    lv_display_t *display = NULL;
    esp_err_t ret = esp_lv_present_start(presenter, &display);
    if (ret != ESP_OK) {
        return ret;
    }

    handoff_lvgl_screen_create(display, cycle, cycles);
    lv_obj_invalidate(lv_display_get_screen_active(display));
    const int64_t phase_end =
        esp_timer_get_time() + (int64_t)CONFIG_APP_PHASE_SECONDS * 1000000;
    pump_lvgl_until(phase_end);
    *out_frames = esp_lv_present_get_frame_count();
    return ESP_OK;
}

static void run_handoff_cycle(esp_display_presenter_t *presenter,
                              esp_gsp_handle_t *gsp,
                              uint32_t batch,
                              int cycle,
                              uint32_t *total_errors)
{
    uint32_t cycle_errors = 0;
    ESP_LOGI(TAG, "batch %" PRIu32 " cycle %d/%d: GSP phase (%d s)",
             batch, cycle, CONFIG_APP_HANDOFF_CYCLES,
             CONFIG_APP_PHASE_SECONDS);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_PHASE_SECONDS * 1000));

    int64_t start = esp_timer_get_time();
    esp_gsp_esp_lcd_pause_t *pause = NULL;
    esp_err_t ret = esp_gsp_esp_lcd_pause(*gsp, HANDOFF_PAUSE_TIMEOUT_MS,
                                          &pause);
    if (ret != ESP_OK) {
        ++(*total_errors);
        ESP_LOGE(TAG,
                 "FAIL batch=%" PRIu32 " cycle=%d stage=pause error=%s "
                 "total_errors=%" PRIu32,
                 batch, cycle, esp_err_to_name(ret), *total_errors);
        return;
    }
    const uint32_t pause_ms = elapsed_ms(start);

    uint32_t lvgl_frames = 0;
    ret = run_lvgl_owner_phase(presenter, cycle, CONFIG_APP_HANDOFF_CYCLES,
                               &lvgl_frames);
    if (ret != ESP_OK) {
        ++(*total_errors);
        ESP_LOGE(TAG,
                 "FAIL batch=%" PRIu32 " cycle=%d stage=lvgl_start error=%s "
                 "total_errors=%" PRIu32,
                 batch, cycle, esp_err_to_name(ret), *total_errors);
        uint32_t ignored_resume_ms = 0;
        ESP_ERROR_CHECK(resume_gsp_with_retry(pause, gsp,
                                              &ignored_resume_ms));
        return;
    }

    if (lvgl_frames == 0) {
        ++cycle_errors;
        ++(*total_errors);
        ESP_LOGE(TAG, "FAIL batch=%" PRIu32 " cycle=%d stage=lvgl "
                 "reason=no_frames", batch, cycle);
    }

    ret = quiesce_lvgl_owner_with_retry(presenter, &cycle_errors,
                                        total_errors, batch, cycle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "FAIL batch=%" PRIu32 " cycle=%d stage=quiesce error=%s; "
                 "LVGL retains presenter ownership",
                 batch, cycle, esp_err_to_name(ret));
        ESP_ERROR_CHECK(ret);
    }

    handoff_lvgl_screen_delete();
    ret = stop_lvgl_with_retry();
    if (ret != ESP_OK) {
        ++cycle_errors;
        ++(*total_errors);
        ESP_LOGE(TAG,
                 "FAIL batch=%" PRIu32 " cycle=%d stage=lvgl_stop error=%s; "
                 "refusing to resume GSP",
                 batch, cycle, esp_err_to_name(ret));
        ESP_ERROR_CHECK(ret);
    }

    uint32_t resume_ms = 0;
    ret = resume_gsp_with_retry(pause, gsp, &resume_ms);
    if (ret != ESP_OK) {
        ++cycle_errors;
        ++(*total_errors);
        ESP_LOGE(TAG, "FAIL batch=%" PRIu32 " cycle=%d stage=resume error=%s",
                 batch, cycle, esp_err_to_name(ret));
        ESP_ERROR_CHECK(ret);
    }
    ESP_LOGI(TAG,
             "%s batch=%" PRIu32 " cycle=%d/%d pause=%" PRIu32
             "ms lvgl_frames=%" PRIu32 " resume=%" PRIu32
             "ms cycle_errors=%" PRIu32 " total_errors=%" PRIu32,
             cycle_errors == 0 ? "PASS" : "FAIL", batch, cycle,
             CONFIG_APP_HANDOFF_CYCLES, pause_ms, lvgl_frames, resume_ms,
             cycle_errors, *total_errors);
}

void handoff_stress_run(esp_display_presenter_t *presenter,
                        esp_gsp_handle_t gsp)
{
    void *load_timer = esp_gsp_timer_create(gsp, 250, hello_load_timer_cb, NULL);
    ESP_ERROR_CHECK(load_timer == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    ESP_LOGI(TAG, "GSP started on app-owned presenter %ux%u", HW_LCD_H_RES,
             HW_LCD_V_RES);

    uint32_t batch = 1;
    uint32_t total_errors = 0;
    for (;;) {
        for (int cycle = 1; cycle <= CONFIG_APP_HANDOFF_CYCLES; ++cycle) {
            run_handoff_cycle(presenter, &gsp, batch, cycle, &total_errors);
        }
        ESP_LOGI(TAG, "batch %" PRIu32 " complete; continuing "
                 "(total_errors=%" PRIu32 ")", batch, total_errors);
        ++batch;
    }
}
