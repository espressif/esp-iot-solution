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
#include "mvh3004d.h"

static const char *TAG = "mvh3004d";
typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
} mvh3004d_dev_t;

mvh3004d_handle_t mvh3004d_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    mvh3004d_dev_t *sensor = (mvh3004d_dev_t *) calloc(1, sizeof(mvh3004d_dev_t));
    sensor->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sensor->i2c_dev == NULL) {
        free(sensor);
        return NULL;
    }
    sensor->dev_addr = dev_addr;
    return (mvh3004d_handle_t) sensor;
}

esp_err_t mvh3004d_delete(mvh3004d_handle_t *sensor)
{
    if (*sensor == NULL) {
        return ESP_OK;
    }
    mvh3004d_dev_t *sens = (mvh3004d_dev_t *)(*sensor);
    i2c_bus_device_delete(&sens->i2c_dev);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

esp_err_t mvh3004d_get_data(mvh3004d_handle_t sensor, float *tp, float *rh)
{
    mvh3004d_dev_t *sens = (mvh3004d_dev_t *) sensor;
    uint8_t data[4] = {0};
    esp_err_t ret = i2c_bus_read_bytes(sens->i2c_dev, NULL_I2C_MEM_ADDR, 4, data);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SEND READ ERROR \n");
        return ESP_FAIL;
    }

    if (rh) {
        *rh = (float)(((data[0] << 8) | data[1]) & 0x3fff) * 100 / (16384 - 1);
    }

    if (tp) {
        *tp = ((float)((data[2] << 6) | data[3] >> 2)) * 165 / 16383 - 40;
    }

    return ESP_OK;
}

esp_err_t mvh3004d_get_huminity(mvh3004d_handle_t sensor, float *rh)
{
    return mvh3004d_get_data(sensor, NULL, rh);
}

esp_err_t mvh3004d_get_temperature(mvh3004d_handle_t sensor, float *tp)
{
    return mvh3004d_get_data(sensor, tp, NULL);
}
