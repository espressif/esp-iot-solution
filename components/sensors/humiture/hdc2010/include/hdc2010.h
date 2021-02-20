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
#ifndef _IOT_HDC2010_H_
#define _IOT_HDC2010_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/i2c.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "math.h"

#define HDC2010_TEMP_LOW        		0x00 			 /*Temperature [7:0]   */
#define HDC2010_TEMP_HIGH       		0x01			 /*Temperature [15:8]  */
#define HDC2010_HUM_LOW         		0x02			 /*Humidity [7:0] */
#define HDC2010_HUM_HIGH        		0x03			 /*Humidity [15:8]*/
#define HDC2010_INTERRUPT       		0x04			 /*DataReady and interrupt configuration*/
#define HDC2010_TEMPE_MAX       		0x05			 /*Max temperature value measured (peak detector)*/
#define HDC2010_HUM_MAX       			0x06			 /*Max humidity value measured (peak detector)*/
#define HDC2010_INT_MASK       			0x07			 /*Interrupt Mask*/
#define HDC2010_TEMP_OFFSET    		 	0x08			 /*Temperature offset adjustment*/
#define HDC2010_HUM_OFFSET     		 	0x09			 /*Humidity offset adjustment*/
#define HDC2010_TEMP_THR_L     			0x0A			 /*Temperature Threshold Low*/
#define HDC2010_TEMP_THR_H      		0x0B			 /*Temperature Threshold High*/
#define HDC2010_HUM_THR_L       		0x0C			 /*Humidity threshold Low*/
#define HDC2010_HUM_THR_H       		0x0D			 /*Humidity threshold High*/
#define HDC2010_RESET_INT_CONF  		0x0E			 /*Soft Reset and Interrupt Configuration*/
#define HDC2010_MEASURE_CONF    		0x0F			 /*Measurement configuration*/
#define HDC2010_MANUFACTURER_ID_L       0xFC			 /*Manufacturer ID Low*/
#define HDC2010_MANUFACTURER_ID_H       0xFD			 /*Manufacturer ID High*/
#define HDC2010_DEVICE_ID_L      		0xFE			 /*Device ID Low*/
#define HDC2010_DEVICE_ID_H       		0xFF			 /*Device ID High*/

#define HDC2010_ERR_VAL                 (-9999)

/* Address 0x08 Temperature Offset Adjustment */
/* The temperature can be adjusted adding the following values that are enable settings the equivalents bits:*/
// 7		6		5		4		3		2		1		0
//-20.62    10.32   5.16    2.58    1.28    0.64    0.32    0.16
/* Address 0x09 Humidity Offset Adjustment */
/* The humidity can be adjusted adding the following values that are enable settings the equivalents bits:*/
// 7		6		5		4		3		2		1		0
// -25		12.5	6.3		3.1		1.6		0.8		0.4		0.2
typedef enum {
    HDC2010_ADDR_PIN_SELECT_GND = 0x40, /*!< set address PIN select GND  */
    HDC2010_ADDR_PIN_SELECT_VDD = 0x41, /*!< set address PIN select VDD  */
} hdc2010_set_address_t;

/* Address 0x04 Interrupt DRDY Register */

typedef enum {
    HDC2010_DRDY_STATUS_NOT_READY = 0, /*!0 = Data Not Ready!   DRDY_STAUS is cleared to 0 when read*/
    HDC2010_DRDY_STATUS_READY = 1, /*! 1= Data Ready !*/
} hdc2010_drdy_status_t; /*DataReady bit status*/
typedef enum {
    HDC2010_HH_STATUS_NOT_READY = 0, /*!0 = No interrupt!   HH_STAUS is cleared to 0 when read*/
    HDC2010_HH_STATUS_READY = 1, /*! 1= interrupt !*/
} hdc2010_hh_status_t; /*Temperature threshold HIGH Interrupt status*/
typedef enum {
    HDC2010_HL_STATUS_NOT_READY = 0, /*!0 = No interrupt!   HL_STAUS is cleared to 0 when read*/
    HDC2010_HL_STATUS_READY = 1, /*! 1= interrupt !*/
} hdc2010_hl_status_t; /*Temperature threshold LOW Interrupt status*/
typedef enum {
    HDC2010_TH_STATUS_NOT_READY = 0, /*!0 = No interrupt!   TH_STAUS is cleared to 0 when read*/
    HDC2010_TH_STATUS_READY = 1, /*! 1= interrupt !*/
} hdc2010_th_status_t; /*Humidity threshold HIGH Interrupt status*/
typedef enum {
    HDC2010_TL_STATUS_NOT_READY = 0, /*!0 = No interrupt!   TL_STAUS is cleared to 0 when read*/
    HDC2010_TL_STATUS_READY = 1, /*! 1= interrupt!*/
} hdc2010_tl_status_t; /*Humidity threshold LOW Interrupt status*/

/*Address 0x07 Interrupt Configuration*/
typedef enum {
    HDC2010_DRDY_MASK_INT_DISABLE = 0, /*0 = DataReady Interrupt generator disable*/
    HDC2010_DRDY_MASK_INT_ENABLE = 1, /*1 = DaReady Interrupt generator enable*/
} hdc2010_drdy_mask_t; /*DataReady Interrupt mask*/
typedef enum {
    HDC2010_TH_MASK_INT_ENABLE = 0, /*0 = Temperature high Interrupt generator enable*/
    HDC2010_TH_MASK_INT_DISABLE = 1, /*1 = Temperature high Interrupt generator disable*/
} hdc2010_th_mask_t; /*Temperature threshold HIGH Interrupt mask*/
typedef enum {
    HDC2010_TL_MASK_INT_ENABLE = 0, /*0 = Temperature low Interrupt generator enable*/
    HDC2010_TL_MASK_INT_DISABLE = 1, /*1 = Temperature low Interrupt generator disable*/
} hdc2010_tl_mask_t; /*Temperature threshold LOW Interrupt mask*/
typedef enum {
    HDC2010_HH_MASK_INT_ENABLE = 0, /*0 = Humidity high Interrupt generator enable*/
    HDC2010_HH_MASK_INT_DISABLE = 1, /*1 = Humidity high Interrupt generator disable*/
} hdc2010_hh_mask_t; /*Humidity threshold HIGH Interrupt mask*/
typedef enum {
    HDC2010_HL_MASK_INT_ENABLE = 0, /*0 = Humidity low Interrupt generator enable*/
    HDC2010_HL_MASK_INT_DISABLE = 1, /*1 = Humidity Low Interrupt generator disable*/
} hdc2010_hl_mask_t; /*Humidity threshold LOW Interrupt mask*/

/* Address 0x0E Reset and DRDY/INT Configuration Field Descriptions */

typedef enum {
    HDC2010_NORMAL_OPERATION = 0, /*0 = Normal Operation mode, this bit is self-clear*/
    HDC2010_SOFT_RESET = 1, /*1 = Soft Reset*/
} hdc2010_soft_res_t; /*EEPROM value reload and registers reset*/

typedef enum {
    HDC2010_ODR_000 = 0, /*000 = No repeated measurements. Trigger on demand*/
    HDC2010_ODR_001 = 1, /*001 = 1/120Hz (1 samples every 2 minutes)*/
    HDC2010_ODR_010 = 2, /*010 = 1/60Hz (1 samples every minute)*/
    HDC2010_ODR_011 = 3, /*011 = 0.1Hz (1 samples every 10 seconds)*/
    HDC2010_ODR_100 = 4, /*100 = 0.2 Hz (1 samples every 5 second)*/
    HDC2010_ODR_101 = 5, /*101 = 1Hz (1 samples every second)*/
    HDC2010_ODR_110 = 6, /*110 = 2Hz (2 samples every second)*/
    HDC2010_ODR_111 = 7, /*111 = 5Hz (5 samples every second)*/
} hdc2010_output_rate_t; /*Output Data Rate*/

typedef enum {
    HDC2010_HEATER_OFF = 0, /*0 = Heater off*/
    HDC2010_HEATER_ON = 1, /*1 = Heater on*/
} hdc2010_heat_en_t; /**/

typedef enum {
    HDC2010_DRDY_INT_EN_HIGH_Z = 0, /*0 = High Z*/
    HDC2010_DRDY_INT_EN_ENABLE = 1, /*1 = Enable*/
} hdc2010_drdy_int_en_t; /*DRDY/INT_EN pin configuration*/

typedef enum {
    HDC2010_INT_POL_ACTIVE_LOW = 0, /*0 = Active Low*/
    HDC2010_INT_POL_ACTIVE_HIGH = 1, /*1 = Active High*/
} hdc2010_int_pol_t; /*Interrupt polarity*/

typedef enum {
    HDC2010_INT_LEVEL_SENSITIVE = 0, /*0 = Level sensitive*/
    HDC2010_INT_COMPARATOR_MODE = 1, /*1 = Comparator mode*/
} hdc2010_int_mode_t; /*Interrupt mode*/

/* Address 0x0F Measurement Configuration */

typedef enum {
    HDC2010_TRES_BIT_14 = 0, /*00: 14 bit*/
    HDC2010_TRES_BIT_11 = 1, /*01: 11 bit*/
    HDC2010_TRES_BIT_8 = 2, /*10: 8 bit*/
    HDC2010_TRES_NONE = 3, /*11: NA (TBC)*/
} hdc2010_tres_t; /*Temperature resolution*/

typedef enum {
    HDC2010_HRES_BIT_14 = 0, /*00: 14 bit*/
    HDC2010_HRES_BIT_11 = 1, /*01: 11 bit*/
    HDC2010_HRES_BIT_8 = 2, /*10: 8 bit*/
    HDC2010_HRES_NONE = 3, /*11: NA (TBC)*/
} hdc2010_hres_t; /*Humidity resolution*/

typedef enum {
    HDC2010_MEAS_CONF_HUM_AND_TEMP = 0, /*00: Humidity + Temperature*/
    HDC2010_MEAS_CONF_HUM_ONLY = 1, /*01: Temperature only*/
    HDC2010_MEAS_CONF_TEMP_ONLY = 2, /*10: Humidity Only*/
    HDC2010_MEAS_CONF_NONE = 3, /*11: NA */
} hdc2010_meas_conf_t; /*Measurement configuration*/

typedef enum {
    HDC2010_MEAS_STOP = 0, /*0: no action */
    HDC2010_MEAS_START = 1, /*1: Start measurement*/
} hdc2010_meas_trig_t; /*Measurement trigger*/

/**
 * @brief  hdc2010
 */
typedef struct {
    hdc2010_drdy_status_t drdy_status : 1;
    hdc2010_hh_status_t hh_status : 1;
    hdc2010_hl_status_t hl_status : 1;
    hdc2010_th_status_t th_status : 1;
    hdc2010_tl_status_t tl_status : 1;
} hdc2010_interrupt_info_t;

/**
 * @brief  hdc2010
 */
typedef struct {
    hdc2010_drdy_mask_t drdy_mask : 1;
    hdc2010_th_mask_t th_mask : 1;
    hdc2010_tl_mask_t tl_mask : 1;
    hdc2010_hh_mask_t hh_mask : 1;
    hdc2010_hl_mask_t hl_mask : 1;
} hdc2010_interrupt_config_t;

/**
 * @brief  hdc2010
 */
typedef struct {
    hdc2010_soft_res_t soft_res : 1;
    hdc2010_output_rate_t output_rate : 3;
    hdc2010_heat_en_t heat_en : 1;
    hdc2010_drdy_int_en_t int_en : 1;
    hdc2010_int_pol_t int_pol : 1;
    hdc2010_int_mode_t int_mode : 1;
} hdc2010_reset_and_drdy_t;

/**
 * @brief  hdc2010
 */
typedef struct {
    hdc2010_tres_t tres : 2;
    hdc2010_hres_t hres : 2;
    hdc2010_meas_conf_t meas_conf : 2;
    hdc2010_meas_trig_t meas_trig : 1;
} hdc2010_measurement_config_t;

typedef void *hdc2010_handle_t;

/**
 * @brief   create hdc2010 handle_t
 *
 * @param   bus device object handle of hdc2010
 * @param   dev_addr device address
 *
 * @return
 *     - hdc2010 handle_t
 */
hdc2010_handle_t hdc2010_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief   delete hdc2010 handle_t
 *
 * @param   sensor point to device object handle of hdc2010
 * @param   del_bus device address
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t hdc2010_delete(hdc2010_handle_t *sensor);

/**
 * @brief   get temperature
 *
 * @param   sensor device object handle of hdc2010
 *
 * @return
 * 	   - HDC2010_ERR_VAL if fails
 *     - Others temperature value
 */
float hdc2010_get_temperature(hdc2010_handle_t sensor);

/**
 * @brief
 *
 * @param sensor device object handle of hdc2010
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t hdc2010_default_init(hdc2010_handle_t sensor);

/**
 * @brief   get humidity
 *
 * @param   sensor device object handle of hdc2010
 *
 * @return
 * 	   - HDC2010_ERR_VAL if fails
 *     - Others, humidity value
 */
float hdc2010_get_humidity(hdc2010_handle_t sensor);

/**
 * @brief   get interrupt info
 *
 * @param   sensor device object handle of hdc2010
 * @param   info hdc2010_interrupt_info_t info
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t hdc2010_get_interrupt_info(hdc2010_handle_t sensor, hdc2010_interrupt_info_t *info);

/**
 * @brief   set interrupt config
 *
 * @param   sensor device object handle of hdc2010
 * @param   config hdc2010_interrupt_config_t info
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t hdc2010_set_interrupt_config(hdc2010_handle_t sensor, hdc2010_interrupt_config_t *config);

/**
 * @brief   get maximum temperature
 *
 * @param   sensor device object handle of hdc2010
 *
 * @return
 *      - HDC2010_ERR_VAL if fails
 *      - Others, maximum temperature value
 */
float hdc2010_get_max_temperature(hdc2010_handle_t sensor);

/**
 * @brief   get maximum humidity
 *
 * @param   sensor device object handle of hdc2010
 *
 * @return
 *     - HDC2010_ERR_VAL if fails
 *     - Others, maximum humidity value
 */
float hdc2010_get_max_humidity(hdc2010_handle_t sensor);

/**
 * @brief   set temperature offset
 *
 * @param   sensor device object handle of hdc2010
 * @param   offset_data offset data
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t hdc2010_set_temperature_offset(hdc2010_handle_t sensor, uint8_t offset_data);

/**
 * @brief   set humidity offset
 *
 * @param   sensor device object handle of hdc2010
 * @param   offset_data offset data
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t hdc2010_set_humidity_offset(hdc2010_handle_t sensor, uint8_t offset_data);

/**
 * @brief   set temperature threshold
 *
 * @param   sensor device object handle of hdc2010
 * @param   temperature_data temperature  value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t hdc2010_set_temperature_threshold(hdc2010_handle_t sensor, float temperature_data);

/**
 * @brief   set humidity threshold
 *
 * @param   sensor device object handle of hdc2010
 * @param   humidity_data humidity  value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t hdc2010_set_humidity_threshold(hdc2010_handle_t sensor, float humidity_data);

/**
 * @brief   set reset and drdy
 *
 * @param   sensor device object handle of hdc2010
 * @param   config hdc2010_reset_and_drdy_t
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t hdc2010_set_reset_and_drdy(hdc2010_handle_t sensor, hdc2010_reset_and_drdy_t *config);

/**
 * @brief   set measurement config
 *
 * @param   sensor device object handle of hdc2010
 * @param   config hdc2010_measurement_config_t
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t hdc2010_set_measurement_config(hdc2010_handle_t sensor, hdc2010_measurement_config_t *config);

/**
 * @brief   set manufacturer id
 *
 * @param  sensor device object handle of hdc2010
 *
 * @return
 *      - HDC2010_ERR_VAL if fails
 *      - manufacturer id value
 */
int hdc2010_get_manufacturer_id(hdc2010_handle_t sensor);

/**
 * @brief   set device id
 *
 * @param  sensor device object handle of hdc2010
 *
 * @return
 *      - HDC2010_ERR_VAL if fails
 *      - device id value
 */
int hdc2010_get_device_id(hdc2010_handle_t sensor);

#ifdef __cplusplus
}
#endif

#endif
