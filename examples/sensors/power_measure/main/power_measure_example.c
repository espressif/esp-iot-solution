/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "driver/gpio.h"
#include "i2c_bus.h"
#include "driver/uart.h"
#include "driver/spi_master.h"
#include "power_measure.h"
#include "power_measure_bl0937.h"
#include "power_measure_bl0942.h"
#include "power_measure_ina236.h"
#include "math.h"

// BL0937 GPIO definitions
#define BL0937_CF_GPIO   CONFIG_BL0937_CF_GPIO
#define BL0937_SEL_GPIO  CONFIG_BL0937_SEL_GPIO
#define BL0937_CF1_GPIO  CONFIG_BL0937_CF1_GPIO

// INA236 I2C definitions
#define I2C_MASTER_SCL_IO           CONFIG_INA236_I2C_SCL_GPIO
#define I2C_MASTER_SDA_IO           CONFIG_INA236_I2C_SDA_GPIO
#define I2C_MASTER_NUM              I2C_NUM_0   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

// BL0942 UART definitions
#define BL0942_UART_NUM             UART_NUM_1
#define BL0942_TX_GPIO              CONFIG_BL0942_UART_TX_GPIO
#define BL0942_RX_GPIO              CONFIG_BL0942_UART_RX_GPIO

#define BL0942_SPI_HOST             SPI2_HOST
#define BL0942_MOSI_GPIO            CONFIG_BL0942_SPI_MOSI_GPIO
#define BL0942_MISO_GPIO            CONFIG_BL0942_SPI_MISO_GPIO
#define BL0942_SCLK_GPIO            CONFIG_BL0942_SPI_SCLK_GPIO
#define BL0942_CS_GPIO              CONFIG_BL0942_SPI_CS_GPIO

static const char *TAG = "PowerMeasureExample";

#define DEFAULT_KI 1.00f     // Current calibration factor
#define DEFAULT_KU 1.00f     // Voltage calibration factor
#define DEFAULT_KP 1.00f     // Power calibration factor

// Hardware configuration - Adjust these based on your actual hardware
#define SAMPLING_RESISTOR    0.001f    // 1mΩ sampling resistor
#define DIVIDER_RESISTOR     2010.0f   // 2010.0Ω voltage divider

// BL0942 calibration and divider values
#define BL0942_SHUNT_RESISTOR    0.001f    // 1mΩ shunt resistor
#define BL0942_DIVIDER_RATIO     3760.0f   // Divider ratio

static power_measure_handle_t power_handle = NULL;

#if CONFIG_POWER_MEASURE_CHIP_INA236
// Global I2C bus handle for INA236
static i2c_bus_handle_t i2c_bus = NULL;
#endif

/**
 * @brief Initialize I2C bus for INA236
 * @return ESP_OK on success, error code on failure
 */
#if CONFIG_POWER_MEASURE_CHIP_INA236
static esp_err_t init_i2c_bus(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C bus");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "I2C bus initialized successfully");
    return ESP_OK;
}
#endif

/**
 * @brief Cleanup I2C bus
 */
#if CONFIG_POWER_MEASURE_CHIP_INA236
static void cleanup_i2c_bus(void)
{
    if (i2c_bus) {
        i2c_bus_delete(&i2c_bus);
        i2c_bus = NULL;
        ESP_LOGI(TAG, "I2C bus cleaned up");
    }
}
#endif

/**
 * @brief Initialize power measurement using factory pattern
 */
static esp_err_t init_power_measurement(void)
{
    esp_err_t ret = ESP_OK;

    // Common configuration
    power_measure_config_t common_config = {
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true,
    };

#if CONFIG_POWER_MEASURE_CHIP_BL0937
    // BL0937 specific configuration
    power_measure_bl0937_config_t bl0937_config = {
        .sel_gpio = BL0937_SEL_GPIO,
        .cf1_gpio = BL0937_CF1_GPIO,
        .cf_gpio = BL0937_CF_GPIO,
        .pin_mode = 0,
        .sampling_resistor = SAMPLING_RESISTOR,
        .divider_resistor = DIVIDER_RESISTOR,
        .ki = DEFAULT_KI,
        .ku = DEFAULT_KU,
        .kp = DEFAULT_KP,
    };

    ESP_LOGI(TAG, "Creating BL0937 power measurement device...");
    ret = power_measure_new_bl0937_device(&common_config, &bl0937_config, &power_handle);

#elif CONFIG_POWER_MEASURE_CHIP_INA236
    // INA236 specific configuration
    power_measure_ina236_config_t ina236_config = {
        .i2c_bus = i2c_bus,
        .i2c_addr = 0x41,
        .alert_en = false,
        .alert_pin = -1,
        .alert_cb = NULL,
    };

    // INA236 doesn't support energy detection
    common_config.enable_energy_detection = false;

    ESP_LOGI(TAG, "Creating INA236 power measurement device...");
    ret = power_measure_new_ina236_device(&common_config, &ina236_config, &power_handle);

#elif CONFIG_POWER_MEASURE_CHIP_BL0942
    // BL0942 specific configuration (defaults driven by Kconfig)
#ifdef CONFIG_BL0942_USE_SPI
    int USE_SPI = 1;
#else
    int USE_SPI = 0;
#endif

    power_measure_bl0942_config_t bl0942_config = {
        .addr = CONFIG_BL0942_DEVICE_ADDRESS,
        .shunt_resistor = BL0942_SHUNT_RESISTOR,
        .divider_ratio = BL0942_DIVIDER_RATIO,
        .use_spi = USE_SPI,
    };

    if (USE_SPI) {
        bl0942_config.spi.spi_host = BL0942_SPI_HOST;
        bl0942_config.spi.mosi_io = BL0942_MOSI_GPIO;
        bl0942_config.spi.miso_io = BL0942_MISO_GPIO;
        bl0942_config.spi.sclk_io = BL0942_SCLK_GPIO;
        bl0942_config.spi.cs_io = BL0942_CS_GPIO;
    } else {
        bl0942_config.uart.uart_num = BL0942_UART_NUM;
        bl0942_config.uart.tx_io = BL0942_TX_GPIO;
        bl0942_config.uart.rx_io = BL0942_RX_GPIO;
        bl0942_config.uart.sel_io = -1;
        bl0942_config.uart.baud = CONFIG_BL0942_UART_BAUD_RATE;
    }

    if (USE_SPI) {
        ESP_LOGI(TAG, "Creating BL0942 device (SPI mode)... MOSI:%d MISO:%d SCLK:%d CS:%d",
                 BL0942_MOSI_GPIO, BL0942_MISO_GPIO, BL0942_SCLK_GPIO, BL0942_CS_GPIO);
    } else {
        ESP_LOGI(TAG, "Creating BL0942 device (UART mode)... UART%d TX:%d RX:%d Baud:%d",
                 BL0942_UART_NUM, BL0942_TX_GPIO, BL0942_RX_GPIO, CONFIG_BL0942_UART_BAUD_RATE);
    }
    ret = power_measure_new_bl0942_device(&common_config, &bl0942_config, &power_handle);

#else
    ESP_LOGE(TAG, "No chip type selected for demonstration");
    return ESP_ERR_INVALID_ARG;
#endif

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create power measurement device: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Power measurement device created successfully");
    return ESP_OK;
}

/**
 * @brief Main measurement loop
 */
static void measurement_loop(void)
{
    float voltage, current, power, power_factor, energy;
    esp_err_t ret;

    ESP_LOGI(TAG, "Starting measurement loop...");

    while (1) {
        // Get voltage
        ret = power_measure_get_voltage(power_handle, &voltage);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Voltage: %.2f V", voltage);
        } else {
            ESP_LOGE(TAG, "Failed to get voltage: %s", esp_err_to_name(ret));
        }

        // Get current
        ret = power_measure_get_current(power_handle, &current);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Current: %.2f A", current);
        } else {
            ESP_LOGE(TAG, "Failed to get current: %s", esp_err_to_name(ret));
        }

        // Get power
        ret = power_measure_get_active_power(power_handle, &power);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Active Power: %.2f W", power);
        } else {
            ESP_LOGE(TAG, "Failed to get power: %s", esp_err_to_name(ret));
        }

        // Get power factor
        ret = power_measure_get_power_factor(power_handle, &power_factor);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Power Factor: %.3f", power_factor);
        } else {
            ESP_LOGE(TAG, "Failed to get power factor: %s", esp_err_to_name(ret));
        }

        // Get energy (only supported by BL0937)
        ret = power_measure_get_energy(power_handle, &energy);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Energy: %.3f kWh", energy);
        } else if (ret == ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGI(TAG, "Energy measurement not supported by this chip");
        } else {
            ESP_LOGE(TAG, "Failed to get energy: %s", esp_err_to_name(ret));
        }

        ESP_LOGI(TAG, "---");

        // Wait 1 seconds before next measurement
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Power Measure Example Starting...");

#ifdef CONFIG_POWER_MEASURE_CHIP_BL0937
    ESP_LOGI(TAG, "Selected chip: BL0937 (GPIO interface)");
    ESP_LOGI(TAG, "GPIO pins - CF:%d, SEL:%d, CF1:%d",
             CONFIG_BL0937_CF_GPIO, CONFIG_BL0937_SEL_GPIO, CONFIG_BL0937_CF1_GPIO);
#elif CONFIG_POWER_MEASURE_CHIP_INA236
    ESP_LOGI(TAG, "Selected chip: INA236 (I2C interface)");
    ESP_LOGI(TAG, "I2C pins - SCL:%d, SDA:%d",
             CONFIG_INA236_I2C_SCL_GPIO, CONFIG_INA236_I2C_SDA_GPIO);
#elif CONFIG_POWER_MEASURE_CHIP_BL0942
    ESP_LOGI(TAG, "Selected chip: BL0942 (UART/SPI interface)");
#ifdef CONFIG_BL0942_USE_SPI
    ESP_LOGI(TAG, "SPI pins - MOSI:%d, MISO:%d, SCLK:%d, CS:%d",
             BL0942_MOSI_GPIO, BL0942_MISO_GPIO, BL0942_SCLK_GPIO, BL0942_CS_GPIO);
#else
    ESP_LOGI(TAG, "UART - UART%d, TX GPIO:%d, RX GPIO:%d, Baud:%d",
             BL0942_UART_NUM, BL0942_TX_GPIO, BL0942_RX_GPIO, CONFIG_BL0942_UART_BAUD_RATE);
#endif
#endif

    esp_err_t ret = ESP_OK;

    // Initialize I2C bus for INA236
#if CONFIG_POWER_MEASURE_CHIP_INA236
    ret = init_i2c_bus();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
        return;
    }
#endif

    // Initialize power measurement using factory pattern
    ret = init_power_measurement();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize power measurement");
#if CONFIG_POWER_MEASURE_CHIP_INA236
        cleanup_i2c_bus();
#endif
        return;
    }

    ESP_LOGI(TAG, "Power measurement initialized successfully");
    ESP_LOGI(TAG, "Calibration factors: KI=%.3f, KU=%.3f, KP=%.3f",
             DEFAULT_KI, DEFAULT_KU, DEFAULT_KP);
    ESP_LOGI(TAG, "Hardware config: Sampling=%.3fΩ, Divider=%.1fΩ",
             SAMPLING_RESISTOR, DIVIDER_RESISTOR);

    // Start measurement loop
    measurement_loop();
}
