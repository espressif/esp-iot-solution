// Copyright 2018-2020 Espressif Systems (Shanghai) PTE LTD
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
#include "esp_event.h"

/**
 * @brief Working mode of Modem
 *
 */
typedef enum {
    ESP_MODEM_COMMAND_MODE = 0, /*!< Command Mode */
    ESP_MODEM_PPP_MODE,         /*!< PPP Mode */
    ESP_MODEM_TRANSITION_MODE   /*!< Transition Mode between data and command mode indicating that
                                 the modem is not yet ready for sending commands nor data */
} esp_modem_mode_t;

/**
 * @brief DTE(Data Terminal Equipment)
 *
 */
struct esp_modem_dte {
    esp_modem_flow_ctrl_t flow_ctrl;                                                    /*!< Flow control of DTE */
    esp_modem_dce_t *dce;                                                               /*!< DCE which connected to the DTE */
    struct esp_modem_netif_driver_s *netif_adapter;
    esp_err_t (*send_cmd)(esp_modem_dte_t *dte, const char *command, uint32_t timeout); /*!< Send command to DCE */
    int (*send_data)(esp_modem_dte_t *dte, const char *data, uint32_t length);          /*!< Send data to DCE */
    esp_err_t (*send_wait)(esp_modem_dte_t *dte, const char *data, uint32_t length,
                           const char *prompt, uint32_t timeout);      /*!< Wait for specific prompt */
    esp_err_t (*change_mode)(esp_modem_dte_t *dte, esp_modem_mode_t new_mode); /*!< Changing working mode */
    esp_err_t (*process_cmd_done)(esp_modem_dte_t *dte);                   /*!< Callback when DCE process command done */
    esp_err_t (*deinit)(esp_modem_dte_t *dte);                             /*!< Deinitialize */
};

/**
 * @brief Type used for reception callback
 *
 */
typedef esp_err_t (*esp_modem_on_receive)(void *buffer, size_t len, void *context);

/**
 * @brief Setup on reception callback
 *
 * @param dte ESP Modem DTE object
 * @param receive_cb Function pointer to the reception callback
 * @param receive_cb_ctx Contextual pointer to be passed to the reception callback
 *
 * @return ESP_OK on success
 */
esp_err_t esp_modem_set_rx_cb(esp_modem_dte_t *dte, esp_modem_on_receive receive_cb, void *receive_cb_ctx);

/**
 * @brief Notify the modem, that ppp netif has closed
 *
 * @note This API should only be used internally by the modem-netif layer
 *
 * @param dte ESP Modem DTE object
 *
 * @return ESP_OK on success
 */
esp_err_t esp_modem_notify_ppp_netif_closed(esp_modem_dte_t *dte);

/**
 * @brief Notify the modem, that all the modem units (DTE, DCE, PPP) has
 * been properly initialized and DTE loop can safely start
 *
 * @param dte ESP Modem DTE object
 *
 * @return ESP_OK on success
 */
esp_err_t esp_modem_notify_initialized(esp_modem_dte_t *dte);

/**
 * @brief Configure runtime parameters for the DTE. Currently supports only the baud rate to be set
 *
 * @param dte ESP Modem DTE object
 *
 * @return ESP_OK on success
 */
esp_err_t esp_modem_dte_set_params(esp_modem_dte_t *dte, const esp_modem_dte_config_t *config);

#ifdef __cplusplus
}
#endif
