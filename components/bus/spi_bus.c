// Copyright 2019-2020 Espressif Systems (Shanghai) PTE LTD
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
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include "spi_bus.h"

typedef struct {
    spi_host_device_t host_id;    /*!<spi device number */
    bool is_init;
    spi_bus_config_t conf;    /*!<spi bus active configuration */
} _spi_bus_t;

typedef struct {
    spi_device_handle_t handle;
    spi_bus_handle_t spi_bus;    /*!<spi bus handle */
    spi_device_interface_config_t conf;    /*!<spi device active configuration */
    SemaphoreHandle_t mutex;    /* mutex to achive device thread-safe*/
} _spi_device_t;

static const char *TAG = "spi_bus";
static _spi_bus_t s_spi_bus[2];
#define ESP_SPI_MUTEX_TICKS_TO_WAIT 2

#define SPI_BUS_CHECK(a, str, ret)  if(!(a)) {                                      \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                                   \
    }

#define SPI_DEVICE_MUTEX_TAKE(p_spi_dev, ret) if (!xSemaphoreTake((p_spi_dev)->mutex, ESP_SPI_MUTEX_TICKS_TO_WAIT)) { \
        ESP_LOGE(TAG, "spi device(%d) take mutex timeout, max wait = %d ticks", (int32_t)((p_spi_dev)->handle), ESP_SPI_MUTEX_TICKS_TO_WAIT); \
        return (ret); \
    }

#define SPI_DEVICE_MUTEX_GIVE(p_spi_dev, ret) if (!xSemaphoreGive((p_spi_dev)->mutex)) { \
        ESP_LOGE(TAG, "spi device(%d) give mutex failed", (int32_t)((p_spi_dev)->handle)); \
        return (ret); \
    }

#define MSB_32_SET(out, in) { uint8_t * d = (uint8_t *)&(in); (out) = d[3] | (d[2] << 8) | (d[1] << 16) | (d[0] << 24); }
#define MSB_24_SET(out, in) { uint8_t * d = (uint8_t *)&(in); (out) = d[2] | (d[1] << 8) | (d[0] << 16); }
#define MSB_16_SET(out, in) { (out) = (((in) & 0xFF00) >> 8) | (((in) & 0xFF) << 8); }
#define MSB_PIX_SET(out, in) { uint8_t * d = (uint8_t *)&(in); (out) = d[1] | (d[0] << 8) | (d[3] << 16) | (d[2] << 24); }

spi_bus_handle_t spi_bus_create(spi_host_device_t host_id, gpio_num_t miso_io_num, gpio_num_t mosi_io_num, gpio_num_t sclk_io_num)
{
    SPI_BUS_CHECK(SPI1_HOST < host_id && host_id <= SPI3_HOST, "Invalid spi host_id", NULL);
    uint8_t index = host_id - 1; //find related index
    spi_bus_config_t buscfg = {
        .miso_io_num = miso_io_num,
        .mosi_io_num = mosi_io_num,
        .sclk_io_num = sclk_io_num,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1024,
    };
    int dma_chan = host_id; //set dma channel equals to host_id by default
    esp_err_t ret = spi_bus_initialize(host_id, &buscfg, dma_chan);
    SPI_BUS_CHECK(ESP_OK == ret, "spi bus create failed", NULL);
    s_spi_bus[index].host_id = host_id;
    memcpy(&s_spi_bus[index].conf, &buscfg, sizeof(spi_bus_config_t));
    s_spi_bus[index].is_init = true;
    ESP_LOGI(TAG, "SPI%d bus created", host_id + 1);
    return (spi_bus_handle_t)&s_spi_bus[index];
}

esp_err_t spi_bus_delete(spi_bus_handle_t *bus_handle)
{
    SPI_BUS_CHECK((NULL != bus_handle) && (NULL != *bus_handle), "Handle error", ESP_ERR_INVALID_ARG);
    _spi_bus_t *spi_bus = (_spi_bus_t *)(*bus_handle);

    if (!spi_bus->is_init) {
        ESP_LOGW(TAG, "spi_bus%d has been de-inited", spi_bus->host_id);
        return ESP_FAIL;
    }

    esp_err_t ret = spi_bus_free(spi_bus->host_id);
    SPI_BUS_CHECK(ESP_OK == ret, "spi bus delete failed", ESP_FAIL);
    ESP_LOGI(TAG, "SPI%d bus delete", spi_bus->host_id + 1);
    memset(spi_bus, 0, sizeof(_spi_bus_t));
    *bus_handle = NULL;
    return ESP_OK;
}

spi_bus_device_handle_t spi_bus_device_create(spi_bus_device_handle_t bus_handle, gpio_num_t cs_io_num, uint8_t mode, int clock_speed_hz)
{
    SPI_BUS_CHECK(NULL != bus_handle, "Pointer error", NULL);
    _spi_bus_t *spi_bus = (_spi_bus_t *)bus_handle;

    _spi_device_t *spi_dev = malloc(sizeof(_spi_device_t));
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = clock_speed_hz,
        .duty_cycle_pos = 128,      //50% duty cycle
        .mode = mode,
        .spics_io_num = cs_io_num,
        .cs_ena_posttrans = 3,      //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
        .queue_size = 3
    };
    esp_err_t ret = spi_bus_add_device(spi_bus->host_id, &devcfg, &spi_dev->handle);

    if (ESP_OK != ret) {
        free(spi_dev);
        ESP_LOGE(TAG, "add spi device failed");
        return NULL;
    }

    spi_dev->mutex = xSemaphoreCreateMutex();

    if (NULL == spi_dev->mutex) {
        free(spi_dev);
        ESP_LOGE(TAG, "spi device create mutex failed");
        return NULL;
    }

    spi_dev->spi_bus = bus_handle;
    memcpy(&spi_dev->conf, &devcfg, sizeof(spi_device_interface_config_t));
    ESP_LOGI(TAG, "SPI%d bus device added, CS=%d Mode=%u Speed=%d", spi_bus->host_id + 1, cs_io_num, mode, clock_speed_hz);
    return (spi_bus_device_handle_t)spi_dev;
}

esp_err_t spi_bus_device_delete(spi_bus_device_handle_t *device_handle)
{
    SPI_BUS_CHECK((NULL != device_handle) && (NULL != *device_handle), "Pointer error", ESP_ERR_INVALID_ARG);
    _spi_device_t *spi_dev = (_spi_device_t *)(*device_handle);
    _spi_bus_t *spi_bus = (_spi_bus_t *)(spi_dev->spi_bus);

    SPI_DEVICE_MUTEX_TAKE(spi_dev, ESP_FAIL);
    esp_err_t ret = spi_bus_remove_device(spi_dev->handle);
    SPI_DEVICE_MUTEX_GIVE(spi_dev, ESP_FAIL);
    SPI_BUS_CHECK(ESP_OK == ret, "spi bus delete device failed", ret);

    vSemaphoreDelete(spi_dev->mutex);
    ESP_LOGI(TAG, "SPI%d device removed, CS=%d", spi_bus->host_id + 1, spi_dev->conf.spics_io_num);
    free(spi_dev);
    *device_handle = NULL;
    return ESP_OK;
}

/* this function should lable with inline*/
inline static esp_err_t _spi_device_polling_transmit(spi_bus_device_handle_t device_handle, spi_transaction_t *trans)
{
    SPI_BUS_CHECK(NULL != device_handle, "Pointer error", ESP_ERR_INVALID_ARG);
    _spi_device_t *spi_dev = (_spi_device_t *)(device_handle);
    esp_err_t ret;
    SPI_DEVICE_MUTEX_TAKE(spi_dev, ESP_FAIL);
    ret = spi_device_polling_transmit(spi_dev->handle, trans);
    SPI_DEVICE_MUTEX_GIVE(spi_dev, ESP_FAIL);
    return ret;
}

esp_err_t spi_bus_transfer_byte(spi_bus_device_handle_t device_handle, uint8_t data_out, uint8_t *data_in)
{
    esp_err_t ret;
    spi_transaction_t trans = {
        .length = 8,
        .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
        .tx_data = {
            [0] = data_out
        }
    };
    ret = _spi_device_polling_transmit(device_handle, &trans);
    SPI_BUS_CHECK(ret == ESP_OK, "spi transfer byte failed", ret);

    if (data_in) {
        *data_in = trans.rx_data[0];
    }

    return ESP_OK;
}

esp_err_t spi_bus_transfer_bytes(spi_bus_device_handle_t device_handle, const uint8_t *data_out, uint8_t *data_in, uint32_t data_len)
{
    esp_err_t ret;
    spi_transaction_t trans = {
        .length = data_len * 8,
        .tx_buffer = NULL,
        .rx_buffer = NULL
    };

    if (data_out) {
        trans.tx_buffer = data_out;
    }

    if (data_in) {
        trans.rx_buffer = data_in;
    }

    ret = _spi_device_polling_transmit(device_handle, &trans);
    SPI_BUS_CHECK(ret == ESP_OK, "spi transfer bytes failed", ret);

    return ESP_OK;
}

esp_err_t spi_bus_transfer_reg16(spi_bus_device_handle_t device_handle, uint16_t data_out, uint16_t *data_in)
{
    esp_err_t ret;
    spi_transaction_t trans = {
        .length = 16,
        .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
        /* default MSB first */
        .tx_data = {
            [0] = (data_out >> 8) & 0xff,
            [1] = data_out & 0xff,
        }
    };
    ret = _spi_device_polling_transmit(device_handle, &trans);
    SPI_BUS_CHECK(ret == ESP_OK, "spi transfer reg16 failed", ret);

    if (data_in) {
        *data_in = (trans.rx_data[0] << 8) | (trans.rx_data[1]);
    }

    return ESP_OK;
}

esp_err_t spi_bus_transfer_reg32(spi_bus_device_handle_t device_handle, uint32_t data_out, uint32_t *data_in)
{
    esp_err_t ret;
    spi_transaction_t trans = {
        .length = 32,
        .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
        /* default MSB first */
        .tx_data = {
            [0] = (data_out >> 24) & 0xff,
            [1] = (data_out >> 16) & 0xff,
            [2] = (data_out >> 8) & 0xff,
            [3] = data_out & 0xff
        }
    };
    ret = _spi_device_polling_transmit(device_handle, &trans);
    SPI_BUS_CHECK(ret == ESP_OK, "spi transfer reg32 failed", ret);

    if (data_in) {
        *data_in = (trans.rx_data[0] << 24) | (trans.rx_data[1] << 16) | (trans.rx_data[2] << 8) | (trans.rx_data[3]);
    }

    return ESP_OK;
}
