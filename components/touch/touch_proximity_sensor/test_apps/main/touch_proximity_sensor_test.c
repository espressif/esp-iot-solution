/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "unity.h"
#include "touch_proximity_sensor.h"

const static char *TAG = "touch-proximity-sensor-test";

#define TEST_MEMORY_LEAK_THRESHOLD (-200)
#define IO_BUZZER_CTRL          36
#define LEDC_LS_TIMER           LEDC_TIMER_1
#define LEDC_LS_CH0_CHANNEL     LEDC_CHANNEL_0
#define LEDC_LS_MODE            LEDC_LOW_SPEED_MODE

static bool s_buzzer_driver_installed = false;
static touch_proximity_handle_t s_touch_proximity_sensor = NULL;

static size_t before_free_8bit;
static size_t before_free_32bit;

static esp_err_t buzzer_driver_install(gpio_num_t buzzer_pin)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT,  //Resolution of PWM duty
        .freq_hz = 5000,                       //Frequency of PWM signal
        .speed_mode = LEDC_LS_MODE,            //Timer mode
        .timer_num = LEDC_LS_TIMER,            //Timer index
        .clk_cfg = LEDC_AUTO_CLK,              //Auto select the source clock (REF_TICK or APB_CLK or RTC_8M_CLK)
    };
    TEST_ESP_OK(ledc_timer_config(&ledc_timer));
    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_LS_CH0_CHANNEL,
        .duty       = 4096,
        .gpio_num   = buzzer_pin,
        .speed_mode = LEDC_LS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_LS_TIMER            //Let timer associate with LEDC channel (Timer1)
    };
    TEST_ESP_OK(ledc_channel_config(&ledc_channel));
    TEST_ESP_OK(ledc_fade_func_install(0));
    TEST_ESP_OK(ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0));
    TEST_ESP_OK(ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel));
    return ESP_OK;
}

static void buzzer_set_voice(bool en)
{
    uint32_t freq = en ? 5000 : 0;
    ledc_set_duty(LEDC_LS_MODE, LEDC_LS_CH0_CHANNEL, freq);
    ledc_update_duty(LEDC_LS_MODE, LEDC_LS_CH0_CHANNEL);
}

static void example_proxi_callback(uint32_t channel, proxi_evt_t event, void *cb_arg)
{
    switch (event) {
    case PROXI_EVT_ACTIVE:
        buzzer_set_voice(1);
        ESP_LOGI(TAG, "CH%"PRIu32", active!", channel);
        break;
    case PROXI_EVT_INACTIVE:
        buzzer_set_voice(0);
        ESP_LOGI(TAG, "CH%"PRIu32", inactive!", channel);
        break;
    default:
        break;
    }
}

TEST_CASE("touch proximity sensor APIs test", "[touch_proximity_sensor][API]")
{
    proxi_config_t config = (proxi_config_t)DEFAULTS_PROX_CONFIGS();
    config.channel_num = 1;
    config.meas_count = 50;
    config.channel_list[0] = TOUCH_PAD_NUM8;
    config.threshold_p[0] = 0.004;
    config.threshold_n[0] = 0.004;
    config.noise_p = 0.001;
    config.debounce_p = 1;
    esp_err_t ret = touch_proximity_sensor_create(&config, &s_touch_proximity_sensor, &example_proxi_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "touch proximity sense create failed");
    }
    touch_proximity_sensor_start(s_touch_proximity_sensor);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    touch_proximity_sensor_stop(s_touch_proximity_sensor);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    touch_proximity_sensor_delete(s_touch_proximity_sensor);
}

TEST_CASE("touch proximity sensor create & start test", "[ignore][touch_proximity_sensor][create & start]")
{
    if (s_buzzer_driver_installed == false) {
        if (buzzer_driver_install(IO_BUZZER_CTRL) == ESP_OK) {
            s_buzzer_driver_installed = true;
        } else {
            ESP_LOGW(TAG, "Buzzer driver install failed, skipping test");
        }
    }
    proxi_config_t config = (proxi_config_t)DEFAULTS_PROX_CONFIGS();
    config.channel_num = 1;
    config.meas_count = 50;
    config.channel_list[0] = TOUCH_PAD_NUM8;
    config.threshold_p[0] = 0.004;
    config.threshold_n[0] = 0.004;
    config.noise_p = 0.001;
    config.debounce_p = 1;
    esp_err_t ret = touch_proximity_sensor_create(&config, &s_touch_proximity_sensor, &example_proxi_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "touch proximity sense create failed");
    }
    touch_proximity_sensor_start(s_touch_proximity_sensor);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "touch proximity sensor has started! when you approach the touch sub-board, the buzzer will sound.");
}

TEST_CASE("touch proximity sensor stop & delete test", "[ignore][touch_proximity_sensor][stop & delete]")
{
    touch_proximity_sensor_stop(s_touch_proximity_sensor);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    touch_proximity_sensor_delete(s_touch_proximity_sensor);
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
    /*
    *   _____                _       ____                _           _ _           _____
    *  |_   _|__  _   _  ___| |__   |  _ \ _ __ _____  _(_)_ __ ___ (_) |_ _   _  |_   _|__  ___| |_
    *    | |/ _ \| | | |/ __| '_ \  | |_) | '__/ _ \ \/ / | '_ ` _ \| | __| | | |   | |/ _ \/ __| __|
    *    | | (_) | |_| | (__| | | | |  __/| | | (_) >  <| | | | | | | | |_| |_| |   | |  __/\__ \ |_
    *    |_|\___/ \__,_|\___|_| |_| |_|   |_|  \___/_/\_\_|_| |_| |_|_|\__|\__, |   |_|\___||___/\__|
    *                                                                      |___/
    */
    printf("  _____                _       ____                _           _ _           _____\n");
    printf(" |_   _|__  _   _  ___| |__   |  _ \\ _ __ _____  _(_)_ __ ___ (_) |_ _   _  |_   _|__  ___| |_\n");
    printf("   | |/ _ \\| | | |/ __| '_ \\  | |_) | '__/ _ \\ \\/ / | '_ ` _ \\| | __| | | |   | |/ _ \\/ __| __|\n");
    printf("   | | (_) | |_| | (__| | | | |  __/| | | (_) >  <| | | | | | | | |_| |_| |   | |  __/\\__ \\ |_\n");
    printf("   |_|\\___/ \\__,_|\\___|_| |_| |_|   |_|  \\___/_/\\_\\_|_| |_| |_|_|\\__|\\__, |   |_|\\___||___/\\__|\n");
    printf("                                                                     |___/\n");
    unity_run_menu();
}
