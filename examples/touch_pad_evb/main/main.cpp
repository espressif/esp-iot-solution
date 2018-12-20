/* Touch Sensor Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "evb.h"

/*
 * This example show the demo for ESP32-Sense Kit.
 * Users can evaluate ESP32 touch sensor function
 * with ESP32-Sense Kit.
 */

static const char* TAG = "evb_main";
QueueHandle_t q_touch = NULL;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int WIFI_INIT_DONE_BIT = BIT1;

/*
 * Init the components in ESP32-Sense Kit.
 */
void evb_component_init()
{
    ESP_LOGI(TAG, "components init...");
    evb_adc_init();         // Use ADC to identify the different board.
    ch450_dev_init();       // Init digital tube to display.
    evb_rgb_led_init();     // Init RGB LED
}

static void evb_touch_init(void *arg)
{
    touch_evt_t evt;

    evb_component_init();
    if (q_touch == NULL) {
        q_touch = xQueueCreate(10, sizeof(touch_evt_t));
    }
    /* Should init the touch function after WiFi init */
    xEventGroupWaitBits(wifi_event_group, WIFI_INIT_DONE_BIT, true, false, portMAX_DELAY);

    int mode = evb_adc_get_mode();
    if (mode == TOUCH_EVB_MODE_MATRIX) {
        evb_touch_matrix_init();                    // [ESP32-Sense Kit] Daughterboards: Matrix Button
    } else if (mode == TOUCH_EVB_MODE_SLIDE) {
        evb_touch_slide_init_then_run();            // [ESP32-Sense Kit] Daughterboards: Linear Slider
    } else if (mode ==  TOUCH_EVB_MODE_SPRING) {
        evb_touch_spring_init();                    // [ESP32-Sense Kit] Daughterboards: Spring Button
    } else if (mode == TOUCH_EVB_MODE_CIRCLE) {
        evb_touch_wheel_init_then_run();            // [ESP32-Sense Kit] Daughterboards: Wheel Slider
    } else if (mode == TOUCH_EVB_MODE_SEQ_SLIDE) {
        evb_touch_seq_slide_init_then_run();        // [ESP32-Sense Kit] Daughterboards: Duplex Slider
    }

    while (1) {
        portBASE_TYPE ret = xQueueReceive(q_touch, &evt, portMAX_DELAY);
        if (ret == pdTRUE) {
            switch (evt.type) {
                case TOUCH_EVT_TYPE_SPRING_PUSH:
                case TOUCH_EVT_TYPE_SPRING_RELEASE:
                    evb_touch_spring_handle(evt.single.idx, evt.type);
                    break;
                case TOUCH_EVT_TYPE_SINGLE:
                case TOUCH_EVT_TYPE_SINGLE_PUSH:
                case TOUCH_EVT_TYPE_SINGLE_RELEASE:
                    evb_touch_button_handle(evt.single.idx, evt.type);
                    break;
                case TOUCH_EVT_TYPE_MATRIX:
                case TOUCH_EVT_TYPE_MATRIX_RELEASE:
                case TOUCH_EVT_TYPE_MATRIX_SERIAL:
                    evb_touch_matrix_handle(evt.matrix.x, evt.matrix.y, evt.type);
                    break;
                default:
                    break;
            }
        }
    }
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        xEventGroupSetBits(wifi_event_group, WIFI_INIT_DONE_BIT);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

extern "C" void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    initialise_wifi();
    xTaskCreate(evb_touch_init, "touch init", 1024*8, NULL, 3, NULL);
#ifdef CONFIG_DATA_SCOPE_DEBUG
    touch_tune_tool_init();
#endif
}
