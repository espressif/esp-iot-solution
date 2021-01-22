// Copyright 2020-2021 Espressif Systems (Shanghai) Co. Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _MCP23017_H_
#define _MCP23017_H_

#include "i2c_bus.h"

#define MCP23017_I2C_ADDRESS_DEFAULT   (0x20)           /*!< 0100A2A1A0+R/W */

#define WRITE_BIT                       I2C_MASTER_WRITE/*!< I2C master write */
#define READ_BIT                        I2C_MASTER_READ /*!< I2C master read */
#define ACK_CHECK_EN                    0x1             /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                   0x0             /*!< I2C master will not check ack from slave */
#define ACK_VAL                         0x0             /*!< I2C ack value */
#define NACK_VAL                        0x1             /*!< I2C nack value */

typedef void* mcp23017_handle_t;                        /*!< handle of mcp23017 */

typedef enum {
    MCP23017_GPIOA = 0x00, /*!< GPIO Port A */
    MCP23017_GPIOB,        /*!< GPIO Port B */
} mcp23017_gpio_port_t; /*!< GPIO Port Type */

typedef enum {
    MCP23017_NOPIN = 0x0000,  /*!< GPIO Pin Num */
    MCP23017_PIN0 = 0x0001,  /*!< GPIO Pin Num */
    MCP23017_PIN1 = 0x0002,  /*!< GPIO Pin Num */
    MCP23017_PIN2 = 0x0004,  /*!< GPIO Pin Num */
    MCP23017_PIN3 = 0x0008,  /*!< GPIO Pin Num */
    MCP23017_PIN4 = 0x0010,  /*!< GPIO Pin Num */
    MCP23017_PIN5 = 0x0020,  /*!< GPIO Pin Num */
    MCP23017_PIN6 = 0x0040,  /*!< GPIO Pin Num */
    MCP23017_PIN7 = 0x0080,  /*!< GPIO Pin Num */
    MCP23017_PIN8 = 0x0100,  /*!< GPIO Pin Num */
    MCP23017_PIN9 = 0x0200,  /*!< GPIO Pin Num */
    MCP23017_PIN10 = 0x0400,  /*!< GPIO Pin Num */
    MCP23017_PIN11 = 0x0800,  /*!< GPIO Pin Num */
    MCP23017_PIN12 = 0x1000,  /*!< GPIO Pin Num */
    MCP23017_PIN13 = 0x2000,  /*!< GPIO Pin Num */
    MCP23017_PIN14 = 0x4000,  /*!< GPIO Pin Num */
    MCP23017_PIN15 = 0x8000,  /*!< GPIO Pin Num */
    MCP23017_ALLPINS = 0xFFFF,  /*!< GPIO Pin Num */
} mcp23017_pin_t; /*!< GPIO Pin Num Type, include all ports*/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create a MCP23017 device
 *
 * @param bus device handle of i2c_bus
 * @param dev_addr device address
 *
 * @return
 *     - mcp23017_handle_t return mcp23017 device handle, NULL is failed.
 */
mcp23017_handle_t mcp23017_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief Delete the MCP23017 device
 *
 * @param p_dev pointer to the device handle of MCP23017
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t mcp23017_delete(mcp23017_handle_t *p_dev);

/**
 * @brief Set MCP23017 interrupt pin,
 *          Only in GPIO work in INPUT mode,
 *              Default:compare last value,
 *
 * @param dev device handle of MCP23017
 * @param pins pin of interrupt
 * @param intr_mode 0: compared against previous, 1: compared against DEFVAL register.
 * @param defaultValue pins default level
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t mcp23017_interrupt_en(mcp23017_handle_t dev, uint16_t pins,
        bool intr_mode, uint16_t defaultValue);

/**
 * @brief delete MCP23017 interrupt pin,
 *
 * @param dev device handle of MCP23017
 * @param pins pin of interrupt
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t mcp23017_interrupt_disable(mcp23017_handle_t dev, uint16_t pins);

/**
 * @brief Set the polarity of the INT output pin
 *
 * @param dev device handle of MCP23017
 * @param gpio pin of interrupt
 * @param chLevel interrupt polarity
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t mcp23017_set_interrupt_polarity(mcp23017_handle_t dev,
        mcp23017_gpio_port_t gpio, uint8_t chLevel);

/**
 * @brief Sequential operation mode set
 *
 * @param dev device handle of MCP23017
 * @param isSeque 1:Prohibit sequential operation, the address pointer is not incremented
 *                  0:Enable sequential operation, address pointer increment
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t mcp23017_set_seque_mode(mcp23017_handle_t dev, uint8_t isSeque);

/**
 * @brief Set MCP23017 pin pullup
 *
 * @param dev device handle of MCP23017
 * @param pins pin of pullup
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t mcp23017_set_pullup(mcp23017_handle_t dev, uint16_t pins);

/**
 * @brief gets the interrupt capture values for pins with interrupts enabled,
 *          and gpio values for the ones that aren't.
 *          clear interrupt flag
 *
 * @param dev device handle of MCP23017
 *
 * @return 
 *     - uint16_t value of interrupt pin
 */
uint16_t mcp23017_get_int_pin(mcp23017_handle_t dev);

/**
 * @brief get interrupt flag of GPIO
 *
 * @param dev device handle of MCP23017
 *
 * @return
 *     - uint16_t value of GPIO interrupt flag
 */
uint16_t mcp23017_get_int_flag(mcp23017_handle_t dev);

/**
 * @brief Check device Present
 *
 * @param dev device handle of MCP23017
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t mcp23017_check_present(mcp23017_handle_t dev);

/**
 * @brief Set MCP23017 GPIOA/GPIOB Mirror:1:Interrupt inconnect; 0:not connect.
 *
 * @param dev device handle of MCP23017
 * @param mirror whether set up mirror interrupt
 * @param gpio select GPIOA/GPIOB
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t mcp23017_mirror_interrupt(mcp23017_handle_t dev, uint8_t mirror,
        mcp23017_gpio_port_t gpio);

/**
 * @brief write output value of GPIOA,(work in output)
 *
 * @param dev device handle of MCP23017
 * @param value value of GPIOX
 * @param gpio GPIO of mcp23017
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t mcp23017_write_io(mcp23017_handle_t dev, uint8_t value,
        mcp23017_gpio_port_t gpio);

/**
 * @brief read value of REG_GPIOA/REG_GPIOB;Reflects the logic level on pin <7: 0>
 *
 * @param dev device handle of MCP23017
 * @param gpio GPIO of mcp23017
 *
 * @return
 *     - uint8_t value of level
 */
uint8_t mcp23017_read_io(mcp23017_handle_t dev, mcp23017_gpio_port_t gpio);

/**
 * @brief set Direction of GPIOA;Set the logic level on pin <7: 0>, 0 - output, 1 - input,
 *
 * @param dev device handle of MCP23017
 * @param value value of GPIOX
 * @param gpio GPIO of mcp23017
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t mcp23017_set_io_dir(mcp23017_handle_t dev, uint8_t value,
        mcp23017_gpio_port_t gpio);

#ifdef __cplusplus
}
#endif


#endif
