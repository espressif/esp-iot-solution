/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "modem_at_parser.h"

typedef struct {
    size_t cid;             /*!< PDP context identifier */
    const char *type;       /*!< Protocol type */
    const char *apn;        /*!< Modem APN (Access Point Name, a logical name to choose data network) */
} esp_modem_at_pdp_t;

typedef struct {
    int n;

    /*
    0 not registered, MT is not currently searching an operator to register to
    1 registered, home network
    2 not registered, but MT is currently trying to attach or searching an operator to register to
    3 registration denied
    */
    int stat;          //!<  indicates the EPS registration status
} esp_modem_at_cereg_t;

typedef struct {
    int rssi;          //!< Signal strength indication, 99 not known or not detectable
    int ber;           //!< Channel bit error rate, 99 not known or not detectable
} esp_modem_at_csq_t;

typedef enum {
    PIN_UNKNOWN,      // SIM in unknown state
    PIN_READY,       // SIM is ready
    PIN_SIM_PIN,     // SIM PIN is required
    PIN_SIM_PIN2,    // SIM PIN2 is required
    PIN_SIM_PUK,     // SIM PUK is required
    PIN_SIM_PUK2,    // SIM PUK2 is required
} esp_modem_pin_state_t;

/**
 * @brief Send AT command and check if response is OK
 *
 * @param[in] at_handle  AT handle
 * @param[in] command    AT command string to send
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_send_command_response_ok(at_handle_t at_handle, const char *command);

/**
 * @brief Send AT command to check if modem is ready
 *
 * @param[in] at_handle AT handle
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_at(at_handle_t at_handle);

/**
 * @brief Set the modem echo mode
 *
 * @param[in] at_handle AT handle
 * @param[in] echo_on true to enable echo, false to disable echo
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_set_echo(at_handle_t at_handle, bool echo_on);

/**
 * @brief Store the current profile to non-volatile memory
 *
 * @param[in] at_handle AT handle
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_store_profile(at_handle_t at_handle);

/**
 * @brief Set PDP context
 *
 * @param[in] at_handle AT handle
 * @param[in] pdp Pointer to PDP context structure
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_NO_MEM if memory allocation fails
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_set_pdp_context(at_handle_t at_handle, const esp_modem_at_pdp_t *pdp);

/**
 * @brief Get PDP context information
 *
 * @param[in] at_handle AT handle
 * @param[out] res_buffer Buffer to store the response
 * @param[in] res_buffer_len Length of the buffer
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_get_pdp_context(at_handle_t at_handle, char *res_buffer, size_t res_buffer_len);

/**
 * @brief Get signal quality information
 *
 * @param[in] at_handle AT handle
 * @param[out] ret_csq Pointer to store signal quality information
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_get_signal_quality(at_handle_t at_handle, esp_modem_at_csq_t *ret_csq);

/**
 * @brief Get manufacturer identification
 *
 * @param[in] at_handle AT handle
 * @param[out] res_buffer Buffer to store the manufacturer ID
 * @param[in] res_buffer_len Length of the buffer
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_get_manufacturer_id(at_handle_t at_handle, char *res_buffer, size_t res_buffer_len);

/**
 * @brief Get model identification
 *
 * @param[in] at_handle AT handle
 * @param[out] res_buffer Buffer to store the model ID
 * @param[in] res_buffer_len Length of the buffer
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_get_module_id(at_handle_t at_handle, char *res_buffer, size_t res_buffer_len);

/**
 * @brief Get revision identification
 *
 * @param[in] at_handle AT handle
 * @param[out] res_buffer Buffer to store the revision ID
 * @param[in] res_buffer_len Length of the buffer
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_get_revision_id(at_handle_t at_handle, char *res_buffer, size_t res_buffer_len);

/**
 * @brief Get IMEI number
 *
 * @param[in] at_handle AT handle
 * @param[out] res_buffer Buffer to store the IMEI number
 * @param[in] res_buffer_len Length of the buffer
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_get_imei_number(at_handle_t at_handle, char *res_buffer, size_t res_buffer_len);

/**
 * @brief Get IMSI number
 *
 * @param[in] at_handle AT handle
 * @param[out] res_buffer Buffer to store the IMSI number
 * @param[in] res_buffer_len Length of the buffer
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_get_imsi_number(at_handle_t at_handle, char *res_buffer, size_t res_buffer_len);

/**
 * @brief Get operator name
 *
 * @param[in] at_handle AT handle
 * @param[out] res_buffer Buffer to store the operator name
 * @param[in] res_buffer_len Length of the buffer
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_get_operator_name(at_handle_t at_handle, char *res_buffer, size_t res_buffer_len);

/**
 * @brief Get network registration status
 *
 * @param[in] at_handle AT handle
 * @param[out] result Pointer to store network registration status
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_get_network_reg_status(at_handle_t at_handle, esp_modem_at_cereg_t *result);

/**
 * @brief Make a data call to enter PPP mode
 *
 * @param[in] at_handle AT handle
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_FAIL if failed to make the call
 */
esp_err_t at_cmd_make_call(at_handle_t at_handle);

/**
 * @brief Hang up a call
 *
 * @param[in] at_handle AT handle
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_hang_up_call(at_handle_t at_handle);

/**
 * @brief Resume data mode (PPP mode)
 *
 * @param[in] at_handle AT handle
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_FAIL if failed to resume data mode
 */
esp_err_t at_cmd_resume_data_mode(at_handle_t at_handle);

/**
 * @brief Exit data mode
 *
 * @param[in] at_handle AT handle
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_exit_data_mode(at_handle_t at_handle);

/**
 * @brief Read SIM PIN state
 *
 * @param[in] at_handle AT handle
 * @param[out] result Pointer to store PIN state
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_read_pin(at_handle_t at_handle, esp_modem_pin_state_t *result);

/**
 * @brief Set SIM PIN
 *
 * @param[in] at_handle AT handle
 * @param[in] pin PIN code (4 digits)
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid argument
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t at_cmd_set_pin(at_handle_t at_handle, const char *pin);

#ifdef __cplusplus
}
#endif
