/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
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

static const char *TAG = "test case";

#ifndef MAX
#define MAX(a, b)                                   (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b)                                   (((a) < (b)) ? (a) : (b))
#endif

static void cct_and_brightness_convert_to_cold_and_warm(float white_max_power, uint8_t cct, uint8_t brightness, uint16_t *out_cold, uint16_t *out_warm)
{
    // 1. Calculate the percentage of cold and warm
    float warm_tmp = (100 - cct) / 100.0;
    float cold_tmp = cct / 100.0;
    ESP_LOGD(TAG, "warm_tmp:%f cold_tmp:%f", warm_tmp, cold_tmp);

    // 2. According to the maximum power assign percentage
    float fin_warm_tmp = warm_tmp * 1.0;
    float fin_cold_tmp = cold_tmp * 1.0;
    float x = white_max_power / 100.0;
    float max_power = x * (255.0);
    ESP_LOGD(TAG, "Line:%d x:%f, max_power:%f", __LINE__, x, max_power);
    for (int i = 0; i <= max_power; i++) {
        if (fin_warm_tmp >= 255 || fin_cold_tmp >= 255) {
            break;
        }
        fin_warm_tmp = warm_tmp * i;
        fin_cold_tmp = cold_tmp * i;
    }
    ESP_LOGD(TAG, "fin_warm_tmp:%f fin_cold_tmp:%f", fin_warm_tmp, fin_cold_tmp);

    // 3. Reassign based on brightness
    *out_warm = fin_warm_tmp * (brightness / 100.0);
    *out_cold = fin_cold_tmp * (brightness / 100.0);

    ESP_LOGD(TAG, "software cct: [input: %d %d], [output:%d %d]", cct, brightness, *out_cold, *out_warm);
}

static void process_color_power_limit(float color_max_power, uint8_t r, uint8_t g, uint8_t b, uint16_t *out_r, uint16_t *out_g, uint16_t *out_b)
{
    if (r == 0 && g == 0 && b == 0) {
        *out_r = 0;
        *out_g = 0;
        *out_b = 0;
        return;
    }

    uint16_t total = 255;
    uint16_t gamma_r = r;
    uint16_t gamma_g = g;
    uint16_t gamma_b = b;

    // 1. First we need to find the mapped value in the gamma table
    // hal_get_driver_feature(QUERY_COLOR_BIT_DEPTH, &total);
    // hal_get_gamma_value(r, g, b, &gamma_r, &gamma_g, &gamma_b);

    // 2. Second, we need to calculate the color distribution ratio of the rgb channel, and their ratio determines the final rendered color.
    float rgb_ratio_r = gamma_r * 1.0 / (total);
    float rgb_ratio_g = gamma_g * 1.0 / (total);
    float rgb_ratio_b = gamma_b * 1.0 / (total);
    ESP_LOGD(TAG, "rgb_ratio_r:%f rgb_ratio_g:%f rgb_ratio_b:%f total:%d", rgb_ratio_r, rgb_ratio_g, rgb_ratio_b, total);

    // 3. Next, we need to calculate the grayscale ratio, which provides a baseline value for subsequent power limiting.
    float grayscale_ratio_r = gamma_r / ((gamma_r + gamma_g + gamma_b) * 1.0);
    float grayscale_ratio_g = gamma_g / ((gamma_r + gamma_g + gamma_b) * 1.0);
    float grayscale_ratio_b = gamma_b / ((gamma_r + gamma_g + gamma_b) * 1.0);
    float baseline = MAX(grayscale_ratio_r, grayscale_ratio_g);
    baseline = MAX(baseline, grayscale_ratio_b);
    ESP_LOGD(TAG, "grayscale_ratio_r:%f grayscale_ratio_g:%f grayscale_ratio_b:%f baseline:%f", grayscale_ratio_r, grayscale_ratio_g, grayscale_ratio_b, baseline);

    // 4. Finally, we recalculate the final value based on the maximum allowed output power.
    float x = color_max_power / 100.0;
    float max_power = x * (total);
    float max_power_baseline = MIN(total, baseline * max_power);
    ESP_LOGD(TAG, "x:%f, max_power:%f max_power_baseline:%f", x, max_power, max_power_baseline);
    *out_r = max_power_baseline * rgb_ratio_r;
    *out_g = max_power_baseline * rgb_ratio_g;
    *out_b = max_power_baseline * rgb_ratio_b;

    ESP_LOGD(TAG, "setp_2 [input: %d %d %d], [output:%d %d %d]", r, g, b, *out_r, *out_g, *out_b);
}

TEST_CASE("white power limit", "[Application Layer]")
{
    uint8_t index = 0;
    uint8_t cct = 100;
    uint8_t brightness = 100;
    uint16_t out_cold = 0;
    uint16_t out_warm = 0;

    for (int i = 0; i <= 100; i += 10) {
        cct = i;
        brightness = 100;
        out_cold = 0;
        out_warm = 0;
        cct_and_brightness_convert_to_cold_and_warm(100, cct, brightness, &out_cold, &out_warm);
        ESP_LOGI(TAG, "%d. [cct: %d brightness:%d] ->  [out_cold:%d out_warm:%d]", ++index, cct, brightness, out_cold, out_warm);
    }
    printf("\r\n");
    for (int i = 0; i <= 100; i += 10) {
        cct = 0;
        brightness = i;
        out_cold = 0;
        out_warm = 0;
        cct_and_brightness_convert_to_cold_and_warm(100, cct, brightness, &out_cold, &out_warm);
        ESP_LOGI(TAG, "%d. [cct: %d brightness:%d] ->  [out_cold:%d out_warm:%d]", ++index, cct, brightness, out_cold, out_warm);
    }
    printf("\r\n");
    for (int i = 0; i <= 100; i += 10) {
        cct = 50;
        brightness = i;
        out_cold = 0;
        out_warm = 0;
        cct_and_brightness_convert_to_cold_and_warm(100, cct, brightness, &out_cold, &out_warm);
        ESP_LOGI(TAG, "%d. [cct: %d brightness:%d] ->  [out_cold:%d out_warm:%d]", ++index, cct, brightness, out_cold, out_warm);
    }
    printf("\r\n");
    for (int i = 0; i <= 100; i += 10) {
        cct = 100;
        brightness = i;
        out_cold = 0;
        out_warm = 0;
        cct_and_brightness_convert_to_cold_and_warm(100, cct, brightness, &out_cold, &out_warm);
        ESP_LOGI(TAG, "%d. [cct: %d brightness:%d] ->  [out_cold:%d out_warm:%d]", ++index, cct, brightness, out_cold, out_warm);
    }
    printf("\r\n");
    for (int i = 0; i <= 100; i += 10) {
        cct = i;
        brightness = 100;
        out_cold = 0;
        out_warm = 0;
        cct_and_brightness_convert_to_cold_and_warm(200, cct, brightness, &out_cold, &out_warm);
        ESP_LOGI(TAG, "%d. [cct: %d brightness:%d] ->  [out_cold:%d out_warm:%d]", ++index, cct, brightness, out_cold, out_warm);
    }
    printf("\r\n");
    for (int i = 0; i <= 100; i += 10) {
        cct = 0;
        brightness = i;
        out_cold = 0;
        out_warm = 0;
        cct_and_brightness_convert_to_cold_and_warm(200, cct, brightness, &out_cold, &out_warm);
        ESP_LOGI(TAG, "%d. [cct: %d brightness:%d] ->  [out_cold:%d out_warm:%d]", ++index, cct, brightness, out_cold, out_warm);
    }
    printf("\r\n");
    for (int i = 0; i <= 100; i += 10) {
        cct = 50;
        brightness = i;
        out_cold = 0;
        out_warm = 0;
        cct_and_brightness_convert_to_cold_and_warm(200, cct, brightness, &out_cold, &out_warm);
        ESP_LOGI(TAG, "%d. [cct: %d brightness:%d] ->  [out_cold:%d out_warm:%d]", ++index, cct, brightness, out_cold, out_warm);
    }
    printf("\r\n");
    for (int i = 0; i <= 100; i += 10) {
        cct = 100;
        brightness = i;
        out_cold = 0;
        out_warm = 0;
        cct_and_brightness_convert_to_cold_and_warm(200, cct, brightness, &out_cold, &out_warm);
        ESP_LOGI(TAG, "%d. [cct: %d brightness:%d] ->  [out_cold:%d out_warm:%d]", ++index, cct, brightness, out_cold, out_warm);
    }
}

TEST_CASE("color power limit", "[Application Layer]")
{
    uint8_t index = 0;
    uint16_t out_r = 0;
    uint16_t out_g = 0;
    uint16_t out_b = 0;

    for (int i = 100; i <= 300; i += 100) {
        uint8_t r = 255;
        uint8_t g = 255;
        uint8_t b = 0;
        process_color_power_limit(i, r, g, b, &out_r, &out_g, &out_b);
        ESP_LOGI(TAG, "%d. [r:%d g:%d b:%d] ->  [out_r:%d out_g:%d out_b:%d] ", ++index, r, g, b, out_r, out_g, out_b);

        r = 127;
        g = 127;
        b = 0;
        process_color_power_limit(i, r, g, b, &out_r, &out_g, &out_b);
        ESP_LOGI(TAG, "%d. [r:%d g:%d b:%d] ->  [out_r:%d out_g:%d out_b:%d] ", ++index, r, g, b, out_r, out_g, out_b);

        r = 63;
        g = 63;
        b = 0;
        process_color_power_limit(i, r, g, b, &out_r, &out_g, &out_b);
        ESP_LOGI(TAG, "%d. [r:%d g:%d b:%d] ->  [out_r:%d out_g:%d out_b:%d] ", ++index, r, g, b, out_r, out_g, out_b);

        r = 255;
        g = 255;
        b = 255;
        process_color_power_limit(i, r, g, b, &out_r, &out_g, &out_b);
        ESP_LOGI(TAG, "%d. [r:%d g:%d b:%d] ->  [out_r:%d out_g:%d out_b:%d] ", ++index, r, g, b, out_r, out_g, out_b);

        r = 127;
        g = 127;
        b = 127;
        process_color_power_limit(i, r, g, b, &out_r, &out_g, &out_b);
        ESP_LOGI(TAG, "%d. [r:%d g:%d b:%d] ->  [out_r:%d out_g:%d out_b:%d] ", ++index, r, g, b, out_r, out_g, out_b);

        r = 63;
        g = 63;
        b = 63;
        process_color_power_limit(i, r, g, b, &out_r, &out_g, &out_b);
        ESP_LOGI(TAG, "%d. [r:%d g:%d b:%d] ->  [out_r:%d out_g:%d out_b:%d] ", ++index, r, g, b, out_r, out_g, out_b);
        printf("\r\n");
    }
}

TEST_CASE("Storage interface", "[Application Layer]")
{
    nvs_flash_init();

    //structure and set data
    lightbulb_status_t status = {
        .cct_percentage = 100,
        .brightness = 100,
        .mode = WORK_WHITE,
    };
    TEST_ESP_OK(lightbulb_status_set_to_nvs(&status));

    //load and check
    lightbulb_status_t status_case1;
    TEST_ESP_OK(lightbulb_status_get_from_nvs(&status_case1));
    TEST_ESP_OK(lightbulb_status_erase_nvs_storage());

    nvs_flash_deinit();
}

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
    TEST_ESP_OK(pwm_init(&conf));

    //3. regist Check, step 1
#if CONFIG_IDF_TARGET_ESP32
    TEST_ESP_OK(pwm_regist_channel(PWM_CHANNEL_R, 25));
    TEST_ESP_OK(pwm_regist_channel(PWM_CHANNEL_G, 26));
    TEST_ESP_OK(pwm_regist_channel(PWM_CHANNEL_B, 27));
#elif CONFIG_IDF_TARGET_ESP32C3
    TEST_ESP_OK(pwm_regist_channel(PWM_CHANNEL_R, 10));
    TEST_ESP_OK(pwm_regist_channel(PWM_CHANNEL_G, 6));
    TEST_ESP_OK(pwm_regist_channel(PWM_CHANNEL_B, 7));
#else
#error "Unsupported chip type"
#endif
    TEST_ESP_OK(pwm_set_channel(PWM_CHANNEL_R, 4096));
    TEST_ESP_OK(pwm_set_channel(PWM_CHANNEL_G, 4096));
    TEST_ESP_OK(pwm_set_channel(PWM_CHANNEL_B, 4096));
    TEST_ESP_OK(pwm_set_rgb_channel(4096, 4096, 4096));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_channel(PWM_CHANNEL_CCT_COLD, 4096));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_channel(PWM_CHANNEL_BRIGHTNESS_WARM, 4096));

    //3. regist Check, step 2
#if CONFIG_IDF_TARGET_ESP32
    TEST_ESP_OK(pwm_regist_channel(PWM_CHANNEL_CCT_COLD, 14));
    TEST_ESP_OK(pwm_regist_channel(PWM_CHANNEL_BRIGHTNESS_WARM, 12));
#elif CONFIG_IDF_TARGET_ESP32C3
    TEST_ESP_OK(pwm_regist_channel(PWM_CHANNEL_CCT_COLD, 3));
    TEST_ESP_OK(pwm_regist_channel(PWM_CHANNEL_BRIGHTNESS_WARM, 4));
#else
#error "Unsupported chip type"
#endif
    TEST_ESP_OK(pwm_set_channel(PWM_CHANNEL_CCT_COLD, 4096));
    TEST_ESP_OK(pwm_set_channel(PWM_CHANNEL_BRIGHTNESS_WARM, 4096));
    TEST_ESP_OK(pwm_set_cctb_or_cw_channel(4096, 4096));
    TEST_ESP_OK(pwm_set_rgbcctb_or_rgbcw_channel(4096, 4096, 4096, 4096, 4096));

    //4. Data range check
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pwm_set_channel(PWM_CHANNEL_R, 4097));

    //5. Color check
    TEST_ESP_OK(pwm_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(pwm_set_rgbcctb_or_rgbcw_channel(4096, 0, 0, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(pwm_set_rgbcctb_or_rgbcw_channel(0, 4096, 0, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(pwm_set_rgbcctb_or_rgbcw_channel(0, 0, 4096, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(pwm_set_rgbcctb_or_rgbcw_channel(0, 0, 0, 4096, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(pwm_set_rgbcctb_or_rgbcw_channel(0, 0, 0, 0, 4096));

    //6. deinit
    TEST_ESP_OK(pwm_set_shutdown());
    TEST_ESP_OK(pwm_deinit());
}

TEST_CASE("PWM", "[Application Layer]")
{
    lightbulb_config_t config = {
        .type = DRIVER_ESP_PWM,
        .driver_conf.pwm.freq_hz = 4000,
        .capability.enable_fades = true,
        .capability.fades_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_mix_cct = false,
        .capability.enable_status_storage = false,
        .capability.mode_mask = COLOR_MODE,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.pwm_io.red = 25,
        .io_conf.pwm_io.green = 26,
        .io_conf.pwm_io.blue = 27,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    TEST_ESP_OK(lightbulb_init(&config));
    vTaskDelay(1000);
    lightbulb_lighting_output_test(LIGHTING_ALL_UNIT, 1000);
    TEST_ESP_OK(lightbulb_deinit());
}
#endif

#ifdef CONFIG_ENABLE_SM2135E_DRIVER
TEST_CASE("SM2135E", "[Underlying Driver]")
{
    //1.Status check
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2135e_set_channel(SM2135E_CHANNEL_R, 255));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2135e_set_rgb_channel(255, 255, 0));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2135e_set_wy_channel(255, 255));

    //2. init check
    driver_sm2135e_t conf = {
        .rgb_current = SM2135E_RGB_CURRENT_20MA,
        .wy_current = SM2135E_WY_CURRENT_40MA,
        .iic_clk = 4,
        .iic_sda = 5,
        .freq_khz = 400,
        .enable_iic_queue = true
    };
    TEST_ESP_OK(sm2135e_init(&conf));

    //3. regist Check, step 1
    TEST_ESP_OK(sm2135e_regist_channel(SM2135E_CHANNEL_R, SM2135E_PIN_OUT3));
    TEST_ESP_OK(sm2135e_regist_channel(SM2135E_CHANNEL_G, SM2135E_PIN_OUT2));
    TEST_ESP_OK(sm2135e_regist_channel(SM2135E_CHANNEL_B, SM2135E_PIN_OUT1));
    TEST_ESP_OK(sm2135e_set_channel(SM2135E_CHANNEL_R, 1));
    TEST_ESP_OK(sm2135e_set_channel(SM2135E_CHANNEL_G, 1));
    TEST_ESP_OK(sm2135e_set_channel(SM2135E_CHANNEL_B, 1));
    TEST_ESP_OK(sm2135e_set_rgb_channel(1, 1, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2135e_set_channel(SM2135E_CHANNEL_W, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2135e_set_channel(SM2135E_CHANNEL_Y, 1));

    //3. regist Check, step 2
    TEST_ESP_OK(sm2135e_regist_channel(SM2135E_CHANNEL_W, SM2135E_PIN_OUT5));
    TEST_ESP_OK(sm2135e_regist_channel(SM2135E_CHANNEL_Y, SM2135E_PIN_OUT4));
    TEST_ESP_OK(sm2135e_set_channel(SM2135E_CHANNEL_W, 1));
    TEST_ESP_OK(sm2135e_set_channel(SM2135E_CHANNEL_Y, 1));
    TEST_ESP_OK(sm2135e_set_wy_channel(1, 1));

    //4. Data range check
    // Nothing

    //5. Color check
    TEST_ESP_OK(sm2135e_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135e_set_rgb_channel(255, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135e_set_rgb_channel(0, 255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135e_set_rgb_channel(0, 0, 255));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135e_set_wy_channel(255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135e_set_wy_channel(0, 255));
    vTaskDelay(1000);

    //6. deinit
    TEST_ESP_OK(sm2135e_set_shutdown());
    TEST_ESP_OK(sm2135e_deinit());
}

TEST_CASE("SM2135E", "[Application Layer]")
{
    lightbulb_config_t config = {
        .type = DRIVER_SM2135E,
        .driver_conf.sm2135e.rgb_current = SM2135E_RGB_CURRENT_20MA,
        .driver_conf.sm2135e.wy_current = SM2135E_WY_CURRENT_40MA,
        .driver_conf.sm2135e.iic_clk = 4,
        .driver_conf.sm2135e.iic_sda = 5,
        .driver_conf.sm2135e.freq_khz = 400,
        .driver_conf.sm2135e.enable_iic_queue = true,
        .capability.enable_fades = true,
        .capability.fades_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_mix_cct = true,
        .capability.enable_status_storage = false,
        .capability.mode_mask = COLOR_AND_WHITE_MODE,
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
    TEST_ESP_OK(lightbulb_init(&config));
    vTaskDelay(1000);
    lightbulb_lighting_output_test(LIGHTING_ALL_UNIT, 2000);
    TEST_ESP_OK(lightbulb_deinit());
    // for (int i = 0; i <= 100; i += 5) {
    //     lightbulb_set_cctb(50, i);
    //     vTaskDelay(pdMS_TO_TICKS(2000));
    // }
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
        .iic_sda = 5,
        .freq_khz = 400,
        .enable_iic_queue = true
    };
    TEST_ESP_OK(sm2135eh_init(&conf));

    //3. regist Check, step 1
    TEST_ESP_OK(sm2135eh_regist_channel(SM2135EH_CHANNEL_R, SM2135EH_PIN_OUT3));
    TEST_ESP_OK(sm2135eh_regist_channel(SM2135EH_CHANNEL_G, SM2135EH_PIN_OUT2));
    TEST_ESP_OK(sm2135eh_regist_channel(SM2135EH_CHANNEL_B, SM2135EH_PIN_OUT1));
    TEST_ESP_OK(sm2135eh_set_channel(SM2135EH_CHANNEL_R, 1));
    TEST_ESP_OK(sm2135eh_set_channel(SM2135EH_CHANNEL_G, 1));
    TEST_ESP_OK(sm2135eh_set_channel(SM2135EH_CHANNEL_B, 1));
    TEST_ESP_OK(sm2135eh_set_rgb_channel(1, 1, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2135eh_set_channel(SM2135EH_CHANNEL_W, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2135eh_set_channel(SM2135EH_CHANNEL_Y, 1));

    //3. regist Check, step 2
    TEST_ESP_OK(sm2135eh_regist_channel(SM2135EH_CHANNEL_W, SM2135EH_PIN_OUT5));
    TEST_ESP_OK(sm2135eh_regist_channel(SM2135EH_CHANNEL_Y, SM2135EH_PIN_OUT4));
    TEST_ESP_OK(sm2135eh_set_channel(SM2135EH_CHANNEL_W, 1));
    TEST_ESP_OK(sm2135eh_set_channel(SM2135EH_CHANNEL_Y, 1));
    TEST_ESP_OK(sm2135eh_set_wy_channel(1, 1));

    //4. Data range check
    // Nothing

    //5. Color check
    TEST_ESP_OK(sm2135eh_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_set_rgb_channel(255, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_set_rgb_channel(0, 255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_set_rgb_channel(0, 0, 255));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_set_wy_channel(255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_set_wy_channel(0, 255));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_set_rgbwy_channel(255, 0, 0, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_set_rgbwy_channel(0, 255, 0, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_set_rgbwy_channel(0, 0, 255, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_set_rgbwy_channel(0, 0, 0, 255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_set_rgbwy_channel(0, 0, 0, 0, 255));
    vTaskDelay(1000);

    //6. deinit
    TEST_ESP_OK(sm2135eh_set_shutdown());
    // Wait for data transmission to complete
    vTaskDelay(1000);
    TEST_ESP_OK(sm2135eh_deinit());
}

TEST_CASE("SM2135EH", "[Application Layer]")
{
    lightbulb_config_t config = {
        .type = DRIVER_SM2135EH,
        .driver_conf.sm2135eh.rgb_current = SM2135EH_RGB_CURRENT_24MA,
        .driver_conf.sm2135eh.wy_current = SM2135EH_WY_CURRENT_40MA,
        .driver_conf.sm2135eh.iic_clk = 4,
        .driver_conf.sm2135eh.iic_sda = 5,
        .driver_conf.sm2135eh.freq_khz = 400,
        .driver_conf.sm2135eh.enable_iic_queue = true,
        .capability.enable_fades = true,
        .capability.fades_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_mix_cct = true,
        .capability.enable_status_storage = false,
        .capability.mode_mask = COLOR_AND_WHITE_MODE,
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
    TEST_ESP_OK(lightbulb_init(&config));
    vTaskDelay(1000);
    lightbulb_lighting_output_test(LIGHTING_ALEXA, 2000);
    TEST_ESP_OK(lightbulb_deinit());
}
#endif

#ifdef CONFIG_ENABLE_BP5758D_DRIVER
TEST_CASE("BP5758D", "[Underlying Driver]")
{
    //1.Status check
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp5758d_set_channel(BP5758D_CHANNEL_R, 1023));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp5758d_set_rgb_channel(1023, 1023, 0));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp5758d_set_cw_channel(1023, 1023));

    //2. init check
    driver_bp5758d_t conf = {
        .current = {10, 10, 10, 20, 20},
        .iic_clk = 4,
        .iic_sda = 5,
        .freq_khz = 300,
        .enable_iic_queue = true
    };
    TEST_ESP_OK(bp5758d_init(&conf));

    //3. regist Check, step 1
    TEST_ESP_OK(bp5758d_regist_channel(BP5758D_CHANNEL_R, BP5758D_PIN_OUT1));
    TEST_ESP_OK(bp5758d_regist_channel(BP5758D_CHANNEL_G, BP5758D_PIN_OUT2));
    TEST_ESP_OK(bp5758d_regist_channel(BP5758D_CHANNEL_B, BP5758D_PIN_OUT3));
    TEST_ESP_OK(bp5758d_set_channel(BP5758D_CHANNEL_R, 1));
    TEST_ESP_OK(bp5758d_set_channel(BP5758D_CHANNEL_G, 1));
    TEST_ESP_OK(bp5758d_set_channel(BP5758D_CHANNEL_B, 1));
    TEST_ESP_OK(bp5758d_set_rgb_channel(1, 1, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp5758d_set_channel(BP5758D_CHANNEL_C, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp5758d_set_channel(BP5758D_CHANNEL_W, 1));

    //3. regist Check, step 2
    TEST_ESP_OK(bp5758d_regist_channel(BP5758D_CHANNEL_C, BP5758D_PIN_OUT5));
    TEST_ESP_OK(bp5758d_regist_channel(BP5758D_CHANNEL_W, BP5758D_PIN_OUT4));
    TEST_ESP_OK(bp5758d_set_channel(BP5758D_CHANNEL_C, 1));
    TEST_ESP_OK(bp5758d_set_channel(BP5758D_CHANNEL_W, 1));
    TEST_ESP_OK(bp5758d_set_cw_channel(1, 1));

    //4. Data range check
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, bp5758d_set_channel(BP5758D_CHANNEL_R, 1024));

    //5. Color check
    TEST_ESP_OK(bp5758d_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_rgb_channel(255, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_rgb_channel(0, 255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_rgb_channel(0, 0, 255));
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_cw_channel(255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_cw_channel(0, 255));
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_rgbcw_channel(0, 0, 0, 255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_rgbcw_channel(0, 0, 0, 0, 255));
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(1000);
    bp5758d_set_standby_mode(true);
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(1000);
    bp5758d_set_standby_mode(true);
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(1000);

    //6. deinit
    TEST_ESP_OK(bp5758d_set_shutdown());
    // Wait for data transmission to complete
    vTaskDelay(1000);
    TEST_ESP_OK(bp5758d_deinit());
}

TEST_CASE("BP5758D", "[Application Layer]")
{
    lightbulb_config_t config = {
        .type = DRIVER_BP5758D,
        .driver_conf.bp5758d.current = {10, 10, 10, 20, 20},
        .driver_conf.bp5758d.iic_clk = 4,
        .driver_conf.bp5758d.iic_sda = 5,
        .driver_conf.bp5758d.freq_khz = 300,
        .driver_conf.bp5758d.enable_iic_queue = true,
        .capability.enable_fades = true,
        .capability.fades_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_mix_cct = true,
        .capability.enable_status_storage = false,
        .capability.mode_mask = COLOR_AND_WHITE_MODE,
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
    TEST_ESP_OK(lightbulb_init(&config));
    vTaskDelay(1000);
    lightbulb_lighting_output_test(LIGHTING_ALL_UNIT, 2000);
    TEST_ESP_OK(lightbulb_deinit());
}
#endif

#ifdef CONFIG_ENABLE_BP1658CJ_DRIVER
TEST_CASE("BP1658CJ", "[Underlying Driver]")
{
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
            .iic_sda = 5,
            .freq_khz = 300,
            .enable_iic_queue = true
        };
        TEST_ESP_OK(bp1658cj_init(&conf));

        //3. regist Check, step 1
        TEST_ESP_OK(bp1658cj_regist_channel(BP1658CJ_CHANNEL_R, BP1658CJ_PIN_OUT1));
        TEST_ESP_OK(bp1658cj_regist_channel(BP1658CJ_CHANNEL_G, BP1658CJ_PIN_OUT2));
        TEST_ESP_OK(bp1658cj_regist_channel(BP1658CJ_CHANNEL_B, BP1658CJ_PIN_OUT3));
        TEST_ESP_OK(bp1658cj_set_channel(BP1658CJ_CHANNEL_R, 1));
        TEST_ESP_OK(bp1658cj_set_channel(BP1658CJ_CHANNEL_G, 1));
        TEST_ESP_OK(bp1658cj_set_channel(BP1658CJ_CHANNEL_B, 1));
        TEST_ESP_OK(bp1658cj_set_rgb_channel(1, 1, 1));
        TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp1658cj_set_channel(BP1658CJ_CHANNEL_C, 1));
        TEST_ESP_ERR(ESP_ERR_INVALID_STATE, bp1658cj_set_channel(BP1658CJ_CHANNEL_W, 1));

        //3. regist Check, step 2
        TEST_ESP_OK(bp1658cj_regist_channel(BP1658CJ_CHANNEL_C, BP1658CJ_PIN_OUT5));
        TEST_ESP_OK(bp1658cj_regist_channel(BP1658CJ_CHANNEL_W, BP1658CJ_PIN_OUT4));
        TEST_ESP_OK(bp1658cj_set_channel(BP1658CJ_CHANNEL_C, 1));
        TEST_ESP_OK(bp1658cj_set_channel(BP1658CJ_CHANNEL_W, 1));
        TEST_ESP_OK(bp1658cj_set_cw_channel(1, 1));

        //4. Data range check
        TEST_ESP_ERR(ESP_ERR_INVALID_ARG, bp1658cj_set_channel(BP1658CJ_CHANNEL_R, 1024));

        //5. Color check
        TEST_ESP_OK(bp1658cj_set_shutdown());
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_rgb_channel(255, 0, 0));
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_rgb_channel(0, 255, 0));
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_rgb_channel(0, 0, 255));
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_shutdown());
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_cw_channel(255, 0));
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_cw_channel(0, 255));
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_shutdown());
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_rgbcw_channel(255, 0, 0, 0, 0));
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_rgbcw_channel(0, 255, 0, 0, 0));
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_rgbcw_channel(0, 0, 255, 0, 0));
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_rgbcw_channel(0, 0, 0, 255, 0));
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_rgbcw_channel(0, 0, 0, 0, 255));
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_rgbcw_channel(255, 0, 0, 0, 0));
        vTaskDelay(1000);
        bp1658cj_set_sleep_mode(true);
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_rgbcw_channel(0, 255, 0, 0, 0));
        vTaskDelay(1000);
        bp1658cj_set_sleep_mode(true);
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_set_rgbcw_channel(0, 0, 255, 0, 0));
        vTaskDelay(1000);

        //6. deinit
        TEST_ESP_OK(bp1658cj_set_shutdown());
        // Wait for data transmission to complete
        vTaskDelay(1000);
        TEST_ESP_OK(bp1658cj_deinit());
    }
}

TEST_CASE("BP1658CJ", "[Application Layer]")
{
    lightbulb_config_t config = {
        .type = DRIVER_BP1658CJ,
        .driver_conf.bp1658cj.rgb_current = BP1658CJ_RGB_CURRENT_10MA,
        .driver_conf.bp1658cj.cw_current = BP1658CJ_CW_CURRENT_30MA,
        .driver_conf.bp1658cj.iic_clk = 4,
        .driver_conf.bp1658cj.iic_sda = 5,
        .driver_conf.bp1658cj.freq_khz = 300,
        .driver_conf.bp1658cj.enable_iic_queue = true,
        .capability.enable_fades = true,
        .capability.fades_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_mix_cct = true,
        .capability.enable_status_storage = false,
        .capability.mode_mask = COLOR_AND_WHITE_MODE,
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
    TEST_ESP_OK(lightbulb_init(&config));
    vTaskDelay(1000);
    lightbulb_lighting_output_test(LIGHTING_ALL_UNIT, 2000);
    TEST_ESP_OK(lightbulb_deinit());
}
#endif

#ifdef CONFIG_ENABLE_SM2x35EGH_DRIVER
TEST_CASE("SM2235EGH", "[Underlying Driver]")
{
    //1.Status check
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_R, 1023));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2x35egh_set_rgb_channel(1023, 1023, 0));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2x35egh_set_cw_channel(1023, 1023));

    //2. init check
    driver_sm2x35egh_t conf = {
        .rgb_current = SM2235EGH_CW_CURRENT_10MA,
        .cw_current = SM2235EGH_CW_CURRENT_40MA,
        .iic_clk = 5,
        .iic_sda = 4,
        .freq_khz = 300,
        .enable_iic_queue = true
    };
    TEST_ESP_OK(sm2x35egh_init(&conf));

    //3. regist Check, step 1
    TEST_ESP_OK(sm2x35egh_regist_channel(SM2x35EGH_CHANNEL_R, SM2x35EGH_PIN_OUT3));
    TEST_ESP_OK(sm2x35egh_regist_channel(SM2x35EGH_CHANNEL_G, SM2x35EGH_PIN_OUT2));
    TEST_ESP_OK(sm2x35egh_regist_channel(SM2x35EGH_CHANNEL_B, SM2x35EGH_PIN_OUT1));
    TEST_ESP_OK(sm2x35egh_set_channel(SM2x35EGH_CHANNEL_R, 1));
    TEST_ESP_OK(sm2x35egh_set_channel(SM2x35EGH_CHANNEL_G, 1));
    TEST_ESP_OK(sm2x35egh_set_channel(SM2x35EGH_CHANNEL_B, 1));
    TEST_ESP_OK(sm2x35egh_set_rgb_channel(1, 1, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_C, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_W, 1));

    //3. regist Check, step 2
    TEST_ESP_OK(sm2x35egh_regist_channel(SM2x35EGH_CHANNEL_C, SM2x35EGH_PIN_OUT4));
    TEST_ESP_OK(sm2x35egh_regist_channel(SM2x35EGH_CHANNEL_W, SM2x35EGH_PIN_OUT5));
    TEST_ESP_OK(sm2x35egh_set_channel(SM2x35EGH_CHANNEL_C, 1));
    TEST_ESP_OK(sm2x35egh_set_channel(SM2x35EGH_CHANNEL_W, 1));
    TEST_ESP_OK(sm2x35egh_set_cw_channel(1, 1));

    //4. Data range check
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, sm2x35egh_set_channel(SM2x35EGH_CHANNEL_R, 1024));

    //5. Color check
    TEST_ESP_OK(sm2x35egh_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_rgb_channel(255, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_rgb_channel(0, 255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_rgb_channel(0, 0, 255));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_cw_channel(255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_cw_channel(0, 255));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_rgbcw_channel(0, 0, 0, 255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_rgbcw_channel(0, 0, 0, 0, 255));
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(1000);
    sm2x35egh_set_standby_mode(true);
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(1000);
    sm2x35egh_set_standby_mode(true);
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(1000);

    //6. deinit
    TEST_ESP_OK(sm2x35egh_set_shutdown());
    // Wait for data transmission to complete
    vTaskDelay(1000);
    TEST_ESP_OK(sm2x35egh_deinit());
}

TEST_CASE("SM2235EGH", "[Application Layer]")
{
    lightbulb_config_t config = {
        .type = DRIVER_SM2235EGH,
        .driver_conf.sm2235egh.rgb_current = SM2235EGH_RGB_CURRENT_20MA,
        .driver_conf.sm2235egh.cw_current = SM2235EGH_CW_CURRENT_40MA,
        .driver_conf.sm2235egh.iic_clk = 5,
        .driver_conf.sm2235egh.iic_sda = 4,
        .driver_conf.sm2235egh.freq_khz = 400,
        .driver_conf.sm2235egh.enable_iic_queue = true,
        .capability.enable_fades = true,
        .capability.fades_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_mix_cct = true,
        .capability.enable_status_storage = false,
        .capability.mode_mask = COLOR_AND_WHITE_MODE,
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
    TEST_ESP_OK(lightbulb_init(&config));
    vTaskDelay(1000);
    lightbulb_lighting_output_test(LIGHTING_ALL_UNIT, 2000);
    TEST_ESP_OK(lightbulb_deinit());
}
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
TEST_CASE("WS2812", "[Underlying Driver]")
{
    driver_ws2812_t ws2812 = {
        .led_num = 1,
        .ctrl_io = 4,
    };

    ws2812_init(&ws2812);
    ws2812_set_rgb_channel(255, 0, 0);
    vTaskDelay(1000);
    ws2812_set_rgb_channel(0, 255, 0);
    vTaskDelay(1000);
    ws2812_set_rgb_channel(0, 0, 255);
    ws2812_deinit();
}

TEST_CASE("WS2812", "[Application Layer]")
{
    lightbulb_config_t config = {
        .type = DRIVER_WS2812,
        .driver_conf.ws2812.led_num = 22,
        .driver_conf.ws2812.ctrl_io = 2,
        .capability.enable_fades = true,
        .capability.fades_ms = 800,
        .capability.enable_status_storage = false,
        .capability.mode_mask = COLOR_MODE,
        .capability.storage_cb = NULL,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    TEST_ESP_OK(lightbulb_init(&config));
    vTaskDelay(1000);
    lightbulb_lighting_output_test(LIGHTING_RAINBOW | LIGHTING_COLOR_EFFECT | LIGHTING_ALEXA, 2000);
    TEST_ESP_OK(lightbulb_deinit());
}
#endif

#ifdef CONFIG_ENABLE_KP18058_DRIVER
TEST_CASE("KP18058", "[Underlying Driver]")
{
    driver_kp18058_t kp18058 = {
        .rgb_current_multiple = 10,
        .cw_current_multiple = 10,
        .iic_clk = 5,
        .iic_sda = 4,
        .iic_freq_khz = 300,
        .enable_iic_queue = true,
    };
    TEST_ESP_OK(kp18058_init(&kp18058));

    //3. regist Check, step 1
    TEST_ESP_OK(kp18058_regist_channel(KP18058_CHANNEL_R, KP18058_PIN_OUT3));
    TEST_ESP_OK(kp18058_regist_channel(KP18058_CHANNEL_G, KP18058_PIN_OUT2));
    TEST_ESP_OK(kp18058_regist_channel(KP18058_CHANNEL_B, KP18058_PIN_OUT1));
    TEST_ESP_OK(kp18058_set_channel(KP18058_CHANNEL_R, 1));
    TEST_ESP_OK(kp18058_set_channel(KP18058_CHANNEL_G, 1));
    TEST_ESP_OK(kp18058_set_channel(KP18058_CHANNEL_B, 1));
    TEST_ESP_OK(kp18058_set_rgb_channel(1, 1, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, kp18058_set_channel(KP18058_CHANNEL_C, 1));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, kp18058_set_channel(KP18058_CHANNEL_W, 1));

    //3. regist Check, step 2
    TEST_ESP_OK(kp18058_regist_channel(KP18058_CHANNEL_C, KP18058_PIN_OUT4));
    TEST_ESP_OK(kp18058_regist_channel(KP18058_CHANNEL_W, KP18058_PIN_OUT5));
    TEST_ESP_OK(kp18058_set_channel(KP18058_CHANNEL_C, 1));
    TEST_ESP_OK(kp18058_set_channel(KP18058_CHANNEL_W, 1));
    TEST_ESP_OK(kp18058_set_cw_channel(1, 1));

    //4. Data range check
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, kp18058_set_channel(KP18058_CHANNEL_R, 1024));

    //5. Color check
    TEST_ESP_OK(kp18058_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_rgb_channel(255, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_rgb_channel(0, 255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_rgb_channel(0, 0, 255));
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_cw_channel(255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_cw_channel(0, 255));
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_shutdown());
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_rgbcw_channel(0, 0, 0, 255, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_rgbcw_channel(0, 0, 0, 0, 255));
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_rgbcw_channel(255, 0, 0, 0, 0));
    vTaskDelay(1000);
    kp18058_set_standby_mode(true);
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_rgbcw_channel(0, 255, 0, 0, 0));
    vTaskDelay(1000);
    kp18058_set_standby_mode(true);
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_rgbcw_channel(0, 0, 255, 0, 0));
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_set_rgbcw_channel(0, 0, 0, 0, 0));
    vTaskDelay(1000);

    //6. deinit
    TEST_ESP_OK(kp18058_set_shutdown());
    // Wait for data transmission to complete
    vTaskDelay(1000);
    TEST_ESP_OK(kp18058_deinit());
}

TEST_CASE("KP18058", "[Application Layer]")
{
    lightbulb_config_t config = {
        .type = DRIVER_KP18058,
        .driver_conf.kp18058.rgb_current_multiple = 15,
        .driver_conf.kp18058.cw_current_multiple = 20,
        .driver_conf.kp18058.iic_clk = 5,
        .driver_conf.kp18058.iic_sda = 4,
        .driver_conf.kp18058.iic_freq_khz = 300,
        .driver_conf.kp18058.enable_iic_queue = true,
        .capability.enable_fades = true,
        .capability.fades_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_mix_cct = true,
        .capability.enable_status_storage = false,
        .capability.mode_mask = COLOR_AND_WHITE_MODE,
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
    TEST_ESP_OK(lightbulb_init(&config));
    vTaskDelay(1000);
    lightbulb_lighting_output_test(LIGHTING_ALL_UNIT, 2000);
    TEST_ESP_OK(lightbulb_deinit());
}
#endif
