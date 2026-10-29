/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "touch_ic_bs8112a3.h"

static const char *TAG = "touch_ic_bs8112a3_test";

// Test configuration
#define TEST_I2C_PORT           I2C_NUM_0
#define TEST_I2C_SCL_IO         3
#define TEST_I2C_SDA_IO         2
#define TEST_I2C_FREQ_HZ        400000
#define TEST_INTERRUPT_PIN      6  // Set to actual GPIO if interrupt testing is needed

static i2c_master_bus_handle_t s_test_i2c_bus_handle = NULL;

/**
 * @brief Initialize I2C bus for testing
 */
static void test_i2c_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = TEST_I2C_PORT,
        .sda_io_num = TEST_I2C_SDA_IO,
        .scl_io_num = TEST_I2C_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &s_test_i2c_bus_handle));
    ESP_LOGI(TAG, "I2C bus initialized for testing");
}

/**
 * @brief Deinitialize I2C bus
 */
static void test_i2c_deinit(void)
{
    if (s_test_i2c_bus_handle) {
        i2c_del_master_bus(s_test_i2c_bus_handle);
        s_test_i2c_bus_handle = NULL;
        ESP_LOGI(TAG, "I2C bus deinitialized");
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}

// ========== Boundary Tests ==========

TEST_CASE("touch_ic_bs8112a3_create_null_config", "[touch_ic_bs8112a3][boundary]")
{
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(NULL, &handle);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    TEST_ASSERT_NULL(handle);
}

TEST_CASE("touch_ic_bs8112a3_create_null_handle", "[touch_ic_bs8112a3][boundary]")
{
    bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = NULL,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg,
    };
    esp_err_t ret = touch_ic_bs8112a3_create(&config, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

TEST_CASE("touch_ic_bs8112a3_create_null_i2c_bus", "[touch_ic_bs8112a3][boundary]")
{
    bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = NULL,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    TEST_ASSERT_NULL(handle);
}

TEST_CASE("touch_ic_bs8112a3_create_null_chip_config", "[touch_ic_bs8112a3][boundary]")
{
    test_i2c_init();
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = NULL,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    TEST_ASSERT_NULL(handle);
    test_i2c_deinit();
}

TEST_CASE("touch_ic_bs8112a3_create_invalid_threshold", "[touch_ic_bs8112a3][boundary]")
{
    test_i2c_init();
    bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    chip_cfg.key_thresholds[0] = 64;  // Invalid: should be 0-63
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);
    // This should fail during chip configuration
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
    if (handle) {
        touch_ic_bs8112a3_delete(handle);
    }
    test_i2c_deinit();
}

TEST_CASE("touch_ic_bs8112a3_delete_null_handle", "[touch_ic_bs8112a3][boundary]")
{
    esp_err_t ret = touch_ic_bs8112a3_delete(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

TEST_CASE("touch_ic_bs8112a3_get_key_status_null_handle", "[touch_ic_bs8112a3][boundary]")
{
    uint16_t status;
    esp_err_t ret = touch_ic_bs8112a3_get_key_status(NULL, &status);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

TEST_CASE("touch_ic_bs8112a3_get_key_value_null_handle", "[touch_ic_bs8112a3][boundary]")
{
    bool pressed;
    esp_err_t ret = touch_ic_bs8112a3_get_key_value(NULL, 0, &pressed);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

TEST_CASE("touch_ic_bs8112a3_get_key_value_invalid_index", "[touch_ic_bs8112a3][boundary]")
{
    test_i2c_init();
    bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

    if (ret == ESP_OK && handle) {
        // Test invalid key index (>= BS8112A3_MAX_KEYS)
        // Valid key indices are 0-11 (BS8112A3_MAX_KEYS = 12)
        bool pressed;
        esp_err_t ret_val = touch_ic_bs8112a3_get_key_value(handle, BS8112A3_MAX_KEYS, &pressed);
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret_val);

        // Test another invalid key index (BS8112A3_MAX_KEYS + 1)
        ret_val = touch_ic_bs8112a3_get_key_value(handle, BS8112A3_MAX_KEYS + 1, &pressed);
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret_val);

        // Test NULL pressed pointer
        ret_val = touch_ic_bs8112a3_get_key_value(handle, 0, NULL);
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret_val);

        touch_ic_bs8112a3_delete(handle);
    } else {
        ESP_LOGW(TAG, "Hardware not available, skipping test");
    }
    test_i2c_deinit();
}

// ========== Function Tests ==========

TEST_CASE("touch_ic_bs8112a3_create_and_delete", "[touch_ic_bs8112a3][function]")
{
    test_i2c_init();
    bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

    if (ret == ESP_OK && handle) {
        TEST_ASSERT_NOT_NULL(handle);
        ESP_LOGI(TAG, "Touch IC created successfully");

        // Test delete
        ret = touch_ic_bs8112a3_delete(handle);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        ESP_LOGI(TAG, "Touch IC deleted successfully");
    } else {
        ESP_LOGW(TAG, "Hardware not available (ret=%s), skipping test", esp_err_to_name(ret));
    }
    test_i2c_deinit();
}

TEST_CASE("touch_ic_bs8112a3_create_with_interrupt", "[touch_ic_bs8112a3][function]")
{
    test_i2c_init();
    bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = TEST_INTERRUPT_PIN,
        .chip_config = &chip_cfg,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

    if (ret == ESP_OK && handle) {
        TEST_ASSERT_NOT_NULL(handle);
        touch_ic_bs8112a3_delete(handle);
    } else {
        ESP_LOGW(TAG, "Hardware not available, skipping test");
    }
    test_i2c_deinit();
}

TEST_CASE("touch_ic_bs8112a3_get_key_status", "[touch_ic_bs8112a3][function]")
{
    test_i2c_init();
    bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

    if (ret == ESP_OK && handle) {
        // Read status multiple times
        for (int i = 0; i < 10; i++) {
            uint16_t status;
            esp_err_t ret_status = touch_ic_bs8112a3_get_key_status(handle, &status);
            if (ret_status == ESP_OK) {
                ESP_LOGI(TAG, "Touch status read %d: 0x%04X", i, status);
            } else {
                ESP_LOGE(TAG, "Failed to read status: %s", esp_err_to_name(ret_status));
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        touch_ic_bs8112a3_delete(handle);
    } else {
        ESP_LOGW(TAG, "Hardware not available, skipping test");
    }
    test_i2c_deinit();
}

TEST_CASE("touch_ic_bs8112a3_get_key_value_all_keys", "[touch_ic_bs8112a3][function]")
{
    test_i2c_init();
    bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

    if (ret == ESP_OK && handle) {
        for (int t = 0; t < 10; t++) {
            // Test all valid key indices
            for (uint8_t i = 0; i < BS8112A3_MAX_KEYS; i++) {
                bool pressed;
                esp_err_t ret_val = touch_ic_bs8112a3_get_key_value(handle, i, &pressed);
                if (ret_val == ESP_OK) {
                    ESP_LOGI(TAG, "Key %02d %spressed", i + 1, pressed ? "is " : "is not ");
                } else {
                    ESP_LOGE(TAG, "Failed to read key %d: %s", i, esp_err_to_name(ret_val));
                }
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        touch_ic_bs8112a3_delete(handle);
    } else {
        ESP_LOGW(TAG, "Hardware not available, skipping test");
    }
    test_i2c_deinit();
}

TEST_CASE("touch_ic_bs8112a3_chip_config_variations", "[touch_ic_bs8112a3][function]")
{
    test_i2c_init();

    // Test with different chip configurations
    bs8112a3_chip_config_t chip_cfg1 = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    chip_cfg1.irq_oms = BS8112A3_IRQ_OMS_ONE_SHOT;
    chip_cfg1.lsc_mode = BS8112A3_LSC_MODE_NORMAL;
    chip_cfg1.k12_mode = BS8112A3_K12_MODE_IRQ;

    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg1,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

    if (ret == ESP_OK && handle) {
        TEST_ASSERT_NOT_NULL(handle);
        ESP_LOGI(TAG, "Touch IC created with custom chip config");
        touch_ic_bs8112a3_delete(handle);
    } else {
        ESP_LOGW(TAG, "Hardware not available, skipping test");
    }
    test_i2c_deinit();
}

TEST_CASE("touch_ic_bs8112a3_threshold_configuration", "[touch_ic_bs8112a3][function]")
{
    test_i2c_init();

    // Test with different thresholds for each key
    bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    for (int i = 0; i < BS8112A3_MAX_KEYS; i++) {
        chip_cfg.key_thresholds[i] = 50 + i;  // Different threshold for each key
        chip_cfg.key_wakeup_enable[i] = (i % 2 == 0);  // Alternate wake-up enable
    }

    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

    if (ret == ESP_OK && handle) {
        TEST_ASSERT_NOT_NULL(handle);
        ESP_LOGI(TAG, "Touch IC created with custom thresholds");
        touch_ic_bs8112a3_delete(handle);
    } else {
        ESP_LOGW(TAG, "Hardware not available, skipping test");
    }
    test_i2c_deinit();
}

static volatile int s_interrupt_count = 0;

static void test_interrupt_callback(touch_ic_bs8112a3_handle_t handle, void *user_data)
{
    // This callback runs in ISR context - only do lightweight operations
    s_interrupt_count++;
    esp_rom_printf("irq called %d times\n", s_interrupt_count);
    // Do NOT call touch_ic_bs8112a3_get_key_status here - it performs I2C operations!
}

TEST_CASE("touch_ic_bs8112a3_interrupt_event_group", "[touch_ic_bs8112a3][function]")
{
    if (TEST_INTERRUPT_PIN == GPIO_NUM_NC) {
        ESP_LOGW(TAG, "Interrupt pin not configured, skipping interrupt test");
        return;
    }

    test_i2c_init();
    s_interrupt_count = 0;

    bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    chip_cfg.k12_mode = BS8112A3_K12_MODE_IRQ;
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = TEST_INTERRUPT_PIN,
        .interrupt_callback = test_interrupt_callback,  // Optional lightweight callback
        .interrupt_user_data = NULL,
        .chip_config = &chip_cfg,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

    if (ret == ESP_OK && handle) {
        TEST_ASSERT_NOT_NULL(handle);

        // Get event group handle (automatically created internally)
        EventGroupHandle_t event_group = touch_ic_bs8112a3_get_event_group(handle);
        TEST_ASSERT_NOT_NULL(event_group);

        ESP_LOGI(TAG, "Touch IC created with interrupt event group, waiting for interrupts...");

        // Wait for interrupts using event group (in task context)
        for (int i = 0; i < 20; i++) {
            EventBits_t bits = xEventGroupWaitBits(event_group, BS8112A3_TOUCH_INTERRUPT_BIT,
                                                   pdTRUE,  // Clear bits after reading
                                                   pdFALSE, // Don't wait for all bits
                                                   portMAX_DELAY);  // Timeout 100ms

            if (bits & BS8112A3_TOUCH_INTERRUPT_BIT) {
                // Now safe to read touch status (in task context)
                uint16_t status;
                esp_err_t ret_status = touch_ic_bs8112a3_get_key_status(handle, &status);
                if (ret_status == ESP_OK) {
                    ESP_LOGI(TAG, "Touch interrupt received! Status: 0x%04X (callback count: %d)",
                             status, s_interrupt_count);
                }
            }
        }

        ESP_LOGI(TAG, "Interrupt event group test completed (interrupt count: %d)", s_interrupt_count);
        touch_ic_bs8112a3_delete(handle);
    } else {
        ESP_LOGW(TAG, "Hardware not available, skipping test");
    }

    test_i2c_deinit();
}

TEST_CASE("touch_ic_bs8112a3_repeated_create_delete", "[touch_ic_bs8112a3][function]")
{
    test_i2c_init();
    bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg,
    };

    // Create and delete multiple times
    for (int i = 0; i < 3; i++) {
        touch_ic_bs8112a3_handle_t handle = NULL;
        esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

        if (ret == ESP_OK && handle) {
            TEST_ASSERT_NOT_NULL(handle);
            ESP_LOGI(TAG, "Created instance %d", i + 1);

            // Read status
            uint16_t status;
            esp_err_t ret_status = touch_ic_bs8112a3_get_key_status(handle, &status);
            if (ret_status == ESP_OK) {
                ESP_LOGI(TAG, "Status: 0x%04X", status);
            } else {
                ESP_LOGE(TAG, "Failed to read status: %s", esp_err_to_name(ret_status));
            }

            // Delete
            ret = touch_ic_bs8112a3_delete(handle);
            TEST_ASSERT_EQUAL(ESP_OK, ret);
            ESP_LOGI(TAG, "Deleted instance %d", i + 1);

            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            ESP_LOGW(TAG, "Hardware not available, skipping test");
            break;
        }
    }
    test_i2c_deinit();
}

TEST_CASE("touch_ic_bs8112a3_read_config", "[touch_ic_bs8112a3][function]")
{
    test_i2c_init();

    // Test with default configuration
    bs8112a3_chip_config_t chip_cfg_write = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg_write,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

    if (ret == ESP_OK && handle) {
        TEST_ASSERT_NOT_NULL(handle);
        ESP_LOGI(TAG, "Touch IC created, reading configuration...");

        // Read configuration
        bs8112a3_chip_config_t chip_cfg_read;
        ret = touch_ic_bs8112a3_read_config(handle, &chip_cfg_read);
        TEST_ASSERT_EQUAL(ESP_OK, ret);

        // Verify read configuration matches written configuration
        TEST_ASSERT_EQUAL(chip_cfg_write.irq_oms, chip_cfg_read.irq_oms);
        TEST_ASSERT_EQUAL(chip_cfg_write.lsc_mode, chip_cfg_read.lsc_mode);
        TEST_ASSERT_EQUAL(chip_cfg_write.k12_mode, chip_cfg_read.k12_mode);

        ESP_LOGI(TAG, "Read config - IRQ OMS: %d, LSC mode: %d, K12 mode: %d",
                 chip_cfg_read.irq_oms, chip_cfg_read.lsc_mode, chip_cfg_read.k12_mode);

        // Verify key thresholds
        for (int i = 0; i < BS8112A3_MAX_KEYS; i++) {
            TEST_ASSERT_EQUAL(chip_cfg_write.key_thresholds[i], chip_cfg_read.key_thresholds[i]);
            TEST_ASSERT_EQUAL(chip_cfg_write.key_wakeup_enable[i], chip_cfg_read.key_wakeup_enable[i]);
        }

        ESP_LOGI(TAG, "Configuration read test passed");
        touch_ic_bs8112a3_delete(handle);
    } else {
        ESP_LOGW(TAG, "Hardware not available, skipping test");
    }
    test_i2c_deinit();
}

TEST_CASE("touch_ic_bs8112a3_read_config_custom", "[touch_ic_bs8112a3][function]")
{
    test_i2c_init();

    // Test with custom configuration
    bs8112a3_chip_config_t chip_cfg_write = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    chip_cfg_write.irq_oms = BS8112A3_IRQ_OMS_ONE_SHOT;
    chip_cfg_write.lsc_mode = BS8112A3_LSC_MODE_NORMAL;
    chip_cfg_write.k12_mode = BS8112A3_K12_MODE_KEY12;

    // Set custom thresholds
    for (int i = 0; i < BS8112A3_MAX_KEYS; i++) {
        chip_cfg_write.key_thresholds[i] = 15 + i;  // Different threshold for each key
        chip_cfg_write.key_wakeup_enable[i] = (i % 2 == 0);  // Alternate wake-up enable
    }

    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg_write,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

    if (ret == ESP_OK && handle) {
        TEST_ASSERT_NOT_NULL(handle);
        ESP_LOGI(TAG, "Touch IC created with custom config, reading configuration...");

        // Read configuration
        bs8112a3_chip_config_t chip_cfg_read;
        ret = touch_ic_bs8112a3_read_config(handle, &chip_cfg_read);
        TEST_ASSERT_EQUAL(ESP_OK, ret);

        // Verify read configuration matches written configuration
        TEST_ASSERT_EQUAL(chip_cfg_write.irq_oms, chip_cfg_read.irq_oms);
        TEST_ASSERT_EQUAL(chip_cfg_write.lsc_mode, chip_cfg_read.lsc_mode);
        TEST_ASSERT_EQUAL(chip_cfg_write.k12_mode, chip_cfg_read.k12_mode);

        ESP_LOGI(TAG, "Read config - IRQ OMS: %d, LSC mode: %d, K12 mode: %d",
                 chip_cfg_read.irq_oms, chip_cfg_read.lsc_mode, chip_cfg_read.k12_mode);

        // Verify key thresholds and wake-up settings
        for (int i = 0; i < BS8112A3_MAX_KEYS; i++) {
            TEST_ASSERT_EQUAL(chip_cfg_write.key_thresholds[i], chip_cfg_read.key_thresholds[i]);
            TEST_ASSERT_EQUAL(chip_cfg_write.key_wakeup_enable[i], chip_cfg_read.key_wakeup_enable[i]);
            ESP_LOGI(TAG, "Key %d: threshold=%d, wakeup=%d", i,
                     chip_cfg_read.key_thresholds[i], chip_cfg_read.key_wakeup_enable[i]);
        }

        ESP_LOGI(TAG, "Custom configuration read test passed");
        touch_ic_bs8112a3_delete(handle);
    } else {
        ESP_LOGW(TAG, "Hardware not available, skipping test");
    }
    test_i2c_deinit();
}

TEST_CASE("touch_ic_bs8112a3_read_config_null_handle", "[touch_ic_bs8112a3][boundary]")
{
    bs8112a3_chip_config_t chip_cfg;
    esp_err_t ret = touch_ic_bs8112a3_read_config(NULL, &chip_cfg);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

TEST_CASE("touch_ic_bs8112a3_read_config_null_config", "[touch_ic_bs8112a3][boundary]")
{
    test_i2c_init();
    bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
    touch_ic_bs8112a3_config_t config = {
        .i2c_bus_handle = s_test_i2c_bus_handle,
        .device_address = BS8112A3_I2C_ADDR,
        .scl_speed_hz = TEST_I2C_FREQ_HZ,
        .irq_pin = GPIO_NUM_NC,
        .chip_config = &chip_cfg,
    };
    touch_ic_bs8112a3_handle_t handle = NULL;
    esp_err_t ret = touch_ic_bs8112a3_create(&config, &handle);

    if (ret == ESP_OK && handle) {
        ret = touch_ic_bs8112a3_read_config(handle, NULL);
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
        touch_ic_bs8112a3_delete(handle);
    } else {
        ESP_LOGW(TAG, "Hardware not available, skipping test");
    }
    test_i2c_deinit();
}
