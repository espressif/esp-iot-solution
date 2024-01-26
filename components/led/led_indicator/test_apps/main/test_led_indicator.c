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
#include "led_gamma.h"
#include "led_convert.h"

// Some resources are lazy allocated in pulse_cnt driver, the threshold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (-200)
#define LED_IO_NUM_0    11
#define LED_IO_NUM_1    12
#define LED_IO_NUM_2    13
#define LED_STRIP_BLINK_GPIO 48
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)
#define MAX_LED_NUM 16

#define TAG "LED indicator Test"

static led_indicator_handle_t led_handle_0 = NULL;
static led_indicator_handle_t led_handle_1 = NULL;
static led_indicator_handle_t led_handle_2 = NULL;

void led_indicator_init()
{
    led_indicator_gpio_config_t led_indicator_gpio_config = {
        .gpio_num = LED_IO_NUM_0,              /**< num of GPIO */
        .is_active_level_high = 1,
    };

    led_indicator_config_t config = {
        .led_indicator_gpio_config = &led_indicator_gpio_config,
        .mode = LED_GPIO_MODE,
        .blink_lists = (void *)NULL,
        .blink_list_num = 0,
    };

    led_handle_0 = led_indicator_create(&config);
    TEST_ASSERT_NOT_NULL(led_handle_0);
}

void led_indicator_deinit()
{
    ESP_LOGI(TAG, "deinit.....");
    esp_err_t ret = led_indicator_delete(led_handle_0);
    TEST_ASSERT(ret == ESP_OK);
    led_handle_0 = NULL;
}

void led_indicator_gpio_mode_test_all()
{
    ESP_LOGI(TAG, "connecting.....");
    esp_err_t ret = led_indicator_start(led_handle_0, BLINK_CONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_CONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "connected.....");
    ret = led_indicator_start(led_handle_0, BLINK_CONNECTED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_CONNECTED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "reconnecting.....");
    ret = led_indicator_start(led_handle_0, BLINK_RECONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_RECONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "updating.....");
    ret = led_indicator_start(led_handle_0, BLINK_UPDATING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_UPDATING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "factory_reset.....");
    ret = led_indicator_start(led_handle_0, BLINK_FACTORY_RESET);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_FACTORY_RESET);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "provisioning.....");
    ret = led_indicator_start(led_handle_0, BLINK_PROVISIONING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_PROVISIONING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "provisioned.....");
    ret = led_indicator_start(led_handle_0, BLINK_PROVISIONED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_PROVISIONED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

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
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "factory_reset.....");
    ret = led_indicator_start(led_handle_0, BLINK_FACTORY_RESET); //higer priority than connecting
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "factory_reset stop");
    ret = led_indicator_stop(led_handle_0, BLINK_FACTORY_RESET); //then switch to low priority
    TEST_ASSERT(ret == ESP_OK);
    ESP_LOGI(TAG, "connecting.....");
    vTaskDelay(3000 / portTICK_PERIOD_MS);

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
        .is_active_level_high = 1,
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
}

void led_indicator_all_deinit()
{
    esp_err_t ret = led_indicator_delete(led_handle_0);
    TEST_ASSERT(ret == ESP_OK);
    led_handle_0 = NULL;
    ret = led_indicator_delete(led_handle_1);
    TEST_ASSERT(ret == ESP_OK);
    led_handle_1 = NULL;
    ret = led_indicator_delete(led_handle_2);
    TEST_ASSERT(ret == ESP_OK);
    led_handle_2 = NULL;
}

void led_indicator_gpio_mode_three_led()
{
    led_indicator_handle_t led_connect = led_handle_0;
    led_indicator_handle_t provision = led_handle_1;
    led_indicator_handle_t led_system = led_handle_2;

    ESP_LOGI(TAG, "provisioning.....");
    esp_err_t ret = led_indicator_start(provision, BLINK_PROVISIONING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "provisioned.....");
    ret = led_indicator_stop(provision, BLINK_PROVISIONING);
    TEST_ASSERT(ret == ESP_OK);

    ret = led_indicator_start(provision, BLINK_PROVISIONED);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "connecting.....");
    ret = led_indicator_start(led_connect, BLINK_CONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "connected.....");
    ret = led_indicator_stop(led_connect, BLINK_CONNECTING);
    TEST_ASSERT(ret == ESP_OK);

    ret = led_indicator_start(led_connect, BLINK_CONNECTED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "lost connection");
    ret = led_indicator_stop(led_connect, BLINK_CONNECTED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "reconnecting.....");
    ret = led_indicator_start(led_connect, BLINK_RECONNECTING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "reconnected.....");
    ret = led_indicator_stop(led_connect, BLINK_RECONNECTING);
    TEST_ASSERT(ret == ESP_OK);

    ret = led_indicator_start(led_connect, BLINK_CONNECTED);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "updating.....");
    ret = led_indicator_start(led_system, BLINK_UPDATING);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    ret = led_indicator_stop(led_system, BLINK_UPDATING);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "factory_reset.....");
    ret = led_indicator_start(led_system, BLINK_FACTORY_RESET);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
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
    BLINK_25_BRIGHTNESS,
    BLINK_50_BRIGHTNESS,
    BLINK_75_BRIGHTNESS,
    BLINK_BREATHE,
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

static const blink_step_t breathe_blink[] = {
    {LED_BLINK_BREATHE, LED_STATE_ON, 1000},
    {LED_BLINK_BRIGHTNESS, LED_STATE_ON, 500},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 1000},
    {LED_BLINK_BRIGHTNESS, LED_STATE_OFF, 500},
    {LED_BLINK_LOOP, 0, 0},
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

static blink_step_t const *led_blink_lst[] = {
    [BLINK_25_BRIGHTNESS] = brightness_25_blink,
    [BLINK_50_BRIGHTNESS] = brightness_50_blink,
    [BLINK_75_BRIGHTNESS] = brightness_75_blink,
    [BLINK_BREATHE]       = breathe_blink,
    [BLINK_DOUBLE]        = double_blink,
    [BLINK_TRIPLE]        = triple_blink,
    [BLINK_FAST]          = fast_blink,
    [BLINK_NUM]           = NULL,
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

    ESP_LOGI(TAG, "double blink.....");
    esp_err_t ret = led_indicator_start(led_handle_0, BLINK_DOUBLE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_DOUBLE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "triple blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_TRIPLE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    ret = led_indicator_stop(led_handle_0, BLINK_TRIPLE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

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

    ESP_LOGI(TAG, "double blink.....");
    esp_err_t ret = led_indicator_start(led_handle_0, BLINK_DOUBLE);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "fast blink preempt .....");
    ret = led_indicator_preempt_start(led_handle_0, BLINK_FAST);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "fast blink preempt stop.....");
    ret = led_indicator_preempt_stop(led_handle_0, BLINK_FAST);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "triple blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_TRIPLE);
    TEST_ASSERT(ret == ESP_OK);

    vTaskDelay(4000 / portTICK_PERIOD_MS);

    led_indicator_deinit();
}

TEST_CASE("breathe test", "[LED][indicator]")
{
    led_indicator_ledc_config_t led_indicator_ledc_config = {
        .is_active_level_high = 1,
        .timer_inited = false,
        .timer_num = LEDC_TIMER_0,
        .gpio_num = LED_IO_NUM_0,
        .channel = LEDC_CHANNEL_0,
    };

    led_indicator_config_t config = {
        .mode = LED_LEDC_MODE,
        .led_indicator_ledc_config = &led_indicator_ledc_config,
        .blink_lists = led_blink_lst,
        .blink_list_num = BLINK_NUM,
    };

    led_handle_0 = led_indicator_create(&config);
    TEST_ASSERT_NOT_NULL(led_handle_0);

    ESP_LOGI(TAG, "breathe 25/100 blink.....");
    esp_err_t ret = led_indicator_start(led_handle_0, BLINK_25_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe 50/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_50_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe 75/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_75_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_BREATHE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(8000 / portTICK_PERIOD_MS);

    led_indicator_deinit();
}

static esp_err_t custom_ledc_init(void *param)
{
    led_indicator_ledc_config_t led_indicator_ledc_config = {
        .is_active_level_high = 1,
        .timer_inited = false,
        .timer_num = LEDC_TIMER_0,
        .gpio_num = LED_IO_NUM_0,
        .channel = LEDC_CHANNEL_0,
    };

    esp_err_t ret = led_indicator_ledc_init(&led_indicator_ledc_config);
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
        .hal_indicator_set_rgb = NULL,
        .hal_indicator_set_hsv = NULL,
        .hardware_data = (void *)LEDC_CHANNEL_0,
    };

    led_indicator_config_t config = {
        .led_indicator_custom_config = &led_indicator_custom_config,
        .mode = LED_CUSTOM_MODE,
        .blink_lists = led_blink_lst,
        .blink_list_num = BLINK_NUM,
    };

    led_handle_0 = led_indicator_create(&config);
    TEST_ASSERT_NOT_NULL(led_handle_0);

    ESP_LOGI(TAG, "breathe 25/100 blink.....");
    esp_err_t ret = led_indicator_start(led_handle_0, BLINK_25_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe 50/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_50_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe 75/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_75_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_BREATHE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(6000 / portTICK_PERIOD_MS);

    led_indicator_deinit();
}

TEST_CASE("test gamma table", "[LED][indicator]")
{
    led_indicator_new_gamma_table(2.3);
}

TEST_CASE("test led preempt func with breath", "[LED][preempt][breath]")
{
    led_indicator_ledc_config_t led_indicator_ledc_config = {
        .is_active_level_high = 1,
        .timer_inited = false,
        .timer_num = LEDC_TIMER_0,
        .gpio_num = LED_IO_NUM_0,
        .channel = LEDC_CHANNEL_0,
    };

    led_indicator_config_t config = {
        .mode = LED_LEDC_MODE,
        .led_indicator_ledc_config = &led_indicator_ledc_config,
        .blink_lists = led_blink_lst,
        .blink_list_num = BLINK_NUM,
    };
    int cnt0 = 3;
    while (cnt0--) {
        led_handle_0 = led_indicator_create(&config);
        TEST_ASSERT_NOT_NULL(led_handle_0);

        ESP_LOGI(TAG, "breathe blink .....");
        esp_err_t ret = led_indicator_start(led_handle_0, BLINK_BREATHE);
        TEST_ASSERT(ret == ESP_OK);
        bool preempted = false;
        int cnt = 3;
        while (cnt--) {
            if (preempted == false) {
                ESP_LOGI(TAG, "preempt blink.....");
                esp_err_t ret = led_indicator_preempt_start(led_handle_0, BLINK_50_BRIGHTNESS);
                TEST_ASSERT(ret == ESP_OK);
                preempted = true;
            } else {
                ESP_LOGI(TAG, "preempt blink stop.....");
                esp_err_t ret = led_indicator_preempt_stop(led_handle_0, BLINK_50_BRIGHTNESS);
                TEST_ASSERT(ret == ESP_OK);
                preempted = false;
            }
            vTaskDelay(3000 / portTICK_PERIOD_MS);
        }
        led_indicator_delete(led_handle_0);

    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

/*********************************************** LED STRIPS *******************************************************/

typedef enum {
    BLINK_RGB_25_BRIGHTNESS,
    BLINK_RGB_50_BRIGHTNESS,
    BLINK_RGB_75_BRIGHTNESS,
    BLINK_RGB_BREATHE,
    BLINK_RGB_RED,
    BLINK_RGB_GREEN,
    BLINK_RGB_BLUE,
    BLINK_RGB_RING_RED_TO_BLUE,
    BLINK_HSV_RED,
    BLINK_HSV_GREEN,
    BLINK_HSV_BLUE,
    BLINK_HSV_RING_RED_TO_BLUE,
    BLINK_RGB_FLASH,
    BLINK_RGB_DOUBLE,
    BLINK_RGB_TRIPLE,
    BLINK_RGB_NUM,
} led_blink_rgb_type_t;

static const blink_step_t rgb_double_blink[] = {
    {LED_BLINK_HOLD,  LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t rgb_triple_blink[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t rgb_breathe_blink[] = {
    {LED_BLINK_BREATHE, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 1000},
    {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 500},
    {LED_BLINK_BREATHE, INSERT_INDEX(MAX_INDEX, LED_STATE_ON), 1000},
    {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_ON), 500},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t rgb_brightness_25_blink[] = {
    {LED_BLINK_BRIGHTNESS, LED_STATE_25_PERCENT, 1000},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t rgb_brightness_50_blink[] = {
    {LED_BLINK_BRIGHTNESS, LED_STATE_50_PERCENT, 1000},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t rgb_brightness_75_blink[] = {
    {LED_BLINK_BRIGHTNESS, LED_STATE_75_PERCENT, 1000},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t rgb_red_blink[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0xFF, 0, 0), 2000},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t rgb_green_blink[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0xFF, 0), 2000},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t rgb_blue_blink[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0xFF), 2000},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t rgb_ring_red_to_blue_blink[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0xFF, 0, 0), 0},
    {LED_BLINK_RGB_RING, SET_IRGB(MAX_INDEX, 0, 0, 0xFF), 4000},
    {LED_BLINK_RGB_RING, SET_IRGB(MAX_INDEX, 0xFF, 0, 0), 4000},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t rgb_flash_blink[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0xFF, 0, 0), 200},
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0xFF, 0xFF, 0), 200},
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0xFF, 0), 200},
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0xFF, 0xFF), 200},
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0xFF), 200},
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0xFF, 0, 0xFF), 200},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t hsv_red_blink[] = {
    {LED_BLINK_HSV, SET_IHSV(MAX_INDEX, 0, 255, 255), 0},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t hsv_green_blink[] = {
    {LED_BLINK_HSV, SET_IHSV(MAX_INDEX, 120, 255, 255), 0},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t hsv_blue_blink[] = {
    {LED_BLINK_HSV, SET_IHSV(MAX_INDEX, 240, 255, 255), 0},
    {LED_BLINK_STOP, 0, 0},
};

static blink_step_t const *led_rgb_blink_lst[] = {
    [BLINK_RGB_25_BRIGHTNESS] = rgb_brightness_25_blink,
    [BLINK_RGB_50_BRIGHTNESS] = rgb_brightness_50_blink,
    [BLINK_RGB_75_BRIGHTNESS] = rgb_brightness_75_blink,
    [BLINK_RGB_BREATHE] = rgb_breathe_blink,
    [BLINK_RGB_RED] = rgb_red_blink,
    [BLINK_RGB_GREEN] = rgb_green_blink,
    [BLINK_RGB_BLUE] = rgb_blue_blink,
    [BLINK_RGB_RING_RED_TO_BLUE] = rgb_ring_red_to_blue_blink,
    [BLINK_HSV_RED] = hsv_red_blink,
    [BLINK_HSV_GREEN] = hsv_green_blink,
    [BLINK_HSV_BLUE] = hsv_blue_blink,
    [BLINK_HSV_RING_RED_TO_BLUE] = rgb_ring_red_to_blue_blink,
    [BLINK_RGB_FLASH] = rgb_flash_blink,
    [BLINK_RGB_DOUBLE] = rgb_double_blink,
    [BLINK_RGB_TRIPLE] = rgb_triple_blink,
    [BLINK_RGB_NUM] = NULL,
};

#define LED_RGB_RED_GPIO 11
#define LED_RGB_GREEN_GPIO 12
#define LED_RGB_BLUE_GPIO 13

TEST_CASE("TEST LED RGB", "[LED RGB][RGB]")
{
    led_indicator_rgb_config_t led_grb_cfg = {
        .is_active_level_high = 1,
        .timer_inited = false,
        .timer_num = LEDC_TIMER_0,
        .red_gpio_num = LED_RGB_RED_GPIO,
        .green_gpio_num = LED_RGB_GREEN_GPIO,
        .blue_gpio_num = LED_RGB_BLUE_GPIO,
        .red_channel = LEDC_CHANNEL_0,
        .green_channel = LEDC_CHANNEL_1,
        .blue_channel = LEDC_CHANNEL_2,
    };

    led_indicator_config_t config = {
        .mode = LED_RGB_MODE,
        .led_indicator_rgb_config = &led_grb_cfg,
        .blink_lists = led_rgb_blink_lst,
        .blink_list_num = BLINK_RGB_NUM,
    };

    led_handle_0 = led_indicator_create(&config);
    TEST_ASSERT_NOT_NULL(led_handle_0);
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "breathe 25/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_25_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe 50/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_50_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe 75/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_75_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "red blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_RED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "breathe blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_BREATHE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(8000 / portTICK_PERIOD_MS);
    led_indicator_stop(led_handle_0, BLINK_RGB_BREATHE);

    ESP_LOGI(TAG, "green blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_GREEN);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "blue blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_BLUE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "ring red to blue blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_RING_RED_TO_BLUE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(12000 / portTICK_PERIOD_MS);
    led_indicator_stop(led_handle_0, BLINK_RGB_RING_RED_TO_BLUE);

    ESP_LOGI(TAG, "hsv red blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_HSV_RED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "hsv green blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_HSV_GREEN);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "hsv blue blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_HSV_BLUE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "hsv ring red to blue blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_HSV_RING_RED_TO_BLUE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(12000 / portTICK_PERIOD_MS);
    led_indicator_stop(led_handle_0, BLINK_HSV_RING_RED_TO_BLUE);

    ESP_LOGI(TAG, "flash blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_FLASH);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(8000 / portTICK_PERIOD_MS);
    led_indicator_stop(led_handle_0, BLINK_RGB_FLASH);

    ESP_LOGI(TAG, "double blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_DOUBLE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "triple blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_TRIPLE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(6000 / portTICK_PERIOD_MS);

    led_indicator_deinit();
}

/* esp32c2 not support rmt */
#if !CONFIG_IDF_TARGET_ESP32C2

TEST_CASE("TEST LED Strips by RGB", "[LED Strips][RGB]")
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = MAX_LED_NUM,                  // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
#endif
    };

    led_indicator_strips_config_t led_indicator_strips_config = {
        .led_strip_cfg = strip_config,
        .led_strip_driver = LED_STRIP_RMT,
        .led_strip_rmt_cfg = rmt_config,
    };

    led_indicator_config_t config = {
        .mode = LED_STRIPS_MODE,
        .led_indicator_strips_config = &led_indicator_strips_config,
        .blink_lists = led_rgb_blink_lst,
        .blink_list_num = BLINK_RGB_NUM,
    };

    led_handle_0 = led_indicator_create(&config);
    TEST_ASSERT_NOT_NULL(led_handle_0);
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "breathe 25/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_25_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe 50/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_50_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe 75/100 blink.....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_75_BRIGHTNESS);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "breathe blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_BREATHE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(8000 / portTICK_PERIOD_MS);
    led_indicator_stop(led_handle_0, BLINK_RGB_BREATHE);

    ESP_LOGI(TAG, "red blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_RED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "green blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_GREEN);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "blue blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_BLUE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "ring red to blue blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_RING_RED_TO_BLUE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(12000 / portTICK_PERIOD_MS);
    led_indicator_stop(led_handle_0, BLINK_RGB_RING_RED_TO_BLUE);

    ESP_LOGI(TAG, "hsv red blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_HSV_RED);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "hsv green blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_HSV_GREEN);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "hsv blue blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_HSV_BLUE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "hsv ring red to blue blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_HSV_RING_RED_TO_BLUE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(12000 / portTICK_PERIOD_MS);
    led_indicator_stop(led_handle_0, BLINK_HSV_RING_RED_TO_BLUE);

    ESP_LOGI(TAG, "flash blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_FLASH);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(8000 / portTICK_PERIOD_MS);
    led_indicator_stop(led_handle_0, BLINK_RGB_FLASH);

    ESP_LOGI(TAG, "double blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_DOUBLE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "triple blink .....");
    ret = led_indicator_start(led_handle_0, BLINK_RGB_TRIPLE);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(6000 / portTICK_PERIOD_MS);

    led_indicator_deinit();
}

TEST_CASE("TEST LED RGB control Real time ", "[LED RGB][Real time]")
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = MAX_LED_NUM,                  // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
#endif
    };

    led_indicator_strips_config_t led_indicator_strips_config = {
        .led_strip_cfg = strip_config,
        .led_strip_driver = LED_STRIP_RMT,
        .led_strip_rmt_cfg = rmt_config,
    };

    led_indicator_config_t config = {
        .mode = LED_STRIPS_MODE,
        .led_indicator_strips_config = &led_indicator_strips_config,
        .blink_lists = led_rgb_blink_lst,
        .blink_list_num = BLINK_RGB_NUM,
    };

    led_handle_0 = led_indicator_create(&config);

    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "set red by rgb_value one by one .....");

    for (int i = 0; i < MAX_LED_NUM; i++) {
        ret = led_indicator_set_rgb(led_handle_0, SET_IRGB(i, 0xFF, 0, 0));
        TEST_ASSERT(ret == ESP_OK);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "set red by rgb_value .....");
    ret = led_indicator_set_rgb(led_handle_0, SET_IRGB(MAX_INDEX, 0xFF, 0, 0));
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "set green by hsv_value .....");
    ret = led_indicator_set_hsv(led_handle_0, SET_IHSV(MAX_INDEX, 240, 255, 255));
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "set brightness by hsv_value .....");
    ret = led_indicator_set_brightness(led_handle_0, INSERT_INDEX(MAX_INDEX, LED_STATE_50_PERCENT));
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    led_indicator_deinit();
}

#endif

static size_t before_free_8bit;
static size_t before_free_32bit;

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
    //   _    ___ ___    ___ _  _ ___ ___ ___   _ _____ ___  ___   _____ ___ ___ _____
    //  | |  | __|   \  |_ _| \| |   \_ _/ __| /_\_   _/ _ \| _ \ |_   _| __/ __|_   _|
    //  | |__| _|| |) |  | || .` | |) | | (__ / _ \| || (_) |   /   | | | _|\__ \ | |
    //  |____|___|___/  |___|_|\_|___/___\___/_/ \_\_| \___/|_|_\   |_| |___|___/ |_|
    printf("  _    ___ ___    ___ _  _ ___ ___ ___   _ _____ ___  ___   _____ ___ ___ _____\n");
    printf(" | |  | __|   \\  |_ _| \\| |   \\_ _/ __| /_\\_   _/ _ \\| _ \\ |_   _| __/ __|_   _|\n");
    printf(" | |__| _|| |) |  | || .` | |) | | (__ / _ \\| || (_) |   /   | | | _|\\__ \\ | |\n");
    printf(" |____|___|___/  |___|_|\\_|___/___\\___/_/ \\_\\_| \\___/|_|_\\   |_| |___|___/ |_|\n");
    unity_run_menu();
}
