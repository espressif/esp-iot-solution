// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_modem_dce.h"

/**
* @brief Result Code from DCE
*
*/
#define MODEM_RESULT_CODE_SUCCESS "OK"              /*!< Acknowledges execution of a command */
#define MODEM_RESULT_CODE_CONNECT "CONNECT"         /*!< A connection has been established */
#define MODEM_RESULT_CODE_RING "RING"               /*!< Detect an incoming call signal from network */
#define MODEM_RESULT_CODE_NO_CARRIER "NO CARRIER"   /*!< Connection termincated or establish a connection failed */
#define MODEM_RESULT_CODE_ERROR "ERROR"             /*!< Command not recognized, command line maximum length exceeded, parameter value invalid */
#define MODEM_RESULT_CODE_NO_DIALTONE "NO DIALTONE" /*!< No dial tone detected */
#define MODEM_RESULT_CODE_BUSY "BUSY"               /*!< Engaged signal detected */
#define MODEM_RESULT_CODE_NO_ANSWER "NO ANSWER"     /*!< Wait for quiet answer */

/**
 * @brief Specific Timeout Constraint, Unit: millisecond
 *
 */
#define MODEM_COMMAND_TIMEOUT_DEFAULT (500)      /*!< Default timeout value for most commands */
#define MODEM_COMMAND_TIMEOUT_OPERATOR (75000)   /*!< Timeout value for getting operator status */
#define MODEM_COMMAND_TIMEOUT_RESET (60000)   /*!< Timeout value for reset command */
#define MODEM_COMMAND_TIMEOUT_MODE_CHANGE (5000) /*!< Timeout value for changing working mode */
#define MODEM_COMMAND_TIMEOUT_POWEROFF (1000)    /*!< Timeout value for power down */

/**
* @brief Strip the tailed "\r\n"
*
* @param str string to strip
* @param len length of string
*/
static inline void strip_cr_lf_tail(char *str, uint32_t len)
{
    if (str[len - 2] == '\r') {
        str[len - 2] = '\0';
    } else if (str[len - 1] == '\r') {
        str[len - 1] = '\0';
    }
}

/**
 * @brief Synchronization
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   None
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_sync(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Enable or not echo mode of DCE
 *
 * @param dce Modem DCE object
 * @param[in] param   bool casted to (void*): true to enable echo mode, false to disable echo mode
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_set_echo(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Store current parameter setting in the user profile
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   None
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_store_profile(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Set flow control mode of DCE in data mode
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   esp_modem_flow_ctrl_t casted to (void*): flow control mode
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_set_flow_ctrl(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Define PDP context
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   esp_modem_dce_pdp_ctx_t type defining PDP context
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_set_pdp_context(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Hang up
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   None
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_hang_up(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Signal strength structure used as a result to esp_modem_dce_get_signal_quality() API
 */
typedef struct esp_modem_dce_csq_ctx_s {
    int rssi;             //!< Signal strength indication
    int ber;              //!< Channel bit error rate
} esp_modem_dce_csq_ctx_t;

/**
 * @brief Get signal quality
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   None
 * @param[out] result esp_modem_dce_csq_ctx_t type returning rssi and ber values
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_get_signal_quality(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Voltage status structure used as a result to esp_modem_dce_get_battery_status() API
 */
typedef struct esp_modem_dce_cbc_ctx_s {
    int battery_status;     //!< current status in mV
    int bcs;                //!< charge status (-1-Not available, 0-Not charging, 1-Charging, 2-Charging done)
    int bcl;                //!< 1-100% battery capacity, -1-Not available
} esp_modem_dce_cbc_ctx_t;

/**
 * @brief Get battery status
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   None
 * @param[out] result esp_modem_dce_cbc_ctx_t type returning battery status and other fields if available
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_get_battery_status(esp_modem_dce_t *dce, void *param, void *result);



/**
 * @brief Get DCE module IMEI number
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   size_t of output string length (casted to void*), max size the resultant string
 * @param[out] result pointer to the string where the resultant IMEI number gets copied (if size param fits)
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_get_imei_number(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Get DCE module IMSI number
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   size_t of output string length (casted to void*), max size the resultant string
 * @param[out] result pointer to the string where the resultant IMSI number gets copied (if size param fits)
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_get_imsi_number(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Get DCE module name
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   size_t of output string length (casted to void*), max size the resultant string
 * @param[out] result pointer to the string where the resultant module name gets copied (if size param fits)
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_get_module_name(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Get operator name
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   size_t of output string length (casted to void*), max size the resultant string
 * @param[out] result pointer to the string where the resultant operator name gets copied (if size param fits)
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_get_operator_name(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Switch to data mode
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   None
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_set_data_mode(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Resume the data mode when PPP has already been started, but switched back to command
 * mode (typically using the `+++` PPP escape sequence)
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   None
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_resume_data_mode(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Switch to command mode
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   None
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_set_command_mode(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Power-down the module
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   None
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_power_down(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Reset the module
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   None
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_reset(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Checks if the module waits for entering the PIN
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   None
 * @param[out] result pointer to bool indicating READY if set to true
 *                    or the module is waiting for PIN if set to false
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_read_pin(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Enters the PIN number
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   4 character string pointer to the PIN
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_set_pin(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Sets the DCE to temporarily use the baudrate specified
 *
 * @param[in] dce     Modem DCE object
 * @param[in] param   string pointer to char representation of the baudrate
 * @param[out] result None
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_set_baud_temp(esp_modem_dce_t *dce, void *param, void *result);
