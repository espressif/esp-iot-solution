/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <esp_console.h>
#include <esp_log.h>

#include "lightbulb.h"
#include "lightbulb_example_cmd.h"

const char *TAG = "lightbulb demo";

//Based on PWM test 5ch (rgbcw)
#define TEST_PWM_RGBCW_LIGHTBULB        1
#define PWM_C_GPIO                      5
#define PWM_W_GPIO                      4

//Based on BP5758D test 5ch (rgbww)
#define TEST_IIC_RGBWW_LIGHTBULB        1
#define MIX_TABLE_SIZE                  11
lightbulb_cct_mapping_data_t table[MIX_TABLE_SIZE] = {
    {.cct_kelvin = 2200, .cct_percentage = 0, .rgbcw = {0.033, 0.033, 0.034, 0.45, 0.45}},
    {.cct_kelvin = 2600, .cct_percentage = 10, .rgbcw = {0.060, 0.060, 0.060, 0.41, 0.41}},
    {.cct_kelvin = 3000, .cct_percentage = 20, .rgbcw = {0.087, 0.087, 0.087, 0.37, 0.370}},
    {.cct_kelvin = 3400, .cct_percentage = 30, .rgbcw = {0.113, 0.113, 0.113, 0.33, 0.33}},
    {.cct_kelvin = 3900, .cct_percentage = 40, .rgbcw = {0.140, 0.140, 0.140, 0.29, 0.29}},
    {.cct_kelvin = 4300, .cct_percentage = 50, .rgbcw = {0.167, 0.167, 0.167, 0.25, 0.25}},
    {.cct_kelvin = 4700, .cct_percentage = 60, .rgbcw = {0.193, 0.193, 0.194, 0.21, 0.21}},
    {.cct_kelvin = 5200, .cct_percentage = 70, .rgbcw = {0.220, 0.220, 0.220, 0.17, 0.17}},
    {.cct_kelvin = 5600, .cct_percentage = 80, .rgbcw = {0.247, 0.247, 0.247, 0.13, 0.13}},
    {.cct_kelvin = 6000, .cct_percentage = 90, .rgbcw = {0.273, 0.273, 0.273, 0.09, 0.09}},
    {.cct_kelvin = 6500, .cct_percentage = 100, .rgbcw = {0.300, 0.300, 0.300, 0.05, 0.05}},
};

void app_main(void)
{
    /* Not a warning, just highlighted */
    ESP_LOGW(TAG, "Lightbulb Example Start");

    lightbulb_config_t config = {
        //1. Select and configure the chip
#ifdef CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_WS2812
        .type = DRIVER_WS2812,
        .driver_conf.ws2812.led_num = CONFIG_WS2812_LED_NUM,
        .driver_conf.ws2812.ctrl_io = CONFIG_WS2812_LED_GPIO,
#endif
#ifdef CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_PWM
        .type = DRIVER_ESP_PWM,
        .driver_conf.pwm.freq_hz = CONFIG_PWM_FREQ_HZ,
#ifdef CONFIG_IDF_TARGET_ESP32C2
        /* Adapt to ESP8684-DevKitM-1
         * For details, please refer to:
         * https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp8684/esp8684-devkitm-1/user_guide.html
        */
        .driver_conf.pwm.invert_level = true,
#endif
#endif
#ifdef CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_BP5758D
        .type = DRIVER_BP57x8D,
        .driver_conf.bp57x8d.freq_khz = 300,
        .driver_conf.bp57x8d.enable_iic_queue = true,
        .driver_conf.bp57x8d.iic_clk = CONFIG_BP5758D_IIC_CLK_GPIO,
        .driver_conf.bp57x8d.iic_sda = CONFIG_BP5758D_IIC_SDA_GPIO,
        .driver_conf.bp57x8d.current = {50, 50, 60, 30, 50},
#endif
        // 2. Configure the drive capability
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_status_storage = false,

#if CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_WS2812
        .capability.led_beads = LED_BEADS_3CH_RGB,
#elif CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_BP5758D
#if TEST_IIC_RGBWW_LIGHTBULB
        .capability.led_beads = LED_BEADS_5CH_RGBWW,
#else
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
#endif
#elif CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_PWM && TEST_PWM_RGBCW_LIGHTBULB
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
#else
        .capability.led_beads = LED_BEADS_3CH_RGB,
#endif
        .capability.storage_cb = NULL,

        //3. Configure driver io
#ifdef CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_PWM
        .io_conf.pwm_io.red = CONFIG_PWM_RED_GPIO,
        .io_conf.pwm_io.green = CONFIG_PWM_GREEN_GPIO,
        .io_conf.pwm_io.blue = CONFIG_PWM_BLUE_GPIO,
        .io_conf.pwm_io.cold_cct = PWM_C_GPIO,
        .io_conf.pwm_io.warm_brightness = PWM_W_GPIO,
#endif
#ifdef CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_BP5758D
        .io_conf.iic_io.red = OUT5,
        .io_conf.iic_io.green = OUT4,
        .io_conf.iic_io.blue = OUT3,
        .io_conf.iic_io.cold_white = OUT1,
        .io_conf.iic_io.warm_yellow = OUT2,
#endif
        //4. Limit param
        .external_limit = NULL,

        //5. Gamma param
        .gamma_conf = NULL,

        //6. Mix table config (optional)
#if CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_BP5758D && TEST_IIC_RGBWW_LIGHTBULB
        .cct_mix_mode.precise.table_size = MIX_TABLE_SIZE,
        .cct_mix_mode.precise.table = table,
        .capability.enable_precise_cct_control = true,
#else
        .cct_mix_mode.standard.kelvin_max = 6500,
        .cct_mix_mode.standard.kelvin_min = 2200,
#endif

        //7. Init param
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    esp_err_t err = lightbulb_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "There may be some errors in the configuration, please check the log.");
        return;
    }

    /**
     * @brief Initialize console function
     *
     */
    err = lightbulb_example_console_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to initialize console, please check the log.");
    } else {
        ESP_LOGW(TAG, "You can use the quit command to exit console mode.");
        while (lightbulb_example_get_console_status()) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    lightbulb_example_console_deinit();

    /**
     * @brief Some preset lighting unit
     * @note Rainbow
     *
     *      red -> orange -> yellow -> green -> blue -> indigo -> purple
     *
     * @note Color effect
     *
     *      You will see the red breathing/blinking lighting
     *
     * @note Alexa
     *
     *      These colors are from the Alexa App Color Control Panel
     *
     */
    ESP_LOGW(TAG, "Showing some preset lighting effects.");
    uint32_t test_case = LIGHTING_RAINBOW | LIGHTING_ALEXA | LIGHTING_COLOR_EFFECT;
#if CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_BP5758D || (TEST_PWM_RGBCW_LIGHTBULB && CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_PWM)
    test_case |= LIGHTING_BASIC_FIVE;
    test_case |= LIGHTING_WHITE_EFFECT;
#endif

    lightbulb_lighting_output_test(test_case, 2000);
    lightbulb_deinit();
    vTaskDelay(pdMS_TO_TICKS(2000));

#ifdef CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_WS2812
    /* Not a warning, just highlighted */
    ESP_LOGW(TAG, "Test underlying Driver");
    driver_ws2812_t ws2812 = {
        .led_num = CONFIG_WS2812_LED_NUM,
        .ctrl_io = CONFIG_WS2812_LED_GPIO,
    };
    ws2812_init(&ws2812, NULL);
    /* Red */
    ws2812_set_rgb_channel(255, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(2000) * 1);
    /* Green */
    ws2812_set_rgb_channel(0, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(2000) * 1);
    /* Blue */
    ws2812_set_rgb_channel(0, 0, 255);
    vTaskDelay(pdMS_TO_TICKS(2000) * 1);

    ws2812_deinit();
#endif

    /* Not a warning, just highlighted */
    ESP_LOGW(TAG, "Lightbulb Example End");
}
