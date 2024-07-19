/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "drv10987_config.h"
#include "drv10987_fault.h"
#include "esp_err.h"
#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_bus_handle_t bus;
    uint8_t dev_addr;
    gpio_num_t dir_pin;
} drv10987_config_t;

typedef struct drv10987_t *drv10987_handle_t;

#define DRV10987_I2C_ADDRESS_DEFAULT   (0x52)

/**
 * @brief Create and init object and return a handle
 *
 * @param handle Pointer to the DRV10987 handle
 * @param config Pointer to configuration
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_create(drv10987_handle_t *handle, drv10987_config_t *config);

/**
 * @brief Deinit object and free memory
 *
 * @param handle Pointer to the DRV10987 handle
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_delete(drv10987_handle_t handle);

/**
 * @brief Disable control for the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_disable_control(drv10987_handle_t handle);

/**
 * @brief Enable control for the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_enable_control(drv10987_handle_t handle);

/**
 * @brief Write to a configuration register of the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 * @param drv10987_config_register_addr_t Address of the configuration register to write to
 * @param config Pointer to the configuration data to be written
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_write_config_register(drv10987_handle_t handle, drv10987_config_register_addr_t config_register_addr, void *config);

/**
 * @brief Write access to EEPROM of the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 * @param maximum_failure_count Maximum allowable failure count for EEPROM read attempts
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_write_access_to_eeprom(drv10987_handle_t handle, uint8_t maximum_failure_count);

/**
 * @brief Enable mass write access to EEPROM of the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_mass_write_acess_to_eeprom(drv10987_handle_t handle);

/**
 * @brief Write the target speed to the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 * @param speed target speed value to be written (It is worth noting that this speed value is a 9bit piece of data, so the maximum value is 511)
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_write_speed(drv10987_handle_t handle, uint16_t speed);

/**
 * @brief Read the EEPROM status from the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 *
 * @return
 *     - EEPROM_NO_READY EEPROM not ready for read/write access
 *     - EEPROM_READY EEPROM ready for read/write access
 */
drv10987_eeprom_status_t drv10987_read_eeprom_status(drv10987_handle_t handle);

/**
 * @brief Read the driver status from the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 * @param fault Pointer to fault
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_read_driver_status(drv10987_handle_t handle, drv10987_fault_t *fault);

/**
 * @brief Read all configuration registers from the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 *
 * @return
 *     - drv10987_config_reg_t All values of the configuration registers
 */
drv10987_config_reg_t drv10987_read_config_register(drv10987_handle_t handle);

/**
 * @brief Read motor phase resistance and kt from the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 * @param phase_res Pointer to phase resistance
 * @param kt Pointer to kt
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_read_phase_resistance_and_kt(drv10987_handle_t handle, float *phase_res, float *kt);

/**
 * @brief Clear any driver faults in the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_clear_driver_fault(drv10987_handle_t handle);

/**
 * @brief  Set the motor direction uvw for the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_set_direction_uvw(drv10987_handle_t handle);

/**
 * @brief  Set the motor direction uwv for the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_set_direction_uwv(drv10987_handle_t handle);

/**
 * @brief Perform an EEPROM test on the DRV10987 motor driver
 *
 * @param handle Pointer to the DRV10987 handle
 * @param config Configuration data to be used for the EEPROM test
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t drv10987_eeprom_test(drv10987_handle_t handle, drv10987_config1_t config1, drv10987_config2_t config2);

#ifdef __cplusplus
}
#endif
