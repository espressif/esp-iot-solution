/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <string.h>
#include <sys/errno.h>

#include "esp_vfs.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* ESP-IDF >= 5.3.0 uses new I2C driver architecture */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#if SOC_I2C_SUPPORT_SLAVE
#include "driver/i2c_slave.h"
#endif
/* Prefer numeric cast: some targets omit I2C_ADDR_BIT_LEN_10 enum member. */
#if defined(SOC_I2C_SUPPORT_10BIT_ADDR) && SOC_I2C_SUPPORT_10BIT_ADDR
#define EXT_VFS_I2C_10BIT_OK 1
#else
#define EXT_VFS_I2C_10BIT_OK 0
#endif
/*
 * Slave API:
 * - IDF 5.3–5.5 with CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2, and IDF >= 6.0: v2
 *   (i2c_slave_write + on_receive with length, receive_buf_depth required)
 * - Otherwise: slave config (I2CIOCSCFG) and transfer return ENOTSUP. New-driver
 *   v1 on_recv_done has no RX length (short writes would false-succeed); do not
 *   mix legacy driver/i2c with the 5.3+ master API in the same build.
 */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#define EXT_VFS_I2C_SLAVE_API_V2 1
#elif defined(CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2) && CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2
#define EXT_VFS_I2C_SLAVE_API_V2 1
#else
#define EXT_VFS_I2C_SLAVE_API_V2 0
#endif
/*
 * Custom transaction ops (i2c_operation_job_t / I2C_MASTER_CMD_* /
 * I2C_DEVICE_ADDRESS_NOT_USED / i2c_master_execute_defined_operations):
 *   5.3.x: >= 5.3.4
 *   5.4.x: >= 5.4.1  (5.4.0 does not have these APIs)
 *   5.5+:  all releases; IDF >= 6.0 likewise
 * A plain ">= 5.3.4" check wrongly enables them on 5.4.0.
 */
#if (ESP_IDF_VERSION_MAJOR == 5 && ESP_IDF_VERSION_MINOR == 3 && ESP_IDF_VERSION_PATCH >= 4) || \
    (ESP_IDF_VERSION_MAJOR == 5 && ESP_IDF_VERSION_MINOR == 4 && ESP_IDF_VERSION_PATCH >= 1) || \
    (ESP_IDF_VERSION_MAJOR == 5 && ESP_IDF_VERSION_MINOR >= 5) || \
    (ESP_IDF_VERSION_MAJOR >= 6)
#define EXT_VFS_I2C_CUSTOM_OPS 1
#else
#define EXT_VFS_I2C_CUSTOM_OPS 0
#endif
#else
#include "driver/i2c.h"
#endif

#include "ext_vfs_export.h"

#include "ioctl/esp_i2c_ioctl.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#if SOC_I2C_SUPPORT_SLAVE
#define I2C_SLAVE_VFS
#endif
#else
/* Pre-5.3: SOC_I2C_SUPPORT_SLAVE may be absent; keep legacy exclusion of C2. */
#ifndef CONFIG_IDF_TARGET_ESP32C2
#define I2C_SLAVE_VFS
#endif
#endif

#define I2C_PORT_NUM                I2C_NUM_MAX
#define I2C_TRANSFER_TIMEOUT_MS     1000
#define I2C_TRANSFER_TIMEOUT_TICKS  pdMS_TO_TICKS(I2C_TRANSFER_TIMEOUT_MS)
#define I2C_SLAVE_BUF_DEPTH         256

typedef struct i2c_stat {
    uint32_t opened :  1;  /*!< 1: I2C device is open, 0: I2C device is not open */
    uint32_t master :  1;  /*!< 1: I2C master mode, 0: slave mode */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    uint32_t addr_10bit : 1; /*!< 1: 10-bit address, 0: 7-bit address */
    uint32_t ack_check : 1;  /*!< Cached I2C_MSG_CHECK_ACK for current master device */
    uint32_t raw_dev : 1;    /*!< 1: device_address is I2C_DEVICE_ADDRESS_NOT_USED (custom ops) */
    /*
     * close() failed while VFS already dropped the FD. Keep handles and allow
     * the next open() of this port to retry cleanup before reusing the port.
     */
    uint32_t cleanup_pending : 1;
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
#ifdef I2C_SLAVE_VFS
#if EXT_VFS_I2C_SLAVE_API_V2
    i2c_slave_dev_handle_t slave_handle;
    SemaphoreHandle_t slave_rx_sem;
    uint8_t slave_rx_hold[I2C_SLAVE_BUF_DEPTH]; /*!< Ring of retained RX bytes */
    size_t slave_rx_rd;                         /*!< Next byte to consume */
    size_t slave_rx_wr;                         /*!< Next byte to append */
    size_t slave_rx_hold_len;
    uint32_t slave_rx_overflow : 1; /*!< Hold dropped RX bytes; report on next read */
#endif
#endif
    uint16_t slave_addr;
    uint32_t scl_speed_hz;
#endif
} i2c_stat_t;

static const char *TAG = "ext_vfs_i2c";

static i2c_stat_t i2c_stat[I2C_PORT_NUM];

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
/*
 * Serialize open / close / ioctl per port so cleanup_pending recovery cannot
 * race two tasks through the same bus/device handles (double-free / UAF).
 * Kept outside i2c_stat[] so open()'s memset does not destroy the mutex.
 */
static SemaphoreHandle_t s_i2c_port_mtx[I2C_PORT_NUM];

static void i2c_port_locks_init(void)
{
    for (int i = 0; i < I2C_PORT_NUM; i++) {
        if (s_i2c_port_mtx[i] == NULL) {
            s_i2c_port_mtx[i] = xSemaphoreCreateMutex();
        }
    }
}

static bool i2c_port_lock(int port)
{
    if (s_i2c_port_mtx[port] == NULL) {
        i2c_port_locks_init();
    }
    if (s_i2c_port_mtx[port] == NULL) {
        return false;
    }
    return xSemaphoreTake(s_i2c_port_mtx[port], portMAX_DELAY) == pdTRUE;
}

static void i2c_port_unlock(int port)
{
    if (s_i2c_port_mtx[port]) {
        xSemaphoreGive(s_i2c_port_mtx[port]);
    }
}
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)

#if defined(I2C_SLAVE_VFS) && EXT_VFS_I2C_SLAVE_API_V2
static portMUX_TYPE s_slave_rx_mux = portMUX_INITIALIZER_UNLOCKED;

static void slave_rx_hold_reset(i2c_stat_t *stat)
{
    stat->slave_rx_rd = 0;
    stat->slave_rx_wr = 0;
    stat->slave_rx_hold_len = 0;
    stat->slave_rx_overflow = 0;
}

/* Copy up to `want` bytes from the ring without moving in-flight append region. */
static size_t slave_rx_hold_take(i2c_stat_t *stat, uint8_t *dst, size_t want)
{
    size_t n;

    portENTER_CRITICAL(&s_slave_rx_mux);
    n = want;
    if (n > stat->slave_rx_hold_len) {
        n = stat->slave_rx_hold_len;
    }
    for (size_t i = 0; i < n; i++) {
        dst[i] = stat->slave_rx_hold[stat->slave_rx_rd];
        stat->slave_rx_rd = (stat->slave_rx_rd + 1) % I2C_SLAVE_BUF_DEPTH;
    }
    stat->slave_rx_hold_len -= n;
    if (stat->slave_rx_hold_len == 0) {
        stat->slave_rx_rd = 0;
        stat->slave_rx_wr = 0;
    }
    portEXIT_CRITICAL(&s_slave_rx_mux);
    return n;
}

static size_t slave_rx_hold_available(i2c_stat_t *stat)
{
    size_t n;

    portENTER_CRITICAL(&s_slave_rx_mux);
    n = stat->slave_rx_hold_len;
    portEXIT_CRITICAL(&s_slave_rx_mux);
    return n;
}

/* Push bytes back to the read side of the ring (timeout unwind). */
static void slave_rx_hold_prepend(i2c_stat_t *stat, const uint8_t *src, size_t n)
{
    portENTER_CRITICAL(&s_slave_rx_mux);
    if (n > I2C_SLAVE_BUF_DEPTH) {
        n = I2C_SLAVE_BUF_DEPTH;
        stat->slave_rx_overflow = 1;
    }
    if (stat->slave_rx_hold_len + n > I2C_SLAVE_BUF_DEPTH) {
        size_t drop = stat->slave_rx_hold_len + n - I2C_SLAVE_BUF_DEPTH;
        /* Drop newest retained bytes (write side) to make room. */
        stat->slave_rx_wr = (stat->slave_rx_wr + I2C_SLAVE_BUF_DEPTH - drop) % I2C_SLAVE_BUF_DEPTH;
        stat->slave_rx_hold_len -= drop;
        stat->slave_rx_overflow = 1;
    }
    stat->slave_rx_rd = (stat->slave_rx_rd + I2C_SLAVE_BUF_DEPTH - n) % I2C_SLAVE_BUF_DEPTH;
    for (size_t i = 0; i < n; i++) {
        stat->slave_rx_hold[(stat->slave_rx_rd + i) % I2C_SLAVE_BUF_DEPTH] = src[i];
    }
    stat->slave_rx_hold_len += n;
    portEXIT_CRITICAL(&s_slave_rx_mux);
}

static bool slave_rx_overflow_take(i2c_stat_t *stat)
{
    bool overflow;

    portENTER_CRITICAL(&s_slave_rx_mux);
    overflow = stat->slave_rx_overflow != 0;
    stat->slave_rx_overflow = 0;
    portEXIT_CRITICAL(&s_slave_rx_mux);
    return overflow;
}

static bool IRAM_ATTR slave_rx_done_cb(i2c_slave_dev_handle_t i2c_slave,
                                       const i2c_slave_rx_done_event_data_t *evt_data,
                                       void *arg)
{
    i2c_stat_t *stat = (i2c_stat_t *)arg;
    BaseType_t hp_task_woken = pdFALSE;

    (void)i2c_slave;
    /* Always retain RX data, even when no ioctl read is waiting. */
    if (evt_data && evt_data->buffer && evt_data->length) {
        portENTER_CRITICAL_ISR(&s_slave_rx_mux);
        size_t space = I2C_SLAVE_BUF_DEPTH - stat->slave_rx_hold_len;
        size_t n = evt_data->length;
        if (n > space) {
            /* Record overflow so the next read can report data loss. */
            stat->slave_rx_overflow = 1;
            n = space;
        }
        for (size_t i = 0; i < n; i++) {
            stat->slave_rx_hold[stat->slave_rx_wr] = evt_data->buffer[i];
            stat->slave_rx_wr = (stat->slave_rx_wr + 1) % I2C_SLAVE_BUF_DEPTH;
        }
        stat->slave_rx_hold_len += n;
        portEXIT_CRITICAL_ISR(&s_slave_rx_mux);
    }
    if (stat->slave_rx_sem) {
        xSemaphoreGiveFromISR(stat->slave_rx_sem, &hp_task_woken);
    }
    return hp_task_woken == pdTRUE;
}
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
/*
 * New I2C driver only exposes one enable_internal_pullup for both pins.
 * After the driver configures pins, restore public per-pin I2C_SDA_PULLUP /
 * I2C_SCL_PULLUP semantics.
 */
static void i2c_apply_vfs_pullups(int sda_pin, int scl_pin, uint32_t flags)
{
    if (flags & I2C_SDA_PULLUP) {
        gpio_pullup_en(sda_pin);
    } else {
        gpio_pullup_dis(sda_pin);
    }
    if (flags & I2C_SCL_PULLUP) {
        gpio_pullup_en(scl_pin);
    } else {
        gpio_pullup_dis(scl_pin);
    }
}
#endif

static int config_i2c_driver(int port, const i2c_cfg_t *cfg)
{
    esp_err_t err;

    if ((cfg->flags & I2C_ADDR_10BIT) && !EXT_VFS_I2C_10BIT_OK) {
        errno = EINVAL;
        return -1;
    }

    if (cfg->flags & I2C_MASTER) {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = port,
            .sda_io_num = cfg->sda_pin,
            .scl_io_num = cfg->scl_pin,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = (cfg->flags & (I2C_SDA_PULLUP | I2C_SCL_PULLUP)) ? true : false,
        };

        err = i2c_new_master_bus(&bus_config, &i2c_stat[port].bus_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
            errno = EIO;
            return -1;
        }
        i2c_apply_vfs_pullups(cfg->sda_pin, cfg->scl_pin, cfg->flags);

        i2c_stat[port].master = 1;
        i2c_stat[port].addr_10bit = (cfg->flags & I2C_ADDR_10BIT) ? 1 : 0;
        i2c_stat[port].scl_speed_hz = cfg->master.clock;
        i2c_stat[port].dev_handle = NULL;
        i2c_stat[port].slave_addr = 0;
        i2c_stat[port].ack_check = 0;
        i2c_stat[port].raw_dev = 0;
#ifdef I2C_SLAVE_VFS
#if EXT_VFS_I2C_SLAVE_API_V2
        i2c_stat[port].slave_handle = NULL;
#endif
#endif
    } else {
#ifdef I2C_SLAVE_VFS
#if EXT_VFS_I2C_SLAVE_API_V2
        i2c_slave_config_t slave_config = {
            .i2c_port = port,
            .sda_io_num = cfg->sda_pin,
            .scl_io_num = cfg->scl_pin,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .slave_addr = cfg->slave.addr,
            .addr_bit_len = (cfg->flags & I2C_ADDR_10BIT) ? ((i2c_addr_bit_len_t)1) : I2C_ADDR_BIT_LEN_7,
            .send_buf_depth = I2C_SLAVE_BUF_DEPTH,
            .receive_buf_depth = I2C_SLAVE_BUF_DEPTH,
            .flags.enable_internal_pullup =
            (cfg->flags & (I2C_SDA_PULLUP | I2C_SCL_PULLUP)) ? true : false,
        };

        err = i2c_new_slave_device(&slave_config, &i2c_stat[port].slave_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2c_new_slave_device failed: %s", esp_err_to_name(err));
            errno = EIO;
            return -1;
        }
        i2c_apply_vfs_pullups(cfg->sda_pin, cfg->scl_pin, cfg->flags);

        i2c_stat[port].master = 0;
        i2c_stat[port].addr_10bit = (cfg->flags & I2C_ADDR_10BIT) ? 1 : 0;
        if (!i2c_stat[port].slave_rx_sem) {
            i2c_stat[port].slave_rx_sem = xSemaphoreCreateBinary();
            if (!i2c_stat[port].slave_rx_sem) {
                i2c_del_slave_device(i2c_stat[port].slave_handle);
                i2c_stat[port].slave_handle = NULL;
                errno = ENOMEM;
                return -1;
            }
        } else {
            xSemaphoreTake(i2c_stat[port].slave_rx_sem, 0);
        }

        slave_rx_hold_reset(&i2c_stat[port]);

        i2c_slave_event_callbacks_t cbs = {};
        cbs.on_receive = slave_rx_done_cb;
        err = i2c_slave_register_event_callbacks(i2c_stat[port].slave_handle, &cbs, &i2c_stat[port]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2c_slave_register_event_callbacks failed: %s", esp_err_to_name(err));
            i2c_del_slave_device(i2c_stat[port].slave_handle);
            i2c_stat[port].slave_handle = NULL;
            errno = EIO;
            return -1;
        }
#else
        /*
         * New-driver v1 cannot report RX length; do not fall back to legacy
         * driver/i2c alongside the 5.3+ master API.
         */
        ESP_LOGE(TAG, "I2C slave requires slave driver v2 "
                 "(CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2 or IDF >= 6.0)");
        errno = ENOTSUP;
        return -1;
#endif
#else
        errno = EINVAL;
        return -1;
#endif
    }

    return 0;
}

static int i2c_master_ensure_device(int port, uint16_t addr, bool want_ack_check, bool raw_dev)
{
    esp_err_t err;

    if (i2c_stat[port].dev_handle &&
            i2c_stat[port].raw_dev == (raw_dev ? 1 : 0) &&
            i2c_stat[port].ack_check == want_ack_check &&
            (raw_dev || i2c_stat[port].slave_addr == addr)) {
        return 0;
    }

    uint16_t device_address = addr;
#if EXT_VFS_I2C_CUSTOM_OPS
    if (raw_dev) {
        device_address = I2C_DEVICE_ADDRESS_NOT_USED;
    }
#else
    (void)raw_dev;
#endif

    i2c_device_config_t dev_config = {
        .dev_addr_length = i2c_stat[port].addr_10bit ? ((i2c_addr_bit_len_t)1) : I2C_ADDR_BIT_LEN_7,
        .device_address = device_address,
        .scl_speed_hz = i2c_stat[port].scl_speed_hz ? i2c_stat[port].scl_speed_hz : 100000,
        .flags.disable_ack_check = !want_ack_check,
    };

    if (i2c_stat[port].dev_handle) {
        err = i2c_master_bus_rm_device(i2c_stat[port].dev_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2c_master_bus_rm_device failed: %s", esp_err_to_name(err));
            errno = EIO;
            return -1;
        }
        i2c_stat[port].dev_handle = NULL;
    }

    err = i2c_master_bus_add_device(i2c_stat[port].bus_handle, &dev_config, &i2c_stat[port].dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
        errno = EIO;
        return -1;
    }

#if EXT_VFS_I2C_CUSTOM_OPS
    i2c_stat[port].slave_addr = raw_dev ? 0 : addr;
    i2c_stat[port].raw_dev = raw_dev ? 1 : 0;
#else
    i2c_stat[port].slave_addr = addr;
    i2c_stat[port].raw_dev = 0;
#endif
    i2c_stat[port].ack_check = want_ack_check;
    return 0;
}

static int i2c_master_transfer(int port, const i2c_msg_t *msg)
{
    esp_err_t err;
    const bool want_ack_check = (msg->flags & I2C_MSG_CHECK_ACK) != 0;
    /* Zero-length writes still need an address byte (probe); transmit() rejects size 0. */
    const bool custom_boundary = (msg->flags & (I2C_MSG_NO_START | I2C_MSG_NO_END)) != 0 ||
                                 ((msg->flags & I2C_MSG_WRITE) != 0 && msg->size == 0);

    if (custom_boundary) {
#if EXT_VFS_I2C_CUSTOM_OPS
        /*
         * Match legacy cmd-link semantics: optional START, address phase,
         * payload R/W, optional STOP. Use NOT_USED device so the driver does not
         * inject a second address on START.
         */
        const bool is_write = (msg->flags & I2C_MSG_WRITE) != 0;
        const bool addr_10bit = i2c_stat[port].addr_10bit != 0;

        /* 10-bit addressing needs a START/restart framing we cannot invent mid-stream. */
        if (addr_10bit && (msg->flags & I2C_MSG_NO_START)) {
            errno = ENOTSUP;
            return -1;
        }

        if (i2c_master_ensure_device(port, msg->addr, want_ack_check, true) != 0) {
            return -1;
        }

        uint8_t addr_7bit;
        uint8_t addr_10bit_wr[2];
        uint8_t addr_10bit_rd;
        /* START + (10-bit: WR2 + START + RD1) + (ACK read) + NACK read + STOP */
        i2c_operation_job_t ops[7];
        size_t n = 0;

        memset(ops, 0, sizeof(ops));
        if (!(msg->flags & I2C_MSG_NO_START)) {
            ops[n++].command = I2C_MASTER_CMD_START;
        }

        if (addr_10bit) {
            uint8_t high = (uint8_t)(((msg->addr >> 8) & 0x03) | 0x78);
            addr_10bit_wr[0] = (uint8_t)((high << 1) | 0);
            addr_10bit_wr[1] = (uint8_t)(msg->addr & 0xff);
            addr_10bit_rd = (uint8_t)((high << 1) | 1);

            if (is_write) {
                ops[n].command = I2C_MASTER_CMD_WRITE;
                ops[n].write.ack_check = want_ack_check;
                ops[n].write.data = addr_10bit_wr;
                ops[n].write.total_bytes = 2;
                n++;
            } else {
                /* Spec/IDF: write-form header, repeated START, then read-form header. */
                ops[n].command = I2C_MASTER_CMD_WRITE;
                ops[n].write.ack_check = want_ack_check;
                ops[n].write.data = addr_10bit_wr;
                ops[n].write.total_bytes = 2;
                n++;
                ops[n++].command = I2C_MASTER_CMD_START;
                ops[n].command = I2C_MASTER_CMD_WRITE;
                ops[n].write.ack_check = want_ack_check;
                ops[n].write.data = &addr_10bit_rd;
                ops[n].write.total_bytes = 1;
                n++;
            }
        } else {
            addr_7bit = (uint8_t)((msg->addr << 1) | (is_write ? 0 : 1));
            ops[n].command = I2C_MASTER_CMD_WRITE;
            ops[n].write.ack_check = want_ack_check;
            ops[n].write.data = &addr_7bit;
            ops[n].write.total_bytes = 1;
            n++;
        }

        if (is_write) {
            if (msg->size) {
                ops[n].command = I2C_MASTER_CMD_WRITE;
                ops[n].write.ack_check = want_ack_check;
                ops[n].write.data = msg->buffer;
                ops[n].write.total_bytes = msg->size;
                n++;
            }
        } else if (msg->size > 0) {
            /* Match legacy I2C_MASTER_LAST_NACK / i2c_master_receive(): ACK then NACK. */
            if (msg->size > 1) {
                ops[n].command = I2C_MASTER_CMD_READ;
                ops[n].read.ack_value = I2C_ACK_VAL;
                ops[n].read.data = msg->buffer;
                ops[n].read.total_bytes = msg->size - 1;
                n++;
            }
            ops[n].command = I2C_MASTER_CMD_READ;
            ops[n].read.ack_value = I2C_NACK_VAL;
            ops[n].read.data = msg->buffer + (msg->size - 1);
            ops[n].read.total_bytes = 1;
            n++;
        }
        if (!(msg->flags & I2C_MSG_NO_END)) {
            ops[n++].command = I2C_MASTER_CMD_STOP;
        }

        err = i2c_master_execute_defined_operations(i2c_stat[port].dev_handle, ops, n,
                                                    I2C_TRANSFER_TIMEOUT_MS);
#else
        /* Custom ops API unavailable on this IDF patch; see CHANGELOG. */
        errno = ENOTSUP;
        return -1;
#endif
    } else {
        if (i2c_master_ensure_device(port, msg->addr, want_ack_check, false) != 0) {
            return -1;
        }

        if (msg->flags & I2C_MSG_WRITE) {
            err = i2c_master_transmit(i2c_stat[port].dev_handle, msg->buffer, msg->size,
                                      I2C_TRANSFER_TIMEOUT_MS);
        } else {
            err = i2c_master_receive(i2c_stat[port].dev_handle, msg->buffer, msg->size,
                                     I2C_TRANSFER_TIMEOUT_MS);
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C transfer failed: %s", esp_err_to_name(err));
        errno = EIO;
        return -1;
    }

    return 0;
}

#ifdef I2C_SLAVE_VFS
static int i2c_slave_transfer(int port, const i2c_msg_t *msg)
{
    esp_err_t err;
    i2c_stat_t *stat = &i2c_stat[port];

#if !EXT_VFS_I2C_SLAVE_API_V2
    (void)port;
    (void)err;
    (void)stat;
    (void)msg;
    ESP_LOGE(TAG, "I2C slave requires slave driver v2 "
             "(CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2 or IDF >= 6.0)");
    errno = ENOTSUP;
    return -1;
#else
    if (msg->flags & I2C_MSG_WRITE) {
        uint32_t write_len = 0;
        err = i2c_slave_write(stat->slave_handle, msg->buffer, msg->size, &write_len, I2C_TRANSFER_TIMEOUT_MS);
        if (err == ESP_OK && write_len != msg->size) {
            err = ESP_ERR_TIMEOUT;
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2C slave write failed: %s", esp_err_to_name(err));
            errno = (err == ESP_ERR_TIMEOUT) ? ETIMEDOUT : EIO;
            return -1;
        }
        return 0;
    }

    if (msg->size == 0) {
        return 0;
    }
    /* Cap to hold depth so timeout/overflow can restore every consumed byte. */
    if (msg->size > I2C_SLAVE_BUF_DEPTH) {
        errno = EINVAL;
        return -1;
    }

    /* Drain stale notifies; data already sits in the hold ring. */
    while (xSemaphoreTake(stat->slave_rx_sem, 0) == pdTRUE) {
    }

    /* Consume from hold; wait for more when empty.
     * Ring take only advances rd, so ISR appends at wr stay valid.
     * Report pending overflow even when hold still has bytes (stream gap).
     * Use FreeRTOS timeout state so TickType_t wraparound cannot falsely expire. */
    size_t got = 0;
    TimeOut_t timeout;
    TickType_t ticks_to_wait = I2C_TRANSFER_TIMEOUT_TICKS;

    vTaskSetTimeOutState(&timeout);

    while (got < msg->size) {
        if (slave_rx_overflow_take(stat)) {
            if (got) {
                slave_rx_hold_prepend(stat, msg->buffer, got);
            }
            ESP_LOGE(TAG, "I2C slave receive overflow");
            errno = EOVERFLOW;
            return -1;
        }
        if (slave_rx_hold_available(stat) == 0) {
            if (xTaskCheckForTimeOut(&timeout, &ticks_to_wait) != pdFALSE ||
                    xSemaphoreTake(stat->slave_rx_sem, ticks_to_wait) != pdTRUE) {
                if (got) {
                    slave_rx_hold_prepend(stat, msg->buffer, got);
                }
                ESP_LOGE(TAG, "I2C slave receive timeout (%u/%u)",
                         (unsigned)got, (unsigned)msg->size);
                errno = ETIMEDOUT;
                return -1;
            }
            continue;
        }
        got += slave_rx_hold_take(stat, msg->buffer + got, msg->size - got);
    }

    if (slave_rx_overflow_take(stat)) {
        slave_rx_hold_prepend(stat, msg->buffer, got);
        ESP_LOGE(TAG, "I2C slave receive overflow");
        errno = EOVERFLOW;
        return -1;
    }

    return 0;
#endif
}
#endif

#define I2C_CLOSE_RETRY       5
#define I2C_CLOSE_RETRY_TICKS pdMS_TO_TICKS(10)

/* Retry when the bus/device is transiently busy (common during in-flight xfer). */
static esp_err_t i2c_close_retry(esp_err_t (*fn)(void *), void *arg)
{
    esp_err_t err = ESP_OK;

    for (int i = 0; i < I2C_CLOSE_RETRY; i++) {
        err = fn(arg);
        if (err != ESP_ERR_INVALID_STATE) {
            return err;
        }
        vTaskDelay(I2C_CLOSE_RETRY_TICKS);
    }
    return err;
}

static esp_err_t i2c_rm_device_wrap(void *arg)
{
    return i2c_master_bus_rm_device((i2c_master_dev_handle_t)arg);
}

static esp_err_t i2c_del_bus_wrap(void *arg)
{
    return i2c_del_master_bus((i2c_master_bus_handle_t)arg);
}

#ifdef I2C_SLAVE_VFS
#if EXT_VFS_I2C_SLAVE_API_V2
static esp_err_t i2c_del_slave_wrap(void *arg)
{
    return i2c_del_slave_device((i2c_slave_dev_handle_t)arg);
}
#endif
#endif

static esp_err_t i2c_driver_close(int port)
{
    esp_err_t err = ESP_OK;
    esp_err_t first_err = ESP_OK;

    if (i2c_stat[port].dev_handle) {
        err = i2c_close_retry(i2c_rm_device_wrap, i2c_stat[port].dev_handle);
        if (err != ESP_OK) {
            if (first_err == ESP_OK) {
                first_err = err;
            }
            ESP_LOGW(TAG, "Failed to remove I2C device: %s", esp_err_to_name(err));
        } else {
            i2c_stat[port].dev_handle = NULL;
        }
    }

#ifdef I2C_SLAVE_VFS
#if EXT_VFS_I2C_SLAVE_API_V2
    if (i2c_stat[port].slave_handle) {
        err = i2c_close_retry(i2c_del_slave_wrap, i2c_stat[port].slave_handle);
        if (err != ESP_OK) {
            if (first_err == ESP_OK) {
                first_err = err;
            }
            ESP_LOGW(TAG, "Failed to delete slave device: %s", esp_err_to_name(err));
        } else {
            i2c_stat[port].slave_handle = NULL;
        }
    }
    /* Keep RX sync state while the slave device is still alive. */
    if (!i2c_stat[port].slave_handle) {
        if (i2c_stat[port].slave_rx_sem) {
            vSemaphoreDelete(i2c_stat[port].slave_rx_sem);
            i2c_stat[port].slave_rx_sem = NULL;
        }
        slave_rx_hold_reset(&i2c_stat[port]);
    }
#endif
#endif

    /*
     * IDF 5.3/5.4 i2c_del_master_bus() does not refuse a bus that still has
     * devices. Freeing the bus while rm_device failed leaves a dangling
     * dev_handle (UAF on the next cleanup retry). Only destroy the bus after
     * the device is gone; slave path never owns a master device on this bus.
     */
    if (i2c_stat[port].bus_handle && !i2c_stat[port].dev_handle) {
        err = i2c_close_retry(i2c_del_bus_wrap, i2c_stat[port].bus_handle);
        if (err != ESP_OK) {
            if (first_err == ESP_OK) {
                first_err = err;
            }
            ESP_LOGW(TAG, "Failed to delete master bus: %s", esp_err_to_name(err));
        } else {
            i2c_stat[port].bus_handle = NULL;
        }
    }

    return first_err;
}

#else  // ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)

static int config_i2c_driver_legacy(int port, const i2c_cfg_t *cfg)
{
    i2c_config_t conf = {};
    esp_err_t err;

    if (cfg->flags & I2C_MASTER) {
        conf.mode = I2C_MODE_MASTER;

        conf.master.clk_speed = cfg->master.clock;

        i2c_stat[port].master = 1;
    } else {
#ifdef I2C_SLAVE_VFS
        conf.mode = I2C_MODE_SLAVE;

        conf.slave.slave_addr    = cfg->slave.addr;
        conf.slave.maximum_speed = cfg->slave.max_clock;
        if (cfg->flags & I2C_ADDR_10BIT) {
            /* Reject only when the capability is explicitly unavailable; if the
             * macro is absent (older IDF / some targets), keep addr_10bit_en. */
#if defined(SOC_I2C_SUPPORT_10BIT_ADDR) && !SOC_I2C_SUPPORT_10BIT_ADDR
            errno = EINVAL;
            return -1;
#else
            conf.slave.addr_10bit_en = true;
#endif
        }

        i2c_stat[port].master = 0;
#else
        errno = EINVAL;
        return -1;
#endif
    }

    if (cfg->flags & I2C_SDA_PULLUP) {
        conf.sda_pullup_en = true;
    }

    if (cfg->flags & I2C_SCL_PULLUP) {
        conf.scl_pullup_en = true;
    }

    conf.sda_io_num = cfg->sda_pin;
    conf.scl_io_num = cfg->scl_pin;

    err = i2c_param_config(port, &conf);
    if (err != ESP_OK) {
        errno = EIO;
        return -1;
    }

    /* Slave mode: IDF requires slv_rx_buf_len > 100 || slv_tx_buf_len > 100.
     * Avoid I2C_MODE_SLAVE here — some targets (e.g. ESP32-C2) omit that enum. */
    size_t slv_rx = 0;
    size_t slv_tx = 0;
#ifdef I2C_SLAVE_VFS
    if (!i2c_stat[port].master) {
        slv_rx = I2C_SLAVE_BUF_DEPTH;
        slv_tx = I2C_SLAVE_BUF_DEPTH;
    }
#endif
    err = i2c_driver_install(port, conf.mode, slv_rx, slv_tx, 0);
    if (err != ESP_OK) {
        errno = EIO;
        return -1;
    }

    return 0;
}

static int i2c_master_transfer_legacy(int port, const i2c_msg_t *msg)
{
    esp_err_t err;
    i2c_cmd_handle_t cmd;
    uint8_t data;
    bool check_en;

    cmd = i2c_cmd_link_create();
    if (!cmd) {
        goto errout_create_cmdlink;
    }

    if (!(msg->flags & I2C_MSG_NO_START)) {
        err = i2c_master_start(cmd);
        if (err != ESP_OK) {
            goto errout_start_cmd;
        }
    }

    data  = msg->flags & I2C_MSG_WRITE ? I2C_MASTER_WRITE : I2C_MASTER_READ;
    data |= msg->addr << 1;
    check_en = msg->flags & I2C_MSG_CHECK_ACK ? true : false;
    err = i2c_master_write_byte(cmd, data, check_en);
    if (err != ESP_OK) {
        goto errout_start_cmd;
    }

    if (msg->flags & I2C_MSG_WRITE) {
        err = i2c_master_write(cmd, msg->buffer, msg->size, check_en);
        if (err != ESP_OK) {
            goto errout_start_cmd;
        }
    } else {
        err = i2c_master_read(cmd, msg->buffer, msg->size, I2C_MASTER_LAST_NACK);
        if (err != ESP_OK) {
            goto errout_start_cmd;
        }
    }

    if (!(msg->flags & I2C_MSG_NO_END)) {
        err = i2c_master_stop(cmd);
        if (err != ESP_OK) {
            goto errout_start_cmd;
        }
    }

    err = i2c_master_cmd_begin(port, cmd, I2C_TRANSFER_TIMEOUT_TICKS);
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        errno = EIO;
        return -1;
    }

    return 0;

errout_start_cmd:
    i2c_cmd_link_delete(cmd);
errout_create_cmdlink:
    errno = ENOMEM;
    return -1;
}

#ifdef I2C_SLAVE_VFS
static int i2c_slave_transfer_legacy(int port, const i2c_msg_t *msg)
{
    int ret;

    if (msg->flags & I2C_MSG_WRITE) {
        ret = i2c_slave_write_buffer(port, msg->buffer, msg->size, I2C_TRANSFER_TIMEOUT_TICKS);
    } else {
        ret = i2c_slave_read_buffer(port, msg->buffer, msg->size, I2C_TRANSFER_TIMEOUT_TICKS);
    }

    if (ret < 0) {
        errno = EIO;
        return -1;
    }
    if ((uint32_t)ret != msg->size) {
        errno = ETIMEDOUT;
        return -1;
    }

    return 0;
}
#endif

static esp_err_t i2c_driver_close_legacy(int port)
{
    return i2c_driver_delete(port);
}

#endif

static int config_i2c(int port, const i2c_cfg_t *cfg)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    return config_i2c_driver(port, cfg);
#else
    return config_i2c_driver_legacy(port, cfg);
#endif
}

static int i2c_transfer(int port, const i2c_msg_t *msg)
{
    int ret;

    if (i2c_stat[port].master) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
        ret = i2c_master_transfer(port, msg);
#else
        ret = i2c_master_transfer_legacy(port, msg);
#endif
    } else {
#ifdef I2C_SLAVE_VFS
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#if EXT_VFS_I2C_SLAVE_API_V2
        if (!i2c_stat[port].slave_handle) {
            errno = EINVAL;
            return -1;
        }
        if (!i2c_stat[port].slave_rx_sem && !(msg->flags & I2C_MSG_WRITE)) {
            errno = EINVAL;
            return -1;
        }
#endif
        ret = i2c_slave_transfer(port, msg);
#else
        ret = i2c_slave_transfer_legacy(port, msg);
#endif
#else
        ret = -1;
#endif
    }

    return ret;
}

static int i2c_master_read_write(int port, uint16_t addr, uint8_t *buffer, uint32_t size, bool write, bool check_ack)
{
    int ret;
    int flags;
    i2c_msg_t msg = {0};

    flags  = write ? I2C_MSG_WRITE : 0;
    flags |= check_ack ? I2C_MSG_CHECK_ACK : 0;

    msg.flags  = flags;
    msg.addr   = addr;
    msg.buffer = buffer;
    msg.size   = size;
    ret = i2c_transfer(port, &msg);

    return ret;
}

static int i2c_master_exchange(int port, const i2c_ex_msg_t *ex_msg)
{
    int ret;
    uint32_t ticks;
    bool check_ack;

    if (ex_msg->flags & I2C_EX_MSG_DELAY_EN) {
        ticks = ex_msg->delay_ms / portTICK_PERIOD_MS;
        if (ex_msg->delay_ms % portTICK_PERIOD_MS) {
            ticks++;
        }
    } else {
        ticks = 0;
    }

    if (ex_msg->flags & I2C_EX_MSG_CHECK_ACK) {
        check_ack = true;
    } else {
        check_ack = false;
    }

    if (ex_msg->flags & I2C_EX_MSG_READ_FIRST) {
        ret = i2c_master_read_write(port, ex_msg->addr, ex_msg->rx_buffer,
                                    ex_msg->rx_size, false, check_ack);
        if (ret < 0) {
            return -1;
        }

        if (ticks) {
            vTaskDelay(ticks);
        }

        ret = i2c_master_read_write(port, ex_msg->addr, ex_msg->tx_buffer,
                                    ex_msg->tx_size, true, check_ack);
        if (ret < 0) {
            return -1;
        }
    } else {
        ret = i2c_master_read_write(port, ex_msg->addr, ex_msg->tx_buffer,
                                    ex_msg->tx_size, true, check_ack);
        if (ret < 0) {
            return -1;
        }

        if (ticks) {
            vTaskDelay(ticks);
        }

        ret = i2c_master_read_write(port, ex_msg->addr, ex_msg->rx_buffer,
                                    ex_msg->rx_size, false, check_ack);
        if (ret < 0) {
            return -1;
        }
    }

    return 0;
}

static int i2c_exchange(int port, const i2c_ex_msg_t *ex_msg)
{
    int ret;

    if (i2c_stat[port].master) {
        ret = i2c_master_exchange(port, ex_msg);
    } else {
        errno = EINVAL;
        ret = -1;
    }

    return ret;
}

static int i2c_open(const char *path, int flags, int mode)
{
    int fd;

    ESP_LOGV(TAG, "open(%s, %x, %x)", path, flags, mode);

    fd = atoi(path + 1);
    if (fd < 0 || fd >= I2C_PORT_NUM) {
        ESP_LOGE(TAG, "fd=%d is out of range", fd);
        errno = EINVAL;
        return -1;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    if (!i2c_port_lock(fd)) {
        errno = EIO;
        return -1;
    }
#endif

    if (i2c_stat[fd].opened) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
        /*
         * VFS frees the FD even when close() fails, so a prior failed close can
         * leave opened=1 with live handles and no usable fd. Retry purge here.
         */
        if (i2c_stat[fd].cleanup_pending) {
            esp_err_t err = i2c_driver_close(fd);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "pending I2C cleanup still failing: %s", esp_err_to_name(err));
                i2c_port_unlock(fd);
                errno = EBUSY;
                return -1;
            }
            i2c_stat[fd].cleanup_pending = 0;
            i2c_stat[fd].opened = 0;
        } else {
            i2c_port_unlock(fd);
            errno = EBUSY;
            return -1;
        }
#else
        errno = EBUSY;
        return -1;
#endif
    }

    memset(&i2c_stat[fd], 0, sizeof(i2c_stat_t));
    i2c_stat[fd].opened = 1;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    i2c_port_unlock(fd);
#endif
    return fd;
}

static int i2c_close(int fd)
{
    esp_err_t err;

    ESP_LOGV(TAG, "close(%d)", fd);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    if (!i2c_port_lock(fd)) {
        errno = EIO;
        return -1;
    }
    err = i2c_driver_close(fd);
#else
    err = i2c_driver_close_legacy(fd);
#endif

    if (err != ESP_OK) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
        /* Keep handles; mark so a later open() can retry cleanup. */
        i2c_stat[fd].cleanup_pending = 1;
        i2c_port_unlock(fd);
#endif
        errno = EIO;
        return -1;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    i2c_stat[fd].cleanup_pending = 0;
#endif
    i2c_stat[fd].opened = 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    i2c_port_unlock(fd);
#endif
    return 0;
}

static int i2c_ioctl(int fd, int cmd, va_list va_args)
{
    int ret;

    ESP_LOGV(TAG, "cmd=%x", cmd);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    if (!i2c_port_lock(fd)) {
        errno = EIO;
        return -1;
    }
#endif

    switch (cmd) {
    case I2CIOCSCFG: {
        i2c_cfg_t *cfg = va_arg(va_args, i2c_cfg_t *);

        if (!cfg) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
            i2c_port_unlock(fd);
#endif
            errno = EINVAL;
            return -1;
        }

        ret = config_i2c(fd, cfg);
        if (ret < 0) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
            i2c_port_unlock(fd);
#endif
            return -1;
        }

        break;
    }
    case I2CIOCRDWR: {
        i2c_msg_t *msg = va_arg(va_args, i2c_msg_t *);

        if (!msg) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
            i2c_port_unlock(fd);
#endif
            errno = EINVAL;
            return -1;
        }

        ret = i2c_transfer(fd, msg);
        if (ret < 0) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
            i2c_port_unlock(fd);
#endif
            return -1;
        }

        break;
    }
    case I2CIOCEXCHANGE: {
        i2c_ex_msg_t *ex_msg = va_arg(va_args, i2c_ex_msg_t *);

        if (!ex_msg) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
            i2c_port_unlock(fd);
#endif
            errno = EINVAL;
            return -1;
        }

        ret = i2c_exchange(fd, ex_msg);
        if (ret < 0) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
            i2c_port_unlock(fd);
#endif
            return -1;
        }

        break;
    }
    default: {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
        i2c_port_unlock(fd);
#endif
        errno = EINVAL;
        return -1;
    }
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    i2c_port_unlock(fd);
#endif
    return 0;
}

int ext_vfs_i2c_init(void)
{
    static const esp_vfs_t vfs = {
        .flags   = ESP_VFS_FLAG_DEFAULT,
        .open    = i2c_open,
        .ioctl   = i2c_ioctl,
        .close   = i2c_close,
    };
    const char *base_path = "/dev/i2c";

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    i2c_port_locks_init();
    for (int i = 0; i < I2C_PORT_NUM; i++) {
        if (s_i2c_port_mtx[i] == NULL) {
            ESP_LOGE(TAG, "failed to create I2C port mutex");
            return ESP_ERR_NO_MEM;
        }
    }
#endif

    esp_err_t err = esp_vfs_register(base_path, &vfs, NULL);
    if (err != ESP_OK) {
        return err;
    }

    return 0;
}
