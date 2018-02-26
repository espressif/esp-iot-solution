// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <stdio.h>
#include "driver/i2c.h"
#include "iot_mcp23017.h"
#include "iot_i2c_bus.h"

#define MCP23017_PORT_A_BYTE(x)         (x & 0xFF)                      //get pin of GPIOA
#define MCP23017_PORT_B_BYTE(x)         (x >> 8)                        //get pin of GPIOB
#define MCP23017_PORT_AB_WORD(buff)     (buff[0] | (buff[1] << 8))      //get pin of GPIOA and pin of GPIOB

typedef enum {
    MCP23017_REG_IODIRA = 0,
    MCP23017_REG_IODIRB,
    MCP23017_REG_IPOLA,
    MCP23017_REG_IPOLB,
    MCP23017_REG_GPINTENA,
    MCP23017_REG_GPINTENB,
    MCP23017_REG_DEFVALA,
    MCP23017_REG_DEFVALB,
    MCP23017_REG_INTCONA,
    MCP23017_REG_INTCONB,
    MCP23017_REG_IOCONA,
    MCP23017_REG_IOCONB,
    MCP23017_REG_GPPUA,
    MCP23017_REG_GPPUB,
    MCP23017_REG_INTFA,
    MCP23017_REG_INTFB,
    MCP23017_REG_INTCAPA,
    MCP23017_REG_INTCAPB,
    MCP23017_REG_GPIOA,
    MCP23017_REG_GPIOB,
    MCP23017_REG_OLATA,
    MCP23017_REG_OLATB,
} mcp23017_reg_t;

typedef enum {
    MCP23017_IOCON_UNIMPLEMENTED = 0,
    MCP23017_IOCON_INTPOL = 2,
    MCP23017_IOCON_ODR = 4,
    MCP23017_IOCON_HAEN = 8,
    MCP23017_IOCON_DISSLW = 16,
    MCP23017_IOCON_SEQOP = 32,
    MCP23017_IOCON_MIRROR = 64,
    MCP23017_IOCON_BANK = 128,
} mcp23017_iocon_t;

typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
    uint16_t intEnabledPins;			//pin of interrupt
} mcp23017_dev_t;

esp_err_t iot_mcp23017_write_byte(mcp23017_handle_t dev, uint8_t reg_addr,
        uint8_t data)
{
    esp_err_t ret;
    mcp23017_dev_t* device = (mcp23017_dev_t*) dev;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
            ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_mcp23017_write(mcp23017_handle_t dev, uint8_t reg_start_addr,
        uint8_t reg_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    i2c_cmd_handle_t cmd;
    esp_err_t ret = ESP_FAIL;
    mcp23017_dev_t* device = (mcp23017_dev_t*) dev;
    if (data_buf != NULL) {
        cmd = i2c_cmd_link_create();
        for (i = 0; i < reg_num; i++) {
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
                    ACK_CHECK_EN);
            i2c_master_write_byte(cmd, reg_start_addr + i, ACK_CHECK_EN);
            i2c_master_write_byte(cmd, data_buf[i], ACK_CHECK_EN);
        }
        ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
    }
    return ret;
}

esp_err_t iot_mcp23017_read_byte(mcp23017_handle_t dev, uint8_t reg,
        uint8_t *data)
{
    esp_err_t ret = ESP_FAIL;
    mcp23017_dev_t* device = (mcp23017_dev_t*) dev;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
            ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) {
        return ret;
    }

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | READ_BIT,
            ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data, NACK_VAL);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_mcp23017_read(mcp23017_handle_t dev, uint8_t reg_start_addr,
        uint8_t reg_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    i2c_cmd_handle_t cmd;
    esp_err_t ret = ESP_FAIL;
    mcp23017_dev_t* device = (mcp23017_dev_t*) dev;
    if (data_buf != NULL) {
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
                ACK_CHECK_EN);
        i2c_master_write_byte(cmd, reg_start_addr, ACK_CHECK_EN);
        i2c_master_stop(cmd);
        ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_FAIL) {
            return ret;
        }

        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (device->dev_addr << 1) | READ_BIT,
                ACK_CHECK_EN);
        for (i = 0; i < reg_num - 1; i++) {
            i2c_master_read_byte(cmd, &data_buf[i], ACK_VAL);
        }
        i2c_master_read_byte(cmd, &data_buf[i], NACK_VAL);
        i2c_master_stop(cmd);
        ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
    }
    return ret;
}

mcp23017_handle_t iot_mcp23017_create(i2c_bus_handle_t bus, uint16_t dev_addr)
{
    mcp23017_dev_t* device = (mcp23017_dev_t*) calloc(1,
            sizeof(mcp23017_dev_t));
    device->bus = bus;
    device->dev_addr = dev_addr;
    return (mcp23017_handle_t) device;
}

esp_err_t iot_mcp23017_delete(mcp23017_handle_t dev, bool del_bus)
{
    mcp23017_dev_t* device = (mcp23017_dev_t*) dev;
    if (del_bus) {
        iot_i2c_bus_delete(device->bus);
        device->bus = NULL;
    }
    free(device);
    return ESP_OK;
}

esp_err_t iot_mcp23017_set_pullup(mcp23017_handle_t dev, uint16_t pins)
{
    uint8_t data[] = { MCP23017_PORT_A_BYTE(pins), MCP23017_PORT_B_BYTE(pins) };
    return iot_mcp23017_write(dev, MCP23017_REG_GPIOA, sizeof(data), data); //set REG_GPIOA(); REG_GPIOB();
}

esp_err_t iot_mcp23017_interrupt_en(mcp23017_handle_t dev, uint16_t pins,
bool isDefault, uint16_t defaultValue)
{
    mcp23017_dev_t* device = (mcp23017_dev_t*) dev;
    //write register REG_GPINTENA(pins) REG_GPINTENB(pins) DEFVALA(0) DEFVALB(0) INTCONA(0) INTCONB(0)
    uint8_t data[] = { MCP23017_PORT_A_BYTE(pins), MCP23017_PORT_B_BYTE(pins) };
    if (!isDefault) {
        uint8_t data1[] = { 0, 0, 0, 0 };
        if (iot_mcp23017_write(dev, MCP23017_REG_DEFVALA, sizeof(data1),
                data1) == ESP_FAIL) {
            return ESP_FAIL;
        }
    } else {
        uint8_t data1[] = { MCP23017_PORT_A_BYTE(defaultValue),
                MCP23017_PORT_B_BYTE(defaultValue), MCP23017_PORT_A_BYTE(pins),
                MCP23017_PORT_B_BYTE(pins) };
        if (iot_mcp23017_write(dev, MCP23017_REG_DEFVALA, sizeof(data1),
                data1) == ESP_FAIL) {
            return ESP_FAIL;
        }
    }
    if (iot_mcp23017_write(dev, MCP23017_REG_GPINTENA, sizeof(data),
            data) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->intEnabledPins = device->intEnabledPins | pins;
    return ESP_OK;
}

esp_err_t iot_mcp23017_interrupt_disable(mcp23017_handle_t dev, uint16_t pins)
{
    mcp23017_dev_t* device = (mcp23017_dev_t*) dev;
    //write register REG_GPINTENA(pins) REG_GPINTENB(pins) DEFVALA(0) DEFVALB(0) INTCONA(0) INTCONB(0)
    uint8_t data[] = { MCP23017_PORT_A_BYTE(device->intEnabledPins & ~pins),
            MCP23017_PORT_B_BYTE(device->intEnabledPins & ~pins) };
    if (iot_mcp23017_write(dev, MCP23017_REG_GPINTENA, sizeof(data),
            data) == ESP_FAIL) {
        return ESP_FAIL;
    }
    device->intEnabledPins = device->intEnabledPins & ~pins;
    return ESP_OK;
}

esp_err_t iot_mcp23017_set_interrupt_polarity(mcp23017_handle_t dev,
        mcp23017_gpio_t gpio, uint8_t chLevel)
{
    uint8_t getIOCON = {
            (gpio == MCP23017_GPIOA ) ?
                    MCP23017_REG_IOCONA : MCP23017_REG_IOCONB };
    uint8_t ioCONValue[] = { 0, 0 };
    if (iot_mcp23017_read(dev, getIOCON, sizeof(ioCONValue),
            ioCONValue) == ESP_FAIL) {
        return ESP_FAIL;
    }
    uint8_t setIOCON[] = {
            (gpio == MCP23017_GPIOA ) ?
                    MCP23017_REG_IOCONA : MCP23017_REG_IOCONB, 0 };
    if (chLevel) {
        setIOCON[1] = *ioCONValue | MCP23017_IOCON_INTPOL;
    } else {
        setIOCON[1] = *ioCONValue & ~MCP23017_IOCON_INTPOL;
    }
    return iot_mcp23017_write_byte(dev, setIOCON[0], setIOCON[1]);
}

esp_err_t iot_mcp23017_set_seque_mode(mcp23017_handle_t dev, uint8_t isSeque)
{
    uint8_t getIOCON = { MCP23017_REG_IOCONA };
    uint8_t ioCONValue[] = { 0, 0 };
    if (iot_mcp23017_read(dev, getIOCON, sizeof(ioCONValue),
            ioCONValue) == ESP_FAIL) {
        return ESP_FAIL;
    }

    uint8_t setIOCON[] = { MCP23017_REG_IOCONA, 0 };
    if (isSeque) {
        setIOCON[1] = *ioCONValue | MCP23017_IOCON_SEQOP;
    } else {
        setIOCON[1] = *ioCONValue & ~MCP23017_IOCON_SEQOP;
    }
    return iot_mcp23017_write_byte(dev, setIOCON[0], setIOCON[1]);
}

esp_err_t iot_mcp23017_mirror_interrupt(mcp23017_handle_t dev, uint8_t mirror,
        mcp23017_gpio_t gpio)
{
    uint8_t getIOCON = {
            (gpio == MCP23017_GPIOA ) ?
                    MCP23017_REG_IOCONA : MCP23017_REG_IOCONB };
    uint8_t ioCONValue[] = { 0, 0 }; // STM32 i2c minimum read is 2 bytes
    if (iot_mcp23017_read(dev, getIOCON, sizeof(ioCONValue),
            ioCONValue) == ESP_FAIL) {
        return ESP_FAIL;
    }
    // Now munge the MIRROR bit and write IOCON back out
    uint8_t setIOCON[] = {
            (gpio == MCP23017_GPIOA ) ?
                    MCP23017_REG_IOCONA : MCP23017_REG_IOCONB, 0 };
    if (mirror) {
        setIOCON[1] = *ioCONValue | MCP23017_IOCON_MIRROR;
    } else {
        setIOCON[1] = *ioCONValue & ~MCP23017_IOCON_MIRROR;
    }
    return iot_mcp23017_write_byte(dev, setIOCON[0], setIOCON[1]);
}

esp_err_t iot_mcp23017_set_io_dir(mcp23017_handle_t dev, uint8_t value,
        mcp23017_gpio_t gpio)
{
    return iot_mcp23017_write_byte(dev,
            (gpio == MCP23017_GPIOA ) ?
                    MCP23017_REG_IODIRA : MCP23017_REG_IODIRB, value);
}

esp_err_t iot_mcp23017_write_io(mcp23017_handle_t dev, uint8_t value,
        mcp23017_gpio_t gpio)
{
    return iot_mcp23017_write_byte(dev,
            (gpio == MCP23017_GPIOA ) ? MCP23017_REG_GPIOA : MCP23017_REG_GPIOB,
            value);
}

uint8_t iot_mcp23017_read_io(mcp23017_handle_t dev, mcp23017_gpio_t gpio)
{
    uint8_t data = 0;
    iot_mcp23017_read_byte(dev,
            (gpio == MCP23017_GPIOA ) ? MCP23017_REG_GPIOA : MCP23017_REG_GPIOB,
            &data);
    return data;
}

uint16_t iot_mcp23017_get_int_pin(mcp23017_handle_t dev)
{
    mcp23017_dev_t* device = (mcp23017_dev_t*) dev;
    uint16_t pinValues = 0;

    if (device->intEnabledPins != 0) {
        uint8_t getIntPins[] = { MCP23017_REG_INTCAPA };
        uint8_t intPins[2] = { 0 };
        iot_mcp23017_read(device, getIntPins[0], sizeof(intPins), intPins);
        pinValues = MCP23017_PORT_AB_WORD(intPins);
    }

    uint8_t getGPIOPins[] = { MCP23017_REG_GPIOA };
    uint8_t gpioPins[2] = { 0 };
    if (iot_mcp23017_read(device, getGPIOPins[0], sizeof(gpioPins),
            gpioPins)==ESP_FAIL) return ESP_FAIL;
    uint16_t gpioValue = MCP23017_PORT_AB_WORD(gpioPins);
    pinValues |= (gpioValue & ~device->intEnabledPins); // Don't let current gpio values overwrite the intcap values

    return pinValues;
}

uint16_t iot_mcp23017_get_int_flag(mcp23017_handle_t dev)
{
    mcp23017_dev_t* device = (mcp23017_dev_t*) dev;
    uint8_t intfpins[2] = { 0 };
    uint8_t getIntPins[] = { MCP23017_REG_INTFA };
    uint16_t pinIntfValues = 0;
    iot_mcp23017_read(dev, getIntPins[0], sizeof(intfpins), intfpins);
    pinIntfValues = MCP23017_PORT_AB_WORD(intfpins);
    return pinIntfValues & device->intEnabledPins;
}

esp_err_t iot_mcp23017_check_present(mcp23017_handle_t dev)
{
    uint8_t lastregValue = 0x00;
    uint8_t regValue = 0x00;
    iot_mcp23017_read_byte(dev, MCP23017_REG_INTCONA, &lastregValue);
    iot_mcp23017_write_byte(dev, MCP23017_REG_INTCONA, 0xAA);
    iot_mcp23017_read_byte(dev, MCP23017_REG_INTCONA, &regValue);
    iot_mcp23017_write_byte(dev, MCP23017_REG_INTCONA, lastregValue);
    return (regValue == 0xAA) ? ESP_OK : ESP_FAIL;
}

