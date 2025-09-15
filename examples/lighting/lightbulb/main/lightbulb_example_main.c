/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
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
#ifdef CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_SM16825E
        .type = DRIVER_SM16825E,
        .driver_conf.sm16825e.led_num = CONFIG_SM16825E_LED_NUM,
        .driver_conf.sm16825e.ctrl_io = CONFIG_SM16825E_LED_GPIO,
        .io_conf.sm16825e_io.red = OUT4,
        .io_conf.sm16825e_io.green = OUT1,
        .io_conf.sm16825e_io.blue = OUT5,
        .io_conf.sm16825e_io.cold_white = OUT3,
        .io_conf.sm16825e_io.warm_yellow = OUT2,
        .driver_conf.sm16825e.current = {150, 150, 150, 150, 150},
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
        .driver_conf.bp57x8d.current = {10, 10, 10, 30, 30},
#endif
        // 2. Configure the drive capability
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_status_storage = false,

#if CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_WS2812
        .capability.led_beads = LED_BEADS_3CH_RGB,
#elif CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_SM16825E
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
#elif CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_BP5758D
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
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
        .io_conf.iic_io.red = OUT2,
        .io_conf.iic_io.green = OUT1,
        .io_conf.iic_io.blue = OUT3,
        .io_conf.iic_io.cold_white = OUT5,
        .io_conf.iic_io.warm_yellow = OUT4,
#endif
        //4. Limit param
        .external_limit = &limit,

        //5. Gamma param
        .gamma_conf = &Gamma,

        //6. Mix table config (optional)
#if CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_BP5758D
        .color_mix_mode.precise.table = color_data,
        .color_mix_mode.precise.table_size = COLOR_SZIE,
        .capability.enable_precise_color_control = 1,
#endif
#if CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_BP5758D && TEST_IIC_RGBWW_LIGHTBULB
        .cct_mix_mode.precise.table_size = MIX_TABLE_SIZE,
        .cct_mix_mode.precise.table = table,
        .capability.enable_precise_cct_control = 1,
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
#if CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_BP5758D || CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_SM16825E || (TEST_PWM_RGBCW_LIGHTBULB && CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_PWM)
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
