/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "iot_hts221.h"
#include "iot_bh1750.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc.h"
#include "driver/rtc_io.h"
#include "lowpower_evb_power.h"
#include "lowpower_evb_sensor.h"
 
 /* Power control */
#define SENSOR_POWER_CNTL_IO        ((gpio_num_t) 27)
 
 /* Sensor */
#define SENSOR_I2C_PORT             ((i2c_port_t) 1)
#define SENSOR_SCL_IO               ((gpio_num_t) 32)
#define SENSOR_SDA_IO               ((gpio_num_t) 33)

CPowerCtrl *sensor_power = NULL;
CI2CBus *i2c_bus_sens    = NULL;
CHts221 *hts221          = NULL;
CBh1750 *bh1750          = NULL;

void sensor_power_on()
{
    sensor_power = new CPowerCtrl(SENSOR_POWER_CNTL_IO);
    sensor_power->on();
}

void sensor_init()
{
    i2c_bus_sens = new CI2CBus((i2c_port_t) SENSOR_I2C_PORT, (gpio_num_t) SENSOR_SCL_IO, (gpio_num_t) SENSOR_SDA_IO);
    hts221 = new CHts221(i2c_bus_sens);
    
    bh1750 = new CBh1750(i2c_bus_sens);
    bh1750->on();
    bh1750->set_mode(BH1750_ONETIME_4LX_RES);
}

float sensor_hts221_get_hum()
{
    return hts221->read_humidity();
}

float sensor_hts221_get_temp()
{
    return hts221->read_temperature();
}

float sensor_bh1750_get_lum()
{
    return bh1750->read();
}

void rtc_sensor_power_on(void)
{
    rtc_gpio_init(SENSOR_POWER_CNTL_IO);
    rtc_gpio_set_direction(SENSOR_POWER_CNTL_IO, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(SENSOR_POWER_CNTL_IO, 0);
}

void rtc_sensor_io_init(void)
{
    rtc_gpio_init(SENSOR_SCL_IO);
    rtc_gpio_set_direction(SENSOR_SCL_IO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_init(SENSOR_SDA_IO);
    rtc_gpio_set_direction(SENSOR_SDA_IO, RTC_GPIO_MODE_INPUT_ONLY);
}

