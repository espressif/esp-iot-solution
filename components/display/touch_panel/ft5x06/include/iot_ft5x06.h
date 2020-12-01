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
#ifndef _IOT_BME280_H_
#define _IOT_BME280_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/i2c.h"
#include "iot_i2c_bus.h"

#define FT5X06_ADDR_DEF    (0x38)

#define SCREEN_XSIZE       (479)
#define SCREEN_YSIZE       (799)

#define WRITE_BIT          (I2C_MASTER_WRITE)       /*!< I2C master write */
#define READ_BIT           (I2C_MASTER_READ)        /*!< I2C master read */
#define ACK_CHECK_EN       0x1                      /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS      0x0                      /*!< I2C master will not check ack from slave */
#define ACK_VAL            0x0                      /*!< I2C ack value */
#define NACK_VAL           0x1                      /*!< I2C nack value */

#define    FT5X0X_REG_THGROUP                0x80   /* touch threshold related to sensitivity */
#define    FT5X0X_REG_THPEAK                 0x81
#define    FT5X0X_REG_THCAL                  0x82
#define    FT5X0X_REG_THWATER                0x83
#define    FT5X0X_REG_THTEMP                 0x84
#define    FT5X0X_REG_THDIFF                 0x85                
#define    FT5X0X_REG_CTRL                   0x86
#define    FT5X0X_REG_TIMEENTERMONITOR       0x87
#define    FT5X0X_REG_PERIODACTIVE           0x88   /* report rate */
#define    FT5X0X_REG_PERIODMONITOR          0x89
#define    FT5X0X_REG_HEIGHT_B               0x8a
#define    FT5X0X_REG_MAX_FRAME              0x8b
#define    FT5X0X_REG_DIST_MOVE              0x8c
#define    FT5X0X_REG_DIST_POINT             0x8d
#define    FT5X0X_REG_FEG_FRAME              0x8e
#define    FT5X0X_REG_SINGLE_CLICK_OFFSET    0x8f
#define    FT5X0X_REG_DOUBLE_CLICK_TIME_MIN  0x90
#define    FT5X0X_REG_SINGLE_CLICK_TIME      0x91
#define    FT5X0X_REG_LEFT_RIGHT_OFFSET      0x92
#define    FT5X0X_REG_UP_DOWN_OFFSET         0x93
#define    FT5X0X_REG_DISTANCE_LEFT_RIGHT    0x94
#define    FT5X0X_REG_DISTANCE_UP_DOWN       0x95
#define    FT5X0X_REG_ZOOM_DIS_SQR           0x96
#define    FT5X0X_REG_RADIAN_VALUE           0x97
#define    FT5X0X_REG_MAX_X_HIGH             0x98
#define    FT5X0X_REG_MAX_X_LOW              0x99
#define    FT5X0X_REG_MAX_Y_HIGH             0x9a
#define    FT5X0X_REG_MAX_Y_LOW              0x9b
#define    FT5X0X_REG_K_X_HIGH               0x9c
#define    FT5X0X_REG_K_X_LOW                0x9d
#define    FT5X0X_REG_K_Y_HIGH               0x9e
#define    FT5X0X_REG_K_Y_LOW                0x9f
#define    FT5X0X_REG_AUTO_CLB_MODE          0xa0
#define    FT5X0X_REG_LIB_VERSION_H          0xa1
#define    FT5X0X_REG_LIB_VERSION_L          0xa2        
#define    FT5X0X_REG_CIPHER                 0xa3
#define    FT5X0X_REG_MODE                   0xa4
#define    FT5X0X_REG_PMODE                  0xa5   /* Power Consume Mode        */    
#define    FT5X0X_REG_FIRMID                 0xa6   /* Firmware version */
#define    FT5X0X_REG_STATE                  0xa7
#define    FT5X0X_REG_FT5201ID               0xa8
#define    FT5X0X_REG_ERR                    0xa9
#define    FT5X0X_REG_CLB                    0xaa

typedef enum {
    TOUCH_EVT_RELEASE = 0x0,
    TOUCH_EVT_PRESS   = 0x1,
} touch_evt_t;

typedef struct {
    uint8_t touch_event;
    uint8_t touch_point;
    uint16_t curx[5];
    uint16_t cury[5];
} touch_info_t;

typedef struct {
    int reversed;
} ft5x06_cfg_t;

typedef struct {
    i2c_bus_handle_t bus;
    uint8_t dev_addr;
    bool xy_swap;
    uint16_t x_size;
    uint16_t y_size;
    //touch_info_t tch_info;
} ft5x06_dev_t;

typedef void*  ft5x06_handle_t;

/**
 * @brief FT5X06 read
 *
 * @param dev object handle of FT5X06.
 * @param start_addr The address of the data to be read.
 * @param read_num The size of the data to be read.
 * @param data_buf Pointer to read data.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ft5x06_read(ft5x06_handle_t dev, uint8_t start_addr,
        uint8_t read_num, uint8_t *data_buf);

/**
 * @brief FT5X06 read
 *
 * @param dev object handle of FT5X06.
 * @param start_addr The address of the data to be read.
 * @param write_num The size of the data to be write.
 * @param data_buf Pointer to write data.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ft5x06_write(ft5x06_handle_t dev, uint8_t start_addr,
        uint8_t write_num, uint8_t *data_buf);

/**
 * @brief FT5X06 read
 *
 * @param dev object handle of FT5X06.
 * @param touch_info_t a pointer of touch_info_t contained touch information.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ft5x06_touch_report(ft5x06_handle_t dev, touch_info_t* ifo);

/**
 * @brief FT5X06 read
 *
 * @param dev object handle of FT5X06.
 * @param dev_addr decice address
 *
 * @return
 *     - ft5x06_handle_t
 */
ft5x06_handle_t iot_ft5x06_create(i2c_bus_handle_t bus, uint16_t dev_addr);

/**
 * @brief FT5X06 read
 *
 * @param dev object handle of FT5X06.
 * @param ft5x06_cfg_t ft5x06 configuration.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ft5x06_init(ft5x06_handle_t dev, ft5x06_cfg_t * cfg);

#ifdef __cplusplus
}
#endif

#endif
