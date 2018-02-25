/* ILI9341_SPI example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
 Some info about the ILI9341: It has an C/D line, which is connected to a GPIO here. It expects this
 line to be low for a command and high for data. We use a pre-transmit callback to control that line

 every transaction has as the user-definable argument the needed state of the D/C line and just
 before the transaction is sent, the callback will set this line to the correct state.
 Note: If not using ESP WROVER KIT, Users must change the pins by setting macro in file spi_ili.c

 To change the fonts style include the header of your desired font from the "component/includes" folder
 and simply pass the address of its GFXfont to setFont API
*/

/*C Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*RTOS Includes*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/*SPI Includes*/
#include "driver/spi_master.h"
#include "iot_lcd.h"
#include "Adafruit_GFX.h"
#include "image.h"
#include "lcd_image.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event_loop.h"

/*Include desired font here*/
#define PROGMEM
#include "FreeSans9pt7b.h"
#include "FreeMono9pt7b.h"
#include "FreeSerif9pt7b.h"
#include "FreeSerifItalic9pt7b.h"
#include "unity.h"

static CEspLcd* tft_obj = NULL;

//-------------TEST CODE-----------------
float target_room_temperature = 23.5;

int color(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 248) | g >> 5) << 8 | ((g & 28) << 3 | b >> 3);
}

int drawPlaceholder(CEspLcd* tft, int x, int y, int width, int height, int bordercolor, const char* headertext, int header_text_offset, const GFXfont* font)
{
    int headersize = 20;
    tft->setTextColor(COLOR_GREEN, COLOR_BLACK);

    tft->drawRoundRect(x, y, width, height, 3, bordercolor);
    if (font) {
        tft->setFont(font);
        tft->drawString(headertext, x + header_text_offset, y + 1 + 14);
    } else {
        tft->drawString(headertext, x + header_text_offset, y + 1);
    }
    tft->drawFastHLine(x, y + headersize, width, bordercolor);
    tft->setFont(NULL);
    return y + headersize;
}
const GFXfont* title_font = &FreeSerif9pt7b;
const GFXfont* text_font = &FreeSerif9pt7b;
const GFXfont* num_font = &FreeSerif9pt7b;

void update_huminity(CEspLcd* tft, int hum)
{
    tft->setFont(num_font);
    char dtmp[10];
    memset(dtmp, 0, sizeof(dtmp));
    sprintf(dtmp, "%d %%", hum);
    tft->fillRect(193, 32, 25, 26, COLOR_BLACK);
    tft->drawString(dtmp, 195, 50);
}

void update_brightness(CEspLcd* tft, float bright)
{
    tft->fillRect(193, 72, 55, 26, COLOR_BLACK);
    tft->drawFloat(bright, 1, 195, 90);
}

void drawTargetTemp(CEspLcd* tft, float temp)
{
    tft->setTextColor(COLOR_WHITE, COLOR_BLACK);
    tft->drawFloatSevSeg(temp, 1, 7, 54, 7);
}

void drawWireFrame(CEspLcd* tft)
{
    tft->setTextColor(COLOR_GREEN, COLOR_BLACK);
    //Target placeholder
    drawPlaceholder(tft, 0, 28, 136, 78, COLOR_RED, "Temperature", 30, title_font);
    tft->setFont(NULL);
    //Temperatures placeholder
    int placeholderbody = drawPlaceholder(tft, 138, 0, 180, 106, COLOR_RED, "Sensor", 60, title_font);

    tft->setTextColor(COLOR_WHITE, COLOR_BLACK);
    tft->setFont(num_font);
    tft->drawBitmap(150, placeholderbody + 5 , (uint16_t*)water_pic_35, 35, 35);
    update_huminity(tft, 68);

    tft->drawBitmap(150, placeholderbody + 45 , (uint16_t*)brightness_pic_35, 35, 35);
    update_brightness(tft, 1536.12);
    //Status placeholder
    tft->setTextColor(COLOR_GREEN, COLOR_BLACK);
    placeholderbody = drawPlaceholder(tft, 0, 140, 319, 97, COLOR_RED, "STATUS", 110, title_font);

    tft->setTextColor(COLOR_WHITE, COLOR_BLACK);
    tft->setFont(NULL);
    tft->drawString("WiFi   : ", 6, placeholderbody + 2);
    tft->drawString("Signal : ", 6, placeholderbody + 22);
    tft->drawString("Status : ", 6, placeholderbody + 42);
}

void setupUI(CEspLcd* tft)
{
    tft->setRotation(3);
    tft->fillScreen(COLOR_BLACK);

    tft->drawBitmap(0, 0, Status_320_240, 320, 240);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    while (1) {
        tft->fillRect(0, 0, 320, 240, COLOR_BLUE);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        tft->fillRect(0, 0, 320, 240, COLOR_GREEN);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        tft->invertDisplay(1);
        tft->drawBitmap(0, 0, Status_320_240, 320, 240);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        tft->fillScreen(COLOR_BLACK);
        tft->drawBitmap(0, 0, esp_logo, 137, 26);
        drawWireFrame(tft);
        drawTargetTemp(tft, target_room_temperature);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        break;
    }
    tft->invertDisplay(0);
    tft->fillScreen(COLOR_BLACK);
    tft->drawBitmap(0, 0, esp_logo, 137, 26);
    drawWireFrame(tft);
    drawTargetTemp(tft, target_room_temperature);
}
//=======================================

//--------- SENSOR ----------
#include "driver/i2c.h"
#include "iot_i2c_bus.h"
#include "iot_hts221.h"
#include "iot_bh1750.h"

#define I2C_MASTER_SCL_IO           GPIO_NUM_27          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           GPIO_NUM_26          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static hts221_handle_t hts221 = NULL;
static bh1750_handle_t bh1750 = NULL;


void hts221_test_task()
{
    uint8_t hts221_deviceid;

    iot_hts221_get_deviceid(hts221, &hts221_deviceid);
    printf("hts221 device ID is: %02x\n", hts221_deviceid);
    hts221_config_t hts221_config;
    iot_hts221_get_config(hts221, &hts221_config);

    hts221_config.avg_h = HTS221_AVGH_32;
    hts221_config.avg_t = HTS221_AVGT_16;
    hts221_config.odr = HTS221_ODR_1HZ;
    hts221_config.bdu_status = HTS221_DISABLE;
    hts221_config.heater_status = HTS221_DISABLE;
    iot_hts221_set_config(hts221, &hts221_config);
    iot_hts221_set_activate(hts221);
}

/**
 * @brief i2c master initialization
 */
void i2c_sensor_init()
{
//    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(I2C_MASTER_NUM, &conf);
    hts221 = iot_hts221_create(i2c_bus, HTS221_I2C_ADDRESS);

    uint8_t hts221_deviceid;
    iot_hts221_get_deviceid(hts221, &hts221_deviceid);
    printf("hts221 device ID is: %02x\n", hts221_deviceid);
    hts221_config_t hts221_config;
    iot_hts221_get_config(hts221, &hts221_config);
    printf("avg_h is: %02x\n", hts221_config.avg_h);
    printf("avg_t is: %02x\n", hts221_config.avg_t);
    printf("odr is: %02x\n", hts221_config.odr);
    printf("bdu_status is: %02x\n", hts221_config.bdu_status);
    printf("heater_status is: %02x\n", hts221_config.heater_status);
    printf("irq_level is: %02x\n", hts221_config.irq_level);
    printf("irq_output_type is: %02x\n", hts221_config.irq_output_type);
    printf("irq_enable is: %02x\n", hts221_config.irq_enable);
    hts221_config.avg_h = HTS221_AVGH_32;
    hts221_config.avg_t = HTS221_AVGT_16;
    hts221_config.odr = HTS221_ODR_1HZ;
    hts221_config.bdu_status = HTS221_DISABLE;
    hts221_config.heater_status = HTS221_DISABLE;
    iot_hts221_set_config(hts221, &hts221_config);
    iot_hts221_set_activate(hts221);

    bh1750 = iot_bh1750_create(i2c_bus, 0x23);
    hts221_test_task();
}

// -------------------------
#define LCD_DATA_TEMP   (1)
#define LCD_DATA_HUM    (2)
#define LCD_DATA_BRI    (3)

QueueHandle_t lcd_data_queue = NULL;

typedef struct
{
    int type;
    union
    {
        float temp;
        int hum;
        float brightness;
    };
} lcd_data_t;


extern "C" void demo_lcd_init()
{
    if (lcd_data_queue == NULL) {
        lcd_data_queue = xQueueCreate(10, sizeof(lcd_data_t));
    }
    /*Initialize LCD*/
    lcd_conf_t lcd_pins = {
        .lcd_model    = LCD_MOD_AUTO_DET,
        .pin_num_miso = GPIO_NUM_25,
        .pin_num_mosi = GPIO_NUM_23,
        .pin_num_clk  = GPIO_NUM_19,
        .pin_num_cs   = GPIO_NUM_22,
        .pin_num_dc   = GPIO_NUM_21,
        .pin_num_rst  = GPIO_NUM_18,
        .pin_num_bckl = GPIO_NUM_5,
        .clk_freq     = 30000000,
        .rst_active_level = 0,
        .bckl_active_level = 0,
        .spi_host = HSPI_HOST,
        .init_spi_bus = true,
    };

    if (tft_obj == NULL) {
        tft_obj = new CEspLcd(&lcd_pins);
    }
    setupUI(tft_obj);
}

void lcd_update_task(void* arg)
{
    lcd_data_t data;
    while (1) {
        int ret = xQueueReceive(lcd_data_queue, &data, portMAX_DELAY);
        if (ret == pdTRUE) {
            switch (data.type) {
            case LCD_DATA_TEMP:
                printf("val recv: %f\n", data.temp);
                drawTargetTemp(tft_obj, data.temp);
                break;
            case LCD_DATA_HUM:
                printf("recv hum: %d\n", data.hum);
                update_huminity(tft_obj, data.hum);
                break;
            case LCD_DATA_BRI:
                printf("recv bri: %f\n", data.brightness);
                update_brightness(tft_obj, data.brightness);
                break;
            default:
                printf("data type error: %d\n", data.type);
            }
        }
    }
}

void demo_sensor_read_task(void* arg)
{

    int16_t temperature, temperature_last = 0;
    int16_t humidity, humidity_last = 0;
    float bh1750_data, bh1750_data_last = 0;
    lcd_data_t data;

    while (1) {
        printf("\n********HTS221 HUMIDITY&TEMPERATURE SENSOR********\n");
        iot_hts221_get_humidity(hts221, &humidity);
        printf("humidity value is: %2.2f\n", (float)humidity / 10);
        iot_hts221_get_temperature(hts221, &temperature);
        printf("temperature value is: %2.2f\n", (float)temperature / 10);
        if (temperature_last != temperature) {
            data.type = LCD_DATA_TEMP;
            data.temp = (float) temperature / 10;
            printf("send temp: %f\n", data.temp);
            xQueueSend(lcd_data_queue, &data, portMAX_DELAY);
            temperature_last = temperature;
        }
        if (humidity_last != humidity) {
            data.type = LCD_DATA_HUM;
            data.hum = humidity / 10;
            printf("send hum: %d\n", data.hum);
            xQueueSend(lcd_data_queue, &data, portMAX_DELAY);
            humidity_last = humidity;
        }

        iot_bh1750_power_on(bh1750);
        iot_bh1750_set_measure_mode(bh1750, BH1750_ONETIME_4LX_RES);
        vTaskDelay(30 / portTICK_RATE_MS);

        iot_bh1750_get_data(bh1750, &bh1750_data);
        printf("brightness: %f\n", bh1750_data);
        if (bh1750_data_last != bh1750_data) {
            data.type = LCD_DATA_BRI;
            data.brightness = bh1750_data;
            printf("send hum: %f\n", data.brightness);
            xQueueSend(lcd_data_queue, &data, portMAX_DELAY);
            bh1750_data_last = bh1750_data;
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

TEST_CASE("LCD thermostat test", "[lcd_demo][iot]")
{
    i2c_sensor_init();
    demo_lcd_init();
    xTaskCreate(lcd_update_task, "lcd_update_task", 2048 * 2, NULL, 10, NULL);
    xTaskCreate(demo_sensor_read_task, "demo_sensor_read_task", 2048, NULL, 10, NULL);
}


