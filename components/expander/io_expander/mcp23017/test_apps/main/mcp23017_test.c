/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#include "esp_io_expander_mcp23017.h"
#include "esp_io_expander_gpio_wrapper.h"
#include "mcp23017.h"

#define I2C_MASTER_SCL_IO       1
#define I2C_MASTER_SDA_IO       2
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_ADDRESS             ESP_IO_EXPANDER_I2C_MCP23017_ADDRESS_000

#define TEST_PIN_COUNT          8
#define TEST_LOOP_DELAY_MS      300
#define TEST_OUTPUT_PINS        (IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2 | \
                                 IO_EXPANDER_PIN_NUM_3 | IO_EXPANDER_PIN_NUM_4 | IO_EXPANDER_PIN_NUM_5 | \
                                 IO_EXPANDER_PIN_NUM_6 | IO_EXPANDER_PIN_NUM_7)
#define TEST_INPUT_PINS         (IO_EXPANDER_PIN_NUM_8 | IO_EXPANDER_PIN_NUM_9 | IO_EXPANDER_PIN_NUM_10 | \
                                 IO_EXPANDER_PIN_NUM_11 | IO_EXPANDER_PIN_NUM_12 | IO_EXPANDER_PIN_NUM_13 | \
                                 IO_EXPANDER_PIN_NUM_14 | IO_EXPANDER_PIN_NUM_15)
#define TEST_PORTA_SHIFT        0
#define TEST_PORTB_SHIFT        8

#define TEST_MEMORY_LEAK_THRESHOLD  (500)

static const char *TAG = "mcp23017 test";
static i2c_master_bus_handle_t i2c_bus = NULL;
static esp_io_expander_handle_t io_expander = NULL;

/* Drive `pattern` on the output port and verify it is read back on the input
 * port. `out_shift`/`in_shift` place the 8-bit pattern onto the port's pins. */
static void check_pattern(uint32_t output_pins, uint32_t input_pins,
                          int out_shift, int in_shift, uint8_t pattern)
{
    uint32_t input_level_mask = 0;
    uint32_t output_mask = (uint32_t)pattern << out_shift;
    uint32_t expected_input_mask = (uint32_t)pattern << in_shift;

    TEST_ESP_OK(esp_io_expander_set_level(io_expander, output_pins, 0));
    if (output_mask != 0) {
        TEST_ESP_OK(esp_io_expander_set_level(io_expander, output_mask, 1));
    }
    vTaskDelay(pdMS_TO_TICKS(TEST_LOOP_DELAY_MS));

    TEST_ESP_OK(esp_io_expander_get_level(io_expander, input_pins, &input_level_mask));
    ESP_LOGI(TAG, "out 0x%04" PRIx32 " -> in 0x%04" PRIx32 " (expected 0x%04" PRIx32 ")",
             output_mask, input_level_mask, expected_input_mask);
    TEST_ASSERT_EQUAL_HEX32(expected_input_mask, input_level_mask);
}

/* Sweep the full pattern set (all-low, all-high, walking one/zero) from the
 * output port to the input port. Ports must already be configured. */
static void run_pattern_sweep(uint32_t output_pins, uint32_t input_pins, int out_shift, int in_shift)
{
    check_pattern(output_pins, input_pins, out_shift, in_shift, 0x00);
    check_pattern(output_pins, input_pins, out_shift, in_shift, 0xFF);
    for (int pin = 0; pin < TEST_PIN_COUNT; pin++) {
        check_pattern(output_pins, input_pins, out_shift, in_shift, BIT(pin));
        check_pattern(output_pins, input_pins, out_shift, in_shift, 0xFF & ~BIT(pin));
    }
}

static void i2c_bus_init(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    TEST_ESP_OK(i2c_new_master_bus(&bus_config, &i2c_bus));
}

static void i2c_bus_deinit(void)
{
    TEST_ESP_OK(i2c_del_master_bus(i2c_bus));
    i2c_bus = NULL;
}

static void mcp23017_test_init(void)
{
    TEST_ESP_OK(esp_io_expander_new_i2c_mcp23017(i2c_bus, I2C_ADDRESS, &io_expander));
    TEST_ASSERT_NOT_NULL(io_expander);
    TEST_ESP_OK(mcp23017_check_present(io_expander));
}

static void mcp23017_test_deinit(void)
{
    TEST_ESP_OK(esp_io_expander_del(io_expander));
    io_expander = NULL;
}

TEST_CASE("IO expander MCP23017 test, connect A-B port together", "[mcp23017][iot][device]")
{
    i2c_bus_init();
    mcp23017_test_init();

    TEST_ESP_OK(esp_io_expander_set_dir(io_expander, TEST_OUTPUT_PINS, IO_EXPANDER_INPUT));
    TEST_ESP_OK(esp_io_expander_set_dir(io_expander, TEST_INPUT_PINS, IO_EXPANDER_INPUT));
    TEST_ESP_OK(esp_io_expander_set_pullupdown(io_expander, TEST_INPUT_PINS, IO_EXPANDER_PULL_UP));
    vTaskDelay(pdMS_TO_TICKS(TEST_LOOP_DELAY_MS));

    uint32_t input_level_mask = 0;
    TEST_ESP_OK(esp_io_expander_get_level(io_expander, TEST_INPUT_PINS, &input_level_mask));
    TEST_ASSERT_EQUAL_HEX32(TEST_INPUT_PINS, input_level_mask);

    TEST_ESP_ERR(ESP_ERR_NOT_SUPPORTED, esp_io_expander_set_pullupdown(io_expander, TEST_INPUT_PINS, IO_EXPANDER_PULL_DOWN));
    TEST_ESP_OK(esp_io_expander_set_pullupdown(io_expander, TEST_INPUT_PINS, IO_EXPANDER_PULL_NONE));

    // Port A output -> port B input
    ESP_LOGI(TAG, "Sweep GPIOA -> GPIOB");
    TEST_ESP_OK(esp_io_expander_set_dir(io_expander, TEST_OUTPUT_PINS, IO_EXPANDER_OUTPUT));
    TEST_ESP_OK(esp_io_expander_set_dir(io_expander, TEST_INPUT_PINS, IO_EXPANDER_INPUT));
    run_pattern_sweep(TEST_OUTPUT_PINS, TEST_INPUT_PINS, TEST_PORTA_SHIFT, TEST_PORTB_SHIFT);

    // Port B output -> port A input
    ESP_LOGI(TAG, "Sweep GPIOB -> GPIOA");
    TEST_ESP_OK(esp_io_expander_set_dir(io_expander, TEST_INPUT_PINS, IO_EXPANDER_OUTPUT));
    TEST_ESP_OK(esp_io_expander_set_dir(io_expander, TEST_OUTPUT_PINS, IO_EXPANDER_INPUT));
    run_pattern_sweep(TEST_INPUT_PINS, TEST_OUTPUT_PINS, TEST_PORTB_SHIFT, TEST_PORTA_SHIFT);

    mcp23017_test_deinit();
    i2c_bus_deinit();
    vTaskDelay(pdMS_TO_TICKS(10));
}

/* Configure `out_base..` as outputs and `in_base..` as inputs, then verify each
 * output pin drives its paired input pin high and low through the GPIO wrapper. */
static void wrapper_check_direction(const char *label, uint8_t out_base, uint8_t in_base)
{
    for (int pin = 0; pin < TEST_PIN_COUNT; pin++) {
        TEST_ESP_OK(gpio_set_direction(out_base + pin, GPIO_MODE_OUTPUT));
        TEST_ESP_OK(gpio_set_direction(in_base + pin, GPIO_MODE_INPUT));
        TEST_ESP_OK(gpio_set_level(out_base + pin, 0));
    }

    for (int pin = 0; pin < TEST_PIN_COUNT; pin++) {
        const uint8_t io_output = out_base + pin;
        const uint8_t io_input = in_base + pin;

        ESP_LOGI(TAG, "Test IO expander %s%d", label, pin);
        TEST_ESP_OK(gpio_set_level(io_output, 1));
        vTaskDelay(pdMS_TO_TICKS(TEST_LOOP_DELAY_MS));
        TEST_ASSERT_EQUAL_INT(1, gpio_get_level(io_input));

        TEST_ESP_OK(gpio_set_level(io_output, 0));
        vTaskDelay(pdMS_TO_TICKS(TEST_LOOP_DELAY_MS));
        TEST_ASSERT_EQUAL_INT(0, gpio_get_level(io_input));
    }
}

TEST_CASE("IO expander MCP23017 GPIO wrapper test, connect A-B port together", "[mcp23017][iot][device]")
{
    i2c_bus_init();
    mcp23017_test_init();

    TEST_ESP_OK(esp_io_expander_gpio_wrapper_append_handler(io_expander, GPIO_NUM_MAX));

    const uint8_t porta_base = GPIO_NUM_MAX;
    const uint8_t portb_base = GPIO_NUM_MAX + TEST_PIN_COUNT;

    wrapper_check_direction("GPIOA->GPIOB pin ", porta_base, portb_base);
    wrapper_check_direction("GPIOB->GPIOA pin ", portb_base, porta_base);

    TEST_ESP_OK(esp_io_expander_gpio_wrapper_remove_handler(io_expander));
    mcp23017_test_deinit();
    i2c_bus_deinit();
    vTaskDelay(pdMS_TO_TICKS(10));
}

void setUp(void)
{
    unity_utils_set_leak_level(TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    if (io_expander != NULL) {
        esp_io_expander_gpio_wrapper_remove_handler(io_expander);
        esp_io_expander_del(io_expander);
        io_expander = NULL;
    }
    if (i2c_bus != NULL) {
        i2c_del_master_bus(i2c_bus);
        i2c_bus = NULL;
    }
    unity_utils_evaluate_leaks();
}

void app_main(void)
{
    printf("MCP23017 TEST\n");
    unity_run_menu();
}
