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
#include "esp_modem.h"


/**
 * @brief Finds the command par its symbolic name
 *
 * @param dce Modem DCE object
 * @param command Symbolic name of the command
 *
 * @return Function pointer to the
 */
dce_command_t esp_modem_dce_find_command(esp_modem_dce_t *dce, const char *command);

/**
 * @brief Delete specific command from the list
 *
 * @param dce Modem DCE object
 * @param command Symbolic name of the command to delete
 *
 * @return ESP_OK on success
 */
esp_err_t esp_modem_dce_delete_command(esp_modem_dce_t *dce, const char *command_id);

/**
 * @brief Deletes all commands in the list
 *
 * @param dce Modem DCE object
 *
 * @return ESP_OK on success
 */
esp_err_t esp_modem_dce_delete_all_commands(esp_modem_dce_t *dce);

/**
 * @brief Creates internal structure for holding the command list
 *
 * @return Pointer to the command list struct on success, NULL otherwise
 */
struct esp_modem_dce_cmd_list *esp_modem_command_list_create(void);

#ifdef __cplusplus
}
#endif
