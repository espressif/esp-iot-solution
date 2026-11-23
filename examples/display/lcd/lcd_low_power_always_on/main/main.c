/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "clock_face.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32P4
#define ACTIVE_CPU_MHZ   100
#define STANDBY_CPU_MHZ  20
#else
#define ACTIVE_CPU_MHZ   160
#define STANDBY_CPU_MHZ  80
#endif

#define NORMAL_MODE_SECONDS   10
#define STANDBY_MODE_SECONDS  130
#define US_PER_SECOND         1000000LL
#define US_PER_MINUTE         (60 * US_PER_SECOND)
#define CLOCK_START_MINUTES   (12 * 60 + 34)
#define MINUTES_PER_DAY       (24 * 60)

static void set_cpu_frequency(int frequency_mhz)
{
    // The panel keeps scanning while the clock task is blocked.
    const esp_pm_config_t config = {
        .max_freq_mhz = frequency_mhz,
        .min_freq_mhz = frequency_mhz,
        .light_sleep_enable = false,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&config));
}

static void wait_until(int64_t deadline_us)
{
    int64_t remaining_us = deadline_us - esp_timer_get_time();
    if (remaining_us > 0) {
        // Round up so the task does not redraw before the minute changes.
        vTaskDelay(pdMS_TO_TICKS((remaining_us + 999) / 1000) + 1);
    }
}

static void run_display_mode(display_mode_t mode, int duration_seconds)
{
    const bool standby = mode == DISPLAY_MODE_STANDBY;

    set_cpu_frequency(ACTIVE_CPU_MHZ);
    const clock_panel_t panel = clock_panel_set_mode(mode);
    ESP_LOGI("clock", "%s mode", standby ? "Standby" : "Normal");

    const int64_t mode_end_us = esp_timer_get_time() + duration_seconds * US_PER_SECOND;
    int previous_minute = CLOCK_FACE_FIRST_DRAW;
    while (esp_timer_get_time() < mode_end_us) {
        const int64_t elapsed_minutes = esp_timer_get_time() / US_PER_MINUTE;
        const int time_of_day = (CLOCK_START_MINUTES + elapsed_minutes) % MINUTES_PER_DAY;

        if (time_of_day != previous_minute) {
            // The panel was opened at the active frequency. Raise it again for minute updates.
            if (standby && previous_minute != CLOCK_FACE_FIRST_DRAW) {
                set_cpu_frequency(ACTIVE_CPU_MHZ);
            }
            clock_face_draw(&panel, previous_minute, time_of_day);
            previous_minute = time_of_day;
            if (standby) {
                set_cpu_frequency(STANDBY_CPU_MHZ);
            }
        }

        const int64_t next_minute_us = (elapsed_minutes + 1) * US_PER_MINUTE;
        wait_until(next_minute_us < mode_end_us ? next_minute_us : mode_end_us);
    }
}

void app_main(void)
{
    while (true) {
        run_display_mode(DISPLAY_MODE_STANDBY, STANDBY_MODE_SECONDS);
        run_display_mode(DISPLAY_MODE_NORMAL, NORMAL_MODE_SECONDS);
    }
}
