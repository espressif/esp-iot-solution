/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "i2c_bus.h"
#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
#include "i2c_bus_soft.h"
#endif

#define I2C_ACK_CHECK_EN 0x1                                                                            /*!< I2C master will check ack from slave*/
#define I2C_ACK_CHECK_DIS 0x0                                                                           /*!< I2C master will not check ack from slave */
#define I2C_BUS_FLG_DEFAULT (0)
#define I2C_BUS_MASTER_BUF_LEN (0)
#define I2C_BUS_MS_TO_WAIT CONFIG_I2C_MS_TO_WAIT
#define I2C_BUS_TICKS_TO_WAIT (I2C_BUS_MS_TO_WAIT/portTICK_PERIOD_MS)
#define I2C_BUS_MUTEX_TICKS_TO_WAIT (I2C_BUS_MS_TO_WAIT/portTICK_PERIOD_MS)

typedef struct {
    i2c_master_bus_config_t bus_config;                                                                 /*!< I2C master bus specific configurations */
    i2c_master_bus_handle_t bus_handle;                                                                 /*!< I2C master bus handle */
    i2c_device_config_t device_config;                                                                  /*!< I2C device configuration, in order to get the frequency information of I2C */
#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    i2c_master_soft_bus_handle_t soft_bus_handle;                                                       /*!< I2C master soft bus handle */
#endif
    bool is_init;                                                                                       /*!< if bus is initialized */
    i2c_config_t conf_activate;                                                                         /*!< I2C active configuration */
    SemaphoreHandle_t mutex;                                                                            /*!< mutex to achieve thread-safe */
    int32_t ref_counter;                                                                                /*!< reference count */
} i2c_bus_t;

typedef struct {
    i2c_device_config_t device_config;                                                                  /*!< I2C device configuration */
    i2c_master_dev_handle_t dev_handle;                                                                 /*!< I2C master bus device handle */
    i2c_device_config_t conf;                                                                           /*!< I2C active configuration */
    i2c_bus_t *i2c_bus;                                                                                 /*!< I2C bus */
} i2c_bus_device_t;

static const char *TAG = "i2c_bus";

#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
static i2c_bus_t s_i2c_bus[I2C_NUM_SW_MAX];                                                             /*!< If software I2C is enabled, additional space is required to store the port. */
#else
static i2c_bus_t s_i2c_bus[I2C_NUM_MAX];
#endif

#define I2C_BUS_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

#define I2C_BUS_CHECK_GOTO(a, str, label) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        goto label; \
    }

#define I2C_BUS_INIT_CHECK(is_init, ret) if(!is_init) { \
        ESP_LOGE(TAG,"%s:%d (%s):i2c_bus has not inited", __FILE__, __LINE__, __FUNCTION__); \
        return (ret); \
    }

#define I2C_BUS_MUTEX_TAKE(mutex, ret) if (!xSemaphoreTake(mutex, I2C_BUS_MUTEX_TICKS_TO_WAIT)) { \
        ESP_LOGE(TAG, "i2c_bus take mutex timeout, max wait = %"PRIu32"ms", I2C_BUS_MUTEX_TICKS_TO_WAIT); \
        return (ret); \
    }

#define I2C_BUS_MUTEX_TAKE_MAX_DELAY(mutex, ret) if (!xSemaphoreTake(mutex, portMAX_DELAY)) { \
        ESP_LOGE(TAG, "i2c_bus take mutex timeout, max wait = %"PRIu32"ms", portMAX_DELAY); \
        return (ret); \
    }

#define I2C_BUS_MUTEX_GIVE(mutex, ret) if (!xSemaphoreGive(mutex)) { \
        ESP_LOGE(TAG, "i2c_bus give mutex failed"); \
        return (ret); \
    }

static esp_err_t i2c_driver_reinit(i2c_port_t port, const i2c_config_t *conf);
static esp_err_t i2c_driver_deinit(i2c_port_t port);
static esp_err_t i2c_bus_write_reg8(i2c_bus_device_handle_t dev_handle, uint8_t mem_address, size_t data_len, const uint8_t *data);
static esp_err_t i2c_bus_read_reg8(i2c_bus_device_handle_t dev_handle, uint8_t mem_address, size_t data_len, uint8_t *data);
inline static bool i2c_config_compare(i2c_port_t port, const i2c_config_t *conf);
/**************************************** Public Functions (Application level)*********************************************/

i2c_bus_handle_t i2c_bus_create(i2c_port_t port, const i2c_config_t *conf)
{
#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    I2C_BUS_CHECK(((i2c_sw_port_t)port < I2C_NUM_SW_MAX) || (port == I2C_NUM_MAX), "I2C port error", NULL);
#else
    I2C_BUS_CHECK(port < I2C_NUM_MAX, "I2C port error", NULL);
#endif
    I2C_BUS_CHECK(conf != NULL, "pointer = NULL error", NULL);
    I2C_BUS_CHECK(conf->mode == I2C_MODE_MASTER, "i2c_bus only supports master mode", NULL);

    if (s_i2c_bus[port].is_init) {
        // if i2c_bus has been inited and configs not changed, return the handle directly
        if (i2c_config_compare(port, conf)) {
            ESP_LOGW(TAG, "i2c%d has been inited, return handle directly, ref_counter=%"PRIi32"", port, s_i2c_bus[port].ref_counter);
            return (i2c_bus_handle_t)&s_i2c_bus[port];
        }
    } else {
        s_i2c_bus[port].mutex = xSemaphoreCreateMutex();
        I2C_BUS_CHECK(s_i2c_bus[port].mutex != NULL, "i2c_bus xSemaphoreCreateMutex failed", NULL);
        s_i2c_bus[port].ref_counter = 0;
    }

    esp_err_t ret = i2c_driver_reinit(port, conf);                                                      /*!< Reconfigure the I2C parameters and initialise the bus. */
    I2C_BUS_CHECK(ret == ESP_OK, "init error", NULL);
    s_i2c_bus[port].conf_activate = *conf;
    s_i2c_bus[port].bus_config.i2c_port = port;
    s_i2c_bus[port].device_config.scl_speed_hz = conf->master.clk_speed;                                /*!< Stores the frequency configured in the bus, overrides the value if the device has a separate frequency */
    ESP_LOGI(TAG, "I2C Bus V2 Config Succeed, Version: %d.%d.%d", I2C_BUS_VER_MAJOR, I2C_BUS_VER_MINOR, I2C_BUS_VER_PATCH);
    return (i2c_bus_handle_t)&s_i2c_bus[port];
}

esp_err_t i2c_bus_delete(i2c_bus_handle_t *p_bus)
{
    I2C_BUS_CHECK(p_bus != NULL && *p_bus != NULL, "pointer = NULL error", ESP_ERR_INVALID_ARG);
    i2c_bus_t *i2c_bus = (i2c_bus_t *)(*p_bus);
    I2C_BUS_INIT_CHECK(i2c_bus->is_init, ESP_FAIL);
    I2C_BUS_MUTEX_TAKE_MAX_DELAY(i2c_bus->mutex, ESP_ERR_TIMEOUT);

    // if ref_counter == 0, de-init the bus
    if ((i2c_bus->ref_counter) > 0) {
        ESP_LOGW(TAG, "i2c%d is also handled by others ref_counter=%"PRIi32", won't be de-inited", i2c_bus->bus_config.i2c_port, i2c_bus->ref_counter);
        return ESP_OK;
    }

    esp_err_t ret = i2c_driver_deinit(i2c_bus->bus_config.i2c_port);
    I2C_BUS_CHECK(ret == ESP_OK, "deinit error", ret);
    vSemaphoreDelete(i2c_bus->mutex);
    *p_bus = NULL;
    return ESP_OK;
}

uint32_t i2c_bus_get_current_clk_speed(i2c_bus_handle_t bus_handle)
{
    I2C_BUS_CHECK(bus_handle != NULL, "Null Bus Handle", 0);
    i2c_bus_t *i2c_bus = (i2c_bus_t *)bus_handle;
    I2C_BUS_INIT_CHECK(i2c_bus->is_init, 0);
    return i2c_bus->conf_activate.master.clk_speed;
}

uint8_t i2c_bus_get_created_device_num(i2c_bus_handle_t bus_handle)
{
    I2C_BUS_CHECK(bus_handle != NULL, "Null Bus Handle", 0);
    i2c_bus_t *i2c_bus = (i2c_bus_t *)bus_handle;
    I2C_BUS_INIT_CHECK(i2c_bus->is_init, 0);
    return i2c_bus->ref_counter;
}

i2c_bus_device_handle_t i2c_bus_device_create(i2c_bus_handle_t bus_handle, uint8_t dev_addr, uint32_t clk_speed)
{
    I2C_BUS_CHECK(bus_handle != NULL, "Null Bus Handle", NULL);
    I2C_BUS_CHECK(clk_speed <= 400000, "clk_speed must <= 400000", NULL);
    i2c_bus_t *i2c_bus = (i2c_bus_t *)bus_handle;
    I2C_BUS_INIT_CHECK(i2c_bus->is_init, NULL);
    i2c_bus_device_t *i2c_device = calloc(1, sizeof(i2c_bus_device_t));
    I2C_BUS_CHECK(i2c_device != NULL, "calloc memory failed", NULL);
    I2C_BUS_MUTEX_TAKE_MAX_DELAY(i2c_bus->mutex, NULL);
    i2c_device->device_config.device_address = dev_addr;
    i2c_device->device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    i2c_device->device_config.scl_speed_hz = i2c_bus->device_config.scl_speed_hz;                       /*!< Transfer the frequency information in the bus to the device. */
    i2c_device->device_config.flags.disable_ack_check = i2c_bus->device_config.flags.disable_ack_check; /*!< Transfer the ack check in the bus to the device. */

    if (clk_speed != 0) {
        i2c_device->device_config.scl_speed_hz = clk_speed;
    }

#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    // For software I2C, it is only necessary to save the device configuration.
    if (i2c_bus->bus_config.i2c_port < I2C_NUM_MAX)
#endif
    {
        esp_err_t ret = i2c_master_bus_add_device(i2c_bus->bus_handle, &i2c_device->device_config, &i2c_device->dev_handle);
        I2C_BUS_CHECK(ret == ESP_OK, "add device error", NULL);
    }
    i2c_device->i2c_bus = i2c_bus;
    i2c_bus->ref_counter++;
    I2C_BUS_MUTEX_GIVE(i2c_bus->mutex, NULL);
    return (i2c_bus_device_handle_t)i2c_device;
}

esp_err_t i2c_bus_device_delete(i2c_bus_device_handle_t *p_dev_handle)
{
    I2C_BUS_CHECK(p_dev_handle != NULL && *p_dev_handle != NULL, "Null Device Handle", ESP_ERR_INVALID_ARG);
    i2c_bus_device_t *i2c_device = (i2c_bus_device_t *)(*p_dev_handle);

#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    if (i2c_device->i2c_bus->bus_config.i2c_port < I2C_NUM_MAX)
#endif
    {
        esp_err_t ret = i2c_master_bus_rm_device(i2c_device->dev_handle);
        I2C_BUS_CHECK(ret == ESP_OK, "remove device error", ret);
    }
    I2C_BUS_MUTEX_TAKE_MAX_DELAY(i2c_device->i2c_bus->mutex, ESP_ERR_TIMEOUT);
    i2c_device->i2c_bus->ref_counter--;                                                                 /*!< ref_counter is reduced only when the device is removed successfully. */
    I2C_BUS_MUTEX_GIVE(i2c_device->i2c_bus->mutex, ESP_FAIL);
    free(i2c_device);
    *p_dev_handle = NULL;
    return ESP_OK;
}

uint8_t i2c_bus_scan(i2c_bus_handle_t bus_handle, uint8_t *buf, uint8_t num)
{
    I2C_BUS_CHECK(bus_handle != NULL, "Handle error", 0);
    i2c_bus_t *i2c_bus = (i2c_bus_t *)bus_handle;
    I2C_BUS_INIT_CHECK(i2c_bus->is_init, 0);
    uint8_t device_count = 0;
    esp_err_t ret = ESP_OK;
    I2C_BUS_MUTEX_TAKE_MAX_DELAY(i2c_bus->mutex, 0);

    for (uint8_t dev_address = 1; dev_address < 127; dev_address++) {
#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
        if (i2c_bus->bus_config.i2c_port > I2C_NUM_MAX) {
            ret  = i2c_master_soft_bus_probe(i2c_bus->soft_bus_handle, dev_address);
        } else
#endif
        {
            ret = i2c_master_probe(i2c_bus->bus_handle, dev_address, I2C_BUS_TICKS_TO_WAIT);
        }
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "found i2c device address = 0x%02x", dev_address);
            if (buf != NULL && device_count < num) {
                *(buf + device_count) = dev_address;
            }
            device_count++;
        }
    }
    I2C_BUS_MUTEX_GIVE(i2c_bus->mutex, 0);
    return device_count;
}

uint8_t i2c_bus_device_get_address(i2c_bus_device_handle_t dev_handle)
{
    I2C_BUS_CHECK(dev_handle != NULL, "device handle error", NULL_I2C_DEV_ADDR);
    i2c_bus_device_t *i2c_device = (i2c_bus_device_t *)dev_handle;
    return i2c_device->device_config.device_address;
}

esp_err_t i2c_bus_read_bytes(i2c_bus_device_handle_t dev_handle, uint8_t mem_address, size_t data_len, uint8_t *data)
{
    return i2c_bus_read_reg8(dev_handle, mem_address, data_len, data);
}

esp_err_t i2c_bus_read_byte(i2c_bus_device_handle_t dev_handle, uint8_t mem_address, uint8_t *data)
{
    return i2c_bus_read_reg8(dev_handle, mem_address, 1, data);
}

esp_err_t i2c_bus_read_bit(i2c_bus_device_handle_t dev_handle, uint8_t mem_address, uint8_t bit_num, uint8_t *data)
{
    uint8_t byte = 0;
    esp_err_t ret = i2c_bus_read_reg8(dev_handle, mem_address, 1, &byte);
    *data = byte & (1 << bit_num);
    *data = (*data != 0) ? 1 : 0;
    return ret;
}

esp_err_t i2c_bus_read_bits(i2c_bus_device_handle_t dev_handle, uint8_t mem_address, uint8_t bit_start, uint8_t length, uint8_t *data)
{
    uint8_t byte = 0;
    esp_err_t ret = i2c_bus_read_byte(dev_handle, mem_address, &byte);

    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t mask = ((1 << length) - 1) << (bit_start - length + 1);
    byte &= mask;
    byte >>= (bit_start - length + 1);
    *data = byte;
    return ret;
}

esp_err_t i2c_bus_write_byte(i2c_bus_device_handle_t dev_handle, uint8_t mem_address, uint8_t data)
{
    return i2c_bus_write_reg8(dev_handle, mem_address, 1, &data);
}

esp_err_t i2c_bus_write_bytes(i2c_bus_device_handle_t dev_handle, uint8_t mem_address, size_t data_len, const uint8_t *data)
{
    return i2c_bus_write_reg8(dev_handle, mem_address, data_len, data);
}

esp_err_t i2c_bus_write_bit(i2c_bus_device_handle_t dev_handle, uint8_t mem_address, uint8_t bit_num, uint8_t data)
{
    uint8_t byte = 0;
    esp_err_t ret = i2c_bus_read_byte(dev_handle, mem_address, &byte);

    if (ret != ESP_OK) {
        return ret;
    }

    byte = (data != 0) ? (byte | (1 << bit_num)) : (byte & ~(1 << bit_num));
    return i2c_bus_write_byte(dev_handle, mem_address, byte);
}

esp_err_t i2c_bus_write_bits(i2c_bus_device_handle_t dev_handle, uint8_t mem_address, uint8_t bit_start, uint8_t length, uint8_t data)
{
    uint8_t byte = 0;
    esp_err_t ret = i2c_bus_read_byte(dev_handle, mem_address, &byte);

    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t mask = ((1 << length) - 1) << (bit_start - length + 1);
    data <<= (bit_start - length + 1); // shift data into correct position
    data &= mask;                     // zero all non-important bits in data
    byte &= ~(mask);                  // zero all important bits in existing byte
    byte |= data;                     // combine data with existing byte
    return i2c_bus_write_byte(dev_handle, mem_address, byte);
}

/**************************************** Public Functions (Low level)*********************************************/

static esp_err_t i2c_bus_read_reg8(i2c_bus_device_handle_t dev_handle, uint8_t mem_address, size_t data_len, uint8_t *data)
{
    I2C_BUS_CHECK(dev_handle != NULL, "device handle error", ESP_ERR_INVALID_ARG);
    I2C_BUS_CHECK(data != NULL, "data pointer error", ESP_ERR_INVALID_ARG);
    i2c_bus_device_t *i2c_device = (i2c_bus_device_t *)dev_handle;
    I2C_BUS_INIT_CHECK(i2c_device->i2c_bus->is_init, ESP_ERR_INVALID_STATE);
    I2C_BUS_MUTEX_TAKE(i2c_device->i2c_bus->mutex, ESP_ERR_TIMEOUT);
    esp_err_t ret = ESP_FAIL;

#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    // Need to distinguish between hardware I2C and software I2C via port
    if (i2c_device->i2c_bus->bus_config.i2c_port > I2C_NUM_MAX) {
        ret = i2c_master_soft_bus_change_frequency(i2c_device->i2c_bus->soft_bus_handle, i2c_device->device_config.scl_speed_hz);
        ret = i2c_master_soft_bus_read_reg8(i2c_device->i2c_bus->soft_bus_handle, i2c_device->device_config.device_address, mem_address, data_len, data);
    } else
#endif
    {
        if (mem_address != NULL_I2C_MEM_ADDR) {
            ret = i2c_master_transmit_receive(i2c_device->dev_handle, &mem_address, 1, data, data_len, I2C_BUS_TICKS_TO_WAIT);
        } else {
            ret = i2c_master_receive(i2c_device->dev_handle, data, data_len, I2C_BUS_TICKS_TO_WAIT);
        }
    }
    I2C_BUS_MUTEX_GIVE(i2c_device->i2c_bus->mutex, ESP_FAIL);
    return ret;
}

esp_err_t i2c_bus_read_reg16(i2c_bus_device_handle_t dev_handle, uint16_t mem_address, size_t data_len, uint8_t *data)
{
    I2C_BUS_CHECK(dev_handle != NULL, "device handle error", ESP_ERR_INVALID_ARG);
    I2C_BUS_CHECK(data != NULL, "data pointer error", ESP_ERR_INVALID_ARG);
    i2c_bus_device_t *i2c_device = (i2c_bus_device_t *)dev_handle;
    I2C_BUS_INIT_CHECK(i2c_device->i2c_bus->is_init, ESP_ERR_INVALID_STATE);
    esp_err_t ret = ESP_FAIL;
    uint8_t memAddress8[2];
    memAddress8[0] = (uint8_t)((mem_address >> 8) & 0x00FF);
    memAddress8[1] = (uint8_t)(mem_address & 0x00FF);
    I2C_BUS_MUTEX_TAKE(i2c_device->i2c_bus->mutex, ESP_ERR_TIMEOUT);

#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    // Need to distinguish between hardware I2C and software I2C via port
    if (i2c_device->i2c_bus->bus_config.i2c_port > I2C_NUM_MAX) {
        ret = i2c_master_soft_bus_change_frequency(i2c_device->i2c_bus->soft_bus_handle, i2c_device->device_config.scl_speed_hz);
        ret = i2c_master_soft_bus_read_reg16(i2c_device->i2c_bus->soft_bus_handle, i2c_device->device_config.device_address, mem_address, data_len, data);
    } else
#endif
    {
        if (mem_address != NULL_I2C_MEM_16BIT_ADDR) {
            ret = i2c_master_transmit_receive(i2c_device->dev_handle, memAddress8, 2, data, data_len, I2C_BUS_TICKS_TO_WAIT);
        } else {
            ret = i2c_master_receive(i2c_device->dev_handle, data, data_len, I2C_BUS_TICKS_TO_WAIT);
        }
    }
    I2C_BUS_MUTEX_GIVE(i2c_device->i2c_bus->mutex, ESP_FAIL);
    return ret;
}

static esp_err_t i2c_bus_write_reg8(i2c_bus_device_handle_t dev_handle, uint8_t mem_address, size_t data_len, const uint8_t *data)
{
    I2C_BUS_CHECK(dev_handle != NULL, "device handle error", ESP_ERR_INVALID_ARG);
    I2C_BUS_CHECK(data != NULL, "data pointer error", ESP_ERR_INVALID_ARG);
    i2c_bus_device_t *i2c_device = (i2c_bus_device_t *)dev_handle;
    I2C_BUS_INIT_CHECK(i2c_device->i2c_bus->is_init, ESP_ERR_INVALID_STATE);
    I2C_BUS_MUTEX_TAKE(i2c_device->i2c_bus->mutex, ESP_ERR_TIMEOUT);
    esp_err_t ret = ESP_FAIL;

#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    // Need to distinguish between hardware I2C and software I2C via port
    if (i2c_device->i2c_bus->bus_config.i2c_port > I2C_NUM_MAX) {
        ret = i2c_master_soft_bus_change_frequency(i2c_device->i2c_bus->soft_bus_handle, i2c_device->device_config.scl_speed_hz);
        ret = i2c_master_soft_bus_write_reg8(i2c_device->i2c_bus->soft_bus_handle, i2c_device->device_config.device_address, mem_address, data_len, data);
    } else
#endif
    {
        if (mem_address != NULL_I2C_MEM_ADDR) {
            uint8_t *data_addr = malloc(data_len + 1);
            if (data_addr == NULL) {
                ESP_LOGE(TAG, "data_addr memory alloc fail");
                I2C_BUS_MUTEX_GIVE(i2c_device->i2c_bus->mutex, ESP_FAIL);
                return ESP_ERR_NO_MEM;                                                                      /*!< If the memory request fails, unlock it immediately and return an error. */
            }
            data_addr[0] = mem_address;
            for (int i = 0; i < data_len; i++) {
                data_addr[i + 1] = data[i];
            }
            ret = i2c_master_transmit(i2c_device->dev_handle, data_addr, data_len + 1, I2C_BUS_TICKS_TO_WAIT);
            free(data_addr);
        } else {
            ret = i2c_master_transmit(i2c_device->dev_handle, data, data_len, I2C_BUS_TICKS_TO_WAIT);
        }
    }
    I2C_BUS_MUTEX_GIVE(i2c_device->i2c_bus->mutex, ESP_FAIL);
    return ret;
}

esp_err_t i2c_bus_write_reg16(i2c_bus_device_handle_t dev_handle, uint16_t mem_address, size_t data_len, const uint8_t *data)
{
    I2C_BUS_CHECK(dev_handle != NULL, "device handle error", ESP_ERR_INVALID_ARG);
    I2C_BUS_CHECK(data != NULL, "data pointer error", ESP_ERR_INVALID_ARG);
    i2c_bus_device_t *i2c_device = (i2c_bus_device_t *)dev_handle;
    I2C_BUS_INIT_CHECK(i2c_device->i2c_bus->is_init, ESP_ERR_INVALID_STATE);
    uint8_t memAddress8[2];
    memAddress8[0] = (uint8_t)((mem_address >> 8) & 0x00FF);
    memAddress8[1] = (uint8_t)(mem_address & 0x00FF);
    I2C_BUS_MUTEX_TAKE(i2c_device->i2c_bus->mutex, ESP_ERR_TIMEOUT);
    esp_err_t ret = ESP_FAIL;

#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    if (i2c_device->i2c_bus->bus_config.i2c_port > I2C_NUM_MAX) {
        ret = i2c_master_soft_bus_change_frequency(i2c_device->i2c_bus->soft_bus_handle, i2c_device->device_config.scl_speed_hz);
        ret = i2c_master_soft_bus_write_reg16(i2c_device->i2c_bus->soft_bus_handle, i2c_device->device_config.device_address, mem_address, data_len, data);
    } else
#endif
    {
        if (mem_address != NULL_I2C_MEM_16BIT_ADDR) {
            uint8_t *data_addr = malloc(data_len + 2);
            if (data_addr == NULL) {
                ESP_LOGE(TAG, "data_addr memory alloc fail");
                I2C_BUS_MUTEX_GIVE(i2c_device->i2c_bus->mutex, ESP_FAIL);
                return ESP_ERR_NO_MEM;                                                                      /*!< If the memory request fails, unlock it immediately and return an error. */
            }
            data_addr[0] = memAddress8[0];
            data_addr[1] = memAddress8[1];
            for (int i = 0; i < data_len; i++) {
                data_addr[i + 2] = data[i];
            }
            ret = i2c_master_transmit(i2c_device->dev_handle, data_addr, data_len + 2, I2C_BUS_TICKS_TO_WAIT);
            free(data_addr);
        } else {
            ret = i2c_master_transmit(i2c_device->dev_handle, data, data_len, I2C_BUS_TICKS_TO_WAIT);
        }
    }
    I2C_BUS_MUTEX_GIVE(i2c_device->i2c_bus->mutex, ESP_FAIL);
    return ret;
}

/**************************************** Private Functions*********************************************/

static esp_err_t i2c_driver_reinit(i2c_port_t port, const i2c_config_t *conf)
{
#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    I2C_BUS_CHECK(((i2c_sw_port_t)port < I2C_NUM_SW_MAX) || (port == I2C_NUM_MAX), "I2C port error", ESP_ERR_INVALID_ARG);
#else
    I2C_BUS_CHECK(port < I2C_NUM_MAX, "i2c port error", ESP_ERR_INVALID_ARG);
#endif
    I2C_BUS_CHECK(conf != NULL, "pointer = NULL error", ESP_ERR_INVALID_ARG);

    if (s_i2c_bus[port].is_init) {
#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
        if (port > I2C_NUM_MAX) {
            i2c_del_master_soft_bus(s_i2c_bus[port].soft_bus_handle);
        } else
#endif
        {
            i2c_del_master_bus(s_i2c_bus[port].bus_handle);
        }
        s_i2c_bus[port].is_init = false;
        ESP_LOGI(TAG, "i2c%d bus deinited", port);
    }

#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    if (port > I2C_NUM_MAX) {
        s_i2c_bus[port].bus_config.i2c_port = port;                                                         /*!< Similarly, it is necessary to preserve the I2C_PORT for later distinction. */
        esp_err_t ret = i2c_new_master_soft_bus(conf, &s_i2c_bus[port].soft_bus_handle);
        I2C_BUS_CHECK(ret == ESP_OK, "i2c software driver install failed", ret);
        s_i2c_bus[port].is_init = true;
        ESP_LOGI(TAG, "i2c%d software bus inited", port);
    } else
#endif
    {
        // Convert i2c_config_t information to i2c_master_bus_config_t and i2c_device_config_t
        s_i2c_bus[port].bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        s_i2c_bus[port].bus_config.i2c_port = port;
        s_i2c_bus[port].bus_config.scl_io_num = conf->scl_io_num;
        s_i2c_bus[port].bus_config.sda_io_num = conf->sda_io_num;
        s_i2c_bus[port].bus_config.glitch_ignore_cnt = 7;                                                   /*!< Set the burr cycle of the host bus */
        s_i2c_bus[port].bus_config.flags.enable_internal_pullup = (conf->scl_pullup_en | conf->sda_pullup_en);
        s_i2c_bus[port].device_config.scl_speed_hz = conf->master.clk_speed;
        s_i2c_bus[port].device_config.flags.disable_ack_check = false;

        esp_err_t ret = i2c_new_master_bus(&s_i2c_bus[port].bus_config, &s_i2c_bus[port].bus_handle);
        I2C_BUS_CHECK(ret == ESP_OK, "i2c driver install failed", ret);
        s_i2c_bus[port].is_init = true;
        ESP_LOGI(TAG, "i2c%d bus inited", port);
    }
    return ESP_OK;
}

static esp_err_t i2c_driver_deinit(i2c_port_t port)
{
#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    I2C_BUS_CHECK(((i2c_sw_port_t)port < I2C_NUM_SW_MAX) || (port == I2C_NUM_MAX), "I2C port error", ESP_ERR_INVALID_ARG);
#else
    I2C_BUS_CHECK(port < I2C_NUM_MAX, "i2c port error", ESP_ERR_INVALID_ARG);
#endif
    I2C_BUS_CHECK(s_i2c_bus[port].is_init == true, "i2c not inited", ESP_ERR_INVALID_STATE);

    // Need to distinguish between hardware I2C and software I2C via port
#if CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    if (port > I2C_NUM_MAX) {
        esp_err_t ret = i2c_del_master_soft_bus(s_i2c_bus[port].soft_bus_handle);
        I2C_BUS_CHECK(ret == ESP_OK, "i2c software driver delete failed", ret);
    } else
#endif
    {
        esp_err_t ret = i2c_del_master_bus(s_i2c_bus[port].bus_handle);
        I2C_BUS_CHECK(ret == ESP_OK, "i2c driver delete failed", ret);
    }
    s_i2c_bus[port].is_init = false;
    ESP_LOGI(TAG, "i2c%d bus deinited", port);
    return ESP_OK;
}

/**
 * @brief compare with active i2c_bus configuration
 *
 * @param port choose which i2c_port's configuration will be compared
 * @param conf new configuration
 * @return true new configuration is equal to active configuration
 * @return false new configuration is not equal to active configuration
 */
inline static bool i2c_config_compare(i2c_port_t port, const i2c_config_t *conf)
{
#if  CONFIG_I2C_BUS_SUPPORT_SOFTWARE
    if (port > I2C_NUM_MAX) {
        if (s_i2c_bus[port].conf_activate.master.clk_speed == conf->master.clk_speed
                && s_i2c_bus[port].conf_activate.sda_io_num == conf->sda_io_num
                && s_i2c_bus[port].conf_activate.scl_io_num == conf->scl_io_num
                && s_i2c_bus[port].conf_activate.scl_pullup_en == conf->scl_pullup_en
                && s_i2c_bus[port].conf_activate.sda_pullup_en == conf->sda_pullup_en) {
            return true;
        }
    } else
#endif
    {
        if (s_i2c_bus[port].bus_config.sda_io_num == conf->sda_io_num
                && s_i2c_bus[port].bus_config.scl_io_num == conf->scl_io_num
                && s_i2c_bus[port].bus_config.flags.enable_internal_pullup == (conf->scl_pullup_en | conf->sda_pullup_en)) {
            return true;
        }
    }
    return false;
}
