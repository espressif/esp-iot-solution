/* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
* SPDX-License-Identifier: Apache-2.0
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/uart.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bl0942_dev_t *bl0942_handle_t;

typedef struct {
    uint8_t addr;               /* Device address [A2,A1] -> 0..3, SSOP10L uses 0 */
    float shunt_resistor_ohm;   /* Sampling resistor R (Ohm) */
    float voltage_div_ratio;    /* Voltage divider ratio K (line = pin*K) */
    bool use_spi;               /* When true, SPI is used instead of UART */

    union {
        struct {
            uart_port_t uart_num;       /* UART port */
            int tx_io_num;              /* UART TX GPIO */
            int rx_io_num;              /* UART RX GPIO */
            int sel_io_num;             /* SEL pin GPIO (set to low for UART), -1 if hard-wired */
            uint32_t baud_rate;         /* 4800/9600/19200/38400 */
        } uart;

        struct {
            spi_host_device_t spi_host; /* e.g. SPI2_HOST/SPI3_HOST */
            int mosi_io_num;            /* SPI MOSI (SDI to BL0942) */
            int miso_io_num;            /* SPI MISO (SDO from BL0942) */
            int sclk_io_num;            /* SPI SCLK */
            int cs_io_num;              /* SPI CS (to A2_NCS), -1 if managed externally */
        } spi;
    };
} bl0942_config_t;

esp_err_t bl0942_create(bl0942_handle_t *handle, const bl0942_config_t *cfg);
esp_err_t bl0942_delete(bl0942_handle_t handle);

/* Raw register reads via UART */
esp_err_t bl0942_read_reg24(bl0942_handle_t handle, uint8_t addr, uint32_t *val);
esp_err_t bl0942_write_reg24(bl0942_handle_t handle, uint8_t addr, uint32_t val);

/* High-level getters (converted to SI units) */
esp_err_t bl0942_get_voltage(bl0942_handle_t handle, float *volt);
esp_err_t bl0942_get_current(bl0942_handle_t handle, float *amp);
esp_err_t bl0942_get_active_power(bl0942_handle_t handle, float *watt);
esp_err_t bl0942_get_energy_pulses(bl0942_handle_t handle, uint32_t *cf_cnt);
esp_err_t bl0942_get_line_freq(bl0942_handle_t handle, float *hz);

#ifdef __cplusplus
}
#endif
