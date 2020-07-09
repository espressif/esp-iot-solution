/* OLED screen Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include "esp_sntp.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/touch_pad.h"
#include "iot_touchpad.h"
#include "iot_apds9960.h"
#include "iot_hts221.h"
#include "iot_wifi_conn.h"

#include "app_oled.h"
#include "app_power.h"

#define AP_SSID      CONFIG_AP_SSID
#define AP_PASSWORD  CONFIG_AP_PASSWORD

//Power control
#define POWER_CNTL_IO              ((gpio_num_t) 19)
CPowerCtrl *periph = NULL;

// Sensor
#define SENSOR_I2C_PORT          ((i2c_port_t) 1)
#define SENSOR_SCL_IO            21
#define SENSOR_SDA_IO            22
CI2CBus *i2c_bus_sens = NULL;
CHts221 *hts221       = NULL;
CApds9960 *gsens      = NULL;

// OLED
CI2CBus *i2c_bus_oled = NULL;
COled *oled = NULL;

// Touch pad
#define TOUCH_PAD_LEFT           ((touch_pad_t) 9)
#define TOUCH_PAD_RIGHT          ((touch_pad_t) 8)
CTouchPad *tp_left = NULL, *tp_right = NULL;

// system
QueueHandle_t oled_queue = NULL;

enum {
    PAGE_TIME = 0,
    PAGE_TEMP,
    PAGE_HUMINITY,
    PAGE_MAX,
};

enum {
    OLED_EVT_PAGE_NEXT,
    OLED_EVT_PAGE_PREV,
    OLED_EVT_SLEEP,
};

typedef struct {
    int type;
} oled_evt_t;

static void i2c_dev_init()
{
    i2c_bus_sens = new CI2CBus((i2c_port_t) SENSOR_I2C_PORT, (gpio_num_t) SENSOR_SCL_IO, (gpio_num_t) SENSOR_SDA_IO);
    hts221 = new CHts221(i2c_bus_sens);
    gsens = new CApds9960(i2c_bus_sens);
    gsens->gesture_init();

    i2c_bus_oled = new CI2CBus((i2c_port_t) OLED_IIC_NUM, (gpio_num_t) OLED_IIC_SCL_NUM, (gpio_num_t) OLED_IIC_SDA_NUM, 100000);
    oled = new COled(i2c_bus_oled);
    oled->init();
}

static void oled_show_page(int page)
{
    static int page_prev = PAGE_MAX;
    if (page_prev == page) {

    } else {
        page_prev = page;
        oled->clean();
    }
    switch (page % PAGE_MAX) {
        case PAGE_TEMP:
            oled->show_temp(hts221->read_temperature());
            break;
        case PAGE_HUMINITY:
            oled->show_humidity(hts221->read_humidity());
            break;
        case PAGE_TIME:
            oled->show_time();
            break;
        default:
            oled->show_time();
            break;
    }
}

static void oled_task(void* arg)
{
    int cur_page = PAGE_TIME;
    portBASE_TYPE ret;
    oled_evt_t evt;
    oled_show_page(cur_page);
    while (1) {
        ret = xQueueReceive(oled_queue, &evt, 500 / portTICK_PERIOD_MS);
        if (ret == pdTRUE) {
            if (evt.type == OLED_EVT_PAGE_NEXT) {
                cur_page += (PAGE_MAX + 1);
                cur_page %= PAGE_MAX;
            } else if (evt.type == OLED_EVT_PAGE_PREV) {
                cur_page += (PAGE_MAX - 1);
                cur_page %= PAGE_MAX;
            } else if (evt.type == OLED_EVT_SLEEP) {
                periph->off();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                printf("enter deep-sleep\n");
                esp_deep_sleep_start();
            }
            oled_show_page(cur_page);
        } else {
            oled_show_page(cur_page);
        }
    }
}

void oled_next_page()
{
    printf("next page...\n");
    oled_evt_t evt;
    evt.type = OLED_EVT_PAGE_NEXT;
    xQueueSend(oled_queue, &evt, portMAX_DELAY);
}

void oled_prev_page()
{
    printf("prev page...\n");
    oled_evt_t evt;
    evt.type = OLED_EVT_PAGE_PREV;
    xQueueSend(oled_queue, &evt, portMAX_DELAY);
}

static void apds9960_task(void* arg)
{
    while (1) {
        int ges = gsens->read_gesture();
        switch (ges) {
            case APDS9960_RIGHT:
                printf("APDS9960_RIGHT, NEXT PAGE***\n");
                oled_next_page();
                break;
            case APDS9960_LEFT:
                printf("APDS9960_LEFT, PREV PAGE***\n");
                oled_prev_page();
                break;
            case APDS9960_DOWN:
                printf("APDS9960_DOWN, NEXT PAGE***\n");
                oled_next_page();
                break;
            case APDS9960_UP:
                printf("APDS9960_UP, PREV PAGE***\n");
                oled_prev_page();
                break;
        }
        vTaskDelay(100 / portTICK_RATE_MS);
    }
}

static void touch_press_1s_cb(void *arg)
{
    CTouchPad *tp = (CTouchPad*) arg;
    touch_pad_t tp_num = tp->tp_num();
    if (tp_num == 9) {
        oled_next_page();
    } else if (tp_num == 8) {
        oled_prev_page();
    }
    ets_printf("press_1s_cb tap callback of touch pad num %d\n", tp_num);
}

static void touch_press_2s_cb(void *arg)
{
    CTouchPad *tp = (CTouchPad*) arg;
    touch_pad_t tp_num = tp->tp_num();
    oled_evt_t evt;
    evt.type = OLED_EVT_SLEEP;
    xQueueSend(oled_queue, &evt, portMAX_DELAY);
    ets_printf("press_2s_cb tap callback of touch pad num %d\n", tp_num);
}

static void application_init()
{
    // Power on peripheral devices
    periph = new CPowerCtrl(POWER_CNTL_IO);
    periph->on();

    // Init I2C devices
    i2c_dev_init();

    // Init touch pad
    tp_left = new CTouchPad(TOUCH_PAD_LEFT);
    tp_left->add_cb(TOUCHPAD_CB_TAP, touch_press_1s_cb, tp_left);
    tp_left->add_custom_cb(2, touch_press_2s_cb, tp_left);
    tp_right = new CTouchPad(TOUCH_PAD_RIGHT);
    tp_right->add_cb(TOUCHPAD_CB_TAP, touch_press_1s_cb, tp_right);
    tp_right->add_custom_cb(2, touch_press_2s_cb, tp_right);

    // Set deep-sleep wake-up mode
    esp_sleep_enable_touchpad_wakeup();
}


static void initialize_sntp(void)
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void obtain_time(void)
{
    initialize_sntp();
    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

void sntp_task(void* pvParameter)
{
    struct tm timeinfo;
    char strftime_buf[64];
    obtain_time();
    time_t g_now;
    time(&g_now);
    setenv("TZ", "GMT-8", 1);
    tzset();
    localtime_r(&g_now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    printf("The current date/time in Shanghai is: %s", strftime_buf);
    vTaskDelete(NULL);
}

static void application_test()
{
    if (oled_queue == NULL) {
        oled_queue = xQueueCreate(10, sizeof(oled_evt_t));
    }
    application_init();
    xTaskCreate(oled_task, "oled_task", 1024 * 8, NULL, 12, NULL);
    xTaskCreate(apds9960_task, "apds9960_task", 1024 * 8, NULL, 9, NULL);

    // SNTP init
    CWiFi *wifi = CWiFi::GetInstance(WIFI_MODE_STA);
    wifi->Connect(AP_SSID, AP_PASSWORD, portMAX_DELAY);
    xTaskCreate(&sntp_task, "sntp_task", 2048*2, NULL, 10, NULL);
}

extern "C" void app_main()
{
    application_test();
}
