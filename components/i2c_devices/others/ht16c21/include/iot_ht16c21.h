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
#ifndef __IOT_HT16C21_H__
#define __IOT_HT16C21_H__

#ifdef __cplusplus
extern "C"
{
#endif
#include "driver/i2c.h"
#include "iot_i2c_bus.h"

#define HT16C21_I2C_ADDRESS_DEFAULT   (0x70)

#define WRITE_BIT      I2C_MASTER_WRITE         /*!< I2C master write */
#define READ_BIT       I2C_MASTER_READ          /*!< I2C master read */
#define ACK_CHECK_EN   0x1                      /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0                      /*!< I2C master will not check ack from slave */
#define ACK_VAL        0x0                      /*!< I2C ack value */
#define NACK_VAL       0x1                      /*!< I2C nack value */

typedef enum {
    HT16C21_CMD_IOOUT = 0x80, /*!< Command to set  as configuration Display Data RAM*/
    HT16C21_CMD_DRIMO = 0x82, /*!< Command to set  as Configuration device mode*/
    HT16C21_CMD_SYSMO = 0x84, /*!< Command to set  as Configuration system mode*/
    HT16C21_CMD_FRAME = 0x86, /*!< Command to set  as Configuration frame frequency*/
    HT16C21_CMD_BLINK = 0x88, /*!< Command to set  as Configuration blinking frequency*/
    HT16C21_CMD_IVA   = 0x8a,   /*!< Command to set  as Configuration internal voltage adjustment (IVA) setting*/
} ht16c21_cmd_t;

typedef enum {
    HT16C21_4DUTY_3BIAS = 0x00, /*!< The drive mode 1/4 duty output and 1/3 bias is selected.*/
    HT16C21_4DUTY_4BIAS = 0x01, /*!< The drive mode 1/4 duty output and 1/4 bias is selected.*/
    HT16C21_8DUTY_3BIAS = 0x02, /*!< The drive mode 1/8 duty output and 1/3 bias is selected.*/
    HT16C21_8DUTY_4BIAS = 0x03, /*!< The drive mode 1/8 duty output and 1/4 bias is selected.*/
} ht16c21_duty_bias_t;

typedef enum {
    HT16C21_OSCILLATOR_OFF_DISPLAY_OFF = 0x00, /*!< Display off and disable the internal system oscillator.*/
    HT16C21_OSCILLATOR_ON_DISPLAY_OFF = 0x02, /*!< Display off and enable the internal system oscillator.*/
    HT16C21_OSCILLATOR_ON_DISPLAY_ON = 0x03, /*!< Display on and enable the internal system oscillator.*/
} ht16c21_oscillator_display_t;

typedef enum {
    HT16C21_FRAME_80HZ = 0x00, /*!< Frame frequency is set to 80Hz.*/
    HT16C21_FRAME_160HZ = 0x01, /*!< Frame frequency is set to 160Hz.*/
} ht16c21_frame_frequency_t;
typedef enum {
    HT16C21_BLINKING_OFF = 0x00, /*!< Blinking function is switched off.*/
    HT16C21_BLINKING_2HZ = 0x01, /*!< Blinking function is set to 2HZ.*/
    HT16C21_BLINKING_1HZ = 0x02, /*!< Blinking function is set to 1HZ.*/
    HT16C21_BLINKING_5HZ = 0x03, /*!< Blinking function is set to 0.5HZ.*/
} ht16c21_blinking_frequency_t;
typedef enum {
    HT16C21_VLCD_PIN_VOL_ADJ_OFF = 0x00, /*!<  The Segment/VLCD pin is set as the VLCD pin. Disable the internal voltage adjustment function
        One external resister must be connected between VLCD pin and VDD pin to determine the bias voltage,and internal voltage follower (OP4)
        must be enabled by setting the DA3~DA0 bits as the value other than “0000”. If the VLCD pin is connected to the VDD pin, the internal
        voltage follower (OP4) must be disabled by setting the DA3~DA0 bits as “0000”..*/
    HT16C21_VLCD_PIN_VOL_ADJ_ON = 0x10,/*!< The Segment/VLCD pin is set as the VLCD pin. Enable the internal voltage adjustment function.
        The VLCD pin is an output pin of which the voltage can be detected by the external MCU host.*/
    HT16C21_SEGMENT_PIN_VOL_ADJ_OFF = 0x20, /*!< The Segment/VLCD pin is set as the Segment pin. Disable the internal voltage adjustment function.
        The bias voltage is supplied by the internal VDD power.The internal voltage-follower (OP4) is disabled automatically and DA3~DA0 don’t care.*/
    HT16C21_SEGMENT_PIN_VOL_ADJ_ON = 0x30,/*!<The Segment/VLCD pin is set as the Segment pin. Enable the internal voltage adjustment function.*/
} ht16c21_pin_and_voltage_t;

typedef struct config {
    ht16c21_duty_bias_t duty_bias;
    ht16c21_oscillator_display_t oscillator_display;
    ht16c21_frame_frequency_t frame_frequency;
    ht16c21_blinking_frequency_t blinking_frequency;
    ht16c21_pin_and_voltage_t pin_and_voltage;
    uint8_t  adjustment_voltage; //Range 0x00 to 0x0F
} ht16c21_config_t;

typedef void* ht16c21_handle_t;

/**
 * @brief   Create and initialization device object and return a device handle
 *
 * @param   bus I2C bus object handle
 * @param   dev_addr I2C device address of device
 *
 * @return
 *     - device object handle of ht16c21
 */
ht16c21_handle_t iot_ht16c21_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief   Delete and release a device object
 *
 * @param   dev object handle of ht16c21
 * @param   del_bus Whether to delete the I2C bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ht16c21_delete(ht16c21_handle_t dev, bool del_bus);

/**
 * @brief   Write command or data to ht16c21
 *
 * @param   dev object handle of ht16c21
 * @param   hd16c21_cmd  will write command
 * @param   val command value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ht16c21_write_cmd(ht16c21_handle_t dev, ht16c21_cmd_t hd16c21_cmd, uint8_t val);

/**
 * @brief   Write RAM or byte data to ht16c21
 *
 * @param   dev object handle of ht16c21
 * @param   address RAM address
 * @param   buf data value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ht16c21_ram_write_byte(ht16c21_handle_t dev, uint8_t address, uint8_t buf);

/**
 * @brief   Write RAM or data to ht16c21
 *
 * @param   dev object handle of ht16c21
 * @param   address RAM address
 * @param   *buf data value pointer
 * @param   *buf data value len
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ht16c21_ram_write(ht16c21_handle_t dev, uint8_t address, uint8_t *buf, uint8_t len);

/**
 * @brief   Read RAM or byte data to ht16c21
 *
 * @param   dev object handle of ht16c21
 * @param   address RAM address
 * @param   buf data value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ht16c21_ram_read_byte(ht16c21_handle_t dev, uint8_t address, uint8_t *data);

/**
 * @brief   Read RAM or data to ht16c21
 *
 * @param   dev object handle of ht16c21
 * @param   address RAM address
 * @param   *buf data value pointer
 * @param   *buf data value len
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ht16c21_ram_read(ht16c21_handle_t dev, uint8_t address, uint8_t *buf, uint8_t len);

/**
 * @brief   init ht16c21
 *
 * @param   dev object handle of ht16c21
 * @param   ht16c21_conf ht16c21 configuration info
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ht16c21_init(ht16c21_handle_t dev, ht16c21_config_t*  ht16c21_conf);

#endif

