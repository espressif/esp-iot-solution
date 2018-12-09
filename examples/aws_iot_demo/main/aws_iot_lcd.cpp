/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "apps/sntp/sntp.h"

/*SPI & LCD Driver Includes*/
#include "iot_lcd.h"
#include "FreeSans9pt7b.h"
#include "iot_wifi_conn.h"
#include "aws_iot_demo.h"
#include "image.h"

#define AWS_DEMO_USE_LCD   CONFIG_ESP32_AWS_LCD

typedef enum {
    LCD_EVT_WIFI_CONNECTING,
    LCD_EVT_SUB_CB,
    LCD_EVT_PUB_CB,
    LCD_EVT_AWS_CONNECTING,
    LCD_EVT_AWS_CONNECTED,
} lcd_evt_type_t;

#if AWS_DEMO_USE_LCD
CEspLcd* tft = NULL;
QueueHandle_t lcd_queue = NULL;
typedef struct {
    lcd_evt_type_t type;
    void* pdata;
} lcd_evt_t;

static void _lcd_disp_time(uint8_t cnt, int y)
{
    time_t now;
    char event_time[10];
    struct tm timeinfo;
    /*Print the time of message*/
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(event_time, sizeof(event_time), "%l:%M %p", &timeinfo);
    tft->drawString(event_time, 260, y + (cnt * 14));
}
#endif

void app_lcd_task(void* arg)
{
#if AWS_DEMO_USE_LCD
    lcd_evt_t evt;
    uint8_t pub_cnt = 0;
    uint8_t sub_cnt = 0;
    for (;;) {
        portBASE_TYPE ret = xQueueReceive(lcd_queue, &evt, portMAX_DELAY);
        if (ret == pdTRUE) {
            switch(evt.type) {
                case LCD_EVT_WIFI_CONNECTING:
                    char str_name[50];
                    sprintf(str_name, "Status: Connecting to %s", EXAMPLE_WIFI_SSID);
                    tft->fillRect(0, 30, 230, 10, COLOR_ESP_BKGD);
                    tft->drawString(str_name, 5, 30);
                    break;
                case LCD_EVT_SUB_CB:
                    /*Clear screen if count has reached 5 already,Cnt to monitor num of messages on the screen*/
                    if (sub_cnt == 5) {
                        sub_cnt = 0;
                        tft->fillRect(5, 68, 320, 71, COLOR_ESP_BKGD);
                    }
                    /*Print the message*/
                    tft->drawString((char*) evt.pdata, 5, 72 + (sub_cnt * 14));
                    _lcd_disp_time(sub_cnt, 72);
                    sub_cnt++;
                    break;
                case LCD_EVT_AWS_CONNECTING:
                    //Update status on LCD
                    tft->fillRect(0, 30, 230, 10, COLOR_ESP_BKGD);
                    tft->drawString("Status: Connecting to AWS...", 5, 30);
                    break;
                case LCD_EVT_AWS_CONNECTED:
                    //Update status on LCD
                    tft->fillRect(0, 30, 230, 10, COLOR_ESP_BKGD);
                    tft->drawString("Status: Connected to AWS...", 5, 30);
                    //Prepare title fonts
                    char str_print[20];
                    tft->setFont(&FreeSans9pt7b);
                    tft->setTextColor(COLOR_GREENYELLOW, COLOR_ESP_BKGD);
                    tft->drawFastHLine(0, 45, 320, COLOR_YELLOW);
                    sprintf(str_print, "Subscribing to %s", SUBSCRIBE_TOPIC);
                    tft->drawString(str_print, 5, 60);
                    tft->drawFastHLine(0, 145, 320, COLOR_YELLOW);
                    sprintf(str_print, "Publishing to %s", PUBLISH_TOPIC);
                    tft->drawString(str_print, 5, 162);
                    //Back to normal fonts
                    tft->setFont(NULL);
                    tft->setTextColor(COLOR_WHITE, COLOR_ESP_BKGD);
                    break;
                case LCD_EVT_PUB_CB:
                    //Clear screen if it is full
                    if (pub_cnt == 5) {
                        tft->fillRect(5, 167, 320, 72, COLOR_ESP_BKGD);
                        pub_cnt = 0;
                    }
                    // print message
                    tft->drawString((const char*) evt.pdata, 5, 170 + (pub_cnt * 14));
                    _lcd_disp_time(pub_cnt, 170);
                    pub_cnt++;
                    break;
                default:
                    break;
            }
        }
    }
#endif
}

static void _lcd_send_event(int type, void* pdata)
{
#if AWS_DEMO_USE_LCD
    lcd_evt_t evt;
    evt.type = (lcd_evt_type_t) type;
    evt.pdata = pdata;
    xQueueSend(lcd_queue, &evt, portMAX_DELAY);
#endif
}

void app_lcd_aws_connected()
{
    _lcd_send_event(LCD_EVT_AWS_CONNECTED, NULL);
}

void app_lcd_aws_connecting()
{
    _lcd_send_event(LCD_EVT_AWS_CONNECTING, NULL);
}

void app_lcd_aws_pub_cb(void* pdata)
{
    _lcd_send_event(LCD_EVT_PUB_CB, pdata);
}

void app_lcd_aws_sub_cb(void* pdata)
{
    _lcd_send_event(LCD_EVT_SUB_CB, pdata);
}

void app_lcd_wifi_connecting()
{
    _lcd_send_event(LCD_EVT_WIFI_CONNECTING, NULL);
}

void app_lcd_init()
{
#if AWS_DEMO_USE_LCD
    if (lcd_queue == NULL) {
        lcd_queue = xQueueCreate(10, sizeof(lcd_evt_t));
        xTaskCreate(app_lcd_task, "app_lcd_task", 2048, NULL, 6, NULL);
    }
    /*ESP Wrover kit config*/
    lcd_conf_t lcd_pins = {
        .lcd_model = LCD_MOD_ST7789,
        .pin_num_miso = GPIO_NUM_25,
        .pin_num_mosi = GPIO_NUM_23,
        .pin_num_clk = GPIO_NUM_19,
        .pin_num_cs = GPIO_NUM_22,
        .pin_num_dc = GPIO_NUM_21,
        .pin_num_rst = GPIO_NUM_18,
        .pin_num_bckl = GPIO_NUM_5,
        .clk_freq = 20 * 1000 * 1000,
        .rst_active_level = 0,
        .bckl_active_level = 0,
        .spi_host = HSPI_HOST,
        .init_spi_bus = true,
    };

    /*Initialize SPI Handler*/
    if (tft == NULL) {
        tft = new CEspLcd(&lcd_pins);
    }

    /*screen initialize*/
    tft->invertDisplay(false);
    tft->setRotation(1);             //Landscape mode
    tft->fillScreen(COLOR_ESP_BKGD);
    tft->drawBitmap(0, 0, esp_logo, 137, 26);
    tft->drawBitmap(243, 0, aws_logo, 77, 31);
#endif
}
