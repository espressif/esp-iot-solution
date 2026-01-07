/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief MIDI over BLE Profile
 */

#include <string.h>
#include <stdlib.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_ble_conn_mgr.h"
#include "esp_ble_midi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "esp_ble_midi";
static esp_ble_midi_rx_cb_t s_rx_cb = NULL;
static void *s_rx_cb_user_ctx = NULL;
static esp_ble_midi_event_cb_t s_evt_cb = NULL;
static bool s_notify_enabled = false;
static SemaphoreHandle_t s_sysex_mutex = NULL;
static volatile bool s_initialized = false;
static volatile bool s_shutting_down = false;

static struct {
    uint8_t buffer[BLE_MIDI_MAX_PKT_LEN];
    uint16_t len;
    bool active;
} s_sysex;

/**
 * @brief Get current BLE MTU size
 *
 * @return Current MTU size, or 23 (minimum) if MTU query fails
 */
static uint16_t esp_ble_midi_get_mtu(void)
{
    uint16_t mtu = 23;  /* Default minimum MTU */
    (void)esp_ble_conn_get_mtu(&mtu);
    return mtu;
}

static inline uint8_t esp_ble_midi_header_ts_high(uint16_t ts13)
{
    /* 0x80 | (timestamp >> 7) lower 6 bits */
    return (uint8_t)(0x80 | ((ts13 >> 7) & 0x3F));
}

static inline uint8_t esp_ble_midi_ts_low(uint16_t ts13)
{
    return (uint8_t)(0x80 | (ts13 & 0x7F));
}

esp_err_t esp_ble_midi_profile_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_sysex_mutex = xSemaphoreCreateMutex();
    if (s_sysex_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sysex mutex");
        return ESP_ERR_NO_MEM;
    }

    memset(&s_sysex, 0, sizeof(s_sysex));

    s_shutting_down = false;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t esp_ble_midi_profile_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    if (s_sysex_mutex == NULL) {
        /* Mutex was already NULL, clear state */
        memset(&s_sysex, 0, sizeof(s_sysex));
        s_notify_enabled = false;
        s_rx_cb = NULL;
        s_rx_cb_user_ctx = NULL;
        s_evt_cb = NULL;
        s_initialized = false;
        s_shutting_down = false;
        return ESP_OK;
    }

    /* Set shutting_down flag first to signal callbacks to exit early */
    s_shutting_down = true;

    /* Try to acquire mutex with longer timeout to ensure callbacks have finished */
    if (xSemaphoreTake(s_sysex_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex during deinit (timeout) - mutex leaked");
        /* Wait a short time for callbacks to see s_shutting_down and exit */
        vTaskDelay(pdMS_TO_TICKS(150));
        s_sysex_mutex = NULL;
        /* DO NOT delete mutex - callbacks may still hold references */
        memset(&s_sysex, 0, sizeof(s_sysex));
        s_notify_enabled = false;
        s_rx_cb = NULL;
        s_rx_cb_user_ctx = NULL;
        s_evt_cb = NULL;
        s_initialized = false;
        s_shutting_down = false;
        return ESP_ERR_TIMEOUT;
    }

    /* Store mutex handle locally before clearing global */
    SemaphoreHandle_t mutex_to_delete = s_sysex_mutex;
    s_sysex_mutex = NULL;

    s_notify_enabled = false;
    s_rx_cb = NULL;
    s_rx_cb_user_ctx = NULL;
    s_evt_cb = NULL;

    /* Clear sysex state while holding the mutex */
    memset(&s_sysex, 0, sizeof(s_sysex));

    /* Release and delete mutex - safe since s_sysex_mutex is already NULL */
    xSemaphoreGive(mutex_to_delete);
    vSemaphoreDelete(mutex_to_delete);

    s_initialized = false;
    s_shutting_down = false;

    return ESP_OK;
}

static esp_err_t esp_ble_midi_send_bep(const uint8_t *midi, uint16_t midi_len, uint16_t timestamp)
{
    if (!midi || !midi_len) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t pkt_size = (uint16_t)(midi_len + 2);
    if (pkt_size > (esp_ble_midi_get_mtu() - 3)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *pkt = (uint8_t *)malloc(pkt_size);
    if (!pkt) {
        return ESP_ERR_NO_MEM;
    }
    uint16_t ts13 = timestamp & ESP_BLE_MIDI_TIMESTAMP_MAX;
    uint16_t pos = 0;
    pkt[pos++] = esp_ble_midi_header_ts_high(ts13);
    pkt[pos++] = esp_ble_midi_ts_low(ts13);
    memcpy(&pkt[pos], midi, midi_len);
    pos += midi_len;
    esp_err_t rc = esp_ble_midi_send(pkt, pos);
    free(pkt);
    return rc;
}

/* Send raw BEP packet without timestamp (used for SysEx middle packets) */
static esp_err_t esp_ble_midi_send_bep_raw(const uint8_t *data, uint16_t data_len, uint16_t timestamp)
{
    if (!data || !data_len) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t pkt_size = (uint16_t)(data_len + 1);
    if (pkt_size > (esp_ble_midi_get_mtu() - 3)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *pkt = (uint8_t *)malloc(pkt_size);
    if (!pkt) {
        return ESP_ERR_NO_MEM;
    }
    uint16_t ts13 = timestamp & ESP_BLE_MIDI_TIMESTAMP_MAX;
    uint16_t pos = 0;

    pkt[pos++] = esp_ble_midi_header_ts_high(ts13);
    memcpy(&pkt[pos], data, data_len);
    pos += data_len;
    esp_err_t rc = esp_ble_midi_send(pkt, pos);
    free(pkt);
    return rc;
}

void esp_ble_midi_on_bep_received(const uint8_t *data, uint16_t len)
{
    if (!data || !len) {
        return;
    }

    if (s_shutting_down) {
        return;
    }

    esp_ble_midi_rx_cb_t rx_cb = NULL;
    void *rx_cb_user_ctx = NULL;
    esp_ble_midi_event_cb_t evt_cb = NULL;
    if (s_sysex_mutex != NULL && !s_shutting_down) {
        if (xSemaphoreTake(s_sysex_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_shutting_down) {
                xSemaphoreGive(s_sysex_mutex);
                return;
            }
            rx_cb = s_rx_cb;
            rx_cb_user_ctx = s_rx_cb_user_ctx;
            evt_cb = s_evt_cb;
            xSemaphoreGive(s_sysex_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire mutex for callback read");
            return;
        }
    } else {
        /* Not initialized */
        return;
    }

    if (rx_cb) {
        rx_cb(data, len, rx_cb_user_ctx);
    }
    if (!evt_cb) {
        return;
    }

    if (s_shutting_down) {
        return;
    }

    /* Decode BLE-MIDI Event Packet */
    uint16_t idx = 0;
    if ((data[idx] & 0x80) == 0) {
        return;
    }

    uint16_t ts_ms = (uint16_t)((data[idx++] & 0x3F) << 7);
    uint8_t running = 0;
    bool mutex_taken = false;
    if (s_sysex_mutex != NULL && !s_shutting_down) {
        mutex_taken = (xSemaphoreTake(s_sysex_mutex, pdMS_TO_TICKS(100)) == pdTRUE);
        if (!mutex_taken) {
            /* Mutex unavailable: skip processing to avoid blocking BLE callback */
            ESP_LOGW(TAG, "Failed to acquire sysex mutex, skipping BEP processing");
            return;
        }
        /* Check shutting_down again after acquiring mutex */
        if (s_shutting_down) {
            xSemaphoreGive(s_sysex_mutex);
            return;
        }
    } else {
        /* Not initialized */
        return;
    }

    while (idx < len) {
        if ((data[idx] & 0x80) == 0) {
            break;
        }
        ts_ms = (uint16_t)((ts_ms & 0x1F80) | (data[idx++] & 0x7F));
        /* If continuing SysEx, consume this fragment's data */
        if (s_sysex.active) {
            while (idx < len) {
                uint8_t b = data[idx];
                if (b >= 0xF8) {
                    evt_cb(ts_ms, ESP_BLE_MIDI_EVENT_NORMAL, &b, 1);
                    idx++;
                    continue;
                }
                if (b == 0xF7) {
                    /* SysEx end byte: append, dispatch, and reset */
                    if (s_sysex.len >= sizeof(s_sysex.buffer)) {
                        /* Buffer overflow: signal error and reset state */
                        evt_cb(ts_ms, ESP_BLE_MIDI_EVENT_SYSEX_OVERFLOW, NULL, 0);
                        s_sysex.active = false;
                        s_sysex.len = 0;
                    } else {
                        s_sysex.buffer[s_sysex.len++] = b;
                        evt_cb(ts_ms, ESP_BLE_MIDI_EVENT_NORMAL, s_sysex.buffer, s_sysex.len);
                        s_sysex.active = false;
                        s_sysex.len = 0;
                    }
                    idx++;
                    break;
                }
                if (b & 0x80) {
                    /* Next timestamp or status: end of this fragment */
                    break;
                }
                if (s_sysex.len >= sizeof(s_sysex.buffer)) {
                    /* Buffer overflow: signal error and reset state */
                    evt_cb(ts_ms, ESP_BLE_MIDI_EVENT_SYSEX_OVERFLOW, NULL, 0);
                    s_sysex.active = false;
                    s_sysex.len = 0;
                    /* Skip remaining bytes in this fragment */
                    while (idx < len && (data[idx] & 0x80) == 0 && data[idx] < 0xF8) {
                        idx++;
                    }
                    break;
                }
                s_sysex.buffer[s_sysex.len++] = b;
                idx++;
            }
            continue;
        }
        /* Emit any real-time bytes first */
        while (idx < len && data[idx] >= 0xF8) {
            uint8_t rt = data[idx++];
            evt_cb(ts_ms, ESP_BLE_MIDI_EVENT_NORMAL, &rt, 1);
        }
        if (idx >= len) {
            break;
        }
        uint8_t b0 = data[idx];
        if (b0 == 0xF0) {
            /* Start SysEx */
            s_sysex.active = true;
            s_sysex.len = 0;
            s_sysex.buffer[s_sysex.len++] = 0xF0;
            idx++;
            while (idx < len) {
                uint8_t b = data[idx];
                if (b >= 0xF8) {
                    evt_cb(ts_ms, ESP_BLE_MIDI_EVENT_NORMAL, &b, 1);
                    idx++;
                    continue;
                }
                if (b == 0xF7) {
                    /* SysEx end byte: append, dispatch, and reset */
                    if (s_sysex.len >= sizeof(s_sysex.buffer)) {
                        /* Buffer overflow: signal error and reset state */
                        evt_cb(ts_ms, ESP_BLE_MIDI_EVENT_SYSEX_OVERFLOW, NULL, 0);
                        s_sysex.active = false;
                        s_sysex.len = 0;
                    } else {
                        s_sysex.buffer[s_sysex.len++] = b;
                        evt_cb(ts_ms, ESP_BLE_MIDI_EVENT_NORMAL, s_sysex.buffer, s_sysex.len);
                        s_sysex.active = false;
                        s_sysex.len = 0;
                    }
                    idx++;
                    break;
                }
                if (b & 0x80) {
                    break;
                }
                if (s_sysex.len >= sizeof(s_sysex.buffer)) {
                    /* Buffer overflow: signal error and reset state */
                    evt_cb(ts_ms, ESP_BLE_MIDI_EVENT_SYSEX_OVERFLOW, NULL, 0);
                    s_sysex.active = false;
                    s_sysex.len = 0;
                    /* Skip remaining bytes in this fragment */
                    while (idx < len && (data[idx] & 0x80) == 0 && data[idx] < 0xF8) {
                        idx++;
                    }
                    break;
                }
                s_sysex.buffer[s_sysex.len++] = b;
                idx++;
            }
            continue;
        }
        uint8_t msg[3];
        uint16_t mlen = 0;
        if (b0 & 0x80) {
            /* Status (non real-time) */
            uint8_t status = b0;
            idx++;
            if (status >= 0x80 && status <= 0xEF) {
                uint8_t type = status & 0xF0;
                uint8_t need = (type == 0xC0 || type == 0xD0) ? 1 : 2;
                if ((idx + need) > len) {
                    break;
                }
                if (need >= 1 && (data[idx] & 0x80)) {
                    break;
                }
                msg[0] = status;
                msg[1] = data[idx++];
                if (need == 2) {
                    if (data[idx] & 0x80) {
                        break;
                    }
                    msg[2] = data[idx++];
                }
                mlen = 1 + need;
                running = status;
            } else {
                /* System Common (non real-time) */
                uint8_t need = 0;
                switch (status) {
                case 0xF1: need = 1; break;
                case 0xF2: need = 2; break;
                case 0xF3: need = 1; break;
                case 0xF6: need = 0; break;
                case 0xF7: need = 0; break;
                default: need = 0; break;
                }
                if ((idx + need) > len) {
                    break;
                }
                if (need >= 1 && (data[idx] & 0x80)) {
                    break;
                }
                msg[0] = status;
                if (need >= 1) {
                    msg[1] = data[idx++];
                }
                if (need == 2) {
                    if (data[idx] & 0x80) {
                        break;
                    }
                    msg[2] = data[idx++];
                }
                mlen = 1 + need;
            }
        } else {
            /* Running status */
            if (running == 0) {
                break;
            }
            uint8_t status = running;
            uint8_t type = status & 0xF0;
            uint8_t need = (type == 0xC0 || type == 0xD0) ? 1 : 2;
            if ((idx + need) > len) {
                break;
            }
            if (data[idx] & 0x80) {
                break;
            }
            msg[0] = status;
            msg[1] = data[idx++];
            if (need == 2) {
                if (data[idx] & 0x80) {
                    break;
                }
                msg[2] = data[idx++];
            }
            mlen = 1 + need;
        }
        if (mlen > 0) {
            evt_cb(ts_ms, ESP_BLE_MIDI_EVENT_NORMAL, msg, mlen);
        }
    }

    if (mutex_taken && s_sysex_mutex != NULL) {
        xSemaphoreGive(s_sysex_mutex);
    }
}

esp_err_t esp_ble_midi_register_rx_cb(esp_ble_midi_rx_cb_t cb, void *user_ctx)
{
    if (!s_initialized || s_shutting_down) {
        ESP_LOGE(TAG, "esp_ble_midi_profile_init() must be called before registering callbacks");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_sysex_mutex != NULL) {
        if (xSemaphoreTake(s_sysex_mutex, portMAX_DELAY) == pdTRUE) {
            s_rx_cb = cb;
            s_rx_cb_user_ctx = user_ctx;
            xSemaphoreGive(s_sysex_mutex);
        } else {
            return ESP_FAIL;
        }
    } else {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t esp_ble_midi_register_event_cb(esp_ble_midi_event_cb_t cb)
{
    if (!s_initialized || s_shutting_down) {
        ESP_LOGE(TAG, "esp_ble_midi_profile_init() must be called before registering callbacks");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_sysex_mutex != NULL) {
        if (xSemaphoreTake(s_sysex_mutex, portMAX_DELAY) == pdTRUE) {
            s_evt_cb = cb;
            xSemaphoreGive(s_sysex_mutex);
        } else {
            return ESP_FAIL;
        }
    } else {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t esp_ble_midi_send(const uint8_t *data, uint16_t len)
{
    if (!data || !len) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_shutting_down) {
        ESP_LOGE(TAG, "esp_ble_midi_profile_init() must be called before sending");
        return ESP_ERR_INVALID_STATE;
    }

    /* Check if notification is enabled via CCCD */
    bool notify_enabled = false;
    if (s_sysex_mutex != NULL) {
        if (xSemaphoreTake(s_sysex_mutex, portMAX_DELAY) == pdTRUE) {
            notify_enabled = s_notify_enabled;
            xSemaphoreGive(s_sysex_mutex);
        } else {
            return ESP_FAIL;
        }
    } else {
        return ESP_ERR_INVALID_STATE;
    }

    if (!notify_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_ble_conn_data_t conn_data = {
        .type = BLE_CONN_UUID_TYPE_128,
        .uuid = {
            .uuid128 = BLE_MIDI_CHAR_UUID128,
        },
        .data = (uint8_t *)data,
        .data_len = len,
    };

    return esp_ble_conn_notify(&conn_data);
}

esp_err_t esp_ble_midi_send_raw_midi(const uint8_t *midi, uint8_t midi_len, uint16_t timestamp)
{
    return esp_ble_midi_send_bep(midi, (uint16_t)midi_len, timestamp);
}

esp_err_t esp_ble_midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp)
{
    uint8_t msg[3];
    msg[0] = (uint8_t)(0x90 | (channel & 0x0F));
    msg[1] = (uint8_t)(note & 0x7F);
    msg[2] = (uint8_t)(velocity & 0x7F);
    return esp_ble_midi_send_bep(msg, sizeof msg, timestamp);
}

esp_err_t esp_ble_midi_send_note_off(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp)
{
    uint8_t msg[3];
    msg[0] = (uint8_t)(0x80 | (channel & 0x0F));
    msg[1] = (uint8_t)(note & 0x7F);
    msg[2] = (uint8_t)(velocity & 0x7F);
    return esp_ble_midi_send_bep(msg, sizeof msg, timestamp);
}

esp_err_t esp_ble_midi_send_cc(uint8_t channel, uint8_t controller, uint8_t value, uint16_t timestamp)
{
    uint8_t msg[3];
    msg[0] = (uint8_t)(0xB0 | (channel & 0x0F));
    msg[1] = (uint8_t)(controller & 0x7F);
    msg[2] = (uint8_t)(value & 0x7F);
    return esp_ble_midi_send_bep(msg, sizeof msg, timestamp);
}

esp_err_t esp_ble_midi_send_pitch_bend(uint8_t channel, uint16_t value, uint16_t timestamp)
{
    uint16_t v14 = (uint16_t)(value & 0x3FFF);
    uint8_t lsb = (uint8_t)(v14 & 0x7F);
    uint8_t msb = (uint8_t)((v14 >> 7) & 0x7F);
    uint8_t msg[3];
    msg[0] = (uint8_t)(0xE0 | (channel & 0x0F));
    msg[1] = lsb;
    msg[2] = msb;
    return esp_ble_midi_send_bep(msg, sizeof msg, timestamp);
}

esp_err_t esp_ble_midi_send_multi(const uint8_t *const msgs[], const uint8_t msg_lens[],
                                  const uint16_t timestamps[], uint16_t count,
                                  size_t *first_unsent_index)
{
    if (!msgs || !msg_lens || !timestamps || count == 0) {
        if (first_unsent_index) {
            *first_unsent_index = 0;
        }
        return ESP_ERR_INVALID_ARG;
    }

    /* Get current MTU and calculate buffer size based on negotiated MTU */
    uint16_t bep_payload_cap = esp_ble_midi_get_mtu() - 3;
    uint8_t *pkt = (uint8_t *)malloc(bep_payload_cap);
    if (!pkt) {
        if (first_unsent_index) {
            *first_unsent_index = 0;
        }
        return ESP_ERR_NO_MEM;
    }

    uint16_t i = 0;
    esp_err_t rc = ESP_OK;

    while (i < count) {
        uint16_t pos = 0;
        uint16_t packet_start_idx = i; /* Track which message index this packet starts from */
        uint16_t ts13 = (uint16_t)(timestamps[i] & ESP_BLE_MIDI_TIMESTAMP_MAX);
        /* Packet header (timestamp high 6 bits) */
        pkt[pos++] = esp_ble_midi_header_ts_high(ts13);

        /* Fill events until buffer would overflow */
        for (; i < count; i++) {
            uint8_t mlen = msg_lens[i];
            if (mlen == 0 || !msgs[i]) {
                continue;
            }
            uint16_t evt_need = (uint16_t)(1 + mlen); /* ts_low + message bytes */
            if ((pos + evt_need) > bep_payload_cap) {
                break;
            }
            uint16_t ts_evt = (uint16_t)(timestamps[i] & ESP_BLE_MIDI_TIMESTAMP_MAX);
            pkt[pos++] = esp_ble_midi_ts_low(ts_evt);
            memcpy(&pkt[pos], msgs[i], mlen);
            pos += mlen;
        }

        if (pos <= 1) {
            /* Check if current message is too large to fit in buffer */
            if (i < count && msg_lens[i] > 0 && msgs[i]) {
                uint16_t evt_need = (uint16_t)(1 + msg_lens[i]); /* ts_low + message bytes */
                if (evt_need > (bep_payload_cap - 1)) {
                    if (first_unsent_index) {
                        *first_unsent_index = i;
                    }
                    free(pkt);
                    return ESP_ERR_INVALID_STATE;
                }
            }
            continue;
        }

        rc = esp_ble_midi_send(pkt, pos);
        if (rc != ESP_OK) {
            /* On failure, set output parameter to indicate first unsent message */
            if (first_unsent_index) {
                *first_unsent_index = packet_start_idx;
            }
            free(pkt);
            return rc;
        }
    }

    free(pkt);

    /* All messages sent successfully */
    if (first_unsent_index) {
        *first_unsent_index = count;
    }
    return rc;
}

esp_err_t esp_ble_midi_send_sysex(const uint8_t *sysex, uint16_t len, uint16_t timestamp)
{
    if (!sysex || len < 2) {
        return ESP_ERR_INVALID_ARG;
    }
    if (sysex[0] != 0xF0 || sysex[len - 1] != 0xF7) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t bep_payload_cap = esp_ble_midi_get_mtu() - 3;

    /* Calculate capacity for each packet type per BLE-MIDI spec:
     * First packet: [header][ts_low][0xF0][data...] = 3 bytes overhead
     * Middle packets: [header][data...] = 1 byte overhead
     * Last packet: [header][ts_low][data...][0xF7] = 3 bytes overhead
     */
    uint16_t first_pkt_cap = (bep_payload_cap >= 3) ? (uint16_t)(bep_payload_cap - 3) : 0;
    uint16_t middle_pkt_cap = (bep_payload_cap >= 1) ? (uint16_t)(bep_payload_cap - 1) : 0;
    uint16_t last_pkt_cap = (bep_payload_cap >= 3) ? (uint16_t)(bep_payload_cap - 3) : 0;

    if (first_pkt_cap == 0 || middle_pkt_cap == 0 || last_pkt_cap == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t *p = sysex;
    uint16_t remaining = len;

    /* Special case: entire SysEx fits in one packet: [header][ts_low][0xF0][data...][0xF7] */
    /* Packet size would be: header (1) + ts_low (1) + SysEx data (len) = len + 2 */
    if ((len + 2) <= bep_payload_cap) {
        uint16_t ts13 = timestamp & ESP_BLE_MIDI_TIMESTAMP_MAX;
        uint16_t pkt_size = (uint16_t)(len + 2); /* header + ts_low + entire SysEx */
        if (pkt_size > (esp_ble_midi_get_mtu() - 3)) {
            return ESP_ERR_INVALID_ARG;
        }
        uint8_t *pkt = (uint8_t *)malloc(pkt_size);
        if (!pkt) {
            return ESP_ERR_NO_MEM;
        }
        uint16_t pos = 0;
        pkt[pos++] = esp_ble_midi_header_ts_high(ts13);
        pkt[pos++] = esp_ble_midi_ts_low(ts13);
        memcpy(&pkt[pos], sysex, len); /* Copy entire SysEx including 0xF0 and 0xF7 */
        pos += len;
        esp_err_t rc = esp_ble_midi_send(pkt, pos);
        free(pkt);
        return rc;
    }

    /* First packet: [header][ts_low][0xF0][data...] */
    if (remaining > 1) {
        uint16_t ts13 = timestamp & ESP_BLE_MIDI_TIMESTAMP_MAX;
        uint16_t chunk = (remaining - 1) > first_pkt_cap ? first_pkt_cap : (remaining - 1);
        /* Packet: header + ts_low + 0xF0 + data */
        if (chunk > (esp_ble_midi_get_mtu() - 6)) {
            return ESP_ERR_INVALID_ARG;
        }
        uint16_t pkt_size = (uint16_t)(chunk + 3);
        uint8_t *pkt = (uint8_t *)malloc(pkt_size);
        if (!pkt) {
            return ESP_ERR_NO_MEM;
        }
        uint16_t pos = 0;
        pkt[pos++] = esp_ble_midi_header_ts_high(ts13);
        pkt[pos++] = esp_ble_midi_ts_low(ts13);
        pkt[pos++] = 0xF0; /* SysEx start */
        memcpy(&pkt[pos], &p[1], chunk); /* Skip 0xF0 in source */
        pos += chunk;
        esp_err_t rc = esp_ble_midi_send(pkt, pos);
        free(pkt);
        if (rc != ESP_OK) {
            return rc;
        }
        p += (chunk + 1); /* +1 for 0xF0 */
        remaining -= (chunk + 1);
    }

    /* Middle packets: [header][data...] (no timestamp byte) */
    /* Continue sending middle packets while we have more than just 0xF7 remaining */
    while (remaining > 1) {
        /* Check if this chunk would leave only 0xF7, making this the last packet */
        uint16_t max_chunk = (remaining - 1) > middle_pkt_cap ? middle_pkt_cap : (remaining - 1);
        /* If remaining bytes fit in last_pkt_cap, send as last packet instead */
        if (remaining <= (last_pkt_cap + 1)) {
            break; /* Handle as last packet */
        }
        /* Send as middle packet (header only, no timestamp) */
        esp_err_t rc = esp_ble_midi_send_bep_raw(p, max_chunk, timestamp);
        if (rc != ESP_OK) {
            return rc;
        }
        p += max_chunk;
        remaining -= max_chunk;
    }

    /* Last packet: [header][ts_low][data...][0xF7] */
    if (remaining >= 1) {
        /* remaining should be >= 1, with last byte being 0xF7 */
        if (remaining > 0 && p[remaining - 1] == 0xF7) {
            uint16_t ts13 = timestamp & ESP_BLE_MIDI_TIMESTAMP_MAX;
            /* Packet contains: header + ts_low + data_bytes_before_f7 + 0xF7 */
            uint16_t data_bytes = remaining - 1; /* Exclude 0xF7 */
            uint16_t pkt_size = (uint16_t)(data_bytes + 3); /* header + ts_low + data + 0xF7 */
            if (pkt_size > (esp_ble_midi_get_mtu() - 3)) {
                return ESP_ERR_INVALID_ARG;
            }
            uint8_t *pkt = (uint8_t *)malloc(pkt_size);
            if (!pkt) {
                return ESP_ERR_NO_MEM;
            }
            uint16_t pos = 0;
            pkt[pos++] = esp_ble_midi_header_ts_high(ts13);
            pkt[pos++] = esp_ble_midi_ts_low(ts13);
            if (data_bytes > 0) {
                memcpy(&pkt[pos], p, data_bytes);
                pos += data_bytes;
            }
            pkt[pos++] = 0xF7; /* SysEx end */
            esp_err_t rc = esp_ble_midi_send(pkt, pos);
            free(pkt);
            if (rc != ESP_OK) {
                return rc;
            }
        } else {
            /* Unexpected state - last byte should be 0xF7 */
            return ESP_ERR_INVALID_STATE;
        }
    }

    return ESP_OK;
}

uint16_t esp_ble_midi_get_timestamp_ms(void)
{
    uint32_t ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    return (uint16_t)(ms & ESP_BLE_MIDI_TIMESTAMP_MAX);
}

esp_err_t esp_ble_midi_set_notify_enabled(bool enabled)
{
    if (!s_initialized || s_shutting_down) {
        ESP_LOGE(TAG, "esp_ble_midi_profile_init() must be called before setting notify state");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_sysex_mutex != NULL) {
        if (xSemaphoreTake(s_sysex_mutex, portMAX_DELAY) == pdTRUE) {
            s_notify_enabled = enabled;
            xSemaphoreGive(s_sysex_mutex);
        } else {
            return ESP_FAIL;
        }
    } else {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

bool esp_ble_midi_is_notify_enabled(void)
{
    if (!s_initialized || s_shutting_down) {
        ESP_LOGD(TAG, "Profile not initialized, returning false for notify state");
        return false;
    }

    bool enabled = false;
    if (s_sysex_mutex != NULL) {
        if (xSemaphoreTake(s_sysex_mutex, portMAX_DELAY) == pdTRUE) {
            enabled = s_notify_enabled;
            xSemaphoreGive(s_sysex_mutex);
        }
    }
    return enabled;
}
