/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

/**
 * @brief Initialize ESP-NOW communication
 *
 */
void app_espnow_init(void);

/**
 * @brief Send raw data via ESP-NOW
 *
 * This function sends raw data to the configured slave device using ESP-NOW protocol.
 *
 * @param data Pointer to the data buffer to send
 * @param length Length of the data in bytes
 * @return esp_err_t ESP_OK if successful, other Error codes if failed
 */
esp_err_t app_espnow_send(const uint8_t *data, size_t length);

/**
 * @brief Send servo angle data via ESP-NOW
 *
 * This function packs and sends servo angle data to the slave device.
 * The data packet format is:
 * [0xFF][0xFF][Servo1_Low][Servo1_High]...[Servo7_Low][Servo7_High][CRC_Low][CRC_High]
 * Each angle is encoded as int16_t (radians multiplied by 100 for 0.01 radian precision)
 * Input range: -180.00 to 180.00 degrees -> -π to π radians -> -314 to 314 (approximately)
 *
 * @param servo_angles Array of servo angles (float values in degrees, can be positive or negative, range: -180.0 to 180.0)
 * @param angles_count Number of angles in the array (should be MAX_SERVO_NUM)
 * @return esp_err_t ESP_OK if successful, other Error codes if failed
 */
esp_err_t app_espnow_send_servo_data(const float *servo_angles, size_t angles_count);
