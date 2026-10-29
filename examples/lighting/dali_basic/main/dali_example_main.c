/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "dali.h"
#include "dali_command.h"
#if CONFIG_DALI_PART102_ENABLED
#include "dali_control_gear.h"
#endif
#if CONFIG_DALI_PART103_ENABLED
#include "dali_control_device.h"
#endif
#if CONFIG_DALI_PART303_304_ENABLED
#include "dali_device_sensors.h"
#endif
#if CONFIG_DALI_PART209_ENABLED
#include "dali_color_control_dt8.h"
#endif

static const char *TAG = "dali_example";

#define DALI_RX_GPIO    5
#define DALI_TX_GPIO    6

/* -------------------------------------------------------------------------
 * STEP 103 sensor address — fixed by commission start address
 * -------------------------------------------------------------------------*/
#define SENSOR_ADDR     10   /* STEP 103 sensor short address (0–63) */
#define SENSOR_INSTANCE 0    /* Occupancy instance number on the sensor */

#define STEP_DELAY_MS   800
#define BLINK_TIMES     6

/* -------------------------------------------------------------------------
 * Runtime-discovered STEP 102 addresses (filled by step2_scan_and_identify)
 * 0xFF means "not found on bus"
 * -------------------------------------------------------------------------*/
#define ADDR_NOT_FOUND  0xFF
#define MAX_102_ADDRS   64

#if CONFIG_DALI_PART102_ENABLED
static uint8_t  g_lamp_addrs[MAX_102_ADDRS];  /* DT6 (non-DT8) lamps */
static uint8_t  g_lamp_count = 0;
#if CONFIG_DALI_PART209_ENABLED
static uint8_t  g_dt8_addr   = ADDR_NOT_FOUND; /* first DT8 found */
#endif
#endif /* CONFIG_DALI_PART102_ENABLED */

/* =========================================================================
 * Helpers
 * ========================================================================= */

#if CONFIG_DALI_PART102_ENABLED
static void dapc(dali_master_handle_t dali, uint8_t addr, uint8_t level)
{
    dali_master_transaction_config_t tx = {
        .addr_type     = DALI_ADDR_SHORT,
        .addr          = addr,
        .is_cmd        = false,
        .command       = level,
        .send_twice    = false,
        .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
    };
    dali_master_do_transaction(dali, &tx, NULL);
}

static void cmd(dali_master_handle_t dali, uint8_t addr, uint8_t command, bool send_twice)
{
    dali_master_transaction_config_t tx = {
        .addr_type     = DALI_ADDR_SHORT,
        .addr          = addr,
        .is_cmd        = true,
        .command       = command,
        .send_twice    = send_twice,
        .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
    };
    dali_master_do_transaction(dali, &tx, NULL);
}

static int query(dali_master_handle_t dali, uint8_t addr, const char *label, uint8_t command)
{
    int result = DALI_RESULT_NO_REPLY;
    dali_master_transaction_config_t tx = {
        .addr_type     = DALI_ADDR_SHORT,
        .addr          = addr,
        .is_cmd        = true,
        .command       = command,
        .send_twice    = false,
        .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
    };
    dali_master_do_transaction(dali, &tx, &result);
    if (DALI_RESULT_IS_VALID(result)) {
        ESP_LOGI(TAG, "  [QUERY] addr=%d  %-22s= 0x%02X (%d)", addr, label, (unsigned)result, result);
    } else {
        ESP_LOGW(TAG, "  [QUERY] addr=%d  %-22s= NO REPLY", addr, label);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return result;
}
#endif /* CONFIG_DALI_PART102_ENABLED */

/* =========================================================================
 * STEP 1: Commissioning
 * ========================================================================= */

static void step1_commission(dali_master_handle_t dali)
{
    ESP_LOGI(TAG, "--- STEP 1: Commissioning ---");

#if CONFIG_DALI_PART102_ENABLED
    uint8_t count102 = 0;
    esp_err_t err = dali_commission(dali, DALI_COMMISSION_ALL, 0, 6, &count102, DALI_TX_TIMEOUT_MS);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "  Part 102 commission done: %d lamp(s) addressed", count102);
    } else {
        ESP_LOGE(TAG, "  Part 102 commission failed: %s", esp_err_to_name(err));
    }
#endif

#if CONFIG_DALI_PART103_ENABLED
    uint8_t count103 = 0;
    esp_err_t err103 = dali_103_commission(dali, DALI_COMMISSION_ALL, 10, 1, &count103, DALI_TX_TIMEOUT_MS);
    if (err103 == ESP_OK) {
        ESP_LOGI(TAG, "  Part 103 commission done: %d input device(s) addressed", count103);
    } else {
        ESP_LOGE(TAG, "  Part 103 commission failed: %s", esp_err_to_name(err103));
    }
#endif

    /* After commission, Part 102/103 leave the bus in quiescent mode.
     * STEP 6 will call STOP_QUIESCENT_MODE when it needs sensor events. */
}

/* =========================================================================
 * STEP 2: Scan & identify addresses
 *   - Fills g_lamp_addrs / g_lamp_count  (DT6 / non-DT8 lamps)
 *   - Fills g_dt8_addr                   (first DT8 found, type == 8)
 * ========================================================================= */

static void step2_scan_and_identify(dali_master_handle_t dali)
{
    ESP_LOGI(TAG, "--- STEP 2: Scan & identify addresses ---");

#if CONFIG_DALI_PART102_ENABLED
    g_lamp_count = 0;
#if CONFIG_DALI_PART209_ENABLED
    g_dt8_addr   = ADDR_NOT_FOUND;
#endif

    ESP_LOGI(TAG, "  [Part 102] scanning short addresses 0-63 ...");
    for (uint8_t addr = 0; addr < 64; addr++) {
        /* Step 1: probe with QUERY_STATUS */
        int status = DALI_RESULT_NO_REPLY;
        dali_master_transaction_config_t tx = {
            .addr_type     = DALI_ADDR_SHORT,
            .addr          = addr,
            .is_cmd        = true,
            .command       = DALI_CMD_QUERY_STATUS,
            .send_twice    = false,
            .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
        };
        dali_master_do_transaction(dali, &tx, &status);
        if (!DALI_RESULT_IS_VALID(status)) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

#if CONFIG_DALI_PART209_ENABLED
        /* Step 2: detect DT8 via ENABLE_DEVICE_TYPE 8 + QUERY_COLOR_CAPABILITIES.
         *
         * IEC 62386-209 §8.3.2: some DT8 gear (e.g. DA5-L) does NOT return a
         * meaningful value for QUERY_DEVICE_TYPE (Part-102 cmd 0x99) unless
         * ENABLE_DEVICE_TYPE 8 was issued first.  Probing via
         * QUERY_COLOR_CAPABILITIES after EDT8 is the reliable method. */
        bool is_dt8 = false;
        {
            dali_enable_device_type(dali, 8, DALI_TX_TIMEOUT_MS);

            int caps = DALI_RESULT_NO_REPLY;
            dali_master_transaction_config_t caps_tx = {
                .addr_type     = DALI_ADDR_SHORT,
                .addr          = addr,
                .is_cmd        = true,
                .command       = DALI_209_QUERY_COLOR_CAPABILITIES,
                .send_twice    = false,
                .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
            };
            dali_master_do_transaction(dali, &caps_tx, &caps);
            if (DALI_RESULT_IS_VALID(caps)) {
                is_dt8 = true;
                ESP_LOGI(TAG, "    addr %2d -> DT8 RGB lamp  (color_caps=0x%02X"
                         " XY:%c Tc:%c PrimaryN:%c RGBWAF:%c)",
                         addr, (uint8_t)caps,
                         (caps & (1 << 4)) ? 'Y' : 'N',
                         (caps & (1 << 5)) ? 'Y' : 'N',
                         (caps & (1 << 6)) ? 'Y' : 'N',
                         (caps & (1 << 7)) ? 'Y' : 'N');
                if (g_dt8_addr == ADDR_NOT_FOUND) {
                    g_dt8_addr = addr;
                }
            }
        }
#else
        bool is_dt8 = false;
#endif /* CONFIG_DALI_PART209_ENABLED */

        if (!is_dt8) {
            int dev_type = DALI_RESULT_NO_REPLY;
            tx.command = DALI_CMD_QUERY_DEVICE_TYPE;
            dali_master_do_transaction(dali, &tx, &dev_type);
            ESP_LOGI(TAG, "    addr %2d -> lamp (type=%d, status=0x%02X)",
                     addr, DALI_RESULT_IS_VALID(dev_type) ? dev_type : -1, (unsigned)status);
            if (g_lamp_count < MAX_102_ADDRS) {
                g_lamp_addrs[g_lamp_count++] = addr;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    ESP_LOGI(TAG, "  Identified: %d DT6 lamp(s)%s",
             g_lamp_count
#if CONFIG_DALI_PART209_ENABLED
             , g_dt8_addr == ADDR_NOT_FOUND ? ", no DT8" : ""
#else
             , ""
#endif
            );
#endif /* CONFIG_DALI_PART102_ENABLED */

#if CONFIG_DALI_PART103_ENABLED
    ESP_LOGI(TAG, "  [Part 103] scanning short addresses 0-63 ...");
    for (uint8_t addr = 0; addr < 64; addr++) {
        uint8_t dev_status = 0;
        esp_err_t err = dali_103_query_device_status(dali, DALI_ADDR_SHORT, addr, &dev_status, DALI_TX_TIMEOUT_MS);
        if (err == ESP_OK) {
            uint8_t num_inst = 0;
            dali_103_query_number_of_instances(dali, DALI_ADDR_SHORT, addr, &num_inst, DALI_TX_TIMEOUT_MS);
            ESP_LOGI(TAG, "    103 addr %2d present  (status=0x%02X, instances=%d)", addr, dev_status, num_inst);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
#endif /* CONFIG_DALI_PART103_ENABLED */
}

/* =========================================================================
 * STEP 3: DAPC dimming sequence — each addressed lamp in turn
 * ========================================================================= */

#if CONFIG_DALI_PART102_ENABLED
static void step3_dapc_all(dali_master_handle_t dali)
{
    ESP_LOGI(TAG, "--- STEP 3: DAPC dimming sequence (all addresses) ---");

    for (uint8_t addr = 0; addr < 64; addr++) {
        /* Probe: skip if no device at this address */
        int probe = DALI_RESULT_NO_REPLY;
        dali_master_transaction_config_t tx = {
            .addr_type = DALI_ADDR_SHORT, .addr = addr,
            .is_cmd = true, .command = DALI_CMD_QUERY_STATUS,
            .send_twice = false, .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
        };
        dali_master_do_transaction(dali, &tx, &probe);
        if (!DALI_RESULT_IS_VALID(probe)) {
            continue;
        }

        ESP_LOGI(TAG, "  addr=%d: OFF -> 25%% -> 50%% -> MAX -> OFF", addr);
        dapc(dali, addr, 0x00);
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
        dapc(dali, addr, 0x3F);
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
        dapc(dali, addr, 0x7F);
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
        dapc(dali, addr, 0xFE);
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
        dapc(dali, addr, 0x00);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

/* =========================================================================
 * STEP 4: Indirect commands
 * ========================================================================= */

static void step4_indirect(dali_master_handle_t dali)
{
    /* Use the first discovered DT6 lamp; fall back to DT8 if no DT6 found */
    uint8_t target = (g_lamp_count > 0) ? g_lamp_addrs[0] :
#if CONFIG_DALI_PART209_ENABLED
                     (g_dt8_addr != ADDR_NOT_FOUND) ? g_dt8_addr :
#endif
                     ADDR_NOT_FOUND;

    if (target == ADDR_NOT_FOUND) {
        ESP_LOGW(TAG, "--- STEP 4: No Part 102 device found, skipping ---");
        return;
    }

    ESP_LOGI(TAG, "--- STEP 4: Indirect commands (addr=%d) ---", target);

    ESP_LOGI(TAG, "  RECALL_MAX_LEVEL");
    cmd(dali, target, DALI_CMD_RECALL_MAX_LEVEL, false);
    vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

    ESP_LOGI(TAG, "  STORE_ACTUAL_LEVEL (send-twice)");
    cmd(dali, target, DALI_CMD_STORE_ACTUAL_LEVEL, true);
    vTaskDelay(pdMS_TO_TICKS(200));
}
#endif /* CONFIG_DALI_PART102_ENABLED */

/* =========================================================================
 * STEP 5: Query commands — Part 102 lamp + Part 103 sensor
 * ========================================================================= */

static void step5_query(dali_master_handle_t dali)
{
    ESP_LOGI(TAG, "--- STEP 5: Query commands ---");

#if CONFIG_DALI_PART102_ENABLED
    /* Query all discovered DT6 lamps */
    for (uint8_t i = 0; i < g_lamp_count; i++) {
        uint8_t addr = g_lamp_addrs[i];
        ESP_LOGI(TAG, "  [Part 102] DT6 lamp addr=%d", addr);
        query(dali, addr, "STATUS",       DALI_CMD_QUERY_STATUS);
        query(dali, addr, "ACTUAL_LEVEL", DALI_CMD_QUERY_ACTUAL_LEVEL);
        query(dali, addr, "MAX_LEVEL",    DALI_CMD_QUERY_MAX_LEVEL);
        query(dali, addr, "MIN_LEVEL",    DALI_CMD_QUERY_MIN_LEVEL);
        query(dali, addr, "DEVICE_TYPE",  DALI_CMD_QUERY_DEVICE_TYPE);
    }

#if CONFIG_DALI_PART209_ENABLED
    if (g_dt8_addr != ADDR_NOT_FOUND) {
        ESP_LOGI(TAG, "  [Part 102] DT8 lamp addr=%d", g_dt8_addr);
        query(dali, g_dt8_addr, "STATUS",       DALI_CMD_QUERY_STATUS);
        query(dali, g_dt8_addr, "ACTUAL_LEVEL", DALI_CMD_QUERY_ACTUAL_LEVEL);
        query(dali, g_dt8_addr, "DEVICE_TYPE",  DALI_CMD_QUERY_DEVICE_TYPE);
    }
#endif
#endif /* CONFIG_DALI_PART102_ENABLED */

#if CONFIG_DALI_PART103_ENABLED
    ESP_LOGI(TAG, "  [Part 103] sensor addr=%d", SENSOR_ADDR);
    uint8_t dev_status = 0;
    if (dali_103_query_device_status(dali, DALI_ADDR_SHORT, SENSOR_ADDR,
                                     &dev_status, DALI_TX_TIMEOUT_MS) == ESP_OK) {
        ESP_LOGI(TAG, "    103 device_status = 0x%02X", dev_status);
    } else {
        ESP_LOGW(TAG, "    103 device_status = NO REPLY");
    }

    uint8_t num_inst = 0;
    if (dali_103_query_number_of_instances(dali, DALI_ADDR_SHORT, SENSOR_ADDR,
                                           &num_inst, DALI_TX_TIMEOUT_MS) == ESP_OK) {
        ESP_LOGI(TAG, "    103 num_instances = %d", num_inst);
        for (uint8_t i = 0; i < num_inst; i++) {
            uint8_t inst_type = 0xFF;
            dali_103_query_instance_type(dali, DALI_ADDR_SHORT, SENSOR_ADDR,
                                         i, &inst_type, DALI_TX_TIMEOUT_MS);
            ESP_LOGI(TAG, "      instance[%d] type=%d%s", i, inst_type,
                     inst_type == 3 ? " (occupancy)" :
                     inst_type == 4 ? " (light)" : "");
        }
    }
#endif /* CONFIG_DALI_PART103_ENABLED */

#if CONFIG_DALI_PART303_304_ENABLED
    bool occupied = false;
    if (dali_303_query_occupancy(dali, DALI_ADDR_SHORT, SENSOR_ADDR,
                                 SENSOR_INSTANCE, &occupied, DALI_TX_TIMEOUT_MS) == ESP_OK) {
        ESP_LOGI(TAG, "    303 occupancy = %s", occupied ? "OCCUPIED" : "UNOCCUPIED");
    }
#endif
}

/* =========================================================================
 * STEP 6: Sensor-triggered lighting
 *   - Detected occupied:
 *       · All DT6 lamps blink simultaneously
 *       · DT8 lamp alternates Red / Blue
 * ========================================================================= */

#if CONFIG_DALI_PART102_ENABLED && CONFIG_DALI_PART303_304_ENABLED
static void step6_sensor_triggered(dali_master_handle_t dali)
{
    ESP_LOGI(TAG, "--- STEP 6: Sensor-triggered lighting demo ---");
    ESP_LOGI(TAG, "  Waiting for occupancy on sensor addr=%d ...", SENSOR_ADDR);

    /* Poll occupancy — sensor stays in quiescent mode the whole time */
    bool occupied = false;
    while (!occupied) {
        dali_303_query_occupancy(dali, DALI_ADDR_SHORT, SENSOR_ADDR,
                                 SENSOR_INSTANCE, &occupied, DALI_TX_TIMEOUT_MS);
        if (!occupied) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
    ESP_LOGI(TAG, "  Occupied! Triggering simultaneous DT6 blink and DT8 Red/Blue flash.");

    /* -----------------------------------------------------------------------
     * DT8: turn on first so color changes are visible
     * --------------------------------------------------------------------- */
#if CONFIG_DALI_PART209_ENABLED
    if (g_dt8_addr != ADDR_NOT_FOUND) {
        cmd(dali, g_dt8_addr, DALI_CMD_RECALL_MAX_LEVEL, false);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
#endif

    /* -----------------------------------------------------------------------
     * Simultaneous blink loop:
     *   - Each iteration: DT6 lamps ON + DT8 Red → DT6 lamps OFF + DT8 Blue
     * --------------------------------------------------------------------- */
#if CONFIG_DALI_PART209_ENABLED
    dali_color_val_t color;
#endif

    for (int i = 0; i < BLINK_TIMES; i++) {
        /* --- Phase A: DT6 ON (bright) + DT8 Red --- */
        for (uint8_t j = 0; j < g_lamp_count; j++) {
            dapc(dali, g_lamp_addrs[j], 0xFE);
        }
#if CONFIG_DALI_PART209_ENABLED
        if (g_dt8_addr != ADDR_NOT_FOUND) {
            ESP_LOGI(TAG, "  [%d/%d] DT6 ON + DT8 Red", i + 1, BLINK_TIMES);
            color.rgb.r = 0xFE; color.rgb.g = 0x00; color.rgb.b = 0x00;
            dali_master_set_color(dali, DALI_ADDR_SHORT, g_dt8_addr,
                                  DALI_COLOR_RGB, color, DALI_TX_TIMEOUT_MS);
        } else
#endif
        {
            ESP_LOGI(TAG, "  [%d/%d] DT6 ON", i + 1, BLINK_TIMES);
        }
        vTaskDelay(pdMS_TO_TICKS(500));

        /* --- Phase B: DT6 OFF + DT8 Blue --- */
        for (uint8_t j = 0; j < g_lamp_count; j++) {
            dapc(dali, g_lamp_addrs[j], 0x00);
        }
#if CONFIG_DALI_PART209_ENABLED
        if (g_dt8_addr != ADDR_NOT_FOUND) {
            ESP_LOGI(TAG, "  [%d/%d] DT6 OFF + DT8 Blue", i + 1, BLINK_TIMES);
            color.rgb.r = 0x00; color.rgb.g = 0x00; color.rgb.b = 0xFE;
            dali_master_set_color(dali, DALI_ADDR_SHORT, g_dt8_addr,
                                  DALI_COLOR_RGB, color, DALI_TX_TIMEOUT_MS);
        } else
#endif
        {
            ESP_LOGI(TAG, "  [%d/%d] DT6 OFF", i + 1, BLINK_TIMES);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    /* Return all lamps to idle */
    for (uint8_t j = 0; j < g_lamp_count; j++) {
        dapc(dali, g_lamp_addrs[j], 0x00);
    }
#if CONFIG_DALI_PART209_ENABLED
    if (g_dt8_addr != ADDR_NOT_FOUND) {
        dapc(dali, g_dt8_addr, 0x00);
    }
#endif
}
#endif /* CONFIG_DALI_PART102_ENABLED && CONFIG_DALI_PART303_304_ENABLED */

/* =========================================================================
 * app_main
 * ========================================================================= */

void app_main(void)
{
    /* Create DALI master — adjust GPIO numbers to match your board */
    dali_master_handle_t dali;
    dali_master_config_t cfg = {
        .rx_gpio  = DALI_RX_GPIO,
        .tx_gpio  = DALI_TX_GPIO,
        .invert_tx = true,
        .invert_rx = true,
    };
    dali_master_rmt_config_t rmt_cfg = {
        .mem_block_symbols = 64,
    };
    ESP_ERROR_CHECK(dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

    /* STEP 1: assign short addresses to all gear and input devices */
    step1_commission(dali);
    vTaskDelay(pdMS_TO_TICKS(500));

    /* STEP 2: scan which addresses responded and identify DT6 / DT8 */
    step2_scan_and_identify(dali);
    vTaskDelay(pdMS_TO_TICKS(500));

#if CONFIG_DALI_PART102_ENABLED
    /* STEP 3: DAPC dimming on every present lamp */
    step3_dapc_all(dali);
    vTaskDelay(pdMS_TO_TICKS(500));

    /* STEP 4: indirect commands on first discovered lamp */
    step4_indirect(dali);
    vTaskDelay(pdMS_TO_TICKS(500));
#endif

    /* STEP 5: query all discovered lamps + Part 103 sensor */
    step5_query(dali);
    vTaskDelay(pdMS_TO_TICKS(500));

#if CONFIG_DALI_PART102_ENABLED && CONFIG_DALI_PART303_304_ENABLED
    /* STEP 6: wait for occupancy, then simultaneous DT6 blink + DT8 Red/Blue */
    step6_sensor_triggered(dali);
#endif

    ESP_LOGI(TAG, "=== Demo complete ===");
}
