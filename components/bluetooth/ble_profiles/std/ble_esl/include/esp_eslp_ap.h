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
/* BLE ESLP AP EVENTS BASE */
ESP_EVENT_DECLARE_BASE(BLE_ESLP_AP_EVENTS);
/** @endcond **/

/**
 * @brief   ESL Profile AP Events
 */
typedef enum {
    BLE_ESLP_AP_EVENT_TAG_CONFIGURED = 1,                                   /*!< Tag GATT association complete */
    BLE_ESLP_AP_EVENT_ECP_RESPONSE,                                         /*!< ECP indication/notification received */
    BLE_ESLP_AP_EVENT_ECP_TIMEOUT,                                          /*!< ECP procedure timeout (30 s) */
    BLE_ESLP_AP_EVENT_TAG_INFO,                                             /*!< ESL info characteristics read complete */
    BLE_ESLP_AP_EVENT_PAWR_STARTED,                                         /*!< PAwR advertising started */
    BLE_ESLP_AP_EVENT_PAWR_STOPPED,                                         /*!< PAwR advertising stopped */
    BLE_ESLP_AP_EVENT_SYNC_TRANSFERRED,                                     /*!< PAST set_info accepted */
    BLE_ESLP_AP_EVENT_SUBEV_RESP,                                           /*!< PAwR response received */
    BLE_ESLP_AP_EVENT_IMAGE_TRANSFERRED,                                    /*!< OTP image transfer finished */
    BLE_ESLP_AP_EVENT_TAG_STATE_CHANGED,                                    /*!< Tracked Tag lifecycle state changed */
    BLE_ESLP_AP_EVENT_TAG_TIMEOUT,                                          /*!< Tracked Tag sync/unsync timeout */
} esp_ble_eslp_ap_event_t;

/**
 * @brief   Tracked Tag state-changed event
 */
typedef struct {
    esp_ble_esl_address_t address;                                          /*!< ESL Address */
    uint16_t conn_handle;                                                   /*!< Connection handle, or 0xFFFF if none */
    esp_ble_eslp_state_t prev_state;                                        /*!< Previous state */
    esp_ble_eslp_state_t new_state;                                         /*!< New state */
} esp_ble_eslp_ap_tag_state_changed_t;

/**
 * @brief   Tracked Tag timeout event
 */
typedef struct {
    esp_ble_esl_address_t address;                                          /*!< ESL Address */
    uint16_t conn_handle;                                                   /*!< Connection handle, or 0xFFFF if none */
    esp_ble_eslp_timeout_kind_t kind;                                       /*!< Timeout kind */
} esp_ble_eslp_ap_tag_timeout_t;

/**
 * @brief   Image transfer parameters
 */
typedef struct {
    uint16_t conn_handle;                                                   /*!< Connection handle */
    uint8_t image_index;                                                    /*!< Target Image_Index on the ESL */
    const uint8_t *data;                                                    /*!< Image payload */
    uint32_t data_len;                                                      /*!< Image payload length */
} esp_ble_eslp_ap_image_transfer_t;

/**
 * @brief   Image Transferred Event Data
 */
typedef struct {
    uint16_t conn_handle;                                                   /*!< Connection handle */
    uint8_t image_index;                                                    /*!< Image index that was transferred */
    esp_err_t status;                                                       /*!< ESP_OK on success */
} esp_ble_eslp_ap_image_transferred_t;

/**
 * @brief   Tag Association Configuration
 */
typedef struct {
    esp_ble_esl_address_t address;                                          /*!< ESL Address */
    esp_ble_esl_key_material_t ap_sync_key;                                 /*!< AP Sync Key Material */
    esp_ble_esl_key_material_t resp_key;                                    /*!< Response Key Material */
    esp_ble_esl_abs_time_t abs_time;                                        /*!< Absolute time, 0 uses a local timestamp */
    bool subscribe_ecp;                                                     /*!< Subscribe to ECP indications */
    bool send_update_complete;                                              /*!< If true, write Update Complete after config writes */
} esp_ble_eslp_ap_tag_config_t;

/**
 * @brief   PAwR Advertising Parameters
 */
typedef struct {
    uint16_t itvl_min;                                                      /*!< Min interval in 1.25 ms units */
    uint16_t itvl_max;                                                      /*!< Max interval in 1.25 ms units */
    uint8_t num_subevents;                                                  /*!< Number of subevents */
    uint8_t subevent_interval;                                              /*!< Interval between subevents in 1.25 ms units */
    uint8_t response_slot_delay;                                            /*!< Delay to first response slot in 1.25 ms units */
    uint8_t response_slot_spacing;                                          /*!< Spacing between response slots in 0.125 ms units */
    uint8_t num_response_slots;                                             /*!< Response slots per subevent */
    bool include_tx_power;                                                  /*!< Include TX power in the advertising PDU */
} esp_ble_eslp_ap_pawr_config_t;

/**
 * @brief   ECP Response Event Data
 */
typedef struct {
    uint16_t conn_handle;                                                   /*!< Connection handle */
    uint8_t data_len;                                                       /*!< Response length */
    uint8_t data[BLE_ESL_ECP_MAX_SIZE];                                     /*!< Response bytes */
} esp_ble_eslp_ap_ecp_response_t;

/**
 * @brief   ECP Procedure Timeout Event Data
 */
typedef struct {
    uint16_t conn_handle;                                                   /*!< Connection handle */
    uint8_t esl_id;                                                         /*!< Target ESL_ID */
    uint8_t group_id;                                                       /*!< Target Group_ID */
} esp_ble_eslp_ap_ecp_timeout_t;

/** Max Display Information records stored from a remote Tag */
#ifndef BLE_ESLP_AP_MAX_DISPLAYS
#define BLE_ESLP_AP_MAX_DISPLAYS                                            8
#endif

/** Max LED Information octets stored from a remote Tag */
#ifndef BLE_ESLP_AP_MAX_LEDS
#define BLE_ESLP_AP_MAX_LEDS                                                8
#endif

/** Max Sensor Information characteristic length */
#ifndef BLE_ESLP_AP_SENSOR_INFO_MAX_LEN
#define BLE_ESLP_AP_SENSOR_INFO_MAX_LEN                                     64
#endif

/**
 * @brief   ESL information read from a connected Tag
 */
typedef struct {
    uint16_t conn_handle;                                                   /*!< Connection handle */
    esp_err_t status;                                                       /*!< Overall status (ESP_OK if at least one char read) */

    bool has_display_info;                                                  /*!< Display Information present */
    uint8_t num_displays;                                                   /*!< Number of display records */
    esp_ble_esl_display_info_t displays[BLE_ESLP_AP_MAX_DISPLAYS];          /*!< Display records */

    bool has_image_info;                                                    /*!< Image Information present */
    esp_ble_esl_image_info_t image_info;                                    /*!< Image Information */

    bool has_sensor_info;                                                   /*!< Sensor Information present */
    uint16_t sensor_info_len;                                               /*!< Sensor Information length */
    uint8_t sensor_info[BLE_ESLP_AP_SENSOR_INFO_MAX_LEN];                   /*!< Raw Sensor Information */

    bool has_led_info;                                                      /*!< LED Information present */
    uint8_t num_leds;                                                       /*!< Number of LED info octets */
    uint8_t leds[BLE_ESLP_AP_MAX_LEDS];                                     /*!< LED Information octets */

    bool has_pnp_id;                                                        /*!< DIS PnP ID present */
    uint8_t pnp_id[7];                                                      /*!< PnP ID (src, vid, pid, ver) */
} esp_ble_eslp_ap_tag_info_t;

/**
 * @brief   LED control settings for LED Control / LED Timed Control
 */
typedef struct {
    uint8_t color_red;                                                      /*!< 2-bit red (0-3) */
    uint8_t color_green;                                                    /*!< 2-bit green (0-3) */
    uint8_t color_blue;                                                     /*!< 2-bit blue (0-3) */
    uint8_t brightness;                                                     /*!< 2-bit brightness (0-3) */
    uint8_t flashing_pattern[7];                                            /*!< 56-bit flashing pattern */
    uint8_t repeat_type;                                                    /*!< 0 = count, 1 = duration */
    uint16_t repeats_duration;                                              /*!< 15-bit count or duration (s) */
} esp_ble_eslp_ap_led_settings_t;

/**
 * @brief   PAwR Subevent Response Event Data
 */
typedef struct {
    uint8_t subevent;                                                       /*!< Subevent index */
    uint8_t response_slot;                                                  /*!< Response slot */
    uint8_t data_status;                                                    /*!< Data status */
    bool address_valid;                                                     /*!< True if @p address was resolved */
    esp_ble_esl_address_t address;                                          /*!< Tag address (when address_valid) */
    uint8_t data_len;                                                       /*!< Decrypted TLV length */
    uint8_t data[BLE_ESL_PAYLOAD_MAX_SIZE];                                 /*!< Plaintext response TLV(s) */
} esp_ble_eslp_ap_subev_resp_t;

/**
 * @brief   Tag Configured Event Data
 */
typedef struct {
    uint16_t conn_handle;                                                   /*!< Connection handle */
    esp_ble_esl_address_t address;                                          /*!< Configured ESL address */
} esp_ble_eslp_ap_tag_configured_t;

/**
 * @brief Initialization Electronic Shelf Label AP Profile
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_init(void);

/**
 * @brief Deinitialization Electronic Shelf Label AP Profile
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_deinit(void);

/**
 * @brief Fill default Tag association configuration
 *
 * @param[out] cfg The pointer to store the configuration
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_eslp_ap_tag_config_default(esp_ble_eslp_ap_tag_config_t *cfg);

/**
 * @brief Write Address / Keys / Absolute Time to a connected Tag
 *
 * The link must already be encrypted; otherwise ESP_ERR_INVALID_STATE is returned.
 *
 * @param[in] conn_handle Connection handle
 * @param[in] cfg         The pointer to store the Tag configuration
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization or unencrypted link
 */
esp_err_t esp_ble_eslp_ap_configure_tag(uint16_t conn_handle, const esp_ble_eslp_ap_tag_config_t *cfg);

/**
 * @brief Write an ECP command to a connected Tag
 *
 * @param[in] conn_handle Connection handle
 * @param[in] data        The pointer to store the ECP TLV
 * @param[in] len         The length of ECP TLV
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_write_ecp(uint16_t conn_handle, const uint8_t *data, uint16_t len);

/**
 * @brief Read ESL information characteristics from a connected Tag
 *
 * @param[in]  conn_handle Connection handle
 * @param[out] out_info    The pointer to store the Tag information
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_NOT_FOUND if none of the optional info chars were present
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_read_info(uint16_t conn_handle, esp_ble_eslp_ap_tag_info_t *out_info);

/**
 * @brief Send Ping (opcode 0x00)
 *
 * @param[in]  esl_id   ESL ID
 * @param[in]  group_id ESL Group ID
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_ping(uint8_t esl_id, uint8_t group_id);

/**
 * @brief Send Unassociate (opcode 0x01), ECP only
 *
 * @param[in]  esl_id   ESL ID
 * @param[in]  group_id ESL Group ID
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_unassociate(uint8_t esl_id, uint8_t group_id);

/**
 * @brief Send Service Reset (opcode 0x02)
 *
 * @param[in]  esl_id   ESL ID
 * @param[in]  group_id ESL Group ID
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_service_reset(uint8_t esl_id, uint8_t group_id);

/**
 * @brief Send Factory Reset (opcode 0x03), ECP only
 *
 * @param[in]  esl_id   ESL ID
 * @param[in]  group_id ESL Group ID
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_factory_reset(uint8_t esl_id, uint8_t group_id);

/**
 * @brief Send Update Complete (opcode 0x04), ECP only
 *
 * @param[in]  esl_id   ESL ID
 * @param[in]  group_id ESL Group ID
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_update_complete(uint8_t esl_id, uint8_t group_id);

/**
 * @brief Send Read Sensor Data (opcode 0x10)
 *
 * @param[in]  esl_id        ESL ID
 * @param[in]  group_id      ESL Group ID
 * @param[in]  sensor_index  Sensor index
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_read_sensor(uint8_t esl_id, uint8_t group_id, uint8_t sensor_index);

/**
 * @brief Send Refresh Display (opcode 0x11)
 *
 * @param[in]  esl_id         ESL ID
 * @param[in]  group_id       ESL Group ID
 * @param[in]  display_index  Display index
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_refresh_display(uint8_t esl_id, uint8_t group_id, uint8_t display_index);

/**
 * @brief Send Display Image (opcode 0x20)
 *
 * @param[in]  esl_id         ESL ID
 * @param[in]  group_id       ESL Group ID
 * @param[in]  display_index  Display index
 * @param[in]  image_index    Image index
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_display_image(uint8_t esl_id, uint8_t group_id,
                                        uint8_t display_index, uint8_t image_index);

/**
 * @brief Send Display Timed Image (opcode 0x60)
 *
 * @param[in]  esl_id         ESL ID
 * @param[in]  group_id       ESL Group ID
 * @param[in]  display_index  Display index
 * @param[in]  image_index    Image index
 * @param[in]  absolute_time  Absolute time in ms; 0 cancels a pending timed display
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_display_timed_image(uint8_t esl_id, uint8_t group_id,
                                              uint8_t display_index, uint8_t image_index,
                                              uint32_t absolute_time);

/**
 * @brief Send LED Control (opcode 0xB0)
 *
 * @param[in]  esl_id     ESL ID
 * @param[in]  group_id   ESL Group ID
 * @param[in]  led_index  LED index
 * @param[in]  settings   The pointer to store the LED settings
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_led_control(uint8_t esl_id, uint8_t group_id, uint8_t led_index,
                                      const esp_ble_eslp_ap_led_settings_t *settings);

/**
 * @brief Send LED Timed Control (opcode 0xF0)
 *
 * @param[in]  esl_id         ESL ID
 * @param[in]  group_id       ESL Group ID
 * @param[in]  led_index      LED index
 * @param[in]  settings       The pointer to store the LED settings
 * @param[in]  absolute_time  Absolute time in ms; 0 cancels a pending timed LED command
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_led_timed_control(uint8_t esl_id, uint8_t group_id, uint8_t led_index,
                                            const esp_ble_eslp_ap_led_settings_t *settings,
                                            uint32_t absolute_time);

/**
 * @brief Start local PAwR advertising
 *
 * @param[in]  pawr The pointer to store the PAwR parameters
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_NOT_SUPPORTED on unsupported feature
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_pawr_start(const esp_ble_eslp_ap_pawr_config_t *pawr);

/**
 * @brief Stop local PAwR advertising
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_pawr_stop(void);

/**
 * @brief Transfer local periodic sync info to a Tag
 *
 * @param[in] conn_handle  Connection handle
 * @param[in] service_data PAST service data
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_NOT_SUPPORTED on unsupported feature
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_sync_transfer(uint16_t conn_handle, uint16_t service_data);

/**
 * @brief Queue an ESL command TLV for a PAwR subevent
 *
 * For unicast, set rsp_slot_start to the response slot of the last addressed TLV
 * and rsp_slot_count to 1. For broadcast-only payloads, set rsp_slot_count to 0.
 *
 * @param[in] subevent       Subevent index
 * @param[in] tlv            The pointer to store the command TLV
 * @param[in] tlv_len        The length of command TLV
 * @param[in] rsp_slot_start First response slot to listen to
 * @param[in] rsp_slot_count Number of response slots to listen to
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 *  - ESP_ERR_NOT_SUPPORTED on unsupported feature
 */
esp_err_t esp_ble_eslp_ap_queue_pawr_command(uint8_t subevent, const uint8_t *tlv, uint8_t tlv_len,
                                             uint8_t rsp_slot_start, uint8_t rsp_slot_count);

/**
 * @brief Queue a Ping command for a group
 *
 * @param[in] group_id ESL Group ID
 * @param[in] esl_id   ESL ID
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 *  - ESP_ERR_NOT_SUPPORTED on unsupported feature
 */
esp_err_t esp_ble_eslp_ap_queue_ping(uint8_t group_id, uint8_t esl_id);

/**
 * @brief Transfer an image to a connected Tag via OTP
 *
 * @param[in] params The pointer to store the transfer parameters
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_ap_transfer_image(const esp_ble_eslp_ap_image_transfer_t *params);

/**
 * @brief Get the tracked lifecycle state of a remote Tag
 *
 * @param[in]  addr      ESL Address
 * @param[out] out_state The pointer to store the state
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_NOT_FOUND if the Tag is not tracked
 */
esp_err_t esp_ble_eslp_ap_get_tag_state(const esp_ble_esl_address_t *addr, esp_ble_eslp_state_t *out_state);

/**
 * @brief Refresh Synchronized-state activity timer for a connected Tag
 *
 * @param[in] conn_handle Connection handle
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_NOT_FOUND if no tracked Tag matches the connection
 */
esp_err_t esp_ble_eslp_ap_note_tag_sync_activity(uint16_t conn_handle);

#ifdef __cplusplus
}
#endif
