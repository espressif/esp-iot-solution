/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "driver/uart.h"
#include "driver/gpio.h"

enum xl330_m077_error_code_t {
    XL330_M077_ERROR_RESULT_FAIL = 0x01,
    XL330_M077_ERROR_INSTRUCTION_FAIL,
    XL330_M077_ERROR_CRC_FAIL,
    XL330_M077_ERROR_DATA_RANGE_ERROR,
    XL330_M077_ERROR_DATA_LENGTH_ERROR,
    XL330_M077_ERROR_DATA_TIMEOUT,
    XL330_M077_ERROR_DATA_LIMIT_ERROR,
    XL330_M077_ERROR_DATA_ACCESS_ERROR,
};

typedef enum {
    XL330_M077_OPERATION_CURRENT = 0x00,
    XL330_M077_OPERATION_VELOCITY = 0x01,
    XL330_M077_OPERATION_POSITION = 0x03,
    XL330_M077_OPERATION_EXTENDED_POSITION = 0x04,
    XL330_M077_OPERATION_CURRENT_BASED_POSITION = 0x05,
    XL330_M077_OPERATION_PWM_CONTROL = 0x06,
} xl330_m077_operation_t;

typedef struct {
    uart_port_t uart_num;
    gpio_num_t tx_pin;
    gpio_num_t rx_pin;
    uint32_t baud_rate;
} xl330_m077_bus_config_t;

typedef void *xl330_m077_bus_handle_t;

/**
 * @brief Initialize the XL330-M077 serial bus
 *
 * @param config Configuration of the XL330-M077 serial bus
 * @return xl330_m077_bus_handle_t Handle to the XL330-M077 serial bus, NULL if failed
 */
xl330_m077_bus_handle_t xl330_m077_bus_init(const xl330_m077_bus_config_t *config);

/**
 * @brief Scan for devices on the XL330-M077 serial bus
 *
 * @param bus_handle Handle to the XL330-M077 serial bus
 * @return esp_err_t ESP_OK if successful, other Error codes if failed
 */
esp_err_t xl330_m077_scan_devices(xl330_m077_bus_handle_t bus_handle);

/**
 * @brief Add a device to the XL330-M077 serial bus
 *
 * @param bus_handle Handle to the XL330-M077 serial bus
 * @param id ID of the device to add
 * @return esp_err_t ESP_OK if successful, other Error codes if failed
 */
esp_err_t xl330_m077_add_device(xl330_m077_bus_handle_t bus_handle, uint8_t id);

/**
 * @brief Read the position of a device on the XL330-M077 serial bus
 *
 * @param bus_handle Handle to the XL330-M077 serial bus
 * @param id ID of the device to read the position of
 * @param position Pointer to the position of the device
 * @return esp_err_t ESP_OK if successful, other Error codes if failed
 */
esp_err_t xl330_m077_read_position(xl330_m077_bus_handle_t bus_handle, uint8_t id, float *position);

/**
 * @brief Ping a device on the XL330-M077 serial bus
 *
 * @param bus_handle Handle to the XL330-M077 serial bus
 * @param id ID of the device to ping
 * @return esp_err_t ESP_OK if successful, other Error codes if failed
 */
esp_err_t xl330_m077_ping(xl330_m077_bus_handle_t bus_handle, uint8_t id);

/**
 * @brief Set the operation mode of a device on the XL330-M077 serial bus
 *
 * @param bus_handle Handle to the XL330-M077 serial bus
 * @param id ID of the device to set the operation mode of
 * @param operation Operation mode to set
 * @return esp_err_t ESP_OK if successful, other Error codes if failed
 */
esp_err_t xl330_m077_set_operation(xl330_m077_bus_handle_t bus_handle, uint8_t id, xl330_m077_operation_t operation);

/**
 * @brief Control the LED of a device on the XL330-M077 serial bus
 *
 * @param bus_handle Handle to the XL330-M077 serial bus
 * @param id ID of the device to control the LED of
 * @param if_on Whether to turn the LED on or off
 * @return esp_err_t ESP_OK if successful, other Error codes if failed
 */
esp_err_t xl330_m077_control_led(xl330_m077_bus_handle_t bus_handle, uint8_t id, bool if_on);

/**
 * @brief Control the torque of a device on the XL330-M077 serial bus
 *
 * @param bus_handle Handle to the XL330-M077 serial bus
 * @param id ID of the device to control the torque of
 * @param if_on Whether to turn the torque on or off
 * @return esp_err_t ESP_OK if successful, other Error codes if failed
 */
esp_err_t xl330_m077_control_torque(xl330_m077_bus_handle_t bus_handle, uint8_t id, bool if_on);

/**
 * @brief Move to a position of a device on the XL330-M077 serial bus
 *
 * @param bus_handle Handle to the XL330-M077 serial bus
 * @param id ID of the device to move to the position of
 * @param position Position to move to
 * @return esp_err_t ESP_OK if successful, other Error codes if failed
 */
esp_err_t xl330_m077_move_to_position(xl330_m077_bus_handle_t bus_handle, uint8_t id, float position);

/**
 * @brief Reboot a device on the XL330-M077 serial bus
 *
 * @param bus_handle Handle to the XL330-M077 serial bus
 * @param id ID of the device to reboot
 * @return esp_err_t ESP_OK if successful, other Error codes if failed
 */
esp_err_t xl330_m077_reboot(xl330_m077_bus_handle_t bus_handle, uint8_t id);
