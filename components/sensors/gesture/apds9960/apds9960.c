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
#include "apds9960.h"

#define APDS9960_TIMEOUT_MS_DEFAULT   (1000)
typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
    uint32_t timeout;
    apds9960_control_t _control_t; /*< config control register>*/
    apds9960_enable_t _enable_t;   /*< config enable register>*/
    apds9960_config1_t _config1_t; /*< config config1 register>*/
    apds9960_config2_t _config2_t; /*< config config2 register>*/
    apds9960_config3_t _config3_t; /*< config config3 register>*/
    apds9960_gconf1_t _gconf1_t;   /*< config gconfig1 register>*/
    apds9960_gconf2_t _gconf2_t;   /*< config gconfig2 register>*/
    apds9960_gconf3_t _gconf3_t;   /*< config gconfig3 register>*/
    apds9960_gconf4_t _gconf4_t;   /*< config gconfig4 register>*/
    apds9960_status_t _status_t;   /*< config status register>*/
    apds9960_gstatus_t _gstatus_t; /*< config gstatus register>*/
    apds9960_propulse_t _ppulse_t; /*< config pro pulse register>*/
    apds9960_gespulse_t _gpulse_t; /*< config ges pulse register>*/
    apds9960_pers_t _pers_t;       /*< config pers register>*/
    uint8_t gest_cnt;              /*< counter of gesture >*/
    uint8_t up_cnt;                /*< counter of up gesture >*/
    uint8_t down_cnt;              /*< counter of down gesture >*/
    uint8_t left_cnt;              /*< counter of left gesture >*/
    uint8_t right_cnt;             /*< counter of right gesture >*/
} apds9960_dev_t;

static float __powf(const float x, const float y)
{
    return (float)(pow((double) x, (double) y));
}

uint8_t apds9960_get_enable(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return (sens->_enable_t.gen << 6) | (sens->_enable_t.pien << 5) | (sens->_enable_t.aien << 4)
           | (sens->_enable_t.wen << 3) | (sens->_enable_t.pen << 2) | (sens->_enable_t.aen << 1) | sens->_enable_t.pon;
}

uint8_t apds9960_get_pers(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return (sens->_pers_t.ppers << 4) | sens->_pers_t.apers;
}

uint8_t apds9960_get_ppulse(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return (sens->_ppulse_t.pplen << 6) | sens->_ppulse_t.ppulse;
}

uint8_t apds9960_get_gpulse(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return (sens->_gpulse_t.gplen << 6) | sens->_gpulse_t.gpulse;
}

uint8_t apds9960_get_control(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return (sens->_control_t.leddrive << 6) | (sens->_control_t.pgain << 2) | sens->_control_t.again;
}

uint8_t apds9960_get_config1(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return sens->_config1_t.wlong << 1;
}

uint8_t apds9960_get_config2(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return (sens->_config2_t.psien << 7) | (sens->_config2_t.cpsien << 6) | (sens->_config2_t.led_boost << 4) | 1;
}

uint8_t apds9960_get_config3(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return (sens->_config3_t.pcmp << 5) | (sens->_config3_t.sai << 4) | (sens->_config3_t.pmask_u << 3)
           | (sens->_config3_t.pmask_d << 2) | (sens->_config3_t.pmask_l << 1) | sens->_config3_t.pmask_r;
}

void apds9960_set_status(apds9960_handle_t sensor, uint8_t data)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_status_t.avalid = data & 0x01;
    sens->_status_t.pvalid = (data >> 1) & 0x01;
    sens->_status_t.gint = (data >> 2) & 0x01;
    sens->_status_t.aint = (data >> 4) & 0x01;
    sens->_status_t.pint = (data >> 5) & 0x01;
    sens->_status_t.pgsat = (data >> 6) & 0x01;
    sens->_status_t.cpsat = (data >> 7) & 0x01;
}

void apds9960_set_gstatus(apds9960_handle_t sensor, uint8_t data)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_gstatus_t.gfov = (data >> 1) & 0x01;
    sens->_gstatus_t.gvalid = data & 0x01;
}

uint8_t apds9960_get_gconf1(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return (sens->_gconf1_t.gfifoth << 7) | (sens->_gconf1_t.gexmsk << 5) | sens->_gconf1_t.gexpers;
}

uint8_t apds9960_get_gconf2(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return (sens->_gconf2_t.ggain << 5) | (sens->_gconf2_t.gldrive << 3) | sens->_gconf2_t.gwtime;
}

uint8_t apds9960_get_gconf3(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return sens->_gconf3_t.gdims;
}

uint8_t apds9960_get_gconf4(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return (sens->_gconf4_t.gien << 1) | sens->_gconf4_t.gmode;
}

void apds9960_set_gconf4(apds9960_handle_t sensor, uint8_t data)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_gconf4_t.gien = (data >> 1) & 0x01;
    sens->_gconf4_t.gmode = data & 0x01;
}

void apds9960_reset_counts(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->gest_cnt = 0;
    sens->up_cnt = 0;
    sens->down_cnt = 0;
    sens->left_cnt = 0;
    sens->right_cnt = 0;
}

esp_err_t apds9960_set_timeout(apds9960_handle_t sensor, uint32_t tout_ms)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->timeout = tout_ms;
    return ESP_OK;
}

esp_err_t apds9960_get_deviceid(apds9960_handle_t sensor, uint8_t *deviceid)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, APDS9960_WHO_AM_I_REG, &tmp);
    *deviceid = tmp;
    return ret;
}

esp_err_t apds9960_set_mode(apds9960_handle_t sensor, apds9960_mode_t mode)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    uint8_t tmp;

    if (i2c_bus_read_byte(sens->i2c_dev, APDS9960_MODE_ENABLE, &tmp) != ESP_OK) {
        return ESP_FAIL;
    }

    tmp |= mode;
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_MODE_ENABLE, tmp);
}

apds9960_mode_t apds9960_get_mode(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    uint8_t value;

    /* Read current ENABLE register */
    if (i2c_bus_read_byte(sens->i2c_dev, APDS9960_MODE_ENABLE, &value) != ESP_OK) {
        return ESP_FAIL;
    }

    return (apds9960_mode_t) value;
}

esp_err_t apds9960_enable_gesture_engine(apds9960_handle_t sensor, bool en)
{
    esp_err_t ret;
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;

    if (!en) {
        sens->_gconf4_t.gmode = 0;
        if (i2c_bus_write_byte(sens->i2c_dev, APDS9960_GCONF4,
                               (sens->_gconf4_t.gien << 1) | sens->_gconf4_t.gmode) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    sens->_enable_t.gen = en;
    ret = i2c_bus_write_byte(sens->i2c_dev, APDS9960_MODE_ENABLE,
                             (sens->_enable_t.gen << 6) | (sens->_enable_t.pien << 5) | (sens->_enable_t.aien << 4)
                             | (sens->_enable_t.wen << 3) | (sens->_enable_t.pen << 2) | (sens->_enable_t.aen << 1)
                             | sens->_enable_t.pon);
    apds9960_reset_counts(sensor);
    return ret;
}

esp_err_t apds9960_set_led_drive_boost(apds9960_handle_t sensor, apds9960_leddrive_t drive,
        apds9960_ledboost_t boost)
{
    // set BOOST
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_config2_t.led_boost = boost;

    if (i2c_bus_write_byte(sens->i2c_dev, APDS9960_CONFIG2,
                           (sens->_config2_t.psien << 7) | (sens->_config2_t.cpsien << 6) | (sens->_config2_t.led_boost << 4)
                           | 1) != ESP_OK) {
        return ESP_FAIL;
    }

    sens->_control_t.leddrive = drive;
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_CONTROL,
                              ((sens->_control_t.leddrive << 6) | (sens->_control_t.pgain << 2) | sens->_control_t.again));
}

esp_err_t apds9960_set_wait_time(apds9960_handle_t sensor, uint8_t time)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_WTIME, time);
}

esp_err_t apds9960_set_adc_integration_time(apds9960_handle_t sensor, uint16_t iTimeMS)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    float temp;
    // convert ms into 2.78ms increments
    temp = iTimeMS;
    temp /= 2.78;
    temp = 256 - temp;

    if (temp > 255) {
        temp = 255;
    } else if (temp < 0) {
        temp = 0;
    }

    /* Update the timing register */
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_ATIME, (uint8_t) temp);
}

float apds9960_get_adc_integration_time(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    float temp;
    uint8_t val;

    if (i2c_bus_read_byte(sens->i2c_dev, APDS9960_ATIME, &val) != ESP_OK) {
        return ESP_FAIL;
    }

    temp = val;
    // convert to units of 2.78 ms
    temp = 256 - temp;
    temp *= 2.78;
    return temp;
}

esp_err_t apds9960_set_ambient_light_gain(apds9960_handle_t sensor, apds9960_again_t again)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_control_t.again = again;

    /* Update the timing register */
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_CONTROL,
                              ((sens->_control_t.leddrive << 6) | (sens->_control_t.pgain << 2) | sens->_control_t.again));
}

apds9960_again_t apds9960_get_ambient_light_gain(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    uint8_t val;

    if (i2c_bus_read_byte(sens->i2c_dev, APDS9960_CONTROL, &val) != ESP_OK) {
        return ESP_FAIL;
    }

    return (apds9960_again_t)(val & 0x03);
}

esp_err_t apds9960_enable_gesture_interrupt(apds9960_handle_t sensor, bool en)
{
    esp_err_t ret;
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_enable_t.gen = en;
    ret = i2c_bus_write_byte(sens->i2c_dev, APDS9960_MODE_ENABLE,
                             ((sens->_enable_t.gen << 6) | (sens->_enable_t.pien << 5) | (sens->_enable_t.aien << 4)
                              | (sens->_enable_t.wen << 3) | (sens->_enable_t.pen << 2) | (sens->_enable_t.aen << 1)
                              | sens->_enable_t.pon));
    apds9960_clear_interrupt(sensor);
    return ret;
}

esp_err_t apds9960_enable_proximity_engine(apds9960_handle_t sensor, bool en)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_enable_t.pen = en;
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_MODE_ENABLE,
                              ((sens->_enable_t.gen << 6) | (sens->_enable_t.pien << 5) | (sens->_enable_t.aien << 4)
                               | (sens->_enable_t.wen << 3) | (sens->_enable_t.pen << 2) | (sens->_enable_t.aen << 1)
                               | sens->_enable_t.pon));
}

esp_err_t apds9960_set_proximity_gain(apds9960_handle_t sensor, apds9960_pgain_t pgain)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_control_t.pgain = pgain;

    /* Update the timing register */
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_CONTROL,
                              (sens->_control_t.leddrive << 6) | (sens->_control_t.pgain << 2) | sens->_control_t.again);
}

apds9960_pgain_t apds9960_get_proximity_gain(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    uint8_t val;

    if (i2c_bus_read_byte(sens->i2c_dev, APDS9960_CONTROL, &val) != ESP_OK) {
        return ESP_FAIL;
    }

    return (apds9960_pgain_t)(val & 0x0C);
}

esp_err_t apds9960_set_proximity_pulse(apds9960_handle_t sensor, apds9960_ppulse_len_t pLen, uint8_t pulses)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;

    if (pulses < 1) {
        pulses = 1;
    } else if (pulses > 64) {
        pulses = 64;
    }

    pulses--;
    sens->_ppulse_t.pplen = pLen;
    sens->_ppulse_t.ppulse = pulses;
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_PPULSE, (sens->_ppulse_t.pplen << 6) | sens->_ppulse_t.ppulse);
}

esp_err_t apds9960_enable_color_engine(apds9960_handle_t sensor, bool en)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_enable_t.aen = en;
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_MODE_ENABLE,
                              ((sens->_enable_t.gen << 6) | (sens->_enable_t.pien << 5) | (sens->_enable_t.aien << 4)
                               | (sens->_enable_t.wen << 3) | (sens->_enable_t.pen << 2) | (sens->_enable_t.aen << 1)
                               | sens->_enable_t.pon));
}

bool apds9960_color_data_ready(apds9960_handle_t sensor)
{
    uint8_t data;
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    i2c_bus_read_byte(sens->i2c_dev, APDS9960_STATUS, &data);
    apds9960_set_status(sensor, data);
    return sens->_status_t.avalid;
}

esp_err_t apds9960_get_color_data(apds9960_handle_t sensor, uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    uint8_t data[2] = { 0 };
    i2c_bus_read_bytes(sens->i2c_dev, APDS9960_CDATAL, 2, data);
    *c = (data[0] << 8) | data[1];
    i2c_bus_read_bytes(sens->i2c_dev, APDS9960_RDATAL, 2, data);
    *r = (data[0] << 8) | data[1];
    i2c_bus_read_bytes(sens->i2c_dev, APDS9960_GDATAL, 2, data);
    *g = (data[0] << 8) | data[1];
    i2c_bus_read_bytes(sens->i2c_dev, APDS9960_BDATAL, 2, data);
    *b = (data[0] << 8) | data[1];
    return ESP_OK;
}

uint16_t apds9960_calculate_color_temperature(apds9960_handle_t sensor, uint16_t r, uint16_t g, uint16_t b)
{
    float rgb_xcorrelation, rgb_ycorrelation, rgb_zcorrelation; /* RGB to XYZ correlation      */
    float chromaticity_xc, chromaticity_yc; /* Chromaticity co-ordinates   */
    float formula; /* McCamy's formula            */
    float cct;

    /* 1. Map RGB values to their XYZ counterparts.    */
    /* Based on 6500K fluorescent, 3000K fluorescent   */
    /* and 60W incandescent values for a wide range.   */
    /* Note: Y = Illuminance or lux                    */
    rgb_xcorrelation = (-0.14282F * r) + (1.54924F * g) + (-0.95641F * b);
    rgb_ycorrelation = (-0.32466F * r) + (1.57837F * g) + (-0.73191F * b);
    rgb_zcorrelation = (-0.68202F * r) + (0.77073F * g) + (0.56332F * b);

    /* 2. Calculate the chromaticity co-ordinates      */
    chromaticity_xc = (rgb_xcorrelation) / (rgb_xcorrelation);
    chromaticity_yc = (rgb_ycorrelation) / (rgb_xcorrelation + rgb_ycorrelation + rgb_zcorrelation);

    /* 3. Use McCamy's formula to determine the CCT    */
    formula = (chromaticity_xc - 0.3320F) / (0.1858F - chromaticity_yc);

    /* Calculate the final CCT */
    cct = (449.0F * __powf(formula, 3)) + (3525.0F * __powf(formula, 2)) + (6823.3F * formula) + 5520.33F;

    /* Return the results in degrees Kelvin */
    return (uint16_t) cct;
}

uint16_t apds9960_calculate_lux(apds9960_handle_t sensor, uint16_t r, uint16_t g, uint16_t b)
{
    float illuminance;

    /* This only uses RGB ... how can we integrate clear or calculate lux */
    /* based exclusively on clear since this might be more reliable?      */
    illuminance = (-0.32466F * r) + (1.57837F * g) + (-0.73191F * b);

    return (uint16_t) illuminance;
}

esp_err_t apds9960_enable_color_interrupt(apds9960_handle_t sensor, bool en)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_enable_t.aien = en;
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_MODE_ENABLE,
                              ((sens->_enable_t.gen << 6) | (sens->_enable_t.pien << 5) | (sens->_enable_t.aien << 4)
                               | (sens->_enable_t.wen << 3) | (sens->_enable_t.pen << 2) | (sens->_enable_t.aen << 1)
                               | sens->_enable_t.pon));
}

esp_err_t apds9960_set_int_limits(apds9960_handle_t sensor, uint16_t low, uint16_t high)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    i2c_bus_write_byte(sens->i2c_dev, APDS9960_AILTL, low & 0xFF);
    i2c_bus_write_byte(sens->i2c_dev, APDS9960_AILTH, low >> 8);
    i2c_bus_write_byte(sens->i2c_dev, APDS9960_AIHTL, high & 0xFF);
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_AIHTH, high >> 8);
}

esp_err_t apds9960_enable_proximity_interrupt(apds9960_handle_t sensor, bool en)
{
    esp_err_t ret;
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_enable_t.pien = en;
    ret = i2c_bus_write_byte(sens->i2c_dev, APDS9960_MODE_ENABLE,
                             ((sens->_enable_t.gen << 6) | (sens->_enable_t.pien << 5) | (sens->_enable_t.aien << 4)
                              | (sens->_enable_t.wen << 3) | (sens->_enable_t.pen << 2) | (sens->_enable_t.aen << 1)
                              | sens->_enable_t.pon));
    apds9960_clear_interrupt(sensor);
    return ret;
}

uint8_t apds9960_read_proximity(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    uint8_t data;

    if (i2c_bus_read_byte(sens->i2c_dev, APDS9960_PDATA, &data)) {
        return ESP_FAIL;
    }

    return data;
}

esp_err_t apds9960_set_proximity_interrupt_threshold(apds9960_handle_t sensor, uint8_t low, uint8_t high,
        uint8_t persistance)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    i2c_bus_write_byte(sens->i2c_dev, APDS9960_PILT, low);
    i2c_bus_write_byte(sens->i2c_dev, APDS9960_PIHT, high);

    if (persistance > 7) {
        persistance = 7;
    }

    sens->_pers_t.ppers = persistance;
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_PERS, (sens->_pers_t.ppers << 4) | sens->_pers_t.apers);
}

bool apds9960_get_proximity_interrupt(apds9960_handle_t sensor)
{
    uint8_t data;
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    i2c_bus_read_byte(sens->i2c_dev, APDS9960_STATUS, &data);
    apds9960_set_status(sensor, data);
    return sens->_status_t.pint;
}

esp_err_t apds9960_clear_interrupt(apds9960_handle_t sensor)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    esp_err_t ret = i2c_bus_write_byte(sens->i2c_dev, NULL_I2C_MEM_ADDR, APDS9960_AICLEAR);
    return ret;
}

esp_err_t apds9960_enable(apds9960_handle_t sensor, bool en)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_enable_t.pon = en;
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_MODE_ENABLE,
                              ((sens->_enable_t.gen << 6) | (sens->_enable_t.pien << 5) | (sens->_enable_t.aien << 4)
                               | (sens->_enable_t.wen << 3) | (sens->_enable_t.pen << 2) | (sens->_enable_t.aen << 1)
                               | sens->_enable_t.pon));
}

esp_err_t apds9960_set_gesture_dimensions(apds9960_handle_t sensor, uint8_t dims)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_gconf3_t.gdims = dims;
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_GCONF3, sens->_gconf3_t.gdims);
}

esp_err_t apds9960_set_light_intlow_threshold(apds9960_handle_t sensor, uint16_t threshold)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    uint8_t val_low;
    uint8_t val_high;
    /* Break 16-bit threshold into 2 8-bit values */
    val_low = threshold & 0x00FF;
    val_high = (threshold & 0xFF00) >> 8;

    /* Write low byte */
    if (i2c_bus_write_byte(sens->i2c_dev, APDS9960_AILTL, val_low) != ESP_OK) {
        return ESP_FAIL;
    }

    /* Write high byte */
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_AILTH, val_high);
}

esp_err_t apds9960_set_light_inthigh_threshold(apds9960_handle_t sensor, uint16_t threshold)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    uint8_t val_low;
    uint8_t val_high;
    /* Break 16-bit threshold into 2 8-bit values */
    val_low = threshold & 0x00FF;
    val_high = (threshold & 0xFF00) >> 8;

    /* Write low byte */
    if (i2c_bus_write_byte(sens->i2c_dev, APDS9960_AIHTL, val_low) != ESP_OK) {
        return ESP_FAIL;
    }

    /* Write high byte */
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_AIHTH, val_high);
}

esp_err_t apds9960_set_gesture_fifo_threshold(apds9960_handle_t sensor, uint8_t thresh)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_gconf1_t.gfifoth = thresh;
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_GCONF1,
                              ((sens->_gconf1_t.gfifoth << 7) | (sens->_gconf1_t.gexmsk << 5) | sens->_gconf1_t.gexpers));
}

esp_err_t apds9960_set_gesture_waittime(apds9960_handle_t sensor, apds9960_gwtime_t time)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    uint8_t val;

    /* Read value from GCONF2 register */
    if (i2c_bus_read_byte(sens->i2c_dev, APDS9960_GCONF2, &val) != ESP_OK) {
        return ESP_FAIL;
    }

    /* Set bits in register to given value */
    time &= 0x07;
    val &= 0xf8;
    val |= time;
    /* Write register value back into GCONF2 register */
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_GCONF2, val);
}

esp_err_t apds9960_set_gesture_gain(apds9960_handle_t sensor, apds9960_ggain_t gain)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_gconf2_t.ggain = gain;
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_GCONF2,
                              ((sens->_gconf2_t.ggain << 5) | (sens->_gconf2_t.gldrive << 3) | sens->_gconf2_t.gwtime));
}

esp_err_t apds9960_set_gesture_proximity_threshold(apds9960_handle_t sensor, uint8_t entthresh, uint8_t exitthresh)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    esp_err_t ret;

    if (i2c_bus_write_byte(sens->i2c_dev, APDS9960_GPENTH, entthresh)) {
        return ESP_FAIL;
    }

    ret = i2c_bus_write_byte(sens->i2c_dev, APDS9960_GEXTH, exitthresh);
    return ret;
}

esp_err_t apds9960_set_gesture_offset(apds9960_handle_t sensor, uint8_t offset_up, uint8_t offset_down,
        uint8_t offset_left, uint8_t offset_right)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    i2c_bus_write_byte(sens->i2c_dev, APDS9960_GOFFSET_U, offset_up);
    i2c_bus_write_byte(sens->i2c_dev, APDS9960_GOFFSET_D, offset_down);
    i2c_bus_write_byte(sens->i2c_dev, APDS9960_GOFFSET_L, offset_left);
    i2c_bus_write_byte(sens->i2c_dev, APDS9960_GOFFSET_R, offset_right);
    return ESP_OK;
}

uint8_t apds9960_read_gesture(apds9960_handle_t sensor)
{
    uint8_t toRead;
    uint8_t buf[256];
    unsigned long t = 0;
    uint8_t gestureReceived;
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;

    while (1) {
        int up_down_diff = 0;
        int left_right_diff = 0;
        gestureReceived = 0;

        if (!apds9960_gesture_valid(sensor)) {
            return 0;
        }

        vTaskDelay(30 / portTICK_RATE_MS);
        i2c_bus_read_byte(sens->i2c_dev, APDS9960_GFLVL, &toRead);
        i2c_bus_read_bytes(sens->i2c_dev, APDS9960_GFIFO_U, toRead, buf);

        if (abs((int) buf[0] - (int) buf[1]) > 13) {
            up_down_diff += (int) buf[0] - (int) buf[1];
        }

        if (abs((int) buf[2] - (int) buf[3]) > 13) {
            left_right_diff += (int) buf[2] - (int) buf[3];
        }

        if (up_down_diff != 0) {
            if (up_down_diff < 0) {
                if (sens->down_cnt > 0) {
                    gestureReceived = APDS9960_UP;
                } else {
                    sens->up_cnt++;
                }
            } else if (up_down_diff > 0) {
                if (sens->up_cnt > 0) {
                    gestureReceived = APDS9960_DOWN;
                } else {
                    sens->down_cnt++;
                }
            }
        }

        if (left_right_diff != 0) {
            if (left_right_diff < 0) {
                if (sens->right_cnt > 0) {
                    gestureReceived = APDS9960_LEFT;
                } else {
                    sens->left_cnt++;
                }
            } else if (left_right_diff > 0) {
                if (sens->left_cnt > 0) {
                    gestureReceived = APDS9960_RIGHT;
                } else {
                    sens->right_cnt++;
                }
            }
        }

        if (up_down_diff != 0 || left_right_diff != 0) {
            t = xTaskGetTickCount();
        }

        if (gestureReceived || xTaskGetTickCount() - t > (300 / portTICK_RATE_MS)) {
            apds9960_reset_counts(sensor);
            return gestureReceived;
        }
    }
}

bool apds9960_gesture_valid(apds9960_handle_t sensor)
{
    uint8_t data;
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    i2c_bus_read_byte(sens->i2c_dev, APDS9960_GSTATUS, &data);
    sens->_gstatus_t.gfov = (data >> 1) & 0x01;
    sens->_gstatus_t.gvalid = data & 0x01;
    return sens->_gstatus_t.gvalid;
}

esp_err_t apds9960_set_gesture_pulse(apds9960_handle_t sensor, apds9960_gpulselen_t gpulseLen, uint8_t pulses)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    sens->_gpulse_t.gplen = gpulseLen;
    sens->_gpulse_t.gpulse = pulses; //10 pulses
    return i2c_bus_write_byte(sens->i2c_dev, APDS9960_GPULSE, (sens->_gpulse_t.gplen << 6) | sens->_gpulse_t.gpulse);
}

esp_err_t apds9960_set_gesture_enter_thresh(apds9960_handle_t sensor, uint8_t threshold)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    esp_err_t ret;
    ret = i2c_bus_write_byte(sens->i2c_dev, APDS9960_GPENTH, threshold);
    return ret;
}

esp_err_t apds9960_set_gesture_exit_thresh(apds9960_handle_t sensor, uint8_t threshold)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) sensor;
    esp_err_t ret;
    ret = i2c_bus_write_byte(sens->i2c_dev, APDS9960_GEXTH, threshold);
    return ret;
}

apds9960_handle_t apds9960_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    apds9960_dev_t *sens = (apds9960_dev_t *) calloc(1, sizeof(apds9960_dev_t));
    sens->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sens->i2c_dev == NULL) {
        free(sens);
        return NULL;
    }
    sens->dev_addr = dev_addr;
    sens->timeout = APDS9960_TIMEOUT_MS_DEFAULT;
    return (apds9960_handle_t) sens;
}

esp_err_t apds9960_delete(apds9960_handle_t *sensor)
{
    if (*sensor == NULL) {
        return ESP_OK;
    }

    apds9960_dev_t *sens = (apds9960_dev_t *)(*sensor);
    i2c_bus_device_delete(&sens->i2c_dev);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

esp_err_t apds9960_gesture_init(apds9960_handle_t sensor)
{
    /* Set default values for ambient light and proximity registers */
    apds9960_set_adc_integration_time(sensor, 10);
    apds9960_set_ambient_light_gain(sensor, APDS9960_AGAIN_4X);
    apds9960_enable_gesture_engine(sensor, false);
    apds9960_enable_proximity_engine(sensor, false);
    apds9960_enable_color_engine(sensor, false);
    apds9960_enable_color_interrupt(sensor, false);
    apds9960_enable_proximity_interrupt(sensor, false);
    apds9960_clear_interrupt(sensor);
    apds9960_enable(sensor, false);
    apds9960_enable(sensor, true);
    apds9960_set_gesture_dimensions(sensor, APDS9960_DIMENSIONS_ALL);
    apds9960_set_gesture_fifo_threshold(sensor, APDS9960_GFIFO_4);
    apds9960_set_gesture_gain(sensor, APDS9960_GGAIN_4X);
    apds9960_set_gesture_proximity_threshold(sensor, 50, 0);
    apds9960_reset_counts(sensor);
    apds9960_set_led_drive_boost(sensor, APDS9960_LEDDRIVE_100MA, APDS9960_LEDBOOST_100PCNT);
    apds9960_set_gesture_waittime(sensor, APDS9960_GWTIME_2_8MS);
    apds9960_set_gesture_pulse(sensor, APDS9960_GPULSELEN_32US, 8);
    apds9960_enable_proximity_engine(sensor, true);
    return apds9960_enable_gesture_engine(sensor, true);
}
