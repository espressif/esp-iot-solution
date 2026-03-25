/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"

#include "esp_xiaozhi_chat.h"

/* Allow at most this many bytes less free memory after the test (signed delta). */
#define TEST_MEMORY_LEAK_THRESHOLD 150

static const char *TAG = "ESP_XIAOZHI_TEST";

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    /* Use signed delta so that "allow 150 bytes leak" is correct; size_t diff would wrap. */
    intptr_t delta = (intptr_t)after_free - (intptr_t)before_free;
    printf("MALLOC_CAP_%s usage: Free memory delta: %ld\n", type, (long)delta);
    TEST_ASSERT_MESSAGE(delta >= -TEST_MEMORY_LEAK_THRESHOLD, "memory leak detected");
}

static void dummy_audio_cb(const uint8_t *data, int len, void *ctx)
{
    (void)data;
    (void)len;
    (void)ctx;
}

static void dummy_event_cb(esp_xiaozhi_chat_event_t event, void *event_data, void *ctx)
{
    (void)event;
    (void)event_data;
    (void)ctx;
}

static esp_xiaozhi_chat_config_t make_test_config(void)
{
    esp_xiaozhi_chat_config_t config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();
    config.audio_callback = NULL;
    config.event_callback = NULL;
    config.audio_callback_ctx = NULL;
    config.event_callback_ctx = NULL;
    config.mcp_engine = NULL;
    config.owns_mcp_engine = false;
    return config;
}

static void clear_nvs_namespace(const char *name_space)
{
    nvs_handle_t handle = 0;
    esp_err_t ret = nvs_open(name_space, NVS_READWRITE, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return;
    }

    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(ESP_OK, nvs_erase_all(handle));
    TEST_ASSERT_EQUAL(ESP_OK, nvs_commit(handle));
    nvs_close(handle);
}

static void write_nvs_string(const char *name_space, const char *key, const char *value)
{
    nvs_handle_t handle = 0;
    TEST_ASSERT_EQUAL(ESP_OK, nvs_open(name_space, NVS_READWRITE, &handle));
    TEST_ASSERT_EQUAL(ESP_OK, nvs_set_str(handle, key, value));
    TEST_ASSERT_EQUAL(ESP_OK, nvs_commit(handle));
    nvs_close(handle);
}

void setUp(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    before_free_8bit = esp_get_free_heap_size();
    before_free_32bit = esp_get_free_internal_heap_size();
}

void tearDown(void)
{
    size_t after_free_8bit = esp_get_free_heap_size();
    size_t after_free_32bit = esp_get_free_internal_heap_size();

    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

/**
 * @brief Test basic component initialization
 */
TEST_CASE("esp_xiaozhi_init_deinit", "[esp_xiaozhi]")
{
    esp_xiaozhi_chat_config_t config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();
    config.audio_callback = NULL;
    config.event_callback = NULL;
    config.audio_callback_ctx = NULL;
    config.event_callback_ctx = NULL;
    config.mcp_engine = NULL;

    esp_xiaozhi_chat_handle_t chat_handle = 0;
    esp_err_t     ret = esp_xiaozhi_chat_init(&config, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_EQUAL(0, chat_handle);

    ret = esp_xiaozhi_chat_deinit(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @brief Test component initialization with NULL config
 */
TEST_CASE("esp_xiaozhi_init_null_config", "[esp_xiaozhi]")
{
    esp_xiaozhi_chat_handle_t chat_handle = 0;
    esp_err_t ret = esp_xiaozhi_chat_init(NULL, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    TEST_ASSERT_EQUAL(0, chat_handle);
}

/**
 * @brief Test component initialization with NULL handle
 */
TEST_CASE("esp_xiaozhi_init_null_handle", "[esp_xiaozhi]")
{
    esp_xiaozhi_chat_config_t config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();
    config.audio_callback = NULL;
    config.event_callback = NULL;
    config.audio_callback_ctx = NULL;
    config.event_callback_ctx = NULL;
    config.mcp_engine = NULL;

    esp_err_t ret = esp_xiaozhi_chat_init(&config, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test component deinit with NULL handle
 */
TEST_CASE("esp_xiaozhi_deinit_null_handle", "[esp_xiaozhi]")
{
    esp_err_t ret = esp_xiaozhi_chat_deinit(0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test default configuration macro
 */
TEST_CASE("esp_xiaozhi_default_config", "[esp_xiaozhi]")
{
    esp_xiaozhi_chat_config_t config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();

    TEST_ASSERT_EQUAL(ESP_XIAOZHI_CHAT_AUDIO_TYPE_OPUS, config.audio_type);
    TEST_ASSERT_NULL(config.audio_callback);
    TEST_ASSERT_NULL(config.event_callback);
    TEST_ASSERT_NULL(config.audio_callback_ctx);
    TEST_ASSERT_NULL(config.event_callback_ctx);
    TEST_ASSERT_NULL(config.mcp_engine);
    TEST_ASSERT_FALSE(config.owns_mcp_engine);
    TEST_ASSERT_FALSE(config.has_mqtt_config);
    TEST_ASSERT_FALSE(config.has_websocket_config);
}

/**
 * @brief Test stop with NULL handle
 */
TEST_CASE("esp_xiaozhi_stop_null_handle", "[esp_xiaozhi]")
{
    esp_err_t ret = esp_xiaozhi_chat_stop(0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test close_audio_channel with NULL handle
 */
TEST_CASE("esp_xiaozhi_close_audio_channel_null_handle", "[esp_xiaozhi]")
{
    esp_err_t ret = esp_xiaozhi_chat_close_audio_channel(0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test open_audio_channel with NULL handle
 */
TEST_CASE("esp_xiaozhi_open_audio_channel_null_handle", "[esp_xiaozhi]")
{
    esp_err_t ret = esp_xiaozhi_chat_open_audio_channel(0, NULL, NULL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test send_audio_data with invalid arguments
 */
TEST_CASE("esp_xiaozhi_send_audio_data_invalid_args", "[esp_xiaozhi]")
{
    esp_xiaozhi_chat_config_t config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();
    config.audio_callback = NULL;
    config.event_callback = NULL;
    config.audio_callback_ctx = NULL;
    config.event_callback_ctx = NULL;
    config.mcp_engine = NULL;

    esp_xiaozhi_chat_handle_t chat_handle = 0;
    esp_err_t ret = esp_xiaozhi_chat_init(&config, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_EQUAL(0, chat_handle);

    ret = esp_xiaozhi_chat_send_audio_data(0, (const char *)"x", 1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = esp_xiaozhi_chat_send_audio_data(chat_handle, NULL, 1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = esp_xiaozhi_chat_send_audio_data(chat_handle, (const char *)"x", 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = esp_xiaozhi_chat_deinit(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @brief Test send_wake_word_detected with invalid arguments
 */
TEST_CASE("esp_xiaozhi_send_wake_word_detected_invalid_args", "[esp_xiaozhi]")
{
    esp_xiaozhi_chat_config_t config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();
    config.audio_callback = NULL;
    config.event_callback = NULL;
    config.audio_callback_ctx = NULL;
    config.event_callback_ctx = NULL;
    config.mcp_engine = NULL;

    esp_xiaozhi_chat_handle_t chat_handle = 0;
    esp_err_t ret = esp_xiaozhi_chat_init(&config, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_EQUAL(0, chat_handle);

    ret = esp_xiaozhi_chat_send_wake_word(0, "wake");
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = esp_xiaozhi_chat_send_wake_word(chat_handle, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = esp_xiaozhi_chat_deinit(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @brief Test send_start_listening with NULL handle
 */
TEST_CASE("esp_xiaozhi_send_start_listening_null_handle", "[esp_xiaozhi]")
{
    esp_err_t ret = esp_xiaozhi_chat_send_start_listening(0, ESP_XIAOZHI_CHAT_LISTENING_MODE_MANUAL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test send_stop_listening with NULL handle
 */
TEST_CASE("esp_xiaozhi_send_stop_listening_null_handle", "[esp_xiaozhi]")
{
    esp_err_t ret = esp_xiaozhi_chat_send_stop_listening(0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test send_abort_speaking with NULL handle
 */
TEST_CASE("esp_xiaozhi_send_abort_speaking_null_handle", "[esp_xiaozhi]")
{
    esp_err_t ret = esp_xiaozhi_chat_send_abort_speaking(0, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test start with NULL handle
 */
TEST_CASE("esp_xiaozhi_start_null_handle", "[esp_xiaozhi]")
{
    esp_err_t ret = esp_xiaozhi_chat_start(0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test init with callbacks set then deinit (no crash)
 */
TEST_CASE("esp_xiaozhi_init_with_callbacks_deinit", "[esp_xiaozhi]")
{
    esp_xiaozhi_chat_config_t config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();
    config.audio_callback = dummy_audio_cb;
    config.event_callback = dummy_event_cb;
    config.audio_callback_ctx = NULL;
    config.event_callback_ctx = NULL;
    config.mcp_engine = NULL;

    esp_xiaozhi_chat_handle_t chat_handle = 0;
    esp_err_t ret = esp_xiaozhi_chat_init(&config, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_EQUAL(0, chat_handle);

    ret = esp_xiaozhi_chat_deinit(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @brief Test stop on initialized but not started handle (valid handle, no mcp started)
 */
TEST_CASE("esp_xiaozhi_stop_before_start", "[esp_xiaozhi]")
{
    esp_xiaozhi_chat_config_t config = make_test_config();

    esp_xiaozhi_chat_handle_t chat_handle = 0;
    esp_err_t ret = esp_xiaozhi_chat_init(&config, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_EQUAL(0, chat_handle);

    ret = esp_xiaozhi_chat_stop(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = esp_xiaozhi_chat_deinit(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("esp_xiaozhi_single_instance_only", "[esp_xiaozhi]")
{
    esp_xiaozhi_chat_config_t config = make_test_config();
    esp_xiaozhi_chat_handle_t first = 0;
    esp_xiaozhi_chat_handle_t second = 0;

    esp_err_t ret = esp_xiaozhi_chat_init(&config, &first);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_EQUAL(0, first);

    ret = esp_xiaozhi_chat_init(&config, &second);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
    TEST_ASSERT_EQUAL(0, second);

    ret = esp_xiaozhi_chat_deinit(first);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("esp_xiaozhi_session_commands_require_session", "[esp_xiaozhi]")
{
    esp_xiaozhi_chat_config_t config = make_test_config();
    esp_xiaozhi_chat_handle_t chat_handle = 0;

    esp_err_t ret = esp_xiaozhi_chat_init(&config, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = esp_xiaozhi_chat_send_wake_word(chat_handle, "wake");
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    ret = esp_xiaozhi_chat_send_start_listening(chat_handle, ESP_XIAOZHI_CHAT_LISTENING_MODE_MANUAL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    ret = esp_xiaozhi_chat_send_stop_listening(chat_handle);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    ret = esp_xiaozhi_chat_send_abort_speaking(chat_handle, ESP_XIAOZHI_CHAT_ABORT_SPEAKING_REASON_STOP_LISTENING);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    ret = esp_xiaozhi_chat_deinit(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("esp_xiaozhi_open_audio_channel_rejects_invalid_audio_params", "[esp_xiaozhi]")
{
    esp_xiaozhi_chat_config_t config = make_test_config();
    esp_xiaozhi_chat_handle_t chat_handle = 0;

    esp_err_t ret = esp_xiaozhi_chat_init(&config, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    esp_xiaozhi_chat_audio_t invalid_format = {
        .format = "",
    };
    ret = esp_xiaozhi_chat_open_audio_channel(chat_handle, &invalid_format, NULL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    esp_xiaozhi_chat_audio_t invalid_sample_rate = {
        .sample_rate = 7999,
    };
    ret = esp_xiaozhi_chat_open_audio_channel(chat_handle, &invalid_sample_rate, NULL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    esp_xiaozhi_chat_audio_t invalid_channels = {
        .channels = 3,
    };
    ret = esp_xiaozhi_chat_open_audio_channel(chat_handle, &invalid_channels, NULL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    esp_xiaozhi_chat_audio_t invalid_frame_duration = {
        .frame_duration = 9,
    };
    ret = esp_xiaozhi_chat_open_audio_channel(chat_handle, &invalid_frame_duration, NULL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = esp_xiaozhi_chat_deinit(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("esp_xiaozhi_deinit_keeps_external_mcp_engine", "[esp_xiaozhi]")
{
    esp_mcp_t *mcp = NULL;
    esp_err_t ret = esp_mcp_create(&mcp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(mcp);

    esp_xiaozhi_chat_config_t config = make_test_config();
    config.mcp_engine = mcp;
    config.owns_mcp_engine = false;

    esp_xiaozhi_chat_handle_t chat_handle = 0;
    ret = esp_xiaozhi_chat_init(&config, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = esp_xiaozhi_chat_deinit(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = esp_mcp_destroy(mcp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("esp_xiaozhi_start_returns_not_found_when_websocket_config_missing", "[esp_xiaozhi]")
{
    clear_nvs_namespace("websocket");
    clear_nvs_namespace("mqtt");

    esp_mcp_t *mcp = NULL;
    esp_err_t ret = esp_mcp_create(&mcp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    esp_xiaozhi_chat_config_t config = make_test_config();
    config.mcp_engine = mcp;
    config.has_websocket_config = true;

    esp_xiaozhi_chat_handle_t chat_handle = 0;
    ret = esp_xiaozhi_chat_init(&config, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = esp_xiaozhi_chat_start(chat_handle);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ret);

    ret = esp_xiaozhi_chat_deinit(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = esp_mcp_destroy(mcp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("esp_xiaozhi_start_returns_not_found_when_websocket_url_missing", "[esp_xiaozhi]")
{
    clear_nvs_namespace("websocket");
    clear_nvs_namespace("mqtt");
    write_nvs_string("websocket", "token", "test-token");

    esp_mcp_t *mcp = NULL;
    esp_err_t ret = esp_mcp_create(&mcp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    esp_xiaozhi_chat_config_t config = make_test_config();
    config.mcp_engine = mcp;
    config.has_websocket_config = true;

    esp_xiaozhi_chat_handle_t chat_handle = 0;
    ret = esp_xiaozhi_chat_init(&config, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = esp_xiaozhi_chat_start(chat_handle);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ret);

    ret = esp_xiaozhi_chat_deinit(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = esp_mcp_destroy(mcp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("esp_xiaozhi_start_returns_not_found_when_mqtt_config_missing", "[esp_xiaozhi]")
{
    clear_nvs_namespace("websocket");
    clear_nvs_namespace("mqtt");

    esp_mcp_t *mcp = NULL;
    esp_err_t ret = esp_mcp_create(&mcp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    esp_xiaozhi_chat_config_t config = make_test_config();
    config.mcp_engine = mcp;
    config.has_mqtt_config = true;

    esp_xiaozhi_chat_handle_t chat_handle = 0;
    ret = esp_xiaozhi_chat_init(&config, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = esp_xiaozhi_chat_start(chat_handle);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ret);

    ret = esp_xiaozhi_chat_deinit(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = esp_mcp_destroy(mcp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("esp_xiaozhi_start_returns_not_found_when_mqtt_endpoint_missing", "[esp_xiaozhi]")
{
    clear_nvs_namespace("websocket");
    clear_nvs_namespace("mqtt");
    write_nvs_string("mqtt", "client_id", "test-client");

    esp_mcp_t *mcp = NULL;
    esp_err_t ret = esp_mcp_create(&mcp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    esp_xiaozhi_chat_config_t config = make_test_config();
    config.mcp_engine = mcp;
    config.has_mqtt_config = true;

    esp_xiaozhi_chat_handle_t chat_handle = 0;
    ret = esp_xiaozhi_chat_init(&config, &chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = esp_xiaozhi_chat_start(chat_handle);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ret);

    ret = esp_xiaozhi_chat_deinit(chat_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = esp_mcp_destroy(mcp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void app_main(void)
{
    //  NOTE: Unity test framework is used for testing
    //  Run tests with: idf.py -p <PORT> flash monitor
    ESP_LOGI(TAG, "Starting ESP Xiaozhi component tests");
    unity_run_menu();
}
