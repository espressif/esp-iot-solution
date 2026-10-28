/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>

#include "esp_lcd_touch_mux.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

typedef struct {
    esp_lcd_touch_t base;
    esp_lcd_touch_mux_frame_t frame;
    uint32_t read_count;
} fake_touch_t;

static volatile uint32_t s_interrupt_count;

static esp_err_t fake_read(esp_lcd_touch_handle_t touch)
{
    fake_touch_t *fake = (fake_touch_t *)touch;
    fake->read_count++;
    return ESP_OK;
}

static bool fake_get_xy(esp_lcd_touch_handle_t touch, uint16_t *x, uint16_t *y, uint16_t *strength,
                        uint8_t *point_num, uint8_t max_point_num)
{
    fake_touch_t *fake = (fake_touch_t *)touch;
    uint8_t count = fake->frame.count < max_point_num ? fake->frame.count : max_point_num;
    for (uint8_t i = 0; i < count; ++i) {
        x[i] = fake->frame.points[i].x;
        y[i] = fake->frame.points[i].y;
        if (strength != NULL) {
            strength[i] = fake->frame.points[i].strength;
        }
    }
    *point_num = count;
    return count > 0;
}

static void fake_interrupt_callback(esp_lcd_touch_handle_t touch)
{
    (void)touch;
    s_interrupt_count++;
}

static void init_fake_touch(fake_touch_t *fake)
{
    memset(fake, 0, sizeof(*fake));
    fake->base.read_data = fake_read;
    fake->base.get_xy = fake_get_xy;
    fake->base.config.int_gpio_num = GPIO_NUM_NC;
}

TEST_CASE("mux forwards physical samples", "[touch_mux]")
{
    fake_touch_t fake;
    init_fake_touch(&fake);
    fake.frame.count = 1;
    fake.frame.points[0] = (esp_lcd_touch_point_data_t) {
        .x = 12, .y = 34, .strength = 56
    };
    const esp_lcd_touch_mux_config_t config = ESP_LCD_TOUCH_MUX_CONFIG_DEFAULT();
    esp_lcd_touch_handle_t mux = NULL;
    TEST_ESP_OK(esp_lcd_touch_mux_new(&fake.base, &config, &mux));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_lcd_touch_mux_begin(&fake.base, 0));

    esp_lcd_touch_point_data_t points[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {0};
    uint8_t count = 0;
    TEST_ESP_OK(esp_lcd_touch_read_data(mux));
    TEST_ESP_OK(esp_lcd_touch_get_data(mux, points, &count, CONFIG_ESP_LCD_TOUCH_MAX_POINTS));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(12, points[0].x);
    TEST_ASSERT_EQUAL_UINT16(34, points[0].y);
    TEST_ASSERT_EQUAL_UINT32(1, fake.read_count);
    TEST_ESP_OK(esp_lcd_touch_del(mux));
}

TEST_CASE("mux polling replays all virtual frames and release", "[touch_mux]")
{
    fake_touch_t fake;
    init_fake_touch(&fake);
    esp_lcd_touch_mux_config_t config = ESP_LCD_TOUCH_MUX_CONFIG_DEFAULT();
    config.queue_depth = 4;
    esp_lcd_touch_handle_t mux = NULL;
    TEST_ESP_OK(esp_lcd_touch_mux_new(&fake.base, &config, &mux));

    TEST_ESP_OK(esp_lcd_touch_mux_begin(mux, 0));
    for (uint16_t x = 10; x <= 30; x += 10) {
        const esp_lcd_touch_mux_frame_t frame = {
            .count = 1,
            .points = {{.track_id = 7, .x = x, .y = 20}},
        };
        TEST_ESP_OK(esp_lcd_touch_mux_inject(mux, &frame, 0));
    }
    TEST_ESP_OK(esp_lcd_touch_mux_end(mux, 0));

    for (uint16_t expected_x = 10; expected_x <= 30; expected_x += 10) {
        esp_lcd_touch_point_data_t point = {0};
        uint8_t count = 0;
        TEST_ESP_OK(esp_lcd_touch_read_data(mux));
        TEST_ESP_OK(esp_lcd_touch_get_data(mux, &point, &count, 1));
        TEST_ASSERT_EQUAL_UINT8(1, count);
        TEST_ASSERT_EQUAL_UINT16(expected_x, point.x);
        TEST_ASSERT_EQUAL_UINT8(7, point.track_id);
    }
    esp_lcd_touch_point_data_t point = {0};
    uint8_t count = 1;
    TEST_ESP_OK(esp_lcd_touch_read_data(mux));
    TEST_ESP_OK(esp_lcd_touch_get_data(mux, &point, &count, 1));
    TEST_ASSERT_EQUAL_UINT8(0, count);

    esp_lcd_touch_mux_stats_t stats = {0};
    TEST_ESP_OK(esp_lcd_touch_mux_get_stats(mux, &stats));
    TEST_ASSERT_EQUAL_UINT32(4, stats.injected_frames);
    TEST_ASSERT_EQUAL_UINT32(4, stats.consumed_frames);
    TEST_ASSERT_FALSE(stats.virtual_active);
    TEST_ASSERT_EQUAL_UINT32(0, stats.software_interrupts);
    TEST_ESP_OK(esp_lcd_touch_del(mux));
}

TEST_CASE("mux GPTimer wakes interrupt consumer", "[touch_mux]")
{
    fake_touch_t fake;
    init_fake_touch(&fake);
    fake.base.config.int_gpio_num = GPIO_NUM_0;
    const esp_lcd_touch_mux_config_t config = ESP_LCD_TOUCH_MUX_CONFIG_DEFAULT();
    esp_lcd_touch_handle_t mux = NULL;
    TEST_ESP_OK(esp_lcd_touch_mux_new(&fake.base, &config, &mux));
    TEST_ESP_OK(esp_lcd_touch_register_interrupt_callback(mux, fake_interrupt_callback));
    s_interrupt_count = 0;

    TEST_ESP_OK(esp_lcd_touch_mux_begin(mux, 0));
    const esp_lcd_touch_mux_frame_t frame = {.count = 1, .points = {{.x = 1, .y = 2}}};
    TEST_ESP_OK(esp_lcd_touch_mux_inject(mux, &frame, 0));
    for (size_t i = 0; i < 10 && s_interrupt_count == 0; ++i) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0, s_interrupt_count);

    TEST_ESP_OK(esp_lcd_touch_read_data(mux));
    TEST_ESP_OK(esp_lcd_touch_mux_end(mux, 0));
    TEST_ESP_OK(esp_lcd_touch_read_data(mux));
    esp_lcd_touch_mux_stats_t stats = {0};
    TEST_ESP_OK(esp_lcd_touch_mux_get_stats(mux, &stats));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2, stats.software_interrupts);
    TEST_ESP_OK(esp_lcd_touch_del(mux));
}

TEST_CASE("mux cancel replaces pending motion with release", "[touch_mux]")
{
    fake_touch_t fake;
    init_fake_touch(&fake);
    esp_lcd_touch_mux_config_t config = ESP_LCD_TOUCH_MUX_CONFIG_DEFAULT();
    config.queue_depth = 4;
    esp_lcd_touch_handle_t mux = NULL;
    TEST_ESP_OK(esp_lcd_touch_mux_new(&fake.base, &config, &mux));

    TEST_ESP_OK(esp_lcd_touch_mux_begin(mux, 0));
    const esp_lcd_touch_mux_frame_t frame = {.count = 1, .points = {{.x = 1, .y = 2}}};
    TEST_ESP_OK(esp_lcd_touch_mux_inject(mux, &frame, 0));
    TEST_ESP_OK(esp_lcd_touch_mux_cancel(mux));
    TEST_ESP_OK(esp_lcd_touch_read_data(mux));

    esp_lcd_touch_point_data_t point = {0};
    uint8_t count = 1;
    TEST_ESP_OK(esp_lcd_touch_get_data(mux, &point, &count, 1));
    TEST_ASSERT_EQUAL_UINT8(0, count);
    TEST_ESP_OK(esp_lcd_touch_del(mux));
}

TEST_CASE("mux reserves the final release when queue is full", "[touch_mux]")
{
    fake_touch_t fake;
    init_fake_touch(&fake);
    esp_lcd_touch_mux_config_t config = ESP_LCD_TOUCH_MUX_CONFIG_DEFAULT();
    config.queue_depth = 4;
    esp_lcd_touch_handle_t mux = NULL;
    TEST_ESP_OK(esp_lcd_touch_mux_new(&fake.base, &config, &mux));
    TEST_ESP_OK(esp_lcd_touch_mux_begin(mux, 0));

    const esp_lcd_touch_mux_frame_t frame = {.count = 1, .points = {{.x = 1, .y = 2}}};
    for (size_t i = 0; i < 4; ++i) {
        TEST_ESP_OK(esp_lcd_touch_mux_inject(mux, &frame, 0));
    }
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_lcd_touch_mux_inject(mux, &frame, 0));
    TEST_ESP_OK(esp_lcd_touch_mux_end(mux, 0));
    TEST_ESP_OK(esp_lcd_touch_read_data(mux));

    esp_lcd_touch_point_data_t point = {0};
    uint8_t count = 1;
    TEST_ESP_OK(esp_lcd_touch_get_data(mux, &point, &count, 1));
    TEST_ASSERT_EQUAL_UINT8(0, count);
    esp_lcd_touch_mux_stats_t stats = {0};
    TEST_ESP_OK(esp_lcd_touch_mux_get_stats(mux, &stats));
    TEST_ASSERT_EQUAL_UINT32(1, stats.forced_releases);
    TEST_ASSERT_FALSE(stats.virtual_active);
    TEST_ESP_OK(esp_lcd_touch_del(mux));
}

static int64_t replay_with_poll_period(esp_lcd_touch_handle_t mux, uint32_t poll_period_ms)
{
    TEST_ESP_OK(esp_lcd_touch_mux_begin(mux, 0));
    for (uint16_t x = 1; x <= 3; ++x) {
        const esp_lcd_touch_mux_frame_t frame = {.count = 1, .points = {{.x = x, .y = 1}}};
        TEST_ESP_OK(esp_lcd_touch_mux_inject(mux, &frame, 0));
    }
    TEST_ESP_OK(esp_lcd_touch_mux_end(mux, 0));

    int64_t start_us = esp_timer_get_time();
    for (uint16_t expected_x = 1; expected_x <= 3; ++expected_x) {
        if (poll_period_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(poll_period_ms));
        }
        esp_lcd_touch_point_data_t point = {0};
        uint8_t count = 0;
        TEST_ESP_OK(esp_lcd_touch_read_data(mux));
        TEST_ESP_OK(esp_lcd_touch_get_data(mux, &point, &count, 1));
        TEST_ASSERT_EQUAL_UINT8(1, count);
        TEST_ASSERT_EQUAL_UINT16(expected_x, point.x);
    }
    TEST_ESP_OK(esp_lcd_touch_read_data(mux));
    esp_lcd_touch_point_data_t point = {0};
    uint8_t count = 1;
    TEST_ESP_OK(esp_lcd_touch_get_data(mux, &point, &count, 1));
    TEST_ASSERT_EQUAL_UINT8(0, count);
    return esp_timer_get_time() - start_us;
}

TEST_CASE("mux playback follows polling consumer rate", "[touch_mux]")
{
    fake_touch_t fake;
    init_fake_touch(&fake);
    esp_lcd_touch_mux_config_t config = ESP_LCD_TOUCH_MUX_CONFIG_DEFAULT();
    config.queue_depth = 4;
    esp_lcd_touch_handle_t mux = NULL;
    TEST_ESP_OK(esp_lcd_touch_mux_new(&fake.base, &config, &mux));

    int64_t fast_us = replay_with_poll_period(mux, 0);
    int64_t slow_us = replay_with_poll_period(mux, 10);
    TEST_ASSERT_TRUE(slow_us > fast_us + 20000);
    TEST_ESP_OK(esp_lcd_touch_del(mux));
}

void app_main(void)
{
    unity_run_menu();
}
