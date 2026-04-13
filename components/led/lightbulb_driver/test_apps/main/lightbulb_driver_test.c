/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <unity.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <driver/spi_master.h>

#include <lightbulb.h>

#if 0
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#ifndef MAX
#define MAX(a, b)                                   (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b)                                   (((a) < (b)) ? (a) : (b))
#endif

#define TEST_MEMORY_LEAK_THRESHOLD (-1000)

static size_t before_free_8bit;
static size_t before_free_32bit;

static IRAM_ATTR uint8_t parity_check(uint8_t input)
{
    static uint8_t count_table[256] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
    };

    return (count_table[input] % 2 == 0) ? input : (input |= 0x01);
}

#define MIX_TABLE_SIZE                  15
lightbulb_cct_mapping_data_t table[MIX_TABLE_SIZE] = {
    {.cct_kelvin = 2200, .cct_percentage = 0, .rgbcw = {0.547, 0.0, 0.0, 0.0, 0.453}},
    {.cct_kelvin = 2400, .cct_percentage = 8, .rgbcw = {0.361, 0.019, 0.0, 0.0, 0.620}},
    {.cct_kelvin = 2700, .cct_percentage = 16, .rgbcw = {0.0, 0.00, 0.00, 0.00, 1.0}},
    {.cct_kelvin = 3200, .cct_percentage = 24, .rgbcw = {0.00, 0.215, 0.083, 0.0, 0.702}},
    {.cct_kelvin = 3600, .cct_percentage = 32, .rgbcw = {0.00, 0.293, 0.127, 0.0, 0.579}},
    {.cct_kelvin = 4000, .cct_percentage = 40, .rgbcw = {0.00, 0.360, 0.140, 0.0, 0.499}},
    {.cct_kelvin = 4500, .cct_percentage = 48, .rgbcw = {0.00, 0.397, 0.183, 00.0, 0.420}},
    {.cct_kelvin = 4900, .cct_percentage = 56, .rgbcw = {0.00, 0.425, 0.203, 0.00, 0.372}},
    {.cct_kelvin = 5200, .cct_percentage = 64, .rgbcw = {0.00, 0.420, 0.225, 0.00, 0.355}},
    {.cct_kelvin = 5500, .cct_percentage = 72, .rgbcw = {0.00, 0.421, 0.242, 0.00, 0.337}},
    {.cct_kelvin = 5900, .cct_percentage = 80, .rgbcw = {0.00, 0.420, 0.261, 0.00, 0.319}},
    {.cct_kelvin = 6100, .cct_percentage = 88, .rgbcw = {0.00, 0.432, 0.263, 0.00, 0.304}},
    {.cct_kelvin = 6300, .cct_percentage = 94, .rgbcw = {0.00, 0.443, 0.269, 0.00, 0.288}},
    {.cct_kelvin = 6900, .cct_percentage = 98, .rgbcw = {0.00, 0.443, 0.269, 0.00, 0.288}},
    {.cct_kelvin = 7000, .cct_percentage = 100, .rgbcw = {0.00, 0.444, 0.288, 0.00, 0.268}},
};

#define COLOR_SZIE 24
lightbulb_color_mapping_data_t color_data[COLOR_SZIE] = {
    {.rgbcw_100 = {1.0000, 0.0000, 0.0000}, .rgbcw_50 = {0.5865, 0.2954, 0.1181}, .rgbcw_0 = {0.5098, 0.4118, 0.0784}, .hue = 0},
    {.rgbcw_100 = {0.8285, 0.1715, 0.0000}, .rgbcw_50 = {0.5498, 0.3649, 0.0853}, .rgbcw_0 = {0.4018, 0.4375, 0.1607}, .hue = 15},
    {.rgbcw_100 = {0.7168, 0.2832, 0.0000}, .rgbcw_50 = {0.4850, 0.4261, 0.0888}, .rgbcw_0 = {0.3786, 0.4466, 0.1748}, .hue = 30},
    {.rgbcw_100 = {0.5932, 0.4068, 0.0000}, .rgbcw_50 = {0.4902, 0.4566, 0.0532}, .rgbcw_0 = {0.3981, 0.4466, 0.1553}, .hue = 45},
    {.rgbcw_100 = {0.5164, 0.4836, 0.0000}, .rgbcw_50 = {0.4394, 0.4928, 0.0678}, .rgbcw_0 = {0.3846, 0.4808, 0.1346}, .hue = 60},
    {.rgbcw_100 = {0.3794, 0.6206, 0.0000}, .rgbcw_50 = {0.3728, 0.5658, 0.0614}, .rgbcw_0 = {0.3922, 0.4706, 0.1373}, .hue = 75},
    {.rgbcw_100 = {0.2947, 0.7053, 0.0000}, .rgbcw_50 = {0.3333, 0.5949, 0.0718}, .rgbcw_0 = {0.3762, 0.4752, 0.1485}, .hue = 90},
    {.rgbcw_100 = {0.1188, 0.8812, 0.0000}, .rgbcw_50 = {0.2624, 0.6679, 0.0697}, .rgbcw_0 = {0.3568, 0.4874, 0.1558}, .hue = 105},
    {.rgbcw_100 = {0.0000, 1.0000, 0.0000}, .rgbcw_50 = {0.2212, 0.7019, 0.0769}, .rgbcw_0 = {0.3591, 0.4864, 0.1545}, .hue = 120},
    {.rgbcw_100 = {0.0000, 0.9808, 0.0192}, .rgbcw_50 = {0.2056, 0.6822, 0.1121}, .rgbcw_0 = {0.3411, 0.5047, 0.1542}, .hue = 135},
    {.rgbcw_100 = {0.0000, 0.8978, 0.1022}, .rgbcw_50 = {0.2056, 0.6495, 0.1449}, .rgbcw_0 = {0.3411, 0.5000, 0.1589}, .hue = 150},
    {.rgbcw_100 = {0.0000, 0.8117, 0.1883}, .rgbcw_50 = {0.1963, 0.6308, 0.1729}, .rgbcw_0 = {0.3411, 0.5000, 0.1589}, .hue = 165},
    {.rgbcw_100 = {0.0000, 0.6869, 0.3131}, .rgbcw_50 = {0.1869, 0.6028, 0.2103}, .rgbcw_0 = {0.3364, 0.4860, 0.1776}, .hue = 180},
    {.rgbcw_100 = {0.0000, 0.6096, 0.3904}, .rgbcw_50 = {0.2103, 0.5514, 0.2383}, .rgbcw_0 = {0.3458, 0.4720, 0.1822}, .hue = 195},
    {.rgbcw_100 = {0.0000, 0.3624, 0.6376}, .rgbcw_50 = {0.2336, 0.4720, 0.2944}, .rgbcw_0 = {0.3458, 0.4720, 0.1822}, .hue = 210},
    {.rgbcw_100 = {0.0000, 0.1637, 0.8363}, .rgbcw_50 = {0.2586, 0.4124, 0.3290}, .rgbcw_0 = {0.3458, 0.4720, 0.1822}, .hue = 225},
    {.rgbcw_100 = {0.0000, 0.0000, 1.0000}, .rgbcw_50 = {0.2721, 0.3281, 0.3998}, .rgbcw_0 = {0.3458, 0.4720, 0.1822}, .hue = 240},
    {.rgbcw_100 = {0.1747, 0.0000, 0.8253}, .rgbcw_50 = {0.3413, 0.3125, 0.3462}, .rgbcw_0 = {0.3458, 0.4720, 0.1822}, .hue = 255},
    {.rgbcw_100 = {0.3763, 0.0000, 0.6237}, .rgbcw_50 = {0.4078, 0.2816, 0.3107}, .rgbcw_0 = {0.3458, 0.4720, 0.1822}, .hue = 270},
    {.rgbcw_100 = {0.4717, 0.0000, 0.5283}, .rgbcw_50 = {0.4493, 0.2754, 0.2754}, .rgbcw_0 = {0.3458, 0.4720, 0.1822}, .hue = 285},
    {.rgbcw_100 = {0.6216, 0.0000, 0.3784}, .rgbcw_50 = {0.5049, 0.2524, 0.2427}, .rgbcw_0 = {0.4000, 0.4190, 0.1810}, .hue = 300},
    {.rgbcw_100 = {0.7248, 0.0000, 0.2752}, .rgbcw_50 = {0.5238, 0.2619, 0.2143}, .rgbcw_0 = {0.4000, 0.4190, 0.1810}, .hue = 315},
    {.rgbcw_100 = {0.8268, 0.0000, 0.1732}, .rgbcw_50 = {0.5476, 0.2714, 0.1810}, .rgbcw_0 = {0.4000, 0.4190, 0.1810}, .hue = 330},
    {.rgbcw_100 = {0.9110, 0.0000, 0.0890}, .rgbcw_50 = {0.5762, 0.2810, 0.1429}, .rgbcw_0 = {0.4000, 0.4190, 0.1810}, .hue = 345}
};

TEST_CASE("Parity Check", "[Application Layer]")
{
    for (int i = 0; i < 256; i++) {
        if (i % 16 == 0) {
            printf("\r\n");
        }
        uint8_t result = parity_check(i & 0xFE);
        printf("%d,\t", result);
    }
}

TEST_CASE("Power Check 1", "[Application Layer]")
{
    lightbulb_power_limit_t limit = {
        .color_max_power = 100,
        .color_max_value = 100,
        .color_min_value = 10,
        .white_max_power = 100,
        .white_max_brightness = 100,
        .white_min_brightness = 10
    };

    lightbulb_gamma_config_t Gamma = {
        .balance_coefficient = {1.0, 1.0, 1.0, 1.0, 1.0},
        .color_curve_coefficient = 2.0,
        .white_curve_coefficient = 2.0,
    };

    lightbulb_handle_t handle = NULL;

    //TEST 1, max value: 1023
    lightbulb_config_t config1 = {
        .driver_conf.bp57x8d.current = {10, 10, 10, 20, 20},
        .driver_conf.bp57x8d.iic_clk = 4,
        .driver_conf.bp57x8d.iic_sda = 3,
        .driver_conf.bp57x8d.freq_khz = 300,
        .driver_conf.bp57x8d.enable_iic_queue = true,
        .capability.enable_fade = 0,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.iic_io.red = OUT1,
        .io_conf.iic_io.green = OUT2,
        .io_conf.iic_io.blue = OUT3,
        .io_conf.iic_io.cold_white = OUT5,
        .io_conf.iic_io.warm_yellow = OUT4,
        .external_limit = &limit,
        .gamma_conf = &Gamma,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    handle = lightbulb_new_bp57x8d_device(&config1);
    TEST_ASSERT_NOT_NULL(handle);
    for (int i = 0; i < 360; i += 30) {
        for (int j = 0;  j < 100; j += 10) {
            TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(handle, i, j, 100));
            vTaskDelay(10);
        }
    }
    for (int i = 0; i <= 100; i += 10) {
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_cctb(handle, i, 100));
        vTaskDelay(10);
    }
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));

    //TEST 2, max value: 4096-8192
    lightbulb_config_t config2 = {
        .driver_conf.pwm.freq_hz = 4000,
        .capability.enable_fade = 0,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,

#if CONFIG_IDF_TARGET_ESP32
        .io_conf.pwm_io.red = 25,
        .io_conf.pwm_io.green = 26,
        .io_conf.pwm_io.blue = 27,
#else
        .io_conf.pwm_io.red = 10,
        .io_conf.pwm_io.green = 6,
        .io_conf.pwm_io.blue = 7,
        .io_conf.pwm_io.cold_cct = 4,
        .io_conf.pwm_io.warm_brightness = 5,
#endif

        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    handle = lightbulb_new_pwm_device(&config2);
    TEST_ASSERT_NOT_NULL(handle);
    for (int i = 0; i < 360; i += 30) {
        for (int j = 0;  j < 100; j += 10) {
            TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(handle, i, j, 100));
            vTaskDelay(10);
        }
    }
    for (int i = 0; i <= 100; i += 10) {
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_cctb(handle, i, 100));
        vTaskDelay(10);
    }
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));

    //TEST 3, max value: 255
    lightbulb_config_t config3 = {
        .driver_conf.ws2812.led_num = 22,
        .driver_conf.ws2812.ctrl_io = 2,
        .capability.enable_fade = 0,
        .capability.fade_time_ms = 800,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    handle = lightbulb_new_ws2812_device(&config3);
    TEST_ASSERT_NOT_NULL(handle);
    for (int i = 0; i < 360; i += 30) {
        for (int j = 0;  j < 100; j += 10) {
            TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(handle, i, j, 100));
            vTaskDelay(10);
        }
    }
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
}

TEST_CASE("Power Check 2", "[Application Layer]")
{
    lightbulb_power_limit_t limit = {
        .color_max_power = 200,
        .color_max_value = 100,
        .color_min_value = 10,
        .white_max_power = 200,
        .white_max_brightness = 100,
        .white_min_brightness = 10
    };

    lightbulb_gamma_config_t Gamma = {
        .balance_coefficient = {1.0, 1.0, 1.0, 1.0, 1.0},
        .color_curve_coefficient = 2.0,
        .white_curve_coefficient = 2.0,
    };

    lightbulb_handle_t handle = NULL;

    //TEST 1, max value: 1023
    lightbulb_config_t config1 = {
        .driver_conf.bp57x8d.current = {10, 10, 10, 20, 20},
        .driver_conf.bp57x8d.iic_clk = 4,
        .driver_conf.bp57x8d.iic_sda = 3,
        .driver_conf.bp57x8d.freq_khz = 300,
        .driver_conf.bp57x8d.enable_iic_queue = true,
        .capability.enable_fade = 0,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.iic_io.red = OUT1,
        .io_conf.iic_io.green = OUT2,
        .io_conf.iic_io.blue = OUT3,
        .io_conf.iic_io.cold_white = OUT5,
        .io_conf.iic_io.warm_yellow = OUT4,
        .external_limit = &limit,
        .gamma_conf = &Gamma,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    handle = lightbulb_new_bp57x8d_device(&config1);
    TEST_ASSERT_NOT_NULL(handle);
    for (int i = 0; i < 360; i += 30) {
        for (int j = 0;  j < 100; j += 10) {
            TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(handle, i, j, 100));
            vTaskDelay(10);
        }
    }
    for (int i = 0; i <= 100; i += 10) {
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_cctb(handle, i, 100));
        vTaskDelay(10);
    }
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));

    //TEST 2, max value: 4096-8192
    lightbulb_config_t config2 = {
        .driver_conf.pwm.freq_hz = 4000,
        .capability.enable_fade = 0,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,

#if CONFIG_IDF_TARGET_ESP32
        .io_conf.pwm_io.red = 25,
        .io_conf.pwm_io.green = 26,
        .io_conf.pwm_io.blue = 27,
#else
        .io_conf.pwm_io.red = 10,
        .io_conf.pwm_io.green = 6,
        .io_conf.pwm_io.blue = 7,
        .io_conf.pwm_io.cold_cct = 4,
        .io_conf.pwm_io.warm_brightness = 5,
#endif

        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    handle = lightbulb_new_pwm_device(&config2);
    TEST_ASSERT_NOT_NULL(handle);
    for (int i = 0; i < 360; i += 30) {
        for (int j = 0;  j < 100; j += 10) {
            TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(handle, i, j, 100));
            vTaskDelay(10);
        }
    }
    for (int i = 0; i <= 100; i += 10) {
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_cctb(handle, i, 100));
        vTaskDelay(10);
    }
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));

    //TEST 3, max value: 255
    lightbulb_config_t config3 = {
        .driver_conf.ws2812.led_num = 22,
        .driver_conf.ws2812.ctrl_io = 2,
        .capability.enable_fade = 0,
        .capability.fade_time_ms = 800,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    handle = lightbulb_new_ws2812_device(&config3);
    TEST_ASSERT_NOT_NULL(handle);
    for (int i = 0; i < 360; i += 30) {
        for (int j = 0;  j < 100; j += 10) {
            TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(handle, i, j, 100));
            vTaskDelay(10);
        }
    }
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
}

TEST_CASE("Power Check 3", "[Application Layer]")
{
    lightbulb_power_limit_t limit = {
        .color_max_power = 300,
        .color_max_value = 100,
        .color_min_value = 10,
        .white_max_power = 200,
        .white_max_brightness = 100,
        .white_min_brightness = 10
    };

    lightbulb_gamma_config_t Gamma = {
        .balance_coefficient = {1.0, 1.0, 1.0, 1.0, 1.0},
        .color_curve_coefficient = 2.0,
        .white_curve_coefficient = 2.0,
    };

    lightbulb_handle_t handle = NULL;

    //TEST 1, max value: 1023
    lightbulb_config_t config1 = {
        .driver_conf.bp57x8d.current = {10, 10, 10, 20, 20},
        .driver_conf.bp57x8d.iic_clk = 4,
        .driver_conf.bp57x8d.iic_sda = 3,
        .driver_conf.bp57x8d.freq_khz = 300,
        .driver_conf.bp57x8d.enable_iic_queue = true,
        .capability.enable_fade = 0,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.iic_io.red = OUT1,
        .io_conf.iic_io.green = OUT2,
        .io_conf.iic_io.blue = OUT3,
        .io_conf.iic_io.cold_white = OUT5,
        .io_conf.iic_io.warm_yellow = OUT4,
        .external_limit = &limit,
        .gamma_conf = &Gamma,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    handle = lightbulb_new_bp57x8d_device(&config1);
    TEST_ASSERT_NOT_NULL(handle);
    for (int i = 0; i < 360; i += 30) {
        for (int j = 0;  j < 100; j += 10) {
            TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(handle, i, j, 100));
            vTaskDelay(10);
        }
    }
    for (int i = 0; i <= 100; i += 10) {
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_cctb(handle, i, 100));
        vTaskDelay(10);
    }
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));

    //TEST 2, max value: 4096-8192
    lightbulb_config_t config2 = {
        .driver_conf.pwm.freq_hz = 4000,
        .capability.enable_fade = 0,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,

#if CONFIG_IDF_TARGET_ESP32
        .io_conf.pwm_io.red = 25,
        .io_conf.pwm_io.green = 26,
        .io_conf.pwm_io.blue = 27,
#else
        .io_conf.pwm_io.red = 10,
        .io_conf.pwm_io.green = 6,
        .io_conf.pwm_io.blue = 7,
        .io_conf.pwm_io.cold_cct = 4,
        .io_conf.pwm_io.warm_brightness = 5,
#endif

        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    handle = lightbulb_new_pwm_device(&config2);
    TEST_ASSERT_NOT_NULL(handle);
    for (int i = 0; i < 360; i += 30) {
        for (int j = 0;  j < 100; j += 10) {
            TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(handle, i, j, 100));
            vTaskDelay(10);
        }
    }
    for (int i = 0; i <= 100; i += 10) {
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_cctb(handle, i, 100));
        vTaskDelay(10);
    }
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));

    //TEST 3, max value: 255
    lightbulb_config_t config3 = {
        .driver_conf.ws2812.led_num = 22,
        .driver_conf.ws2812.ctrl_io = 2,
        .capability.enable_fade = 0,
        .capability.fade_time_ms = 800,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    handle = lightbulb_new_ws2812_device(&config3);
    TEST_ASSERT_NOT_NULL(handle);
    for (int i = 0; i < 360; i += 10) {
        for (int j = 0;  j < 100; j += 10) {
            TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(handle, i, j, 100));
            vTaskDelay(10);
        }
    }
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
}

TEST_CASE("Power Check 4", "[Application Layer]")
{
    lightbulb_power_limit_t limit = {
        .color_max_power = 200,
        .color_max_value = 100,
        .color_min_value = 10,
        .white_max_power = 200,
        .white_max_brightness = 100,
        .white_min_brightness = 10
    };

    lightbulb_gamma_config_t Gamma = {
        .balance_coefficient = {1.0, 1.0, 1.0, 1.0, 1.0},
        .color_curve_coefficient = 2.0,
        .white_curve_coefficient = 2.0,
    };

    lightbulb_handle_t handle = NULL;

    lightbulb_config_t config1 = {
        .driver_conf.bp57x8d.current = {10, 10, 10, 20, 20},
        .driver_conf.bp57x8d.iic_clk = 4,
        .driver_conf.bp57x8d.iic_sda = 3,
        .driver_conf.bp57x8d.freq_khz = 300,
        .driver_conf.bp57x8d.enable_iic_queue = true,
        .capability.enable_fade = 0,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .color_mix_mode.precise.table = color_data,
        .color_mix_mode.precise.table_size = COLOR_SZIE,
        .capability.enable_precise_color_control = 1,
        .cct_mix_mode.precise.table_size = MIX_TABLE_SIZE,
        .cct_mix_mode.precise.table = table,
        .capability.enable_precise_cct_control = 1,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.iic_io.red = OUT1,
        .io_conf.iic_io.green = OUT2,
        .io_conf.iic_io.blue = OUT3,
        .io_conf.iic_io.cold_white = OUT5,
        .io_conf.iic_io.warm_yellow = OUT4,
        .external_limit = &limit,
        .gamma_conf = &Gamma,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    handle = lightbulb_new_bp57x8d_device(&config1);
    TEST_ASSERT_NOT_NULL(handle);
    for (int i = 0; i < 360; i += 30) {
        for (int j = 0;  j < 100; j += 10) {
            TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(handle, i, j, 100));
            vTaskDelay(10);
        }
    }
    for (int i = 0; i <= 100; i += 10) {
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_cctb(handle, i, 100));
        vTaskDelay(10);
    }
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
}

#ifdef CONFIG_ENABLE_PWM_DRIVER
TEST_CASE("PWM", "[Underlying Driver]")
{
    //1.Status check
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_channel(PWM_CHANNEL_R, 4095));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_rgb_channel(4095, 4095, 0));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_cctb_or_cw_channel(4095, 4095));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_rgbcctb_or_rgbcw_channel(4095, 4095, 4095, 4095, 4095));

    //2. init check
    driver_pwm_t conf = {
        .freq_hz = 4000,
    };
    TEST_ASSERT_EQUAL(ESP_OK, pwm_init(&conf, NULL, NULL));

    //3. regist Check, step 1
#if CONFIG_IDF_TARGET_ESP32
    TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_R, 25));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_G, 26));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_B, 27));
#else
    TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_R, 10));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_G, 6));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_B, 7));
#endif
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_channel(PWM_CHANNEL_R, 4096));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_channel(PWM_CHANNEL_G, 4096));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_channel(PWM_CHANNEL_B, 4096));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgb_channel(4096, 4096, 4096));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_channel(PWM_CHANNEL_CCT_COLD, 4096));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_channel(PWM_CHANNEL_BRIGHTNESS_WARM, 4096));

    //3. regist Check, step 2
#if CONFIG_IDF_TARGET_ESP32
    TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_CCT_COLD, 14));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_BRIGHTNESS_WARM, 12));
#else
    TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_CCT_COLD, 3));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_BRIGHTNESS_WARM, 4));
#endif
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_channel(PWM_CHANNEL_CCT_COLD, 4096));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_channel(PWM_CHANNEL_BRIGHTNESS_WARM, 4096));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_cctb_or_cw_channel(4096, 4096));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgbcctb_or_rgbcw_channel(4096, 4096, 4096, 4096, 4096));

    //4. Color check
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgbcctb_or_rgbcw_channel(4096, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgbcctb_or_rgbcw_channel(0, 4096, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgbcctb_or_rgbcw_channel(0, 0, 4096, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgbcctb_or_rgbcw_channel(0, 0, 0, 4096, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgbcctb_or_rgbcw_channel(0, 0, 0, 0, 4096));

    //6. deinit
    TEST_ASSERT_EQUAL(ESP_OK, pwm_set_shutdown());
    TEST_ASSERT_EQUAL(ESP_OK, pwm_deinit());
}

TEST_CASE("PWM", "[Application Layer]")
{
    lightbulb_config_t config = {
        .driver_conf.pwm.freq_hz = 4000,
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,

#if CONFIG_IDF_TARGET_ESP32
        .io_conf.pwm_io.red = 25,
        .io_conf.pwm_io.green = 26,
        .io_conf.pwm_io.blue = 27,
#else
        .io_conf.pwm_io.red = 10,
        .io_conf.pwm_io.green = 6,
        .io_conf.pwm_io.blue = 7,
#endif

        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_handle_t handle = lightbulb_new_pwm_device(&config);
    TEST_ASSERT_NOT_NULL(handle);
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(handle, LIGHTING_BASIC_FIVE, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
}
#endif

#ifdef CONFIG_ENABLE_SM2135EH_DRIVER
TEST_CASE("SM2135EH", "[Underlying Driver]")
{
    //1.Status check
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2135eh_set_channel(SM2135EH_CHANNEL_R, 255));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2135eh_set_rgb_channel(255, 255, 0));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2135eh_set_wy_channel(255, 255));

    //2. init check
    driver_sm2135eh_t conf = {
        .rgb_current = SM2135EH_RGB_CURRENT_24MA,
        .wy_current = SM2135EH_WY_CURRENT_50MA,
        .iic_clk = 4,
        .iic_sda = 3,
        .freq_khz = 400,
        .enable_iic_queue = true
    };
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_init(&conf, NULL, NULL));

    //3. regist Check, step 1
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_regist_channel(SM2135EH_CHANNEL_R, SM2135EH_PIN_OUT3));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_regist_channel(SM2135EH_CHANNEL_G, SM2135EH_PIN_OUT2));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_regist_channel(SM2135EH_CHANNEL_B, SM2135EH_PIN_OUT1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_channel(SM2135EH_CHANNEL_R, 1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_channel(SM2135EH_CHANNEL_G, 1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_channel(SM2135EH_CHANNEL_B, 1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_rgb_channel(1, 1, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2135eh_set_channel(SM2135EH_CHANNEL_W, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2135eh_set_channel(SM2135EH_CHANNEL_Y, 1));

    //3. regist Check, step 2
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_regist_channel(SM2135EH_CHANNEL_W, SM2135EH_PIN_OUT5));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_regist_channel(SM2135EH_CHANNEL_Y, SM2135EH_PIN_OUT4));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_channel(SM2135EH_CHANNEL_W, 1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_channel(SM2135EH_CHANNEL_Y, 1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_wy_channel(1, 1));

    //4. Data range check
    // Nothing

    //5. Color check
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_rgb_channel(255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_rgb_channel(0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_rgb_channel(0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_wy_channel(255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_wy_channel(0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_rgbwy_channel(255, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_rgbwy_channel(0, 255, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_rgbwy_channel(0, 0, 255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_rgbwy_channel(0, 0, 0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_rgbwy_channel(0, 0, 0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));

    //6. Current check
    TEST_ASSERT_EQUAL(SM2135EH_WY_CURRENT_0MA, sm2135eh_wy_current_mapping(0));
    TEST_ASSERT_EQUAL(SM2135EH_WY_CURRENT_30MA, sm2135eh_wy_current_mapping(30));
    TEST_ASSERT_EQUAL(SM2135EH_WY_CURRENT_MAX, sm2135eh_wy_current_mapping(31));
    TEST_ASSERT_EQUAL(SM2135EH_WY_CURRENT_72MA, sm2135eh_wy_current_mapping(72));
    TEST_ASSERT_EQUAL(SM2135EH_WY_CURRENT_MAX, sm2135eh_wy_current_mapping(73));
    TEST_ASSERT_EQUAL(SM2135EH_RGB_CURRENT_MAX, sm2135eh_rgb_current_mapping(0));
    TEST_ASSERT_EQUAL(SM2135EH_RGB_CURRENT_9MA, sm2135eh_rgb_current_mapping(9));
    TEST_ASSERT_EQUAL(SM2135EH_RGB_CURRENT_MAX, sm2135eh_rgb_current_mapping(10));
    TEST_ASSERT_EQUAL(SM2135EH_RGB_CURRENT_34MA, sm2135eh_rgb_current_mapping(34));
    TEST_ASSERT_EQUAL(SM2135EH_RGB_CURRENT_44MA, sm2135eh_rgb_current_mapping(44));
    TEST_ASSERT_EQUAL(SM2135EH_RGB_CURRENT_MAX, sm2135eh_rgb_current_mapping(45));

    //7. Deinit
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_set_shutdown());
    // Wait for data transmission to complete
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_deinit());
}

TEST_CASE("SM2135EH", "[Application Layer]")
{
    lightbulb_config_t config = {
        .driver_conf.sm2135eh.rgb_current = SM2135EH_RGB_CURRENT_24MA,
        .driver_conf.sm2135eh.wy_current = SM2135EH_WY_CURRENT_40MA,
        .driver_conf.sm2135eh.iic_clk = 4,
        .driver_conf.sm2135eh.iic_sda = 3,
        .driver_conf.sm2135eh.freq_khz = 400,
        .driver_conf.sm2135eh.enable_iic_queue = true,
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.iic_io.red = OUT3,
        .io_conf.iic_io.green = OUT2,
        .io_conf.iic_io.blue = OUT1,
        .io_conf.iic_io.cold_white = OUT5,
        .io_conf.iic_io.warm_yellow = OUT4,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_handle_t handle = lightbulb_new_sm2135eh_device(&config);
    TEST_ASSERT_NOT_NULL(handle);
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(handle, LIGHTING_BASIC_FIVE, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
}
#endif

#ifdef CONFIG_ENABLE_BP57x8D_DRIVER
TEST_CASE("BP57x8D", "[Underlying Driver]")
{
    //1.Status check
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp57x8d_set_channel(BP57x8D_CHANNEL_R, 1023));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp57x8d_set_rgb_channel(1023, 1023, 0));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp57x8d_set_cw_channel(1023, 1023));

    //2. init check
    driver_bp57x8d_t conf = {
        .current = {10, 10, 10, 20, 20},
        .iic_clk = 4,
        .iic_sda = 3,
        .freq_khz = 300,
        .enable_iic_queue = true
    };
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_init(&conf, NULL, NULL));

    //3. regist Check, step 1
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_regist_channel(BP57x8D_CHANNEL_R, BP57x8D_PIN_OUT1));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_regist_channel(BP57x8D_CHANNEL_G, BP57x8D_PIN_OUT2));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_regist_channel(BP57x8D_CHANNEL_B, BP57x8D_PIN_OUT3));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_channel(BP57x8D_CHANNEL_R, 1));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_channel(BP57x8D_CHANNEL_G, 1));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_channel(BP57x8D_CHANNEL_B, 1));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgb_channel(1, 1, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp57x8d_set_channel(BP57x8D_CHANNEL_C, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp57x8d_set_channel(BP57x8D_CHANNEL_W, 1));

    //3. regist Check, step 2
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_regist_channel(BP57x8D_CHANNEL_C, BP57x8D_PIN_OUT5));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_regist_channel(BP57x8D_CHANNEL_W, BP57x8D_PIN_OUT4));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_channel(BP57x8D_CHANNEL_C, 1));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_channel(BP57x8D_CHANNEL_W, 1));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_cw_channel(1, 1));

    //4. Data range check
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, bp57x8d_set_channel(BP57x8D_CHANNEL_R, 1024));

    //5. Color check
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgb_channel(255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgb_channel(0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgb_channel(0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_cw_channel(255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_cw_channel(0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgbcw_channel(0, 0, 0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgbcw_channel(0, 0, 0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    bp57x8d_set_standby_mode(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    bp57x8d_set_standby_mode(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));

    //6. Deinit
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_shutdown());
    // Wait for data transmission to complete
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_deinit());
}

TEST_CASE("BP57x8D", "[Application Layer]")
{
    lightbulb_config_t config = {
        .driver_conf.bp57x8d.current = {10, 10, 10, 20, 20},
        .driver_conf.bp57x8d.iic_clk = 4,
        .driver_conf.bp57x8d.iic_sda = 3,
        .driver_conf.bp57x8d.freq_khz = 300,
        .driver_conf.bp57x8d.enable_iic_queue = true,
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.iic_io.red = OUT1,
        .io_conf.iic_io.green = OUT2,
        .io_conf.iic_io.blue = OUT3,
        .io_conf.iic_io.cold_white = OUT5,
        .io_conf.iic_io.warm_yellow = OUT4,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_handle_t handle = lightbulb_new_bp57x8d_device(&config);
    TEST_ASSERT_NOT_NULL(handle);
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(handle, LIGHTING_BASIC_FIVE, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
}
#endif

#ifdef CONFIG_ENABLE_BP1658CJ_DRIVER
TEST_CASE("BP1658CJ", "[Underlying Driver]")
{
    //1.Status check
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp1658cj_set_channel(BP1658CJ_CHANNEL_R, 1023));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp1658cj_set_rgb_channel(1023, 1023, 0));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp1658cj_set_cw_channel(1023, 1023));

    //2. init check
    driver_bp1658cj_t conf = {
        .rgb_current = BP1658CJ_RGB_CURRENT_10MA,
        .cw_current = BP1658CJ_CW_CURRENT_30MA,
        .iic_clk = 4,
        .iic_sda = 3,
        .freq_khz = 300,
        .enable_iic_queue = true
    };
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_init(&conf, NULL, NULL));

    //3. regist Check, step 1
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_regist_channel(BP1658CJ_CHANNEL_R, BP1658CJ_PIN_OUT1));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_regist_channel(BP1658CJ_CHANNEL_G, BP1658CJ_PIN_OUT2));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_regist_channel(BP1658CJ_CHANNEL_B, BP1658CJ_PIN_OUT3));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_channel(BP1658CJ_CHANNEL_R, 1));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_channel(BP1658CJ_CHANNEL_G, 1));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_channel(BP1658CJ_CHANNEL_B, 1));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgb_channel(1, 1, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp1658cj_set_channel(BP1658CJ_CHANNEL_C, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp1658cj_set_channel(BP1658CJ_CHANNEL_W, 1));

    //3. regist Check, step 2
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_regist_channel(BP1658CJ_CHANNEL_C, BP1658CJ_PIN_OUT5));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_regist_channel(BP1658CJ_CHANNEL_W, BP1658CJ_PIN_OUT4));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_channel(BP1658CJ_CHANNEL_C, 1));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_channel(BP1658CJ_CHANNEL_W, 1));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_cw_channel(1, 1));

    //4. Data range check
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, bp1658cj_set_channel(BP1658CJ_CHANNEL_R, 1024));

    //5. Color check
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgb_channel(255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgb_channel(0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgb_channel(0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_cw_channel(255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_cw_channel(0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgbcw_channel(0, 0, 0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgbcw_channel(0, 0, 0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    bp1658cj_set_sleep_mode(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    bp1658cj_set_sleep_mode(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));

    //6. Current check
    TEST_ASSERT_EQUAL(BP1658CJ_RGB_CURRENT_0MA, bp1658cj_rgb_current_mapping(0));
    TEST_ASSERT_EQUAL(BP1658CJ_RGB_CURRENT_MAX, bp1658cj_rgb_current_mapping(5));
    TEST_ASSERT_EQUAL(BP1658CJ_RGB_CURRENT_10MA, bp1658cj_rgb_current_mapping(10));
    TEST_ASSERT_EQUAL(BP1658CJ_RGB_CURRENT_90MA, bp1658cj_rgb_current_mapping(90));
    TEST_ASSERT_EQUAL(BP1658CJ_RGB_CURRENT_150MA, bp1658cj_rgb_current_mapping(150));
    TEST_ASSERT_EQUAL(BP1658CJ_RGB_CURRENT_MAX, bp1658cj_rgb_current_mapping(160));
    TEST_ASSERT_EQUAL(BP1658CJ_CW_CURRENT_0MA, bp1658cj_cw_current_mapping(0));
    TEST_ASSERT_EQUAL(BP1658CJ_CW_CURRENT_MAX, bp1658cj_cw_current_mapping(2));
    TEST_ASSERT_EQUAL(BP1658CJ_CW_CURRENT_50MA, bp1658cj_cw_current_mapping(50));
    TEST_ASSERT_EQUAL(BP1658CJ_CW_CURRENT_75MA, bp1658cj_cw_current_mapping(75));
    TEST_ASSERT_EQUAL(BP1658CJ_CW_CURRENT_MAX, bp1658cj_cw_current_mapping(80));

    //7. deinit
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_shutdown());
    // Wait for data transmission to complete
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_deinit());
}

TEST_CASE("BP1658CJ", "[Application Layer]")
{
    lightbulb_config_t config = {
        .driver_conf.bp1658cj.rgb_current = BP1658CJ_RGB_CURRENT_10MA,
        .driver_conf.bp1658cj.cw_current = BP1658CJ_CW_CURRENT_30MA,
        .driver_conf.bp1658cj.iic_clk = 4,
        .driver_conf.bp1658cj.iic_sda = 3,
        .driver_conf.bp1658cj.freq_khz = 300,
        .driver_conf.bp1658cj.enable_iic_queue = true,
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.iic_io.red = OUT3,
        .io_conf.iic_io.green = OUT4,
        .io_conf.iic_io.blue = OUT5,
        .io_conf.iic_io.cold_white = OUT1,
        .io_conf.iic_io.warm_yellow = OUT2,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_handle_t handle = lightbulb_new_bp1658cj_device(&config);
    TEST_ASSERT_NOT_NULL(handle);
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(handle, LIGHTING_BASIC_FIVE, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
}
#endif

#ifdef CONFIG_ENABLE_SM2182E_DRIVER
TEST_CASE("SM2182E", "[Underlying Driver]")
{
    //1.Status check
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2182e_set_cw_channel(1024, 1024));

    //2. init check
    driver_sm2182e_t conf = {
        .cw_current = SM2182E_CW_CURRENT_30MA,
        .iic_clk = 4,
        .iic_sda = 3,
        .freq_khz = 300,
        .enable_iic_queue = true
    };
    TEST_ASSERT_EQUAL(ESP_OK, sm2182e_init(&conf, NULL, NULL));

    //3. regist Check, step 1
    TEST_ASSERT_EQUAL(ESP_OK, sm2182e_regist_channel(SM2182E_CHANNEL_C, SM2182E_PIN_OUT1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2182e_regist_channel(SM2182E_CHANNEL_W, SM2182E_PIN_OUT2));

    //4. Data range check
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, sm2182e_set_cw_channel(SM2182E_CHANNEL_C, 1024));

    //5. Color check
    TEST_ASSERT_EQUAL(ESP_OK, sm2182e_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2182e_set_cw_channel(0, 1023));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2182e_set_cw_channel(1023, 0));

    //6. Current
    TEST_ASSERT_EQUAL(SM2182E_CW_CURRENT_5MA, sm2182e_cw_current_mapping(5));
    TEST_ASSERT_EQUAL(SM2182E_CW_CURRENT_80MA, sm2182e_cw_current_mapping(80));
    TEST_ASSERT_EQUAL(SM2182E_CW_CURRENT_MAX, sm2182e_cw_current_mapping(6));

    //7. deinit
    TEST_ASSERT_EQUAL(ESP_OK, sm2182e_set_shutdown());
    // Wait for data transmission to complete
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, sm2182e_deinit());
}

TEST_CASE("SM2182E", "[Application Layer]")
{
    lightbulb_config_t config = {
        .driver_conf.sm2182e.cw_current = SM2182E_CW_CURRENT_30MA,
        .driver_conf.sm2182e.iic_clk = 4,
        .driver_conf.sm2182e.iic_sda = 3,
        .driver_conf.sm2182e.freq_khz = 300,
        .driver_conf.sm2182e.enable_iic_queue = true,
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_2CH_CW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.iic_io.cold_white = OUT1,
        .io_conf.iic_io.warm_yellow = OUT2,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_WHITE,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.brightness = 100,
        .init_status.cct_percentage = 0,
    };
    lightbulb_handle_t handle = lightbulb_new_sm2182e_device(&config);
    TEST_ASSERT_NOT_NULL(handle);
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(handle, LIGHTING_COLD_TO_WARM | LIGHTING_WARM_TO_COLD, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
}
#endif

#ifdef CONFIG_ENABLE_SM2x35EGH_DRIVER
TEST_CASE("SM2x35EGH", "[Underlying Driver]")
{
    //1.Status check
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_R, 1023));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2x35egh_set_rgb_channel(1023, 1023, 0));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2x35egh_set_cw_channel(1023, 1023));

    //2. init check
    driver_sm2x35egh_t conf = {
        .rgb_current = SM2235EGH_CW_CURRENT_10MA,
        .cw_current = SM2235EGH_CW_CURRENT_40MA,
        .iic_clk = 4,
        .iic_sda = 3,
        .freq_khz = 300,
        .enable_iic_queue = true
    };
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_init(&conf, NULL, NULL));

    //3. regist Check, step 1
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_regist_channel(SM2x35EGH_CHANNEL_R, SM2x35EGH_PIN_OUT3));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_regist_channel(SM2x35EGH_CHANNEL_G, SM2x35EGH_PIN_OUT2));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_regist_channel(SM2x35EGH_CHANNEL_B, SM2x35EGH_PIN_OUT1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_R, 1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_G, 1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_B, 1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_rgb_channel(1, 1, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_C, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_W, 1));

    //3. regist Check, step 2
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_regist_channel(SM2x35EGH_CHANNEL_C, SM2x35EGH_PIN_OUT4));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_regist_channel(SM2x35EGH_CHANNEL_W, SM2x35EGH_PIN_OUT5));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_C, 1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_W, 1));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_cw_channel(1, 1));

    //4. Data range check
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_R, 1024));

    //5. Color check
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_rgb_channel(255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_rgb_channel(0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_rgb_channel(0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_cw_channel(255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_cw_channel(0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_rgbcw_channel(0, 0, 0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_rgbcw_channel(0, 0, 0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    sm2x35egh_set_standby_mode(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    sm2x35egh_set_standby_mode(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));

    //6. Current
    TEST_ASSERT_EQUAL(SM2235EGH_RGB_CURRENT_MAX, sm2235egh_rgb_current_mapping(0));
    TEST_ASSERT_EQUAL(SM2235EGH_RGB_CURRENT_4MA, sm2235egh_rgb_current_mapping(4));
    TEST_ASSERT_EQUAL(SM2235EGH_RGB_CURRENT_MAX, sm2235egh_rgb_current_mapping(6));
    TEST_ASSERT_EQUAL(SM2235EGH_RGB_CURRENT_40MA, sm2235egh_rgb_current_mapping(40));
    TEST_ASSERT_EQUAL(SM2235EGH_RGB_CURRENT_64MA, sm2235egh_rgb_current_mapping(64));
    TEST_ASSERT_EQUAL(SM2235EGH_RGB_CURRENT_MAX, sm2235egh_rgb_current_mapping(68));
    TEST_ASSERT_EQUAL(SM2335EGH_RGB_CURRENT_MAX, sm2335egh_rgb_current_mapping(5));
    TEST_ASSERT_EQUAL(SM2335EGH_RGB_CURRENT_10MA, sm2335egh_rgb_current_mapping(10));
    TEST_ASSERT_EQUAL(SM2335EGH_RGB_CURRENT_MAX, sm2335egh_rgb_current_mapping(15));
    TEST_ASSERT_EQUAL(SM2335EGH_RGB_CURRENT_100MA, sm2335egh_rgb_current_mapping(100));
    TEST_ASSERT_EQUAL(SM2335EGH_RGB_CURRENT_160MA, sm2335egh_rgb_current_mapping(160));
    TEST_ASSERT_EQUAL(SM2335EGH_RGB_CURRENT_MAX, sm2335egh_rgb_current_mapping(170));
    TEST_ASSERT_EQUAL(SM2235EGH_CW_CURRENT_MAX, sm2235egh_cw_current_mapping(0));
    TEST_ASSERT_EQUAL(SM2235EGH_CW_CURRENT_5MA, sm2235egh_cw_current_mapping(5));
    TEST_ASSERT_EQUAL(SM2235EGH_CW_CURRENT_MAX, sm2235egh_cw_current_mapping(6));
    TEST_ASSERT_EQUAL(SM2235EGH_CW_CURRENT_55MA, sm2235egh_cw_current_mapping(55));
    TEST_ASSERT_EQUAL(SM2235EGH_CW_CURRENT_80MA, sm2235egh_cw_current_mapping(80));
    TEST_ASSERT_EQUAL(SM2235EGH_CW_CURRENT_MAX, sm2235egh_cw_current_mapping(85));
    TEST_ASSERT_EQUAL(SM2335EGH_CW_CURRENT_MAX, sm2335egh_cw_current_mapping(0));
    TEST_ASSERT_EQUAL(SM2335EGH_CW_CURRENT_5MA, sm2335egh_cw_current_mapping(5));
    TEST_ASSERT_EQUAL(SM2335EGH_CW_CURRENT_MAX, sm2335egh_cw_current_mapping(6));
    TEST_ASSERT_EQUAL(SM2335EGH_CW_CURRENT_55MA, sm2335egh_cw_current_mapping(55));
    TEST_ASSERT_EQUAL(SM2335EGH_CW_CURRENT_80MA, sm2335egh_cw_current_mapping(80));
    TEST_ASSERT_EQUAL(SM2335EGH_CW_CURRENT_MAX, sm2335egh_cw_current_mapping(85));

    //7. deinit
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_set_shutdown());
    // Wait for data transmission to complete
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_deinit());
}

TEST_CASE("SM2x35EGH", "[Application Layer]")
{
    lightbulb_config_t config = {
        .driver_conf.sm2x35egh.rgb_current = SM2235EGH_RGB_CURRENT_20MA,
        .driver_conf.sm2x35egh.cw_current = SM2235EGH_CW_CURRENT_40MA,
        .driver_conf.sm2x35egh.iic_clk = 4,
        .driver_conf.sm2x35egh.iic_sda = 3,
        .driver_conf.sm2x35egh.freq_khz = 400,
        .driver_conf.sm2x35egh.enable_iic_queue = true,
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.iic_io.red = OUT3,
        .io_conf.iic_io.green = OUT2,
        .io_conf.iic_io.blue = OUT1,
        .io_conf.iic_io.cold_white = OUT4,
        .io_conf.iic_io.warm_yellow = OUT5,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_handle_t handle = lightbulb_new_sm2x35egh_device(&config);
    TEST_ASSERT_NOT_NULL(handle);
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(handle, LIGHTING_BASIC_FIVE, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
}
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
TEST_CASE("WS2812", "[Underlying Driver]")
{
    driver_ws2812_t ws2812 = {
        .led_num = 1,
        .ctrl_io = 4,
    };
    TEST_ASSERT_EQUAL(ESP_OK, ws2812_init(&ws2812, NULL, NULL));
    ws2812_set_rgb_channel(255, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    ws2812_set_rgb_channel(0, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    ws2812_set_rgb_channel(0, 0, 255);
    TEST_ASSERT_EQUAL(ESP_OK, ws2812_deinit());
}

TEST_CASE("WS2812", "[Application Layer]")
{
    lightbulb_config_t config = {
        .driver_conf.ws2812.led_num = 22,
        .driver_conf.ws2812.ctrl_io = 2,
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_handle_t handle = lightbulb_new_ws2812_device(&config);
    TEST_ASSERT_NOT_NULL(handle);
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(handle, LIGHTING_RAINBOW, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
}
#endif

#ifdef CONFIG_ENABLE_KP18058_DRIVER
TEST_CASE("KP18058", "[Underlying Driver]")
{
    driver_kp18058_t kp18058 = {
        .rgb_current_multiple = 3,
        .cw_current_multiple = 10,
        .iic_clk = 5,
        .iic_sda = 10,
        .iic_freq_khz = 300,
        .enable_iic_queue = true,
    };
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_init(&kp18058, NULL, NULL));

    //3. regist Check, step 1
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_regist_channel(KP18058_CHANNEL_R, KP18058_PIN_OUT3));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_regist_channel(KP18058_CHANNEL_G, KP18058_PIN_OUT2));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_regist_channel(KP18058_CHANNEL_B, KP18058_PIN_OUT1));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_channel(KP18058_CHANNEL_R, 1));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_channel(KP18058_CHANNEL_G, 1));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_channel(KP18058_CHANNEL_B, 1));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgb_channel(1, 1, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, kp18058_set_channel(KP18058_CHANNEL_C, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, kp18058_set_channel(KP18058_CHANNEL_W, 1));

    //3. regist Check, step 2
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_regist_channel(KP18058_CHANNEL_C, KP18058_PIN_OUT4));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_regist_channel(KP18058_CHANNEL_W, KP18058_PIN_OUT5));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_channel(KP18058_CHANNEL_C, 1));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_channel(KP18058_CHANNEL_W, 1));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_cw_channel(1, 1));

    //4. Data range check
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, kp18058_set_channel(KP18058_CHANNEL_R, 1024));

    //5. Color check
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgb_channel(255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgb_channel(0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgb_channel(0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_cw_channel(255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_cw_channel(0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgbcw_channel(0, 0, 0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgbcw_channel(0, 0, 0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    kp18058_set_standby_mode(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    kp18058_set_standby_mode(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgbcw_channel(0, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));

    //6. Current check
    TEST_ASSERT_EQUAL(-1, kp18058_rgb_current_mapping(0));
    TEST_ASSERT_EQUAL(-1, kp18058_rgb_current_mapping(1));
    TEST_ASSERT_EQUAL(0, kp18058_rgb_current_mapping(1.5));
    TEST_ASSERT_EQUAL(-1, kp18058_rgb_current_mapping(1.6));
    TEST_ASSERT_EQUAL(9, kp18058_rgb_current_mapping(15));
    TEST_ASSERT_EQUAL(31, kp18058_rgb_current_mapping(48));
    TEST_ASSERT_EQUAL(-1, kp18058_rgb_current_mapping(49.5));
    TEST_ASSERT_EQUAL(-1, kp18058_cw_current_mapping(-1));
    TEST_ASSERT_EQUAL(0, kp18058_cw_current_mapping(0));
    TEST_ASSERT_EQUAL(-1, kp18058_cw_current_mapping(1));
    TEST_ASSERT_EQUAL(1, kp18058_cw_current_mapping(2.5));
    TEST_ASSERT_EQUAL(10, kp18058_cw_current_mapping(25));
    TEST_ASSERT_EQUAL(11, kp18058_cw_current_mapping(27.5));
    TEST_ASSERT_EQUAL(-1, kp18058_cw_current_mapping(28));
    TEST_ASSERT_EQUAL(31, kp18058_cw_current_mapping(77.5));
    TEST_ASSERT_EQUAL(-1, kp18058_cw_current_mapping(80));
    TEST_ASSERT_EQUAL(KP18058_COMPENSATION_VOLTAGE_INVALID, kp18058_compensation_mapping(0));
    TEST_ASSERT_EQUAL(KP18058_COMPENSATION_VOLTAGE_140V, kp18058_compensation_mapping(140));
    TEST_ASSERT_EQUAL(KP18058_COMPENSATION_VOLTAGE_330V, kp18058_compensation_mapping(330));
    TEST_ASSERT_EQUAL(KP18058_SLOPE_INVALID, kp18058_slope_mapping(0));
    TEST_ASSERT_EQUAL(KP18058_SLOPE_7_5, kp18058_slope_mapping(7.5));
    TEST_ASSERT_EQUAL(KP18058_SLOPE_15_0, kp18058_slope_mapping(15));
    TEST_ASSERT_EQUAL(KP18058_CHOPPING_INVALID, kp18058_chopping_freq_mapping(0));
    TEST_ASSERT_EQUAL(KP18058_CHOPPING_4KHZ, kp18058_chopping_freq_mapping(4000));
    TEST_ASSERT_EQUAL(KP18058_CHOPPING_500HZ, kp18058_chopping_freq_mapping(500));

    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_rgbcw_channel(512, 512, 512, 512, 512));
    vTaskDelay(pdMS_TO_TICKS(10000));
    //7. Deinit
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_shutdown());
    // Wait for data transmission to complete
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_deinit());
}

TEST_CASE("KP18058", "[Application Layer]")
{
    lightbulb_config_t config = {
        .driver_conf.kp18058.rgb_current_multiple = 3,
        .driver_conf.kp18058.cw_current_multiple = 10,
        .driver_conf.kp18058.iic_clk = 5,
        .driver_conf.kp18058.iic_sda = 10,
        .driver_conf.kp18058.iic_freq_khz = 300,
        .driver_conf.kp18058.enable_iic_queue = true,
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.iic_io.red = OUT3,
        .io_conf.iic_io.green = OUT2,
        .io_conf.iic_io.blue = OUT1,
        .io_conf.iic_io.cold_white = OUT5,
        .io_conf.iic_io.warm_yellow = OUT4,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_handle_t handle = lightbulb_new_kp18058_device(&config);
    TEST_ASSERT_NOT_NULL(handle);
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(handle, LIGHTING_BASIC_FIVE, 1000);
    vTaskDelay(pdMS_TO_TICKS(2000));
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
}
#endif

#ifdef CONFIG_ENABLE_SM16825E_DRIVER
TEST_CASE("SM16825E", "[Underlying Driver]")
{
    driver_sm16825e_t conf = {
        .led_num = 1,
        .ctrl_io = 9,
        .current = {150, 150, 150, 150, 150},
    };
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_init(&conf, NULL, NULL));

    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_R, SM16825E_PIN_OUTW));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_G, SM16825E_PIN_OUTR));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_B, SM16825E_PIN_OUTY));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_W, SM16825E_PIN_OUTB));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_Y, SM16825E_PIN_OUTG));

    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, sm16825e_set_channel_current(SM16825E_CHANNEL_R, 9));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, sm16825e_set_channel_current(SM16825E_CHANNEL_R, 301));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_channel_current(SM16825E_CHANNEL_R, 10));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_channel_current(SM16825E_CHANNEL_G, 300));

    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(65535, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(0, 65535, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(0, 0, 65535, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(0, 0, 0, 65535, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(0, 0, 0, 0, 65535));
    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_standby(true));
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(1000, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(1000));

    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_standby(true));
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(0, 2000, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(1000));

    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_standby(true));
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(0, 0, 3000, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(1000));

    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_standby(false));
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(0, 0, 0, 4000, 0));
    vTaskDelay(pdMS_TO_TICKS(1000));

    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_shutdown());
    vTaskDelay(pdMS_TO_TICKS(500));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_deinit());
    // Free SPI bus for next SM16825E test
    spi_bus_free(SPI2_HOST);
}

TEST_CASE("SM16825E", "[Application Layer]")
{
    lightbulb_config_t config = {
        .driver_conf.sm16825e.led_num = 1,
        .driver_conf.sm16825e.ctrl_io = 9,
        .driver_conf.sm16825e.current = {150, 150, 150, 150, 150},
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.sm16825e_io.red = OUT4,
        .io_conf.sm16825e_io.green = OUT1,
        .io_conf.sm16825e_io.blue = OUT5,
        .io_conf.sm16825e_io.cold_white = OUT3,
        .io_conf.sm16825e_io.warm_yellow = OUT2,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_handle_t handle = lightbulb_new_sm16825e_device(&config);
    TEST_ASSERT_NOT_NULL(handle);
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(handle, LIGHTING_BASIC_FIVE, 1000);
    vTaskDelay(pdMS_TO_TICKS(2000));
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(handle));
    spi_bus_free(SPI2_HOST);
}

TEST_CASE("SM16825E partial channel registration", "[Underlying Driver]")
{
    // Case 0: 0 channel
    {
        driver_sm16825e_t conf = {
            .led_num = 1,
            .ctrl_io = 9,
            .current = {100, 100, 100, 100, 100},
        };
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_init(&conf, NULL, NULL));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(1000, 2000, 3000, 4000, 5000));
        vTaskDelay(pdMS_TO_TICKS(1000));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_deinit());
        spi_bus_free(SPI2_HOST);
    }

    // Case 1: Only register 2 channels (W, Y)
    {
        driver_sm16825e_t conf = {
            .led_num = 1,
            .ctrl_io = 9,
            .current = {100, 100, 100, 100, 100},
        };
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_init(&conf, NULL, NULL));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_W, SM16825E_PIN_OUTW));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_Y, SM16825E_PIN_OUTY));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(0, 0, 0, 65535, 65535));
        vTaskDelay(pdMS_TO_TICKS(1000));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_deinit());
        spi_bus_free(SPI2_HOST);
    }

    // Case 2: Only register 3 channels (R, G, B)
    {
        driver_sm16825e_t conf = {
            .led_num = 1,
            .ctrl_io = 9,
            .current = {100, 100, 100, 100, 100},
        };
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_init(&conf, NULL, NULL));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_R, SM16825E_PIN_OUTR));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_G, SM16825E_PIN_OUTG));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_B, SM16825E_PIN_OUTB));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(65535, 65535, 65535, 0, 0));
        vTaskDelay(pdMS_TO_TICKS(1000));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_deinit());
        spi_bus_free(SPI2_HOST);
    }

    // Case 3: Register 5 channels (R, G, B, W, Y), in standard mapping order
    {
        driver_sm16825e_t conf = {
            .led_num = 1,
            .ctrl_io = 9,
            .current = {100, 100, 100, 100, 100},
        };
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_init(&conf, NULL, NULL));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_R, SM16825E_PIN_OUTR));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_G, SM16825E_PIN_OUTG));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_B, SM16825E_PIN_OUTB));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_W, SM16825E_PIN_OUTW));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_Y, SM16825E_PIN_OUTY));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(100, 200, 300, 400, 500));
        vTaskDelay(pdMS_TO_TICKS(1000));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_deinit());
        spi_bus_free(SPI2_HOST);
    }

    // Case 4: Register in random order (shuffle mapping), then write a full frame, confirm that it does not crash
    {
        driver_sm16825e_t conf = {
            .led_num = 1,
            .ctrl_io = 9,
            .current = {100, 100, 100, 100, 100},
        };
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_init(&conf, NULL, NULL));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_R, SM16825E_PIN_OUTW));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_G, SM16825E_PIN_OUTR));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_B, SM16825E_PIN_OUTY));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_W, SM16825E_PIN_OUTB));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_Y, SM16825E_PIN_OUTG));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(123, 456, 789, 321, 654));
        vTaskDelay(pdMS_TO_TICKS(1000));
        TEST_ASSERT_EQUAL(ESP_OK, sm16825e_deinit());
        spi_bus_free(SPI2_HOST);
    }
}
#endif

TEST_CASE("I2C Multi-instance Prevention", "[Underlying Driver][I2C]")
{
    ESP_LOGI("TEST", "Step 1: Initializing BP5758D (first I2C device)");
    driver_bp57x8d_t bp5758d_conf = {
        .current = {10, 10, 10, 20, 20},
        .iic_clk = 4,
        .iic_sda = 3,
        .freq_khz = 300,
        .enable_iic_queue = true
    };
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_init(&bp5758d_conf, NULL, NULL));
    ESP_LOGI("TEST", "✔ BP5758D initialized successfully");

    ESP_LOGI("TEST", "Step 2: Attempting to initialize BP1658CJ (second I2C device, should fail)");
    driver_bp1658cj_t bp1658cj_conf = {
        .rgb_current = BP1658CJ_RGB_CURRENT_10MA,
        .cw_current = BP1658CJ_CW_CURRENT_30MA,
        .iic_clk = 4,
        .iic_sda = 3,
        .freq_khz = 300,
        .enable_iic_queue = true
    };

    esp_err_t ret = bp1658cj_init(&bp1658cj_conf, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
    ESP_LOGI("TEST", "✔ BP1658CJ init correctly rejected (ESP_ERR_INVALID_STATE)");

    ESP_LOGI("TEST", "Step 3: Verifying BP5758D still functions correctly");
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_regist_channel(BP57x8D_CHANNEL_R, BP57x8D_PIN_OUT1));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_regist_channel(BP57x8D_CHANNEL_G, BP57x8D_PIN_OUT2));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_regist_channel(BP57x8D_CHANNEL_B, BP57x8D_PIN_OUT3));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_rgb_channel(255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_set_shutdown());
    ESP_LOGI("TEST", "✔ BP5758D still functioning normally");

    ESP_LOGI("TEST", "Step 4: Deinitializing BP5758D");
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_deinit());
    ESP_LOGI("TEST", "✔ BP5758D deinitialized successfully");

    ESP_LOGI("TEST", "Step 5: Re-attempting to initialize BP1658CJ (should succeed now)");
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_init(&bp1658cj_conf, NULL, NULL));
    ESP_LOGI("TEST", "✔ BP1658CJ initialized successfully after BP5758D was deinitialized");

    ESP_LOGI("TEST", "Step 6: Verifying BP1658CJ functions correctly");
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_regist_channel(BP1658CJ_CHANNEL_R, BP1658CJ_PIN_OUT1));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_regist_channel(BP1658CJ_CHANNEL_G, BP1658CJ_PIN_OUT2));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_regist_channel(BP1658CJ_CHANNEL_B, BP1658CJ_PIN_OUT3));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_rgb_channel(0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_set_shutdown());
    ESP_LOGI("TEST", "✔ BP1658CJ functioning normally");

    ESP_LOGI("TEST", "Step 7: Attempting to initialize BP5758D again (should fail)");
    ret = bp57x8d_init(&bp5758d_conf, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
    ESP_LOGI("TEST", "✔ BP5758D init correctly rejected while BP1658CJ is active");

    ESP_LOGI("TEST", "Step 8: Final cleanup");
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_deinit());
    ESP_LOGI("TEST", "✔ BP1658CJ deinitialized successfully");

    ESP_LOGI("TEST", "✅ I2C multi-instance prevention test completed successfully!");
}

TEST_CASE("SPI Multi-instance Prevention", "[Underlying Driver][SPI]")
{
    ESP_LOGI("TEST", "=== Testing SPI2_HOST resource conflict between WS2812 and SM16825E ===");

    ESP_LOGI("TEST", "Step 1: Initializing WS2812 (first SPI device using SPI2_HOST)");
    driver_ws2812_t ws2812_conf = {
        .led_num = 1,
        .ctrl_io = 4,
    };
    TEST_ASSERT_EQUAL(ESP_OK, ws2812_init(&ws2812_conf, NULL, NULL));
    ESP_LOGI("TEST", "✔ WS2812 initialized successfully on SPI2_HOST");

    ESP_LOGI("TEST", "Step 2: Attempting to initialize SM16825E (second SPI device, should fail)");
    driver_sm16825e_t sm16825e_conf = {
        .led_num = 1,
        .ctrl_io = 9,
        .current = {150, 150, 150, 150, 150},
    };

    esp_err_t ret = sm16825e_init(&sm16825e_conf, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
    ESP_LOGI("TEST", "✔ SM16825E init correctly rejected (ESP_ERR_INVALID_STATE) - SPI2_HOST conflict detected");

    ESP_LOGI("TEST", "Step 3: Verifying WS2812 still functions correctly");
    TEST_ASSERT_EQUAL(ESP_OK, ws2812_set_rgb_channel(255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(ESP_OK, ws2812_set_rgb_channel(0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(ESP_OK, ws2812_set_rgb_channel(0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_LOGI("TEST", "✔ WS2812 still functioning normally");

    ESP_LOGI("TEST", "Step 4: Deinitializing WS2812");
    TEST_ASSERT_EQUAL(ESP_OK, ws2812_deinit());
    ESP_LOGI("TEST", "✔ WS2812 deinitialized successfully, SPI2_HOST released");

    ESP_LOGI("TEST", "Step 5: Re-attempting to initialize SM16825E (should succeed now)");
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_init(&sm16825e_conf, NULL, NULL));
    ESP_LOGI("TEST", "✔ SM16825E initialized successfully after WS2812 was deinitialized");

    ESP_LOGI("TEST", "Step 6: Verifying SM16825E functions correctly");
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_R, SM16825E_PIN_OUTR));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_G, SM16825E_PIN_OUTG));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_B, SM16825E_PIN_OUTB));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_W, SM16825E_PIN_OUTW));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_regist_channel(SM16825E_CHANNEL_Y, SM16825E_PIN_OUTY));

    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(65535, 0, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(0, 65535, 0, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_rgbwy_channel(0, 0, 65535, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_set_shutdown());
    ESP_LOGI("TEST", "✔ SM16825E functioning normally");

    ESP_LOGI("TEST", "Step 7: Attempting to initialize WS2812 again (should fail)");
    ret = ws2812_init(&ws2812_conf, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
    ESP_LOGI("TEST", "✔ WS2812 init correctly rejected while SM16825E is active");

    ESP_LOGI("TEST", "Step 8: Final cleanup");
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, sm16825e_deinit());
    spi_bus_free(SPI2_HOST);
    ESP_LOGI("TEST", "✔ SM16825E deinitialized successfully");

    ESP_LOGI("TEST", "✅ SPI multi-instance prevention test completed successfully!");
}

TEST_CASE("NVS storage missing nvs_key error", "[Application Layer]")
{
    ESP_LOGI("TEST", "Testing that enable_status_storage=true without nvs_key returns an error");

#ifdef CONFIG_ENABLE_PWM_DRIVER
    lightbulb_config_t config = {
        .driver_conf.pwm.freq_hz = 4000,
        .capability.enable_fade = false,
        .capability.enable_status_storage = true,   // storage enabled
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
        .nvs_key = NULL,                            // intentionally not set
#if CONFIG_IDF_TARGET_ESP32
        .io_conf.pwm_io = {
            .red = 25,
            .green = 26,
            .blue = 27,
        },
#else
        .io_conf.pwm_io = {
            .red = 10,
            .green = 6,
            .blue = 7,
        },
#endif
    };

    lightbulb_handle_t handle = lightbulb_new_pwm_device(&config);
    TEST_ASSERT_NULL(handle);
    ESP_LOGI("TEST", "✔ lightbulb_new_pwm_device correctly returned NULL when nvs_key is missing");
#endif

    ESP_LOGI("TEST", "✅ Missing nvs_key error test completed successfully!");
}

TEST_CASE("Multi-instance NVS storage", "[Application Layer]")
{
    ESP_LOGI("TEST", "Testing multi-instance NVS storage functionality 🌟");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        TEST_ASSERT_EQUAL(ESP_OK, nvs_flash_erase());
        ret = nvs_flash_init();
    }
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ESP_LOGI("TEST", "Phase 1: Creating two lightbulb instances with separate NVS storage");

#ifdef CONFIG_ENABLE_PWM_DRIVER
    lightbulb_config_t config1 = {
        .driver_conf.pwm.freq_hz = 4000,
        .capability.enable_fade = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 120,
        .init_status.saturation = 80,
        .init_status.value = 60,
        .init_status.cct_percentage = 50,
        .init_status.brightness = 70,
        .nvs_key = "status_1",
#if CONFIG_IDF_TARGET_ESP32
        .io_conf.pwm_io = {
            .red = 25,
            .green = 26,
            .blue = 27,
            .cold_cct = 14,
            .warm_brightness = 12,
        },
#else
        .io_conf.pwm_io = {
            .red = 10,
            .green = 6,
            .blue = 7,
            .cold_cct = 3,
            .warm_brightness = 4,
        },
#endif
    };

    lightbulb_handle_t bulb1 = lightbulb_new_pwm_device(&config1);
    TEST_ASSERT_NOT_NULL(bulb1);
    ESP_LOGI("TEST", "Instance 1 (PWM) created with key='status_1'");
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
    lightbulb_config_t config2 = {
        .driver_conf.ws2812.led_num = 1,
        .driver_conf.ws2812.ctrl_io = 2,
        .capability.enable_fade = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 240,
        .init_status.saturation = 100,
        .init_status.value = 80,
        .nvs_key = "status_2",
    };

    lightbulb_handle_t bulb2 = lightbulb_new_ws2812_device(&config2);
    TEST_ASSERT_NOT_NULL(bulb2);
    ESP_LOGI("TEST", "Instance 2 (WS2812) created with key='status_2'");
#endif

    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI("TEST", "Phase 2: Setting different states and saving to NVS");

#ifdef CONFIG_ENABLE_PWM_DRIVER
    lightbulb_status_t status1 = {
        .mode = WORK_WHITE,
        .on = true,
        .hue = 0,
        .saturation = 0,
        .value = 0,
        .cct_percentage = 75,
        .brightness = 85,
    };
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_status_set_to_nvs(bulb1, &status1));
    ESP_LOGI("TEST", "Instance 1 status saved: brightness=%d, cct=%d%%", status1.brightness, status1.cct_percentage);
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
    lightbulb_status_t status2 = {
        .mode = WORK_COLOR,
        .on = true,
        .hue = 300,
        .saturation = 90,
        .value = 95,
        .cct_percentage = 0,
        .brightness = 0,
    };
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_status_set_to_nvs(bulb2, &status2));
    ESP_LOGI("TEST", "Instance 2 status saved: hue=%d, saturation=%d, value=%d", status2.hue, status2.saturation, status2.value);
#endif

    ESP_LOGI("TEST", "Phase 3: Cleaning up instances 🧹");

#ifdef CONFIG_ENABLE_PWM_DRIVER
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(bulb1));
    ESP_LOGI("TEST", "Instance 1 deinitialized");
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(bulb2));
    ESP_LOGI("TEST", "Instance 2 deinitialized");
#endif

    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI("TEST", "Phase 4: Recreating instances and reading from NVS");

#ifdef CONFIG_ENABLE_PWM_DRIVER
    bulb1 = lightbulb_new_pwm_device(&config1);
    TEST_ASSERT_NOT_NULL(bulb1);

    lightbulb_status_t read_status1;
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_status_get_from_nvs(bulb1, &read_status1));

    TEST_ASSERT_EQUAL(status1.mode, read_status1.mode);
    TEST_ASSERT_EQUAL(status1.on, read_status1.on);
    TEST_ASSERT_EQUAL(status1.brightness, read_status1.brightness);
    TEST_ASSERT_EQUAL(status1.cct_percentage, read_status1.cct_percentage);
    ESP_LOGI("TEST", "Instance 1 status verified: brightness=%d, cct=%d%% ✔",
             read_status1.brightness, read_status1.cct_percentage);
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
    bulb2 = lightbulb_new_ws2812_device(&config2);
    TEST_ASSERT_NOT_NULL(bulb2);

    lightbulb_status_t read_status2;
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_status_get_from_nvs(bulb2, &read_status2));

    TEST_ASSERT_EQUAL(status2.mode, read_status2.mode);
    TEST_ASSERT_EQUAL(status2.on, read_status2.on);
    TEST_ASSERT_EQUAL(status2.hue, read_status2.hue);
    TEST_ASSERT_EQUAL(status2.saturation, read_status2.saturation);
    TEST_ASSERT_EQUAL(status2.value, read_status2.value);
    ESP_LOGI("TEST", "Instance 2 status verified: hue=%d, saturation=%d, value=%d ✔",
             read_status2.hue, read_status2.saturation, read_status2.value);
#endif

    ESP_LOGI("TEST", "Phase 5: Testing NVS erase functionality");

#ifdef CONFIG_ENABLE_PWM_DRIVER
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_status_erase_nvs_storage(bulb1));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, lightbulb_status_get_from_nvs(bulb1, &read_status1));
    ESP_LOGI("TEST", "Instance 1 NVS storage erased successfully ✔");
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_status_erase_nvs_storage(bulb2));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, lightbulb_status_get_from_nvs(bulb2, &read_status2));
    ESP_LOGI("TEST", "Instance 2 NVS storage erased successfully ✔");
#endif

    ESP_LOGI("TEST", "Phase 6: Final cleanup");

#ifdef CONFIG_ENABLE_PWM_DRIVER
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(bulb1));
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(bulb2));
#endif

    nvs_flash_deinit();
    ESP_LOGI("TEST", "✔ Multi-instance NVS storage test completed successfully!");
}

TEST_CASE("Multi-instance Fade and Effect Interaction", "[Application Layer]")
{
    ESP_LOGI("TEST", "=== Testing fade timer behavior with multi-instance simultaneous effects ===");

    lightbulb_handle_t bulb1 = NULL;
    lightbulb_handle_t bulb2 = NULL;

#ifdef CONFIG_ENABLE_PWM_DRIVER
    ESP_LOGI("TEST", "Phase 1: Initializing first instance (PWM driver) with fade enabled");
    lightbulb_config_t config1 = {
        .driver_conf.pwm.freq_hz = 4000,
        .capability.enable_fade = true,
        .capability.fade_time_ms = 1000,  // 1 second fade time
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,

#if CONFIG_IDF_TARGET_ESP32
        .io_conf.pwm_io.red = 25,
        .io_conf.pwm_io.green = 26,
        .io_conf.pwm_io.blue = 27,
#else
        .io_conf.pwm_io.red = 10,
        .io_conf.pwm_io.green = 6,
        .io_conf.pwm_io.blue = 7,
#endif

        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };

    bulb1 = lightbulb_new_pwm_device(&config1);
    TEST_ASSERT_NOT_NULL(bulb1);
    ESP_LOGI("TEST", "✔ Instance 1 (PWM) initialized with fade enabled");
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
    ESP_LOGI("TEST", "Phase 2: Initializing second instance (WS2812 driver) with effects");
    lightbulb_config_t config2 = {
        .driver_conf.ws2812.led_num = 22,
        .driver_conf.ws2812.ctrl_io = 4,
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };

    bulb2 = lightbulb_new_ws2812_device(&config2);
    TEST_ASSERT_NOT_NULL(bulb2);
    ESP_LOGI("TEST", "✔ Instance 2 (WS2812) initialized");
#endif

    vTaskDelay(pdMS_TO_TICKS(500));

#ifdef CONFIG_ENABLE_PWM_DRIVER
    ESP_LOGI("TEST", "Phase 3: Testing fade with frequent set API calls on Instance 1");
    ESP_LOGI("TEST", "Starting continuous color transitions with fade...");

    for (int i = 0; i < 5; i++) {
        uint16_t hue = (i * 72) % 360;  // Cycle through colors: 0, 72, 144, 216, 288
        ESP_LOGI("TEST", "Setting Instance 1 to hue=%d (fade timer should restart)", hue);
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hue(bulb1, hue));

        vTaskDelay(pdMS_TO_TICKS(300));

        ESP_LOGI("TEST", "Interrupting fade with saturation change");
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_saturation(bulb1, 80));
        vTaskDelay(pdMS_TO_TICKS(300));

        ESP_LOGI("TEST", "Interrupting fade again with value change");
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_value(bulb1, 90));
        vTaskDelay(pdMS_TO_TICKS(400));
    }
    ESP_LOGI("TEST", "✔ Instance 1 fade interruption test completed");
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
    ESP_LOGI("TEST", "Phase 4: Starting lighting effect on Instance 2 while Instance 1 continues");

    lightbulb_effect_config_t effect_config = {
        .effect_type = EFFECT_BREATH,
        .mode = WORK_COLOR,
        .hue = 240,  // Blue
        .saturation = 100,
        .cct = 0,
        .min_value_brightness = 10,
        .max_value_brightness = 100,
        .effect_cycle_ms = 2000,  // 2 second breath cycle
        .total_ms = 8000,  // Run for 8 seconds
        .user_cb = NULL,
        .interrupt_forbidden = false,
    };

    ESP_LOGI("TEST", "Starting breath effect on Instance 2 (should run independently)");
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_basic_effect_start(bulb2, &effect_config));
    ESP_LOGI("TEST", "✔ Breath effect started on Instance 2");
#endif

#if defined(CONFIG_ENABLE_PWM_DRIVER) && defined(CONFIG_ENABLE_WS2812_DRIVER)
    ESP_LOGI("TEST", "Phase 5: Testing simultaneous operations on both instances");

    for (int i = 0; i < 4; i++) {
        uint16_t hue = (i * 90) % 360;
        ESP_LOGI("TEST", "Cycle %d: Setting Instance 1 to hue=%d while effect runs on Instance 2", i + 1, hue);
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hue(bulb1, hue));
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP_LOGI("TEST", "Cycle %d: Setting Instance 1 value to %d", i + 1, 100 - (i * 20));
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_value(bulb1, 100 - (i * 20)));
        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    ESP_LOGI("TEST", "✔ Both instances operated simultaneously without interference");
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
    ESP_LOGI("TEST", "Phase 6: Stopping effect on Instance 2");
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_basic_effect_stop(bulb2));
    ESP_LOGI("TEST", "✔ Effect stopped on Instance 2");
    vTaskDelay(pdMS_TO_TICKS(500));
#endif

#if defined(CONFIG_ENABLE_PWM_DRIVER) && defined(CONFIG_ENABLE_WS2812_DRIVER)
    ESP_LOGI("TEST", "Phase 7: Testing rapid set API calls on both instances");

    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hue(bulb1, i * 36));
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hue(bulb2, (360 - i * 36)));
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    ESP_LOGI("TEST", "✔ Rapid updates on both instances completed");
#endif

#ifdef CONFIG_ENABLE_PWM_DRIVER
    ESP_LOGI("TEST", "Phase 8: Testing fade disable/enable cycle on Instance 1");

    // Disable fade
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_fades_function(bulb1, false));
    ESP_LOGI("TEST", "Fade disabled - color changes should be instant");

    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(bulb1, 0, 100, 100));  // Red
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(bulb1, 120, 100, 100));  // Green
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(bulb1, 240, 100, 100));  // Blue
    vTaskDelay(pdMS_TO_TICKS(200));

    // Re-enable fade
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_fades_function(bulb1, true));
    ESP_LOGI("TEST", "Fade re-enabled - color changes should be smooth again");

    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(bulb1, 0, 100, 100));  // Red
    vTaskDelay(pdMS_TO_TICKS(500));
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_set_hsv(bulb1, 120, 100, 100));  // Green
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI("TEST", "✔ Fade enable/disable cycle completed");
#endif

#if defined(CONFIG_ENABLE_PWM_DRIVER) && defined(CONFIG_ENABLE_WS2812_DRIVER)
    ESP_LOGI("TEST", "Phase 9: Testing concurrent effects on both instances");

    // Start effect on Instance 1
    lightbulb_effect_config_t effect1 = {
        .effect_type = EFFECT_BLINK,
        .mode = WORK_COLOR,
        .hue = 0,  // Red
        .saturation = 100,
        .cct = 0,
        .min_value_brightness = 0,
        .max_value_brightness = 100,
        .effect_cycle_ms = 1000,
        .total_ms = 5000,
        .user_cb = NULL,
        .interrupt_forbidden = false,
    };

    // Start effect on Instance 2
    lightbulb_effect_config_t effect2 = {
        .effect_type = EFFECT_BREATH,
        .mode = WORK_COLOR,
        .hue = 300,  // Purple
        .saturation = 100,
        .cct = 0,
        .min_value_brightness = 10,
        .max_value_brightness = 100,
        .effect_cycle_ms = 1500,
        .total_ms = 5000,
        .user_cb = NULL,
        .interrupt_forbidden = false,
    };

    ESP_LOGI("TEST", "Starting blink effect on Instance 1 and breath effect on Instance 2");
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_basic_effect_start(bulb1, &effect1));
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_basic_effect_start(bulb2, &effect2));

    ESP_LOGI("TEST", "Both effects running... waiting for completion");
    vTaskDelay(pdMS_TO_TICKS(5500));  // Wait for effects to complete

    ESP_LOGI("TEST", "✔ Concurrent effects completed successfully");
#endif

    ESP_LOGI("TEST", "Phase 10: Cleanup");

#ifdef CONFIG_ENABLE_PWM_DRIVER
    if (bulb1) {
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(bulb1));
        ESP_LOGI("TEST", "✔ Instance 1 cleaned up");
    }
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
    if (bulb2) {
        TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit(bulb2));
        ESP_LOGI("TEST", "✔ Instance 2 cleaned up");
    }
#endif

    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI("TEST", "✅ Multi-instance fade and effect interaction test completed successfully!");
}

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("LIGHTBULB DRIVER TEST \n");
    unity_run_menu();
}
