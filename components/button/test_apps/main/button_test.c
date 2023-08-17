/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_adc/adc_cali.h"
#endif
#include "unity.h"
#include "iot_button.h"
#include "sdkconfig.h"

static const char *TAG = "BUTTON TEST";

#define TEST_MEMORY_LEAK_THRESHOLD (-400)
#define BUTTON_IO_NUM  0
#define BUTTON_ACTIVE_LEVEL   0
#define BUTTON_NUM 16

static size_t before_free_8bit;
static size_t before_free_32bit;
static button_handle_t g_btns[BUTTON_NUM] = {0};

static int get_btn_index(button_handle_t btn)
{
    for (size_t i = 0; i < BUTTON_NUM; i++) {
        if (btn == g_btns[i]) {
            return i;
        }
    }
    return -1;
}

static void button_press_down_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_PRESS_DOWN, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_PRESS_DOWN", get_btn_index((button_handle_t)arg));
}

static void button_press_up_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_PRESS_UP, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_PRESS_UP[%d]", get_btn_index((button_handle_t)arg), iot_button_get_ticks_time((button_handle_t)arg));
}

static void button_press_repeat_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "BTN%d: BUTTON_PRESS_REPEAT[%d]", get_btn_index((button_handle_t)arg), iot_button_get_repeat((button_handle_t)arg));
}

static void button_single_click_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_SINGLE_CLICK, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_SINGLE_CLICK", get_btn_index((button_handle_t)arg));
}

static void button_double_click_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_DOUBLE_CLICK, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_DOUBLE_CLICK", get_btn_index((button_handle_t)arg));
}

static void button_long_press_start_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_LONG_PRESS_START, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_LONG_PRESS_START", get_btn_index((button_handle_t)arg));
}

static void button_long_press_hold_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_LONG_PRESS_HOLD, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_LONG_PRESS_HOLD[%d],count is [%d]", get_btn_index((button_handle_t)arg), iot_button_get_ticks_time((button_handle_t)arg), iot_button_get_long_press_hold_cnt((button_handle_t)arg));
}

static void button_press_repeat_done_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_PRESS_REPEAT_DONE, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_PRESS_REPEAT_DONE[%d]", get_btn_index((button_handle_t)arg), iot_button_get_repeat((button_handle_t)arg));
}

static esp_err_t custom_button_gpio_init(void *param)
{
    button_gpio_config_t *cfg = (button_gpio_config_t *)param;

    return button_gpio_init(cfg);
}

static uint8_t custom_button_gpio_get_key_value(void *param)
{
    button_gpio_config_t *cfg = (button_gpio_config_t *)param;

    return button_gpio_get_key_level((void *)cfg->gpio_num);
}

static esp_err_t custom_button_gpio_deinit(void *param)
{
    button_gpio_config_t *cfg = (button_gpio_config_t *)param;

    return button_gpio_deinit(cfg->gpio_num);
}

TEST_CASE("custom button test", "[button][iot]")
{
    button_gpio_config_t *gpio_cfg = calloc(1, sizeof(button_gpio_config_t));
    gpio_cfg->active_level = 0;
    gpio_cfg->gpio_num = 0;

    button_config_t cfg = {
        .type = BUTTON_TYPE_CUSTOM,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .custom_button_config = {
            .button_custom_init = custom_button_gpio_init,
            .button_custom_deinit = custom_button_gpio_deinit,
            .button_custom_get_key_value = custom_button_gpio_get_key_value,
            .active_level = 0,
            .priv = gpio_cfg,
        },
    };

    g_btns[0] = iot_button_create(&cfg);
    TEST_ASSERT_NOT_NULL(g_btns[0]);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_DOWN, button_press_down_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_UP, button_press_up_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_REPEAT, button_press_repeat_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_SINGLE_CLICK, button_single_click_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_DOUBLE_CLICK, button_double_click_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_LONG_PRESS_START, button_long_press_start_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_LONG_PRESS_HOLD, button_long_press_hold_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_REPEAT_DONE, button_press_repeat_done_cb, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    iot_button_delete(g_btns[0]);
}

TEST_CASE("gpio button test", "[button][iot]")
{
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = 0,
            .active_level = 0,
        },
    };
    g_btns[0] = iot_button_create(&cfg);
    TEST_ASSERT_NOT_NULL(g_btns[0]);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_DOWN, button_press_down_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_UP, button_press_up_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_REPEAT, button_press_repeat_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_SINGLE_CLICK, button_single_click_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_DOUBLE_CLICK, button_double_click_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_LONG_PRESS_START, button_long_press_start_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_LONG_PRESS_HOLD, button_long_press_hold_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_REPEAT_DONE, button_press_repeat_done_cb, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    iot_button_delete(g_btns[0]);
}

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
TEST_CASE("adc button test", "[button][iot]")
{
    /** ESP32-S3-Korvo board */
    const uint16_t vol[6] = {380, 820, 1180, 1570, 1980, 2410};
    button_config_t cfg = {0};
    cfg.type = BUTTON_TYPE_ADC;
    cfg.long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS;
    cfg.short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS;
    for (size_t i = 0; i < 6; i++) {
        cfg.adc_button_config.adc_channel = 7,
        cfg.adc_button_config.button_index = i;
        if (i == 0) {
            cfg.adc_button_config.min = (0 + vol[i]) / 2;
        } else {
            cfg.adc_button_config.min = (vol[i - 1] + vol[i]) / 2;
        }

        if (i == 5) {
            cfg.adc_button_config.max = (vol[i] + 3000) / 2;
        } else {
            cfg.adc_button_config.max = (vol[i] + vol[i + 1]) / 2;
        }

        g_btns[i] = iot_button_create(&cfg);
        TEST_ASSERT_NOT_NULL(g_btns[i]);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_DOWN, button_press_down_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_UP, button_press_up_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_REPEAT, button_press_repeat_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_SINGLE_CLICK, button_single_click_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_DOUBLE_CLICK, button_double_click_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_LONG_PRESS_START, button_long_press_start_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_LONG_PRESS_HOLD, button_long_press_hold_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_REPEAT_DONE, button_press_repeat_done_cb, NULL);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    for (size_t i = 0; i < 6; i++) {
        iot_button_delete(g_btns[i]);
    }
}
#else
static esp_err_t adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BUTTON_WIDTH,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BUTTON_WIDTH,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated ? ESP_OK : ESP_FAIL;
}

TEST_CASE("adc button idf5 drive test", "[button][iot]")
{
    adc_oneshot_unit_handle_t adc1_handle;
    adc_cali_handle_t adc1_cali_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc1_handle);
    TEST_ASSERT_TRUE(ret == ESP_OK);
    adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_11, &adc1_cali_handle);

    /** ESP32-S3-Korvo board */
    const uint16_t vol[6] = {380, 820, 1180, 1570, 1980, 2410};
    button_config_t cfg = {0};
    cfg.type = BUTTON_TYPE_ADC;
    cfg.long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS;
    cfg.short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS;
    for (size_t i = 0; i < 6; i++) {
        cfg.adc_button_config.adc_handle = &adc1_handle;
        cfg.adc_button_config.adc_channel = 7,
        cfg.adc_button_config.button_index = i;
        if (i == 0) {
            cfg.adc_button_config.min = (0 + vol[i]) / 2;
        } else {
            cfg.adc_button_config.min = (vol[i - 1] + vol[i]) / 2;
        }

        if (i == 5) {
            cfg.adc_button_config.max = (vol[i] + 3000) / 2;
        } else {
            cfg.adc_button_config.max = (vol[i] + vol[i + 1]) / 2;
        }

        g_btns[i] = iot_button_create(&cfg);
        TEST_ASSERT_NOT_NULL(g_btns[i]);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_DOWN, button_press_down_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_UP, button_press_up_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_REPEAT, button_press_repeat_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_SINGLE_CLICK, button_single_click_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_DOUBLE_CLICK, button_double_click_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_LONG_PRESS_START, button_long_press_start_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_LONG_PRESS_HOLD, button_long_press_hold_cb, NULL);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_REPEAT_DONE, button_press_repeat_done_cb, NULL);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    for (size_t i = 0; i < 6; i++) {
        iot_button_delete(g_btns[i]);
    }
}
#endif

#define GPIO_OUTPUT_IO_45 45
static SemaphoreHandle_t g_check = NULL;
static SemaphoreHandle_t g_auto_check_pass = NULL;
static const char* button_event_str[BUTTON_EVENT_MAX] = {
    "BUTTON_PRESS_DOWN",
    "BUTTON_PRESS_UP",
    "BUTTON_PRESS_REPEAT",
    "BUTTON_PRESS_REPEAT_DONE",
    "BUTTON_SINGLE_CLICK",
    "BUTTON_DOUBLE_CLICK",
    "BUTTON_LONG_PRESS_START",
    "BUTTON_LONG_PRESS_HOLD",
};

static void button_auto_press_test_task(void *arg)
{
    // test BUTTON_PRESS_DOWN
    xSemaphoreTake(g_check, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    // // test BUTTON_PRESS_UP
    xSemaphoreTake(g_check, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    // test BUTTON_PRESS_REPEAT
    xSemaphoreTake(g_check, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    // test BUTTON_PRESS_REPEAT_DONE
    xSemaphoreTake(g_check, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // test BUTTON_SINGLE_CLICK
    xSemaphoreTake(g_check, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    // test BUTTON_DOUBLE_CLICK
    xSemaphoreTake(g_check, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    // test BUTTON_LONG_PRESS_START
    xSemaphoreTake(g_check, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // test BUTTON_LONG_PRESS_HOLD
    xSemaphoreTake(g_check, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);

    xSemaphoreTake(g_check, portMAX_DELAY);
    ESP_LOGI(TAG, "Auto Press Success!");
    vTaskDelete(NULL);
}

static button_event_t state = BUTTON_PRESS_DOWN;

static void button_auto_check_cb(void *arg, void *data)
{
    if (iot_button_get_event(g_btns[0]) == state) {
        ESP_LOGI(TAG, "Auto check: button event %s pass", button_event_str[state]);
        if (++state >= BUTTON_EVENT_MAX) {
            xSemaphoreGive(g_check);
            xSemaphoreGive(g_auto_check_pass);
            return;
        }
        xSemaphoreGive(g_check);
    }
}

TEST_CASE("gpio button auto-test", "[button][iot][auto]")
{
    state = BUTTON_PRESS_DOWN;
    g_check = xSemaphoreCreateBinary();
    g_auto_check_pass = xSemaphoreCreateBinary();
    xSemaphoreGive(g_check);
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = 0,
            .active_level = 0,
        },
    };
    g_btns[0] = iot_button_create(&cfg);
    TEST_ASSERT_NOT_NULL(g_btns[0]);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_DOWN, button_auto_check_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_UP, button_auto_check_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_REPEAT, button_auto_check_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_SINGLE_CLICK, button_auto_check_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_DOUBLE_CLICK, button_auto_check_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_LONG_PRESS_START, button_auto_check_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_LONG_PRESS_HOLD, button_auto_check_cb, NULL);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_REPEAT_DONE, button_auto_check_cb, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, iot_button_set_param(g_btns[0], BUTTON_LONG_PRESS_TIME_MS, (void *)2000));

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_OUTPUT_IO_45),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);

    xTaskCreate(button_auto_press_test_task, "button_auto_press_test_task", 1024 * 4, NULL, 10, NULL);

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(g_auto_check_pass, pdMS_TO_TICKS(5000)));
    TEST_ASSERT_EQUAL(ESP_OK,iot_button_delete(g_btns[0]));
    vSemaphoreDelete(g_check);
    vSemaphoreDelete(g_auto_check_pass);
    vTaskDelay(pdMS_TO_TICKS(100));
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
    printf("USB STREAM TEST \n");
    unity_run_menu();
}
