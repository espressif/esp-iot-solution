/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "lightbulb.h"

/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <unity.h>
#include <nvs_flash.h>
#include <esp_log.h>

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

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

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
    TEST_ASSERT_EQUAL(ESP_OK, pwm_init(&conf, NULL));

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
        .type = DRIVER_ESP_PWM,
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
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_init(&config));
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(LIGHTING_BASIC_FIVE, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit());
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
    TEST_ASSERT_EQUAL(ESP_OK, sm2135eh_init(&conf, NULL));

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
        .type = DRIVER_SM2135EH,
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
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_init(&config));
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(LIGHTING_BASIC_FIVE, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit());
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
    TEST_ASSERT_EQUAL(ESP_OK, bp57x8d_init(&conf, NULL));

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
        .type = DRIVER_BP57x8D,
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
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_init(&config));
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(LIGHTING_BASIC_FIVE, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit());
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
    TEST_ASSERT_EQUAL(ESP_OK, bp1658cj_init(&conf, NULL));

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
        .type = DRIVER_BP1658CJ,
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
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_init(&config));
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(LIGHTING_BASIC_FIVE, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit());
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
    TEST_ASSERT_EQUAL(ESP_OK, sm2x35egh_init(&conf, NULL));

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
        .type = DRIVER_SM2x35EGH,
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
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_init(&config));
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(LIGHTING_BASIC_FIVE, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit());
}
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
TEST_CASE("WS2812", "[Underlying Driver]")
{
    driver_ws2812_t ws2812 = {
        .led_num = 1,
        .ctrl_io = 4,
    };
    TEST_ASSERT_EQUAL(ESP_OK, ws2812_init(&ws2812, NULL));
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
        .type = DRIVER_WS2812,
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
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_init(&config));
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(LIGHTING_RAINBOW, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit());
}
#endif

#ifdef CONFIG_ENABLE_KP18058_DRIVER
TEST_CASE("KP18058", "[Underlying Driver]")
{
    driver_kp18058_t kp18058 = {
        .rgb_current_multiple = 3,
        .cw_current_multiple = 10,
        .iic_clk = 4,
        .iic_sda = 3,
        .iic_freq_khz = 300,
        .enable_iic_queue = true,
    };
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_init(&kp18058, NULL));

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

    //7. Deinit
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_set_shutdown());
    // Wait for data transmission to complete
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, kp18058_deinit());
}

TEST_CASE("KP18058", "[Application Layer]")
{
    lightbulb_config_t config = {
        .type = DRIVER_KP18058,
        .driver_conf.kp18058.rgb_current_multiple = 3,
        .driver_conf.kp18058.cw_current_multiple = 10,
        .driver_conf.kp18058.iic_clk = 4,
        .driver_conf.kp18058.iic_sda = 3,
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
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_init(&config));
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(LIGHTING_BASIC_FIVE, 1000);
    vTaskDelay(pdMS_TO_TICKS(2000));
    TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit());
}
#endif

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
