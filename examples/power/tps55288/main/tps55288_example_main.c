/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "tps55288.h"

static const char *TAG = "example";

#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY

#define TPS55288_I2C_ADDR               0x74
/* Sense resistor, in milliohms. Change this to match the board. */
#define TPS55288_SENSE_RESISTOR_MOHM    10
#define TPS55288_IOUT_LIMIT_MA          500

/* Hold each target long enough for a meter to settle. */
#define TPS55288_VOUT_HOLD_MS           1000

#define TPS55288_STATUS_SCP             (1U << 7)
#define TPS55288_STATUS_OCP             (1U << 6)
#define TPS55288_STATUS_OVP             (1U << 5)

static void i2c_bus_init(i2c_master_bus_handle_t *bus_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));
}

static void log_status(tps55288_handle_t tps)
{
    uint8_t status = 0;
    const char *mode;

    ESP_ERROR_CHECK(tps55288_read_status(tps, &status));
    switch (status & 0x03) {
    case 0x00:
        mode = "boost";
        break;
    case 0x01:
        mode = "buck";
        break;
    case 0x02:
        mode = "buck-boost";
        break;
    default:
        mode = "not switching";
        break;
    }

    ESP_LOGI(TAG, "STATUS=0x%02X SCP=%d OCP=%d OVP=%d mode=%s",
             status,
             (status & TPS55288_STATUS_SCP) ? 1 : 0,
             (status & TPS55288_STATUS_OCP) ? 1 : 0,
             (status & TPS55288_STATUS_OVP) ? 1 : 0,
             mode);
}

void app_main(void)
{
    i2c_master_bus_handle_t bus_handle;
    tps55288_handle_t tps = NULL;

    i2c_bus_init(&bus_handle);
    const tps55288_config_t config = {
        .bus = bus_handle,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        .i2c_addr = TPS55288_I2C_ADDR,
        .sense_resistor_mohm = TPS55288_SENSE_RESISTOR_MOHM,
    };
    ESP_ERROR_CHECK(tps55288_create(&config, &tps));
    ESP_LOGI(TAG, "I2C initialized successfully");

    log_status(tps);

    /* Registers survive an ESP reset. Turn the output off before writing VOUT,
     * or a still-enabled rail jumps to the new target. */
    ESP_ERROR_CHECK(tps55288_enable_output(tps, false));
    ESP_ERROR_CHECK(tps55288_set_iout_limit_ma(tps, TPS55288_IOUT_LIMIT_MA));
    ESP_ERROR_CHECK(tps55288_set_vout_mv(tps, 2000));
    ESP_ERROR_CHECK(tps55288_enable_output(tps, true));
    vTaskDelay(pdMS_TO_TICKS(TPS55288_VOUT_HOLD_MS));

    ESP_ERROR_CHECK(tps55288_set_vout_mv(tps, 3000));
    vTaskDelay(pdMS_TO_TICKS(TPS55288_VOUT_HOLD_MS));

    ESP_ERROR_CHECK(tps55288_enable_output(tps, false));
}
