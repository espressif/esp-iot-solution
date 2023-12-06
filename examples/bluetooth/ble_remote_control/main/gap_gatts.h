/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string.h> // memcpy

#include "esp_log.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"

// Example include
#include "hidd.h"

/**
 * @brief   GAP GATT Server Profile
 * @details Store information about the server profile for easier access (such as for tear down)
 * @details Allows for multiple profiles to be registered
*/
typedef struct {
    uint16_t gatts_interface;
    uint16_t app_id;
    uint16_t conn_id;
    esp_gatts_cb_t callback;
    esp_bd_addr_t remote_bda;
} gatts_profile_inst_t;

/**
 * @brief   Sequence to tear down the GATTS Profile and GAP resources
*/
esp_err_t gap_gatts_deinit(void);

/****************************************************************************************
 * Generic Access Profile - GAP Related Functions
****************************************************************************************/

/**
 * @brief   Initialize the GAP security parameters
*/
void gap_set_security(void);

/**
 * @brief   Callback function for GAP events
 * @param[in] event     GAP event type (see esp_gap_ble_api.h)
 * @param[in] param     GAP event parameters (differ based on event type)
*/
void gap_event_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

/****************************************************************************************
 * Generic ATTribute Profile (Server) - GATTS Related Functions
****************************************************************************************/

/**
 * @brief   Callback function for GATTS events
 * @param[in] event         GATTS event type (see esp_gatts_api.h)
 * @param[in] gatts_if      GATTS interface to register the profile with
 * @param[in] param         GATTS event parameters (differ based on event type)
*/
void gatts_event_callback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                          esp_ble_gatts_cb_param_t *param);
