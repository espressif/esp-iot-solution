/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "image_stream_parser.h"

typedef struct {
    char content[4096];
    size_t length;
    size_t callback_count;
    bool abort;
} capture_t;

static bool capture_base64(const char *data, size_t len, void *user_data)
{
    capture_t *ctx = (capture_t *)user_data;
    ctx->callback_count++;
    if (ctx->abort || ctx->length + len >= sizeof(ctx->content)) {
        return false;
    }
    memcpy(ctx->content + ctx->length, data, len);
    ctx->length += len;
    ctx->content[ctx->length] = '\0';
    return true;
}

static bool feed_in_chunks(openai_image_sse_parser_t *parser, const char *data, size_t length, size_t chunk_size)
{
    for (size_t i = 0; i < length; i += chunk_size) {
        const size_t count = length - i < chunk_size ? length - i : chunk_size;
        if (!openai_image_sse_feed(parser, data + i, count)) {
            return false;
        }
    }
    return true;
}

static void test_split_events(void)
{
    const char *events =
        "event: image_generation.partial_image\r\n"
        "data: {\"b64_json\":\"QUJD\",\"partial_image_index\":0}\r\n\r\n"
        "event: image_generation.completed\r\n"
        "data: {\"type\":\"image_generation.completed\",\"b64_json\": \"AQIDBA==\",\"usage\":{\"total_tokens\":1}}\r\n\r\n";
    for (size_t chunk = 1; chunk <= 43; chunk += 7) {
        openai_image_sse_parser_t parser;
        capture_t capture = {0};
        openai_image_sse_init(&parser, capture_base64, &capture);
        assert(feed_in_chunks(&parser, events, strlen(events), chunk));
        assert(openai_image_sse_complete(&parser));
        assert(capture.length == 8);
        assert(strcmp(capture.content, "AQIDBA==") == 0);
    }
}

static void test_large_image(void)
{
    openai_image_sse_parser_t parser;
    capture_t capture = {0};
    openai_image_sse_init(&parser, capture_base64, &capture);
    const char *start = "event: image_generation.completed\ndata: {\"b64_json\":\"";
    assert(feed_in_chunks(&parser, start, strlen(start), 1));
    for (size_t i = 0; i < 384; i++) {
        assert(openai_image_sse_feed(&parser, "TWFu", 4));
    }
    assert(openai_image_sse_feed(&parser, "\"}\n\n", 4));
    assert(openai_image_sse_complete(&parser));
    assert(capture.length == 1536);
    assert(capture.callback_count >= 3);
}

static void test_missing_and_truncated_image(void)
{
    openai_image_sse_parser_t parser;
    capture_t capture = {0};
    const char *partial = "event: image_generation.partial_image\ndata: {\"b64_json\":\"QUJD\"}\n\n";
    openai_image_sse_init(&parser, capture_base64, &capture);
    assert(openai_image_sse_feed(&parser, partial, strlen(partial)));
    assert(!openai_image_sse_complete(&parser));
    assert(capture.length == 0);

    const char *truncated = "event: image_generation.completed\ndata: {\"b64_json\":\"QUJD";
    openai_image_sse_init(&parser, capture_base64, &capture);
    assert(openai_image_sse_feed(&parser, truncated, strlen(truncated)));
    assert(!openai_image_sse_complete(&parser));

    const char *empty = "event: image_generation.completed\ndata: {\"b64_json\":\"\"}\n\n";
    openai_image_sse_init(&parser, capture_base64, &capture);
    assert(!openai_image_sse_feed(&parser, empty, strlen(empty)));
}

static void test_errors(void)
{
    openai_image_sse_parser_t parser;
    capture_t capture = {0};
    const char *error = "event: error\ndata: {\"message\":\"failure\"}\n\n";
    openai_image_sse_init(&parser, capture_base64, &capture);
    assert(!openai_image_sse_feed(&parser, error, strlen(error)));
    assert(!openai_image_sse_complete(&parser));

    const char *invalid_base64 = "event: image_generation.completed\ndata: {\"b64_json\":\"QU!D\"}\n";
    openai_image_sse_init(&parser, capture_base64, &capture);
    assert(!openai_image_sse_feed(&parser, invalid_base64, strlen(invalid_base64)));

    const char *valid = "event: image_generation.completed\ndata: {\"b64_json\":\"QUJD\"}\n";
    openai_image_sse_init(&parser, capture_base64, &capture);
    capture.abort = true;
    assert(!openai_image_sse_feed(&parser, valid, strlen(valid)));
    assert(!openai_image_sse_complete(&parser));
}

int main(void)
{
    test_split_events();
    test_large_image();
    test_missing_and_truncated_image();
    test_errors();
    puts("OpenAI image SSE parser tests passed");
    return 0;
}
