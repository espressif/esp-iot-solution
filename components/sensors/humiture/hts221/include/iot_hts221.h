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
#ifndef _IOT_HTS221_H_
#define _IOT_HTS221_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"
#include "iot_i2c_bus.h"


/**
* @brief  State enable. 
*/
typedef enum {
    HTS221_DISABLE    = 0x00, 
    HTS221_ENABLE     = 0x01
} hts221_state_t; 

/**
* @brief  Bit status
*/
typedef enum {
    HTS221_RESET     = 0x00, 
    HTS221_SET       = 0x01
} hts221_bit_status_t; 

/**
* @brief  Humidity average. 
*/
typedef enum {
  HTS221_AVGH_4         = 0x00,         /*!< Internal average on 4 samples */
  HTS221_AVGH_8         = 0x01,         /*!< Internal average on 8 samples */
  HTS221_AVGH_16        = 0x02,         /*!< Internal average on 16 samples */
  HTS221_AVGH_32        = 0x03,         /*!< Internal average on 32 samples */
  HTS221_AVGH_64        = 0x04,         /*!< Internal average on 64 samples */
  HTS221_AVGH_128       = 0x05,         /*!< Internal average on 128 samples */
  HTS221_AVGH_256       = 0x06,         /*!< Internal average on 256 samples */
  HTS221_AVGH_512       = 0x07          /*!< Internal average on 512 samples */
}hts221_avgh_t;

/**
* @brief  Temperature average. 
*/
typedef enum {
  HTS221_AVGT_2         = 0x00,        /*!< Internal average on 2 samples */
  HTS221_AVGT_4         = 0x08,        /*!< Internal average on 4 samples */
  HTS221_AVGT_8         = 0x10,        /*!< Internal average on 8 samples */
  HTS221_AVGT_16        = 0x18,        /*!< Internal average on 16 samples */
  HTS221_AVGT_32        = 0x20,        /*!< Internal average on 32 samples */
  HTS221_AVGT_64        = 0x28,        /*!< Internal average on 64 samples */
  HTS221_AVGT_128       = 0x30,        /*!< Internal average on 128 samples */
  HTS221_AVGT_256       = 0x38         /*!< Internal average on 256 samples */
}hts221_avgt_t;

/**
* @brief  Output data rate configuration.
*/
typedef enum {   
  HTS221_ODR_ONE_SHOT  = 0x00,         /*!< Output Data Rate: one shot */
  HTS221_ODR_1HZ       = 0x01,         /*!< Output Data Rate: 1Hz */
  HTS221_ODR_7HZ       = 0x02,         /*!< Output Data Rate: 7Hz */
  HTS221_ODR_12_5HZ    = 0x03,         /*!< Output Data Rate: 12.5Hz */
} hts221_odr_t;

/**
* @brief  Push-pull/Open Drain selection on DRDY pin.
*/
typedef enum {
  HTS221_PUSHPULL   = 0x00,   /*!< DRDY pin in push pull */
  HTS221_OPENDRAIN  = 0x40    /*!< DRDY pin in open drain */
}hts221_outputtype_t;

/**
* @brief  Active level of DRDY pin.
*/
typedef enum {
  HTS221_HIGH_LVL   = 0x00,   /*!< HIGH state level for DRDY pin */
  HTS221_LOW_LVL    = 0x80    /*!< LOW state level for DRDY pin */
}hts221_drdylevel_t;

/**
* @brief  HTS221 Init structure definition. 
*/   
typedef struct
{
  hts221_avgh_t        avg_h;            /*!< Humidity average */
  hts221_avgt_t        avg_t;            /*!< Temperature average */
  hts221_odr_t         odr;              /*!< Output data rate */
  hts221_state_t       bdu_status;       /*!< HTS221_ENABLE/HTS221_DISABLE the block data update */
  hts221_state_t       heater_status;    /*!< HTS221_ENABLE/HTS221_DISABLE the internal heater */
  
  hts221_drdylevel_t   irq_level;        /*!< HTS221_HIGH_LVL/HTS221_LOW_LVL the level for DRDY pin */
  hts221_outputtype_t  irq_output_type;  /*!< Output configuration for DRDY pin */
  hts221_state_t       irq_enable;       /*!< HTS221_ENABLE/HTS221_DISABLE interrupt on DRDY pin */
}hts221_config_t; 

/**
* @brief  Bitfield positioning.
*/
#define HTS221_BIT(x)    ((uint8_t)x)

/**
* @brief  I2C address.
*/
#define HTS221_I2C_ADDRESS    ((uint8_t)0x5F)

/**
* @brief Device Identification register.
* \code
* Read
* Default value: 0xBC
* 7:0 This read-only register contains the device identifier for HTS221.
* \endcode
*/
#define HTS221_WHO_AM_I_REG         ((uint8_t)0x0F)

/**
* @brief Device Identification value.
*/
#define HTS221_WHO_AM_I_VAL         ((uint8_t)0xBC)

/**
* @brief Humidity and temperature average mode register.
* \code
* Read/write
* Default value: 0x1B
* 7:6 Reserved.
* 5:3 AVGT2-AVGT1-AVGT0: Select the temperature internal average.
*
*      AVGT2 | AVGT1 | AVGT0 | Nr. Internal Average
*   ----------------------------------------------------
*       0    |   0   |   0   |    2 
*       0    |   0   |   1   |    4  
*       0    |   1   |   0   |    8    
*       0    |   1   |   1   |    16 
*       1    |   0   |   0   |    32 
*       1    |   0   |   1   |    64  
*       1    |   1   |   0   |    128    
*       1    |   1   |   1   |    256 
*
* 2:0 AVGH2-AVGH1-AVGH0: Select humidity internal average.
*      AVGH2 | AVGH1 |  AVGH0 | Nr. Internal Average
*   ------------------------------------------------------
*       0    |   0   |   0   |    4 
*       0    |   0   |   1   |    8  
*       0    |   1   |   0   |    16    
*       0    |   1   |   1   |    32 
*       1    |   0   |   0   |    64 
*       1    |   0   |   1   |    128  
*       1    |   1   |   0   |    256    
*       1    |   1   |   1   |    512 
*
* \endcode
*/
#define HTS221_AV_CONF_REG        ((uint8_t)0x10)

#define HTS221_AVGT_BIT           (HTS221_BIT(3))
#define HTS221_AVGH_BIT           (HTS221_BIT(0))

#define HTS221_AVGH_MASK          ((uint8_t)0x07)
#define HTS221_AVGT_MASK          ((uint8_t)0x38)

/**
* @brief Control register 1.
* \code
* Read/write
* Default value: 0x00
* 7 PD: power down control. 0 - power down mode; 1 - active mode.
* 6:3 Reserved.
* 2 BDU: block data update. 0 - continuous update; 1 - output registers not updated until MSB and LSB reading.
* 1:0 ODR1, ODR0: output data rate selection.
*
*   ODR1  | ODR0  | Humidity output data-rate(Hz)  | Pressure output data-rate(Hz)
*   ----------------------------------------------------------------------------------
*     0   |   0   |         one shot               |         one shot 
*     0   |   1   |            1                      |            1 
*     1   |   0   |            7                      |            7     
*     1   |   1   |           12.5                   |           12.5  
*
* \endcode
*/
#define HTS221_CTRL_REG1      ((uint8_t)0x20)

#define HTS221_PD_BIT          (HTS221_BIT(7))
#define HTS221_BDU_BIT         (HTS221_BIT(2))
#define HTS221_ODR_BIT         (HTS221_BIT(0))

#define HTS221_PD_MASK        ((uint8_t)0x80)
#define HTS221_BDU_MASK       ((uint8_t)0x04)
#define HTS221_ODR_MASK       ((uint8_t)0x03)

/**
* @brief Control register 2.
* \code
* Read/write
* Default value: 0x00
* 7 BOOT:  Reboot memory content. 0: normal mode; 1: reboot memory content. Self-cleared upon completation.
* 6:2 Reserved.
* 1 HEATHER: 0: heater enable; 1: heater disable.
* 0 ONE_SHOT: 0: waiting for start of conversion; 1: start for a new dataset. Self-cleared upon completation.
* \endcode
*/
#define HTS221_CTRL_REG2      ((uint8_t)0x21)

#define HTS221_BOOT_BIT        (HTS221_BIT(7))
#define HTS221_HEATER_BIT      (HTS221_BIT(1))
#define HTS221_ONESHOT_BIT     (HTS221_BIT(0))

#define HTS221_BOOT_MASK       ((uint8_t)0x80)
#define HTS221_HEATER_MASK     ((uint8_t)0x02)
#define HTS221_ONE_SHOT_MASK   ((uint8_t)0x01)

/**
* @brief Control register 3.
* \code
* Read/write
* Default value: 0x00
* 7 DRDY_H_L: Interrupt edge. 0: active high, 1: active low.
* 6 PP_OD: Push-Pull/OpenDrain selection on interrupt pads. 0: push-pull; 1: open drain.
* 5:3 Reserved.
* 2 DRDY: interrupt config. 0: disable, 1: enable.
* \endcode
*/
#define HTS221_CTRL_REG3      ((uint8_t)0x22)

#define HTS221_DRDY_H_L_BIT    (HTS221_BIT(7))
#define HTS221_PP_OD_BIT       (HTS221_BIT(6))
#define HTS221_DRDY_BIT        (HTS221_BIT(2))

#define HTS221_DRDY_H_L_MASK   ((uint8_t)0x80)
#define HTS221_PP_OD_MASK      ((uint8_t)0x40)
#define HTS221_DRDY_MASK       ((uint8_t)0x04)

/**
* @brief  Status register.
* \code
* Read
* Default value: 0x00
* 7:2 Reserved.
* 1 H_DA: Humidity data available. 0: new data for humidity is not yet available; 1: new data for humidity is available.
* 0 T_DA: Temperature data available. 0: new data for temperature is not yet available; 1: new data for temperature is available.
* \endcode
*/
#define HTS221_STATUS_REG    ((uint8_t)0x27)

#define HTS221_H_DA_BIT       (HTS221_BIT(1))
#define HTS221_T_DA_BIT       (HTS221_BIT(0))

#define HTS221_HDA_MASK       ((uint8_t)0x02)
#define HTS221_TDA_MASK       ((uint8_t)0x01)

/**
* @brief  Humidity data (LSB).
* \code
* Read
* Default value: 0x00.
* HOUT7 - HOUT0: Humidity data LSB (2's complement).
* \endcode
*/
#define HTS221_HR_OUT_L_REG        ((uint8_t)0x28)

/**
* @brief  Humidity data (MSB).
* \code
* Read
* Default value: 0x00.
* HOUT15 - HOUT8: Humidity data MSB (2's complement).    
* \endcode
*/ 
#define HTS221_HR_OUT_H_REG        ((uint8_t)0x29)


/**
* @brief  Temperature data (LSB).
* \code
* Read
* Default value: 0x00.
* TOUT7 - TOUT0: temperature data LSB. 
* \endcode
*/
#define HTS221_TEMP_OUT_L_REG       ((uint8_t)0x2A)

/**
* @brief  Temperature data (MSB).
* \code
* Read
* Default value: 0x00.
* TOUT15 - TOUT8: temperature data MSB. 
* \endcode
*/
#define HTS221_TEMP_OUT_H_REG       ((uint8_t)0x2B)

/**
* @brief  Calibration registers.
* \code
* Read
* \endcode
*/
#define HTS221_H0_RH_X2          ((uint8_t)0x30)
#define HTS221_H1_RH_X2          ((uint8_t)0x31)
#define HTS221_T0_DEGC_X8        ((uint8_t)0x32)
#define HTS221_T1_DEGC_X8        ((uint8_t)0x33)
#define HTS221_T0_T1_DEGC_H2     ((uint8_t)0x35)
#define HTS221_H0_T0_OUT_L       ((uint8_t)0x36)
#define HTS221_H0_T0_OUT_H       ((uint8_t)0x37)
#define HTS221_H1_T0_OUT_L       ((uint8_t)0x3A)
#define HTS221_H1_T0_OUT_H       ((uint8_t)0x3B)
#define HTS221_T0_OUT_L          ((uint8_t)0x3C)
#define HTS221_T0_OUT_H          ((uint8_t)0x3D)
#define HTS221_T1_OUT_L          ((uint8_t)0x3E)
#define HTS221_T1_OUT_H          ((uint8_t)0x3F)


typedef void* hts221_handle_t;

/**
 * @brief Write value to one register of HTS221
 *
 * @param sensor object handle of hts221
 * @param reg_addr register address
 * @param data register value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_write_byte(hts221_handle_t sensor, uint8_t reg_addr, uint8_t data);

/**
 * @brief Write value to multiple register of HTS221
 *
 * @param sensor object handle of hts221
 * @param reg_addr start address of register
 * @param data_buf Pointer of register values buffer
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_write(hts221_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf);

/**
 * @brief Read value from register of HTS221
 *
 * @param sensor object handle of hts221
 * @param reg_addr register address
 * @param data register value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_read_byte(hts221_handle_t sensor, uint8_t reg, uint8_t *data);

/**
 * @brief Read value from multiple register of HTS221
 *
 * @param sensor object handle of hts221
 * @param reg_addr start address of register
 * @param data_buf Pointer of register values buffer
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_read(hts221_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf);

/**
 * @brief Get device identification of HTS221
 *
 * @param sensor object handle of hts221
 * @param deviceid a pointer of device ID
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_get_deviceid(hts221_handle_t sensor, uint8_t* deviceid);

/**
 * @brief Set configration of HTS221
 *
 * @param sensor object handle of hts221
 * @param hts221_config a structure pointer of configration
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_set_config(hts221_handle_t sensor, hts221_config_t* hts221_config);

/**
 * @brief Get configration of HTS221
 *
 * @param sensor object handle of hts221
 * @param hts221_config a structure pointer of configration
 *
 * @return
 *     - ESP_OK Success
 *     - others Fail
 */
esp_err_t iot_hts221_get_config(hts221_handle_t sensor, hts221_config_t* hts221_config);

/**
 * @brief Activate HTS221
 *
 * @param sensor object handle of hts221
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_set_activate(hts221_handle_t sensor);

/**
 * @brief Set HTS221 as power down mode
 *
 * @param sensor object handle of hts221
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_set_powerdown(hts221_handle_t sensor);

/**
 * @brief Set output data rate
 *
 * @param sensor object handle of hts221
 * @param odr output data rate value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_set_odr(hts221_handle_t sensor, hts221_odr_t odr);

/**
 * @brief Set humidity average
 *
 * @param sensor object handle of hts221
 * @param avgh selections of the numbers of averaged humidity samples  
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_set_avgh(hts221_handle_t sensor, hts221_avgh_t avgh);

/**
 * @brief Set temperature average
 *
 * @param sensor object handle of hts221
 * @param avgt  selections of the numbers of averaged temperature samples
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_set_avgt(hts221_handle_t sensor, hts221_avgt_t avgt);

/**
 * @brief Enable block data update
 *
 * @param sensor object handle of hts221
 * @param status enable/diable status
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_set_bdumode(hts221_handle_t sensor, hts221_state_t status);

/**
 * @brief  Reboot memory content
 *
 * @param sensor object handle of hts221
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_memory_boot(hts221_handle_t sensor);

/**
 * @brief Enable heater
 *
 * @param sensor object handle of hts221
 * @param status enable/diable status
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_set_heaterstate(hts221_handle_t sensor, hts221_state_t status);

/**
 * @brief  Enable one-shot mode
 *
 * @param sensor object handle of hts221
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_start_oneshot(hts221_handle_t sensor);

/**
 * @brief Set level configuration of the interrupt pin DRDY
 *
 * @param sensor object handle of hts221
 * @param value active level selections
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_set_irq_activelevel(hts221_handle_t sensor, hts221_drdylevel_t value);

/**
 * @brief Set Push-pull/open drain configuration for the interrupt pin DRDY
 *
 * @param sensor object handle of hts221
 * @param value output type selections
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_set_irq_outputtype(hts221_handle_t sensor, hts221_outputtype_t value);

/**
 * @brief Enable/disable the interrupt mode
 *
 * @param sensor object handle of hts221
 * @param status enable/diable status
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_set_irq_enable(hts221_handle_t sensor, hts221_state_t status);

/**
 * @brief Read HTS221 humidity output registers
 *
 * @param sensor object handle of hts221
 * @param humidity pointer to the returned humidity raw value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_get_raw_humidity(hts221_handle_t sensor, int16_t *humidity);

/**
 * @brief Read HTS221 Humidity output registers, and calculate humidity
 *
 * @param sensor object handle of hts221
 * @param humidity pointer to the returned humidity value that must be divided by 10 to get the value in [%]
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_get_humidity(hts221_handle_t sensor, int16_t *humidity);

/**
 * @brief Read HTS221 temperature output registers
 *
 * @param sensor object handle of hts221
 * @param temperature pointer to the returned temperature raw value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_get_raw_temperature(hts221_handle_t sensor, int16_t *temperature);

/**
 * @brief Read HTS221 temperature output registers, and calculate temperature
 *
 * @param sensor object handle of hts221
 * @param temperature pointer to the returned temperature value that must be divided by 10 to get the value in ['C]
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_get_temperature(hts221_handle_t sensor, int16_t *temperature);

/**
 * @brief Create and init sensor object and return a sensor handle
 *
 * @param bus I2C bus object handle
 * @param dev_addr I2C device address of sensor
 *
 * @return
 *     - NULL Fail
 *     - Others Success
 */
hts221_handle_t iot_hts221_create(i2c_bus_handle_t bus, uint16_t dev_addr);

/**
 * @brief Delete and release a sensor object
 *
 * @param sensor object handle of hts221
 * @param del_bus Whether to delete the I2C bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_hts221_delete(hts221_handle_t sensor, bool del_bus);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/**
 * class of HTS221 temperature and humidity sensor
 */
class CHts221
{
private:
    hts221_handle_t m_sensor_handle;
    CI2CBus *bus;

    /**
     * prevent copy constructing
     */
    CHts221(const CHts221&);
    CHts221& operator = (const CHts221&);
public:
    /**
     * @brief Constructor of CHts221 class
     * @param p_i2c_bus pointer to CI2CBus object
     * @param addr sensor device address
     */
    CHts221(CI2CBus *p_i2c_bus, uint8_t addr = HTS221_I2C_ADDRESS);

    /**
     * @brief Destructor function of CHts221 class
     */
    ~CHts221();

    /**
     * @brief read temperature
     * @return temperature value
     */
    float read_temperature();

    /**
     * @brief read humidity
     * @return humidity value
     */
    float read_humidity();

    /**
     * @brief read device ID
     * @return device id
     */
    uint8_t id();
};
#endif
#endif

