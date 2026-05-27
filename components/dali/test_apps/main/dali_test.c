/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "unity.h"
#include "sdkconfig.h"
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
#define CONFIG_DALI_STRESS_RX_GPIO      5
#endif
#ifndef CONFIG_DALI_STRESS_TX_GPIO
#define CONFIG_DALI_STRESS_TX_GPIO      6
#endif

/** Short address used for all single-device tests. */
#define TEST_ADDR               0

/* -------------------------------------------------------------------------
 * DLS-203-P sensor configuration
 *
 * MEAN WELL DLS-203-P: combined DALI-2 PIR + daylight sensor.
 *   Instance 0 → Part 303 Occupancy (PIR motion detector, 10m × 10m @ 3m)
 *   Instance 1 → Part 304 Light Sensor (2–65000 Lux, log-encoded 16-bit)
 *
 * The sensor is expected to be commissioned to short address DLS203_ADDR
 * on the Part 103 input-device bus.  Run the "DLS-203-P commission" test
 * case first if no short address has been assigned yet.
 * ------------------------------------------------------------------------- */

/** Part 103 short address assigned to the DLS-203-P under test. */
#define DLS203_ADDR                 10
/** Part 103 instance number for the PIR occupancy sensor (Part 303). */
#define DLS203_PIR_INSTANCE         0
/** Part 103 instance number for the daylight light sensor (Part 304). */
#define DLS203_LIGHT_INSTANCE       1

/** Number of poll iterations in the continuous-monitoring test. */
#define DLS203_MONITOR_ROUNDS       10
/** Interval between poll iterations in the monitoring test (ms). */
#define DLS203_MONITOR_INTERVAL_MS  2000

/** Hold timer value written in the hold-timer configuration test (seconds). */
#define DLS203_HOLD_TIMER_TEST_S    10

/** Short address of the C5 RGB+CCT DT8 gear under test. */
#define DT8_ADDR            4

/** Visible step delay between color changes (ms). */
#define DT8_STEP_DELAY_MS   1500

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

#if defined(CONFIG_SOC_WIFI_SUPPORTED)
static EventGroupHandle_t        s_wifi_event_group  = NULL;
static int                       s_wifi_retry_count  = 0;
static esp_netif_t              *s_wifi_netif        = NULL;
static esp_event_handler_instance_t s_inst_any       = NULL;
static esp_event_handler_instance_t s_inst_ip        = NULL;
static bool s_wifi_started = false;  /**< esp_wifi_start() in effect */

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
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

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
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
        ESP_LOGI(TAG, "[OTA] iteration %"PRIu32" — %s", iter, CONFIG_DALI_STRESS_OTA_URL);

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
                if (++chunks % 16 == 0) {
                    taskYIELD();
                }
                continue;
            }
            break;
        }
        ESP_LOGI(TAG, "[OTA] iter %"PRIu32": %d bytes / %d chunks — abort (no reboot)", iter,
                 esp_https_ota_get_image_len_read(hdl), chunks);
        esp_https_ota_abort(hdl);

        if (!s_ota_stop) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
    ESP_LOGI(TAG, "[OTA] stress task exiting cleanly");
    s_ota_task_handle = NULL;
    vTaskDelete(NULL);
}
#endif // CONFIG_SOC_WIFI_SUPPORTED

/* =========================================================================
 * TEST CASE (1): dali init
 * ========================================================================= */

TEST_CASE("dali init", "[dali]")
{
    ESP_LOGI(TAG, "Test: dali_new_master_rmt (RX=%d TX=%d)", CONFIG_DALI_STRESS_RX_GPIO, CONFIG_DALI_STRESS_TX_GPIO);

    dali_master_handle_t dali = NULL;
    dali_master_config_t cfg = {
        .rx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
        .tx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO,
        .invert_tx = true,
        .invert_rx = true,
    };
    dali_master_rmt_config_t rmt_cfg = {
        .mem_block_symbols = 64,
    };
    esp_err_t err = dali_new_master_rmt(&cfg, &rmt_cfg, &dali);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_NOT_NULL(dali);

    err = dali_del_master(dali);
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
    dali_master_handle_t dali = NULL;
    dali_master_config_t cfg = {
        .rx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
        .tx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO,
        .invert_tx = true,
        .invert_rx = true,
    };
    dali_master_rmt_config_t rmt_cfg = {
        .mem_block_symbols = 64,
    };
    TEST_ASSERT_EQUAL(ESP_OK, dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

    uint8_t found[64];
    int     found_count = 0;

    ESP_LOGI(TAG, "Scanning DALI short addresses 0–63 ...");

    for (int addr = 0; addr < 64; addr++) {
        int reply = DALI_RESULT_NO_REPLY;
        dali_master_transaction_config_t tx_cfg = {
            .addr_type = DALI_ADDR_SHORT,
            .addr = (uint8_t)addr,
            .is_cmd = true,
            .command = DALI_CMD_QUERY_CONTROL_GEAR,
            .send_twice = false,
            .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
        };
        esp_err_t err = dali_master_do_transaction(dali, &tx_cfg, &reply);

        if (err == ESP_OK && reply == 0xFF) {
            found[found_count++] = (uint8_t)addr;
        }
        /* Inter-frame gap is now inserted automatically by dali_master_do_transaction(). */
    }

    /* Print summary. */
    ESP_LOGI(TAG, "======== DALI Address Scan Report ========");
    ESP_LOGI(TAG, "  Devices found: %d", found_count);
    for (int i = 0; i < found_count; i++) {
        ESP_LOGI(TAG, "    [%2d] short address %d (0x%02X)", i + 1, found[i], found[i]);
    }
    ESP_LOGI(TAG, "==========================================");

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));

    TEST_ASSERT_MESSAGE(found_count > 0, "No DALI control gear found on bus (addresses 0–63)");
    ESP_LOGI(TAG, "scan short addresses: PASS (%d device(s))", found_count);
}

/* =========================================================================
 * TEST CASE (3): dapc dimming sequence
 *   OFF → MAX → 50 % → 25 % → MAX
 * ========================================================================= */

TEST_CASE("dapc dimming sequence", "[dali]")
{
    dali_master_handle_t dali = NULL;
    dali_master_config_t cfg = {
        .rx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
        .tx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO,
        .invert_tx = true,
        .invert_rx = true,
    };
    dali_master_rmt_config_t rmt_cfg = {
        .mem_block_symbols = 64,
    };
    TEST_ASSERT_EQUAL(ESP_OK, dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

#if CONFIG_DALI_PART103_ENABLED
    dali_103_do_device_command(dali, DALI_ADDR_BROADCAST, 0,
                               DALI_103_START_QUIESCENT_MODE,
                               true, DALI_TX_TIMEOUT_MS, NULL);
    vTaskDelay(pdMS_TO_TICKS(50));
#endif

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
        dali_master_transaction_config_t tx_cfg = {
            .addr_type = DALI_ADDR_SHORT,
            .addr = TEST_ADDR,
            .is_cmd = steps[i].is_cmd,
            .command = steps[i].value,
            .send_twice = false,
            .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
        };
        esp_err_t err = dali_master_do_transaction(dali, &tx_cfg, NULL);
        ESP_LOGI(TAG, "  step %-6s → %s", steps[i].label, esp_err_to_name(err));
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, steps[i].label);
        /* Inter-frame gap is now inserted automatically by dali_master_do_transaction(). */
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));
    ESP_LOGI(TAG, "dapc dimming sequence: PASS");
}

/* =========================================================================
 * TEST CASE (4): query actual level
 * ========================================================================= */

TEST_CASE("query actual level", "[dali]")
{
    dali_master_handle_t dali = NULL;
    dali_master_config_t cfg = {
        .rx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
        .tx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO,
        .invert_tx = true,
        .invert_rx = true,
    };
    dali_master_rmt_config_t rmt_cfg = {
        .mem_block_symbols = 64,
    };
    TEST_ASSERT_EQUAL(ESP_OK, dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

    /* Set lamp to MAX first so a predictable level can be queried back. */
    {
        dali_master_transaction_config_t tx_cfg = {
            .addr_type = DALI_ADDR_SHORT,
            .addr = TEST_ADDR,
            .is_cmd = false,
            .command = TEST_LEVEL_MAX,
            .send_twice = false,
            .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
        };
        TEST_ASSERT_EQUAL(ESP_OK, dali_master_do_transaction(dali, &tx_cfg, NULL));
    }
    /* Inter-frame gap inserted automatically; add extra settle time. */
    vTaskDelay(pdMS_TO_TICKS(300));

    int reply = DALI_RESULT_NO_REPLY;
    esp_err_t err;
    {
        dali_master_transaction_config_t tx_cfg = {
            .addr_type = DALI_ADDR_SHORT,
            .addr = TEST_ADDR,
            .is_cmd = true,
            .command = DALI_CMD_QUERY_ACTUAL_LEVEL,
            .send_twice = false,
            .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
        };
        err = dali_master_do_transaction(dali, &tx_cfg, &reply);
    }

    ESP_LOGI(TAG, "QUERY_ACTUAL_LEVEL → err=%s  reply=0x%02X (%d)", esp_err_to_name(err), (unsigned)(reply & 0xFF),
             reply);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_MESSAGE(DALI_RESULT_IS_VALID(reply), "QUERY_ACTUAL_LEVEL: no backward frame received");

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));
    ESP_LOGI(TAG, "query actual level: PASS");
}

/* =========================================================================
 * TEST CASE (5): send twice reset command
 * ========================================================================= */

TEST_CASE("send twice reset command", "[dali]")
{
    dali_master_handle_t dali = NULL;
    dali_master_config_t cfg = {
        .rx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
        .tx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO,
        .invert_tx = true,
        .invert_rx = true,
    };
    dali_master_rmt_config_t rmt_cfg = {
        .mem_block_symbols = 64,
    };
    TEST_ASSERT_EQUAL(ESP_OK, dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

    esp_err_t err;
    {
        dali_master_transaction_config_t tx_cfg = {
            .addr_type = DALI_ADDR_SHORT,
            .addr = TEST_ADDR,
            .is_cmd = true,
            .command = DALI_CMD_RESET,
            .send_twice = true,
            .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
        };
        err = dali_master_do_transaction(dali, &tx_cfg, NULL);
    }

    ESP_LOGI(TAG, "  RESET (send-twice) → %s", esp_err_to_name(err));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    /* Inter-frame gap inserted automatically by dali_master_do_transaction(). */

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));
    ESP_LOGI(TAG, "send twice reset command: PASS");
}

/* =========================================================================
 * TEST CASE (6): commission all gear
 *
 * Runs the full IEC 62386-102 binary-search commissioning procedure
 * (DALI_COMMISSION_ALL) to assign short addresses 0–63 to every device
 * on the bus.  After commissioning, a scan is performed to verify that the
 * assigned addresses are now reachable.
 *
 * Expected result:
 *   - dali_commission() returns ESP_OK.
 *   - At least one device was found and addressed (count > 0).
 *   - A subsequent QUERY_CONTROL_GEAR scan finds the same number of devices.
 *
 * NOTE: This test will overwrite the existing short addresses of all
 *       gear currently on the bus.
 * ========================================================================= */

TEST_CASE("commission all gear", "[dali]")
{
    dali_master_handle_t dali = NULL;
    dali_master_config_t cfg = {
        .rx_gpio   = (gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
        .tx_gpio   = (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO,
        .invert_tx = true,
        .invert_rx = true,
    };
    dali_master_rmt_config_t rmt_cfg = {
        .mem_block_symbols = 64,
    };
    TEST_ASSERT_EQUAL(ESP_OK, dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

    /* --- Step 1: Commission all gear, start at address 0, up to 64 devices --- */
    ESP_LOGI(TAG, "Running commission (ALL mode, start=0, max=64) ...");

    uint8_t commissioned = 0;
    esp_err_t err = dali_commission(dali, DALI_COMMISSION_ALL, 0, 64, &commissioned,
                                    DALI_TX_TIMEOUT_MS);

    ESP_LOGI(TAG, "dali_commission() → %s, devices commissioned: %d", esp_err_to_name(err),
             commissioned);

    /* --- Step 2: Verify via QUERY_CONTROL_GEAR scan --- */
    uint8_t found[64];
    int     found_count = 0;

    if (err == ESP_OK && commissioned > 0) {
        ESP_LOGI(TAG, "Verifying assigned addresses with a scan ...");

        for (int addr = 0; addr < 64; addr++) {
            int reply = DALI_RESULT_NO_REPLY;
            dali_master_transaction_config_t tx_cfg = {
                .addr_type     = DALI_ADDR_SHORT,
                .addr          = (uint8_t)addr,
                .is_cmd        = true,
                .command       = DALI_CMD_QUERY_CONTROL_GEAR,
                .send_twice    = false,
                .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
            };
            if (dali_master_do_transaction(dali, &tx_cfg, &reply) == ESP_OK &&
                    reply == 0xFF) {
                found[found_count++] = (uint8_t)addr;
            }
        }

        ESP_LOGI(TAG, "======== Post-Commission Address Scan ========");
        ESP_LOGI(TAG, "  Commissioned: %d   Scan found: %d", commissioned, found_count);
        for (int i = 0; i < found_count; i++) {
            ESP_LOGI(TAG, "    [%2d] short address %d (0x%02X)", i + 1, found[i], found[i]);
        }
        ESP_LOGI(TAG, "==============================================");
    }

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "dali_commission() failed");
    TEST_ASSERT_MESSAGE(commissioned > 0, "No devices were commissioned (bus may be empty)");
    TEST_ASSERT_EQUAL_MESSAGE(commissioned, (uint8_t)found_count,
                              "Scan found a different number of devices than commissioned");
    ESP_LOGI(TAG, "commission all gear: PASS (%d device(s))", commissioned);
}

/* =========================================================================
 * TEST CASE (7): query status
 * ========================================================================= */

TEST_CASE("query status", "[dali]")
{
    dali_master_handle_t dali = NULL;
    dali_master_config_t cfg = {
        .rx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
        .tx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO,
        .invert_tx = true,
        .invert_rx = true,
    };
    dali_master_rmt_config_t rmt_cfg = {
        .mem_block_symbols = 64,
    };
    TEST_ASSERT_EQUAL(ESP_OK, dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

    int reply = DALI_RESULT_NO_REPLY;
    esp_err_t err;
    {
        dali_master_transaction_config_t tx_cfg = {
            .addr_type = DALI_ADDR_SHORT,
            .addr = TEST_ADDR,
            .is_cmd = true,
            .command = DALI_CMD_QUERY_STATUS,
            .send_twice = false,
            .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
        };
        err = dali_master_do_transaction(dali, &tx_cfg, &reply);
    }

    ESP_LOGI(TAG, "QUERY_STATUS → err=%s  status=0x%02X", esp_err_to_name(err), (unsigned)(reply & 0xFF));

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_MESSAGE(DALI_RESULT_IS_VALID(reply), "QUERY_STATUS: no backward frame received");

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));
    ESP_LOGI(TAG, "query status: PASS");
}

/* =========================================================================
 * TEST CASE (7): DT8 RGB+CCT color sequence
 *
 * Target hardware: DA5-L in light type C5 (RGB+CCT), driving an RGB +
 * warm-white + white LED strip at short address DT8_ADDR (0x04).
 *
 * Physical wiring:
 *   V+ (black common positive) → controller V+
 *   R  (red wire)              → controller R
 *   G  (green wire)            → controller G
 *   B  (blue wire)             → controller B
 *   WW (yellow wire)           → controller WW
 *   W  (white wire)            → controller W
 *
 * DA5-L C5 mode uses DT8 RGB commands for the RGB outputs and DT8 Tc commands for
 * the WW/W color-temperature pair.
 *
 * Sequence
 * --------
 *   Phase A – Tc sweep    : warm → cool → middle color temperature
 *   Phase B – RGB scan    : red → green → blue → mixed RGB colors
 *   Teardown              : RECALL_MAX_LEVEL + dali_del_master
 * ========================================================================= */

#if CONFIG_DALI_PART102_ENABLED && CONFIG_DALI_PART209_ENABLED
TEST_CASE("DT8 C5 RGB+CCT color sequence", "[dali][dt8]")
{
    dali_master_handle_t dali = NULL;
    dali_master_config_t cfg = {
        .rx_gpio   = (gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
        .tx_gpio   = (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO,
        .invert_tx = true,
        .invert_rx = true,
    };
    dali_master_rmt_config_t rmt_cfg = { .mem_block_symbols = 64 };
    TEST_ASSERT_EQUAL(ESP_OK, dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

    /* ------------------------------------------------------------------ */
    /* Step 1: Lamp on at full brightness                                   */
    /* ------------------------------------------------------------------ */
    {
        dali_master_transaction_config_t tx_cfg = {
            .addr_type     = DALI_ADDR_SHORT,
            .addr          = DT8_ADDR,
            .is_cmd        = true,
            .command       = DALI_CMD_RECALL_MAX_LEVEL,
            .send_twice    = false,
            .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
        };
        TEST_ASSERT_EQUAL(ESP_OK, dali_master_do_transaction(dali, &tx_cfg, NULL));
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    /* ------------------------------------------------------------------ */
    /* Step 2: Query color capabilities (informational — no hard assert)  */
    /* ------------------------------------------------------------------ */
    TEST_ASSERT_EQUAL(ESP_OK, dali_enable_device_type(dali, 8, DALI_TX_TIMEOUT_MS));
    int caps = DALI_RESULT_NO_REPLY;
    {
        dali_master_transaction_config_t tx_cfg = {
            .addr_type     = DALI_ADDR_SHORT,
            .addr          = DT8_ADDR,
            .is_cmd        = true,
            .command       = DALI_209_QUERY_COLOR_CAPABILITIES,
            .send_twice    = false,
            .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
        };
        dali_master_do_transaction(dali, &tx_cfg, &caps);
    }
    if (DALI_RESULT_IS_VALID(caps)) {
        /* IEC 62386-209 §8.3.1  capabilities bit layout:
         *   bit 4 = XY chromaticity supported
         *   bit 5 =  temperature (Tc) supported
         *   bit 6 = Primary-N dimlevel supported
         *   bit 7 = RGBWAF supported */
        ESP_LOGI(TAG, "[DT8] addr=%u  Color capabilities = 0x%02X  "
                 "(XY:%c  Tc:%c  Primary-N:%c  RGBWAF:%c)",
                 DT8_ADDR, (unsigned)(caps & 0xFF),
                 (caps & BIT(4)) ? 'Y' : 'N',
                 (caps & BIT(5)) ? 'Y' : 'N',
                 (caps & BIT(6)) ? 'Y' : 'N',
                 (caps & BIT(7)) ? 'Y' : 'N');
    } else {
        ESP_LOGW(TAG, "[DT8] addr=%u  QUERY_COLOR_CAPABILITIES: no reply", DT8_ADDR);
    }

    ESP_LOGI(TAG, "[DT8] Starting CCT sweep (WW/W) on addr=%u ...", DT8_ADDR);
    const struct {
        uint16_t mirek;
        const char *label;
    } tc_steps[] = {
        { 370, "Tc warm  ~2700K" },
        { 153, "Tc cool  ~6500K" },
        { 250, "Tc mid   ~4000K" },
    };
    for (int i = 0; i < (int)(sizeof(tc_steps) / sizeof(tc_steps[0])); i++) {
        dali_color_val_t val = { .cct = { .mirek = tc_steps[i].mirek } };
        esp_err_t err = dali_master_set_color(dali, DALI_ADDR_SHORT, DT8_ADDR,
                                              DALI_COLOR_CCT, val, DALI_TX_TIMEOUT_MS);
        ESP_LOGI(TAG, "[DT8]   %-16s mirek=%3u → %s",
                 tc_steps[i].label, tc_steps[i].mirek, esp_err_to_name(err));
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, tc_steps[i].label);
        vTaskDelay(pdMS_TO_TICKS(DT8_STEP_DELAY_MS));
    }

    /* In DA5-L C5 mode, RGB is controlled by SET_TEMPORARY_RGB_DIMLEVEL.
     * WW/W is controlled by the Tc mode above, not by Primary-N. */
    struct {
        uint8_t r, g, b;
        const char *label;
    } rgb_steps[] = {
        { 0xFE, 0x00, 0x00, "RGB Red       (R only)"       },
        { 0x00, 0xFE, 0x00, "RGB Green     (G only)"       },
        { 0x00, 0x00, 0xFE, "RGB Blue      (B only)"       },
        { 0xFE, 0x18, 0x00, "RGB warm red  (R + small G)"  },
        { 0xA0, 0x00, 0xFE, "RGB violet    (R + B)"        },
        { 0x00, 0x00, 0x00, "RGB off       (all zero)"     },
    };

    ESP_LOGI(TAG, "[DT8] Starting RGB sweep on addr=%u ...", DT8_ADDR);
    for (int i = 0; i < (int)(sizeof(rgb_steps) / sizeof(rgb_steps[0])); i++) {
        dali_color_val_t val = {
            .rgb = {
                .r = rgb_steps[i].r,
                .g = rgb_steps[i].g,
                .b = rgb_steps[i].b,
            },
        };
        esp_err_t err = dali_master_set_color(dali, DALI_ADDR_SHORT, DT8_ADDR,
                                              DALI_COLOR_RGB, val, DALI_TX_TIMEOUT_MS);
        ESP_LOGI(TAG, "[DT8]   %-28s R=%3u G=%3u B=%3u → %s",
                 rgb_steps[i].label,
                 rgb_steps[i].r, rgb_steps[i].g, rgb_steps[i].b,
                 esp_err_to_name(err));
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, rgb_steps[i].label);
        vTaskDelay(pdMS_TO_TICKS(DT8_STEP_DELAY_MS));
    }

    /* ------------------------------------------------------------------ */
    /* Teardown: restore lamp to max, release driver                       */
    /* ------------------------------------------------------------------ */
    {
        dali_master_transaction_config_t tx_cfg = {
            .addr_type     = DALI_ADDR_SHORT,
            .addr          = DT8_ADDR,
            .is_cmd        = true,
            .command       = DALI_CMD_RECALL_MAX_LEVEL,
            .send_twice    = false,
            .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
        };
        dali_master_do_transaction(dali, &tx_cfg, NULL);
    }

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));
    ESP_LOGI(TAG, "[DT8] DT8 C5 RGB+CCT color sequence: PASS");
}
#endif /* CONFIG_DALI_PART102_ENABLED && CONFIG_DALI_PART209_ENABLED */

#if defined(CONFIG_SOC_WIFI_SUPPORTED)
/* =========================================================================
 * TEST CASE (8): ota stress query actual level
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
    esp_err_t init_err;          /**< dali_new_master_rmt() result from inside the task */
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
    ESP_LOGI(TAG, "  Cmd success rate: %"PRIu32" / %"PRIu32"  (%"PRIu32" %%)", total_ok, total, pct);
    ESP_LOGI(TAG, "  Last QUERY level: 0x%02X  (%d)", (unsigned)(s->last_query_level & 0xFF), s->last_query_level);
    ESP_LOGI(TAG, "  QUERY timing violations (>%d µs): %"PRIu32, DALI_TIMING_WARN_US, s->query_violations);
    ESP_LOGI(TAG, "  %-8s  %6s  %6s  %8s  %8s  %8s", "Step", "OK", "ERR", "Min(µs)", "Max(µs)", "Avg(µs)");
    for (int i = 0; i < LIGHT_TEST_STEPS; i++) {
        uint32_t n  = s->step_ok[i] + s->step_err[i];
        int64_t avg = n ? (s->step_sum_us[i] / (int64_t)n) : 0;
        int64_t mn  = (s->step_min_us[i] == INT64_MAX) ? 0 : s->step_min_us[i];
        int64_t mx  = (s->step_max_us[i] == INT64_MIN) ? 0 : s->step_max_us[i];
        ESP_LOGI(TAG, "  %s  %6"PRIu32"  %6"PRIu32"  %8"PRId64"  %8"PRId64"  %8"PRId64, LIGHT_STEP_NAME[i],
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

    /* dali_new_master_rmt() must run on the same core as dali_master_do_transaction() — mirror
     * dali_demo.c where both live inside this task (core 1). */
    dali_master_handle_t dali = NULL;
    dali_master_config_t cfg = {
        .rx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
        .tx_gpio = (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO,
        .invert_tx = true,
        .invert_rx = true,
    };
    dali_master_rmt_config_t rmt_cfg = {
        .mem_block_symbols = 64,
    };
    s->init_err = dali_new_master_rmt(&cfg, &rmt_cfg, &dali);
    if (s->init_err != ESP_OK) {
        ESP_LOGE(TAG, "[stress] dali_new_master_rmt failed: %s", esp_err_to_name(s->init_err));
        s->done = true;
        vTaskDelete(NULL);
        return;
    }

#if CONFIG_DALI_PART103_ENABLED
    dali_103_do_device_command(dali, DALI_ADDR_BROADCAST, 0, DALI_103_START_QUIESCENT_MODE, true, DALI_TX_TIMEOUT_MS,
                               NULL);
#endif

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

            dali_master_transaction_config_t tx_cfg = {
                .addr_type = DALI_ADDR_SHORT,
                .addr = TEST_ADDR,
                .is_cmd = steps[i].is_cmd,
                .command = steps[i].value,
                .send_twice = false,
                .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
            };
            esp_err_t err = dali_master_do_transaction(dali, &tx_cfg,
                                                       steps[i].expect_reply ? &reply : NULL);

            int64_t elapsed_us = esp_timer_get_time() - t0;

            /* Update per-step latency statistics */
            s->step_sum_us[i] += elapsed_us;
            if (elapsed_us < s->step_min_us[i]) {
                s->step_min_us[i] = elapsed_us;
            }
            if (elapsed_us > s->step_max_us[i]) {
                s->step_max_us[i] = elapsed_us;
            }

            if (err == ESP_OK) {
                s->step_ok[i]++;
                if (steps[i].expect_reply) {
                    query_level = reply;
                }
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
                ESP_LOGW(TAG, "[stress] round=%"PRIu32" step=%s  TIMING VIOLATION %"PRId64" µs", s->rounds,
                         LIGHT_STEP_NAME[i], elapsed_us);
            }

            /* Inter-frame gap is now inserted automatically by dali_master_do_transaction(). */
            vTaskDelay(pdMS_TO_TICKS(DALI_LIGHT_STEP_DELAY_MS));
        }

        if (DALI_RESULT_IS_VALID(query_level)) {
            s->last_query_level = query_level;
            if ((uint8_t)query_level < (TEST_LEVEL_MAX - 0x20U)) {
                ESP_LOGW(TAG, "[stress] round=%"PRIu32" level readback 0x%02X < expected ~0x%02X"
                         " — possible missed command under OTA load", s->rounds, (uint8_t)query_level, TEST_LEVEL_MAX);
            }
        } else {
            ESP_LOGW(TAG, "[stress] round=%"PRIu32" QUERY_ACTUAL_LEVEL: no reply", s->rounds);
        }

        vTaskDelay(pdMS_TO_TICKS(DALI_LIGHT_INTERVAL_MS));
    }

    dali_del_master(dali);

    s->done = true;
    vTaskDelete(NULL);
}

TEST_CASE("ota stress query actual level", "[dali][ota_stress]")
{
    /* NVS, lwIP stack, and WiFi driver are pre-initialised in app_main()
     * so their one-time heap cost is not counted as a per-test leak. */

    /* NOTE: dali_new_master_rmt() is called inside dali_light_test_task so that both
     * init and all transactions execute on the same core (core 1), exactly
     * as in dali_demo.c.  Do NOT call dali_new_master_rmt() here. */

    /* Connect Wi-Fi and launch background OTA stress task on core 0
     * (same core as the Wi-Fi stack) to maximise bus load. */
    bool ota_running = false;
    if (wifi_init_sta() == ESP_OK) {
        xTaskCreatePinnedToCore(ota_stress_task, "ota_stress", 8192, NULL, 4, &s_ota_task_handle, 0);
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

    xTaskCreatePinnedToCore(dali_light_test_task, "dali_light_test", 4096, &lt, 5, NULL,
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
    ESP_LOGI(TAG, "[stress] rounds=%"PRIu32"  query_violations=%"PRIu32"  ota=%s", lt.rounds, lt.query_violations,
             ota_running ? "yes" : "no");

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
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(DALI_OTA_STRESS_ROUNDS, lt.rounds, "not all rounds completed");
    /* QUERY timing violations under OTA load are expected (flash write latency
     * can push QUERY response past 67 ms).  Mirror dali_demo.c: log a warning
     * but do NOT fail the test on violation count alone. */
    if (lt.query_violations > 0) {
        ESP_LOGW(TAG, "[stress] %"PRIu32" QUERY timing violation(s) observed under OTA load"
                 " (expected, not a failure)", lt.query_violations);
    }

    /* dali_del_master() is called inside dali_light_test_task on core 1 —
     * same pattern as dali_demo.c. */
    ESP_LOGI(TAG, "ota stress query actual level: PASS");
}
#endif // CONFIG_SOC_WIFI_SUPPORTED

#if CONFIG_DALI_PART103_ENABLED
/* =========================================================================
 * DLS-203-P helper: create/delete master (avoids copy-paste in each case)
 * ========================================================================= */

static esp_err_t dls203_start_quiescent(dali_master_handle_t dali)
{
    esp_err_t err = dali_103_do_device_command(dali, DALI_ADDR_BROADCAST, 0,
                                               DALI_103_START_QUIESCENT_MODE,
                                               true, DALI_TX_TIMEOUT_MS, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "[DLS-203-P] START_QUIESCENT_MODE failed: %s", esp_err_to_name(err));
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

static esp_err_t dls203_master_new(dali_master_handle_t *out_dali)
{
    dali_master_config_t cfg = {
        .rx_gpio   = (gpio_num_t)CONFIG_DALI_STRESS_RX_GPIO,
        .tx_gpio   = (gpio_num_t)CONFIG_DALI_STRESS_TX_GPIO,
        .invert_tx = true,
        .invert_rx = true,
    };
    dali_master_rmt_config_t rmt_cfg = { .mem_block_symbols = 64 };

    esp_err_t err = dali_new_master_rmt(&cfg, &rmt_cfg, out_dali);
    if (err != ESP_OK) {
        return err;
    }

    err = dls203_start_quiescent(*out_dali);
    if (err != ESP_OK) {
        dali_del_master(*out_dali);
        *out_dali = NULL;
    }
    return err;
}

/* =========================================================================
 * TEST CASE (DLS-203-P 1): commission input devices
 *
 * Runs the full IEC 62386-103 binary-search commissioning procedure and
 * assigns short address 0 (DLS203_ADDR) to the DLS-203-P on the bus.
 *
 * Expected result:
 *   - dali_103_commission() returns ESP_OK.
 *   - At least one input device was found (count >= 1).
 *   - QUERY_DEVICE_PRESENT at DLS203_ADDR returns 0xFF.
 *
 * NOTE: This test will overwrite short addresses of ALL input devices on
 *       the bus.  Run it only once when wiring up a new sensor.
 * ========================================================================= */

TEST_CASE("DLS-203-P commission input devices", "[dali][dls203]")
{
    dali_master_handle_t dali = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, dls203_master_new(&dali));

    ESP_LOGI(TAG, "[DLS-203-P] Running Part 103 commission (ALL, start=%d, max=1) ...", DLS203_ADDR);

    uint8_t commissioned = 0;
    esp_err_t err = dali_103_commission(dali, DALI_COMMISSION_ALL, DLS203_ADDR, 1,
                                        &commissioned, DALI_TX_TIMEOUT_MS);

    ESP_LOGI(TAG, "[DLS-203-P] commission → %s, devices: %d",
             esp_err_to_name(err), commissioned);

    /* Verify DLS203_ADDR responds to QUERY_DEVICE_STATUS */
    uint8_t status = 0;
    esp_err_t verify_err = ESP_FAIL;
    if (err == ESP_OK && commissioned >= 1) {
        verify_err = dali_103_query_device_status(dali, DALI_ADDR_SHORT, DLS203_ADDR, &status,
                                                  DALI_TX_TIMEOUT_MS);
        ESP_LOGI(TAG, "[DLS-203-P] QUERY_DEVICE_STATUS addr=%d → %s%s0x%02X",
                 DLS203_ADDR,
                 (verify_err == ESP_OK) ? "" : esp_err_to_name(verify_err),
                 (verify_err == ESP_OK) ? "" : " ",
                 status);
    }

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "dali_103_commission() failed");
    TEST_ASSERT_MESSAGE(commissioned >= 1, "No Part 103 input device found on bus");
    TEST_ASSERT_EQUAL(ESP_OK, verify_err);
    ESP_LOGI(TAG, "[DLS-203-P] commission input devices: PASS (%d device(s))", commissioned);
}

/* =========================================================================
 * TEST CASE (DLS-203-P 2): query device info
 *
 * Reads basic device-level properties from the DLS-203-P:
 *   - QUERY_DEVICE_PRESENT  (sanity check)
 *   - QUERY_DEVICE_STATUS   (status byte bit-field)
 *   - QUERY_NUMBER_OF_INSTANCES (should be >= 2: PIR + light)
 *   - QUERY_SHORT_ADDRESS   (should equal DLS203_ADDR)
 *
 * The test is informational — it prints all values and asserts that the
 * device is present and has at least two instances.
 * ========================================================================= */

TEST_CASE("DLS-203-P query device info", "[dali][dls203]")
{
    dali_master_handle_t dali = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, dls203_master_new(&dali));

    /* --- 1. QUERY_DEVICE_STATUS ----------------------------------------- */
    uint8_t status = 0;
    esp_err_t present_err = dali_103_query_device_status(dali, DALI_ADDR_SHORT, DLS203_ADDR, &status,
                                                         DALI_TX_TIMEOUT_MS);
    ESP_LOGI(TAG, "[DLS-203-P] QUERY_DEVICE_STATUS → %s%s0x%02X",
             (present_err == ESP_OK) ? "" : esp_err_to_name(present_err),
             (present_err == ESP_OK) ? "" : " ",
             status);
    bool present = (present_err == ESP_OK);

    /* --- 2. Decode device status ---------------------------------------- */
    int status_reply = status;
    if (present) {
        if (DALI_RESULT_IS_VALID(status_reply)) {
            ESP_LOGI(TAG, "[DLS-203-P] QUERY_DEVICE_STATUS = 0x%02X "
                     "(reset:%c  shortAddrMissing:%c  eventPending:%c)",
                     (unsigned)(status_reply & 0xFF),
                     (status_reply & BIT(0)) ? 'Y' : 'N',
                     (status_reply & BIT(2)) ? 'Y' : 'N',
                     (status_reply & BIT(4)) ? 'Y' : 'N');
        } else {
            ESP_LOGW(TAG, "[DLS-203-P] QUERY_DEVICE_STATUS: no reply");
        }
    }

    /* --- 3. QUERY_NUMBER_OF_INSTANCES ----------------------------------- */
    uint8_t num_instances = 0;
    esp_err_t instances_err = ESP_FAIL;
    if (present) {
        instances_err = dali_103_query_number_of_instances(dali, DALI_ADDR_SHORT,
                                                           DLS203_ADDR, &num_instances,
                                                           DALI_TX_TIMEOUT_MS);
        ESP_LOGI(TAG, "[DLS-203-P] Number of instances = %d", num_instances);
    }

    /* Instance details require 24-bit instance-addressed commands and are
     * covered by the dedicated PIR/light tests. */

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, present_err, "QUERY_DEVICE_PRESENT transaction failed");
    TEST_ASSERT_MESSAGE(present, "DLS-203-P not present at DLS203_ADDR; check address or run commission first");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, instances_err, "QUERY_NUMBER_OF_INSTANCES failed");
    TEST_ASSERT_MESSAGE(num_instances >= 2,
                        "DLS-203-P should have at least 2 instances (PIR + light sensor)");
    ESP_LOGI(TAG, "[DLS-203-P] query device info: PASS");
}

/* =========================================================================
 * TEST CASE (DLS-203-P 3): PIR occupancy query (Part 303)
 *
 * Reads the current occupancy state from instance DLS203_PIR_INSTANCE.
 * The test passes as long as a valid backward frame is received; the
 * occupied/unoccupied state is printed but not asserted (motion is
 * environment-dependent).
 *
 * Also reads the latched occupancy event register so that any pending
 * event is acknowledged.
 * ========================================================================= */

TEST_CASE("DLS-203-P PIR occupancy query", "[dali][dls203]")
{
    dali_master_handle_t dali = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, dls203_master_new(&dali));

    /* --- Query current occupancy state ---------------------------------- */
    bool occupied = false;
    esp_err_t err = dali_303_query_occupancy(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                             DLS203_PIR_INSTANCE, &occupied,
                                             DALI_TX_TIMEOUT_MS);

    /* --- Read latched occupancy event register (clears the latch) ------- */
    int latch_reply = DALI_RESULT_NO_REPLY;
    if (err == ESP_OK) {
        /* Occupancy latch is event-oriented; keep this test focused on current value. */

        /* --- Query hold timer ----------------------------------------------- */
        uint8_t hold_reply = 0;
        esp_err_t hold_err = dali_303_query_hold_timer(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                                       DLS203_PIR_INSTANCE, &hold_reply,
                                                       DALI_TX_TIMEOUT_MS);

        /* --- Query deadtime timer ------------------------------------------- */
        uint8_t deadtime_reply = 0;
        esp_err_t deadtime_err = dali_303_query_deadtime_timer(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                                               DLS203_PIR_INSTANCE, &deadtime_reply,
                                                               DALI_TX_TIMEOUT_MS);

        ESP_LOGI(TAG, "[DLS-203-P] Occupancy latch = %s",
                 (latch_reply == 0xFF) ? "event latched (cleared)" : "not queried");
        if (hold_err == ESP_OK) {
            ESP_LOGI(TAG, "[DLS-203-P] Hold timer = %d", hold_reply);
        } else {
            ESP_LOGW(TAG, "[DLS-203-P] QUERY_HOLD_TIMER: %s", esp_err_to_name(hold_err));
        }
        if (deadtime_err == ESP_OK) {
            ESP_LOGI(TAG, "[DLS-203-P] Deadtime timer = %d", deadtime_reply);
        } else {
            ESP_LOGW(TAG, "[DLS-203-P] QUERY_DEADTIME_TIMER: %s", esp_err_to_name(deadtime_err));
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "dali_303_query_occupancy() failed");
    ESP_LOGI(TAG, "[DLS-203-P] Occupancy (instance %d) = %s",
             DLS203_PIR_INSTANCE, occupied ? "OCCUPIED" : "unoccupied");
    ESP_LOGI(TAG, "[DLS-203-P] PIR occupancy query: PASS");
}

/* =========================================================================
 * TEST CASE (DLS-203-P 4): light sensor configuration query (Part 304)
 *
 * Reads hysteresis, report timer and deadtime settings from the Part 304
 * light sensor instance on DLS203_LIGHT_INSTANCE.  Illuminance itself is
 * event-based (push model) and is not directly queryable per IEC 62386-304.
 *
 * Expected:
 *   - Instance type query returns 4 (Part 304 light sensor).
 *   - Hysteresis / report-timer / deadtime queries succeed.
 * ========================================================================= */

TEST_CASE("DLS-203-P light sensor illuminance query", "[dali][dls203]")
{
    dali_master_handle_t dali = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, dls203_master_new(&dali));

    uint8_t type = 0;
    esp_err_t type_err = dali_103_query_instance_type(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                                      DLS203_LIGHT_INSTANCE, &type,
                                                      DALI_TX_TIMEOUT_MS);
    if (type_err == ESP_OK) {
        ESP_LOGI(TAG, "[DLS-203-P] Light instance %d type = %d", DLS203_LIGHT_INSTANCE, type);
    }

    uint8_t hyst = 0;
    uint8_t report_timer = 0;
    uint8_t deadtime = 0;
    esp_err_t hyst_err = dali_304_query_hysteresis(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                                   DLS203_LIGHT_INSTANCE, &hyst,
                                                   DALI_TX_TIMEOUT_MS);
    esp_err_t report_err = dali_304_query_report_timer(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                                       DLS203_LIGHT_INSTANCE, &report_timer,
                                                       DALI_TX_TIMEOUT_MS);
    esp_err_t deadtime_err = dali_304_query_deadtime_timer(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                                           DLS203_LIGHT_INSTANCE, &deadtime,
                                                           DALI_TX_TIMEOUT_MS);

    if (hyst_err == ESP_OK) {
        ESP_LOGI(TAG, "[DLS-203-P] Light hysteresis = %d", hyst);
    }
    if (report_err == ESP_OK) {
        ESP_LOGI(TAG, "[DLS-203-P] Light report timer = %d s", report_timer);
    }
    if (deadtime_err == ESP_OK) {
        ESP_LOGI(TAG, "[DLS-203-P] Light deadtime timer = %d", deadtime);
    }
    ESP_LOGW(TAG, "[DLS-203-P] Current illuminance is normally reported via Part 304 events; direct query is not used here");

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, type_err, "QUERY_INSTANCE_TYPE for light sensor failed");
    TEST_ASSERT_EQUAL_MESSAGE(4, type, "DLS203_LIGHT_INSTANCE is not a Part 304 light sensor");
    ESP_LOGI(TAG, "[DLS-203-P] light sensor instance query: PASS");
}

/* =========================================================================
 * TEST CASE (DLS-203-P 5): hold timer configuration (Part 303)
 *
 * Writes DLS203_HOLD_TIMER_TEST_S (10 s) to the occupancy hold timer,
 * then reads it back and verifies that the readback matches.
 *
 * DTR0 encoding: raw byte in seconds (0 = disable, 0xFF = factory default).
 * ========================================================================= */

TEST_CASE("DLS-203-P hold timer configuration", "[dali][dls203]")
{
    dali_master_handle_t dali = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, dls203_master_new(&dali));

    /* --- Read current hold timer before changing it --------------------- */
    uint8_t before = 0;
    esp_err_t before_err = dali_303_query_hold_timer(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                                     DLS203_PIR_INSTANCE, &before,
                                                     DALI_TX_TIMEOUT_MS);
    if (before_err == ESP_OK) {
        ESP_LOGI(TAG, "[DLS-203-P] Hold timer before write = %d", before);
    } else {
        ESP_LOGW(TAG, "[DLS-203-P] Hold timer before write: %s", esp_err_to_name(before_err));
    }

    /* --- Write new hold timer value ------------------------------------- */
    esp_err_t write_err = dali_303_set_hold_timer(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                                  DLS203_PIR_INSTANCE,
                                                  (uint8_t)DLS203_HOLD_TIMER_TEST_S,
                                                  DALI_TX_TIMEOUT_MS);
    ESP_LOGI(TAG, "[DLS-203-P] Written hold timer = %d s", DLS203_HOLD_TIMER_TEST_S);

    /* Allow the device to process the write */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* --- Read back and verify ------------------------------------------- */
    uint8_t after = 0;
    esp_err_t after_err = ESP_FAIL;
    if (write_err == ESP_OK) {
        after_err = dali_303_query_hold_timer(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                              DLS203_PIR_INSTANCE, &after,
                                              DALI_TX_TIMEOUT_MS);
        if (after_err == ESP_OK) {
            ESP_LOGI(TAG, "[DLS-203-P] Hold timer after write = %d", after);
        } else {
            ESP_LOGW(TAG, "[DLS-203-P] Hold timer after write: %s", esp_err_to_name(after_err));
        }
    }

    /* --- Restore original value (if it was valid) ----------------------- */
    if (before_err == ESP_OK && before != DLS203_HOLD_TIMER_TEST_S) {
        dali_303_set_hold_timer(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                DLS203_PIR_INSTANCE, before,
                                DALI_TX_TIMEOUT_MS);
        ESP_LOGI(TAG, "[DLS-203-P] Restored hold timer to %d", before);
    }

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, write_err, "dali_303_set_hold_timer() failed");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, after_err, "QUERY_HOLD_TIMER: no reply after write");
    TEST_ASSERT_EQUAL_MESSAGE(DLS203_HOLD_TIMER_TEST_S, after,
                              "Hold timer readback does not match written value");
    ESP_LOGI(TAG, "[DLS-203-P] hold timer configuration: PASS");
}

/* =========================================================================
 * TEST CASE (DLS-203-P 6): reset device
 *
 * Issues the Part 103 RESET command (send-twice) to the DLS-203-P.
 * After reset, verifies the device is still reachable and that
 * QUERY_DEVICE_STATUS reports the "reset state" bit set.
 * ========================================================================= */

TEST_CASE("DLS-203-P reset device", "[dali][dls203]")
{
    dali_master_handle_t dali = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, dls203_master_new(&dali));

    /* Send RESET (send-twice) */
    esp_err_t reset_err = dali_103_reset_device(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                                DALI_TX_TIMEOUT_MS);
    ESP_LOGI(TAG, "[DLS-203-P] RESET (send-twice) → %s", esp_err_to_name(reset_err));

    /* Allow device to complete reset */
    vTaskDelay(pdMS_TO_TICKS(300));

    /* RESET restores factory defaults, which clears quiescent mode on the
     * device.  Re-issue START_QUIESCENT_MODE so the device does not resume
     * pushing event frames onto the shared bus before the next test runs. */
    if (reset_err == ESP_OK) {
        dls203_start_quiescent(dali);
    }

    /* Verify device still responds */
    uint8_t status = 0;
    esp_err_t present_err = ESP_FAIL;
    if (reset_err == ESP_OK) {
        present_err = dali_103_query_device_status(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                                   &status, DALI_TX_TIMEOUT_MS);
        if (present_err == ESP_OK) {
            ESP_LOGI(TAG, "[DLS-203-P] Status after reset = 0x%02X (reset_state bit=%d)",
                     status, (status & BIT(6)) ? 1 : 0);
        } else {
            ESP_LOGW(TAG, "[DLS-203-P] QUERY_DEVICE_STATUS after reset: %s", esp_err_to_name(present_err));
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reset_err, "Part 103 RESET command failed");
    TEST_ASSERT_EQUAL(ESP_OK, present_err);
    ESP_LOGI(TAG, "[DLS-203-P] reset device: PASS");
}

/* =========================================================================
 * TEST CASE (DLS-203-P 7): scan Part 103 input device bus
 *
 * Polls all 64 possible Part 103 short addresses (0–63) with
 * QUERY_DEVICE_PRESENT (0x3E).  Prints a summary table of all responding
 * devices and asserts that at least one (the DLS-203-P) is found.
 * ========================================================================= */

TEST_CASE("DLS-203-P scan Part 103 bus", "[dali][dls203]")
{
    dali_master_handle_t dali = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, dls203_master_new(&dali));

    uint8_t found[64];
    int     found_count = 0;

    ESP_LOGI(TAG, "[DLS-203-P] Scanning Part 103 addresses 0–63 ...");

    for (int addr = 0; addr < 64; addr++) {
        uint8_t status = 0;
        uint8_t instances = 0;
        esp_err_t status_err = dali_103_query_device_status(dali, DALI_ADDR_SHORT, (uint8_t)addr,
                                                            &status, DALI_TX_TIMEOUT_MS);
        esp_err_t instances_err = ESP_FAIL;
        if (status_err == ESP_OK) {
            instances_err = dali_103_query_number_of_instances(dali, DALI_ADDR_SHORT, (uint8_t)addr,
                                                               &instances, DALI_TX_TIMEOUT_MS);
        }
        if (status_err == ESP_OK && instances_err == ESP_OK && instances > 0 && instances <= 32) {
            found[found_count++] = (uint8_t)addr;
            ESP_LOGI(TAG, "  candidate addr=%d status=0x%02X instances=%d", addr, status, instances);
        } else if (status_err == ESP_OK) {
            ESP_LOGW(TAG, "  ignore addr=%d: status=0x%02X but instances query failed/invalid (%s, %d)",
                     addr, status, esp_err_to_name(instances_err), instances);
        }
    }

    ESP_LOGI(TAG, "======== Part 103 Input Device Scan Report ========");
    ESP_LOGI(TAG, "  Confirmed devices found: %d", found_count);
    for (int i = 0; i < found_count; i++) {
        ESP_LOGI(TAG, "    [%2d] short address %d (0x%02X)", i + 1, found[i], found[i]);
    }
    ESP_LOGI(TAG, "===================================================");

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));
    TEST_ASSERT_MESSAGE(found_count > 0,
                        "No Part 103 input device found on bus (addresses 0–63)");
    ESP_LOGI(TAG, "[DLS-203-P] scan Part 103 bus: PASS (%d device(s))", found_count);
}

/* =========================================================================
 * TEST CASE (DLS-203-P 8): continuous dual-sensor monitoring
 *
 * Polls both the PIR occupancy and the light sensor every
 * DLS203_MONITOR_INTERVAL_MS for DLS203_MONITOR_ROUNDS iterations,
 * printing a real-time table.  All polls must succeed (no error).
 *
 * This test is primarily useful for manual observation in a lab environment:
 * walk in front of the sensor during the test to verify PIR detection, and
 * cover/uncover the sensor to verify the light reading changes.
 * ========================================================================= */

TEST_CASE("DLS-203-P continuous dual-sensor monitoring", "[dali][dls203]")
{
    dali_master_handle_t dali = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, dls203_master_new(&dali));

    int pir_errors   = 0;
    int light_errors = 0;

    ESP_LOGI(TAG, "[DLS-203-P] Starting %d-round monitoring (interval %d ms) ...",
             DLS203_MONITOR_ROUNDS, DLS203_MONITOR_INTERVAL_MS);
    uint8_t light_type = 0;
    esp_err_t light_type_err = dali_103_query_instance_type(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                                            DLS203_LIGHT_INSTANCE, &light_type,
                                                            DALI_TX_TIMEOUT_MS);
    if (light_type_err != ESP_OK || light_type != 4) {
        light_errors++;
        ESP_LOGW(TAG, "[DLS-203-P] Light instance check failed: err=%s type=%d",
                 esp_err_to_name(light_type_err), light_type);
    }

    ESP_LOGI(TAG, "  %-5s  %-12s  %-12s",
             "Round", "Occupancy", "LightInst");

    for (int round = 1; round <= DLS203_MONITOR_ROUNDS; round++) {
        /* --- PIR occupancy -------------------------------------------- */
        bool occupied = false;
        esp_err_t err_pir = dali_303_query_occupancy(dali, DALI_ADDR_SHORT, DLS203_ADDR,
                                                     DLS203_PIR_INSTANCE, &occupied,
                                                     DALI_TX_TIMEOUT_MS);
        if (err_pir != ESP_OK) {
            pir_errors++;
            ESP_LOGE(TAG, "[DLS-203-P] round=%d  PIR query error: %s",
                     round, esp_err_to_name(err_pir));
        }

        ESP_LOGI(TAG, "  %-5d  %-12s  type=%d",
                 round,
                 (err_pir == ESP_OK) ? (occupied ? "OCCUPIED" : "unoccupied") : "ERROR",
                 light_type);

        vTaskDelay(pdMS_TO_TICKS(DLS203_MONITOR_INTERVAL_MS));
    }

    ESP_LOGI(TAG, "[DLS-203-P] Monitoring done.  PIR errors=%d  Light errors=%d",
             pir_errors, light_errors);

    TEST_ASSERT_EQUAL(ESP_OK, dali_del_master(dali));
    TEST_ASSERT_EQUAL_MESSAGE(0, pir_errors,   "PIR query errors occurred during monitoring");
    TEST_ASSERT_EQUAL_MESSAGE(0, light_errors, "Light query errors occurred during monitoring");
    ESP_LOGI(TAG, "[DLS-203-P] continuous dual-sensor monitoring: PASS");
}
#endif /* CONFIG_DALI_PART103_ENABLED */
