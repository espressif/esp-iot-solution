/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "mcp23017.h"
#include "esp_log.h"

#define I2C_TIMEOUT_MS                  (1000)
#define I2C_CLK_SPEED                   (100000)
#define MCP23017_IO_COUNT               (16)
#define MCP23017_DIR_REG_DEFAULT_VAL    (0xFFFF)
#define MCP23017_OUT_REG_DEFAULT_VAL    (0x0000)
#define MCP23017_PULLUP_DEFAULT_VAL     (0x0000)
#define MCP23017_PULLUP_SEL_DEFAULT_VAL (0x0000)

#define MCP23017_PORT_A_BYTE(x)         (x & 0xFF)                      //get pin of GPIOA
#define MCP23017_PORT_B_BYTE(x)         (x >> 8)                        //get pin of GPIOB
#define MCP23017_PORT_AB_WORD(buff)     (buff[0] | (buff[1] << 8))      //get pin of GPIOA and pin of GPIOB

static const char *TAG = "mcp23017";

#define MCP23017_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

#define MCP23017_CHECK_GOTO(a, str, label) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        goto label; \
    }

/**
 * @brief register address when iocon.bank == 0 (default)
 *
 */
typedef enum {
    MCP23017_REG_IODIRA = 0, /*!< DIRECTION REGISTER A */
    MCP23017_REG_IODIRB,     /*!< DIRECTION REGISTER B */
    MCP23017_REG_IPOLA, /*!< INPUT POLARITY REGISTER A */
    MCP23017_REG_IPOLB, /*!< INPUT POLARITY REGISTER B */
    MCP23017_REG_GPINTENA, /*!< NTERRUPT-ON-CHANGE CONTROL REGISTER A */
    MCP23017_REG_GPINTENB,  /*!< NTERRUPT-ON-CHANGE CONTROL REGISTER B */
    MCP23017_REG_DEFVALA,  /*!< DEFAULT COMPARE VALUE A */
    MCP23017_REG_DEFVALB,  /*!< DEFAULT COMPARE VALUE B */
    MCP23017_REG_INTCONA,  /*!< INTERRUPT-ON-CHANGE CONTROL REGISTER A */
    MCP23017_REG_INTCONB,  /*!< INTERRUPT-ON-CHANGE CONTROL REGISTER B */
    MCP23017_REG_IOCONA,  /*!< I/O EXPANDER CONFIGURATION REGISTER */
    MCP23017_REG_IOCONB,  /*!< I/O EXPANDER CONFIGURATION REGISTER */
    MCP23017_REG_GPPUA,  /*!< PULL-UP RESISTOR REGISTER A */
    MCP23017_REG_GPPUB,  /*!< PULL-UP RESISTOR REGISTER B */
    MCP23017_REG_INTFA,  /*!< INTERRUPT FLAG REGISTER A */
    MCP23017_REG_INTFB,  /*!< INTERRUPT FLAG REGISTER B */
    MCP23017_REG_INTCAPA,  /*!< INTERRUPT CAPTURED VALUE FOR PORT REGISTER A */
    MCP23017_REG_INTCAPB,  /*!< INTERRUPT CAPTURED VALUE FOR PORT REGISTER B */
    MCP23017_REG_GPIOA,  /*!<  GENERAL PURPOSE I/O PORT REGISTER A */
    MCP23017_REG_GPIOB,  /*!<  GENERAL PURPOSE I/O PORT REGISTER B */
    MCP23017_REG_OLATA,  /*!< OUTPUT LATCH REGISTER 0 A */
    MCP23017_REG_OLATB,  /*!< OUTPUT LATCH REGISTER 0 B */
} mcp23017_reg_t;

typedef enum {
    MCP23017_IOCON_UNIMPLEMENTED = 0x01,
    MCP23017_IOCON_INTPOL = 0x01 << 1,
    MCP23017_IOCON_ODR = 0x01 << 2,
    MCP23017_IOCON_HAEN = 0x01 << 3,
    MCP23017_IOCON_DISSLW = 0x01 << 4,
    MCP23017_IOCON_SEQOP = 0x01 << 5,
    MCP23017_IOCON_MIRROR = 0x01 << 6,
    MCP23017_IOCON_BANK = 0x01 << 7,
} mcp23017_reg_iocon_t;

typedef struct {
    esp_io_expander_t base;
    i2c_master_dev_handle_t i2c_dev;
    uint16_t direction;
    uint16_t output;
    uint16_t pullup;
    uint16_t intEnabledPins;//pin of interrupt
} mcp23017_dev_t;

static esp_err_t read_input_reg(esp_io_expander_handle_t handle, uint32_t *value);
static esp_err_t write_output_reg(esp_io_expander_handle_t handle, uint32_t value);
static esp_err_t read_output_reg(esp_io_expander_handle_t handle, uint32_t *value);
static esp_err_t write_direction_reg(esp_io_expander_handle_t handle, uint32_t value);
static esp_err_t read_direction_reg(esp_io_expander_handle_t handle, uint32_t *value);
static esp_err_t write_pullup_en_reg(esp_io_expander_handle_t handle, uint32_t value);
static esp_err_t read_pullup_en_reg(esp_io_expander_handle_t handle, uint32_t *value);
static esp_err_t write_pullup_sel_reg(esp_io_expander_handle_t handle, uint32_t value);
static esp_err_t read_pullup_sel_reg(esp_io_expander_handle_t handle, uint32_t *value);
static esp_err_t reset(esp_io_expander_handle_t handle);
static esp_err_t del(esp_io_expander_handle_t handle);

static mcp23017_dev_t *mcp23017_from_handle(esp_io_expander_handle_t handle)
{
    return __containerof(handle, mcp23017_dev_t, base);
}

static uint16_t mcp23017_update_port_value(uint16_t data, uint8_t value, mcp23017_gpio_port_t gpio)
{
    if (gpio == MCP23017_GPIOA) {
        return (data & 0xFF00) | value;
    }
    return (data & 0x00FF) | ((uint16_t)value << 8);
}

esp_err_t esp_io_expander_new_i2c_mcp23017(i2c_master_bus_handle_t i2c_bus, uint32_t dev_addr,
                                           esp_io_expander_handle_t *handle_ret)
{
    MCP23017_CHECK(handle_ret != NULL, "invalid arg", ESP_ERR_INVALID_ARG)

    mcp23017_dev_t *p_device = (mcp23017_dev_t *) calloc(1, sizeof(mcp23017_dev_t));
    MCP23017_CHECK(p_device != NULL, "calloc failed", ESP_ERR_NO_MEM)

    const i2c_device_config_t i2c_dev_cfg = {
        .device_address = dev_addr,
        .scl_speed_hz = I2C_CLK_SPEED,
    };
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &i2c_dev_cfg, &p_device->i2c_dev);
    MCP23017_CHECK_GOTO(ret == ESP_OK, "add i2c device failed", err)

    p_device->base.config.io_count = MCP23017_IO_COUNT;
    p_device->base.config.flags.dir_out_bit_zero = 1;
    p_device->base.config.flags.pullup_high_bit_zero = 1;
    p_device->base.read_input_reg = read_input_reg;
    p_device->base.write_output_reg = write_output_reg;
    p_device->base.read_output_reg = read_output_reg;
    p_device->base.write_direction_reg = write_direction_reg;
    p_device->base.read_direction_reg = read_direction_reg;
    p_device->base.write_pullup_en_reg = write_pullup_en_reg;
    p_device->base.read_pullup_en_reg = read_pullup_en_reg;
    p_device->base.write_pullup_sel_reg = write_pullup_sel_reg;
    p_device->base.read_pullup_sel_reg = read_pullup_sel_reg;
    p_device->base.del = del;
    p_device->base.reset = reset;

    ret = reset(&p_device->base);
    MCP23017_CHECK_GOTO(ret == ESP_OK, "reset failed", err_dev)

    *handle_ret = &p_device->base;
    return ESP_OK;

err_dev:
    i2c_master_bus_rm_device(p_device->i2c_dev);
err:
    free(p_device);
    return ret;
}

mcp23017_handle_t mcp23017_create(i2c_master_bus_handle_t bus, uint8_t dev_addr)
{
    esp_io_expander_handle_t handle = NULL;

    if (esp_io_expander_new_i2c_mcp23017(bus, dev_addr, &handle) != ESP_OK) {
        return NULL;
    }

    return (mcp23017_handle_t)handle;
}

esp_err_t mcp23017_delete(mcp23017_handle_t *p_dev)
{
    MCP23017_CHECK(p_dev != NULL && *p_dev != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    esp_err_t ret = esp_io_expander_del(*p_dev);

    if (ret == ESP_OK) {
        *p_dev = NULL;
    }

    return ret;
}

esp_err_t mcp23017_write(mcp23017_handle_t dev, uint8_t reg_start_addr,
                         uint8_t reg_num, uint8_t *data_buf)
{
    MCP23017_CHECK(dev != NULL && data_buf != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    esp_err_t ret = ESP_FAIL;
    mcp23017_dev_t *p_device = mcp23017_from_handle(dev);

    for (size_t i = 0; i < reg_num; i++) {
        uint8_t write_buf[] = { reg_start_addr + i, data_buf[i] };
        ret = i2c_master_transmit(p_device->i2c_dev, write_buf, sizeof(write_buf), I2C_TIMEOUT_MS);

        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t mcp23017_read(mcp23017_handle_t dev, uint8_t reg_start_addr,
                        uint8_t reg_num, uint8_t *data_buf)
{
    MCP23017_CHECK(dev != NULL && data_buf != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    esp_err_t ret = ESP_FAIL;
    mcp23017_dev_t *p_device = mcp23017_from_handle(dev);

    for (size_t i = 0; i < reg_num; i++) {
        uint8_t reg_addr = reg_start_addr + i;
        ret = i2c_master_transmit_receive(p_device->i2c_dev, &reg_addr, sizeof(reg_addr), &data_buf[i],
                                          sizeof(data_buf[i]), I2C_TIMEOUT_MS);

        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t mcp23017_set_pullup(mcp23017_handle_t dev, uint16_t pins)
{
    return write_pullup_en_reg(dev, pins);
}

esp_err_t mcp23017_interrupt_en(mcp23017_handle_t dev, uint16_t pins,
                                bool intr_mode, uint16_t defaultValue)
{
    MCP23017_CHECK(dev != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    mcp23017_dev_t *p_device = mcp23017_from_handle(dev);

    //write register REG_GPINTENA(pins) REG_GPINTENB(pins) DEFVALA(0) DEFVALB(0) INTCONA(0) INTCONB(0)
    uint8_t data[] = { MCP23017_PORT_A_BYTE(pins), MCP23017_PORT_B_BYTE(pins) };

    if (!intr_mode) {
        uint8_t data1[] = { 0, 0, 0, 0 };
        esp_err_t ret = mcp23017_write(dev, MCP23017_REG_DEFVALA, sizeof(data1), data1);

        if (ret != ESP_OK) {
            return ret;
        }
    } else {
        uint8_t data1[] = { MCP23017_PORT_A_BYTE(defaultValue),
                            MCP23017_PORT_B_BYTE(defaultValue), MCP23017_PORT_A_BYTE(pins),
                            MCP23017_PORT_B_BYTE(pins)
                          };
        esp_err_t ret = mcp23017_write(dev, MCP23017_REG_DEFVALA, sizeof(data1), data1);

        if (ret != ESP_OK) {
            return ret;
        }
    }

    esp_err_t ret = mcp23017_write(dev, MCP23017_REG_GPINTENA, sizeof(data), data);
    if (ret != ESP_OK) {
        return ret;
    }

    p_device->intEnabledPins = p_device->intEnabledPins | pins;
    return ESP_OK;
}

esp_err_t mcp23017_interrupt_disable(mcp23017_handle_t dev, uint16_t pins)
{
    MCP23017_CHECK(dev != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    mcp23017_dev_t *p_device = mcp23017_from_handle(dev);
    //write register REG_GPINTENA(pins) REG_GPINTENB(pins) DEFVALA(0) DEFVALB(0) INTCONA(0) INTCONB(0)
    uint8_t data[] = { MCP23017_PORT_A_BYTE(p_device->intEnabledPins & ~pins),
                       MCP23017_PORT_B_BYTE(p_device->intEnabledPins & ~pins)
                     };

    esp_err_t ret = mcp23017_write(dev, MCP23017_REG_GPINTENA, sizeof(data), data);
    if (ret != ESP_OK) {
        return ret;
    }

    p_device->intEnabledPins = p_device->intEnabledPins & ~pins;
    return ESP_OK;
}

esp_err_t mcp23017_set_interrupt_polarity(mcp23017_handle_t dev,
                                          mcp23017_gpio_port_t gpio, uint8_t chLevel)
{
    MCP23017_CHECK(dev != NULL, "invalid arg", ESP_ERR_INVALID_ARG)

    uint8_t getIOCON = {
        (gpio == MCP23017_GPIOA) ?
        MCP23017_REG_IOCONA : MCP23017_REG_IOCONB
    };
    uint8_t ioCONValue[] = { 0, 0 };
    esp_err_t ret = mcp23017_read(dev, getIOCON, sizeof(ioCONValue), ioCONValue);

    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t data[] = { 0 };

    if (chLevel) {
        data[0] = *ioCONValue | MCP23017_IOCON_INTPOL;
    } else {
        data[0] = *ioCONValue & ~MCP23017_IOCON_INTPOL;
    }

    return mcp23017_write(dev, getIOCON, sizeof(data), data);
}

esp_err_t mcp23017_set_seque_mode(mcp23017_handle_t dev, uint8_t isSeque)
{
    MCP23017_CHECK(dev != NULL, "invalid arg", ESP_ERR_INVALID_ARG)

    uint8_t getIOCON = { MCP23017_REG_IOCONA };
    uint8_t ioCONValue[] = { 0, 0 };
    esp_err_t ret = mcp23017_read(dev, getIOCON, sizeof(ioCONValue), ioCONValue);

    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t data[] = { 0 };

    if (isSeque) {
        data[0] = *ioCONValue | MCP23017_IOCON_SEQOP;
    } else {
        data[0] = *ioCONValue & ~MCP23017_IOCON_SEQOP;
    }

    return mcp23017_write(dev, MCP23017_REG_IOCONA, sizeof(data), data);
}

esp_err_t mcp23017_mirror_interrupt(mcp23017_handle_t dev, uint8_t mirror,
                                    mcp23017_gpio_port_t gpio)
{
    MCP23017_CHECK(dev != NULL, "invalid arg", ESP_ERR_INVALID_ARG)

    uint8_t getIOCON = {
        (gpio == MCP23017_GPIOA) ?
        MCP23017_REG_IOCONA : MCP23017_REG_IOCONB
    };
    uint8_t ioCONValue[] = { 0, 0 };
    esp_err_t ret = mcp23017_read(dev, getIOCON, sizeof(ioCONValue), ioCONValue);

    if (ret != ESP_OK) {
        return ret;
    }

    // Now munge the MIRROR bit and write IOCON back out
    uint8_t data[] = { 0 };

    if (mirror) {
        data[0] = *ioCONValue | MCP23017_IOCON_MIRROR;
    } else {
        data[0] = *ioCONValue & ~MCP23017_IOCON_MIRROR;
    }

    return mcp23017_write(dev, getIOCON, sizeof(data), data);
}

esp_err_t mcp23017_set_io_dir(mcp23017_handle_t dev, uint8_t value,
                              mcp23017_gpio_port_t gpio)
{
    MCP23017_CHECK(dev != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    mcp23017_dev_t *p_device = mcp23017_from_handle(dev);
    esp_err_t ret = mcp23017_write(dev,
                                   (gpio == MCP23017_GPIOA) ?
                                   MCP23017_REG_IODIRA : MCP23017_REG_IODIRB, 1, &value);

    if (ret == ESP_OK) {
        p_device->direction = mcp23017_update_port_value(p_device->direction, value, gpio);
    }

    return ret;
}

esp_err_t mcp23017_write_io(mcp23017_handle_t dev, uint8_t value,
                            mcp23017_gpio_port_t gpio)
{
    MCP23017_CHECK(dev != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    mcp23017_dev_t *p_device = mcp23017_from_handle(dev);
    esp_err_t ret = mcp23017_write(dev,
                                   (gpio == MCP23017_GPIOA) ? MCP23017_REG_GPIOA : MCP23017_REG_GPIOB, 1, &value);

    if (ret == ESP_OK) {
        p_device->output = mcp23017_update_port_value(p_device->output, value, gpio);
    }

    return ret;
}

uint8_t mcp23017_read_io(mcp23017_handle_t dev, mcp23017_gpio_port_t gpio)
{
    MCP23017_CHECK(dev != NULL, "invalid arg", 0)

    uint8_t data = 0;
    mcp23017_read(dev,
                  (gpio == MCP23017_GPIOA) ? MCP23017_REG_GPIOA : MCP23017_REG_GPIOB,
                  1, &data);
    return data;
}

uint16_t mcp23017_get_int_pin(mcp23017_handle_t dev)
{
    MCP23017_CHECK(dev != NULL, "invalid arg", 0)
    mcp23017_dev_t *p_device = mcp23017_from_handle(dev);
    uint16_t pinValues = 0;

    if (p_device->intEnabledPins != 0) {
        uint8_t intPins[2] = { 0 };
        mcp23017_read(dev, MCP23017_REG_INTCAPA, sizeof(intPins), intPins);
        pinValues = MCP23017_PORT_AB_WORD(intPins);
    }

    uint8_t gpioPins[2] = { 0 };

    if (mcp23017_read(dev, MCP23017_REG_GPIOA, sizeof(gpioPins),
                      gpioPins) != ESP_OK) {
        return 0;   // ESP_FAIL would truncate to 0xFFFF (looks like all pins set)
    }

    uint16_t gpioValue = MCP23017_PORT_AB_WORD(gpioPins);
    pinValues |= (gpioValue & ~p_device->intEnabledPins); // Don't let current gpio values overwrite the intcap values

    return pinValues;
}

uint16_t mcp23017_get_int_flag(mcp23017_handle_t dev)
{
    MCP23017_CHECK(dev != NULL, "invalid arg", 0)
    mcp23017_dev_t *p_device = mcp23017_from_handle(dev);
    uint8_t intfpins[2] = { 0 };
    uint16_t pinIntfValues = 0;
    mcp23017_read(dev, MCP23017_REG_INTFA, sizeof(intfpins), intfpins);
    pinIntfValues = MCP23017_PORT_AB_WORD(intfpins);
    return pinIntfValues & p_device->intEnabledPins;
}

esp_err_t mcp23017_check_present(mcp23017_handle_t dev)
{
    MCP23017_CHECK(dev != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    uint8_t lastregValue = 0x00;
    uint8_t regValue = 0x00;
    uint8_t data = 0xAA;

    // Save original value first so a bus failure can't corrupt INTCONA
    esp_err_t ret = mcp23017_read(dev, MCP23017_REG_INTCONA, 1, &lastregValue);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = mcp23017_write(dev, MCP23017_REG_INTCONA, 1, &data);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = mcp23017_read(dev, MCP23017_REG_INTCONA, 1, &regValue);
    // Restore original value regardless of the read-back result
    esp_err_t restore = mcp23017_write(dev, MCP23017_REG_INTCONA, 1, &lastregValue);
    if (ret != ESP_OK) {
        return ret;
    }
    if (restore != ESP_OK) {
        return restore;
    }
    return (regValue == 0xAA) ? ESP_OK : ESP_FAIL;
}

static esp_err_t read_input_reg(esp_io_expander_handle_t handle, uint32_t *value)
{
    MCP23017_CHECK(handle != NULL && value != NULL, "invalid arg", ESP_ERR_INVALID_ARG)

    uint8_t data[2] = { 0 };
    esp_err_t ret = mcp23017_read(handle, MCP23017_REG_GPIOA, sizeof(data), data);

    if (ret == ESP_OK) {
        *value = MCP23017_PORT_AB_WORD(data);
    }

    return ret;
}

static esp_err_t write_output_reg(esp_io_expander_handle_t handle, uint32_t value)
{
    MCP23017_CHECK(handle != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    mcp23017_dev_t *p_device = mcp23017_from_handle(handle);
    uint8_t data[] = { MCP23017_PORT_A_BYTE(value), MCP23017_PORT_B_BYTE(value) };
    esp_err_t ret = mcp23017_write(handle, MCP23017_REG_OLATA, sizeof(data), data);

    if (ret == ESP_OK) {
        p_device->output = value & 0xFFFF;
    }

    return ret;
}

static esp_err_t read_output_reg(esp_io_expander_handle_t handle, uint32_t *value)
{
    MCP23017_CHECK(handle != NULL && value != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    mcp23017_dev_t *p_device = mcp23017_from_handle(handle);

    *value = p_device->output;
    return ESP_OK;
}

static esp_err_t write_direction_reg(esp_io_expander_handle_t handle, uint32_t value)
{
    MCP23017_CHECK(handle != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    mcp23017_dev_t *p_device = mcp23017_from_handle(handle);
    uint8_t data[] = { MCP23017_PORT_A_BYTE(value), MCP23017_PORT_B_BYTE(value) };
    esp_err_t ret = mcp23017_write(handle, MCP23017_REG_IODIRA, sizeof(data), data);

    if (ret == ESP_OK) {
        p_device->direction = value & 0xFFFF;
    }

    return ret;
}

static esp_err_t read_direction_reg(esp_io_expander_handle_t handle, uint32_t *value)
{
    MCP23017_CHECK(handle != NULL && value != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    mcp23017_dev_t *p_device = mcp23017_from_handle(handle);

    *value = p_device->direction;
    return ESP_OK;
}

static esp_err_t write_pullup_en_reg(esp_io_expander_handle_t handle, uint32_t value)
{
    MCP23017_CHECK(handle != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    mcp23017_dev_t *p_device = mcp23017_from_handle(handle);
    uint16_t pullup = value & 0xFFFF;
    uint8_t data[] = { MCP23017_PORT_A_BYTE(pullup), MCP23017_PORT_B_BYTE(pullup) };
    esp_err_t ret = mcp23017_write(handle, MCP23017_REG_GPPUA, sizeof(data), data);

    if (ret == ESP_OK) {
        p_device->pullup = pullup;
    }

    return ret;
}

static esp_err_t read_pullup_en_reg(esp_io_expander_handle_t handle, uint32_t *value)
{
    MCP23017_CHECK(handle != NULL && value != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    mcp23017_dev_t *p_device = mcp23017_from_handle(handle);

    *value = p_device->pullup;
    return ESP_OK;
}

static esp_err_t write_pullup_sel_reg(esp_io_expander_handle_t handle, uint32_t value)
{
    MCP23017_CHECK(handle != NULL, "invalid arg", ESP_ERR_INVALID_ARG)

    // MCP23017 only has pull-up resistors; the pull direction is not selectable.
    MCP23017_CHECK(value == MCP23017_PULLUP_SEL_DEFAULT_VAL, "pull-down is not supported", ESP_ERR_NOT_SUPPORTED)
    return ESP_OK;
}

static esp_err_t read_pullup_sel_reg(esp_io_expander_handle_t handle, uint32_t *value)
{
    MCP23017_CHECK(handle != NULL && value != NULL, "invalid arg", ESP_ERR_INVALID_ARG)

    // Use zero as the virtual pull-up selection so pull-down requests reach the
    // write callback and can be rejected without affecting pull-up or pull-none.
    *value = MCP23017_PULLUP_SEL_DEFAULT_VAL;
    return ESP_OK;
}

static esp_err_t reset(esp_io_expander_handle_t handle)
{
    MCP23017_CHECK(handle != NULL, "invalid arg", ESP_ERR_INVALID_ARG)

    uint8_t iocon = 0;
    uint8_t int_disable[] = { 0, 0 };
    uint8_t int_default[] = { 0, 0, 0, 0 };
    esp_err_t ret = mcp23017_write(handle, MCP23017_REG_IOCONA, 1, &iocon);

    MCP23017_CHECK(ret == ESP_OK, "write IOCONA failed", ret)
    ret = mcp23017_write(handle, MCP23017_REG_IOCONB, 1, &iocon);
    MCP23017_CHECK(ret == ESP_OK, "write IOCONB failed", ret)
    ret = mcp23017_write(handle, MCP23017_REG_GPINTENA, sizeof(int_disable), int_disable);
    MCP23017_CHECK(ret == ESP_OK, "write interrupt enable failed", ret)
    ret = mcp23017_write(handle, MCP23017_REG_DEFVALA, sizeof(int_default), int_default);
    MCP23017_CHECK(ret == ESP_OK, "write interrupt default failed", ret)
    mcp23017_dev_t *p_device = mcp23017_from_handle(handle);
    ret = write_pullup_en_reg(handle, MCP23017_PULLUP_DEFAULT_VAL);
    MCP23017_CHECK(ret == ESP_OK, "write pullup failed", ret)
    ret = write_direction_reg(handle, MCP23017_DIR_REG_DEFAULT_VAL);
    MCP23017_CHECK(ret == ESP_OK, "write direction failed", ret)
    ret = write_output_reg(handle, MCP23017_OUT_REG_DEFAULT_VAL);
    MCP23017_CHECK(ret == ESP_OK, "write output failed", ret)

    p_device->intEnabledPins = 0;

    return ESP_OK;
}

static esp_err_t del(esp_io_expander_handle_t handle)
{
    MCP23017_CHECK(handle != NULL, "invalid arg", ESP_ERR_INVALID_ARG)
    mcp23017_dev_t *p_device = mcp23017_from_handle(handle);

    esp_err_t ret = i2c_master_bus_rm_device(p_device->i2c_dev);
    MCP23017_CHECK(ret == ESP_OK, "delete i2c device failed", ret)
    free(p_device);
    return ESP_OK;
}
