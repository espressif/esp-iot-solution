/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * IDF-like ESL AP façade used by the backported product example.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "ble_esl_common.h"
#include "esp_eslp_ap.h"
#include "esp_ble_conn_mgr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BLE_ESL_AP_EVT_SCAN_RESULT = 1,
    BLE_ESL_AP_EVT_CONNECTED,
    BLE_ESL_AP_EVT_DISCONNECTED,
    BLE_ESL_AP_EVT_ESL_INFO,
    BLE_ESL_AP_EVT_CONFIGURED,
    BLE_ESL_AP_EVT_IMAGE_TRANSFERRED,
    BLE_ESL_AP_EVT_SYNCHRONIZED,
    BLE_ESL_AP_EVT_RESPONSE,
    BLE_ESL_AP_EVT_CMD_TIMEOUT,
    BLE_ESL_AP_EVT_CMD_FAILED,
    BLE_ESL_AP_EVT_ABS_TIME_WRITTEN,
    BLE_ESL_AP_EVT_STATE_CHANGED,
    BLE_ESL_AP_EVT_CONNECT_FAILED,
} ble_esl_ap_evt_t;

typedef void (*ble_esl_ap_callback_t)(ble_esl_ap_evt_t event, void *param);

typedef struct {
    uint16_t periodic_adv_interval_min;
    uint16_t periodic_adv_interval_max;
    uint8_t num_subevents;
    uint8_t subevent_interval;
    uint8_t response_slot_delay;
    uint8_t response_slot_spacing;
    uint8_t num_response_slots;
} ble_esl_ap_pawr_params_t;

typedef struct {
    ble_esl_ap_callback_t callback;
    ble_esl_ap_pawr_params_t pawr_config;
} ble_esl_ap_config_t;

typedef struct {
    uint8_t addr[6];
    uint8_t addr_type;
    int8_t rssi;
    bool is_associated;
    bool data_complete;
    bool is_connectable;
    bool advertises_esl_service;
    const uint8_t *adv_data;
    uint16_t adv_data_len;
    uint8_t adv_data_storage[ESP_BLE_CONN_SCAN_RESULT_ADV_MAX_LEN];
} ble_esl_ap_scan_result_t;

typedef struct {
    uint16_t conn_handle;
    uint8_t addr[6];
    uint8_t addr_type;
    bool has_display_info;
    bool has_led_info;
    bool has_sensor_info;
} ble_esl_ap_conn_info_t;

typedef struct {
    uint16_t conn_handle;
    uint16_t reason;
    uint8_t addr[6];
} ble_esl_ap_disconn_info_t;

typedef struct {
    int status;
    uint8_t addr[6];
    uint8_t addr_type;
} ble_esl_ap_connect_failed_t;

typedef struct {
    uint16_t conn_handle;
    void *displays;
    void *sensors;
    void *leds;
} ble_esl_ap_esl_info_t;

typedef struct {
    uint16_t conn_handle;
    int status;
} ble_esl_ap_configured_t;

typedef struct {
    uint16_t conn_handle;
    uint8_t image_index;
    esp_err_t status;
} ble_esl_ap_image_transferred_t;

typedef struct {
    uint16_t conn_handle;
    ble_esl_address_t esl_addr;
    esp_err_t status;
} ble_esl_ap_synchronized_t;

typedef struct {
    uint16_t conn_handle;
    esp_err_t status;
} ble_esl_ap_abs_time_written_t;

typedef struct {
    ble_esl_address_t esl_addr;
    ble_esl_state_t old_state;
    ble_esl_state_t new_state;
} ble_esl_ap_state_changed_t;

typedef struct {
    uint8_t esl_id;
    uint8_t group_id;
    int type;
    struct {
        const uint8_t *data;
        uint8_t data_len;
    } sensor_value;
    struct {
        const uint8_t *data;
        uint8_t data_len;
    } vendor;
} esl_ap_response_t;

typedef struct {
    uint8_t esl_id;
    uint8_t group_id;
} esl_ap_cmd_timeout_t;

typedef struct {
    uint8_t esl_id;
    uint8_t group_id;
    esp_err_t status;
} esl_ap_cmd_failed_t;

typedef esp_ble_eslp_ap_led_settings_t esl_ap_led_settings_t;

typedef struct {
    uint8_t esl_id;
    uint8_t group_id;
    ble_esl_key_material_t ap_sync_key;
    ble_esl_key_material_t resp_key;
} ble_esl_ap_esl_config_t;

typedef struct {
    uint16_t conn_handle;
    uint8_t image_index;
    const uint8_t *data;
    uint32_t data_len;
    bool truncate;
} ble_esl_ap_image_transfer_params_t;

typedef struct {
    ble_esl_address_t esl_address;
    uint8_t ble_addr_type;
    uint8_t ble_addr[6];
    ble_esl_key_material_t ap_sync_key;
    ble_esl_key_material_t resp_key;
} ble_esl_ap_persisted_esl_t;

esp_err_t ble_esl_ap_init(const ble_esl_ap_config_t *config);
esp_err_t ble_esl_ap_start_scan(void);
esp_err_t ble_esl_ap_stop_scan(void);
esp_err_t ble_esl_ap_start_pawr(void);
esp_err_t ble_esl_ap_stop_pawr(void);
esp_err_t ble_esl_ap_connect(const uint8_t addr[6], uint8_t addr_type);
esp_err_t ble_esl_ap_connect_synced(ble_esl_address_t addr);
esp_err_t ble_esl_ap_disconnect(uint16_t conn_handle);
void ble_esl_ap_abort_tag_connections(void);
esp_err_t ble_esl_ap_read_info(uint16_t conn_handle);
esp_err_t ble_esl_ap_configure(uint16_t conn_handle, const ble_esl_ap_esl_config_t *cfg);
esp_err_t ble_esl_ap_transfer_image(const ble_esl_ap_image_transfer_params_t *params);
esp_err_t ble_esl_ap_synchronize(uint16_t conn_handle);
bool ble_esl_ap_synchronize_in_progress(void);
esp_err_t ble_esl_ap_write_absolute_time(uint16_t conn_handle);
esp_err_t ble_esl_ap_unassociate(uint8_t esl_id, uint8_t group_id);
/** Release facade/profile slots after confirmed Unassociate; not for ordinary disconnects. */
esp_err_t ble_esl_ap_forget_tag(uint8_t esl_id, uint8_t group_id);
esp_err_t ble_esl_ap_led_control(uint8_t esl_id, uint8_t group_id, uint8_t led_index,
                                 const esl_ap_led_settings_t *led);
esp_err_t ble_esl_ap_pawr_command(uint8_t esl_id, uint8_t group_id, uint8_t opcode,
                                  const uint8_t *params, uint8_t params_len);
ble_esl_state_t ble_esl_ap_get_esl_state(ble_esl_address_t addr);
esp_err_t ble_esl_ap_restore_persisted_esl(const ble_esl_ap_persisted_esl_t *info);

#ifdef __cplusplus
}
#endif
