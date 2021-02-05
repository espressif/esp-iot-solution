// Copyright 2020-2021 Espressif Systems (Shanghai) CO LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp_system.h"
#include "esp_log.h"
#include "led_indicator.h"
#include "unity.h"

#define LED_IO_NUM_0    23
#define LED_IO_NUM_1    18
#define LED_IO_NUM_2    5

#define TAG "led indicator"

static led_indicator_handle_t led_handle_0 = NULL;
static led_indicator_handle_t led_handle_1 = NULL;
static led_indicator_handle_t led_handle_2 = NULL;

void led_indicator_init()
{
    led_indicator_config_t config = {
        .off_level = 0,
        .mode = LED_GPIO_MODE,
    };

    led_handle_0 = iot_led_indicator_create(LED_IO_NUM_0, &config);
    TEST_ASSERT_NOT_NULL(led_handle_0);
    TEST_ASSERT(led_handle_0 == iot_led_indicator_get_handle(LED_IO_NUM_0)); //test get handle
}

void led_indicator_deinit()
{
    esp_err_t ret = iot_led_indicator_delete(&led_handle_0);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NULL(led_handle_0);
}

void led_indicator_gpio_mode_test_all()
{
    ESP_LOGI(TAG, "connecting.....");
    esp_err_t ret = iot_led_indicator_start(led_handle_0, connecting);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = iot_led_indicator_stop(led_handle_0, connecting);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "connected.....");
    ret = iot_led_indicator_start(led_handle_0, connected);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = iot_led_indicator_stop(led_handle_0, connected);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "reconnecting.....");
    ret = iot_led_indicator_start(led_handle_0, reconnecting);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = iot_led_indicator_stop(led_handle_0, reconnecting);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "updating.....");
    ret = iot_led_indicator_start(led_handle_0, updating);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = iot_led_indicator_stop(led_handle_0, updating);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "factory_reset.....");
    ret = iot_led_indicator_start(led_handle_0, factory_reset);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = iot_led_indicator_stop(led_handle_0, factory_reset);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "provisioning.....");
    ret = iot_led_indicator_start(led_handle_0, provisioning);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = iot_led_indicator_stop(led_handle_0, provisioning);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "provisioned.....");
    ret = iot_led_indicator_start(led_handle_0, provisioned);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = iot_led_indicator_stop(led_handle_0, provisioned);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "test all condition done.....");
}

TEST_CASE("blink test all in order", "[led][indicator]")
{
    led_indicator_init();
    led_indicator_gpio_mode_test_all();
    led_indicator_deinit();
}

void led_indicator_gpio_mode_preempt()
{
    ESP_LOGI(TAG, "connecting.....");
    esp_err_t ret = iot_led_indicator_start(led_handle_0, connecting);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(3000 / portTICK_RATE_MS);


    ESP_LOGI(TAG, "factory_reset.....");
    ret = iot_led_indicator_start(led_handle_0, factory_reset); //higer priority than connecting
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(3000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "factory_reset stop");
    ret = iot_led_indicator_stop(led_handle_0, factory_reset); //then switch to low priority
    TEST_ASSERT(ret == ESP_OK);
    ESP_LOGI(TAG, "connecting.....");
    vTaskDelay(3000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "connecting stop");
    ret = iot_led_indicator_stop(led_handle_0, connecting);
    TEST_ASSERT(ret == ESP_OK);
}

TEST_CASE("blink test with preempt", "[led][indicator]")
{
    led_indicator_init();
    led_indicator_gpio_mode_preempt();
    led_indicator_deinit();
}

void led_indicator_all_init()
{
    led_indicator_config_t config = {
        .off_level = 0,
        .mode = LED_GPIO_MODE,
    };

    led_handle_0 = iot_led_indicator_create(LED_IO_NUM_0, &config);
    TEST_ASSERT_NOT_NULL(led_handle_0);
    led_handle_1 = iot_led_indicator_create(LED_IO_NUM_1, &config);
    TEST_ASSERT_NOT_NULL(led_handle_1);
    led_handle_2 = iot_led_indicator_create(LED_IO_NUM_2, &config);
    TEST_ASSERT_NOT_NULL(led_handle_2);
    TEST_ASSERT(led_handle_0 == iot_led_indicator_get_handle(LED_IO_NUM_0)); //test get handle
    TEST_ASSERT(led_handle_1 == iot_led_indicator_get_handle(LED_IO_NUM_1)); //test get handle
    TEST_ASSERT(led_handle_2 == iot_led_indicator_get_handle(LED_IO_NUM_2)); //test get handle
}

void led_indicator_all_deinit()
{
    esp_err_t ret = iot_led_indicator_delete(&led_handle_0);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NULL(led_handle_0);
    ret = iot_led_indicator_delete(&led_handle_1);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NULL(led_handle_1);
    ret = iot_led_indicator_delete(&led_handle_2);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NULL(led_handle_2);
}

void led_indicator_gpio_mode_three_led()
{
    led_indicator_handle_t led_connect = led_handle_0;
    led_indicator_handle_t provision = led_handle_1;
    led_indicator_handle_t led_system = led_handle_2;

    ESP_LOGI(TAG, "provisioning.....");
    esp_err_t ret = iot_led_indicator_start(provision, provisioning);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "provisioned.....");
    ret = iot_led_indicator_stop(provision, provisioning);
    TEST_ASSERT(ret == ESP_OK);
    ret = iot_led_indicator_start(provision, provisioned);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "connecting.....");
    ret = iot_led_indicator_start(led_connect, connecting);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "connected.....");
    ret = iot_led_indicator_stop(led_connect, connecting);
    TEST_ASSERT(ret == ESP_OK);
    ret = iot_led_indicator_start(led_connect, connected);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "lost connection");
    ret = iot_led_indicator_stop(led_connect, connected);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "reconnecting.....");
    ret = iot_led_indicator_start(led_connect, reconnecting);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "reconnected.....");
    ret = iot_led_indicator_stop(led_connect, reconnecting);
    TEST_ASSERT(ret == ESP_OK);
    ret = iot_led_indicator_start(led_connect, connected);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "updating.....");
    ret = iot_led_indicator_start(led_system, updating);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = iot_led_indicator_stop(led_system, updating);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "factory_reset.....");
    ret = iot_led_indicator_start(led_system, factory_reset);
    TEST_ASSERT(ret == ESP_OK);
    vTaskDelay(4000 / portTICK_RATE_MS);
    ret = iot_led_indicator_stop(led_system, factory_reset);
    TEST_ASSERT(ret == ESP_OK);

    ESP_LOGI(TAG, "test all condition done.....");
}


TEST_CASE("blink three led", "[led][indicator]")
{
    led_indicator_all_init();
    led_indicator_gpio_mode_three_led();
    led_indicator_all_deinit();
}