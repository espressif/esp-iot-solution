/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "unity.h"
#include "string.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "max17048.h"

#define I2C_MASTER_SCL_IO          (gpio_num_t)4         /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO          (gpio_num_t)5         /*!< gpio number for I2C master data  */
#define MAX17048_ALERT_IO          (gpio_num_t)3         /*!< gpio number for MAX17048 alert pin */

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

static i2c_bus_handle_t i2c_bus = NULL;
static max17048_handle_t max17048 = NULL;
static const char* TAG = "max17048_test";
SemaphoreHandle_t interrupt_semaphore;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    xSemaphoreGiveFromISR(interrupt_semaphore, NULL);
}

static void max17048_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400 * 1000,
    };
    i2c_bus = i2c_bus_create(I2C_NUM_0, &conf);
    max17048 = max17048_create(i2c_bus, MAX17048_I2C_ADDR_DEFAULT);
}

static void max17048_deinit()
{
    max17048_delete(&max17048);
    i2c_bus_delete(&i2c_bus);
}

TEST_CASE("max17048 basic information query test", "[voltage][percent][charge_rate]")
{
    max17048_init();
    float voltage = 0, percent = 0, charge_rate = 0;
    TEST_ASSERT(max17048_get_cell_voltage(max17048, &voltage) == ESP_OK);
    TEST_ASSERT(max17048_get_cell_percent(max17048, &percent) == ESP_OK);
    TEST_ASSERT(max17048_get_charge_rate(max17048, &charge_rate) == ESP_OK);
    ESP_LOGI(TAG, "Voltage:%.2fV, percent:%.2f%%, charge_rate:%.2f%%", voltage, percent, charge_rate);
    max17048_deinit();
}

TEST_CASE("max17048 sotft reset test", "[reset]")
{
    max17048_init();
    TEST_ASSERT(max17048_reset(max17048) == ESP_OK);
    max17048_deinit();
}

TEST_CASE("max17048 voltage alert threshold test", "[voltage][alert]")
{
    max17048_init();
    float voltage = 0, alert_min = 0, alert_max = 0;
    TEST_ASSERT(max17048_get_cell_voltage(max17048, &voltage) == ESP_OK);
    ESP_LOGI(TAG, "The set VLART.min and VLART.max are respectively: %.2fV and %.2fV", voltage - 0.2, voltage + 0.2);
    TEST_ASSERT(max17048_set_alert_volatge(max17048, voltage - 0.2, voltage + 0.2) == ESP_OK);
    TEST_ASSERT(max17048_get_alert_voltage(max17048, &alert_min, &alert_max) == ESP_OK);
    ESP_LOGI(TAG, "The VLART.min and VLART.max reads are %.2fV and %.2fV respectively.", alert_min, alert_max);
    max17048_deinit();
}

TEST_CASE("max17048 hibernation mode test", "[hibernation]")
{
    max17048_init();
    bool is_hibernation = false;
    TEST_ASSERT(max17048_enter_hibernation_mode(max17048) == ESP_OK);
    TEST_ASSERT(max17048_is_hibernate(max17048, &is_hibernation) == ESP_OK);
    ESP_LOGI(TAG, "Hibernation_status:%d", is_hibernation);
    max17048_deinit();
}

TEST_CASE("max17048 reset voltage test", "[reset_voltage]")
{
    max17048_init();
    float reset_voltage = 0;
    TEST_ASSERT(max17048_set_reset_voltage(max17048, 2.5f) == ESP_OK);
    TEST_ASSERT(max17048_get_reset_voltage(max17048, &reset_voltage) == ESP_OK);
    ESP_LOGI(TAG, "Reset_voltage:%.2fV", reset_voltage);

    bool is_alert = false;
    TEST_ASSERT(max17048_set_reset_voltage_alert_enabled(max17048, true) == ESP_OK);
    TEST_ASSERT(max17048_is_reset_voltage_alert_enabled(max17048, &is_alert) == ESP_OK);
    ESP_LOGI(TAG, "First reset_voltage_alert_status:%d", is_alert);
    TEST_ASSERT(max17048_set_reset_voltage_alert_enabled(max17048, false) == ESP_OK);
    TEST_ASSERT(max17048_is_reset_voltage_alert_enabled(max17048, &is_alert) == ESP_OK);
    ESP_LOGI(TAG, "Second reset_voltage_alert_status:%d", is_alert);
    max17048_deinit();
}

TEST_CASE("max17048 sleep mode test", "[sleep]")
{
    max17048_init();
    bool is_sleep = false;
    TEST_ASSERT(max17048_set_sleep_enabled(max17048, true) == ESP_OK);
    TEST_ASSERT(max17048_set_sleep(max17048, true) == ESP_OK);
    TEST_ASSERT(max17048_is_sleeping(max17048, &is_sleep) == ESP_OK);
    ESP_LOGI(TAG, "First sleep_status:%d", is_sleep);
    TEST_ASSERT(max17048_set_sleep(max17048, false) == ESP_OK);
    TEST_ASSERT(max17048_is_sleeping(max17048, &is_sleep) == ESP_OK);
    ESP_LOGI(TAG, "Second sleep_status:%d", is_sleep);
    max17048_deinit();
}

TEST_CASE("max17048 alert test", "[alert]")
{
    float voltage = 0;
    interrupt_semaphore = xSemaphoreCreateBinary();
    TEST_ASSERT(interrupt_semaphore != NULL);

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << MAX17048_ALERT_IO),
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(MAX17048_ALERT_IO, gpio_isr_handler, NULL);

    max17048_init();
    TEST_ASSERT(max17048_reset(max17048) == ESP_OK);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Set the alert voltage to trigger an alert
    TEST_ASSERT(max17048_get_cell_voltage(max17048, &voltage) == ESP_OK);
    TEST_ASSERT(max17048_set_alert_volatge(max17048, voltage - 0.5, voltage - 0.3) == ESP_OK);
    ESP_LOGI(TAG, "Current voltage:%.2fV, alert_min:%.2fV, alert_max:%.2fV", voltage, voltage - 0.5, voltage - 0.3);

    if (xSemaphoreTake(interrupt_semaphore, pdMS_TO_TICKS(1000)) != pdTRUE) {
        TEST_FAIL_MESSAGE("Interrupt was not triggered within the timeout period.");
    }

    // Clear alert
    TEST_ASSERT(max17048_set_alert_volatge(max17048, voltage - 0.3, voltage + 0.3) == ESP_OK);
    max17048_clear_alert_status_bit(max17048);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    TEST_ASSERT(gpio_get_level(MAX17048_ALERT_IO) == 1);

    gpio_isr_handler_remove(MAX17048_ALERT_IO);
    gpio_uninstall_isr_service();
    if (interrupt_semaphore != NULL) {
        vSemaphoreDelete(interrupt_semaphore);
    }
    max17048_deinit();
}

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
    printf("MAX17048/MAX17049 TEST \n");
    unity_run_menu();
}
