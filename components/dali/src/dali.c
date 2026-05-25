/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "dali.h"

static const char *TAG = "dali";

/** RMT clock resolution used by this driver (1 MHz → 1 µs per tick). */
#define DALI_RMT_RESOLUTION_HZ      1000000U

/** Convert microseconds to RMT ticks. */
#define DALI_US_TO_RMT_TICKS(us)    ((us) * (DALI_RMT_RESOLUTION_HZ / 1000000U))

/** Convert microseconds to nanoseconds (used for RMT filter/idle config). */
#define DALI_US_TO_NS(us)           ((us) * 1000U)

/** DALI half-period Te in microseconds (nominal 416.67 µs). */
#define DALI_TE_US                  416U

/** Delay between the first and second transmission of a send-twice command (ms). */
#define DALI_SEND_TWICE_DELAY_MS    40

#define DALI_BF_TIMEOUT_MS          50

/** Minimum inter-frame gap inserted automatically after every transaction (ms).
 *  IEC 62386 requires > 22 Te ≈ 9.2 ms; 20 ms gives comfortable margin. */
#define DALI_IFG_MS                 20

/** Address mask bits for each addressing mode (S-bit excluded). */
#define DALI_ADDR_MASK_SHORT        0x00U
#define DALI_ADDR_MASK_GROUP        0x80U
#define DALI_ADDR_MASK_BROADCAST    0xFEU

/**
 * @brief Internal driver context.  One instance per dali_new_master_rmt() call.
 */
struct dali_master_t {
    rmt_channel_handle_t rx_channel;
    rmt_channel_handle_t tx_channel;
    rmt_encoder_handle_t tx_encoder;
    QueueHandle_t        rx_queue;
    rmt_receive_config_t rx_cfg;
    rmt_symbol_word_t    rx_raw[32]; /* static DMA buffer — must not be on stack */

    /* TX symbol pointers — set once during init based on polarity config. */
    const rmt_symbol_word_t *symbol_one;
    const rmt_symbol_word_t *symbol_zero;
    const rmt_symbol_word_t *symbol_stop;
};

/* --- Non-inverting TX symbols (invert_tx = false, default) --- */
static const rmt_symbol_word_t s_sym_one_normal = {
    .level0    = 0,
    .duration0 = DALI_US_TO_RMT_TICKS(DALI_TE_US),
    .level1    = 1,
    .duration1 = DALI_US_TO_RMT_TICKS(DALI_TE_US),
};
static const rmt_symbol_word_t s_sym_zero_normal = {
    .level0    = 1,
    .duration0 = DALI_US_TO_RMT_TICKS(DALI_TE_US),
    .level1    = 0,
    .duration1 = DALI_US_TO_RMT_TICKS(DALI_TE_US),
};
static const rmt_symbol_word_t s_sym_stop_normal = {
    /* Bus idle = high.  With HW inversion: GPIO must be LOW during idle.
     * RMT transmits this stop symbol and then the GPIO returns to its
     * natural idle low level — both produce the correct bus-high idle. */
    .level0    = 0,
    .duration0 = DALI_US_TO_RMT_TICKS(DALI_TE_US) * 2,
    .level1    = 0,
    .duration1 = DALI_US_TO_RMT_TICKS(DALI_TE_US) * 2,
};

/* --- Inverting TX symbols (invert_tx = true) --- */
static const rmt_symbol_word_t s_sym_one_invert = {
    .level0    = 1,
    .duration0 = DALI_US_TO_RMT_TICKS(DALI_TE_US),
    .level1    = 0,
    .duration1 = DALI_US_TO_RMT_TICKS(DALI_TE_US),
};
static const rmt_symbol_word_t s_sym_zero_invert = {
    .level0    = 0,
    .duration0 = DALI_US_TO_RMT_TICKS(DALI_TE_US),
    .level1    = 1,
    .duration1 = DALI_US_TO_RMT_TICKS(DALI_TE_US),
};
static const rmt_symbol_word_t s_sym_stop_invert = {
    .level0    = 0,
    .duration0 = DALI_US_TO_RMT_TICKS(DALI_TE_US) * 2,
    .level1    = 0,
    .duration1 = DALI_US_TO_RMT_TICKS(DALI_TE_US) * 2,
};

/**
 * @brief Expand received (duration, level) RMT segments into a Te-sampled sequence
 *        and decode the Manchester-encoded backward frame byte.
 *
 * Each backward frame is 22 Te max: start(2) + data(16) + stop(4) = 22 Te.
 */
static esp_err_t dali_decode_backward_frame_byte(const rmt_symbol_word_t *symbols, size_t num_symbols,
                                                 uint8_t *out_byte, uint8_t *out_bits_decoded)
{
    const uint32_t te_ticks = DALI_US_TO_RMT_TICKS(DALI_TE_US);
    const size_t   max_te   = 40; /* generous cap to avoid runaway expansion */

    uint8_t te_levels[max_te];
    memset(te_levels, 0, max_te * sizeof(uint8_t));
    size_t  te_len = 0;

    for (size_t i = 0; i < num_symbols && te_len < max_te; i++) {
        const uint16_t dur0 = symbols[i].duration0;
        const uint16_t lvl0 = symbols[i].level0;
        const uint16_t dur1 = symbols[i].duration1;
        const uint16_t lvl1 = symbols[i].level1;

        /* Round duration to nearest Te count, clamp to [1..8]. */
        uint32_t n0 = (dur0 + (te_ticks / 2)) / te_ticks;
        uint32_t n1 = (dur1 + (te_ticks / 2)) / te_ticks;
        if (n0 == 0) {
            n0 = 1;
        } else if (n0 > 8) {
            n0 = 8;
        }
        if (n1 == 0) {
            n1 = 1;
        } else if (n1 > 8) {
            n1 = 8;
        }

        for (uint32_t k = 0; k < n0 && te_len < max_te; k++) {
            te_levels[te_len++] = (uint8_t)(lvl0 & 0x01U);
        }
        for (uint32_t k = 0; k < n1 && te_len < max_te; k++) {
            te_levels[te_len++] = (uint8_t)(lvl1 & 0x01U);
        }
    }

    /* Find the first Manchester half-bit pair (levels differ). */
    size_t start = 0;
    while (start + 1 < te_len && te_levels[start] == te_levels[start + 1]) {
        start++;
    }
    if (start + 1 >= te_len) {
        *out_bits_decoded = 0;
        return ESP_ERR_INVALID_STATE;
    }

    /* Treat the first valid pair as the START bit (which is always '1').
     * Use its pattern to decode following bits. */
    const uint8_t one_a = te_levels[start];
    const uint8_t one_b = te_levels[start + 1];
    if (one_a == one_b) {
        *out_bits_decoded = 0;
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t frame = 0;
    uint8_t bits  = 0;

    /* Data bits follow immediately after the start bit: 8 bits, MSB first. */
    size_t idx = start + 2;
    while (bits < 8 && (idx + 1) < te_len) {
        const uint8_t a = te_levels[idx];
        const uint8_t b = te_levels[idx + 1];
        if (a == one_a && b == one_b) {
            frame = (uint8_t)((frame << 1) | 1U);
            bits++;
        } else if (a == one_b && b == one_a) {
            frame = (uint8_t)(frame << 1);
            bits++;
        } else {
            /* Invalid Manchester pair (not a transition). */
            break;
        }
        idx += 2;
    }

    *out_byte = frame;
    *out_bits_decoded = bits;
    return (bits == 8) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

/**
 * @brief RMT simple-encoder callback for DALI forward frames.
 *
 * Encodes a 2-byte DALI command (address + data) as:
 *   1 start bit (logic 1) + 16 data bits (MSB first) + 1 stop symbol (2 Te)
 *
 * The RMT simple encoder calls this repeatedly until *done is set.
 * @p arg points to the dali_master_t context so the callback can access the
 * correct symbol table for this instance.
 */
static size_t dali_tx_encode_cb(const void *data, size_t data_size, size_t symbols_written, size_t symbols_free,
                                rmt_symbol_word_t *symbols, bool *done, void *arg)
{
    struct dali_master_t *dev = (struct dali_master_t *)arg;

    if (symbols_free < 18) {
        return 0;
    }
    if (symbols_written == 0) {
        symbols[0] = *dev->symbol_one;
        return 1;
    }
    const uint8_t *bytes = (const uint8_t *)data;
    size_t byte_idx = (symbols_written - 1) / 8;
    if (byte_idx < data_size) {
        size_t out = 0;
        for (int mask = 0x80; mask != 0; mask >>= 1) {
            symbols[out++] = (bytes[byte_idx] & mask) ? *dev->symbol_one : *dev->symbol_zero;
        }
        return out; /* 8 symbols per byte */
    }
    symbols[0] = *dev->symbol_stop;
    *done = true;
    return 1;
}

static bool IRAM_ATTR dali_rx_done_cb(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata,
                                      void *user_data)
{
    BaseType_t hp_woken = pdFALSE;
    xQueueSendFromISR((QueueHandle_t)user_data, edata, &hp_woken);
    return hp_woken == pdTRUE;
}

esp_err_t dali_new_master_rmt(const dali_master_config_t *config, const dali_master_rmt_config_t *rmt_config,
                              dali_master_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(config != NULL && handle != NULL, ESP_ERR_INVALID_ARG, TAG, "config and handle must not be NULL");
    ESP_RETURN_ON_FALSE(config->rx_gpio >= 0 && config->tx_gpio >= 0, ESP_ERR_INVALID_ARG, TAG,
                        "Invalid GPIO number: rx=%d tx=%d", config->rx_gpio, config->tx_gpio);

    /* Use caller-supplied config or fall back to built-in defaults. */
    const dali_master_rmt_config_t default_rmt_cfg = {
        .mem_block_symbols = 64,
    };
    if (rmt_config == NULL) {
        rmt_config = &default_rmt_cfg;
    }

    ESP_LOGI(TAG, "TX polarity: %s, RX polarity: %s",
             config->invert_tx ? "inverting" : "normal",
             config->invert_rx ? "inverting" : "normal");

    /* Resolve mem_block_symbols: 0 means auto-detect from SOC capability. */
    uint32_t mem_block = rmt_config->mem_block_symbols;
    if (mem_block == 0) {
#if defined(SOC_RMT_MEM_WORDS_PER_CHANNEL)
        mem_block = SOC_RMT_MEM_WORDS_PER_CHANNEL; /* e.g. 48 on C3/S3 */
#else
        mem_block = 64; /* ESP32 / ESP32-S2 */
#endif
    }

    esp_err_t ret = ESP_OK;

    struct dali_master_t *dev = calloc(1, sizeof(struct dali_master_t));
    ESP_RETURN_ON_FALSE(dev != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate DALI context");

    /* Select TX symbol table at runtime based on polarity configuration. */
    if (config->invert_tx) {
        dev->symbol_one  = &s_sym_one_invert;
        dev->symbol_zero = &s_sym_zero_invert;
        dev->symbol_stop = &s_sym_stop_invert;
    } else {
        dev->symbol_one  = &s_sym_one_normal;
        dev->symbol_zero = &s_sym_zero_normal;
        dev->symbol_stop = &s_sym_stop_normal;
    }

    rmt_rx_channel_config_t rx_cfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = DALI_RMT_RESOLUTION_HZ,
        .mem_block_symbols = mem_block,
        .gpio_num          = config->rx_gpio,
        .flags.invert_in   = config->invert_rx ? 1 : 0,
    };
    ESP_GOTO_ON_ERROR(rmt_new_rx_channel(&rx_cfg, &dev->rx_channel), err, TAG, "Failed to create RX channel");

    dev->rx_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    ESP_GOTO_ON_FALSE(dev->rx_queue != NULL, ESP_ERR_NO_MEM, err, TAG, "Failed to create RX queue");

    rmt_rx_event_callbacks_t rx_cbs = {
        .on_recv_done = dali_rx_done_cb,
    };
    ESP_GOTO_ON_ERROR(rmt_rx_register_event_callbacks(dev->rx_channel, &rx_cbs, dev->rx_queue), err, TAG,
                      "Failed to register RX callbacks");

    /* RMT filter: ignore glitches shorter than 2 µs.
     * Idle threshold: 2 ms — longer than the BF stop-bits high (833 µs) so
     * reception ends cleanly after the BF, but shorter than the minimum IFG
     * between two FFs (9.2 ms) so we never accidentally merge frames. */
    dev->rx_cfg = (rmt_receive_config_t) {
        .signal_range_min_ns = DALI_US_TO_NS(2),
        .signal_range_max_ns = DALI_US_TO_NS(2000),
    };

    rmt_tx_channel_config_t tx_cfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = DALI_RMT_RESOLUTION_HZ,
        .gpio_num          = config->tx_gpio,
        .mem_block_symbols = mem_block,
        .trans_queue_depth = 4,
        .flags.invert_out  = false,
    };
    ESP_GOTO_ON_ERROR(rmt_new_tx_channel(&tx_cfg, &dev->tx_channel), err, TAG, "Failed to create TX channel");

    const rmt_simple_encoder_config_t enc_cfg = {
        .callback = dali_tx_encode_cb,
        .arg      = dev,  /* pass context so callback can access symbol table */
    };
    ESP_GOTO_ON_ERROR(rmt_new_simple_encoder(&enc_cfg, &dev->tx_encoder), err, TAG, "Failed to create TX encoder");

    ESP_GOTO_ON_ERROR(rmt_enable(dev->tx_channel), err, TAG, "Failed to enable TX channel");
    ESP_GOTO_ON_ERROR(rmt_enable(dev->rx_channel), err, TAG, "Failed to enable RX channel");

    ESP_LOGI(TAG, "DALI driver initialized (RX GPIO %d%s, TX GPIO %d%s, mem_block=%"PRIu32")",
             config->rx_gpio, config->invert_rx ? " inv" : "",
             config->tx_gpio, config->invert_tx ? " inv" : "",
             mem_block);

    *handle = dev;
    return ESP_OK;

err:
    dali_del_master(dev);
    return ret;
}

esp_err_t dali_del_master(dali_master_handle_t handle)
{
    if (handle == NULL) {
        return ESP_OK;
    }
    struct dali_master_t *dev = handle;

    if (dev->tx_channel) {
        rmt_disable(dev->tx_channel);
        rmt_del_channel(dev->tx_channel);
        dev->tx_channel = NULL;
    }
    if (dev->tx_encoder) {
        rmt_del_encoder(dev->tx_encoder);
        dev->tx_encoder = NULL;
    }
    if (dev->rx_channel) {
        rmt_disable(dev->rx_channel);
        rmt_del_channel(dev->rx_channel);
        dev->rx_channel = NULL;
    }
    if (dev->rx_queue) {
        vQueueDelete(dev->rx_queue);
        dev->rx_queue = NULL;
    }
    free(dev);
    ESP_LOGI(TAG, "DALI driver de-initialized");
    return ESP_OK;
}

esp_err_t dali_master_do_transaction(dali_master_handle_t handle,
                                     const dali_master_transaction_config_t *config,
                                     int *result)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle must not be NULL");
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config must not be NULL");

    struct dali_master_t *dev = handle;
    dali_addr_type_t addr_type = config->addr_type;
    uint8_t addr = config->addr;
    bool is_cmd = config->is_cmd;
    uint8_t command = config->command;
    bool send_twice = config->send_twice;
    int tx_timeout_ms = config->tx_timeout_ms;

    /* Validate address range. */
    if (addr_type == DALI_ADDR_SHORT && addr > 63) {
        ESP_LOGE(TAG, "Short address out of range: %d", addr);
        return ESP_ERR_INVALID_ARG;
    }
    if (addr_type == DALI_ADDR_GROUP && addr > 15) {
        ESP_LOGE(TAG, "Group address out of range: %d", addr);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t addr_byte;
    switch (addr_type) {
    case DALI_ADDR_SPECIAL:
        addr_byte = addr;
        break;
    case DALI_ADDR_BROADCAST:
        addr_byte = DALI_ADDR_MASK_BROADCAST | (is_cmd ? 0x01U : 0x00U);
        break;
    case DALI_ADDR_SHORT:
        addr_byte = DALI_ADDR_MASK_SHORT | (uint8_t)(addr << 1) | (is_cmd ? 0x01U : 0x00U);
        break;
    case DALI_ADDR_GROUP:
        addr_byte = DALI_ADDR_MASK_GROUP | (uint8_t)(addr << 1) | (is_cmd ? 0x01U : 0x00U);
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t tx_buf[2] = {addr_byte, command};
    const rmt_transmit_config_t tx_cfg = {.loop_count = 0};

    xQueueReset(dev->rx_queue);

    /* --- No backward frame expected: TX only --------------------------- */
    if (result == NULL) {
        ESP_RETURN_ON_ERROR(rmt_transmit(dev->tx_channel, dev->tx_encoder, tx_buf, sizeof(tx_buf), &tx_cfg), TAG,
                            "TX transmit failed");
        ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(dev->tx_channel, tx_timeout_ms), TAG, "TX wait timeout");

        if (send_twice) {
            vTaskDelay(pdMS_TO_TICKS(DALI_SEND_TWICE_DELAY_MS));
            ESP_RETURN_ON_ERROR(rmt_transmit(dev->tx_channel, dev->tx_encoder, tx_buf, sizeof(tx_buf), &tx_cfg), TAG,
                                "TX retransmit failed");
            ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(dev->tx_channel, tx_timeout_ms), TAG, "TX retransmit wait timeout");
        }

        /* IEC 62386: next FF must be sent > 22 Te after this FF ends. */
        vTaskDelay(pdMS_TO_TICKS(DALI_IFG_MS));
        return ESP_OK;
    }

    /* --- Backward frame expected: TX then RX --------------------------- */
    ESP_RETURN_ON_ERROR(rmt_transmit(dev->tx_channel, dev->tx_encoder, tx_buf, sizeof(tx_buf), &tx_cfg), TAG,
                        "TX transmit failed");
    ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(dev->tx_channel, tx_timeout_ms), TAG, "TX wait timeout");

    esp_err_t rx_err = rmt_receive(dev->rx_channel, dev->rx_raw, sizeof(dev->rx_raw), &dev->rx_cfg);
    if (rx_err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_receive failed: %s", esp_err_to_name(rx_err));
        vTaskDelay(pdMS_TO_TICKS(DALI_IFG_MS));
        return rx_err;
    }

    /* --- Wait for backward frame reception ----------------------------- */
    /* The RX channel also sees the FF itself, but the decoder discards it
     * (a 17-symbol FF is not a valid 8-bit BF).  We keep reading events
     * until one decodes successfully or the deadline expires.              */
    rmt_rx_done_event_data_t rx_data;
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(DALI_BF_TIMEOUT_MS);
    bool bf_found = false;

    while (!bf_found) {
        TickType_t now = xTaskGetTickCount();
        if (now >= deadline) {
            break;
        }
        TickType_t remaining = deadline - now;

        if (xQueueReceive(dev->rx_queue, &rx_data, remaining) != pdPASS) {
            break;
        }

        uint8_t frame = 0;
        uint8_t bit_count = 0;
        esp_err_t dec_err = dali_decode_backward_frame_byte(rx_data.received_symbols, rx_data.num_symbols, &frame,
                                                            &bit_count);
        if (dec_err == ESP_OK) {
            *result = frame;
            ESP_LOGD(TAG, "BF received: 0x%02X (bits decoded: %d)", frame, bit_count);
            bf_found = true;
        } else {
            /* Could be the FF echo or a garbled frame — re-arm and keep waiting. */
            ESP_LOGD(TAG, "RX event discarded (bits decoded: %d, symbols: %u) — waiting for BF", bit_count,
                     (unsigned)rx_data.num_symbols);
            esp_err_t rearm = rmt_receive(dev->rx_channel, dev->rx_raw, sizeof(dev->rx_raw), &dev->rx_cfg);
            if (rearm != ESP_OK) {
                ESP_LOGE(TAG, "rmt_receive re-arm failed: %s", esp_err_to_name(rearm));
                break;
            }
        }
    }

    if (!bf_found) {
        *result = DALI_RESULT_NO_REPLY;
        ESP_LOGD(TAG, "BF timeout — no reply");
    }

    /* IEC 62386: next FF must be sent > 22 Te after BF ends (or after FF if no reply). */
    vTaskDelay(pdMS_TO_TICKS(DALI_IFG_MS));
    return ESP_OK;
}
