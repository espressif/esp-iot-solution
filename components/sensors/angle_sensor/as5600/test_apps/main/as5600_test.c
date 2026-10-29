/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdbool.h>
#include "unity.h"
#include "as5600.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MASTER_SCL_IO    32
#define I2C_MASTER_SDA_IO    33
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000

static const char *const s_as5600_magnet_status_str[] = {
    [AS5600_MAGNET_OK] = "OK",
    [AS5600_MAGNET_TOO_WEAK] = "too weak",
    [AS5600_MAGNET_TOO_STRONG] = "too strong",
    [AS5600_MAGNET_NOT_DETECTED] = "not detected",
};

#define AS5600_MAGNET_STATUS_STR_COUNT (sizeof(s_as5600_magnet_status_str) / sizeof(s_as5600_magnet_status_str[0]))

/** Degrees tolerance after ZPOS cal: noise / filter may leave angle near 0 or wrap to ~360. */
#define AS5600_ZERO_CAL_TOL_DEG 5.0f

static bool as5600_degrees_near_zero_or_wrap_360(float deg, float tol_deg)
{
    return (deg <= tol_deg) || (deg >= 360.0f - tol_deg);
}

static as5600_handle_t as5600 = NULL;
static i2c_master_bus_handle_t bus_handle = NULL;

static void as5600_test_init(void)
{
    esp_err_t ret;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ret = i2c_new_master_bus(&bus_config, &bus_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    as5600_i2c_config_t i2c_conf = {
        .scl_speed_hz = I2C_MASTER_FREQ_HZ
    };
    ret = as5600_new_sensor(bus_handle, &i2c_conf, &as5600);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(as5600);
}

static void as5600_test_deinit(void)
{
    if (as5600 != NULL) {
        TEST_ASSERT_EQUAL(ESP_OK, as5600_del_sensor(as5600));
        as5600 = NULL;
    }
    if (bus_handle != NULL) {
        i2c_del_master_bus(bus_handle);
        bus_handle = NULL;
    }
}

static void as5600_test_get_angle(void)
{
    uint16_t raw;
    float degrees;
    as5600_magnet_status_t status;
    esp_err_t ret;

    ret = as5600_get_angle_raw(as5600, &raw);
    if (ret != ESP_OK) {
        printf("AS5600 not connected or no ack\n");
        return;
    }

    ret = as5600_get_angle_degrees(as5600, &degrees);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    ret = as5600_get_magnet_status(as5600, &status);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    const char *status_str = ((unsigned)status < AS5600_MAGNET_STATUS_STR_COUNT)
                             ? s_as5600_magnet_status_str[status]
                             : "unknown";
    printf("AS5600 raw: %u, degrees: %.2f, magnet status: %s\n", (unsigned)raw, (double)degrees, status_str);
}

static void as5600_test_zero_calibration(void)
{
    esp_err_t ret;
    as5600_magnet_status_t status;

    ret = as5600_get_magnet_status(as5600, &status);
    if (ret != ESP_OK) {
        printf("AS5600 magnet status read failed, skip zero calibration check\n");
        return;
    }
    if (status != AS5600_MAGNET_OK) {
        printf("AS5600 magnet not OK (status=%d), skip zero calibration check\n", (int)status);
        return;
    }

    float degrees_before;
    ret = as5600_get_angle_degrees(as5600, &degrees_before);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    printf("AS5600 before ZPOS cal: degrees=%.2f\n", (double)degrees_before);

    ret = as5600_set_zero_position(as5600);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    /* Allow filtered ANGLE output to settle after ZPOS write */
    vTaskDelay(pdMS_TO_TICKS(20));

    float degrees;
    ret = as5600_get_angle_degrees(as5600, &degrees);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    printf("AS5600 after ZPOS cal: degrees=%.2f (expect ~0 or ~360 wrap)\n", (double)degrees);
    TEST_ASSERT_TRUE_MESSAGE(as5600_degrees_near_zero_or_wrap_360(degrees, AS5600_ZERO_CAL_TOL_DEG),
                             "angle after zero calibration should be near 0 deg (or near 360 if wrapped)");
}

static void as5600_test_conf_readback(void)
{
    const as5600_conf_t conf_write = {
        .power_mode   = AS5600_POWER_MODE_LPM1,
        .hysteresis   = AS5600_HYSTERESIS_2_LSB,
        .output_stage = AS5600_OUTPUT_STAGE_ANALOG_REDUCED,
        .pwm_freq     = AS5600_PWM_FREQ_460HZ,
        .slow_filter  = AS5600_SLOW_FILTER_4X,
        .fast_filter  = AS5600_FAST_FILTER_9_LSB,
        .watchdog     = true,
    };

    esp_err_t ret = as5600_set_conf(as5600, &conf_write);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    as5600_conf_t conf_read = {0};
    ret = as5600_get_conf(as5600, &conf_read);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    TEST_ASSERT_EQUAL_INT(conf_write.power_mode,   conf_read.power_mode);
    TEST_ASSERT_EQUAL_INT(conf_write.hysteresis,   conf_read.hysteresis);
    TEST_ASSERT_EQUAL_INT(conf_write.output_stage, conf_read.output_stage);
    TEST_ASSERT_EQUAL_INT(conf_write.pwm_freq,     conf_read.pwm_freq);
    TEST_ASSERT_EQUAL_INT(conf_write.slow_filter,  conf_read.slow_filter);
    TEST_ASSERT_EQUAL_INT(conf_write.fast_filter,  conf_read.fast_filter);
    TEST_ASSERT_EQUAL_INT(conf_write.watchdog,     conf_read.watchdog);

    printf("AS5600 CONF readback: pm=%d hyst=%d outs=%d pwmf=%d sf=%d fth=%d wd=%d\n",
           (int)conf_read.power_mode, (int)conf_read.hysteresis,
           (int)conf_read.output_stage, (int)conf_read.pwm_freq,
           (int)conf_read.slow_filter, (int)conf_read.fast_filter,
           (int)conf_read.watchdog);

    /* Restore power-on defaults */
    const as5600_conf_t conf_default = {0};
    TEST_ASSERT_EQUAL(ESP_OK, as5600_set_conf(as5600, &conf_default));
}

TEST_CASE("AS5600 angle sensor test", "[as5600][iot][sensor]")
{
    as5600_test_init();
    as5600_test_get_angle();
    as5600_test_deinit();
}

TEST_CASE("AS5600 ZPOS zero calibration", "[as5600][iot][sensor]")
{
    as5600_test_init();
    as5600_test_zero_calibration();
    as5600_test_deinit();
}

TEST_CASE("AS5600 CONF write-readback", "[as5600][iot][sensor]")
{
    as5600_test_init();
    as5600_test_conf_readback();
    as5600_test_deinit();
}

void app_main(void)
{
    printf("  ___   ____ _____ ___  ___ \n");
    printf(" / _ \\ / ___|_   _/ _ \\| __|\n");
    printf("| (_) |\\___ \\ | || | | | _| \n");
    printf(" \\___/ |___/ |_||_| |_|___|\n");
    printf(" Angle Sensor Test\n\n");

    unity_run_menu();
}
