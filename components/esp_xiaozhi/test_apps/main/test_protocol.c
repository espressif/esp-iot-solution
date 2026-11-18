/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "unity.h"
#include "cJSON.h"
#include "esp_xiaozhi_chat.h"
#include "esp_xiaozhi_info_priv.h"
#include "esp_xiaozhi_protocol_priv.h"
#include "esp_xiaozhi_payload.h"
#include "esp_xiaozhi_websocket.h"
#include "esp_xiaozhi_udp.h"
#include "esp_xiaozhi_udp_probe.h"

typedef struct {
    SemaphoreHandle_t received;
    const uint8_t *expected;
    size_t length;
    uint32_t timestamp;
    bool valid;
} udp_audio_test_t;

static void udp_audio_received(void *ctx, const uint8_t *data, size_t length, uint32_t timestamp)
{
    udp_audio_test_t *state = ctx;
    state->valid = length == state->length && timestamp == state->timestamp && !memcmp(state->expected, data, length);
    xSemaphoreGive(state->received);
}

TEST_CASE("UDP single receive buffer decrypts full and partial AES blocks", "[xiaozhi_protocol]")
{
    int server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT_GREATER_OR_EQUAL(0, server);
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)}};
    TEST_ASSERT_EQUAL(0, bind(server, (struct sockaddr *)&address, sizeof(address)));
    socklen_t address_size = sizeof(address);
    TEST_ASSERT_EQUAL(0, getsockname(server, (struct sockaddr *)&address, &address_size));
    struct timeval timeout = {.tv_sec = 1};
    TEST_ASSERT_EQUAL(0, setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)));
    uint8_t *audio = malloc(CONFIG_XIAOZHI_UDP_MAX_AUDIO_PAYLOAD_SIZE);
    uint8_t *packet = malloc(CONFIG_XIAOZHI_UDP_MAX_AUDIO_PAYLOAD_SIZE + 16);
    TEST_ASSERT_NOT_NULL(audio);
    TEST_ASSERT_NOT_NULL(packet);
    for (size_t i = 0; i < CONFIG_XIAOZHI_UDP_MAX_AUDIO_PAYLOAD_SIZE; i++) {
        audio[i] = (uint8_t)(i * 13);
    }
    udp_audio_test_t state = {.received = xSemaphoreCreateBinary(), .expected = audio};
    TEST_ASSERT_NOT_NULL(state.received);
    esp_xiaozhi_udp_handle_t udp;
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_udp_init(&udp, udp_audio_received, NULL, &state));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_udp_configure(udp, "127.0.0.1", ntohs(address.sin_port),
                                                        "000102030405060708090a0b0c0d0e0f", "01000000010203040000000000000000", false));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_udp_open(udp));
    size_t largest = CONFIG_XIAOZHI_UDP_MAX_AUDIO_PAYLOAD_SIZE;
    if (largest + 16 > CONFIG_XIAOZHI_UDP_RECEIVE_BUFFER_SIZE) {
        largest = CONFIG_XIAOZHI_UDP_RECEIVE_BUFFER_SIZE - 16;
    }
    const size_t lengths[] = {1, 15, 16, 17, 63, 64, 65, largest};
    for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        state.length = lengths[i];
        state.timestamp = 0x12340000 + i;
        TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_udp_send(udp, audio, state.length, state.timestamp));
        struct sockaddr_in peer;
        socklen_t peer_size = sizeof(peer);
        int received = recvfrom(server, packet, CONFIG_XIAOZHI_UDP_MAX_AUDIO_PAYLOAD_SIZE + 16, 0,
                                (struct sockaddr *)&peer, &peer_size);
        TEST_ASSERT_EQUAL(state.length + 16, received);
        TEST_ASSERT_EQUAL(received, sendto(server, packet, received, 0, (struct sockaddr *)&peer, peer_size));
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(state.received, pdMS_TO_TICKS(1000)));
        TEST_ASSERT_TRUE(state.valid);
    }
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_udp_deinit(udp));
    vSemaphoreDelete(state.received);
    free(audio);
    free(packet);
    close(server);
}

TEST_CASE("protocol capabilities share one source and copy font metadata", "[xiaozhi_protocol]")
{
    char bundle[] = "font-bundle";
    esp_xiaozhi_protocol_capabilities_t caps = {
        .mcp = true, .mcp_20260728 = true, .udp_probe = true, .aec = true,
        .glyph_push = true, .font_bundle = bundle, .font_charset = "unicode", .font_size = 24, .font_bpp = 4,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_protocol_set_capabilities(&caps));
    bundle[0] = 'X';
    cJSON *hello = cJSON_CreateObject();
    cJSON *bootstrap = cJSON_CreateObject();
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_protocol_add_capabilities(hello, true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_protocol_add_capabilities(bootstrap, false));
    TEST_ASSERT_TRUE(cJSON_Compare(cJSON_GetObjectItem(hello, "features"), cJSON_GetObjectItem(bootstrap, "features"), true));
    TEST_ASSERT_EQUAL_STRING("font-bundle", cJSON_GetObjectItem(cJSON_GetObjectItem(hello, "text_font"), "bundle")->valuestring);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(bootstrap, "text_font"));
    caps.mcp = false;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_xiaozhi_protocol_set_capabilities(&caps));
    cJSON_Delete(hello);
    cJSON_Delete(bootstrap);
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_protocol_set_capabilities(NULL));
}

TEST_CASE("server hello accepts an omitted or empty session ID as upstream does", "[xiaozhi_protocol]")
{
    const char *fixtures[] = {
        "{\"type\":\"hello\",\"transport\":\"websocket\"}",
        "{\"type\":\"hello\",\"transport\":\"websocket\",\"session_id\":\"\"}",
        "{\"type\":\"hello\",\"transport\":\"udp\",\"session_id\":\"session-a\"}",
    };
    const char *expected[] = {"", "", "session-a"};
    for (unsigned i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
        cJSON *hello = cJSON_Parse(fixtures[i]);
        TEST_ASSERT_NOT_NULL(hello);
        char session[32] = "previous-session";
        TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_protocol_read_session_id(hello, session, sizeof(session)));
        TEST_ASSERT_EQUAL_STRING(expected[i], session);
        cJSON_Delete(hello);
    }
}

TEST_CASE("modern malformed and notification initialize cannot mutate legacy vision", "[xiaozhi_protocol]")
{
    const char *fixtures[] = {
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"_meta\":{\"custom\":true}}}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\"}}}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":null,\"method\":\"initialize\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":1.5,\"method\":\"initialize\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":false}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"_meta\":false}}",
    };
    for (unsigned i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
        cJSON *request = cJSON_Parse(fixtures[i]);
        TEST_ASSERT_NOT_NULL(request);
        TEST_ASSERT_EQUAL(i < 2, esp_xiaozhi_protocol_is_legacy_initialize(request));
        cJSON_Delete(request);
    }
}

TEST_CASE("WebSocket v1 v2 v3 audio framing round trips and validates boundaries", "[xiaozhi_protocol]")
{
    const uint8_t audio[] = {0x12, 0x34, 0x56};
    uint8_t frame[32];
    size_t written;
    for (int version = 1; version <= 3; version++) {
        TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_protocol_encode_audio(version, audio, sizeof(audio), 0x12345678, frame, sizeof(frame), &written));
        const uint8_t *payload;
        size_t length;
        uint32_t timestamp;
        unsigned type;
        TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_protocol_decode_frame(version, frame, written, &payload, &length, &timestamp, &type));
        TEST_ASSERT_EQUAL(sizeof(audio), length);
        TEST_ASSERT_EQUAL_HEX8_ARRAY(audio, payload, length);
        TEST_ASSERT_EQUAL_UINT32(version == 2 ? 0x12345678 : 0, timestamp);
        TEST_ASSERT_EQUAL(0, type);
        if (version > 1) {
            TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_xiaozhi_protocol_decode_frame(version, frame, written - 1, &payload, &length, &timestamp, &type));
            frame[version == 2 ? 3 : 0] = 1;
            TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_protocol_decode_frame(version, frame, written, &payload, &length, &timestamp, &type));
            TEST_ASSERT_EQUAL(1, type);
            frame[version == 2 ? 3 : 0] = 2;
            TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_xiaozhi_protocol_decode_frame(version, frame, written, &payload, &length, &timestamp, &type));
        }
    }
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, esp_xiaozhi_protocol_encode_audio(3, audio, UINT16_MAX + 1U, 0, frame, SIZE_MAX, &written));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, esp_xiaozhi_protocol_encode_audio(2, audio, sizeof(audio), 0, frame, 16, &written));
}

TEST_CASE("fragment assembly rejects gaps overlap and oversize and recovers", "[xiaozhi_protocol]")
{
    esp_xiaozhi_payload_t payload = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_payload_append(&payload, "abc", 3, 0, 6, 8));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_xiaozhi_payload_append(&payload, "d", 1, 4, 6, 8));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_xiaozhi_payload_append(&payload, "d", 1, 2, 6, 8));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_payload_append(&payload, "def", 3, 3, 6, 8));
    TEST_ASSERT_EQUAL_MEMORY("abcdef", payload.buf, 6);
    esp_xiaozhi_payload_clear(&payload);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, esp_xiaozhi_payload_append(&payload, "abcdefghi", 9, 0, 9, 8));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_payload_append(&payload, "x", 1, 0, 1, 8));
    esp_xiaozhi_payload_clear(&payload);
}

TEST_CASE("UDP probe retries at 0 1 3 seconds and fails at deadline", "[xiaozhi_protocol]")
{
    esp_xiaozhi_udp_probe_t probe;
    const uint8_t ssrc[] = {0x11, 0x22, 0x33, 0x44};
    esp_xiaozhi_udp_probe_init(&probe, ssrc);
    TEST_ASSERT_EQUAL(ESP_XIAOZHI_PROBE_SEND, esp_xiaozhi_udp_probe_poll(&probe, 0));
    uint8_t ping[32];
    memcpy(ping, probe.packet, sizeof(ping));
    TEST_ASSERT_EQUAL(1, probe.sequence);
    TEST_ASSERT_EQUAL(ESP_XIAOZHI_PROBE_NONE, esp_xiaozhi_udp_probe_poll(&probe, 999999));
    TEST_ASSERT_EQUAL(ESP_XIAOZHI_PROBE_SEND, esp_xiaozhi_udp_probe_poll(&probe, 1000000));
    TEST_ASSERT_EQUAL(ESP_XIAOZHI_PROBE_NONE, esp_xiaozhi_udp_probe_poll(&probe, 2999999));
    TEST_ASSERT_EQUAL(ESP_XIAOZHI_PROBE_SEND, esp_xiaozhi_udp_probe_poll(&probe, 3000000));
    TEST_ASSERT_EQUAL_MEMORY(ping, probe.packet, sizeof(ping));
    TEST_ASSERT_EQUAL(3, probe.attempts);
    TEST_ASSERT_EQUAL(ESP_XIAOZHI_PROBE_NONE, esp_xiaozhi_udp_probe_poll(&probe, 4999999));
    TEST_ASSERT_EQUAL(ESP_XIAOZHI_PROBE_FAILED, esp_xiaozhi_udp_probe_poll(&probe, 5000000));
    TEST_ASSERT_EQUAL(ESP_XIAOZHI_PROBE_NONE, esp_xiaozhi_udp_probe_poll(&probe, 6000000));
}

TEST_CASE("UDP probe matches current pending round with a strict deadline and never wraps", "[xiaozhi_protocol]")
{
    esp_xiaozhi_udp_probe_t probe;
    const uint8_t ssrc[] = {1, 2, 3, 4};
    esp_xiaozhi_udp_probe_init(&probe, ssrc);
    TEST_ASSERT_EQUAL(ESP_XIAOZHI_PROBE_SEND, esp_xiaozhi_udp_probe_poll(&probe, 100));
    uint8_t pong[32];
    memcpy(pong, probe.packet, sizeof(pong));
    pong[0] = 3;
    TEST_ASSERT_TRUE(esp_xiaozhi_udp_probe_matches(&probe, pong, sizeof(pong), 5000099));
    TEST_ASSERT_FALSE(esp_xiaozhi_udp_probe_matches(&probe, pong, sizeof(pong), 5000100));
    pong[8] = 1;
    TEST_ASSERT_FALSE(esp_xiaozhi_udp_probe_matches(&probe, pong, sizeof(pong), 200));
    pong[8] = 0;
    TEST_ASSERT_FALSE(esp_xiaozhi_udp_probe_matches(&probe, pong, 31, 200));
    esp_xiaozhi_udp_probe_complete(&probe, 200, 1500000);
    TEST_ASSERT_FALSE(esp_xiaozhi_udp_probe_matches(&probe, pong, 32, 201));
    TEST_ASSERT_EQUAL(ESP_XIAOZHI_PROBE_NONE, esp_xiaozhi_udp_probe_poll(&probe, 16500199));
    TEST_ASSERT_EQUAL(ESP_XIAOZHI_PROBE_SEND, esp_xiaozhi_udp_probe_poll(&probe, 16500200));
    TEST_ASSERT_EQUAL(2, probe.sequence);
    TEST_ASSERT_FALSE(esp_xiaozhi_udp_probe_matches(&probe, pong, 32, 16500201));
    probe.pending = false;
    probe.sequence = UINT32_MAX;
    TEST_ASSERT_EQUAL(ESP_XIAOZHI_PROBE_FAILED, esp_xiaozhi_udp_probe_poll(&probe, 40000000));
}

TEST_CASE("UDP probe uses raw key header only and authenticates type", "[xiaozhi_protocol]")
{
    uint8_t key[16];
    for (size_t i = 0; i < sizeof(key); i++) {
        key[i] = i;
    }
    uint8_t header[] = {2, 0, 0, 16, 0x11, 0x22, 0x33, 0x44, 0, 0, 0, 0, 0, 0, 0, 1};
    const uint8_t expected_ping[] = {0xda, 0xef, 0x4d, 0xd6, 0x3a, 0x34, 0xfe, 0xb1, 0x52, 0xd1, 0x60, 0x95, 0x50, 0x0e, 0x95, 0x06};
    const uint8_t expected_pong[] = {0x7c, 0xd8, 0x50, 0x17, 0x61, 0x35, 0x5f, 0x10, 0x09, 0x44, 0x7b, 0xad, 0x2c, 0x4f, 0x8a, 0x56};
    uint8_t tag[16];
    TEST_ASSERT_TRUE(esp_xiaozhi_udp_probe_mac(key, header, tag));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_ping, tag, sizeof(tag));
    header[0] = 3;
    TEST_ASSERT_TRUE(esp_xiaozhi_udp_probe_mac(key, header, tag));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_pong, tag, sizeof(tag));
}

static esp_err_t sign_challenge(void *ctx, const uint8_t *challenge, size_t length, uint8_t hmac[32])
{
    TEST_ASSERT_EQUAL_STRING("challenge", ctx);
    TEST_ASSERT_EQUAL_MEMORY("challenge", challenge, length);
    for (unsigned i = 0; i < 32; i++) {
        hmac[i] = i;
    }
    return ESP_OK;
}

TEST_CASE("UDP configuration rejects invalid keys ports and probe tuple reuse", "[xiaozhi_protocol]")
{
    esp_xiaozhi_udp_handle_t udp;
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_udp_init(&udp, NULL, NULL, NULL));
    const char *key = "000102030405060708090a0b0c0d0e0f";
    const char *nonce = "01000000112233440000000000000000";
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_xiaozhi_udp_configure(udp, "localhost", 0, key, nonce, true));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_xiaozhi_udp_configure(udp, "localhost", 65536, key, nonce, true));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_xiaozhi_udp_configure(udp, "localhost", 8000, "bad", nonce, true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_udp_configure(udp, "localhost", 8000, key, nonce, true));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_xiaozhi_udp_configure(udp, "localhost", 8000, key, nonce, true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_udp_configure(udp, "localhost", 8000, key,
                                                        "01000000112233450000000000000000", true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_udp_deinit(udp));
}

TEST_CASE("activation v1 v2 signing and error handling", "[xiaozhi_protocol]")
{
    esp_xiaozhi_chat_info_t info = {.has_activation_challenge = true, .activation_challenge = "challenge"};
    char *payload = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_info_build_activation(&info, NULL, &payload));
    TEST_ASSERT_EQUAL_STRING("{}", payload);
    cJSON_free(payload);
    esp_xiaozhi_chat_bootstrap_config_t config = {.serial_number = "serial-123", .sign_challenge = sign_challenge, .ctx = "challenge"};
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_info_build_activation(&info, &config, &payload));
    cJSON *root = cJSON_Parse(payload);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_STRING("hmac-sha256", cJSON_GetObjectItem(root, "algorithm")->valuestring);
    TEST_ASSERT_EQUAL_STRING("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", cJSON_GetObjectItem(root, "hmac")->valuestring);
    cJSON_Delete(root);
    cJSON_free(payload);
    config.sign_challenge = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_xiaozhi_info_build_activation(&info, &config, &payload));
    TEST_ASSERT_NULL(payload);
    config.serial_number = "   ";
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_xiaozhi_info_build_activation(&info, &config, &payload));
}

TEST_CASE("bootstrap parses configuration and rejects trailing JSON", "[xiaozhi_protocol]")
{
    esp_xiaozhi_chat_info_t info = {.current_version = strdup("1.0.0")};
    TEST_ASSERT_NOT_NULL(info.current_version);
    const char *response = "{\"activation\":{\"code\":\"123\",\"challenge\":\"challenge\",\"timeout_ms\":30000},\"mqtt\":{\"endpoint\":\"host:8883\",\"keepalive\":240},\"firmware\":{\"version\":\"9.0.0\",\"url\":\"https://example.com/fw.bin\"}}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_info_parse_response(response, strlen(response), &info));
    TEST_ASSERT_TRUE(info.has_mqtt_config);
    TEST_ASSERT_TRUE(info.has_activation_challenge);
    TEST_ASSERT_TRUE(info.has_new_version);
    TEST_ASSERT_EQUAL_STRING("123", info.activation_code);
    TEST_ASSERT_EQUAL(30000, info.activation_timeout_ms);
    free(info.current_version);
    info.current_version = strdup("v2.7-dirty");
    TEST_ASSERT_NOT_NULL(info.current_version);
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_info_parse_response(response, strlen(response), &info));
    TEST_ASSERT_FALSE(info.has_new_version);
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_info_parse_response("{\"firmware\":{\"version\":\"bad\",\"url\":\"https://example.com/fw.bin\",\"force\":1}}",
                                                              sizeof("{\"firmware\":{\"version\":\"bad\",\"url\":\"https://example.com/fw.bin\",\"force\":1}}") - 1, &info));
    TEST_ASSERT_TRUE(info.has_new_version);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, esp_xiaozhi_info_parse_response("{} {}", 5, &info));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_chat_free_info(&info));
}

TEST_CASE("modern vision is request scoped and rejects malformed endpoints without fallback", "[xiaozhi_protocol]")
{
    esp_xiaozhi_chat_config_t config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();
    config.mcp_engine = NULL;
    esp_xiaozhi_chat_handle_t chat;
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_chat_init(&config, &chat));
    esp_mcp_request_context_t request = {.protocol_version = "2026-07-28"};
    char url[128], token[64];
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_xiaozhi_chat_get_request_vision(chat, &request, url, sizeof(url), token, sizeof(token)));
    request.meta_json = "{\"io.modelcontextprotocol/clientCapabilities\":{\"extensions\":{\"me.xiaozhi/vision\":{\"url\":\"https://example.com/explain\",\"token\":\"request-token\"}}}}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_chat_get_request_vision(chat, &request, url, sizeof(url), token, sizeof(token)));
    TEST_ASSERT_EQUAL_STRING("request-token", token);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, esp_xiaozhi_chat_get_request_vision(chat, &request, url, 4, token, sizeof(token)));
    request.meta_json = "{\"io.modelcontextprotocol/clientCapabilities\":{\"extensions\":{\"me.xiaozhi/vision\":{\"url\":\"file:///tmp/x\"}}}}";
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_xiaozhi_chat_get_request_vision(chat, &request, url, sizeof(url), token, sizeof(token)));
    TEST_ASSERT_FALSE(esp_xiaozhi_chat_is_audio_channel_opened(chat));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_chat_deinit(chat));
}

static unsigned frame_messages;
static char frame_message[32];

static esp_err_t receive_frame(esp_xiaozhi_transport_handle_t transport, const char *topic, size_t topic_length,
                               const char *data, size_t length)
{
    (void)transport;
    (void)topic;
    (void)topic_length;
    frame_messages++;
    size_t copy = length < sizeof(frame_message) - 1 ? length : sizeof(frame_message) - 1;
    memcpy(frame_message, data, copy);
    frame_message[copy] = '\0';
    return ESP_OK;
}

TEST_CASE("WebSocket chunks continuations FIN and interleaved PING dispatch exactly once", "[xiaozhi_protocol]")
{
    esp_xiaozhi_websocket_handle_t websocket;
    frame_messages = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_websocket_init(&websocket));
    esp_xiaozhi_transport_callbacks_t callbacks = {.on_message_callback = receive_frame};
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_websocket_set_callbacks(websocket, 1, &callbacks));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_websocket_feed_frame(websocket, 1, false, "a", 1, 0, 2));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_websocket_feed_frame(websocket, 1, false, "b", 1, 1, 2));
    TEST_ASSERT_EQUAL(0, frame_messages);
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_websocket_feed_frame(websocket, 0, true, "c", 1, 0, 2));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_websocket_feed_frame(websocket, 9, true, "ping", 4, 0, 4));
    TEST_ASSERT_EQUAL(0, frame_messages);
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_websocket_feed_frame(websocket, 0, true, "d", 1, 1, 2));
    TEST_ASSERT_EQUAL(1, frame_messages);
    TEST_ASSERT_EQUAL_STRING("abcd", frame_message);
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_websocket_feed_frame(websocket, 1, true, "next", 4, 0, 4));
    TEST_ASSERT_EQUAL(2, frame_messages);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_xiaozhi_websocket_feed_frame(websocket, 0, true, "bad", 3, 0, 3));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_websocket_feed_frame(websocket, 1, true, "recovered", 9, 0, 9));
    TEST_ASSERT_EQUAL(3, frame_messages);
    TEST_ASSERT_EQUAL_STRING("recovered", frame_message);
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_websocket_deinit(websocket));
}
