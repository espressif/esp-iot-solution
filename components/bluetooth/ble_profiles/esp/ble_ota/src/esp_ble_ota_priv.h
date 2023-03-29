/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifndef _ESP_BLE_OTA_PRIV_H_
#define _ESP_BLE_OTA_PRIV_H_

#ifdef CONFIG_OTA_WITH_PROTOCOMM

#include <protocomm.h>
#include <protocomm_security.h>

#include "manager.h"
#include "ota.h"

/**
 * @brief   Notify manager that ota is done
 *
 * Stops the ota. This is called by the get_status_handler()
 * when the status is connected.
 *
 * @return
 *  - ESP_OK      : OTA will be stopped
 *  - ESP_FAIL    : Failed to stop ota
*/
esp_err_t esp_ble_ota_mgr_done(void);

/**
 * @brief   Get protocomm handlers for ota endpoint
 *
 * @param[in] ptr   pointer to structure to be set
 *
 * @return
 *  - ESP_OK   : success
 *  - ESP_ERR_INVALID_ARG : null argument
*/
esp_err_t get_ota_handlers(ota_handlers_t *ptr);

#endif /* CONFIG_OTA_WITH_PROTOCOMM */

#endif
