/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A callback can consume Base64 fragments without buffering the full image.
typedef bool (*openai_image_stream_cb_t)(const char *data, size_t len, void *user_data);

typedef struct {
    openai_image_stream_cb_t callback;
    void *user_data;
    char field[16];
    char value[48];
    char chunk[512];
    size_t field_len;
    size_t value_len;
    size_t chunk_len;
    size_t image_len;
    uint8_t line_state;
    uint8_t event_type;
    uint8_t json_state;
    uint8_t key_match;
    bool found_image;
    bool completed;
    bool error;
} openai_image_sse_parser_t;

void openai_image_sse_init(openai_image_sse_parser_t *parser, openai_image_stream_cb_t callback, void *user_data);
bool openai_image_sse_feed(openai_image_sse_parser_t *parser, const char *data, size_t len);
bool openai_image_sse_complete(const openai_image_sse_parser_t *parser);
