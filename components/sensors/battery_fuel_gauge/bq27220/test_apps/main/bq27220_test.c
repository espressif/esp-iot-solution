/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "string.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "bq27220.h"

#define I2C_MASTER_SCL_IO          GPIO_NUM_1         /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO          GPIO_NUM_2         /*!< gpio number for I2C master data  */

#define TEST_MEMORY_LEAK_FIRST_THRESHOLD (-110)
#define MANUAL_REBOOT_TEST_NAME "bq27220 profile is not rewritten after 200 ESP restarts"
#define MANUAL_REBOOT_TEST_MAGIC 0x42513252U
#define MANUAL_REBOOT_TEST_COUNT 200U
#define PROFILE_UPDATE_START_LOG "Start updating battery profile"

typedef enum {
    REBOOT_TEST_PROFILE_DEFAULT,
    REBOOT_TEST_PROFILE_ALTERNATE,
} reboot_test_profile_t;

typedef struct {
    uint32_t magic;
    uint32_t inverted_magic;
    uint32_t reboot_count;
    reboot_test_profile_t profile;
} reboot_test_state_t;

static i2c_bus_handle_t i2c_bus = NULL;
static bq27220_handle_t bq27220 = NULL;
// Keep the manual test progress across esp_restart().
static __NOINIT_ATTR reboot_test_state_t reboot_test_state;
static vprintf_like_t previous_log_vprintf;
static uint32_t profile_update_start_count;

static const char *TAG = "bq27220_test";

static const parameter_cedv_t default_cedv = {
    .full_charge_cap = 650,
    .design_cap = 650,
    .reserve_cap = 0,
    .near_full = 200,
    .self_discharge_rate = 20,
    .EDV0 = 3490,
    .EDV1 = 3511,
    .EDV2 = 3535,
    .EMF = 3670,
    .C0 = 115,
    .R0 = 968,
    .T0 = 4547,
    .R1 = 4764,
    .TC = 11,
    .C1 = 0,
    .DOD0 = 4147,
    .DOD10 = 4002,
    .DOD20 = 3969,
    .DOD30 = 3938,
    .DOD40 = 3880,
    .DOD50 = 3824,
    .DOD60 = 3794,
    .DOD70 = 3753,
    .DOD80 = 3677,
    .DOD90 = 3574,
    .DOD100 = 3490,
};

static const gauging_config_t default_config = {
    .CCT = 1,
    .CSYNC = 0,
    .EDV_CMP = 0,
    .SC = 1,
    .FIXED_EDV0 = 0,
    .FCC_LIM = 1,
    .FC_FOR_VDQ = 1,
    .IGNORE_SD = 1,
    .SME0 = 0,
};

static bool reboot_test_state_is_valid(void)
{
    bool profile_is_valid = reboot_test_state.profile == REBOOT_TEST_PROFILE_DEFAULT || reboot_test_state.profile == REBOOT_TEST_PROFILE_ALTERNATE;
    return reboot_test_state.magic == MANUAL_REBOOT_TEST_MAGIC && reboot_test_state.inverted_magic == ~MANUAL_REBOOT_TEST_MAGIC &&
           reboot_test_state.reboot_count <= MANUAL_REBOOT_TEST_COUNT && profile_is_valid;
}

static void reboot_test_state_clear(void)
{
    memset(&reboot_test_state, 0, sizeof(reboot_test_state));
}

static int test_log_vprintf(const char *format, va_list args)
{
    if (strstr(format, PROFILE_UPDATE_START_LOG) != NULL) {
        ++profile_update_start_count;
    }
    return previous_log_vprintf(format, args);
}

static bool test_bq27220_init_with_cedv(const parameter_cedv_t *cedv)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400 * 1000,
    };
    i2c_bus = i2c_bus_create(I2C_NUM_0, &conf);

    bq27220_config_t bq27220_cfg = {
        .i2c_bus = i2c_bus,
        .cfg = &default_config,
        .cedv = cedv,
    };
    bq27220 = bq27220_create(&bq27220_cfg);
    if (!bq27220) {
        (void)i2c_bus_delete(&i2c_bus);
    }
    return bq27220 != NULL;
}

static void test_bq27220_init()
{
    TEST_ASSERT_TRUE(test_bq27220_init_with_cedv(&default_cedv));
}

static void test_bq27220_deinit()
{
    TEST_ASSERT(bq27220_delete(bq27220) == ESP_OK);
    bq27220 = NULL;
    TEST_ASSERT(i2c_bus_delete(&i2c_bus) == ESP_OK);
}

static bool test_bq27220_init_and_detect_update(const parameter_cedv_t *cedv, uint32_t *update_count)
{
    profile_update_start_count = 0;
    previous_log_vprintf = esp_log_set_vprintf(test_log_vprintf);
    bool initialized = test_bq27220_init_with_cedv(cedv);
    esp_log_set_vprintf(previous_log_vprintf);
    *update_count = profile_update_start_count;
    if (!initialized) {
        return false;
    }
    esp_err_t ret = bq27220_delete(bq27220);
    bq27220 = NULL;
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete BQ27220 handle: %s", esp_err_to_name(ret));
        return false;
    }
    ret = i2c_bus_delete(&i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete I2C bus: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}

static void reboot_test_fail(const char *message)
{
    ESP_LOGE(TAG, "%s at restart %lu/%u", message, (unsigned long)reboot_test_state.reboot_count, MANUAL_REBOOT_TEST_COUNT);
    reboot_test_state_clear();
    TEST_FAIL_MESSAGE(message);
}

static void test_bq27220_print_info(bq27220_handle_t bq27220Handle)
{
    battery_status_t status = {};
    bq27220_get_battery_status(bq27220Handle, &status);
    ESP_LOGI(TAG, "Battery Status - DSG: %d, SYSDWN: %d, TDA: %d, BATTPRES: %d, AUTH_GD: %d, OCVGD: %d, TCA: %d, RSVD: %d, CHGINH: %d, FC: %d, OTD: %d, OTC: %d, SLEEP: %d, OCVFAIL: %d, OCVCOMP: %d, FD: %d",
             status.DSG, status.SYSDWN, status.TDA, status.BATTPRES,
             status.AUTH_GD, status.OCVGD, status.TCA, status.RSVD,
             status.CHGINH, status.FC, status.OTD, status.OTC,
             status.SLEEP, status.OCVFAIL, status.OCVCOMP, status.FD);

    uint16_t vol = bq27220_get_voltage(bq27220Handle);
    int16_t current = bq27220_get_current(bq27220Handle);
    uint16_t rc = bq27220_get_remaining_capacity(bq27220Handle);
    uint16_t full_cap = bq27220_get_full_charge_capacity(bq27220Handle);
    uint16_t temp = bq27220_get_temperature(bq27220Handle) / 10 - 273; // Convert from 0.1K to Celsius
    uint16_t cycle_cnt = bq27220_get_cycle_count(bq27220Handle);
    uint16_t soc = bq27220_get_state_of_charge(bq27220Handle);
    int16_t avg_power = bq27220_get_average_power(bq27220Handle); // in mW
    int16_t max_load = bq27220_get_maxload_current(bq27220Handle); // in mA
    uint16_t time_to_empty = bq27220_get_time_to_empty(bq27220Handle);
    uint16_t time_to_full = bq27220_get_time_to_full(bq27220Handle);

    ESP_LOGI(TAG, "Battery Info - Vol: %dmv, Current: %dmA, Power: %dmW, Remaining Capacity: %dmAh, Full Charge Capacity: %dmAh, Temperature: %dC, Cycle Count: %d, SOC: %d%%, Max Load: %dmA, Time to empty: %dmin, Time to full: %dmin",
             vol, current, avg_power, rc, full_cap, temp, cycle_cnt, soc, max_load, time_to_empty, time_to_full);
}

TEST_CASE("bq27220 basic information query test", "[voltage][percent][charge_rate]")
{
    test_bq27220_init();
    test_bq27220_print_info(bq27220);
    test_bq27220_deinit();
}

TEST_CASE(MANUAL_REBOOT_TEST_NAME, "[bq27220][manual]")
{
    parameter_cedv_t alternate_cedv = default_cedv;
    alternate_cedv.DOD20++;

    if (!reboot_test_state_is_valid()) {
        uint32_t update_count = 0;
        reboot_test_state_clear();
        if (!test_bq27220_init_and_detect_update(&default_cedv, &update_count)) {
            reboot_test_fail("Initial default profile initialization failed");
            return;
        }
        reboot_test_profile_t selected_profile = REBOOT_TEST_PROFILE_DEFAULT;
        if (update_count == 0) {
            // Force one mismatch when the default profile already matches.
            uint32_t alternate_update_count = 0;
            if (!test_bq27220_init_and_detect_update(&alternate_cedv, &alternate_update_count)) {
                reboot_test_fail("Initial alternate profile initialization failed");
                return;
            }
            update_count += alternate_update_count;
            selected_profile = REBOOT_TEST_PROFILE_ALTERNATE;
        }
        if (update_count != 1) {
            reboot_test_fail("Profile must be updated exactly once before restart verification");
            return;
        }
        reboot_test_state = (reboot_test_state_t) {
            .magic = MANUAL_REBOOT_TEST_MAGIC,
            .inverted_magic = ~MANUAL_REBOOT_TEST_MAGIC,
            .reboot_count = 0,
            .profile = selected_profile,
        };
        ESP_LOGI(TAG, "Initial mismatch caused exactly one profile update; starting %u software restarts", MANUAL_REBOOT_TEST_COUNT);
    } else {
        if (esp_reset_reason() != ESP_RST_SW) {
            reboot_test_fail("Unexpected reset reason during reboot test");
            return;
        }
        const parameter_cedv_t *selected_cedv = reboot_test_state.profile == REBOOT_TEST_PROFILE_DEFAULT ? &default_cedv : &alternate_cedv;
        uint32_t update_count = 0;
        if (!test_bq27220_init_and_detect_update(selected_cedv, &update_count)) {
            reboot_test_fail("BQ27220 initialization failed after ESP restart");
            return;
        }
        if (update_count != 0) {
            reboot_test_fail("Battery profile update was triggered after ESP restart");
            return;
        }
        ++reboot_test_state.reboot_count;
        ESP_LOGI(TAG, "Restart verification passed: %lu/%u", (unsigned long)reboot_test_state.reboot_count, MANUAL_REBOOT_TEST_COUNT);
    }

    if (reboot_test_state.reboot_count < MANUAL_REBOOT_TEST_COUNT) {
        esp_restart();
    }

    reboot_test_state_clear();
    ESP_LOGI(TAG, "Profile update remained skipped after %u ESP restarts", MANUAL_REBOOT_TEST_COUNT);
}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, ssize_t threshold, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    if (!(delta >= threshold)) {
        ESP_LOGE(TAG, "Memory leak detected, delta: %d bytes, threshold: %d bytes", delta, threshold);
    }
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    ssize_t threshold = TEST_MEMORY_LEAK_FIRST_THRESHOLD;
    static bool is_first = true;
    if (is_first) {
        is_first = false;
    } else {
        threshold = 0;
    }
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, threshold, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, threshold, "32BIT");
}

void app_main(void)
{
    printf("BQ27220 TEST \n");
    if (reboot_test_state_is_valid()) {
        // Resume the selected manual case without serial input after each restart.
        UNITY_BEGIN();
        unity_run_test_by_name(MANUAL_REBOOT_TEST_NAME);
        UNITY_END();
    }
    unity_run_menu();
}
