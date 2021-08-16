// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
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

/**
 * @brief modem netif adapter type
 *
 */
typedef struct esp_modem_netif_driver_s esp_modem_netif_driver_t;

/**
 * @defgroup ESP_MODEM_NETIF Modem netif adapter API
 * @brief  network interface adapter for esp-modem
 */

/** @addtogroup ESP_MODEM_NETIF
 * @{
 */

/**
 * @brief Creates handle to esp_modem used as an esp-netif driver
 *
 * @param dte ESP Modem DTE object
 *
 * @return opaque pointer to esp-modem IO driver used to attach to esp-netif
 */
esp_modem_netif_driver_t *esp_modem_netif_new(esp_modem_dte_t *dte);

/**
 * @brief Destroys the esp-netif driver handle as well as the internal netif
 * object attached to it
 *
 * @param h pointer to the esp-netif adapter for esp-modem
 */
void esp_modem_netif_destroy(esp_modem_netif_driver_t *h);

/**
 * @brief Clears default handlers for esp-modem lifecycle
 *
 * @param h pointer to the esp-netif adapter for esp-modem
 */
esp_err_t esp_modem_netif_clear_default_handlers(esp_modem_netif_driver_t *h);

/**
 * @brief Setups default handlers for esp-modem lifecycle
 *
 * @param h pointer to the esp-netif adapter for esp-modem
 * @param esp_netif pointer corresponding esp-netif instance
 */
esp_err_t esp_modem_netif_set_default_handlers(esp_modem_netif_driver_t *h, esp_netif_t * esp_netif);

/**
 * @}
 */


/**
 * @defgroup ESP_MODEM_NETIF_LEGACY Modem netif adapter legacy API
 * @brief  Legacy API for modem netif
 */

/** @addtogroup ESP_MODEM_NETIF_LEGACY
 * @{
 */

/**
 * @brief Destroys the esp-netif driver handle the same way
 * as esp_modem_netif_destroy()
 *
 * @note This API is only provided for legacy API
 */
void esp_modem_netif_teardown(esp_modem_netif_driver_t *h);

/**
 * @brief The same as `esp_modem_netif_new()`, but autostarts the netif
 * on esp_netif_attach().
 *
 * @note This API is only provided for legacy API
 */
esp_modem_netif_driver_t *esp_modem_netif_setup(esp_modem_dte_t *dte);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
