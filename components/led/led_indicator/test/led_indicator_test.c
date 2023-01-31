/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_idf_version.h"
#include "led_indicator.h"
#include "led_indicator_blink_default.h"
#include "unity.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define portTICK_RATE_MS 1
#endif

#define LED_IO_NUM_0    15
#define LED_IO_NUM_1    16
#define LED_IO_NUM_2    17

#define TAG "LED indicator"

static led_indicator_handle_t led_handle_0 = NULL;
static led_indicator_handle_t led_handle_1 = NULL;
static led_indicator_handle_t led_handle_2 = NULL;

void led_indicator_init()
{
    led_indicator_gpio_config_t led_indicator_gpio_config = {
        .gpio_num = LED_IO_NUM_0,              /**< num of GPIO */
        .is_active_level_high = 0,
    };

    led_indicator_config_t config = {
        .led_indicator_gpio_config = &led_indicator_gpio_config,
        .mode = LED_GPIO_MODE,
        .blink_lists = (void *)NULL,
        .blink_list_num = 0,
    };

    led_handle_0 = led_indicator_create(&config);
    TEST_ASSERT_NOT_NULL(led_handle_0);
    TEST_ASSERT(led_handle_0 == led_indicator_get_handle((void *)LED_IO_NUM_0)); //test get handle
}

void led_indicator_deinit()
{
    esp_err_t ret = led_indicator_delete(led_handle_0);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NULL(led_handle_0);
}

void led_indicator_gpio_mode_test_all()
{
    ESP_LOGI(TAG, "connecting.....");
    esp_err_t ret = led_indicator_start(led_handle_0, BLINK_CONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_CONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "connected.....");
    ret = led_indicator_start(led_handle_0, BLINK_CONNECTED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_CONNECTED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "reconnecting.....");
    ret = led_indicator_start(led_handle_0, BLINK_RECONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_RECONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "updating.....");
    ret = led_indicator_start(led_handle_0, BLINK_UPDATING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_UPDATING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "factory_reset.....");
    ret = led_indicator_start(led_handle_0, BLINK_FACTORY_RESET);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_FACTORY_RESET);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "provisioning.....");
    ret = led_indicator_start(led_handle_0, BLINK_PROVISIONING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_PROVISIONING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "provisioned.....");
    ret = led_indicator_start(led_handle_0, BLINK_PROVISIONED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_PROVISIONED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "test all condition done.....");
}

TEST_CASE("blink test all in order", "[LED][indicator]")
{
    led_indicator_init();
    led_indicator_gpio_mode_test_all();
    led_indicator_deinit();
}

void led_indicator_gpio_mode_preempt()
{
    ESP_LOGI(TAG, "connecting.....");
    esp_err_t ret = led_indicator_start(led_handle_0, BLINK_CONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(3000 / portTICK_RATE_MS);


    ESP_LOGI(TAG, "factory_reset.....");
    ret = led_indicator_start(led_handle_0, BLINK_FACTORY_RESET); //higer priority than connecting
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(3000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "factory_reset stop");
    ret = led_indicator_stop(led_handle_0, BLINK_FACTORY_RESET); //then switch to low priority
    TEST_ASSERT(ret == ESP_OK);
    ESP_LOGI(TAG, "connecting.....");
    vTaskDelay(3000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "connecting stop");
    ret = led_indicator_stop(led_handle_0, BLINK_CONNECTING);
    TEST_ASSERT(ret == ESP_OK);
}

TEST_CASE("blink test with preempt", "[LED][indicator]")
{
    led_indicator_init();
    led_indicator_gpio_mode_preempt();
    led_indicator_deinit();
}

void led_indicator_all_init()
{
    led_indicator_gpio_config_t led_indicator_gpio_config = {
        .gpio_num = LED_IO_NUM_0,              /**< num of GPIO */
        .is_active_level_high = 0,
    };

    led_indicator_config_t config = {
        .led_indicator_gpio_config = &led_indicator_gpio_config,
        .mode = LED_GPIO_MODE,
        .blink_lists = NULL,
        .blink_list_num = 0,
    };

    led_handle_0 = led_indicator_create(&config);

    TEST_ASSERT_NOT_NULL(led_handle_0);
    led_indicator_gpio_config.gpio_num = LED_IO_NUM_1;
    led_handle_1 = led_indicator_create(&config);
    TEST_ASSERT_NOT_NULL(led_handle_1);
    led_indicator_gpio_config.gpio_num = LED_IO_NUM_2;
    led_handle_2 = led_indicator_create(&config);
    TEST_ASSERT_NOT_NULL(led_handle_2);
    TEST_ASSERT(led_handle_0 == led_indicator_get_handle((void *)LED_IO_NUM_0)); //test get handle
    TEST_ASSERT(led_handle_1 == led_indicator_get_handle((void *)LED_IO_NUM_1)); //test get handle
    TEST_ASSERT(led_handle_2 == led_indicator_get_handle((void *)LED_IO_NUM_2)); //test get handle
}

void led_indicator_all_deinit()
{
    esp_err_t ret = led_indicator_delete(led_handle_0);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NULL(led_handle_0);
    ret = led_indicator_delete(led_handle_1);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NULL(led_handle_1);
    ret = led_indicator_delete(led_handle_2);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NULL(led_handle_2);
}

void led_indicator_gpio_mode_three_led()
{
    led_indicator_handle_t led_connect = led_handle_0;
    led_indicator_handle_t provision = led_handle_1;
    led_indicator_handle_t led_system = led_handle_2;

    ESP_LOGI(TAG, "provisioning.....");
    esp_err_t ret = led_indicator_start(provision, BLINK_PROVISIONING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "provisioned.....");
    ret = led_indicator_stop(provision, BLINK_PROVISIONING);
    TEST_ASSERT(ret == ESP_OK);
    ret = led_indicator_start(provision, BLINK_PROVISIONED);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "connecting.....");
    ret = led_indicator_start(led_connect, BLINK_CONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "connected.....");
    ret = led_indicator_stop(led_connect, BLINK_CONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    ret = led_indicator_start(led_connect, BLINK_CONNECTED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "lost connection");
    ret = led_indicator_stop(led_connect, BLINK_CONNECTED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "reconnecting.....");
    ret = led_indicator_start(led_connect, BLINK_RECONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "reconnected.....");
    ret = led_indicator_stop(led_connect, BLINK_RECONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    ret = led_indicator_start(led_connect, BLINK_CONNECTED);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "updating.....");
    ret = led_indicator_start(led_system, BLINK_UPDATING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = led_indicator_stop(led_system, BLINK_UPDATING);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "factory_reset.....");
    ret = led_indicator_start(led_system, BLINK_FACTORY_RESET);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = led_indicator_stop(led_system, BLINK_FACTORY_RESET);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "test all condition done.....");
}

TEST_CASE("blink three LED", "[LED][indicator]")
{
    led_indicator_all_init();
    led_indicator_gpio_mode_three_led();
    led_indicator_all_deinit();
}

typedef enum {
    BLINK_DOUBLE,
    BLINK_TRIPLE,
    BLINK_FAST,
    BLINK_NUM,
} led_blink_type_t;

static const blink_step_t double_blink[] = {
    {LED_BLINK_HOLD,  LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t triple_blink[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t fast_blink[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_LOOP, 0, 0},
};

static blink_step_t const *led_blink_lst[] = {
    [BLINK_DOUBLE] = double_blink,
    [BLINK_TRIPLE] = triple_blink,
    [BLINK_FAST] = fast_blink,
    [BLINK_NUM] = NULL,
};


TEST_CASE("User defined blink", "[LED][indicator]")
{
    led_indicator_gpio_config_t led_indicator_gpio_config = {
        .gpio_num = LED_IO_NUM_0,              /**< num of GPIO */
        .is_active_level_high = 1,
    };

    led_indicator_config_t config = {
        .led_indicator_gpio_config = &led_indicator_gpio_config,
        .mode = LED_GPIO_MODE,
        .blink_lists = led_blink_lst,
        .blink_list_num = BLINK_NUM,
    };

    led_handle_0 = led_indicator_create(&config);
    TEST_ASSERT_NOT_NULL(led_handle_0);
    TEST_ASSERT(led_handle_0 == led_indicator_get_handle((void *)LED_IO_NUM_0)); //test get handle

    ESP_LOGI(TAG, "double blink.....");
    esp_err_t ret = led_indicator_start(led_handle_0, BLINK_DOUBLE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_RATE_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_DOUBLE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "triple blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_TRIPLE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_RATE_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_TRIPLE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    led_indicator_deinit();
}

TEST_CASE("Preempt blink lists test", "[LED][indicator]")
{
    led_indicator_gpio_config_t led_indicator_gpio_config = {
        . gpio_num = LED_IO_NUM_0,              /**< num of GPIO */
        . is_active_level_high = 1,
    };

    led_indicator_config_t config = {
        .led_indicator_gpio_config = &led_indicator_gpio_config,
        .mode = LED_GPIO_MODE,
        .blink_lists = led_blink_lst,
        .blink_list_num = BLINK_NUM,
    };

    led_handle_0 = led_indicator_create(&config);
    TEST_ASSERT_NOT_NULL(led_handle_0);
    TEST_ASSERT(led_handle_0 == led_indicator_get_handle((void *)LED_IO_NUM_0)); //test get handle

    ESP_LOGI(TAG, "double blink.....");
    esp_err_t ret = led_indicator_start(led_handle_0, BLINK_DOUBLE);
    vTaskDelay(1000 / portTICK_RATE_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "fast blink preempt .....");
    ret = led_indicator_preempt_start(led_handle_0, BLINK_FAST);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "fast blink preempt stop.....");
    ret = led_indicator_preempt_stop(led_handle_0, BLINK_FAST);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "triple blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_TRIPLE);
    TEST_ASSERT(ret == ESP_OK);

    vTaskDelay(4000 / portTICK_RATE_MS);

    led_indicator_deinit();
}

typedef enum {
    BLINK_DOUBLE_BRIGHTNESS,
    BLINK_25_BRIGHTNESS,
    BLINK_50_BRIGHTNESS,
    BLINK_75_BRIGHTNESS,
    BLINK_BREATHE,
    BLINK_BREATHE_NUM,
} led_breathe_blink_type_t;

static const blink_step_t breathe_blink[] = {
    {LED_BLINK_BREATHE, LED_STATE_ON, 1000},
    {LED_BLINK_BRIGHTNESS, LED_STATE_ON, 500},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 1000},
    {LED_BLINK_BRIGHTNESS, LED_STATE_OFF, 500},
    {LED_BLINK_BREATHE, LED_STATE_25_PERCENT, 1000},
    {LED_BLINK_BRIGHTNESS, LED_STATE_25_PERCENT, 500},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t brightness_25_blink[] = {
    {LED_BLINK_BRIGHTNESS, LED_STATE_25_PERCENT, 1000},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t brightness_50_blink[] = {
    {LED_BLINK_BRIGHTNESS, LED_STATE_50_PERCENT, 1000},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t brightness_75_blink[] = {
    {LED_BLINK_BRIGHTNESS, LED_STATE_75_PERCENT, 1000},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t brightness_double_blink[] = {
    {LED_BLINK_HOLD, LED_STATE_ON ,500},
    {LED_BLINK_HOLD, LED_STATE_OFF ,1000},
    {LED_BLINK_HOLD, LED_STATE_ON ,500},
    {LED_BLINK_HOLD, LED_STATE_OFF ,500},
    {LED_BLINK_STOP, 0, 0},
};

static blink_step_t const *breathe_led_blink_lst[] = {
    [BLINK_DOUBLE_BRIGHTNESS] = brightness_double_blink,
    [BLINK_BREATHE] = breathe_blink,
    [BLINK_25_BRIGHTNESS] = brightness_25_blink,
    [BLINK_50_BRIGHTNESS] = brightness_50_blink,
    [BLINK_75_BRIGHTNESS] = brightness_75_blink,
    [BLINK_BREATHE_NUM] = NULL,
};

TEST_CASE("breathe test", "[LED][indicator]")
{
    ledc_timer_config_t timer_config = {
        .duty_resolution = LEDC_TIMER_13_BIT,        // resolution of PWM duty
        .freq_hz = 5000,                             // frequency of PWM signal
        .speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
        .timer_num = LEDC_TIMER_1,                   // timer index
        .clk_cfg = LEDC_AUTO_CLK,                    // Auto select the source clock
    };

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0,
        .gpio_num   = LED_IO_NUM_0,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_1,
        .flags.output_invert = 0
    };

    led_indicator_ledc_config_t led_indicator_ledc_config = {
        .is_active_level_high = 1,
        .ledc_timer_config = &timer_config,
        .ledc_channel_config = &ledc_channel,
    };

    led_indicator_config_t config = {
        .led_indicator_ledc_config = &led_indicator_ledc_config,
        .mode = LED_LEDC_MODE,
        .blink_lists = breathe_led_blink_lst,
        .blink_list_num = BLINK_BREATHE_NUM,
    };

    led_handle_0 = led_indicator_create(&config);
    TEST_ASSERT_NOT_NULL(led_handle_0);
    TEST_ASSERT(led_handle_0 == led_indicator_get_handle((void *)LEDC_CHANNEL_0)); //test get handle

    ESP_LOGI(TAG, "breathe blink .....");
    esp_err_t ret = led_indicator_start(led_handle_0, BLINK_BREATHE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(6000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "breathe 25/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_25_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_RATE_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe 50/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_50_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_RATE_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe 75/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_75_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_RATE_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "double breathe blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_DOUBLE_BRIGHTNESS);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(3000 / portTICK_RATE_MS);

    led_indicator_deinit();
}

static esp_err_t custom_ledc_init(void *param)
{
    ledc_timer_config_t timer_config = {
        .duty_resolution = LEDC_TIMER_13_BIT,        // resolution of PWM duty
        .freq_hz = 5000,                             // frequency of PWM signal
        .speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
        .timer_num = LEDC_TIMER_1,                   // timer index
        .clk_cfg = LEDC_AUTO_CLK,                    // Auto select the source clock
    };

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0,
        .gpio_num   = LED_IO_NUM_0,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_1,
        .flags.output_invert = 0
    };

    led_indicator_ledc_config_t custom_led_indicator_ledc_config = {
        .is_active_level_high = 1,
        .ledc_timer_config = &timer_config,
        .ledc_channel_config = &ledc_channel,
    };

    esp_err_t ret = led_indicator_ledc_init(&custom_led_indicator_ledc_config);
    return ret;
}

TEST_CASE("custom mode test", "[LED][indicator]")
{
    led_indicator_custom_config_t led_indicator_custom_config = {
        .is_active_level_high = 1,
        .duty_resolution = LED_DUTY_13_BIT,
        .hal_indicator_init = custom_ledc_init,
        .hal_indicator_deinit = led_indicator_ledc_deinit,
        .hal_indicator_set_on_off = led_indicator_ledc_set_on_off,
        .hal_indicator_set_brightness = led_indicator_ledc_set_brightness,
        .hardware_data = (void *)LEDC_CHANNEL_0,
    };

    led_indicator_config_t config = {
        .led_indicator_custom_config = &led_indicator_custom_config,
        .mode = LED_CUSTOM_MODE,
        .blink_lists = breathe_led_blink_lst,
        .blink_list_num = BLINK_BREATHE_NUM,
    };

    led_handle_0 = led_indicator_create(&config);
    TEST_ASSERT_NOT_NULL(led_handle_0);
    TEST_ASSERT(led_handle_0 == led_indicator_get_handle((void *)LEDC_CHANNEL_0)); //test get handle

    ESP_LOGI(TAG, "breathe blink .....");
    esp_err_t ret = led_indicator_start(led_handle_0, BLINK_BREATHE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(6000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "breathe 25/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_25_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_RATE_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe 50/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_50_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_RATE_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe 75/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_75_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_RATE_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "double breathe blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_DOUBLE_BRIGHTNESS);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(3000 / portTICK_RATE_MS);

    led_indicator_deinit();
}