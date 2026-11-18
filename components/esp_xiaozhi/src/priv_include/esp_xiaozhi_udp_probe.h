/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum {
    ESP_XIAOZHI_PROBE_NONE,
    ESP_XIAOZHI_PROBE_SEND,
    ESP_XIAOZHI_PROBE_FAILED,
} esp_xiaozhi_probe_action_t;

typedef struct {
    uint8_t packet[32];
    uint32_t sequence;
    unsigned attempts;
    bool pending;
    bool failed;
    int64_t started;
    int64_t next_round;
} esp_xiaozhi_udp_probe_t;

static inline void esp_xiaozhi_udp_probe_init(esp_xiaozhi_udp_probe_t *probe, const uint8_t ssrc[4])
{
    memset(probe, 0, sizeof(*probe));
    probe->packet[0] = 2;
    probe->packet[3] = 16;
    memcpy(probe->packet + 4, ssrc, 4);
}

static inline esp_xiaozhi_probe_action_t esp_xiaozhi_udp_probe_poll(esp_xiaozhi_udp_probe_t *probe, int64_t now)
{
    if (probe->failed) {
        return ESP_XIAOZHI_PROBE_NONE;
    }
    if (!probe->pending) {
        if (now < probe->next_round) {
            return ESP_XIAOZHI_PROBE_NONE;
        }
        if (probe->sequence == UINT32_MAX) {
            probe->failed = true;
            return ESP_XIAOZHI_PROBE_FAILED;
        }
        probe->sequence++;
        for (unsigned i = 0; i < 4; i++) {
            probe->packet[12 + i] = probe->sequence >> (24 - i * 8);
        }
        probe->pending = true;
        probe->started = now;
        probe->attempts = 1;
        return ESP_XIAOZHI_PROBE_SEND;
    }
    if (now - probe->started >= 5000000) {
        probe->failed = true;
        return ESP_XIAOZHI_PROBE_FAILED;
    }
    int64_t retry = probe->attempts == 1 ? 1000000 : 3000000;
    if (probe->attempts < 3 && now - probe->started >= retry) {
        probe->attempts++;
        return ESP_XIAOZHI_PROBE_SEND;
    }
    return ESP_XIAOZHI_PROBE_NONE;
}

static inline bool esp_xiaozhi_udp_probe_matches(const esp_xiaozhi_udp_probe_t *probe,
                                                 const uint8_t *packet, size_t length, int64_t now)
{
    return length == 32 && probe->pending && !probe->failed &&
           now >= probe->started && now - probe->started < 5000000 && packet[0] == 3 &&
           memcmp(packet + 1, probe->packet + 1, 15) == 0;
}

static inline void esp_xiaozhi_udp_probe_complete(esp_xiaozhi_udp_probe_t *probe, int64_t now, uint32_t jitter)
{
    probe->pending = false;
    probe->next_round = now + 15000000 + jitter % 1500001;
}
