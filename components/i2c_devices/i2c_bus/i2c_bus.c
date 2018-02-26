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
#include "esp_log.h"
#include "driver/i2c.h"
#include "iot_i2c_bus.h"

typedef struct {
    i2c_config_t i2c_conf;   /*!<I2C bus parameters*/
    i2c_port_t i2c_port;     /*!<I2C port number */
} i2c_bus_t;

static const char* I2C_BUS_TAG = "i2c_bus";
#define I2C_BUS_CHECK(a, str, ret)  if(!(a)) {                                             \
    ESP_LOGE(I2C_BUS_TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);      \
    return (ret);                                                                   \
    }
#define ESP_INTR_FLG_DEFAULT  (0)
#define ESP_I2C_MASTER_BUF_LEN  (0)

i2c_bus_handle_t iot_i2c_bus_create(i2c_port_t port, i2c_config_t* conf)
{
    I2C_BUS_CHECK(port < I2C_NUM_MAX, "I2C port error", NULL);
    I2C_BUS_CHECK(conf != NULL, "Pointer error", NULL);
    i2c_bus_t* bus = (i2c_bus_t*) calloc(1, sizeof(i2c_bus_t));
    bus->i2c_conf = *conf;
    bus->i2c_port = port;
    esp_err_t ret = i2c_param_config(bus->i2c_port, &bus->i2c_conf);
    if(ret != ESP_OK) {
        goto error;
    }
    ret = i2c_driver_install(bus->i2c_port, bus->i2c_conf.mode, ESP_I2C_MASTER_BUF_LEN, ESP_I2C_MASTER_BUF_LEN, ESP_INTR_FLG_DEFAULT);
    if(ret != ESP_OK) {
        goto error;
    }
    return (i2c_bus_handle_t) bus;

    error:
    if(bus) {
        free(bus);
    }
    return NULL;
}

esp_err_t iot_i2c_bus_delete(i2c_bus_handle_t bus)
{
    I2C_BUS_CHECK(bus != NULL, "Handle error", ESP_FAIL);
    i2c_bus_t* i2c_bus = (i2c_bus_t*) bus;
    i2c_driver_delete(i2c_bus->i2c_port);
    free(bus);
    return ESP_OK;
}

esp_err_t iot_i2c_bus_cmd_begin(i2c_bus_handle_t bus, i2c_cmd_handle_t cmd, portBASE_TYPE ticks_to_wait)
{
    I2C_BUS_CHECK(bus != NULL, "Handle error", ESP_FAIL);
    I2C_BUS_CHECK(cmd != NULL, "I2C cmd error", ESP_FAIL);
    i2c_bus_t* i2c_bus = (i2c_bus_t*) bus;
    return i2c_master_cmd_begin(i2c_bus->i2c_port, cmd, ticks_to_wait);
}
