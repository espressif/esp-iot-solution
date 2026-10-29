/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "power_measure.h"
#include "power_measure_bl0937.h"
#include "power_measure_bl0942.h"
#include "power_measure_ina236.h"
#include "unity.h"
#include "i2c_bus.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

/* BL0937 test pins */
#define BL0937_CF_GPIO   GPIO_NUM_3  // CF  pin
#define BL0937_SEL_GPIO  GPIO_NUM_4  // SEL pin
#define BL0937_CF1_GPIO  GPIO_NUM_7  // CF1 pin

/* INA236 test pins */
#define I2C_MASTER_SCL_IO           GPIO_NUM_13          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           GPIO_NUM_20          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */
#define INA236_I2C_ADDRESS_DEFAULT      0x41

/* BL0942 test pins */
#define BL0942_UART_NUM             UART_NUM_1
#define BL0942_TX_GPIO              GPIO_NUM_6
#define BL0942_RX_GPIO              GPIO_NUM_7
#define BL0942_SEL_GPIO             GPIO_NUM_5

static const char *TAG = "PowerMeasureTest";

static size_t before_free_8bit;
static size_t before_free_32bit;

// Default calibration factors
#define DEFAULT_KI 1.0f
#define DEFAULT_KU 1.0f
#define DEFAULT_KP 1.0f

/* Helper to create I2C bus for INA236 tests */
static i2c_bus_handle_t test_i2c_bus = NULL;

/* Power measure device handles for testing */
static power_measure_handle_t test_power_handle = NULL;

// Quick probe for INA236 presence to avoid blocking when hardware is absent
static bool ina236_quick_probe(uint8_t addr)
{
    ESP_LOGI(TAG, "Probing for INA236 at address 0x%02X...", addr);

    // Setup I2C bus for probing
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_bus_handle_t probe_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    if (probe_bus == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C bus for probing");
        return false;
    }

    i2c_bus_device_handle_t probe_dev = i2c_bus_device_create(probe_bus, addr, 0);
    if (probe_dev == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C device for probing");
        TEST_ASSERT(ESP_OK == i2c_bus_delete(&probe_bus));
        return false;
    }

    uint8_t dummy_data = 0;
    esp_err_t err = i2c_bus_read_bytes(probe_dev, 0x00, 1, &dummy_data);

    TEST_ASSERT(ESP_OK == i2c_bus_device_delete(&probe_dev));
    TEST_ASSERT(ESP_OK == i2c_bus_delete(&probe_bus));

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "INA236 detected at address 0x%02X", addr);
        return true;
    } else {
        ESP_LOGW(TAG, "INA236 not detected at address 0x%02X (error: %s)", addr, esp_err_to_name(err));
        return false;
    }
}

static esp_err_t setup_i2c_bus(void)
{
    if (test_i2c_bus) {
        return ESP_OK; // Already initialized
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    // i2c_bus_create will handle driver installation internally
    test_i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    if (test_i2c_bus == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C bus");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "I2C bus initialized successfully");
    return ESP_OK;
}

static void cleanup_i2c_bus(void)
{
    if (test_i2c_bus) {
        TEST_ASSERT(ESP_OK == i2c_bus_delete(&test_i2c_bus));
        test_i2c_bus = NULL;
    }
}

static void cleanup_power_device(void)
{
    if (test_power_handle) {
        power_measure_delete(test_power_handle);
        test_power_handle = NULL;
    }
}

/* BL0937 Tests */
TEST_CASE("BL0937 Initialization Test", "[power_measure][bl0937]")
{
    // Clean up any existing device
    cleanup_power_device();

    power_measure_config_t config = {
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true
    };

    power_measure_bl0937_config_t bl0937_config = {
        .sel_gpio = BL0937_SEL_GPIO,
        .cf1_gpio = BL0937_CF1_GPIO,
        .cf_gpio = BL0937_CF_GPIO,
        .pin_mode = 0,
        .sampling_resistor = 0.001f,
        .divider_resistor = 2010.0f,
        .ki = DEFAULT_KI,
        .ku = DEFAULT_KU,
        .kp = DEFAULT_KP
    };

    esp_err_t ret = power_measure_new_bl0937_device(&config, &bl0937_config, &test_power_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(test_power_handle);

    cleanup_power_device();
}

TEST_CASE("BL0937 Get Voltage Test", "[power_measure][bl0937]")
{
    // Clean up any existing device
    cleanup_power_device();

    power_measure_config_t config = {
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true
    };

    power_measure_bl0937_config_t bl0937_config = {
        .sel_gpio = BL0937_SEL_GPIO,
        .cf1_gpio = BL0937_CF1_GPIO,
        .cf_gpio = BL0937_CF_GPIO,
        .pin_mode = 0,
        .sampling_resistor = 0.001f,
        .divider_resistor = 2010.0f,
        .ki = DEFAULT_KI,
        .ku = DEFAULT_KU,
        .kp = DEFAULT_KP
    };

    esp_err_t ret = power_measure_new_bl0937_device(&config, &bl0937_config, &test_power_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(test_power_handle);

    vTaskDelay(2000 / portTICK_PERIOD_MS);  // Wait for measurements to stabilize

    float voltage = 0.0f;
    esp_err_t ret_v = power_measure_get_voltage(test_power_handle, &voltage);
    TEST_ASSERT_EQUAL(ESP_OK, ret_v);
    ESP_LOGI(TAG, "BL0937 Voltage: %.2f V", voltage);
    TEST_ASSERT_GREATER_OR_EQUAL(0.0f, voltage);

    cleanup_power_device();
}

TEST_CASE("BL0937 Get Current Test", "[power_measure][bl0937]")
{
    // Clean up any existing device
    cleanup_power_device();

    power_measure_config_t config = {
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true
    };

    power_measure_bl0937_config_t bl0937_config = {
        .sel_gpio = BL0937_SEL_GPIO,
        .cf1_gpio = BL0937_CF1_GPIO,
        .cf_gpio = BL0937_CF_GPIO,
        .pin_mode = 0,
        .sampling_resistor = 0.001f,
        .divider_resistor = 2010.0f,
        .ki = DEFAULT_KI,
        .ku = DEFAULT_KU,
        .kp = DEFAULT_KP
    };

    esp_err_t ret = power_measure_new_bl0937_device(&config, &bl0937_config, &test_power_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(test_power_handle);

    vTaskDelay(2000 / portTICK_PERIOD_MS);  // Wait for measurements to stabilize

    float current = 0.0f;
    esp_err_t ret_c = power_measure_get_current(test_power_handle, &current);
    TEST_ASSERT_EQUAL(ESP_OK, ret_c);
    ESP_LOGI(TAG, "BL0937 Current: %.2f A", current);
    TEST_ASSERT_GREATER_OR_EQUAL(0.0f, current);

    cleanup_power_device();
}

TEST_CASE("BL0937 Get Active Power Test", "[power_measure][bl0937]")
{
    // Clean up any existing device
    cleanup_power_device();

    power_measure_config_t config = {
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true
    };

    power_measure_bl0937_config_t bl0937_config = {
        .sel_gpio = BL0937_SEL_GPIO,
        .cf1_gpio = BL0937_CF1_GPIO,
        .cf_gpio = BL0937_CF_GPIO,
        .pin_mode = 0,
        .sampling_resistor = 0.001f,
        .divider_resistor = 2010.0f,
        .ki = DEFAULT_KI,
        .ku = DEFAULT_KU,
        .kp = DEFAULT_KP
    };

    esp_err_t ret = power_measure_new_bl0937_device(&config, &bl0937_config, &test_power_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(test_power_handle);

    vTaskDelay(2000 / portTICK_PERIOD_MS);  // Wait for measurements to stabilize

    float power = 0.0f;
    esp_err_t ret_p = power_measure_get_active_power(test_power_handle, &power);
    TEST_ASSERT_EQUAL(ESP_OK, ret_p);
    ESP_LOGI(TAG, "BL0937 Power: %.2f W", power);
    TEST_ASSERT_GREATER_OR_EQUAL(0.0f, power);

    cleanup_power_device();
}

TEST_CASE("BL0937 Get Power Factor Test", "[power_measure][bl0937]")
{
    // Clean up any existing device
    cleanup_power_device();

    power_measure_config_t config = {
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true
    };

    power_measure_bl0937_config_t bl0937_config = {
        .sel_gpio = BL0937_SEL_GPIO,
        .cf1_gpio = BL0937_CF1_GPIO,
        .cf_gpio = BL0937_CF_GPIO,
        .pin_mode = 0,
        .sampling_resistor = 0.001f,
        .divider_resistor = 2010.0f,
        .ki = DEFAULT_KI,
        .ku = DEFAULT_KU,
        .kp = DEFAULT_KP
    };

    esp_err_t ret = power_measure_new_bl0937_device(&config, &bl0937_config, &test_power_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(test_power_handle);

    vTaskDelay(2000 / portTICK_PERIOD_MS);  // Wait for measurements to stabilize

    float power_factor = 0.0f;
    esp_err_t ret_pf = power_measure_get_power_factor(test_power_handle, &power_factor);
    TEST_ASSERT_EQUAL(ESP_OK, ret_pf);
    ESP_LOGI(TAG, "BL0937 Power Factor: %.3f", power_factor);
    TEST_ASSERT_GREATER_OR_EQUAL(0.0f, power_factor);
    TEST_ASSERT_LESS_OR_EQUAL(1.0f, power_factor);

    cleanup_power_device();
}

TEST_CASE("BL0937 Get Energy Test", "[power_measure][bl0937]")
{
    // Clean up any existing device
    cleanup_power_device();

    power_measure_config_t config = {
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true
    };

    power_measure_bl0937_config_t bl0937_config = {
        .sel_gpio = BL0937_SEL_GPIO,
        .cf1_gpio = BL0937_CF1_GPIO,
        .cf_gpio = BL0937_CF_GPIO,
        .pin_mode = 0,
        .sampling_resistor = 0.001f,
        .divider_resistor = 2010.0f,
        .ki = DEFAULT_KI,
        .ku = DEFAULT_KU,
        .kp = DEFAULT_KP
    };

    esp_err_t ret = power_measure_new_bl0937_device(&config, &bl0937_config, &test_power_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(test_power_handle);

    esp_err_t ret_reset = power_measure_reset_energy_calculation(test_power_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret_reset);

    vTaskDelay(3000 / portTICK_PERIOD_MS);  // Wait for energy accumulation

    float energy = 0.0f;
    esp_err_t ret_e = power_measure_get_energy(test_power_handle, &energy);
    TEST_ASSERT_EQUAL(ESP_OK, ret_e);
    ESP_LOGI(TAG, "BL0937 Energy: %.6f kWh", energy);
    TEST_ASSERT_GREATER_OR_EQUAL(0.0f, energy);

    cleanup_power_device();
}

/* INA236 Tests */
TEST_CASE("INA236 Initialization Test", "[power_measure][ina236]")
{
    // Clean up any existing device
    cleanup_power_device();
    cleanup_i2c_bus();

    // Probe device quickly first to avoid installing i2c driver twice
    if (!ina236_quick_probe(INA236_I2C_ADDRESS_DEFAULT)) {
        TEST_IGNORE_MESSAGE("INA236 not detected on I2C - hardware not connected");
        return;
    }

    esp_err_t bus_ret = setup_i2c_bus();
    if (bus_ret != ESP_OK) {
        TEST_IGNORE_MESSAGE("I2C bus setup failed - INA236 not connected?");
        return;
    }

    power_measure_config_t config = {
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = false  // INA236 doesn't support energy detection
    };

    power_measure_ina236_config_t ina236_config = {
        .i2c_bus = test_i2c_bus,
        .i2c_addr = INA236_I2C_ADDRESS_DEFAULT,
        .alert_en = false,
        .alert_pin = -1,
        .alert_cb = NULL
    };

    esp_err_t ret = power_measure_new_ina236_device(&config, &ina236_config, &test_power_handle);
    if (ret != ESP_OK) {
        cleanup_i2c_bus();
        TEST_IGNORE_MESSAGE("INA236 initialization failed - chip not responding");
        return;
    }
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(test_power_handle);

    cleanup_power_device();
    cleanup_i2c_bus();
}

TEST_CASE("INA236 Get Voltage Test", "[power_measure][ina236]")
{
    // Clean up any existing device
    cleanup_power_device();
    cleanup_i2c_bus();

    // Probe device quickly first to avoid installing i2c driver twice
    if (!ina236_quick_probe(INA236_I2C_ADDRESS_DEFAULT)) {
        TEST_IGNORE_MESSAGE("INA236 not detected on I2C - hardware not connected");
        return;
    }

    esp_err_t bus_ret = setup_i2c_bus();
    if (bus_ret != ESP_OK) {
        TEST_IGNORE_MESSAGE("I2C bus setup failed - INA236 not connected?");
        return;
    }

    power_measure_config_t config = {
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = false
    };

    power_measure_ina236_config_t ina236_config = {
        .i2c_bus = test_i2c_bus,
        .i2c_addr = INA236_I2C_ADDRESS_DEFAULT,
        .alert_en = false,
        .alert_pin = -1,
        .alert_cb = NULL
    };

    esp_err_t ret = power_measure_new_ina236_device(&config, &ina236_config, &test_power_handle);
    if (ret != ESP_OK) {
        cleanup_i2c_bus();
        TEST_IGNORE_MESSAGE("INA236 initialization failed - chip not responding");
        return;
    }
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(test_power_handle);

    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Wait for measurements to stabilize

    float voltage = 0.0f;
    esp_err_t ret_v = power_measure_get_voltage(test_power_handle, &voltage);
    TEST_ASSERT_EQUAL(ESP_OK, ret_v);
    ESP_LOGI(TAG, "INA236 Voltage: %.3f V", voltage);
    TEST_ASSERT_GREATER_OR_EQUAL(0.0f, voltage);

    cleanup_power_device();
    cleanup_i2c_bus();
}

TEST_CASE("INA236 Get Current Test", "[power_measure][ina236]")
{
    // Clean up any existing device
    cleanup_power_device();
    cleanup_i2c_bus();

    // Probe device quickly first to avoid installing i2c driver twice
    if (!ina236_quick_probe(INA236_I2C_ADDRESS_DEFAULT)) {
        TEST_IGNORE_MESSAGE("INA236 not detected on I2C - hardware not connected");
        return;
    }

    esp_err_t bus_ret = setup_i2c_bus();
    if (bus_ret != ESP_OK) {
        TEST_IGNORE_MESSAGE("I2C bus setup failed - INA236 not connected?");
        return;
    }

    power_measure_config_t config = {
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = false
    };

    power_measure_ina236_config_t ina236_config = {
        .i2c_bus = test_i2c_bus,
        .i2c_addr = INA236_I2C_ADDRESS_DEFAULT,
        .alert_en = false,
        .alert_pin = -1,
        .alert_cb = NULL
    };

    esp_err_t ret = power_measure_new_ina236_device(&config, &ina236_config, &test_power_handle);
    if (ret != ESP_OK) {
        cleanup_i2c_bus();
        TEST_IGNORE_MESSAGE("INA236 initialization failed - chip not responding");
        return;
    }
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(test_power_handle);

    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Wait for measurements to stabilize

    float current = 0.0f;
    esp_err_t ret_c = power_measure_get_current(test_power_handle, &current);
    TEST_ASSERT_EQUAL(ESP_OK, ret_c);
    ESP_LOGI(TAG, "INA236 Current: %.3f A", current);
    // Current can be negative for INA236 depending on shunt direction

    cleanup_power_device();
    cleanup_i2c_bus();
}

/* BL0942 Tests */
TEST_CASE("BL0942 Initialization Test", "[power_measure][bl0942]")
{
    // Clean up any existing device
    cleanup_power_device();

    power_measure_config_t config = {
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true
    };

    power_measure_bl0942_config_t bl0942_config = {
        .addr = 0,
        .shunt_resistor = 0.001f,
        .divider_ratio = 1000.0f,
        .use_spi = false,
    };

    bl0942_config.uart.uart_num = BL0942_UART_NUM;
    bl0942_config.uart.tx_io = BL0942_TX_GPIO;
    bl0942_config.uart.rx_io = BL0942_RX_GPIO;
    bl0942_config.uart.sel_io = BL0942_SEL_GPIO;
    bl0942_config.uart.baud = 9600;

    esp_err_t ret = power_measure_new_bl0942_device(&config, &bl0942_config, &test_power_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(test_power_handle);

    // Test basic measurements
    float voltage, current, power, energy;
    ret = power_measure_get_voltage(test_power_handle, &voltage);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    ESP_LOGI(TAG, "BL0942 Voltage: %.2f V", voltage);

    ret = power_measure_get_current(test_power_handle, &current);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    ESP_LOGI(TAG, "BL0942 Current: %.3f A", current);

    ret = power_measure_get_active_power(test_power_handle, &power);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    ESP_LOGI(TAG, "BL0942 Power: %.2f W", power);

    ret = power_measure_get_energy(test_power_handle, &energy);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    ESP_LOGI(TAG, "BL0942 Energy: %.2f Wh", energy);

    // Clean up
    cleanup_power_device();
}

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n",
           type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    // Ensure all resources are released before measuring heap for leak check
    cleanup_power_device();
    cleanup_i2c_bus();
    // Uninstall global GPIO ISR service if installed to avoid cross-test residue
    gpio_uninstall_isr_service();

    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("POWER MEASURE TEST (BL0937 + INA236)\n");
    unity_run_menu();
}
