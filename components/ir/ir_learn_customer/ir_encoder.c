/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"

#include "ir_encoder.h"
#include "ir_learn_err_check.h"

static const char *TAG = "raw_encoder";

typedef struct {
    rmt_encoder_t base;           // the base "class", declares the standard encoder interface
    rmt_encoder_t *copy_encoder;  // use the copy_encoder to encode the leading and ending pulse
    rmt_encoder_t *bytes_encoder; // use the bytes_encoder to encode the address and command data
    rmt_symbol_word_t nec_leading_symbol; // NEC leading code with RMT representation
    rmt_symbol_word_t nec_ending_symbol;  // NEC ending code with RMT representation
    int state;
} rmt_ir_nec_encoder_t;

static size_t ir_encoder_rmt_raw(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    IR_LEARN_CHECK(encoder, "invalid argument", ESP_ERR_INVALID_ARG);

    rmt_ir_nec_encoder_t *nec_encoder = __containerof(encoder, rmt_ir_nec_encoder_t, base);
    rmt_encode_state_t session_state = 0;
    rmt_encode_state_t state = 0;
    size_t encoded_symbols = 0;
    rmt_encoder_handle_t copy_encoder = nec_encoder->copy_encoder;

    encoded_symbols += copy_encoder->encode(copy_encoder, channel, primary_data, data_size * sizeof(rmt_symbol_word_t), &session_state);
    if (session_state & RMT_ENCODING_COMPLETE) {
        nec_encoder->state = 1; // we can only switch to next state when p_symbolsrent encoder finished
    }
    if (session_state & RMT_ENCODING_MEM_FULL) {
        state |= RMT_ENCODING_MEM_FULL;
        goto out; // yield if there's no free space to put other encoding artifacts
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t ir_encoder_del(rmt_encoder_t *encoder)
{
    IR_LEARN_CHECK(encoder, "invalid argument", ESP_ERR_INVALID_ARG);

    rmt_ir_nec_encoder_t *nec_encoder = __containerof(encoder, rmt_ir_nec_encoder_t, base);
    if (nec_encoder->copy_encoder) {
        rmt_del_encoder(nec_encoder->copy_encoder);
    }
    if (nec_encoder->bytes_encoder) {
        rmt_del_encoder(nec_encoder->bytes_encoder);
    }
    free(nec_encoder);
    return ESP_OK;
}

static esp_err_t ir_encoder_reset(rmt_encoder_t *encoder)
{
    IR_LEARN_CHECK(encoder, "invalid argument", ESP_ERR_INVALID_ARG);

    rmt_ir_nec_encoder_t *nec_encoder = __containerof(encoder, rmt_ir_nec_encoder_t, base);
    if (nec_encoder->copy_encoder) {
        rmt_encoder_reset(nec_encoder->copy_encoder);
    }
    if (nec_encoder->bytes_encoder) {
        rmt_encoder_reset(nec_encoder->bytes_encoder);
    }
    nec_encoder->state = 0;
    return ESP_OK;
}

esp_err_t ir_encoder_new(const ir_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_ir_nec_encoder_t *nec_encoder = NULL;
    IR_LEARN_CHECK(config && ret_encoder, "invalid argument", ESP_ERR_INVALID_ARG);

    nec_encoder = calloc(1, sizeof(rmt_ir_nec_encoder_t));
    IR_LEARN_CHECK(nec_encoder, "no mem for ir nec encoder", ESP_ERR_NO_MEM);
    nec_encoder->base.encode = ir_encoder_rmt_raw;
    nec_encoder->base.del = ir_encoder_del;
    nec_encoder->base.reset = ir_encoder_reset;

    rmt_copy_encoder_config_t copy_encoder_config = {};
    ret = rmt_new_copy_encoder(&copy_encoder_config, &nec_encoder->copy_encoder);
    IR_LEARN_CHECK_GOTO((ESP_OK == ret), "create copy encoder failed", ESP_FAIL, err);

    *ret_encoder = &nec_encoder->base;
    return ESP_OK;
err:
    if (nec_encoder) {
        if (nec_encoder->bytes_encoder) {
            rmt_del_encoder(nec_encoder->bytes_encoder);
        }
        if (nec_encoder->copy_encoder) {
            rmt_del_encoder(nec_encoder->copy_encoder);
        }
        free(nec_encoder);
    }
    return ret;
}
