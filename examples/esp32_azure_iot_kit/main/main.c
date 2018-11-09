/* ESP32 Azure Iot Kit Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "iot_button.h"
#include "iot_i2c_bus.h"
#include "iot_hts221.h"
#include "iot_bh1750.h"
#include "iot_ssd1306.h"
#include "ssd1306_fonts.h"
#include "iot_mpu6050.h"

#define I2C_MASTER_SCL_IO           26          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           25          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

#define BUTTON_IO_NUM               0
#define BUTTON_ACTIVE_LEVEL         BUTTON_ACTIVE_LOW

static button_handle_t page_btn = NULL;
static i2c_bus_handle_t i2c_bus = NULL;
static bh1750_handle_t bh1750_dev = NULL;
static hts221_handle_t hts221_dev = NULL;
static ssd1306_handle_t ssd1306_dev = NULL;
static mpu6050_handle_t mpu6050_dev = NULL;

static xQueueHandle q_page_num;
static uint8_t g_page_num = 0;

static mpu6050_acce_value_t acce;
static mpu6050_gyro_value_t gyro;
static complimentary_angle_t complimentary_angle;

static const char *TAG = "esp32_azure_kit_main";

typedef struct {
    float temp;
    float humi;
    float lumi;
} sensor_data_t;

void page_btn_tap_cb()
{
    g_page_num++;
    if (g_page_num >= 4) {
        g_page_num = 0;
    }
    xQueueSend(q_page_num, &g_page_num, 0);
}

void page_button_init()
{
    page_btn = iot_button_create((gpio_num_t)BUTTON_IO_NUM, BUTTON_ACTIVE_LEVEL);
    iot_button_set_evt_cb(page_btn, BUTTON_CB_TAP, page_btn_tap_cb, "TAP");
}

void i2c_bus_init()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(I2C_MASTER_NUM, &conf);
}

void bh1750_init()
{
    bh1750_dev = iot_bh1750_create(i2c_bus, BH1750_I2C_ADDRESS_DEFAULT);
    iot_bh1750_power_on(bh1750_dev);
    iot_bh1750_set_measure_mode(bh1750_dev, BH1750_CONTINUE_4LX_RES);
}

void hts221_init()
{
    hts221_dev = iot_hts221_create(i2c_bus, HTS221_I2C_ADDRESS);

    hts221_config_t hts221_config;    
    iot_hts221_get_config(hts221_dev, &hts221_config);

    hts221_config.avg_h = HTS221_AVGH_32;
    hts221_config.avg_t = HTS221_AVGT_16;
    hts221_config.odr = HTS221_ODR_1HZ;
    hts221_config.bdu_status = HTS221_DISABLE;
    hts221_config.heater_status = HTS221_DISABLE;
    iot_hts221_set_config(hts221_dev, &hts221_config);
    
    iot_hts221_set_activate(hts221_dev);
}

esp_err_t ssd1306_show_signs(ssd1306_handle_t dev)
{
    iot_ssd1306_draw_bitmap(dev, 0, 2, &c_chSingal816[0], 16, 8);
    iot_ssd1306_draw_bitmap(dev, 24, 2, &c_chBluetooth88[0], 8, 8);
    iot_ssd1306_draw_bitmap(dev, 40, 2, &c_chMsg816[0], 16, 8);
    iot_ssd1306_draw_bitmap(dev, 64, 2, &c_chGPRS88[0], 8, 8);
    iot_ssd1306_draw_bitmap(dev, 90, 2, &c_chAlarm88[0], 8, 8);
    iot_ssd1306_draw_bitmap(dev, 112, 2, &c_chBat816[0], 16, 8);

    return iot_ssd1306_refresh_gram(dev);
}

void ssd1306_init()
{
    ssd1306_dev = iot_ssd1306_create(i2c_bus, SSD1306_I2C_ADDRESS);
    iot_ssd1306_refresh_gram(ssd1306_dev);
    iot_ssd1306_clear_screen(ssd1306_dev, 0x00);
    ssd1306_show_signs(ssd1306_dev);
}

void mpu6050_init()
{
    mpu6050_dev = iot_mpu6050_create(i2c_bus, MPU6050_I2C_ADDRESS);
    iot_mpu6050_wake_up(mpu6050_dev);
    iot_mpu6050_set_acce_fs(mpu6050_dev, ACCE_FS_4G);
    iot_mpu6050_set_gyro_fs(mpu6050_dev, GYRO_FS_500DPS);
}

void dev_init()
{
    page_button_init();
    i2c_bus_init();
    bh1750_init();
    hts221_init();
    ssd1306_init();
    mpu6050_init();
}

esp_err_t ssd1306_show_data(ssd1306_handle_t dev , sensor_data_t sensor_data)
{
    char sensor_data_str[10] = {0};

    sprintf(sensor_data_str, "%.1f", sensor_data.temp);
    iot_ssd1306_draw_string(dev, 70, 16, (const uint8_t *) sensor_data_str, 16, 1);

    sprintf(sensor_data_str, "%.1f", sensor_data.humi);
    iot_ssd1306_draw_string(dev, 70, 32, (const uint8_t *) sensor_data_str, 16, 1);

    sprintf(sensor_data_str, "%.1f", sensor_data.lumi);
    iot_ssd1306_draw_string(dev, 70, 48, (const uint8_t *) sensor_data_str, 16, 1);

    return iot_ssd1306_refresh_gram(dev);
}

/* show environment sensor data */
void ssd1306_show_env_data(void)
{
    int16_t temp;
    int16_t humi;
    float lumi;
    char data_str[10] = {0};

    iot_ssd1306_draw_string(ssd1306_dev, 0, 16, (const uint8_t *) "Temp:", 16, 1);
    iot_ssd1306_draw_string(ssd1306_dev, 0, 32, (const uint8_t *) "Humi:", 16, 1);
    iot_ssd1306_draw_string(ssd1306_dev, 0, 48, (const uint8_t *) "Lumi:", 16, 1);
    iot_ssd1306_refresh_gram(ssd1306_dev);
    
    iot_hts221_get_temperature(hts221_dev, &temp);
    iot_hts221_get_humidity(hts221_dev, &humi);
    iot_bh1750_get_data(bh1750_dev, &lumi);
    ESP_LOGI(TAG, "temperature: %.1f, humidity: %.1f, luminance: %.1f", (float)temp/10, (float)humi/10, lumi);
    
    sprintf(data_str, "%.1f", (float)temp/10);
    iot_ssd1306_draw_string(ssd1306_dev, 70, 16, (const uint8_t *) data_str, 16, 1);
    sprintf(data_str, "%.1f", (float)humi/10);
    iot_ssd1306_draw_string(ssd1306_dev, 70, 32, (const uint8_t *) data_str, 16, 1);
    sprintf(data_str, "%.1f", lumi);
    iot_ssd1306_draw_string(ssd1306_dev, 70, 48, (const uint8_t *) data_str, 16, 1);

    iot_ssd1306_refresh_gram(ssd1306_dev);
}

/* show acceleration data */
void ssd1306_show_acce_data(void)
{
    char data_str[10] = {0};

    iot_ssd1306_draw_string(ssd1306_dev, 0, 16, (const uint8_t *) "Acce_x:", 16, 1);
    iot_ssd1306_draw_string(ssd1306_dev, 0, 32, (const uint8_t *) "Acce_y:", 16, 1);
    iot_ssd1306_draw_string(ssd1306_dev, 0, 48, (const uint8_t *) "Acce_z:", 16, 1);

    ESP_LOGI(TAG, "acce_x:%.2f, acce_y:%.2f, acce_z:%.2f", acce.acce_x, acce.acce_y, acce.acce_z);

    sprintf(data_str, "%.2f", acce.acce_x);
    iot_ssd1306_draw_string(ssd1306_dev, 70, 16, (const uint8_t *) data_str, 16, 1);
    sprintf(data_str, "%.2f", acce.acce_y);
    iot_ssd1306_draw_string(ssd1306_dev, 70, 32, (const uint8_t *) data_str, 16, 1);
    sprintf(data_str, "%.2f", acce.acce_z);
    iot_ssd1306_draw_string(ssd1306_dev, 70, 48, (const uint8_t *) data_str, 16, 1);
    
    iot_ssd1306_refresh_gram(ssd1306_dev);
}

/* show gyroscope data */
void ssd1306_show_gyro_data(void)
{
    char data_str[10] = {0};

    iot_ssd1306_draw_string(ssd1306_dev, 0, 16, (const uint8_t *) "Gyro_x:", 16, 1);
    iot_ssd1306_draw_string(ssd1306_dev, 0, 32, (const uint8_t *) "Gyro_y:", 16, 1);
    iot_ssd1306_draw_string(ssd1306_dev, 0, 48, (const uint8_t *) "Gyro_z:", 16, 1);

    ESP_LOGI(TAG, "gyro_x:%.2f, gyro_y:%.2f, gyro_z:%.2f", gyro.gyro_x, gyro.gyro_y, gyro.gyro_z);

    sprintf(data_str, "%.2f", gyro.gyro_x);
    iot_ssd1306_draw_string(ssd1306_dev, 70, 16, (const uint8_t *) data_str, 16, 1);
    sprintf(data_str, "%.2f", gyro.gyro_y);
    iot_ssd1306_draw_string(ssd1306_dev, 70, 32, (const uint8_t *) data_str, 16, 1);
    sprintf(data_str, "%.2f", gyro.gyro_z);
    iot_ssd1306_draw_string(ssd1306_dev, 70, 48, (const uint8_t *) data_str, 16, 1);
    
    iot_ssd1306_refresh_gram(ssd1306_dev);
}

/* show complimentary angle */
void ssd1306_show_complimentary_angle(void)
{
    char data_str[10] = {0};
    
    iot_ssd1306_draw_string(ssd1306_dev, 0, 16, (const uint8_t *) "Roll:", 16, 1);
    iot_ssd1306_draw_string(ssd1306_dev, 0, 32, (const uint8_t *) "Pitch:", 16, 1);

    ESP_LOGI(TAG, "roll:%.2f, pitch:%.2f", complimentary_angle.roll, complimentary_angle.pitch);

    sprintf(data_str, "%.2f", complimentary_angle.roll);
    iot_ssd1306_draw_string(ssd1306_dev, 70, 16, (const uint8_t *) data_str, 16, 1);
    sprintf(data_str, "%.2f", complimentary_angle.pitch);
    iot_ssd1306_draw_string(ssd1306_dev, 70, 32, (const uint8_t *) data_str, 16, 1);
    
    iot_ssd1306_refresh_gram(ssd1306_dev);
}

void ssd1306_show_task(void* pvParameters)
{
    uint8_t page_num = 0;
    while (1) {
        if (xQueueReceive(q_page_num, &page_num, 1000 / portTICK_RATE_MS) == pdTRUE) {
            iot_ssd1306_clear_screen(ssd1306_dev, 0x00);
            ssd1306_show_signs(ssd1306_dev);
        }
        
        switch (page_num) {
            case 0:
                ssd1306_show_env_data();
                break;
            case 1:
                ssd1306_show_acce_data();
                break;
            case 2:
                ssd1306_show_gyro_data();
                break;
            case 3:
                ssd1306_show_complimentary_angle();
                break;
            default:
                break;
        }
    }
    
    iot_ssd1306_delete(ssd1306_dev, true);
    vTaskDelete(NULL);
}

void mpu6050_read_task(void* pvParameters)
{
    while (1) {
        iot_mpu6050_get_acce(mpu6050_dev, &acce);
        iot_mpu6050_get_gyro(mpu6050_dev, &gyro);
        iot_mpu6050_complimentory_filter(mpu6050_dev, &acce, &gyro, &complimentary_angle);
        vTaskDelay(5 / portTICK_RATE_MS);
    }
}

void app_main()
{
    dev_init();
    vTaskDelay(1000 / portTICK_RATE_MS);
    q_page_num = xQueueCreate(10, sizeof(uint8_t));
    xTaskCreate(ssd1306_show_task, "ssd1306_show_task", 2048 * 2, NULL, 5,NULL);
    xTaskCreate(mpu6050_read_task, "mpu6050_read_task", 2048 * 2, NULL, 5,NULL);
}

