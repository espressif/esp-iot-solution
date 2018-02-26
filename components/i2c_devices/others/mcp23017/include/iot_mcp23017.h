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
#ifndef _IOT_MCP23017_H_
#define _IOT_MCP23017_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/i2c.h"
#include "iot_i2c_bus.h"

#define MCP23017_GPIOA                 (uint8_t)0x00    //select GPIOA
#define MCP23017_GPIOB                 (uint8_t)0x01    //select GPIOB
#define MCP23017_I2C_ADDRESS_DEFAULT   (0x20)           //0100A2A1A0+R/W  // 7-bit i2c addresses MUST be shifted left 1 bit (chibios does this for us)

#define WRITE_BIT                       I2C_MASTER_WRITE/*!< I2C master write */
#define READ_BIT                        I2C_MASTER_READ /*!< I2C master read */
#define ACK_CHECK_EN                    0x1             /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                   0x0             /*!< I2C master will not check ack from slave */
#define ACK_VAL                         0x0             /*!< I2C ack value */
#define NACK_VAL                        0x1             /*!< I2C nack value */

typedef void* mcp23017_handle_t;                        /*handle of mcp23017*/
typedef uint8_t mcp23017_gpio_t;

typedef enum {
    MCP23017_NOPIN = 0x0000,
    MCP23017_PIN0 = 0x0001,
    MCP23017_PIN1 = 0x0002,
    MCP23017_PIN2 = 0x0004,
    MCP23017_PIN3 = 0x0008,
    MCP23017_PIN4 = 0x0010,
    MCP23017_PIN5 = 0x0020,
    MCP23017_PIN6 = 0x0040,
    MCP23017_PIN7 = 0x0080,
    MCP23017_PIN8 = 0x0100,
    MCP23017_PIN9 = 0x0200,
    MCP23017_PIN10 = 0x0400,
    MCP23017_PIN11 = 0x0800,
    MCP23017_PIN12 = 0x1000,
    MCP23017_PIN13 = 0x2000,
    MCP23017_PIN14 = 0x4000,
    MCP23017_PIN15 = 0x8000,
    MCP23017_ALLPINS = 0xFFFF,
} mcp23017_pin_t;

/**
 * @brief   Write a data on addr
 *
 * @param   dev object handle of mcp230017
 * @param   reg_addr register address
 * @param   data write data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_write_byte(mcp23017_handle_t dev, uint8_t reg_addr,
        uint8_t data);

/**
 * @brief   Write some data start addr
 *
 * @param   dev object handle of mcp230017
 * @param   reg_start_addr register address
 * @param   reg_num data length
 * @param   data_buf a pointer of will write data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_write(mcp23017_handle_t dev, uint8_t reg_start_addr,
        uint8_t reg_num, uint8_t *data_buf);

/**
 * @brief   Read a data on addr
 *
 * @param   dev object handle of mcp230017
 * @param   reg register address
 * @param   data pointer of save data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_read_byte(mcp23017_handle_t dev, uint8_t reg,
        uint8_t *data);

/**
 * @brief   Read some data start addr
 *
 * @param   dev object handle of mcp230017
 * @param   reg_start_addrregister address
 * @param   reg_num data length
 * @param   data_buf pointer of save data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_read(mcp23017_handle_t dev, uint8_t reg_start_addr,
        uint8_t reg_num, uint8_t *data_buf);

/**
 * @brief   Create MCP23017 handle_t
 *
 * @param   bus object handle of I2C
 * @param   dev_addr decice address
 *
 * @return
 *     - mcp23017_handle_t
 */
mcp23017_handle_t iot_mcp23017_create(i2c_bus_handle_t bus, uint16_t dev_addr);

/**
 * @brief   delete MCP23017 handle_t
 *
 * @param   dev object handle of MCP23017
 * @param   del_bus whether delete i2c bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_delete(mcp23017_handle_t dev, bool del_bus);

/**
 * @brief   Set MCP23017 interrupt pin,
 *          Only in GPIO work in INPUT mode,
 *              Default:compare last value,
 *
 * @param   dev object handle of MCP23017
 * @param   pins pin of interrupt
 * @param   isDefault pins level use default
 * @param   defaultValue pins default level
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_interrupt_en(mcp23017_handle_t dev, uint16_t pins,
        bool isDefault, uint16_t defaultValue);

/**
 * @brief delete MCP23017 interrupt pin,
 *
 * @param dev object handle of MCP23017
 * @param pins pin of interrupt
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_interrupt_disable(mcp23017_handle_t dev, uint16_t pins);

/**
 * @brief   Set the polarity of the INT output pin
 *
 * @param   dev object handle of MCP23017
 * @param   gpio pin of interrupt
 * @param   chLevel interrupt polarity
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_set_interrupt_polarity(mcp23017_handle_t dev,
        mcp23017_gpio_t gpio, uint8_t chLevel);

/**
 * @brief   Sequential operation mode set
 *
 * @param   dev object handle of MCP23017
 * @param   isSeque 1:Prohibit sequential operation, the address pointer is not incremented
 *                  0:Enable sequential operation, address pointer increment
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_set_seque_mode(mcp23017_handle_t dev, uint8_t isSeque);

/**
 * @brief   Set MCP23017 pin pullup
 *
 * @param   dev object handle of MCP23017
 * @param   pins pin of pullup
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_set_pullup(mcp23017_handle_t dev, uint16_t pins);

/**
 * @brief   gets the interrupt capture values for pins with interrupts enabled,
 *          and gpio values for the ones that aren't.
 *          clear interrupt flag
 *
 * @param   dev object handle of MCP23017
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
uint16_t iot_mcp23017_get_int_pin(mcp23017_handle_t dev);

/**
 * @brief   get interrupt flag of GPIO
 *
 * @param   dev object handle of MCP23017
 *
 * @return
 *     - value of GPIO interrupt flag
 */
uint16_t iot_mcp23017_get_int_flag(mcp23017_handle_t dev);

/**
 * @brief   Check device Present
 *
 * @param   dev object handle of MCP23017
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_check_present(mcp23017_handle_t dev);

/**
 * @brief   Set MCP23017 GPIOA/GPIOB Mirror:1:Interrupt inconnect; 0:not connect.
 *
 * @param   dev object handle of MCP23017
 * @param   mirror whether set up mirror interrupt
 * @param   gpio select GPIOA/GPIOB
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_mirror_interrupt(mcp23017_handle_t dev, uint8_t mirror,
        mcp23017_gpio_t gpio);

/**
 * @brief   write output value of GPIOA,(work in output)
 *
 * @param   dev object handle of MCP23017
 * @param   value value of GPIOX
 * @param   gpio GPIO of mcp23017
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_write_io(mcp23017_handle_t dev, uint8_t value,
        mcp23017_gpio_t gpio);

/**
 * @brief   read value of REG_GPIOA/REG_GPIOB;Reflects the logic level on pin <7: 0>
 *
 * @param   dev object handle of MCP23017
 * @param   gpio GPIO of mcp23017
 *
 * @return
 *     - uint8_t value of level
 */
uint8_t iot_mcp23017_read_io(mcp23017_handle_t dev, mcp23017_gpio_t gpio);

/**
 * @brief   set Direction of GPIOA;Set the logic level on pin <7: 0>
 *
 * @param   dev object handle of MCP23017
 * @param   value value of GPIOX
 * @param   gpio GPIO of mcp23017
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mcp23017_set_io_dir(mcp23017_handle_t dev, uint8_t value,
        mcp23017_gpio_t gpio);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/**
 * class of MCP23017  dev
 * simple usage:
 * CMCP23017 *MCP23017 = new CMCP23017(&i2c_bus);
 * MCP23017.read_port_a();
 * ......
 * delete(MCP23017);
 */
class CMCP23017
{
private:
    mcp23017_handle_t m_dev_handle;
    CI2CBus *bus;

    /**
     * prevent copy constructing
     */
    CMCP23017(const CMCP23017&);
    CMCP23017& operator =(const CMCP23017&);
public:
    /**
     * @brief   constructor of CMCP23017
     *
     * @param   p_i2c_bus pointer to CI2CBus object
     * @param   addr slave device address
     */
    CMCP23017(CI2CBus *p_i2c_bus, uint8_t addr = MCP23017_I2C_ADDRESS_DEFAULT);

    ~CMCP23017();

    /**
     * @brief   set Direction of GPIOA;Set the logic level on pin <7: 0>
     *
     * @param   value of GPIOA
     * @param   gpio GPIO of mcp23017
     *
     * @return
     *     - ESP_OK Success
     *     - ESP_FAIL Fail
     */
    esp_err_t set_iodirection(uint8_t value, mcp23017_gpio_t gpio);

    /**
     * @brief   set output value of GPIOA,(work in output)
     *
     * @param   value value of GPIOA
     * @param   gpio GPIO of mcp23017
     *
     * @return
     *     - ESP_OK Success
     *     - ESP_FAIL Fail
     */
    esp_err_t write_ioport(uint8_t value, mcp23017_gpio_t gpio);

    /**
     * @brief   read value of REG_GPIOA/REG_GPIOB;Reflects the logic level on pin <7: 0>
     *
     * @param   gpio select GPIOA/GPIOB
     *
     * @return
     *     - uint8_t
     */
    uint8_t read_ioport(mcp23017_gpio_t gpio);

    /**
     * @brief   Set MCP23017 GPIOA/GPIOB Mirror:1:Interrupt inconnect; 0:not connect.
     *
     * @param   mirror whether set up mirror interrupt
     * @param   gpio select GPIOA/GPIOB
     *
     * @return
     *     - ESP_OK Success
     *     - ESP_FAIL Fail
     */
    esp_err_t mirror_interrupt(uint8_t mirror, mcp23017_gpio_t gpio);

    /**
     * @brief   Set MCP23017 pin pullup
     *
     * @param   pins pin of pullup
     *
     * @return
     *     - ESP_OK Success
     *     - ESP_FAIL Fail
     */
    esp_err_t set_pullups(uint16_t pins);

    /**
     * @brief   Set MCP23017 interrupt pin,
     *          Only in GPIO work in INPUT mode,
     *              Default:compare last value,
     *
     * @param   pins pin of interrupt
     * @param   isDefault whether select default value
     * @param   defaultValue default value
     *
     * @return
     *     - ESP_OK Success
     *     - ESP_FAIL Fail
     */
    esp_err_t enable_interrupt_pins(uint16_t pins, bool isDefault,
            uint16_t defaultValue);

    /**
     * @brief   Delete MCP23017 interrupt pin,
     *
     * @param   pins pin of interrupt
     *
     * @return
     *     - ESP_OK Success
     *     - ESP_FAIL Fail
     */
    esp_err_t disable_interrupt_pins(uint16_t pins);

    /**
     * @brief   gets the interrupt capture values for pins with interrupts enabled,
     *          and gpio values for the ones that aren't.
     *          clear interrupt flag
     *
     * @return
     *     - ESP_OK Success
     *     - ESP_FAIL Fail
     */
    uint16_t get_intpin_values(void);

    /**
     * @brief   get interrupt flag of GPIOA
     *
     * @return
     *     - value of GPIOA interrupt flag
     */
    uint16_t get_intflag_values(void);

    /**
     * @brief   Check device Present
     *
     * @return
     *     - ESP_OK Success
     *     - ESP_FAIL Fail
     */
    esp_err_t check_device_present(void);
};
#endif

#endif

