/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <esp_log.h>

#include "lightbulb.h"

const char *TAG = "lightbulb demo";

void app_main(void)
{
    /* Not a warning, just highlighted */
    ESP_LOGW(TAG, "Lightbulb Example Start");
    vTaskDelay(pdMS_TO_TICKS(1000) * 1);

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
        .driver_conf.pwm.phase_delay.flag = PWM_RGBCW_CHANNEL_PHASE_DELAY_FLAG,
#ifdef CONFIG_IDF_TARGET_ESP32C2
        /* Adapt to ESP8684-DevKitM-1
         * For details, please refer to:
         * https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp8684/esp8684-devkitm-1/user_guide.html
        */
        .driver_conf.pwm.invert_level = true,
#endif
#endif
#ifdef CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_SM2135E
        .type = DRIVER_SM2135E,
        .driver_conf.sm2135e.freq_khz = 400,
        .driver_conf.sm2135e.enable_iic_queue = true,
        .driver_conf.sm2135e.iic_clk = CONFIG_SM2135E_IIC_CLK_GPIO,
        .driver_conf.sm2135e.iic_sda = CONFIG_SM2135E_IIC_SDA_GPIO,
        .driver_conf.sm2135e.rgb_current = SM2135E_RGB_CURRENT_20MA,
        .driver_conf.sm2135e.wy_current = SM2135E_WY_CURRENT_40MA,
#endif
        // 2. Configure the drive capability
        .capability.enable_fades = true,
        .capability.fades_ms = 800,
        .capability.enable_status_storage = false,
        .capability.mode_mask = COLOR_MODE,
        .capability.storage_cb = NULL,

        //3. Configure driver io
#ifdef CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_PWM
        .io_conf.pwm_io.red = CONFIG_PWM_RED_GPIO,
        .io_conf.pwm_io.green = CONFIG_PWM_GREEN_GPIO,
        .io_conf.pwm_io.blue = CONFIG_PWM_BLUE_GPIO,
#endif
#ifdef CONFIG_LIGHTBULB_DEMO_DRIVER_SELECT_SM2135E
        .io_conf.iic_io.red = OUT3,
        .io_conf.iic_io.green = OUT2,
        .io_conf.iic_io.blue = OUT1,
        .io_conf.iic_io.cold_white = OUT5,
        .io_conf.iic_io.warm_yellow = OUT4,
#endif
        //4. Limit param
        .external_limit = NULL,

        //5. Gamma param
        .gamma_conf = NULL,

        //6. Init param
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
    vTaskDelay(pdMS_TO_TICKS(1000) * 1);

    /**
     * @brief Some preset lighting unit
     * @note rainbow
     *
     *      red -> orange -> yellow -> green -> blue -> indigo -> purple
     *
     * @note color effect
     *
     *      You will see the red breathing/blinking lighting
     *
     * @note Alexa
     *
     *      These colors are from the Alexa App Color Control Panel
     *
     */
    lightbulb_lighting_output_test(LIGHTING_RAINBOW | LIGHTING_COLOR_EFFECT | LIGHTING_ALEXA, 2000);
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
