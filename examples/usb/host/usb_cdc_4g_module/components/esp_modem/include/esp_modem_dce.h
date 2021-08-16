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

#include "esp_types.h"
#include "esp_err.h"
#include "esp_modem.h"
#include "esp_modem_dte.h"



/**
 * @brief Forward declaration of the command list object, which (if enabled) is used
 * to populate a command palette and access commands by its symbolic name
 */
struct esp_modem_dce_cmd_list;

/**
 * @brief Working state of DCE
 *
 */
typedef enum {
    ESP_MODEM_STATE_PROCESSING, /*!< In processing */
    ESP_MODEM_STATE_SUCCESS,    /*!< Process successfully */
    ESP_MODEM_STATE_FAIL        /*!< Process failed */
} esp_modem_state_t;

/**
 * @brief Generic command type used in DCE unit
 */
typedef esp_err_t (*dce_command_t)(esp_modem_dce_t *dce, void *param, void *result);

/**
 * @brief Type of line handlers called fro DTE upon line response reception
 */
typedef esp_err_t (*esp_modem_dce_handle_line_t)(esp_modem_dce_t *dce, const char *line);



/**
 * @brief DCE(Data Communication Equipment)
 *
 */
struct esp_modem_dce {
    esp_modem_state_t state;                                                    /*!< Modem working state */
    esp_modem_mode_t mode;                                                      /*!< Working mode */
    esp_modem_dte_t *dte;                                                       /*!< DTE which connect to DCE */
    struct esp_modem_dce_cmd_list *dce_cmd_list;
    esp_modem_dce_config_t config;
    esp_modem_dce_handle_line_t handle_line;                                    /*!< Handle line strategy */

    void *handle_line_ctx;                                                      /*!< DCE context reserved for handle_line
                                                                                    processing */
    // higher level actions DCE unit can take
    esp_err_t (*set_working_mode)(esp_modem_dce_t *dce, esp_modem_mode_t mode); /*!< Set working mode */
    esp_err_t (*deinit)(esp_modem_dce_t *dce);                                  /*!< Destroys the DCE */
    esp_err_t (*start_up)(esp_modem_dce_t *dce);                                /*!< Start-up sequence */

    // list of essential commands for esp-modem basic work
    dce_command_t hang_up;                                                      /*!< generic command for hang-up */
    dce_command_t set_pdp_context;                                              /*!< generic command for pdp context */
    dce_command_t set_data_mode;                                                /*!< generic command for data mode */
    dce_command_t resume_data_mode;                                             /*!< generic command to resume already dialed data mode */
    dce_command_t set_command_mode;                                             /*!< generic command for command mode */
    dce_command_t set_echo;                                                     /*!< generic command for echo mode */
    dce_command_t sync;                                                         /*!< generic command for sync */
    dce_command_t set_flow_ctrl;                                                /*!< generic command for flow-ctrl */
    dce_command_t store_profile;                                                /*!< generic command for store-profile */
};


// DCE commands building blocks
/**
 * @brief Sending generic command to DCE
 *
 * @param[in] dce     Modem DCE object
 * @param[in] command String command
 * @param[in] timeout Command timeout in ms
 * @param[in] handle_line Function ptr which processes the command response
 * @param[in] ctx         Function ptr context
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_dce_generic_command(esp_modem_dce_t *dce, const char * command, uint32_t timeout, esp_modem_dce_handle_line_t handle_line, void *ctx);

/**
 * @brief Indicate that processing current command has done
 *
 * @param dce Modem DCE object
 * @param state Modem state after processing
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t esp_modem_process_command_done(esp_modem_dce_t *dce, esp_modem_state_t state);

/**
 * @brief Default handler for response
 * Some responses for command are simple, commonly will return OK when succeed of ERROR when failed
 *
 * @param dce Modem DCE object
 * @param line line string
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t esp_modem_dce_handle_response_default(esp_modem_dce_t *dce, const char *line);


// DCE higher level commands
/**
 * @brief Set Working Mode
 *
 * @param dce Modem DCE object
 * @param mode working mode
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t esp_modem_dce_set_working_mode(esp_modem_dce_t *dce, esp_modem_mode_t mode);

/**
 * @brief Default start-up sequence, which sets the modem into an operational mode
 *
 * @param dce Modem DCE object
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t esp_modem_dce_default_start_up(esp_modem_dce_t *dce);

/**
 * @brief Destroys the DCE
 *
 * @param dce Modem DCE object
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t esp_modem_dce_default_destroy(esp_modem_dce_t *dce);

/**
 * @brief Initializes the DCE
 *
 * @param dce Modem DCE object
 * @param config DCE configuration structure
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t esp_modem_dce_default_init(esp_modem_dce_t *dce, esp_modem_dce_config_t* config);

/**
 * @brief Sets the DCE parameters. This API updates runtime DCE config parameters,
 * typically used to update PDP context data.
 *
 * @param dce Modem DCE object
 * @param config DCE configuration structure
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t esp_modem_dce_set_params(esp_modem_dce_t *dce, esp_modem_dce_config_t* config);


// list command operations
/**
 * @brief Executes a specific command from the list
 *
 * @param dce       Modem DCE object
 * @param command   Symbolic name of the command to execute
 * @param param     Generic parameter to the command
 * @param result    Generic output parameter
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_TIMEOUT if timeout while waiting for expected response
 */
esp_err_t esp_modem_command_list_run(esp_modem_dce_t *dce, const char * command, void * param, void* result);

/**
 * @brief Deinitialize the command list
 *
 * @param dce       Modem DCE object
 *
 * @return ESP_OK on success
 */
esp_err_t esp_modem_command_list_deinit(esp_modem_dce_t *dce);

/**
 * @brief Initializes default command list with predefined command palette
 *
 * @param dce       Modem DCE object
 *
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t esp_modem_set_default_command_list(esp_modem_dce_t *dce);

/**
 * @brief Add or set specific command to the command list
 *
 * @param dce        Modem DCE object
 * @param command_id Command symbolic name
 * @param command    Generic command function pointer
 *
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t esp_modem_command_list_set_cmd(esp_modem_dce_t *dce, const char * command_id, dce_command_t command);

#ifdef __cplusplus
}
#endif
