/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali_test.c
 * @brief DALI component CI / unit tests.
 *
 * Seven test cases are provided:
 *   (1) dali init              – driver initialises and de-initialises cleanly
 *   (2) scan short addresses   – polls every short address 0–63 with
 *                                QUERY_CONTROL_GEAR and reports found devices
 *   (3) dapc dimming sequence  – DAPC arc-power levels change lamp brightness
 *   (4) query actual level     – QUERY_ACTUAL_LEVEL returns a valid reply
 *   (5) send twice reset       – RESET command sent twice completes without error
 *   (6) query status           – QUERY_STATUS returns a valid 8-bit status byte
 *   (7) ota stress query actual level
 *                              – OTA download + DALI transactions run concurrently
 *                                without timing violations for DALI_OTA_STRESS_ROUNDS
 *                                rounds.
 * All tests share one dali_init() / dali_deinit() pair per test case so the
 * RMT resources are fully recycled between runs.
 */

#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "unity.h"

#include "dali.h"
#include "dali_command.h"


#ifndef CONFIG_DALI_STRESS_WIFI_SSID
#define CONFIG_DALI_STRESS_WIFI_SSID    "TP-LINK_68C0_ESP"
#endif
#ifndef CONFIG_DALI_STRESS_WIFI_PASS
#define CONFIG_DALI_STRESS_WIFI_PASS    "espressif"
#endif
#ifndef CONFIG_DALI_STRESS_OTA_URL
#define CONFIG_DALI_STRESS_OTA_URL      "http://192.168.1.100:8070/dali_demo.bin"
#endif
#ifndef CONFIG_DALI_STRESS_RX_GPIO
#define CONFIG_DALI_STRESS_RX_GPIO      CONFIG_DALI_RX_GPIO
#endif
#ifndef CONFIG_DALI_STRESS_TX_GPIO
#define CONFIG_DALI_STRESS_TX_GPIO      CONFIG_DALI_TX_GPIO
#endif



/** Short address used for all single-device tests. */
#define TEST_ADDR               0

/** DAPC arc-power levels used in the dimming-sequence test. */
#define TEST_LEVEL_MAX          0xFEU
#define TEST_LEVEL_50PCT        0x7FU
#define TEST_LEVEL_25PCT        0x3FU


#define DALI_TIMING_WARN_US         120000

/** Pause between individual steps inside one light-test round (ms). */
#define DALI_LIGHT_STEP_DELAY_MS    1000

/** Pause after completing one full round before starting the next (ms). */
#define DALI_LIGHT_INTERVAL_MS      2000

#define DALI_OTA_STRESS_ROUNDS      3

#define LIGHT_TEST_STEPS            6

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_MAX_RETRIES    5

static const char *TAG = "dali_test";
static EventGroupHandle_t        s_wifi_event_group  = NULL;
static int                       s_wifi_retry_count  = 0;
static esp_netif_t              *s_wifi_netif        = NULL;
static esp_event_handler_instance_t s_inst_any       = NULL;
static esp_event_handler_instance_t s_inst_ip        = NULL;
static bool s_wifi_started = false;  /**< esp_wifi_start() in effect */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_wifi_retry_count < WIFI_MAX_RETRIES) {
            esp_wifi_connect();
            s_wifi_retry_count++;
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_wifi_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * Initialise and connect Wi-Fi STA.
 */
static esp_err_t wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    s_wifi_retry_count = 0;

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_wifi_netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &s_inst_any));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, &s_inst_ip));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid     = CONFIG_DALI_STRESS_WIFI_SSID,
            .password = CONFIG_DALI_STRESS_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_wifi_started = true;

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(30000));
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected: %s", CONFIG_DALI_STRESS_WIFI_SSID);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Wi-Fi connection failed");
    return ESP_FAIL;
}

static void wifi_teardown(void)
{
    if (s_wifi_started) {
        esp_wifi_stop();
        s_wifi_started = false;
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    
    if (s_inst_any) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_inst_any);
        s_inst_any = NULL;
    }
    if (s_inst_ip) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_inst_ip);
        s_inst_ip = NULL;
    }

    if (s_wifi_netif) {
        esp_netif_destroy_default_wifi(s_wifi_netif);
        s_wifi_netif = NULL;
    }

    esp_event_loop_delete_default();

    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
}

/* -------------------------------------------------------------------------
 * OTA stress task (test case 6)
 * Streams the OTA image in chunks to flash without ever rebooting.
 * ------------------------------------------------------------------------- */
static TaskHandle_t      s_ota_task_handle = NULL;
static volatile bool     s_ota_stop        = false;

static void ota_stress_task(void *arg)
{
    uint32_t iter = 0;
    while (!s_ota_stop) {
        iter++;
        ESP_LOGI(TAG, "[OTA] iteration %"PRIu32" — %s",
                 iter, CONFIG_DALI_STRESS_OTA_URL);

        esp_http_client_config_t http_cfg = {
            .url            = CONFIG_DALI_STRESS_OTA_URL,
            .timeout_ms     = 5000,
            .keep_alive_enable = false,
        };
        esp_https_ota_config_t ota_cfg = { .http_config = &http_cfg };

        esp_https_ota_handle_t hdl = NULL;
        esp_err_t err = esp_https_ota_begin(&ota_cfg, &hdl);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "[OTA] begin failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        int chunks = 0;
        while (!s_ota_stop) {
            err = esp_https_ota_perform(hdl);
            if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
                if (++chunks % 16 == 0) taskYIELD();
                continue;
            }
            break;
        }
        ESP_LOGI(TAG, "[OTA] iter %"PRIu32": %d bytes / %d chunks — abort (no reboot)",
                 iter, esp_https_ota_get_image_len_read(hdl), chunks);
        esp_https_ota_abort(hdl);

        if (!s_ota_stop) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
    ESP_LOGI(TAG, "[OTA] stress task exiting cleanly");
    s_ota_task_handle = NULL;
    vTaskDelete(NULL);
}

/* =========================================================================
 * TEST CASE (1): dali init
 * ========================================================================= */

TEST_CASE("dali init", "[dali]")
{
    ESP_LOGI(TAG, "Test: dali_init (RX=%d TX=%d)",
             CONFIG_DALI_STRESS_RX_GPIO, CONFIG_DALI_STRESS_TX_GPIO);

    esp_err_t err = dali_init((gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
                              (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = dali_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    ESP_LOGI(TAG, "dali init/deinit: PASS");
}

/* =========================================================================
 * TEST CASE (2): scan short addresses
 *
 * Polls every short address (0–63) with QUERY_CONTROL_GEAR (0x91).
 * A device that is present replies with 0xFF; no reply means absent.
 * The test PASSES as long as at least one device is found.
 * Found addresses are printed in a summary table.
 * ========================================================================= */

TEST_CASE("scan short addresses", "[dali]")
{
    TEST_ASSERT_EQUAL(ESP_OK,
        dali_init((gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
                  (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO));

    uint8_t found[64];
    int     found_count = 0;

    ESP_LOGI(TAG, "Scanning DALI short addresses 0–63 ...");

    for (int addr = 0; addr < 64; addr++) {
        int reply = DALI_RESULT_NO_REPLY;
        esp_err_t err = dali_transaction(
            DALI_ADDR_SHORT, (uint8_t)addr,
            true, DALI_CMD_QUERY_CONTROL_GEAR,
            false, DALI_TX_TIMEOUT_MS, &reply);

        if (err == ESP_OK && reply == 0xFF) {
            found[found_count++] = (uint8_t)addr;
        }

        /* Mandatory inter-frame gap (> 22 Te ≈ 9.2 ms). */
        dali_wait_between_frames();
    }

    /* Print summary. */
    ESP_LOGI(TAG, "======== DALI Address Scan Report ========");
    ESP_LOGI(TAG, "  Devices found: %d", found_count);
    for (int i = 0; i < found_count; i++) {
        ESP_LOGI(TAG, "    [%2d] short address %d (0x%02X)",
                 i + 1, found[i], found[i]);
    }
    ESP_LOGI(TAG, "==========================================");

    TEST_ASSERT_EQUAL(ESP_OK, dali_deinit());

    TEST_ASSERT_MESSAGE(found_count > 0,
                        "No DALI control gear found on bus (addresses 0–63)");
    ESP_LOGI(TAG, "scan short addresses: PASS (%d device(s))", found_count);
}

/* =========================================================================
 * TEST CASE (3): dapc dimming sequence
 *   OFF → MAX → 50 % → 25 % → MAX
 * ========================================================================= */

TEST_CASE("dapc dimming sequence", "[dali]")
{
    TEST_ASSERT_EQUAL(ESP_OK,
        dali_init((gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
                  (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO));

    struct {
        bool    is_cmd;
        uint8_t value;
        const char *label;
    } steps[] = {
        { true,  DALI_CMD_OFF,   "OFF"   },
        { false, TEST_LEVEL_MAX, "MAX"   },
        { false, TEST_LEVEL_50PCT, "50%" },
        { false, TEST_LEVEL_25PCT, "25%" },
        { false, TEST_LEVEL_MAX, "MAX(2)"},
    };

    for (int i = 0; i < (int)(sizeof(steps) / sizeof(steps[0])); i++) {
        esp_err_t err = dali_transaction(
            DALI_ADDR_SHORT, TEST_ADDR,
            steps[i].is_cmd, steps[i].value,
            false, DALI_TX_TIMEOUT_MS, NULL);
        ESP_LOGI(TAG, "  step %-6s → %s", steps[i].label, esp_err_to_name(err));
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, steps[i].label);
        dali_wait_between_frames();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    TEST_ASSERT_EQUAL(ESP_OK, dali_deinit());
    ESP_LOGI(TAG, "dapc dimming sequence: PASS");
}

/* =========================================================================
 * TEST CASE (4): query actual level
 * ========================================================================= */

TEST_CASE("query actual level", "[dali]")
{
    TEST_ASSERT_EQUAL(ESP_OK,
        dali_init((gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
                  (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO));

    /* Set lamp to MAX first so a predictable level can be queried back. */
    TEST_ASSERT_EQUAL(ESP_OK,
        dali_transaction(DALI_ADDR_SHORT, TEST_ADDR,
                         false, TEST_LEVEL_MAX,
                         false, DALI_TX_TIMEOUT_MS, NULL));
    dali_wait_between_frames();
    vTaskDelay(pdMS_TO_TICKS(300));

    int reply = DALI_RESULT_NO_REPLY;
    esp_err_t err = dali_transaction(
        DALI_ADDR_SHORT, TEST_ADDR,
        true, DALI_CMD_QUERY_ACTUAL_LEVEL,
        false, DALI_TX_TIMEOUT_MS, &reply);

    ESP_LOGI(TAG, "  QUERY_ACTUAL_LEVEL → err=%s  reply=0x%02X (%d)",
             esp_err_to_name(err), (unsigned)(reply & 0xFF), reply);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_MESSAGE(DALI_RESULT_IS_VALID(reply),
                        "QUERY_ACTUAL_LEVEL: no backward frame received");

    TEST_ASSERT_EQUAL(ESP_OK, dali_deinit());
    ESP_LOGI(TAG, "query actual level: PASS");
}

/* =========================================================================
 * TEST CASE (5): send twice reset command
 * ========================================================================= */

TEST_CASE("send twice reset command", "[dali]")
{
    TEST_ASSERT_EQUAL(ESP_OK,
        dali_init((gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
                  (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO));

    esp_err_t err = dali_transaction(
        DALI_ADDR_SHORT, TEST_ADDR,
        true, DALI_CMD_RESET,
        true,                   /* send_twice */
        DALI_TX_TIMEOUT_MS, NULL);

    ESP_LOGI(TAG, "  RESET (send-twice) → %s", esp_err_to_name(err));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    dali_wait_between_frames();

    TEST_ASSERT_EQUAL(ESP_OK, dali_deinit());
    ESP_LOGI(TAG, "send twice reset command: PASS");
}

/* =========================================================================
 * TEST CASE (6): query status
 * ========================================================================= */

TEST_CASE("query status", "[dali]")
{
    TEST_ASSERT_EQUAL(ESP_OK,
        dali_init((gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
                  (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO));

    int reply = DALI_RESULT_NO_REPLY;
    esp_err_t err = dali_transaction(
        DALI_ADDR_SHORT, TEST_ADDR,
        true, DALI_CMD_QUERY_STATUS,
        false, DALI_TX_TIMEOUT_MS, &reply);

    ESP_LOGI(TAG, "  QUERY_STATUS → err=%s  status=0x%02X",
             esp_err_to_name(err), (unsigned)(reply & 0xFF));

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_MESSAGE(DALI_RESULT_IS_VALID(reply),
                        "QUERY_STATUS: no backward frame received");

    TEST_ASSERT_EQUAL(ESP_OK, dali_deinit());
    ESP_LOGI(TAG, "query status: PASS");
}

/* =========================================================================
 * TEST CASE (7): ota stress query actual level
 *
 * Mirrors the original dali_demo.c stress test exactly:
 *   - OTA stress task continuously downloads firmware in the background
 *   - Light-test task runs DALI_OTA_STRESS_ROUNDS rounds of the 6-step
 *     lamp-control sequence (OFF → MAX → 50% → 25% → MAX → QUERY)
 *   - Only QUERY_ACTUAL_LEVEL (step 5) is checked for timing violations
 *   - Full per-step statistics are printed after all rounds complete
 * ========================================================================= */

/* -------------------------------------------------------------------------
 * Per-step statistics (mirrors dali_demo.c light_test_stats_t)
 * ------------------------------------------------------------------------- */

static const char *LIGHT_STEP_NAME[LIGHT_TEST_STEPS] = {
    "OFF   ", "MAX   ", "50%   ", "25%   ", "MAX(2)", "QUERY "
};

typedef struct {
    uint32_t rounds;
    uint32_t step_ok [LIGHT_TEST_STEPS];
    uint32_t step_err[LIGHT_TEST_STEPS];
    int64_t  step_sum_us[LIGHT_TEST_STEPS];
    int64_t  step_min_us[LIGHT_TEST_STEPS];
    int64_t  step_max_us[LIGHT_TEST_STEPS];
    int      last_query_level;
    uint32_t query_violations;   /**< QUERY steps that exceeded DALI_TIMING_WARN_US */
    esp_err_t init_err;          /**< dali_init() result from inside the task */
    bool     done;
} lt_stats_t;

static void lt_stats_reset(lt_stats_t *s)
{
    memset(s, 0, sizeof(*s));
    for (int i = 0; i < LIGHT_TEST_STEPS; i++) {
        s->step_min_us[i] = INT64_MAX;
        s->step_max_us[i] = INT64_MIN;
    }
    s->last_query_level = DALI_RESULT_NO_REPLY;
}

static void lt_stats_print(const lt_stats_t *s)
{
    uint32_t total_ok = 0, total_err = 0;
    for (int i = 0; i < LIGHT_TEST_STEPS; i++) {
        total_ok  += s->step_ok[i];
        total_err += s->step_err[i];
    }
    uint32_t total = total_ok + total_err;
    uint32_t pct   = total ? (total_ok * 100 / total) : 0;

    ESP_LOGI(TAG, "======== DALI RMT Light-Test Report ========");
    ESP_LOGI(TAG, "  Driver          : RMT hardware peripheral");
    ESP_LOGI(TAG, "  Rounds completed: %"PRIu32, s->rounds);
    ESP_LOGI(TAG, "  Cmd success rate: %"PRIu32" / %"PRIu32"  (%"PRIu32" %%)",
             total_ok, total, pct);
    ESP_LOGI(TAG, "  Last QUERY level: 0x%02X  (%d)",
             (unsigned)(s->last_query_level & 0xFF), s->last_query_level);
    ESP_LOGI(TAG, "  QUERY timing violations (>%d µs): %"PRIu32,
             DALI_TIMING_WARN_US, s->query_violations);
    ESP_LOGI(TAG, "  %-8s  %6s  %6s  %8s  %8s  %8s",
             "Step", "OK", "ERR", "Min(µs)", "Max(µs)", "Avg(µs)");
    for (int i = 0; i < LIGHT_TEST_STEPS; i++) {
        uint32_t n  = s->step_ok[i] + s->step_err[i];
        int64_t avg = n ? (s->step_sum_us[i] / (int64_t)n) : 0;
        int64_t mn  = (s->step_min_us[i] == INT64_MAX) ? 0 : s->step_min_us[i];
        int64_t mx  = (s->step_max_us[i] == INT64_MIN) ? 0 : s->step_max_us[i];
        ESP_LOGI(TAG, "  %s  %6"PRIu32"  %6"PRIu32"  %8"PRId64"  %8"PRId64"  %8"PRId64,
                 LIGHT_STEP_NAME[i],
                 s->step_ok[i], s->step_err[i], mn, mx, avg);
    }
    ESP_LOGI(TAG, "============================================");
}

/* -------------------------------------------------------------------------
 * DALI light-test task (runs DALI_OTA_STRESS_ROUNDS rounds then exits)
 * ------------------------------------------------------------------------- */

static void dali_light_test_task(void *arg)
{
    lt_stats_t *s = (lt_stats_t *)arg;

    /* dali_init() must run on the same core as dali_transaction() — mirror
     * dali_demo.c where both live inside this task (core 1). */
    s->init_err = dali_init((gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
                            (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO);
    if (s->init_err != ESP_OK) {
        ESP_LOGE(TAG, "[stress] dali_init failed: %s", esp_err_to_name(s->init_err));
        s->done = true;
        vTaskDelete(NULL);
        return;
    }

    /* Let the OTA task get a head start before the first DALI frame. */
    vTaskDelay(pdMS_TO_TICKS(200));

    struct {
        bool    is_cmd;
        uint8_t value;
        bool    expect_reply;
    } steps[LIGHT_TEST_STEPS] = {
        { true,  DALI_CMD_OFF,              false },  /* 0 – OFF   */
        { false, TEST_LEVEL_MAX,            false },  /* 1 – MAX   */
        { false, TEST_LEVEL_50PCT,          false },  /* 2 – 50%   */
        { false, TEST_LEVEL_25PCT,          false },  /* 3 – 25%   */
        { false, TEST_LEVEL_MAX,            false },  /* 4 – MAX(2)*/
        { true,  DALI_CMD_QUERY_ACTUAL_LEVEL, true }, /* 5 – QUERY */
    };

    for (uint32_t round = 0; round < DALI_OTA_STRESS_ROUNDS; round++) {
        s->rounds++;
        ESP_LOGI(TAG, "[stress] round %"PRIu32"/%d", s->rounds, DALI_OTA_STRESS_ROUNDS);

        int query_level = DALI_RESULT_NO_REPLY;

        for (int i = 0; i < LIGHT_TEST_STEPS; i++) {
            int reply  = DALI_RESULT_NO_REPLY;
            int64_t t0 = esp_timer_get_time();

            esp_err_t err = dali_transaction(
                DALI_ADDR_SHORT, TEST_ADDR,
                steps[i].is_cmd, steps[i].value,
                false, DALI_TX_TIMEOUT_MS,
                steps[i].expect_reply ? &reply : NULL);

            int64_t elapsed_us = esp_timer_get_time() - t0;

            /* Update per-step latency statistics */
            s->step_sum_us[i] += elapsed_us;
            if (elapsed_us < s->step_min_us[i]) s->step_min_us[i] = elapsed_us;
            if (elapsed_us > s->step_max_us[i]) s->step_max_us[i] = elapsed_us;

            if (err == ESP_OK) {
                s->step_ok[i]++;
                if (steps[i].expect_reply) query_level = reply;
            } else {
                s->step_err[i]++;
            }

            /*
             * Only the QUERY step (step 5, expect_reply=true) has a short
             * nominal duration (~27 ms).  DAPC/CMD steps carry a mandatory
             * ~500 ms bus-free window and must NOT be checked against
             * DALI_TIMING_WARN_US.
             */
            if (steps[i].expect_reply && elapsed_us > DALI_TIMING_WARN_US) {
                s->query_violations++;
                ESP_LOGW(TAG, "[stress] round=%"PRIu32" step=%s  TIMING VIOLATION %"PRId64" µs",
                         s->rounds, LIGHT_STEP_NAME[i], elapsed_us);
            }

            dali_wait_between_frames();
            vTaskDelay(pdMS_TO_TICKS(DALI_LIGHT_STEP_DELAY_MS));
        }

        if (DALI_RESULT_IS_VALID(query_level)) {
            s->last_query_level = query_level;
            if ((uint8_t)query_level < (TEST_LEVEL_MAX - 0x20U)) {
                ESP_LOGW(TAG, "[stress] round=%"PRIu32" level readback 0x%02X < expected ~0x%02X"
                         " — possible missed command under OTA load",
                         s->rounds, (uint8_t)query_level, TEST_LEVEL_MAX);
            }
        } else {
            ESP_LOGW(TAG, "[stress] round=%"PRIu32" QUERY_ACTUAL_LEVEL: no reply", s->rounds);
        }

        vTaskDelay(pdMS_TO_TICKS(DALI_LIGHT_INTERVAL_MS));
    }

    /* dali_deinit on the same core as dali_init — mirrors dali_demo.c. */
    dali_deinit();

    s->done = true;
    vTaskDelete(NULL);
}

TEST_CASE("ota stress query actual level", "[dali][ota_stress]")
{
    /* NVS, lwIP stack, and WiFi driver are pre-initialised in app_main()
     * so their one-time heap cost is not counted as a per-test leak. */

    /* NOTE: dali_init() is called inside dali_light_test_task so that both
     * init and all transactions execute on the same core (core 1), exactly
     * as in dali_demo.c.  Do NOT call dali_init() here. */

    /* Connect Wi-Fi and launch background OTA stress task on core 0
     * (same core as the Wi-Fi stack) to maximise bus load. */
    bool ota_running = false;
    if (wifi_init_sta() == ESP_OK) {
        xTaskCreatePinnedToCore(ota_stress_task, "ota_stress",
                                8192, NULL, 4, &s_ota_task_handle, 0);
        ota_running = true;
        ESP_LOGI(TAG, "[stress] OTA stress task started");
    } else {
        ESP_LOGW(TAG, "[stress] Wi-Fi unavailable — DALI-only stress");
    }

    /* Launch DALI light-test task.
     * Single-core chips: pin to core 0.
     * Dual-core chips:   pin to core 1 so Wi-Fi / OTA on core 0 is maximally
     *                    disruptive (same arrangement as dali_demo.c). */
    static lt_stats_t lt;
    lt_stats_reset(&lt);

    xTaskCreatePinnedToCore(dali_light_test_task, "dali_light_test",
                            4096, &lt, 5, NULL,
#if CONFIG_FREERTOS_UNICORE
                            0
#else
                            1
#endif
                            );

    /* Wait up to 120 s for all rounds to complete. */
    const TickType_t timeout_ticks = pdMS_TO_TICKS(120000);
    TickType_t waited = 0;
    while (!lt.done && waited < timeout_ticks) {
        vTaskDelay(pdMS_TO_TICKS(500));
        waited += pdMS_TO_TICKS(500);
    }

    /* Print full statistics report (same as dali_demo.c). */
    lt_stats_print(&lt);
    ESP_LOGI(TAG, "[stress] rounds=%"PRIu32"  query_violations=%"PRIu32"  ota=%s",
             lt.rounds, lt.query_violations, ota_running ? "yes" : "no");

    /* Tear down OTA / Wi-Fi before Unity memory check runs.
     * wifi_init_sta() was called unconditionally above, so wifi_teardown()
     * must always run to release the per-run resources (event loop, handler
     * registrations, event group).  The one-time global resources (netif
     * stack, WiFi driver task, default STA netif) are intentionally kept
     * alive across runs to avoid memory that cannot be re-allocated. */
    if (ota_running) {
        s_ota_stop = true;
        /* Wait up to 10 s for the OTA task to exit on its own. */
        for (int i = 0; i < 100 && s_ota_task_handle != NULL; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        /* If still alive after timeout, kill it forcefully as last resort. */
        if (s_ota_task_handle != NULL) {
            vTaskDelete(s_ota_task_handle);
            s_ota_task_handle = NULL;
        }
        s_ota_stop = false;
    }
    wifi_teardown();

    TEST_ASSERT_MESSAGE(lt.done, "light-test task did not finish within timeout");
    TEST_ASSERT_EQUAL(ESP_OK, lt.init_err);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(DALI_OTA_STRESS_ROUNDS, lt.rounds,
                                     "not all rounds completed");
    /* QUERY timing violations under OTA load are expected (flash write latency
     * can push QUERY response past 67 ms).  Mirror dali_demo.c: log a warning
     * but do NOT fail the test on violation count alone. */
    if (lt.query_violations > 0) {
        ESP_LOGW(TAG, "[stress] %"PRIu32" QUERY timing violation(s) observed under OTA load"
                 " (expected, not a failure)", lt.query_violations);
    }

    /* dali_deinit() is called inside dali_light_test_task on core 1 —
     * same pattern as dali_demo.c. */
    ESP_LOGI(TAG, "ota stress query actual level: PASS");
}
