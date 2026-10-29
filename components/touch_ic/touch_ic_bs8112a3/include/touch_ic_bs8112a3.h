/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BS8112A3_I2C_ADDR                   0x50        /*!< BS8112A3 I2C device address */
#define BS8112A3_MAX_KEYS                   12          /*!< Maximum number of keys supported */

/**
 * @brief BS8112A3 handle structure (opaque)
 */
typedef struct touch_ic_bs8112a3_handle_s *touch_ic_bs8112a3_handle_t;

/**
 * @brief IRQ output mode selection enumeration
 */
typedef enum {
    BS8112A3_IRQ_OMS_LEVEL_HOLD = 0,   /*!< Level hold mode: IRQ output remains low while any key is pressed */
    BS8112A3_IRQ_OMS_ONE_SHOT = 1,     /*!< One-shot mode: IRQ output pulses low once when a key is pressed */
} bs8112a3_irq_oms_t;

/**
 * @brief Key12 mode selection enumeration
 */
typedef enum {
    BS8112A3_K12_MODE_KEY12 = 0,    /*!< Key12 mode: Key12 functions as a normal touch key */
    BS8112A3_K12_MODE_IRQ = 1,      /*!< IRQ mode: Key12 functions as IRQ output pin */
} bs8112a3_k12_mode_t;

/**
 * @brief Low power saving mode (LSC) enumeration
 */
typedef enum {
    BS8112A3_LSC_MODE_NORMAL = 0,   /*!< Normal power saving mode: Standard power consumption with normal wake-up time */
    BS8112A3_LSC_MODE_MORE = 1,     /*!< More power saving mode: Lower power consumption but wake-up time increased to 0.5~1 second */
} bs8112a3_lsc_mode_t;

/**
 * @brief Chip configuration structure for BS8112A3
 */
typedef struct {
    /** IRQ mode configuration */
    bs8112a3_irq_oms_t irq_oms;                       /*!< IRQ output mode selection */

    /** Power saving configuration */
    bs8112a3_lsc_mode_t lsc_mode;                     /*!< Low power saving mode selection */

    /** Key12 mode configuration */
    bs8112a3_k12_mode_t k12_mode;                     /*!< Key12 mode selection */

    /** Key threshold configuration (8-63, 6-bit value) */
    uint8_t key_thresholds[BS8112A3_MAX_KEYS];  /*!< Touch threshold for each key (8-63), the smallest threshold means the most sensitive touch detection */

    /** Wake-up configuration for each key */
    /** @note
    The BS81x series chips have two operating modes: standby mode and normal mode.
    If no buttons are touched within 8 seconds of system power-on, the chip automatically enters standby mode to reduce power consumption.
    Once any button is touched (BS8112A-3 and BS8116A-3 have individually configurable wake-up buttons), the BS81x chip is woken up and enters normal mode, displaying the button status.
    After all buttons are released, the chip re-enters standby mode after 8 seconds.
    */
    bool key_wakeup_enable[BS8112A3_MAX_KEYS];  /*!< Wake-up enable for each key (true: enabled, false: disabled) */
} bs8112a3_chip_config_t;

/**
 * @brief Touch interrupt callback function type
 *
 * @note This callback is called from interrupt context (ISR). Do NOT perform I2C operations
 *       or other blocking operations in this callback. Use event group for task notification instead.
 *
 * @param handle Handle returned from touch_ic_bs8112a3_create()
 * @param user_data User data passed in configuration
 */
typedef void (*touch_ic_bs8112a3_interrupt_callback_t)(touch_ic_bs8112a3_handle_t handle, void *user_data);

#define BS8112A3_TOUCH_INTERRUPT_BIT    BIT0    /*!< Event bit for touch interrupt notification */

/**
 * @brief Touch IC BS8112A3 configuration structure
 */
typedef struct {
    /** I2C configuration */
    i2c_master_bus_handle_t i2c_bus_handle; /*!< I2C bus handle from BSP layer */
    uint16_t device_address;                 /*!< I2C device address */
    uint32_t scl_speed_hz;                   /*!< I2C SCL speed in Hz */

    /** GPIO configuration */
    gpio_num_t irq_pin;                      /*!< GPIO pin for IRQ (set to GPIO_NUM_NC to disable) */

    /** Interrupt callback (optional) */
    touch_ic_bs8112a3_interrupt_callback_t interrupt_callback;  /*!< Callback function called on touch interrupt (NULL to disable).
                                                                       Note: Called from ISR context, do NOT perform I2C operations here. */
    void *interrupt_user_data;                                   /*!< User data passed to interrupt callback */

    /** Chip configuration (must not be NULL) */
    const bs8112a3_chip_config_t *chip_config; /*!< Chip-specific configuration, must not be NULL */
} touch_ic_bs8112a3_config_t;

/**
 * @brief Default chip configuration macro
 *
 * Use this macro to initialize a chip configuration structure with default values.
 * Example:
 * @code{c}
 * bs8112a3_chip_config_t chip_cfg = TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG();
 * @endcode
 */
#define TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG() \
    { \
        .irq_oms = BS8112A3_IRQ_OMS_LEVEL_HOLD, \
        .lsc_mode = BS8112A3_LSC_MODE_MORE, \
        .k12_mode = BS8112A3_K12_MODE_KEY12, \
        .key_thresholds = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10}, \
        .key_wakeup_enable = {true, true, true, true, true, true, \
                              true, true, true, true, true, true}, \
    }

/**
 * @brief Create a BS8112A3 touch IC instance
 *
 * @param config Configuration structure containing I2C settings and chip configuration.
 *               chip_config must not be NULL. Use TOUCH_IC_BS8112A3_DEFAULT_CHIP_CONFIG() macro
 *               to initialize default configuration if needed.
 *               If irq_pin is set, a semaphore will be created and signaled when touch interrupt occurs.
 * @param[out] ret_handle Pointer to store the created handle
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments (including NULL chip_config)
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 *      - ESP_ERR_NOT_FOUND: Hardware not responding
 *      - ESP_FAIL: I2C communication failed
 */
esp_err_t touch_ic_bs8112a3_create(const touch_ic_bs8112a3_config_t *config, touch_ic_bs8112a3_handle_t *ret_handle);

/**
 * @brief Delete a BS8112A3 touch IC instance
 *
 * @param handle Handle returned from touch_ic_bs8112a3_create()
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 */
esp_err_t touch_ic_bs8112a3_delete(touch_ic_bs8112a3_handle_t handle);

/**
 * @brief Get current touch button status register value
 *
 * @param handle Handle returned from touch_ic_bs8112a3_create()
 * @param[out] status Pointer to store the 16-bit status register value (bits 0-11 correspond to Key0-Key11)
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid handle or status pointer
 *      - ESP_FAIL: Failed to read key status
 */
esp_err_t touch_ic_bs8112a3_get_key_status(touch_ic_bs8112a3_handle_t handle, uint16_t *status);

/**
 * @brief Get specific touch button state
 *
 * @param handle Handle returned from touch_ic_bs8112a3_create()
 * @param key_index Touch button index (0-11, corresponding to Key0-Key11)
 * @param[out] pressed Pointer to store the button state (true: pressed, false: not pressed)
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid handle or key_index
 *      - ESP_FAIL: Failed to read key status
 */
esp_err_t touch_ic_bs8112a3_get_key_value(touch_ic_bs8112a3_handle_t handle, uint8_t key_index, bool *pressed);

/**
 * @brief Read chip configuration from BS8112A3
 *
 * This function reads the current chip configuration from the hardware and fills
 * the provided configuration structure with the read values.
 *
 * @param handle Handle returned from touch_ic_bs8112a3_create()
 * @param[out] config Pointer to configuration structure to store read values
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid handle or config pointer
 *      - ESP_FAIL: Failed to read configuration or checksum mismatch
 */
esp_err_t touch_ic_bs8112a3_read_config(touch_ic_bs8112a3_handle_t handle, bs8112a3_chip_config_t *config);

/**
 * @brief Get interrupt event group handle
 *
 * @param handle Handle returned from touch_ic_bs8112a3_create()
 * @return EventGroupHandle_t Event group handle (NULL if interrupt is not enabled)
 *
 * @note The event group is automatically created when interrupt is enabled (irq_pin != GPIO_NUM_NC).
 *       Use this event group to wait for touch interrupts in your task.
 *       Wait for BS8112A3_TOUCH_INTERRUPT_BIT to be set.
 *       Example:
 *       @code{c}
 *       EventGroupHandle_t event_group = touch_ic_bs8112a3_get_event_group(handle);
 *       if (event_group) {
 *           EventBits_t bits = xEventGroupWaitBits(event_group, BS8112A3_TOUCH_INTERRUPT_BIT,
 *                                                   pdTRUE, pdFALSE, portMAX_DELAY);
 *           if (bits & BS8112A3_TOUCH_INTERRUPT_BIT) {
 *               // Read touch status here (in task context, not ISR)
 *               uint16_t status;
 *               touch_ic_bs8112a3_get_key_status(handle, &status);
 *           }
 *       }
 *       @endcode
 */
EventGroupHandle_t touch_ic_bs8112a3_get_event_group(touch_ic_bs8112a3_handle_t handle);

#ifdef __cplusplus
}
#endif
