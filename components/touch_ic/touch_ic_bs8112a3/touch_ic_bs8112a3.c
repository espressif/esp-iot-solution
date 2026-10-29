/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "touch_ic_bs8112a3.h"

static const char *TAG = "touch_ic_bs8112a3";

/* Register addresses */
#define BS8112A3_KEY_STATUS0                0x08        /*!< Key status register address */
#define BS8112A3_IRQ_MODE_ADDR              0xB0        /*!< IRQ mode register address */

/* Register configuration constants */
#define BS8112A3_REG_CONFIG_BYTE1           0x00        /*!< Configuration register byte 1 (fixed value) */
#define BS8112A3_REG_CONFIG_BYTE2           0x83        /*!< Configuration register byte 2 (fixed value) */
#define BS8112A3_REG_CONFIG_BYTE3           0xf3        /*!< Configuration register byte 3 (fixed value) */
#define BS8112A3_REG_CONFIG_BYTE4_BASE      0x98        /*!< Configuration register byte 4 base value (LSC mode bit is bit 6) */

/**
 * @brief BS8112A3 handle structure
 */
struct touch_ic_bs8112a3_handle_s {
    i2c_master_dev_handle_t i2c_dev_handle;    /*!< I2C device handle */
    gpio_num_t interrupt_pin;                  /*!< GPIO pin for interrupt (GPIO_NUM_NC if not used) */
    EventGroupHandle_t interrupt_event_group;   /*!< Event group for interrupt notification */
    touch_ic_bs8112a3_interrupt_callback_t interrupt_callback;  /*!< Interrupt callback function */
    void *interrupt_user_data;                 /*!< User data for interrupt callback */
};

/* Private function declarations */
static esp_err_t bs8112a3_read_register(touch_ic_bs8112a3_handle_t handle, uint8_t reg_addr, uint8_t *data, size_t len);
static esp_err_t bs8112a3_write_register(touch_ic_bs8112a3_handle_t handle, uint8_t reg_addr, const uint8_t *data, size_t len);
static esp_err_t bs8112a3_configure_chip(touch_ic_bs8112a3_handle_t handle, const bs8112a3_chip_config_t *chip_config, gpio_num_t irq_pin);
static esp_err_t touch_interrupt_init(touch_ic_bs8112a3_handle_t handle, gpio_num_t pin);

/**
 * @brief Read data from BS8112A3 register
 *
 * @param handle Handle returned from touch_ic_bs8112a3_create()
 * @param reg_addr Register address to read from
 * @param[out] data Buffer to store read data
 * @param len Number of bytes to read
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_FAIL: I2C communication failed
 */
static esp_err_t bs8112a3_read_register(touch_ic_bs8112a3_handle_t handle, uint8_t reg_addr, uint8_t *data, size_t len)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(handle->i2c_dev_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid I2C device handle");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "Invalid data pointer");

    return i2c_master_transmit_receive(handle->i2c_dev_handle, &reg_addr, 1, data, len, 1000);
}

/**
 * @brief Write data to BS8112A3 register
 *
 * @param handle Handle returned from touch_ic_bs8112a3_create()
 * @param reg_addr Register address to write to
 * @param data Data buffer to write
 * @param len Number of bytes to write
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_FAIL: I2C communication failed
 */
static esp_err_t bs8112a3_write_register(touch_ic_bs8112a3_handle_t handle, uint8_t reg_addr, const uint8_t *data, size_t len)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(handle->i2c_dev_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid I2C device handle");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "Invalid data pointer");

    /* Max expected write size based on chip config (18 bytes + margin) */
    uint8_t write_buf[32];
    ESP_RETURN_ON_FALSE(len + 1 <= sizeof(write_buf), ESP_ERR_INVALID_ARG, TAG, "Data length too large: %zu", len);

    write_buf[0] = reg_addr;
    memcpy(&write_buf[1], data, len);

    return i2c_master_transmit(handle->i2c_dev_handle, write_buf, len + 1, 1000);
}

/**
 * @brief Configure BS8112A3 chip settings
 *
 * This function configures the chip with user-provided settings including IRQ mode,
 * power saving mode, Key12 mode, key thresholds, and wake-up settings.
 *
 * @param handle Handle returned from touch_ic_bs8112a3_create()
 * @param config Chip configuration structure
 * @param irq_pin GPIO pin for IRQ (GPIO_NUM_NC if not used)
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid configuration
 *      - ESP_FAIL: Failed to write configuration
 */
static esp_err_t bs8112a3_configure_chip(touch_ic_bs8112a3_handle_t handle, const bs8112a3_chip_config_t *config, gpio_num_t irq_pin)
{
    esp_err_t ret;
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid chip configuration");
    for (int i = 0; i < BS8112A3_MAX_KEYS; i++) {
        ESP_RETURN_ON_FALSE(config->key_thresholds[i] >= 8 && config->key_thresholds[i] < 64,
                            ESP_ERR_INVALID_ARG, TAG, "Invalid key threshold for key %d (must be 8-63)", i);
    }

    /* Validate Key12 mode and IRQ pin configuration */
    ESP_RETURN_ON_FALSE(!(config->k12_mode == BS8112A3_K12_MODE_IRQ && irq_pin == GPIO_NUM_NC),
                        ESP_ERR_INVALID_ARG, TAG, "Key12 mode is IRQ but interrupt pin is not set");
    ESP_RETURN_ON_FALSE(!(config->k12_mode == BS8112A3_K12_MODE_KEY12 && irq_pin != GPIO_NUM_NC),
                        ESP_ERR_INVALID_ARG, TAG, "Key12 mode is KEY12 but interrupt pin is set");

    /* Build configuration data array (18 bytes total) */
    uint8_t config_data[18] = {
        (uint8_t)config->irq_oms,                          /* Byte 0: IRQ output mode selection */
        BS8112A3_REG_CONFIG_BYTE1,                         /* Byte 1: Configuration byte 1 (fixed) */
        BS8112A3_REG_CONFIG_BYTE2,                         /* Byte 2: Configuration byte 2 (fixed) */
        BS8112A3_REG_CONFIG_BYTE3,                         /* Byte 3: Configuration byte 3 (fixed) */
        BS8112A3_REG_CONFIG_BYTE4_BASE | ((uint8_t)config->lsc_mode << 6),  /* Byte 4: Base value + LSC mode bit (bit 6) */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0             /* Bytes 5-16: Key thresholds (12 bytes) + checksum (1 byte) */
    };

    /* Configure key trigger thresholds and wake-up settings */
    uint8_t *keys_th = &config_data[5];
    for (int i = 0; i < BS8112A3_MAX_KEYS; i++) {
        /* Threshold is 6-bit value (0-63), stored in bits 0-5 */
        keys_th[i] = config->key_thresholds[i] & 0x3F;
        /* Wake-up enable bit (bit 7: 0=enabled, 1=disabled) */
        keys_th[i] |= ((uint8_t)!config->key_wakeup_enable[i] << 7);
    }

    /* Key12 mode bit (bit 6 of key 11 threshold byte) */
    keys_th[11] |= ((uint8_t)config->k12_mode << 6);

    /* Calculate checksum (sum of first 17 bytes, stored in byte 17) */
    config_data[17] = 0;
    for (int i = 0; i < 17; i++) {
        config_data[17] += config_data[i];
    }

    /* Write configuration to chip */
    ret = bs8112a3_write_register(handle, BS8112A3_IRQ_MODE_ADDR, config_data, 18);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write configuration");
        return ret;
    }

    ESP_LOGI(TAG, "BS8112A3 chip configured successfully. IRQ OMS: %d, LSC mode: %d, K12 mode: %d",
             config->irq_oms, config->lsc_mode, config->k12_mode);
    return ESP_OK;
}

/**
 * @brief GPIO interrupt handler for BS8112A3
 *
 * This ISR is called when a touch interrupt occurs. It sets the event bit in the event group
 * (if provided) and calls the registered callback function (if provided).
 *
 * @note This function runs in ISR context. Do NOT perform I2C operations or other blocking
 *       operations here. Use event group to notify tasks instead.
 *
 * @param arg Handle pointer passed from gpio_isr_handler_add()
 */
static void IRAM_ATTR touch_interrupt_handler(void *arg)
{
    touch_ic_bs8112a3_handle_t handle = (touch_ic_bs8112a3_handle_t)arg;
    if (!handle) {
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Set event bit in event group (if provided) - can notify multiple tasks */
    if (handle->interrupt_event_group) {
        xEventGroupSetBitsFromISR(handle->interrupt_event_group, BS8112A3_TOUCH_INTERRUPT_BIT, &xHigherPriorityTaskWoken);
    }

    /* Call interrupt callback if registered (for lightweight notification only) */
    if (handle->interrupt_callback) {
        handle->interrupt_callback(handle, handle->interrupt_user_data);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief Initialize GPIO interrupt for BS8112A3
 *
 * Configures the GPIO pin as input with pull-up and sets up interrupt handler
 * for negative edge trigger.
 *
 * @param handle Handle returned from touch_ic_bs8112a3_create()
 * @param pin GPIO pin number for interrupt
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid pin number
 *      - ESP_FAIL: GPIO configuration failed
 */
static esp_err_t touch_interrupt_init(touch_ic_bs8112a3_handle_t handle, gpio_num_t pin)
{
    handle->interrupt_pin = pin;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = gpio_isr_handler_add(pin, touch_interrupt_handler, handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

esp_err_t touch_ic_bs8112a3_create(const touch_ic_bs8112a3_config_t *config, touch_ic_bs8112a3_handle_t *ret_handle)
{
    esp_err_t ret = ESP_OK;
    touch_ic_bs8112a3_handle_t handle = NULL;

    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");
    ESP_RETURN_ON_FALSE(ret_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle pointer");
    ESP_RETURN_ON_FALSE(config->i2c_bus_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid I2C bus handle");
    ESP_RETURN_ON_FALSE(config->chip_config, ESP_ERR_INVALID_ARG, TAG, "Invalid chip configuration");

    /* Allocate handle structure */
    handle = (touch_ic_bs8112a3_handle_t)calloc(1, sizeof(struct touch_ic_bs8112a3_handle_s));
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_NO_MEM, TAG, "Failed to allocate handle");

    /* Initialize handle fields */
    handle->i2c_dev_handle = NULL;
    handle->interrupt_pin = GPIO_NUM_NC;
    handle->interrupt_event_group = NULL;
    handle->interrupt_callback = NULL;
    handle->interrupt_user_data = NULL;

    /* Create I2C device handle */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->device_address,
        .scl_speed_hz = config->scl_speed_hz,
        .flags.disable_ack_check = false,
    };

    ret = i2c_master_bus_add_device(config->i2c_bus_handle, &dev_cfg, &handle->i2c_dev_handle);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ret, err, TAG, "Failed to add I2C device");

    /* Check hardware availability */
    ret = i2c_master_probe(config->i2c_bus_handle, config->device_address, 100);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_ERR_NOT_FOUND, err, TAG, "Touch IC hardware not responding");

    /* Configure chip with user-provided configuration */
    ret = bs8112a3_configure_chip(handle, config->chip_config, config->irq_pin);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ret, err, TAG, "Failed to configure chip");

    /* Initialize interrupt if pin is provided */
    if (config->irq_pin != GPIO_NUM_NC) {
        /* Install GPIO ISR service if not already installed (required for interrupt mode)
         * Note: We don't uninstall the service on delete to avoid affecting other components
         * that may be using the same ISR service. The service is global and can be shared.
         */
        ret = gpio_install_isr_service(0);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
            goto err;
        } else if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGD(TAG, "GPIO ISR service already installed (shared with other components)");
        } else {
            ESP_LOGI(TAG, "GPIO ISR service installed for touch interrupt");
        }

        /* Create event group for interrupt notification */
        handle->interrupt_event_group = xEventGroupCreate();
        ESP_GOTO_ON_FALSE(handle->interrupt_event_group, ESP_ERR_NO_MEM, err, TAG, "Failed to create event group");

        /* Store interrupt callback and user data */
        handle->interrupt_callback = config->interrupt_callback;
        handle->interrupt_user_data = config->interrupt_user_data;

        ret = touch_interrupt_init(handle, config->irq_pin);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ret, err, TAG, "Failed to initialize interrupt");
        ESP_LOGI(TAG, "Touch IC initialized with interrupt mode on GPIO %d", config->irq_pin);
    }

    *ret_handle = handle;
    return ESP_OK;

err:
    if (handle) {
        if (handle->interrupt_pin != GPIO_NUM_NC) {
            gpio_isr_handler_remove(handle->interrupt_pin);
        }
        if (handle->interrupt_event_group) {
            vEventGroupDelete(handle->interrupt_event_group);
        }
        if (handle->i2c_dev_handle) {
            i2c_master_bus_rm_device(handle->i2c_dev_handle);
        }
        free(handle);
    }
    return ret;
}

esp_err_t touch_ic_bs8112a3_delete(touch_ic_bs8112a3_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    /* Remove GPIO interrupt handler */
    if (handle->interrupt_pin != GPIO_NUM_NC) {
        gpio_isr_handler_remove(handle->interrupt_pin);
        /* Note: We don't uninstall the ISR service here because it's global and
         * may be shared with other components. Uninstalling it would break other
         * components that rely on the same ISR service.
         */
    }

    /* Delete event group */
    if (handle->interrupt_event_group) {
        vEventGroupDelete(handle->interrupt_event_group);
    }

    /* Remove I2C device */
    esp_err_t ret = ESP_OK;
    if (handle->i2c_dev_handle) {
        ret = i2c_master_bus_rm_device(handle->i2c_dev_handle);
    }

    /* Free handle */
    free(handle);

    return ret;
}

esp_err_t touch_ic_bs8112a3_get_key_status(touch_ic_bs8112a3_handle_t handle, uint16_t *status)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(status, ESP_ERR_INVALID_ARG, TAG, "Invalid status pointer");

    /* Read register directly (always read latest status from hardware) */
    esp_err_t ret = bs8112a3_read_register(handle, BS8112A3_KEY_STATUS0, (uint8_t *)status, 2);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Failed to read key status");

    return ESP_OK;
}

esp_err_t touch_ic_bs8112a3_get_key_value(touch_ic_bs8112a3_handle_t handle, uint8_t key_index, bool *pressed)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(pressed, ESP_ERR_INVALID_ARG, TAG, "Invalid pressed pointer");
    ESP_RETURN_ON_FALSE(key_index < BS8112A3_MAX_KEYS, ESP_ERR_INVALID_ARG, TAG, "Invalid key index");

    uint16_t status = 0;
    esp_err_t ret = touch_ic_bs8112a3_get_key_status(handle, &status);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Failed to read key status");

    *pressed = (status >> key_index) & 0x01;
    return ESP_OK;
}

EventGroupHandle_t touch_ic_bs8112a3_get_event_group(touch_ic_bs8112a3_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, NULL, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(handle->interrupt_pin != GPIO_NUM_NC, NULL, TAG, "Interrupt not enabled");
    return handle->interrupt_event_group;
}

esp_err_t touch_ic_bs8112a3_read_config(touch_ic_bs8112a3_handle_t handle, bs8112a3_chip_config_t *config)
{
    esp_err_t ret;
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid config pointer");

    /* Read configuration data array (18 bytes total) */
    uint8_t config_data[18] = {0};
    ret = bs8112a3_read_register(handle, BS8112A3_IRQ_MODE_ADDR, config_data, 18);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read configuration");
        return ret;
    }

    /* Verify checksum (sum of first 17 bytes should equal byte 17) */
    uint8_t calculated_checksum = 0;
    for (int i = 0; i < 17; i++) {
        calculated_checksum += config_data[i];
    }
    if (calculated_checksum != config_data[17]) {
        ESP_LOGE(TAG, "Configuration checksum mismatch: expected 0x%02X, got 0x%02X",
                 config_data[17], calculated_checksum);
        return ESP_FAIL;
    }

    /* Parse Byte 0: IRQ output mode selection */
    config->irq_oms = (bs8112a3_irq_oms_t)(config_data[0] & 0x01);

    /* Parse Byte 4: LSC mode bit (bit 6) */
    config->lsc_mode = (bs8112a3_lsc_mode_t)((config_data[4] >> 6) & 0x01);

    /* Parse Bytes 5-16: Key thresholds and wake-up settings */
    uint8_t *keys_th = &config_data[5];
    for (int i = 0; i < BS8112A3_MAX_KEYS; i++) {
        /* Threshold is 6-bit value (bits 0-5) */
        config->key_thresholds[i] = keys_th[i] & 0x3F;
        /* Wake-up enable bit (bit 7: 0=enabled, 1=disabled) */
        config->key_wakeup_enable[i] = !((keys_th[i] >> 7) & 0x01);
    }

    /* Parse Key12 mode bit (bit 6 of key 11 threshold byte) */
    config->k12_mode = (bs8112a3_k12_mode_t)((keys_th[11] >> 6) & 0x01);

    ESP_LOGI(TAG, "BS8112A3 chip configuration read successfully. IRQ OMS: %d, LSC mode: %d, K12 mode: %d",
             config->irq_oms, config->lsc_mode, config->k12_mode);
    return ESP_OK;
}
