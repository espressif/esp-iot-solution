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
#ifndef _IS31FL3736_H_
#define _IS31FL3736_H_
#include "is31fl3736_reg.h"
#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IS31FL3736_CH_BIT(i) ((uint16_t)0x1<<i) /*!< i == channel : 0 ~ max */
/*!< c: return the BIT num that you want control; o: selected channel num from BIT(0)  */
#define IS31FL3736_CH_MASK(c, o) do { \
    int i; \
    for (i=0; i<o; i++) { \
        c = c | IS31FL3736_CH_BIT(i); \
    } \
} while(0)

#define IS31FL3736_PWM_CHANNEL_MAX (0XBE) /*!< max channel num in the PWM mode */
#define IS31FL3736_CSX_MAX         (8)    /*!< in LED matrix mode, there are 8 current sources channel */
#define IS31FL3736_CSX_MAX_MASK    (0xff)
#define IS31FL3736_SWY_MAX         (12)   /*!< in LED matrix mode, there are 12 switch channel */
#define IS31FL3736_SWY_MAX_MASK    (0xfff)

#define IS31FL3736_PAGE(i) (i) /*!< range 0 ~ 3 */

typedef enum {
    ADDR_PIN_GND = 0, /**< [HW]addr pin connect to GMD pin */
    ADDR_PIN_SCL = 1, /**< [HW]addr pin connect to SCL pin */
    ADDR_PIN_SDA = 2, /**< [HW]addr pin connect to SDA pin */
    ADDR_PIN_VCC = 3, /**< [HW]addr pin connect to VCC pin */
    ADDR_PIN_MAX,        /**< Reserve */
} is31fl3736_addr_pin_t;

typedef union {
    struct {
        uint8_t normal_en:1; /**< Software shutdown contorl. 0:shutdown; 1:normal */
        uint8_t breath_en:1; /**< auto breath enable. 0:PWM mode; 1:auto breath mode */
        uint8_t detect_en:1; /**< Open/Short Detection. 0:disable; 1:enable */
        uint8_t :3;          /**< reserve */
        uint8_t sync_mode:2; /**< Synchronize Configuration. 0:high impedance; 1:master; 2:slave */
    };
    uint8_t val;
} is31fl3736_mode_t;

typedef struct {
    uint8_t open_en:1;  /**< Dot Open Interrupt. 0:Disable dot open interrupt; 1:enable */
    uint8_t short_en:1; /**< Dot Short Interrupt. 0:Disable dot short interrupt; 1:enable */
    uint8_t auto_breath:1; /**< Auto Breath Interrupt. 0:Disable auto breath loop finish interrupt; 1:enable */
    uint8_t auto_clr:1; /**< Auto Clear Interrupt. 0:Intr not auto clear; 1:Intr clear when INTB stay low */
    uint8_t :4;         /**< reserve */
} is31fl3736_intr_t;

typedef struct {
    uint8_t open_s:1; /**< Open Bit. 0:No open; 1:Open happens */
    uint8_t short_s:1;/**< Short Bit. 0:No short; 1:short happens */
    uint8_t abm1:1;   /**< Dot Short Interrupt. 0:ABM1 not finish; 1:ABM1 finish */
    uint8_t abm2:1;   /**< Auto Breath Interrupt. 0:ABM2 not finish; 1:ABM2 finish */
    uint8_t abm3:1;   /**< Auto Breath Mode 3 Finish. 0:ABM3 not finish; 1:ABM3 finish */
    uint8_t :3;       /**< reserve */
} is31fl3736_intr_status_t;

typedef enum {
    IS31FL3736_RESISTOR_0   = 0,    /*!< The resistor value */
    IS31FL3736_RESISTOR_0_5 = 0x01, /*!< The resistor value */
    IS31FL3736_RESISTOR_1K  = 0x02, /*!< The resistor value */
    IS31FL3736_RESISTOR_2K  = 0x03, /*!< The resistor value */
    IS31FL3736_RESISTOR_3K  = 0x04, /*!< The resistor value */
    IS31FL3736_RESISTOR_4K  = 0x05, /*!< The resistor value */
    IS31FL3736_RESISTOR_8K  = 0x06, /*!< The resistor value */
    IS31FL3736_RESISTOR_16K = 0x07, /*!< The resistor value */
    IS31FL3736_RESISTOR_32K = 0x08, /*!< The resistor value */
    IS31FL3736_RES_MAX,
} is31fl3736_res_t;

typedef enum {
    IS31FL3736_RESISTOR_SW_PULLUP    = IS31FL3736_REG_PG3_SW_PULLUP, /*!< The resistor value */
    IS31FL3736_RESISTOR_CS_PULLDOWN  = IS31FL3736_REG_PG3_CS_PULLDOWN, /*!< The resistor value */
    IS31FL3736_RES_TYPE_MAX
} is31fl3736_res_type_t;

typedef enum {
    IS31FL3736_LED_OFF = 0, /*!< The resistor value */
    IS31FL3736_LED_ON  = 1, /*!< The resistor value */
    IS31FL3736_LED_MAX,
} is31fl3736_led_stau_t;

typedef enum {
    IS31FL3736_PWM_MODE = 0, /*!< The resistor value */
    IS31FL3736_ABM1     = 1, /*!< The resistor value */
    IS31FL3736_ABM2     = 2, /*!< The resistor value */
    IS31FL3736_ABM3     = 3, /*!< The resistor value */
    IS31FL3736_ABM_MAX,
} is31fl3736_auto_breath_mode_t;

typedef void* is31fl3736_handle_t;

/**
 * @brief Create and init is31fl3736 slave device
 * @param bus I2C bus object handle
 * @param rst_io reset IO number
 * @param addr1 strap value of addr1 pin
 * @param addr2 strap value of addr2 pin
 * @param cur_val current value
 * @return
 *     - NULL Fail
 *     - Others Success
 */
is31fl3736_handle_t is31fl3736_create(i2c_bus_handle_t bus, gpio_num_t rst_io, is31fl3736_addr_pin_t addr1, is31fl3736_addr_pin_t addr2, uint8_t cur_val);

/**
 * @brief Delete and release a is31fl3736 object
 *
 * @param fxled object handle of is31fl3736
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t is31fl3736_delete(is31fl3736_handle_t fxled);

/**
 * @brief the chip ADDR1 & ADDR2 decition the slave iic addr.
 *
 * @param addr1_pin  refer to hardware connect
 * @param addr2_pin  refer to hardware connect
 *
 * @return the iic salve addr
 */
uint8_t is31fl3736_get_i2c_addr(is31fl3736_addr_pin_t addr1_pin, is31fl3736_addr_pin_t addr2_pin);

/**
 * @brief The Configuration mode of IS31FL3736.
 * @param fxled object handle of is31fl3736
 * @param mode  refer to is31fl3736_mode_t
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3736_set_mode(is31fl3736_handle_t fxled, is31fl3736_mode_t mode);

/**
 * @brief The Global Current Control Register modulates all CSx (x=1~8) DC current.
 * @param fxled object handle of is31fl3736
 * @param curr_value  range: 0 ~ 255; I_out = 840/R_ext * curr_value/256
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3736_set_global_current(is31fl3736_handle_t fxled, uint8_t curr_value);

/**
 * @brief SWy Pull-Up Resistor and CSx Pull-Down Resistor Selection.
 * @param fxled object handle of is31fl3736
 * @param type  refer to is31fl3736_res_type_t
 * @param res_val  refer to is31fl3736_res_t
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3736_set_resistor(is31fl3736_handle_t fxled, is31fl3736_res_type_t type, is31fl3736_res_t res_val);

/**
 * @brief contorl the LED matrix.
 * @param fxled object handle of is31fl3736
 * @param cs_x_bit  BIT0 ~ BIT8 represent current source channel 0 ~ channel 8
 * @param sw_y_bit  BIT0 ~ BIT12 represent switch channel 0 ~ channel 12
 * @param status  ON/OFF
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3736_set_led_matrix(is31fl3736_handle_t fxled, uint16_t cs_x_bit, uint16_t sw_y_bit, is31fl3736_led_stau_t status);

/**
 * @brief contorl the LED matrix.
 * @param fxled object handle of is31fl3736
 * @param cs_x_bit  BIT0 ~ BIT8 represent current source channel 0 ~ channel 8
 * @param sw_y_bit  BIT0 ~ BIT12 represent switch channel 0 ~ channel 12
 * @param duty  the PWM duty for this LED 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3736_set_pwm_duty_matrix(is31fl3736_handle_t fxled, uint16_t cs_x_bit, uint16_t sw_y_bit, uint8_t duty);

/**
 * @brief in PWM mode change duty.
 * @param fxled object handle of is31fl3736
 * @param pwm_ch  0 ~ IS31FL3736_PWM_CHANNEL_MAX
 * @param duty  duty for each pwm channel
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3736_set_channel_duty(is31fl3736_handle_t fxled, uint8_t pwm_ch, uint8_t duty);

/**
 * @brief in PWM mode change duty.
 * @param fxled object handle of is31fl3736
 * @param pwm_ch  0 ~ IS31FL3736_PWM_CHANNEL_MAX
 * @param mode  auto breath mode for each pwm channel
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3736_set_breath_mode(is31fl3736_handle_t fxled, uint8_t pwm_ch, is31fl3736_auto_breath_mode_t mode);

/**
 * @brief set the auto breath mode detail.
 * @param fxled object handle of is31fl3736
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3736_update_auto_breath(is31fl3736_handle_t fxled);

/**
 * @brief Reset all register to POR state.
 * @param fxled object handle of is31fl3736
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3736_reset_reg(is31fl3736_handle_t fxled);

/**
 * @brief Change SDB io status to reset IC.
 * @param fxled object handle of is31fl3736
 * @param sdb_io_number io num connected SDB pin
 * @return
 *     - ESP_OK Success
 */
esp_err_t is31fl3736_hw_reset(is31fl3736_handle_t fxled);

/**
 * @brief Ready to write reg page.
 * @param fxled object handle of is31fl3736
 * @param page_num reg page (P0 ~ P3)
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3736_write_page(is31fl3736_handle_t fxled, uint8_t page_num);

/**
 * @brief Write is31fl3736 device register
 * @param fxled object handle of is31fl3736
 * @param reg_addr register address
 * @param data data pointer
 * @param data_num data length
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t is31fl3736_write(is31fl3736_handle_t fxled, uint8_t reg_addr, uint8_t *data, uint8_t data_num);

/**
 * @brief Write is31fl3736 device register
 * @param fxled object handle of is31fl3736
 * @param duty pwm duty
 * @param buf pointer of data map
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t is31fl3736_fill_buf(is31fl3736_handle_t fxled, uint8_t duty, uint8_t* buf);

#ifdef __cplusplus
}
#endif

#endif
