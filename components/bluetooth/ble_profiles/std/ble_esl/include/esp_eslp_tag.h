/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_event_base.h"
#include "esp_esl.h"
#include "esp_eslp_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @cond **/
/* BLE ESLP EVENTS BASE */
ESP_EVENT_DECLARE_BASE(BLE_ESLP_EVENTS);
/** @endcond **/

/**
 * @brief   ESL Profile Tag Events
 */
typedef enum {
    BLE_ESLP_EVENT_STATE_CHANGED = 1,                                       /*!< Tag state changed */
    BLE_ESLP_EVENT_ADDRESS_WRITTEN,                                         /*!< ESL Address written */
    BLE_ESLP_EVENT_AP_SYNC_KEY_WRITTEN,                                     /*!< AP Sync Key Material written */
    BLE_ESLP_EVENT_RESP_KEY_WRITTEN,                                        /*!< Response Key Material written */
    BLE_ESLP_EVENT_ABS_TIME_WRITTEN,                                        /*!< Current Absolute Time written */
    BLE_ESLP_EVENT_ECP_COMMAND,                                             /*!< ECP command received */
    BLE_ESLP_EVENT_ASSOCIATED,                                              /*!< Association completed */
    BLE_ESLP_EVENT_UNASSOCIATED,                                            /*!< Association cleared */
    BLE_ESLP_EVENT_SYNCHRONIZED,                                            /*!< PAST sync established */
    BLE_ESLP_EVENT_SYNC_LOST,                                               /*!< Periodic sync lost */
    BLE_ESLP_EVENT_PAWR_COMMAND,                                            /*!< Command received on a PAwR report */
    BLE_ESLP_EVENT_IMAGE_WRITTEN,                                           /*!< Image object write completed via OTP */
    BLE_ESLP_EVENT_DISPLAY_IMAGE,                                           /*!< Display a stored image (HW action) */
    BLE_ESLP_EVENT_REFRESH_DISPLAY,                                         /*!< Refresh the current display (HW action) */
    BLE_ESLP_EVENT_LED_CONTROL,                                             /*!< LED control request (HW action) */
    BLE_ESLP_EVENT_SENSOR_READ,                                             /*!< Sensor read request (call report_sensor_data) */
} esp_ble_eslp_event_t;

/**
 * @brief   Display Image Event Data
 */
typedef struct {
    uint8_t display_index;                                                  /*!< Target display index */
    uint8_t image_index;                                                    /*!< Image index to show */
} esp_ble_eslp_display_image_t;

/**
 * @brief   Refresh Display Event Data
 */
typedef struct {
    uint8_t display_index;                                                  /*!< Display to refresh */
} esp_ble_eslp_refresh_display_t;

/**
 * @brief   LED Control Event Data
 */
typedef struct {
    uint8_t led_index;                                                      /*!< Target LED index */
    uint8_t color_red;                                                      /*!< 2-bit red (0-3) */
    uint8_t color_green;                                                    /*!< 2-bit green (0-3) */
    uint8_t color_blue;                                                     /*!< 2-bit blue (0-3) */
    uint8_t brightness;                                                     /*!< 2-bit brightness (0-3) */
    uint8_t flashing_pattern[7];                                            /*!< Pattern(5) + bit_off + bit_on */
    uint8_t repeat_type;                                                    /*!< 0 = count, 1 = duration */
    uint16_t repeats_duration;                                              /*!< Count or duration (seconds) */
    bool is_off;                                                            /*!< True if LED should turn off */
} esp_ble_eslp_led_control_t;

/**
 * @brief   Sensor Read Event Data
 */
typedef struct {
    uint8_t sensor_index;                                                   /*!< Sensor to read */
} esp_ble_eslp_sensor_read_t;

/**
 * @brief   Image Written Event Data
 */
typedef struct {
    uint8_t image_index;                                                    /*!< Image index that was written */
    uint32_t size;                                                          /*!< Final image size in bytes */
    esp_err_t status;                                                       /*!< ESP_OK on success */
} esp_ble_eslp_image_written_t;

/**
 * @brief   ECP Command Event Data
 */
typedef struct {
    uint8_t opcode;                                                         /*!< ECP opcode */
    uint8_t params_len;                                                     /*!< Parameter length */
    uint8_t params[BLE_ESL_ECP_MAX_SIZE];                                   /*!< Parameter bytes */
} esp_ble_eslp_ecp_command_t;

/**
 * @brief   PAwR Command Event Data
 */
typedef struct {
    uint16_t sync_handle;                                                   /*!< Periodic sync handle */
    uint16_t event_counter;                                                 /*!< Periodic event counter */
    uint8_t subevent;                                                       /*!< Subevent index */
    uint8_t data_len;                                                       /*!< ESL Payload length */
    uint8_t data[BLE_ESL_PAYLOAD_MAX_SIZE];                                 /*!< Decrypted ESL Payload */
} esp_ble_eslp_pawr_command_t;

/**
 * @brief   ESL Profile Tag Configuration
 */
typedef struct {
    uint16_t basic_state;                                                   /*!< Initial Basic State bitmap */
    bool skip_auto_ecp_response;                                            /*!< Disable automatic ECP responses */
    bool skip_past_receive;                                                 /*!< Skip enabling PAST receive after association */
    bool skip_auto_pawr_response;                                           /*!< Disable automatic Ping response on PAwR */
} esp_ble_eslp_config_t;

/**
 * @brief ECP command callback
 *
 * @param[in]  cmd  The pointer to store the ECP command
 * @param[in]  priv User context
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_NOT_FINISHED to apply default handling
 *  - Other error codes on failure
 */
typedef esp_err_t (*esp_ble_eslp_ecp_handler_t)(const esp_ble_eslp_ecp_command_t *cmd, void *priv);

/**
 * @brief Service Needed hold callback
 *
 * Called on Service Reset. Return true to keep Service Needed set.
 *
 * @param[in] priv User context
 *
 * @return true to keep Service Needed; false to clear it
 */
typedef bool (*esp_ble_eslp_service_needed_hold_cb_t)(void *priv);

/**
 * @brief Initialization Electronic Shelf Label Profile
 *
 * Call esp_ble_esl_set_display_info() first when CONFIG_BLE_ESL_DISPLAY_INFO is enabled.
 *
 * @param[in]  config The pointer to store the profile configuration
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_eslp_init(const esp_ble_eslp_config_t *config);

/**
 * @brief Deinitialization Electronic Shelf Label Profile
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_deinit(void);

/**
 * @brief Register an ECP command callback
 *
 * @param[in]  handler The callback function
 * @param[in]  priv    User context
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_register_ecp_handler(esp_ble_eslp_ecp_handler_t handler, void *priv);

/**
 * @brief Get the current Tag state
 *
 * @param[out]  state The pointer to store the Tag state
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_get_state(esp_ble_eslp_state_t *state);

/**
 * @brief Get whether the Tag is associated
 *
 * @param[out]  associated The pointer to store the association flag
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_is_associated(bool *associated);

/**
 * @brief Get the Basic State bitmap value
 *
 * @param[out]  basic_state The pointer to store the Basic State bitmap
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_get_basic_state(uint16_t *basic_state);

/**
 * @brief Set the Basic State bitmap value
 *
 * @param[in]  basic_state The Basic State bitmap
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_set_basic_state(uint16_t basic_state);

/**
 * @brief Set or clear the Service Needed bit in Basic State
 *
 * @param[in] needed true to set Service Needed; false to clear it
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_set_service_needed(bool needed);

/**
 * @brief Register a hold callback for Service Reset
 *
 * Service Reset clears Service Needed only when the callback returns false.
 * Pass NULL to unregister.
 *
 * @param[in] cb   Hold callback, or NULL
 * @param[in] priv User context
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_register_service_needed_hold(esp_ble_eslp_service_needed_hold_cb_t cb, void *priv);

/**
 * @brief Indicate an ECP response
 *
 * @param[in]  data The pointer to store the response TLV
 * @param[in]  len  The length of response TLV
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_indicate_ecp_response(const uint8_t *data, uint16_t len);

/**
 * @brief Indicate a Basic State response
 *
 * @param[in]  basic_state The pointer to store the Basic State bitmap
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_indicate_basic_state(const uint16_t *basic_state);

/**
 * @brief Indicate an Error response
 *
 * @param[in]  error_code One of BLE_ESL_ERR_*
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_indicate_error(uint8_t error_code);

/**
 * @brief Indicate a Display State response
 *
 * @param[in]  display_index The display index
 * @param[in]  image_index   The image index
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_indicate_display_state(uint8_t display_index, uint8_t image_index);

/**
 * @brief Indicate an LED State response
 *
 * @param[in]  led_index The LED index
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_indicate_led_state(uint8_t led_index);

/**
 * @brief Complete a Sensor Read after BLE_ESLP_EVENT_SENSOR_READ
 *
 * @param[in]  sensor_index Sensor index
 * @param[in]  error_code   0 on success; otherwise BLE_ESL_ERR_*
 * @param[in]  data         The pointer to store the sensor payload
 * @param[in]  data_len     The length of sensor payload
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_report_sensor_data(uint8_t sensor_index, uint8_t error_code,
                                          const uint8_t *data, uint8_t data_len);

/**
 * @brief Get a stored ESL image buffer
 *
 * @param[in]  image_index Image index
 * @param[out] data        The pointer to store the image buffer pointer
 * @param[out] len         The pointer to store the image length
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 *  - ESP_ERR_NOT_FOUND if the image slot is empty
 *  - ESP_ERR_NOT_SUPPORTED if OTP Object Server is disabled
 */
esp_err_t esp_ble_eslp_get_image(uint8_t image_index, const uint8_t **data, uint32_t *len);

#ifdef __cplusplus
}
#endif
