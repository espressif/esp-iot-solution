/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdbool.h>

#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/uart.h"

#include "protocomm_ext.h"
#include "protocomm_ext_security.h"
#include "protocomm_ext_console.h"

/* Default Unity tearDown threshold. Crypto cases may leave a small constant
 * residual and use CRYPTO_LEAKS_THRESHOLD_BYTES with tearDown skipped.
 *
 * NimBLE host/controller init+deinit correctly tears the stack down, but
 * ESP-IDF still leaves multi-KB residual that Unity would flag as a leak.
 * Cases that bring BLE up therefore skip the generic tearDown leak check;
 * leak coverage for the component focuses on HTTP/console paths. */
#define LEAKS_THRESHOLD_BYTES        (400)
#define CRYPTO_LEAKS_THRESHOLD_BYTES (1024)

static bool s_skip_teardown_leak_check;

static const char pop[] = "test_pop";
static const char sec2_user[] = "wifiprov";
static const char sec2_pass[] = "abcd1234";

static protocomm_ext_security1_params_t sec1_params = {
    .data = (const uint8_t *)pop,
    .len = sizeof(pop) - 1,
};

static protocomm_ext_security2_params_t sec2_params = {
    .username = sec2_user,
    .username_len = sizeof(sec2_user) - 1,
    .password = sec2_pass,
    .password_len = sizeof(sec2_pass) - 1,
};

static const esp_http_client_config_t http_cfg = {
    .url = "http://127.0.0.1",
    .timeout_ms = 50,
};

static const protocomm_ext_console_config_t console_cfg = {
    .uart_num = UART_NUM_1,
    .tx_io_num = 4,
    .rx_io_num = 5,
    .baud_rate = 115200,
    .timeout_ms = 100,
};

static protocomm_ext_t *create_http_pc(protocomm_ext_security_method_t sec,
                                       void *sec_data)
{
    protocomm_ext_config_data_t cfg = {
        .transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_HTTP,
        .security_method = sec,
        .transport_data = (void *) &http_cfg,
        .security_data = sec_data,
    };
    return protocomm_ext_new(&cfg);
}

static protocomm_ext_t *create_ble_pc(void)
{
    protocomm_ext_config_data_t cfg = {
        .transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_BLE,
        .security_method = PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1,
        .transport_data = NULL,
        .security_data = (void *) &sec1_params,
    };
    return protocomm_ext_new(&cfg);
}

static protocomm_ext_t *create_console_pc(void)
{
    protocomm_ext_config_data_t cfg = {
        .transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_CONSOLE,
        .security_method = PROTOCOMM_EXT_SECURITY_METHOD_NONE,
        .transport_data = (void *) &console_cfg,
        .security_data = NULL,
    };
    return protocomm_ext_new(&cfg);
}

/* Absorb one-time mbedtls/SRP allocations before leak-sensitive crypto tests. */
static void warmup_security_crypto(void)
{
    protocomm_ext_t *pc = create_http_pc(PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1, &sec1_params);
    if (!pc) {
        return;
    }
    (void)protocomm_ext_security_init(pc);
    (void)protocomm_ext_set_security(pc, PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_2, &sec2_params);
    (void)protocomm_ext_security_init(pc);
    protocomm_ext_delete(pc);
}

TEST_CASE("protocomm_ext_new: invalid config returns NULL", "[protocomm_ext]")
{
    TEST_ASSERT_NULL(protocomm_ext_new(NULL));
}

TEST_CASE("protocomm_ext_new: http transport requires transport_data", "[protocomm_ext]")
{
    protocomm_ext_config_data_t cfg = {
        .transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_HTTP,
        .security_method = PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1,
        .transport_data = NULL,
        .security_data = NULL,
    };
    TEST_ASSERT_NULL(protocomm_ext_new(&cfg));
}

TEST_CASE("protocomm_ext_new: security2 requires username/password", "[protocomm_ext]")
{
    protocomm_ext_config_data_t cfg = {
        .transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_HTTP,
        .security_method = PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_2,
        .transport_data = (void *) &http_cfg,
        .security_data = NULL,
    };
    TEST_ASSERT_NULL(protocomm_ext_new(&cfg));
}

TEST_CASE("protocomm_ext_new: http + sec0/sec1/sec2 create OK", "[protocomm_ext]")
{
    protocomm_ext_t *pc0 = create_http_pc(PROTOCOMM_EXT_SECURITY_METHOD_NONE, NULL);
    TEST_ASSERT_NOT_NULL(pc0);
    protocomm_ext_delete(pc0);

    protocomm_ext_t *pc1 = create_http_pc(PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1, &sec1_params);
    TEST_ASSERT_NOT_NULL(pc1);
    protocomm_ext_delete(pc1);

    protocomm_ext_t *pc2 = create_http_pc(PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_2, &sec2_params);
    TEST_ASSERT_NOT_NULL(pc2);
    protocomm_ext_delete(pc2);
}

TEST_CASE("protocomm_ext_new: console transport create OK", "[protocomm_ext]")
{
    protocomm_ext_t *pc = create_console_pc();
    TEST_ASSERT_NOT_NULL(pc);
    protocomm_ext_delete(pc);
}

TEST_CASE("protocomm_ext_open_session: ble transport requires config", "[protocomm_ext]")
{
    /* ESP-IDF NimBLE teardown leaves multi-KB residual; skip Unity leak check. */
    s_skip_teardown_leak_check = true;

    protocomm_ext_t *pc = create_ble_pc();
    TEST_ASSERT_NOT_NULL(pc);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, protocomm_ext_open_session(pc, NULL));
    protocomm_ext_delete(pc);
}

TEST_CASE("protocomm_ext_open_session/close_session basic flow", "[protocomm_ext]")
{
    /* HTTP open/close does not need a peer (connect is deferred until send). */
    protocomm_ext_t *pc = create_http_pc(PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1, &sec1_params);
    TEST_ASSERT_NOT_NULL(pc);

    TEST_ASSERT_EQUAL(ESP_OK, protocomm_ext_open_session(pc, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, protocomm_ext_close_session(pc));
    protocomm_ext_delete(pc);

    /* BLE create/delete only — real GAP connect needs a live peer (integration).
     * Skip tearDown leak check: NimBLE controller residual after full deinit. */
    s_skip_teardown_leak_check = true;
    pc = create_ble_pc();
    TEST_ASSERT_NOT_NULL(pc);
    protocomm_ext_delete(pc);
}

TEST_CASE("protocomm_ext_open_session: NULL pc returns ESP_ERR_INVALID_ARG", "[protocomm_ext]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, protocomm_ext_open_session(NULL, NULL));
}

TEST_CASE("protocomm_ext_close_session: NULL pc returns ESP_ERR_INVALID_ARG", "[protocomm_ext]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, protocomm_ext_close_session(NULL));
}

TEST_CASE("protocomm_ext_send_data: missing security session returns ESP_ERR_INVALID_STATE", "[protocomm_ext]")
{
    protocomm_ext_t *pc = create_http_pc(PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1, &sec1_params);
    TEST_ASSERT_NOT_NULL(pc);
    TEST_ASSERT_EQUAL(ESP_OK, protocomm_ext_open_session(pc, NULL));

    uint8_t *out = NULL;
    size_t out_len = 0;
    const uint8_t payload[] = "hello";

    esp_err_t ret = protocomm_ext_send_data(pc, "prov-config", payload, sizeof(payload) - 1, &out, &out_len);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    protocomm_ext_close_session(pc);
    protocomm_ext_delete(pc);
}

TEST_CASE("protocomm_ext_establish_security: sec0 handshake build succeeds locally", "[protocomm_ext]")
{
    /* Without a peer, transport send fails — but init + command0 build path is exercised
     * via establish_security returning a transport error rather than crashing.
     * Requires esp_netif/lwIP (initialized in app_main). */
    protocomm_ext_t *pc = create_http_pc(PROTOCOMM_EXT_SECURITY_METHOD_NONE, NULL);
    TEST_ASSERT_NOT_NULL(pc);
    TEST_ASSERT_EQUAL(ESP_OK, protocomm_ext_open_session(pc, NULL));

    esp_err_t ret = protocomm_ext_establish_security(pc, "prov-session");
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    protocomm_ext_close_session(pc);
    protocomm_ext_delete(pc);
}

TEST_CASE("protocomm_ext: create/open/close/delete has no memory leak", "[protocomm_ext][leaks]")
{
    /* Leak check covers HTTP/console-owned memory only. Repeated NimBLE
     * init/deinit is a functional path (covered above) but ESP-IDF leaves
     * controller residual that is not a protocomm_ext leak. */
    s_skip_teardown_leak_check = true;
    unity_utils_record_free_mem();

    for (int i = 0; i < 10; i++) {
        protocomm_ext_t *pc = create_http_pc(PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1, &sec1_params);
        TEST_ASSERT_NOT_NULL(pc);
        TEST_ASSERT_EQUAL(ESP_OK, protocomm_ext_open_session(pc, NULL));
        TEST_ASSERT_EQUAL(ESP_OK, protocomm_ext_close_session(pc));
        protocomm_ext_delete(pc);
    }

    for (int i = 0; i < 5; i++) {
        protocomm_ext_t *pc = create_http_pc(PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_2, &sec2_params);
        TEST_ASSERT_NOT_NULL(pc);
        protocomm_ext_delete(pc);
    }

    for (int i = 0; i < 5; i++) {
        protocomm_ext_t *pc = create_console_pc();
        TEST_ASSERT_NOT_NULL(pc);
        protocomm_ext_delete(pc);
    }

    unity_utils_evaluate_leaks_direct(LEAKS_THRESHOLD_BYTES);
}

TEST_CASE("protocomm_ext_set_security: switch sec0 to sec1 without leak", "[protocomm_ext][leaks]")
{
    s_skip_teardown_leak_check = true;
    unity_utils_record_free_mem();

    protocomm_ext_t *pc = create_http_pc(PROTOCOMM_EXT_SECURITY_METHOD_NONE, NULL);
    TEST_ASSERT_NOT_NULL(pc);

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(ESP_OK,
                          protocomm_ext_set_security(pc, PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1, &sec1_params));
        TEST_ASSERT_EQUAL(ESP_OK, protocomm_ext_security_init(pc));
        TEST_ASSERT_EQUAL(ESP_OK,
                          protocomm_ext_set_security(pc, PROTOCOMM_EXT_SECURITY_METHOD_NONE, NULL));
    }

    TEST_ASSERT_EQUAL(ESP_OK,
                      protocomm_ext_set_security(pc, PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_2, &sec2_params));
    TEST_ASSERT_EQUAL(ESP_OK, protocomm_ext_security_init(pc));
    protocomm_ext_delete(pc);

    unity_utils_evaluate_leaks_direct(CRYPTO_LEAKS_THRESHOLD_BYTES);
}

TEST_CASE("protocomm_ext_set_security: NULL pc returns ESP_ERR_INVALID_ARG", "[protocomm_ext]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      protocomm_ext_set_security(NULL, PROTOCOMM_EXT_SECURITY_METHOD_NONE, NULL));
}

TEST_CASE("protocomm_ext_delete: NULL is safe", "[protocomm_ext]")
{
    protocomm_ext_delete(NULL);
}

void setUp(void)
{
    s_skip_teardown_leak_check = false;
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    if (!s_skip_teardown_leak_check) {
        unity_utils_evaluate_leaks_direct(LEAKS_THRESHOLD_BYTES);
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    warmup_security_crypto();

    printf("Press ENTER to see the list of tests.\n");
    unity_run_menu();
}
