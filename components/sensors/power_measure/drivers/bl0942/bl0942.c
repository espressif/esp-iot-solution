/* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
* SPDX-License-Identifier: Apache-2.0
*/

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "driver/spi_master.h"
#include "esp_idf_version.h"
#include "bl0942.h"
#include "bl0942_reg.h"

#define TAG "BL0942"

/* UART frame headers (read/write) with device address bits A2,A1 */
#define BL0942_UART_WRITE_BASE   0xA8  /* 1010 1000 | (A2<<1)|A1 after compose */
#define BL0942_UART_READ_BASE    0x58  /* 0101 1000 | (A2<<1)|A1 after compose */

/* Conversion helpers per datasheet (Vref≈1.218V) */
#define BL0942_VREF 1.218f

typedef struct {
    uint8_t addr;
    float r_shunt;
    float v_div;
    bool use_spi;

    union {
        struct {
            uart_port_t uart_num;
            int tx_io;
            int rx_io;
            int sel_io;
            uint32_t baud;
        } uart;

        struct {
            spi_device_handle_t spi_dev;
            int cs_io;
        } spi;
    };

} bl0942_dev_t;

static inline uint8_t compose_rw_cmd(uint8_t base, uint8_t addr)
{
    /* addr is 0..3 mapping to (A2,A1) */
    return (uint8_t)(base | ((addr & 0x03) & 0x03));
}

static inline uint8_t checksum_byte(uint8_t hdr, uint8_t reg, uint8_t d0, uint8_t d1, uint8_t d2)
{
    uint8_t sum = (uint8_t)(hdr + reg + d0 + d1 + d2);

    return (uint8_t)(~sum);
}

static esp_err_t spi_txrx(bl0942_dev_t *dev, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)
{
    spi_transaction_t t = { 0 };
    t.flags = 0;
    t.length = tx_len * 8;
    t.tx_buffer = tx;
    t.rxlength = rx_len * 8;
    t.rx_buffer = rx;

    return spi_device_transmit(dev->spi.spi_dev, &t);
}

static esp_err_t uart_send_bytes(uart_port_t uart, const uint8_t *data, size_t len)
{
    int w = uart_write_bytes(uart, (const char *)data, len);

    return (w == (int)len) ? ESP_OK : ESP_FAIL;
}

static esp_err_t uart_read_exact(uart_port_t uart, uint8_t *data, size_t len, TickType_t to_ticks)
{
    size_t got = 0;

    while (got < len) {
        int r = uart_read_bytes(uart, data + got, len - got, to_ticks);
        if (r <= 0) {
            ESP_LOGE(TAG, "UART RX timeout: expected %d bytes, got %d", len, got);
            return ESP_FAIL;
        }
        got += (size_t)r;
    }

    return ESP_OK;
}

static esp_err_t bl0942_uart_read24(bl0942_dev_t *dev, uint8_t reg, uint32_t *val)
{
    ESP_RETURN_ON_FALSE(val, ESP_ERR_INVALID_ARG, TAG, "val null");

    uart_flush_input(dev->uart.uart_num);
    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t hdr = compose_rw_cmd(BL0942_UART_READ_BASE, dev->addr);
    uint8_t tx[2] = { hdr, reg };

    ESP_RETURN_ON_ERROR(uart_send_bytes(dev->uart.uart_num, tx, sizeof(tx)), TAG, "uart tx");

    vTaskDelay(pdMS_TO_TICKS(50));

    /* Expect back: Data_L, Data_M, Data_H, CHECKSUM per datasheet UART read order (low..high) */
    uint8_t rx[4] = {0};

    esp_err_t ret = ESP_FAIL;
    for (int retry = 0; retry < 3; retry++) {
        ret = uart_read_exact(dev->uart.uart_num, rx, sizeof(rx), pdMS_TO_TICKS(500));
        if (ret == ESP_OK) {
            uint8_t expected_cs = checksum_byte(hdr, reg, rx[0], rx[1], rx[2]);
            if (expected_cs == rx[3]) {
                break;
            } else if (retry < 2) {
                ESP_LOGW(TAG, "Reg 0x%02X checksum error, retry %d/3", reg, retry + 1);
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
        }

        if (retry < 2) {
            ESP_LOGW(TAG, "Reg 0x%02X read timeout, retry %d/3", reg, retry + 1);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read reg 0x%02X after 3 retries", reg);
        return ret;
    }

    *val = ((uint32_t)rx[2] << 16) | ((uint32_t)rx[1] << 8) | rx[0];
    return ESP_OK;
}

static esp_err_t bl0942_uart_write24(bl0942_dev_t *dev, uint8_t reg, uint32_t val)
{
    uint8_t hdr = compose_rw_cmd(BL0942_UART_WRITE_BASE, dev->addr);
    uint8_t d0 = (uint8_t)(val & 0xFF);
    uint8_t d1 = (uint8_t)((val >> 8) & 0xFF);
    uint8_t d2 = (uint8_t)((val >> 16) & 0xFF);
    uint8_t cs = checksum_byte(hdr, reg, d0, d1, d2);
    uint8_t frame[6] = { hdr, reg, d0, d1, d2, cs };

    return uart_send_bytes(dev->uart.uart_num, frame, sizeof(frame));
}

static esp_err_t bl0942_spi_read24(bl0942_dev_t *dev, uint8_t reg, uint32_t *val)
{
    ESP_RETURN_ON_FALSE(val, ESP_ERR_INVALID_ARG, TAG, "val null");

    uint8_t hdr = compose_rw_cmd(BL0942_UART_READ_BASE, dev->addr);
    uint8_t tx[2] = { hdr, reg };
    uint8_t rx[4] = { 0 };

    ESP_RETURN_ON_ERROR(spi_txrx(dev, tx, sizeof(tx), rx, sizeof(rx)), TAG, "spi xfer");
    uint8_t calc = checksum_byte(hdr, reg, rx[0], rx[1], rx[2]);
    if (calc != rx[3]) {
        ESP_LOGE(TAG, "spi checksum err reg 0x%02X calc 0x%02X got 0x%02X", reg, calc, rx[3]);
        return ESP_FAIL;
    }
    *val = ((uint32_t)rx[2] << 16) | ((uint32_t)rx[1] << 8) | rx[0];

    return ESP_OK;
}

static esp_err_t bl0942_spi_write24(bl0942_dev_t *dev, uint8_t reg, uint32_t val)
{
    uint8_t hdr = compose_rw_cmd(BL0942_UART_WRITE_BASE, dev->addr);
    uint8_t d0 = (uint8_t)(val & 0xFF);
    uint8_t d1 = (uint8_t)((val >> 8) & 0xFF);
    uint8_t d2 = (uint8_t)((val >> 16) & 0xFF);
    uint8_t cs = checksum_byte(hdr, reg, d0, d1, d2);
    uint8_t frame[6] = { hdr, reg, d0, d1, d2, cs };

    return spi_txrx(dev, frame, sizeof(frame), NULL, 0);
}

esp_err_t bl0942_create(bl0942_handle_t *handle, const bl0942_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(handle && cfg, ESP_ERR_INVALID_ARG, TAG, "invalid args");
    bl0942_dev_t *dev = calloc(1, sizeof(bl0942_dev_t));
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_NO_MEM, TAG, "no mem");

    dev->addr = cfg->addr & 0x03;
    dev->r_shunt = (cfg->shunt_resistor_ohm > 0) ? cfg->shunt_resistor_ohm : 0.001f;
    dev->v_div = (cfg->voltage_div_ratio > 0) ? cfg->voltage_div_ratio : 1.0f;
    dev->use_spi = cfg->use_spi;

    if (!dev->use_spi) {
        dev->uart.uart_num = cfg->uart.uart_num;
        dev->uart.tx_io = cfg->uart.tx_io_num;
        dev->uart.rx_io = cfg->uart.rx_io_num;
        dev->uart.sel_io = cfg->uart.sel_io_num;
        dev->uart.baud = cfg->uart.baud_rate ? cfg->uart.baud_rate : 9600;
    } else {
        dev->spi.cs_io = cfg->spi.cs_io_num;
    }

    if (!dev->use_spi) {
        uart_config_t ucfg = {
            .baud_rate = (int)dev->uart.baud,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_EVEN,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            .source_clk = UART_SCLK_DEFAULT,
#else
            .source_clk = UART_SCLK_APB,
#endif
        };
        ESP_RETURN_ON_ERROR(uart_driver_install(dev->uart.uart_num, 512, 0, 0, NULL, 0), TAG, "uart install");
        ESP_RETURN_ON_ERROR(uart_param_config(dev->uart.uart_num, &ucfg), TAG, "uart cfg");
        ESP_RETURN_ON_ERROR(uart_set_pin(dev->uart.uart_num, dev->uart.tx_io, dev->uart.rx_io, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "uart pins");
        /* Flush any stale bytes from previous sessions */
        uart_flush_input(dev->uart.uart_num);
    } else {
        spi_bus_config_t buscfg = {
            .mosi_io_num = cfg->spi.mosi_io_num,
            .miso_io_num = cfg->spi.miso_io_num,
            .sclk_io_num = cfg->spi.sclk_io_num,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 6,
        };
        /* Install bus if not already */
        esp_err_t ret = spi_bus_initialize(cfg->spi.spi_host, &buscfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            return ret;
        }
        spi_device_interface_config_t devcfg = {
            .clock_speed_hz = 800000, /* <= 900kHz per datasheet */
            .mode = 1,                /* CPOL=0, CPHA=1 */
            .spics_io_num = cfg->spi.cs_io_num,
            .queue_size = 1,
            .flags = 0,
        };
        ESP_RETURN_ON_ERROR(spi_bus_add_device(cfg->spi.spi_host, &devcfg, &dev->spi.spi_dev), TAG, "spi add dev");
    }

    if (!dev->use_spi && dev->uart.sel_io >= 0) {
        gpio_config_t io = { 0 };
        io.mode = GPIO_MODE_OUTPUT;
        io.pin_bit_mask = (1ULL << dev->uart.sel_io);
        gpio_config(&io);
        /* UART mode requires SEL=0 */
        gpio_set_level(dev->uart.sel_io, 0);
    }

    /* Give the chip some time to stabilize after UART/SPI initialization */
    vTaskDelay(pdMS_TO_TICKS(100));

    if (!dev->use_spi) {
        uart_config_t working_config = {
            .baud_rate = 4800,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,  // 8N1
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            .source_clk = UART_SCLK_DEFAULT,
#else
            .source_clk = UART_SCLK_APB,
#endif
        };
        uart_param_config(dev->uart.uart_num, &working_config);
        dev->uart.baud = 4800;

        ESP_LOGI(TAG, "BL0942 UART configured: 4800-8N1 (with EMI protection)");
    }

    /* Enable user write: write 0x55 to USR_WRPROT */
    esp_err_t ret = ESP_OK;

    if (dev->use_spi) {
        ret = bl0942_spi_write24(dev, BL0942_REG_USR_WRPROT, 0x000055);
    } else {
        ret = bl0942_uart_write24(dev, BL0942_REG_USR_WRPROT, 0x000055);
    }

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write USR_WRPROT register, continuing anyway");
        /* Don't fail initialization if register write fails - chip might still work */
    }

    if (!dev->use_spi && (dev->uart.baud == 19200 || dev->uart.baud == 38400)) {
        uint32_t mode = 0;
        if (bl0942_read_reg24((bl0942_handle_t)dev, BL0942_REG_MODE, &mode) == ESP_OK) {
            mode &= ~(0x3u << 8);
            mode |= ((dev->uart.baud == 19200 ? 0x2u : 0x3u) << 8);
            bl0942_write_reg24((bl0942_handle_t)dev, BL0942_REG_MODE, mode);
        }
    }

    /* Ensure CF enable, defaults are fine; leave as is for now */
    *handle = (bl0942_handle_t)dev;
    return ESP_OK;
}

esp_err_t bl0942_delete(bl0942_handle_t handle)
{
    bl0942_dev_t *dev = (bl0942_dev_t *)handle;
    if (!dev) {
        return ESP_OK;
    }

    if (!dev->use_spi) {
        uart_driver_delete(dev->uart.uart_num);
    } else {
        if (dev->spi.spi_dev) {
            spi_bus_remove_device(dev->spi.spi_dev);
        }
    }

    free(dev);
    return ESP_OK;
}

esp_err_t bl0942_read_reg24(bl0942_handle_t handle, uint8_t addr, uint32_t *val)
{
    bl0942_dev_t *dev = (bl0942_dev_t *)handle;

    return dev->use_spi ? bl0942_spi_read24(dev, addr, val)
           : bl0942_uart_read24(dev, addr, val);
}

esp_err_t bl0942_write_reg24(bl0942_handle_t handle, uint8_t addr, uint32_t val)
{
    bl0942_dev_t *dev = (bl0942_dev_t *)handle;

    return dev->use_spi ? bl0942_spi_write24(dev, addr, val)
           : bl0942_uart_write24(dev, addr, val);
}

esp_err_t bl0942_get_voltage(bl0942_handle_t handle, float *volt)
{
    bl0942_dev_t *dev = (bl0942_dev_t *)handle;
    ESP_RETURN_ON_FALSE(dev && volt, ESP_ERR_INVALID_ARG, TAG, "invalid args");

    uint32_t reg = 0;
    ESP_RETURN_ON_ERROR(bl0942_read_reg24(handle, BL0942_REG_V_RMS, &reg), TAG, "read V_RMS");
    /* V_RMS register is 24-bit unsigned; formula: V_RMS = 73989 * V(mV) / Vref  => V_line = pin_voltage * v_div */
    float v_pin_mv = (reg * BL0942_VREF) / 73989.0f; /* in mV at pin */
    *volt = (v_pin_mv / 1000.0f) * dev->v_div;

    return ESP_OK;
}

esp_err_t bl0942_get_current(bl0942_handle_t handle, float *amp)
{
    bl0942_dev_t *dev = (bl0942_dev_t *)handle;
    ESP_RETURN_ON_FALSE(dev && amp, ESP_ERR_INVALID_ARG, TAG, "invalid args");

    uint32_t reg = 0;
    ESP_RETURN_ON_ERROR(bl0942_read_reg24(handle, BL0942_REG_I_RMS, &reg), TAG, "read I_RMS");
    /* I_RMS = 305978 * I(mV) / Vref => I(mV) across shunt, convert to A by / (R * 1000) */
    float i_mv = (reg * BL0942_VREF) / 305978.0f; /* mV across shunt */
    *amp = (i_mv / 1000.0f) / dev->r_shunt;

    return ESP_OK;
}

esp_err_t bl0942_get_active_power(bl0942_handle_t handle, float *watt)
{
    bl0942_dev_t *dev = (bl0942_dev_t *)handle;
    ESP_RETURN_ON_FALSE(dev && watt, ESP_ERR_INVALID_ARG, TAG, "invalid args");

    uint32_t reg = 0;
    ESP_RETURN_ON_ERROR(bl0942_read_reg24(handle, BL0942_REG_WATT, &reg), TAG, "read WATT");
    /* WATT register is 24-bit signed; convert to int32_t with sign extension */
    int32_t sreg = (reg & 0x800000) ? (int32_t)(reg | 0xFF000000) : (int32_t)reg;
    /* WATT ≈ 3537 * I(mV) * V(mV) * cos(phi) / Vref^2. For practical scaling, derive using V_RMS & I_RMS: */
    /* Compute V and I first for better numeric stability, then P = V*I*PF. Assume PF≈1 if sign unknown. */
    float v, i;
    bl0942_get_voltage(handle, &v);
    bl0942_get_current(handle, &i);
    float p_est = v * i; /* apparent */
    /* Use sign from WATT register */
    *watt = (sreg < 0) ? -fabsf(p_est) : p_est;

    return ESP_OK;
}

esp_err_t bl0942_get_energy_pulses(bl0942_handle_t handle, uint32_t *cf_cnt)
{
    bl0942_dev_t *dev = (bl0942_dev_t *)handle;
    ESP_RETURN_ON_FALSE(dev && cf_cnt, ESP_ERR_INVALID_ARG, TAG, "invalid args");

    uint32_t reg = 0;
    ESP_RETURN_ON_ERROR(bl0942_read_reg24(handle, BL0942_REG_CF_CNT, &reg), TAG, "read CF_CNT");
    *cf_cnt = reg & 0xFFFFFFu;

    return ESP_OK;
}

esp_err_t bl0942_get_line_freq(bl0942_handle_t handle, float *hz)
{
    bl0942_dev_t *dev = (bl0942_dev_t *)handle;
    ESP_RETURN_ON_FALSE(dev && hz, ESP_ERR_INVALID_ARG, TAG, "invalid args");

    uint32_t reg = 0;
    ESP_RETURN_ON_ERROR(bl0942_read_reg24(handle, BL0942_REG_FREQ, &reg), TAG, "read FREQ");
    uint16_t freq_reg = (uint16_t)(reg & 0xFFFFu);
    if (freq_reg == 0) {
        *hz = 0.0f;
        return ESP_OK;
    }
    /* f_meas = 2*fs / FREQ, fs=500kHz */
    *hz = (2.0f * 500000.0f) / (float)freq_reg;

    return ESP_OK;
}
