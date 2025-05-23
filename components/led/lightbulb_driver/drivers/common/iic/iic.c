/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <esp_log.h>

#include "iic.h"

#define I2C_MASTER_TX_BUF_DISABLE   (0)
#define I2C_MASTER_RX_BUF_DISABLE   (0)
#define ACK_CHECK_EN                (0x1)
#define ACK_CHECK_DIS               (0x0)
#define ACK_VAL                     (0x0)
#define NACK_VAL                    (0x1)
#define MAX_CMD_DATA_LEN            (15)

#ifdef CONFIG_LB_IIC_TASK_STACK
#define IIC_TASK_STACK              CONFIG_LB_IIC_TASK_STACK
#else
#define IIC_TASK_STACK              2048
#endif

#ifdef CONFIG_LB_IIC_QUEUE_SIZE
#define IIC_QUEUE_SIZE              CONFIG_LB_IIC_QUEUE_SIZE
#else
#define IIC_QUEUE_SIZE              20
#endif

#ifdef CONFIG_LB_IIC_TASK_PRIORITY
#define IIC_TASK_PRIORITY           CONFIG_LB_IIC_TASK_PRIORITY
#else
#define IIC_TASK_PRIORITY           20
#endif

static const char *TAG = "iic";

typedef struct {
    QueueHandle_t cmd_queue_handle;
    TaskHandle_t send_task_handle;
#if ENABLE_NEW_IIC_DRIVER
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t i2c_dev_handle;
#else
    i2c_port_t i2c_master_num;
#endif
    esp_err_t last_err;
} iic_config_t;

typedef struct {
    uint8_t addr;
    uint8_t data[MAX_CMD_DATA_LEN];
    size_t real_data_size;
} i2c_send_data_t;

static iic_config_t *s_obj = NULL;

static esp_err_t _write(uint8_t addr, uint8_t *data_wr, size_t size)
{
#if 0
    printf("--------start----------\r\n");
    ESP_LOG_BUFFER_HEX_LEVEL(" _write addr:", &addr, 1, ESP_LOG_INFO);
    ESP_LOG_BUFFER_HEX_LEVEL(" _write data:", data_wr, size, ESP_LOG_INFO);
    printf("--------stop----------\r\n");
#endif

#if ENABLE_NEW_IIC_DRIVER
    i2c_operation_job_t i2c_ops[] = {
        { .command = I2C_MASTER_CMD_START },
        { .command = I2C_MASTER_CMD_WRITE, .write = { .ack_check = 0, .data = (uint8_t *) &addr, .total_bytes = 1 } },
        { .command = I2C_MASTER_CMD_WRITE, .write = { .ack_check = 0, .data = (uint8_t *) data_wr, .total_bytes = size } },
        { .command = I2C_MASTER_CMD_STOP },
    };
    s_obj->last_err = i2c_master_execute_defined_operations(s_obj->i2c_dev_handle, i2c_ops, sizeof(i2c_ops) / sizeof(i2c_operation_job_t), 10);
#else
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr, ACK_CHECK_DIS);
    // AEG-1520
    if (size > 0) {
        i2c_master_write(cmd, data_wr, size, ACK_CHECK_DIS);
    }
    i2c_master_stop(cmd);
    s_obj->last_err = i2c_master_cmd_begin(s_obj->i2c_master_num, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
#endif

    return s_obj->last_err;
}

static void send_task(void *arg)
{
    i2c_send_data_t data;
    while (true) {
        if (xQueueReceive(s_obj->cmd_queue_handle, &data, portMAX_DELAY) == pdPASS) {
            _write(data.addr, data.data, data.real_data_size);
        }
    }
    vTaskDelete(NULL);
}

static void clean_up(void)
{
    if (s_obj->cmd_queue_handle) {
        vQueueDelete(s_obj->cmd_queue_handle);
        s_obj->cmd_queue_handle = NULL;
    }

    if (s_obj->send_task_handle) {
        vTaskDelete(s_obj->send_task_handle);
        s_obj->send_task_handle = NULL;
    }
}

esp_err_t iic_driver_init(i2c_port_t i2c_master_num, gpio_num_t sda_io_num, gpio_num_t scl_io_num, uint32_t clk_freq_hz)
{
    esp_err_t err = ESP_FAIL;
    s_obj = calloc(1, sizeof(iic_config_t));
    DRIVER_CHECK(s_obj, "alloc fail", return ESP_ERR_NO_MEM);

#if ENABLE_NEW_IIC_DRIVER
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = scl_io_num,
        .sda_io_num = sda_io_num,
        .flags.enable_internal_pullup = true,
    };

    err = i2c_new_master_bus(&i2c_bus_config, &s_obj->bus_handle);
    DRIVER_CHECK(err == ESP_OK, "new i2c fail", goto EXIT);

    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = clk_freq_hz,
        .flags.disable_ack_check = true,
        .device_address = 0xFF,
    };

    err = i2c_master_bus_add_device(s_obj->bus_handle, &i2c_dev_conf, &s_obj->i2c_dev_handle);
    DRIVER_CHECK(err == ESP_OK, "add i2c dev", goto EXIT);
#else
    i2c_config_t conf = { 0 };
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda_io_num;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl_io_num;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = clk_freq_hz;
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;
#endif
    err = i2c_param_config(i2c_master_num, &conf);
    DRIVER_CHECK(err == ESP_OK, "i2c param config fail", goto EXIT);

    err = i2c_driver_install(i2c_master_num, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    DRIVER_CHECK(err == ESP_OK, "i2c driver install fail", goto EXIT);

    s_obj->i2c_master_num = i2c_master_num;
#endif

    return ESP_OK;
EXIT:

#if ENABLE_NEW_IIC_DRIVER
    if (s_obj->bus_handle) {
        free(s_obj->bus_handle);
    }
    if (s_obj->i2c_dev_handle) {
        free(s_obj->i2c_dev_handle);
    }
#endif

    if (s_obj) {
        free(s_obj);
    }

    return err;
}

esp_err_t iic_driver_write(uint8_t addr, uint8_t *data_wr, size_t size)
{
    esp_err_t err = s_obj->last_err;
    if (s_obj->cmd_queue_handle && s_obj->send_task_handle) {
        if (uxQueueSpacesAvailable(s_obj->cmd_queue_handle) == 0) {
            // ESP_LOGE(TAG, "queue full, reset");
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, data_wr, size, ESP_LOG_DEBUG);
            xQueueReset(s_obj->cmd_queue_handle);
            err = ESP_FAIL;
        }
        i2c_send_data_t data = { 0 };
        data.addr = addr;
        data.real_data_size = size;
        //AEG-1520
        if (size > 0) {
            memcpy(data.data, data_wr, size);
        }
        if (xQueueSend(s_obj->cmd_queue_handle, &data, 0) != pdTRUE) {
            ESP_LOGE(TAG, "queue send data fail");
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, data_wr, size, ESP_LOG_DEBUG);
            return ESP_FAIL;
        }
        return err;
    }

    return _write(addr, data_wr, size);
}

esp_err_t iic_driver_send_task_create(void)
{
    DRIVER_CHECK(!s_obj->cmd_queue_handle || !s_obj->send_task_handle, "already initialized", return ESP_ERR_INVALID_STATE);

    s_obj->cmd_queue_handle = xQueueCreate(IIC_QUEUE_SIZE, sizeof(i2c_send_data_t));
    DRIVER_CHECK(s_obj->cmd_queue_handle, "queue create fail", goto EXIT);

    xTaskCreate(send_task, "send_task", IIC_TASK_STACK, NULL, IIC_TASK_PRIORITY, &s_obj->send_task_handle);
    DRIVER_CHECK(s_obj->send_task_handle, "task create fail", goto EXIT);

    return ESP_OK;

EXIT:
    clean_up();
    return ESP_ERR_NO_MEM;
}

esp_err_t iic_driver_task_destroy(void)
{
    clean_up();
    return ESP_OK;
}

esp_err_t iic_driver_deinit(void)
{
#if ENABLE_NEW_IIC_DRIVER
    i2c_master_bus_rm_device(s_obj->i2c_dev_handle);
    i2c_del_master_bus(s_obj->bus_handle);
#else
    i2c_driver_delete(s_obj->i2c_master_num);
#endif

    clean_up();
    return ESP_OK;
}
