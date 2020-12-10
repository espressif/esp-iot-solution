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
#ifndef _VEML6075_H_
#define _VEML6075_H_

#include "i2c_bus.h"

#define VEML6075_I2C_ADDRESS    (0x10)
#define VEML6075_I2C_ERR_RES    (-1)

//CMD
#define VEML6075_RW_CFG       0x00
#define VEML6075_READ_UVA     0x07
#define VEML6075_READ_DARK    0x08
#define VEML6075_READ_UVB     0x09
#define VEML6075_READ_VIS     0x0A
#define VEML6075_READ_IR      0x0B
#define VEML6075_READ_DEVID   0x0C

#define VEML6075_CFG_HD_NORM    (0x00)
#define VEML6075_CFG_HD_HIGH    (0x08)
#define VEML6075_CFG_TRIG       (0x04)
#define VEML6075_CFG_AF_OFF     (0x00)
#define VEML6075_CFG_AF_ON      (0x02)
#define VEML6075_CFG_SD_OFF     (0x00)
#define VEML6075_CFG_SD_ON      (0x01)

// Taken from application note:
// http://www.vishay.com/docs/84339/designingveml6075.pdf

#define VEML6075_UVI_UVA_VIS_COEFF    (2.22)
#define VEML6075_UVI_UVA_IR_COEFF     (1.33)
#define VEML6075_UVI_UVB_VIS_COEFF    (2.95)
#define VEML6075_UVI_UVB_IR_COEFF     (1.74)
#define VEML6075_UVI_UVA_RESPONSE     (1.0 / 909.0)
#define VEML6075_UVI_UVB_RESPONSE     (1.0 / 800.0)

typedef enum {
    VEML6075_INTEGRATION_TIME_50MS = 0, /*!< Command to set integration time 40ms*/
    VEML6075_INTEGRATION_TIME_100MS = 1, /*!< Command to set integration time 80ms*/
    VEML6075_INTEGRATION_TIME_200MS = 2, /*!< Command to set integration time 160ms*/
    VEML6075_INTEGRATION_TIME_400MS = 3, /*!< Command to set integration time 320ms*/
    VEML6075_INTEGRATION_TIME_800MS = 4, /*!< Command to set integration time 640ms*/
    VEML6075_INTEGRATION_TIME_MAX,
} veml6075_integration_time_t;

typedef enum {
    VEML6075_TRIGGER_DIS = 0, /*!< set not trigger 				*/
    VEML6075_TRIGGER_ONCE = 1, /*!< set trigger one time detect cycle*/
    VEML6075_TRIGGER_MAX,
} veml6075_trigger_t;

typedef enum {
    VEML6075_MODE_AUTO = 0, /*!< set auto mode  */
    VEML6075_MODE_FORCE = 1, /*!< set force mode */
    VEML6075_MODE_MAX,
} veml6075_mode_t;

typedef enum {
    VEML6075_SWITCH_EN = 0, /*!< set enable  */
    VEML6075_SWITCH_DIS = 1, /*!< set disable */
    VEML6075_SWITCH_MAX,
} veml6075_switch_t;

/**
 * @brief  VEML6075 Init structure definition.
 */
typedef struct {
    veml6075_integration_time_t integration_time; /*!< set integration time  */
    veml6075_trigger_t trigger; /*!< set  trigger  */
    veml6075_mode_t mode; /*!< set  mode  */
    veml6075_switch_t switch_en; /*!< set if enable  */
} veml6075_config_t;

typedef struct {
    uint16_t raw_uva;
    uint16_t raw_uvb;
    uint16_t raw_dark;
    uint16_t raw_vis;
    uint16_t raw_ir;
} veml6075_raw_data_t;

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
    veml6075_config_t config;
    veml6075_raw_data_t raw_data;
} veml6075_dev_t;

typedef void* veml6075_handle_t;

#define VEML6075_INTEGRATION_TIME_DEFAULT  VEML6075_INTEGRATION_TIME_50MS
#define VEML6075_TRIGGER_DEFAULT           VEML6075_TRIGGER_DIS
#define VEML6075_MODE_DEFAULT              VEML6075_MODE_AUTO
#define VEML6075_SWITCH_DEFAULT            VEML6075_SWITCH_EN

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   create veml6075 sensor device
 * @param   bus device object handle
 * @param   dev_addr device I2C address
 * @return
 *     - NULL if fail to create
 *     - Others handle of veml6075 device
 */
veml6075_handle_t veml6075_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief   delete veml6075 device
 * @param   sensor Point to veml6075 operate handle
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t veml6075_delete(veml6075_handle_t *sensor);

/**
 * @brief  set veml6075 mode
 * @param  sensor veml6075 operate handle
 * @param  device_info device config info
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t veml6075_set_mode(veml6075_handle_t sensor, veml6075_config_t * device_info);

/**
 * @brief  set veml6075 mode
 * @param  sensor veml6075 operate handle
 * @param  device_info device config info
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t veml6075_get_mode(veml6075_handle_t sensor, veml6075_config_t * device_info);

/**
 * @brief The UVA band intensity is returned as a float (0.0 means "no light", higher
 * numbers mean more light was measured
 * 
 * @param sensor veml6075 operate handle
 * @return uint16_t 
 */
esp_err_t veml6075_get_raw_data(veml6075_handle_t sensor);

/**
 * @brief The UVA band intensity is returned as a float (0.0 means "no light", higher
 * numbers mean more light was measured
 * 
 * @param sensor veml6075 operate handle
 * @return uint16_t 
 */
float veml6075_get_uva(veml6075_handle_t sensor);

/**
 * @brief The UVB band intensity is returned as a float (0.0 means "no light", higher
 * numbers mean more light was measured
 * 
 * @param sensor veml6075 operate handle
 * @return uint16_t 
 */
float veml6075_get_uvb(veml6075_handle_t sensor);

/**
 * @brief  * Please see https://www.vishay.com/docs/84339/designingveml6075.pdf for
 * details.
 * The UVIndex is computed from UVA, UVB, IR and Visual counters and returned.
 * *   <2.0  - Low UVIndex intensity
 * *   <5.5  - Moderate
 * *   <7.5  - High
 * *   <10.5 - Very High
 * *   >10.5 - Extreme UVIndex intensity
 *
 * @param sensor veml6075 operate handle
 * @return uint16_t 
 */
float veml6075_get_uv_index(veml6075_handle_t sensor);

/**
 * @brief Return raw counters for light measured in the UVA register.
 * 
 * @param sensor veml6075 operate handle
 * @return uint16_t 
 */
uint16_t veml6075_get_raw_uva(veml6075_handle_t sensor);

/**
 * @brief Return raw counters for light measured in the UVB register.
 * 
 * @param sensor veml6075 operate handle
 * @return uint16_t 
 */
uint16_t veml6075_get_raw_uvb(veml6075_handle_t sensor);

/**
 * @brief Return raw counters for light measured in the RawDark register.
 * 
 * @param sensor veml6075 operate handle
 * @return uint16_t 
 */
uint16_t veml6075_get_raw_dark(veml6075_handle_t sensor);

/**
 * @brief Return raw counters for light measured in the Visual register.
 * 
 * @param sensor veml6075 operate handle
 * @return uint16_t 
 */
uint16_t veml6075_get_raw_vis(veml6075_handle_t sensor);

/**
 * @brief Return raw counters for light measured in the Infra Red register.
 * 
 * @param sensor veml6075 operate handle
 * @return uint16_t 
 */
uint16_t veml6075_get_raw_ir(veml6075_handle_t sensor);

/**implements of light sensor hal interface**/
#ifdef CONFIG_SENSOR_LIGHT_INCLUDED_VEML6075
/**
 * @brief initialize veml6040 with default configurations
 * 
 * @param i2c_bus i2c bus handle the sensor will attached to
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_veml6075_init(i2c_bus_handle_t handle);

/**
 * @brief de-initialize veml6040
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_veml6075_deinit(void);

/**
 * @brief test if veml6040 is active
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_veml6075_test(void);

/**
 * @brief Acquire light sensor ultra violet result one time.
 * light Ultraviolet includes UVA UVB and UV
 * 
 * @param uv Ultraviolet result (unit:lux)
 * @param uva Ultraviolet A data (unit:lux)
 * @param uvb  Ultraviolet B data (unit:lux)
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_veml6075_acquire_uv(float* uv, float* uva, float* uvb);

#endif

#ifdef __cplusplus
}
#endif

#endif
