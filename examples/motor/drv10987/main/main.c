/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "drv10987.h"
#include "esp_log.h"

#define DIRECTION_IO        21
#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_SDA_IO   23
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  100000

static i2c_bus_handle_t i2c_bus = NULL;
static drv10987_handle_t drv10987 = NULL;
static char *TAG = "DRV10987 Demo";

void app_main()
{
    drv10987_fault_t fault = {0};
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);

    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus create failed");
        return;
    }

    drv10987_config_t drv10987_cfg = {
        .bus = i2c_bus,
        .dev_addr = DRV10987_I2C_ADDRESS_DEFAULT,
        .dir_pin = DIRECTION_IO,
    };

    esp_err_t err = drv10987_create(&drv10987, &drv10987_cfg);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DRV10987 create failed");
        return;
    }

    drv10987_set_direction_uvw(drv10987);                                      /*!< Set the direction of motor rotation to V->V->W */
    drv10987_clear_driver_fault(drv10987);                                     /*!< Clear all faults. */
    drv10987_write_access_to_eeprom(drv10987, 5);                              /*!< Enable access to EEPROM and check if EEPROM is ready for read/write access. */

    drv10987_config1_t config1 = DEFAULT_DRV10987_CONFIG1;
    drv10987_write_config_register(drv10987, CONFIG1_REGISTER_ADDR, &config1); /*!< Mainly written for motor phase resistance, FG and Spread spectrum modulation control. */
    drv10987_config2_t config2 = DEFAULT_DRV10987_CONFIG2;
    drv10987_write_config_register(drv10987, CONFIG2_REGISTER_ADDR, &config2); /*!< Mainly written for communtation advance value and kt value.  */
    drv10987_config3_t config3 = DEFAULT_DRV10987_CONFIG3;
    drv10987_write_config_register(drv10987, CONFIG3_REGISTER_ADDR, &config3); /*!< Mainly writes braking modes, open-loop parameters, etc. */
    drv10987_config4_t config4 = DEFAULT_DRV10987_CONFIG4;
    drv10987_write_config_register(drv10987, CONFIG4_REGISTER_ADDR, &config4); /*!< Mainly write the alignment time, open to closed loop threshold, and open loop startup acceleration.  */
    drv10987_config5_t config5 = DEFAULT_DRV10987_CONFIG5;
    drv10987_write_config_register(drv10987, CONFIG5_REGISTER_ADDR, &config5); /*!< Mainly writes hardware overcurrent thresholds, software overcurrent thresholds and some other exceptions. */
    drv10987_config6_t config6 = DEFAULT_DRV10987_CONFIG6;
    drv10987_write_config_register(drv10987, CONFIG6_REGISTER_ADDR, &config6); /*!< Mainly writes Slew-rate control for phase node, Minimum duty-cycle limit, Minimum duty-cycle limit, etc. */
    drv10987_config7_t config7 = DEFAULT_DRV10987_CONFIG7;
    drv10987_write_config_register(drv10987, CONFIG7_REGISTER_ADDR, &config7); /*!< Mainly writes driver dead time, SCORE control constant, IPD current thtrshold, etc. */

    drv10987_mass_write_acess_to_eeprom(drv10987);

    while (1) {
        if (drv10987_read_eeprom_status(drv10987) == EEPROM_READY) {           /*!< EEPROM is now updated with the contents of the shadow sregisters */
            break;
        }
    }

    drv10987_enable_control(drv10987);
    drv10987_write_speed(drv10987, CONFIG_EXAMPLE_MOROT_SPEED);                /*!< Write speed control via I2C */
    drv10987_enable_control(drv10987);

    while (1) {
        drv10987_read_driver_status(drv10987, &fault);
        if (fault.code != 0) {
            ESP_LOGE(TAG, "Fault detected:%d", fault.code);
            drv10987_clear_driver_fault(drv10987);                             /*!< Once the device detects a fault, the motor will stop rotating for 5 seconds. After 5 seconds, the motor will start rotating again. The motor will not start rotating again after 5 seconds unless the fault is cleared within this time period.  */
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    drv10987_delete(drv10987);
}
