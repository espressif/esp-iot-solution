/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * BLE MIDI Service UUID: 03B80E5A-EDE8-4B33-A751-6CE34EC4C700
 */
#define BLE_MIDI_SERVICE_UUID128       { 0x00, 0xC7, 0xC4, 0x4E, 0xE3, 0x6C, 0x51, 0xA7, 0x33, 0x4B, 0xE8, 0xED, 0x5A, 0x0E, 0xB8, 0x03 }

/**
 * BLE MIDI IO Characteristic UUID: 7772E5DB-3868-4112-A1A9-F2669D106BF3
 */
#define BLE_MIDI_CHAR_UUID128          { 0xF3, 0x6B, 0x10, 0x9D, 0x66, 0xF2, 0xA9, 0xA1, 0x12, 0x41, 0x68, 0x38, 0xDB, 0xE5, 0x72, 0x77 }

/**
 * @brief Initialize and register the BLE-MIDI GATT service (server).
 *
 * Service: 03B80E5A-EDE8-4B33-A751-6CE34EC4C700
 * IO Char: 7772E5DB-3868-4112-A1A9-F2669D106BF3 (Write Without Response + Notify)
 *
 * Behavior:
 * - Writes: forwards incoming BEP payloads to the profile (esp_ble_midi_on_bep_received()).
 *   See esp_ble_midi.h for callback registration.
 * - Notifications: sent by the profile via esp_ble_midi_send*() functions.
 *   See esp_ble_midi.h for send function declarations (esp_ble_midi_send(),
 *   esp_ble_midi_send_raw_midi(), esp_ble_midi_send_note_on(), etc.).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if service initialization fails
 *  - ESP_ERR_NO_MEM if memory allocation fails
 */
esp_err_t esp_ble_midi_svc_init(void);

/**
 * @brief De-Initialize the BLE-MIDI GATT service.
 *
 * Removes the MIDI service and its characteristic from the GATT DB that was added by esp_ble_midi_svc_init().
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if the service was not previously initialized
 */
esp_err_t esp_ble_midi_svc_deinit(void);

#ifdef __cplusplus
}
#endif
