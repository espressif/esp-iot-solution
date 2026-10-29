/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * BLE MIDI Service UUID (128-bit): 03B80E5A-EDE8-4B33-A751-6CE34EC4C700
 */
#define BLE_MIDI_SERVICE_UUID128       { 0x00, 0xC7, 0xC4, 0x4E, 0xE3, 0x6C, 0x51, 0xA7, 0x33, 0x4B, 0xE8, 0xED, 0x5A, 0x0E, 0xB8, 0x03 }

/**
 * BLE MIDI IO Characteristic UUID (128-bit): 7772E5DB-3868-4112-A1A9-F2669D106BF3
 */
#define BLE_MIDI_CHAR_UUID128          { 0xF3, 0x6B, 0x10, 0x9D, 0x66, 0xF2, 0xA9, 0xA1, 0x12, 0x41, 0x68, 0x38, 0xDB, 0xE5, 0x72, 0x77 }

/**
 * @brief Maximum BLE-MIDI packet length (BLE 5.0+)
 *
 * Used for buffer allocation and size validation in BLE-MIDI packets.
 */
#define BLE_MIDI_MAX_PKT_LEN          256

/**
 * @brief RX callback for raw BLE-MIDI Event Packet (BEP) payloads.
 *
 * The buffer contains the BEP payload as defined by the BLE-MIDI specification.
 * For parsed per-message events, use esp_ble_midi_register_event_cb().
 *
 * @param[in] data     Pointer to BEP payload buffer
 * @param[in] len      Length of BEP payload
 * @param[in] user_ctx User context pointer passed during registration
 */
typedef void (*esp_ble_midi_rx_cb_t)(const uint8_t *data, uint16_t len, void *user_ctx);

/**
 * @brief MIDI event types
 */
typedef enum {
    ESP_BLE_MIDI_EVENT_NORMAL = 0,      /**< Normal MIDI message */
    ESP_BLE_MIDI_EVENT_SYSEX_OVERFLOW,  /**< SysEx buffer overflow detected */
} esp_ble_midi_event_type_t;

/**
 * @brief Parsed event callback for a single decoded MIDI message.
 *
 * - msg contains one complete MIDI message (status + data).
 * - SysEx is delivered as a complete 0xF0 ... 0xF7 sequence (reassembled).
 * - Real-time messages (0xF8..0xFF) are delivered as 1-byte events and may interleave.
 * - On SysEx overflow (ESP_BLE_MIDI_EVENT_SYSEX_OVERFLOW), msg is NULL and msg_len is 0.
 *
 * @param[in] timestamp_ms  13-bit rolling timestamp in milliseconds
 * @param[in] event_type    Event type (normal message or overflow)
 * @param[in] msg           Pointer to a single MIDI message (status + data), NULL on overflow
 * @param[in] msg_len       Message length in bytes, 0 on overflow
 */
typedef void (*esp_ble_midi_event_cb_t)(uint16_t timestamp_ms, esp_ble_midi_event_type_t event_type, const uint8_t *msg, uint16_t msg_len);

/**
 * @brief Initialize the BLE MIDI profile.
 *
 * This function must be called once before using any other BLE MIDI functions.
 * It initializes internal synchronization primitives and state.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_NO_MEM if mutex creation fails
 *  - ESP_OK if already initialized (idempotent)
 */
esp_err_t esp_ble_midi_profile_init(void);

/**
 * @brief Deinitialize the BLE MIDI profile.
 *
 * This function cleans up internal resources including mutex and clears all callbacks
 * and state. After calling this function, esp_ble_midi_profile_init() must be called
 * again before using any other BLE MIDI functions.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_OK if already deinitialized (idempotent)
 */
esp_err_t esp_ble_midi_profile_deinit(void);

/**
 * @brief Register application RX callback to receive BLE-MIDI event packets.
 *
 * Called from the BLE host task; keep the handler short or hand off to another task.
 *
 * @note esp_ble_midi_profile_init() must be called before registering callbacks.
 *
 * @param[in] cb       RX callback; pass NULL to unregister
 * @param[in] user_ctx User context pointer to be passed to the callback
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if esp_ble_midi_profile_init() has not been called
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_midi_register_rx_cb(esp_ble_midi_rx_cb_t cb, void *user_ctx);

/**
 * @brief Register parsed MIDI event callback.
 *
 * Called once per decoded MIDI message (including SysEx and real-time).
 * Runs in the BLE host task; keep the handler short or post to an app task.
 *
 * @note esp_ble_midi_profile_init() must be called before registering callbacks.
 *
 * @param[in] cb  Event callback; pass NULL to unregister
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if esp_ble_midi_profile_init() has not been called
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_midi_register_event_cb(esp_ble_midi_event_cb_t cb);

/**
 * @brief Send BLE-MIDI event packet via notification.
 *
 * data must be a complete BEP payload (packet header timestamp high bits and
 * a per-message timestamp byte before each message). Most applications should
 * prefer esp_ble_midi_send_raw_midi() or the helper APIs.
 *
 * @note esp_ble_midi_profile_init() must be called before sending.
 *
 * @param[in] data  Pointer to packet buffer
 * @param[in] len   Packet length
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid args
 *  - ESP_ERR_INVALID_STATE if esp_ble_midi_profile_init() has not been called or notification is not enabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_midi_send(const uint8_t *data, uint16_t len);

/** 13-bit rolling timestamp (1 ms resolution) */
#define ESP_BLE_MIDI_TIMESTAMP_MAX              0x1FFF

/**
 * @brief Build and send a BLE-MIDI Event Packet from raw MIDI bytes.
 *
 * Packs the BLE-MIDI timestamps (packet header high bits + per-message low byte).
 *
 * @param[in] midi       Pointer to MIDI message bytes (status + data)
 * @param[in] midi_len   Length of MIDI message (1-3 bytes typical)
 * @param[in] timestamp  13-bit timestamp in milliseconds (rolls at 8192)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_midi_send_raw_midi(const uint8_t *midi, uint8_t midi_len, uint16_t timestamp);

/**
 * @brief Send MIDI Note On
 *
 * @param[in] channel    0-15
 * @param[in] note       0-127
 * @param[in] velocity   0-127
 * @param[in] timestamp  13-bit timestamp in ms
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp);

/**
 * @brief Send MIDI Note Off
 *
 * @param[in] channel    0-15
 * @param[in] note       0-127
 * @param[in] velocity   0-127
 * @param[in] timestamp  13-bit timestamp in ms
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_midi_send_note_off(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp);

/**
 * @brief Send MIDI Control Change
 *
 * @param[in] channel     0-15
 * @param[in] controller  0-127
 * @param[in] value       0-127
 * @param[in] timestamp   13-bit timestamp in ms
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_midi_send_cc(uint8_t channel, uint8_t controller, uint8_t value, uint16_t timestamp);

/**
 * @brief Send MIDI Pitch Bend (14-bit value)
 *
 * @param[in] channel     0-15
 * @param[in] value       0-16383 (8192 is center)
 * @param[in] timestamp   13-bit timestamp in ms
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_midi_send_pitch_bend(uint8_t channel, uint16_t value, uint16_t timestamp);

/**
 * @brief Send multiple MIDI events aggregated into a single BLE-MIDI packet.
 *
 * Aggregates multiple MIDI messages in one notification to improve throughput.
 * Each message will be prefixed with its own BLE-MIDI timestamp byte.
 *
 * Notes:
 * - The function packs as many messages as fit in an internal buffer (<= ATT MTU).
 * - If not all messages fit, this function sends multiple notifications.
 * - For SysEx, pass a complete 0xF0...0xF7 sequence. For automatic fragmentation,
 *   use esp_ble_midi_send_sysex(), or split by MTU and call this repeatedly.
 * - On failure: If first_unsent_index is provided, it will be set to the index of
 *   the first message that was not sent. On success, it will be set to count.
 *   This allows callers to determine which messages were successfully transmitted
 *   before the error occurred.
 *
 * @param[in] msgs                Array of pointers to MIDI messages (status + data bytes)
 * @param[in] msg_lens            Array of message lengths (1..n)
 * @param[in] timestamps          Array of 13-bit timestamps for each message (ms resolution)
 * @param[in] count               Number of messages in arrays
 * @param[out] first_unsent_index Optional output parameter: on failure, set to the index
 *                                of the first message that was not sent; on success,
 *                                set to count. Pass NULL if not needed.
 *
 * @return
 *  - ESP_OK on success (all messages sent)
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_FAIL on other error (partial send may have occurred; check first_unsent_index)
 */
esp_err_t esp_ble_midi_send_multi(const uint8_t *const msgs[], const uint8_t msg_lens[],
                                  const uint16_t timestamps[], uint16_t count,
                                  size_t *first_unsent_index);

/**
 * @brief Send a System Exclusive (SysEx) message with automatic multi-packet fragmentation.
 *
 * The buffer must begin with 0xF0 and end with 0xF7. The function splits the
 * SysEx into multiple BLE-MIDI Event Packets based on current ATT MTU.
 *
 * Notes:
 * - Uses the current MTU to choose fragment sizes (accounting for ATT and BEP overhead).
 * - Real-time messages (0xF8..0xFF) are delivered as separate events on the receiver.
 *
 * @param[in] sysex      Pointer to complete SysEx buffer (0xF0 ... 0xF7)
 * @param[in] len        Length of the SysEx buffer
 * @param[in] timestamp  13-bit timestamp in ms to use for all fragments
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_midi_send_sysex(const uint8_t *sysex, uint16_t len, uint16_t timestamp);

/**
 * @brief Profile entry for raw BLE-MIDI Event Packet received from GATT.
 *
 * The service layer should call this to hand off incoming packets.
 *
 * @param[in] data  Pointer to BEP payload buffer
 * @param[in] len   Length of BEP payload
 */
void esp_ble_midi_on_bep_received(const uint8_t *data, uint16_t len);

/**
 * @brief Get 13-bit rolling timestamp in milliseconds for BLE-MIDI.
 *
 * Returns a 1 ms resolution counter modulo 8192 suitable for BLE-MIDI
 * timestamps. Intended for use as the `timestamp` parameter in send helpers.
 *
 * @return 13-bit timestamp in milliseconds (0..8191)
 */
uint16_t esp_ble_midi_get_timestamp_ms(void);

/**
 * @brief Set notification enabled state (typically called when CCCD is updated).
 *
 * This should be called when the client writes to the CCCD (Client Characteristic
 * Configuration Descriptor) to enable/disable notifications. MIDI send functions
 * will only work when notification is enabled.
 *
 * @note esp_ble_midi_profile_init() must be called before setting notify state.
 *
 * @param[in] enabled  true if notification is enabled, false otherwise
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if esp_ble_midi_profile_init() has not been called
 */
esp_err_t esp_ble_midi_set_notify_enabled(bool enabled);

/**
 * @brief Check if notification is currently enabled.
 *
 * @note esp_ble_midi_profile_init() must be called before checking notify state.
 *
 * @return true if notification is enabled, false otherwise (also returns false if not initialized)
 */
bool esp_ble_midi_is_notify_enabled(void);

#ifdef __cplusplus
}
#endif
