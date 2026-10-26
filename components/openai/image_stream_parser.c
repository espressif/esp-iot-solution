/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "image_stream_parser.h"

#include <string.h>

// SSE field parser: event: image_generation.completed followed by data: {...}.
// A completed image's Base64 data can be several MB, so never buffer a line.
enum {
    LINE_FIELD,
    LINE_EVENT,
    LINE_DATA,
    LINE_SKIP,
};

enum {
    EVENT_NONE,
    EVENT_COMPLETED,
    EVENT_ERROR,
};

enum {
    JSON_FIND_KEY,
    JSON_EXPECT_COLON,
    JSON_EXPECT_VALUE,
    JSON_READ_BASE64,
    JSON_AFTER_VALUE,
};

static const char b64_json_key[] = "\"b64_json\"";

static bool base64_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
}

void openai_image_sse_init(openai_image_sse_parser_t *parser, openai_image_stream_cb_t callback, void *user_data)
{
    memset(parser, 0, sizeof(*parser));
    parser->callback = callback;
    parser->user_data = user_data;
}

static bool flush_chunk(openai_image_sse_parser_t *parser)
{
    if (parser->chunk_len == 0) {
        return true;
    }
    if (!parser->callback(parser->chunk, parser->chunk_len, parser->user_data)) {
        parser->error = true;
        return false;
    }
    parser->chunk_len = 0;
    return true;
}

static bool feed_json(openai_image_sse_parser_t *parser, char c)
{
    switch (parser->json_state) {
    case JSON_FIND_KEY:
        if (c == b64_json_key[parser->key_match]) {
            parser->key_match++;
            if (parser->key_match == sizeof(b64_json_key) - 1) {
                parser->json_state = JSON_EXPECT_COLON;
            }
        } else {
            parser->key_match = (c == b64_json_key[0]) ? 1 : 0;
        }
        break;
    case JSON_EXPECT_COLON:
        if (c == ':') {
            parser->json_state = JSON_EXPECT_VALUE;
        } else if (c != ' ' && c != '\t') {
            parser->error = true;
        }
        break;
    case JSON_EXPECT_VALUE:
        if (c == '"') {
            parser->json_state = JSON_READ_BASE64;
        } else if (c != ' ' && c != '\t') {
            parser->error = true;
        }
        break;
    case JSON_READ_BASE64:
        if (c == '"') {
            if (parser->image_len == 0 || parser->image_len % 4 != 0) {
                parser->error = true;
                break;
            }
            if (!flush_chunk(parser)) {
                return false;
            }
            parser->found_image = true;
            parser->json_state = JSON_AFTER_VALUE;
        } else if (base64_char(c)) {
            parser->chunk[parser->chunk_len++] = c;
            parser->image_len++;
            if (parser->chunk_len == sizeof(parser->chunk) && !flush_chunk(parser)) {
                return false;
            }
        } else {
            parser->error = true;
        }
        break;
    case JSON_AFTER_VALUE:
        break;
    default:
        parser->error = true;
    }
    return !parser->error;
}

static void finish_line(openai_image_sse_parser_t *parser)
{
    if (parser->line_state == LINE_EVENT) {
        parser->value[parser->value_len] = '\0';
        const char *event = parser->value;
        while (*event == ' ') {
            event++;
        }
        parser->event_type = strcmp(event, "image_generation.completed") == 0 ? EVENT_COMPLETED :
                             strcmp(event, "error") == 0 ? EVENT_ERROR : EVENT_NONE;
        if (parser->event_type == EVENT_ERROR) {
            parser->error = true;
        }
    } else if (parser->line_state == LINE_DATA && parser->event_type == EVENT_COMPLETED) {
        if (parser->found_image && parser->json_state == JSON_AFTER_VALUE) {
            parser->completed = true;
        } else {
            parser->error = true;
        }
    } else if (parser->line_state == LINE_FIELD && parser->field_len == 0) {
        parser->event_type = EVENT_NONE;
    }
    parser->field_len = 0;
    parser->value_len = 0;
    parser->line_state = LINE_FIELD;
}

bool openai_image_sse_feed(openai_image_sse_parser_t *parser, const char *data, size_t len)
{
    if (!parser || !parser->callback || (!data && len > 0) || parser->error) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        const char c = data[i];
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            // JSON strings from the Images API are never multiline.
            if (parser->line_state == LINE_DATA && parser->json_state == JSON_READ_BASE64 &&
                    parser->event_type == EVENT_COMPLETED) {
                parser->error = true;
            }
            finish_line(parser);
            if (parser->error) {
                return false;
            }
            continue;
        }
        switch (parser->line_state) {
        case LINE_FIELD:
            if (c == ':') {
                parser->field[parser->field_len] = '\0';
                if (strcmp(parser->field, "event") == 0) {
                    parser->line_state = LINE_EVENT;
                } else if (strcmp(parser->field, "data") == 0) {
                    parser->line_state = LINE_DATA;
                    parser->json_state = JSON_FIND_KEY;
                    parser->key_match = 0;
                    parser->found_image = false;
                } else {
                    parser->line_state = LINE_SKIP;
                }
            } else if (parser->field_len + 1 < sizeof(parser->field)) {
                parser->field[parser->field_len++] = c;
            } else {
                parser->line_state = LINE_SKIP;
            }
            break;
        case LINE_EVENT:
            if (parser->value_len + 1 < sizeof(parser->value)) {
                parser->value[parser->value_len++] = c;
            } else {
                parser->error = true;
            }
            break;
        case LINE_DATA:
            if (parser->event_type == EVENT_COMPLETED && !feed_json(parser, c)) {
                return false;
            }
            break;
        case LINE_SKIP:
            break;
        default:
            parser->error = true;
        }
        if (parser->error) {
            return false;
        }
    }
    return true;
}

bool openai_image_sse_complete(const openai_image_sse_parser_t *parser)
{
    return parser && parser->completed && !parser->error;
}
