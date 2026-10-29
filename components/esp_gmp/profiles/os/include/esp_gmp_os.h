/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_gmp.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OS capability bits
 *
 * These bits are registered by individual profiles (OTA, FT, etc.)
 * at runtime to indicate their availability.
 */
#define ESP_GMP_OS_CAP_OTA_SUPPORTED    (1 << 0)  /**< OTA profile available */
#define ESP_GMP_OS_CAP_FT_SUPPORTED     (1 << 1)  /**< File Transfer profile available */
#define ESP_GMP_OS_CAP_CUSTOM_1         (1 << 2)  /**< Reserved for custom use */
#define ESP_GMP_OS_CAP_CUSTOM_2         (1 << 3)  /**< Reserved for custom use */
#define ESP_GMP_OS_CAP_CUSTOM_3         (1 << 4)  /**< Reserved for custom use */
#define ESP_GMP_OS_CAP_CUSTOM_4         (1 << 5)  /**< Reserved for custom use */
#define ESP_GMP_OS_CAP_CUSTOM_5         (1 << 6)  /**< Reserved for custom use */
#define ESP_GMP_OS_CAP_CUSTOM_6         (1 << 7)  /**< Reserved for custom use */

/**
 * @brief Initialize OS profile
 *
 * Registers OS profile packet handler with GMP core.
 * Call once before using OS profile features.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_gmp_os_init(void);

/**
 * @brief Deinitialize OS profile
 *
 * Unregisters OS profile packet handler and cleans up resources.
 */
void esp_gmp_os_deinit(void);

/**
 * @brief Register a capability bit at runtime
 *
 * Profiles call this during their init to indicate availability.
 * Multiple profiles can register different bits.
 *
 * Example:
 * @code
 * // In esp_gmp_ota_init()
 * esp_gmp_os_register_capability(ESP_GMP_OS_CAP_OTA_SUPPORTED);
 * @endcode
 *
 * @param cap_bit Capability bit mask (e.g., ESP_GMP_OS_CAP_OTA_SUPPORTED)
 */
void esp_gmp_os_register_capability(uint8_t cap_bit);

/**
 * @brief Unregister a capability bit at runtime
 *
 * Profiles call this during their deinit to indicate unavailability.
 *
 * Example:
 * @code
 * // In esp_gmp_ota_deinit()
 * esp_gmp_os_unregister_capability(ESP_GMP_OS_CAP_OTA_SUPPORTED);
 * @endcode
 *
 * @param cap_bit Capability bit mask (e.g., ESP_GMP_OS_CAP_OTA_SUPPORTED)
 */
void esp_gmp_os_unregister_capability(uint8_t cap_bit);

/**
 * @brief Get current capability bits (for internal use)
 *
 * @return Current capability bit mask
 */
uint8_t esp_gmp_os_get_capabilities(void);

#ifdef __cplusplus
}
#endif
