/* Lowpower EVB Example

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
#include "esp_system.h"
#include "esp_log.h"
#include "epaper.h"
#include "epaper_fonts.h"
#include "lowpower_evb_power.h"
#include "lowpower_evb_epaper.h"

static const char *TAG = "lowpower_evb_epaper";

#define EPAPER_POWER_CNTL_IO        ((gpio_num_t) 14)

/* Epaper pin definition */
#define MOSI_PIN        23
#define MISO_PIN        37
#define SCK_PIN         22
#define BUSY_PIN        5
#define DC_PIN          19
#define RST_PIN         18
#define CS_PIN          21

/* Epaper display resolution */
#define EPD_WIDTH       176
#define EPD_HEIGHT      264

/* Epaper color inverse. 1 or 0 = set or reset a bit if set a colored pixel */
#define IF_INVERT_COLOR 1

CPowerCtrl *epaper_power = NULL;

static epaper_handle_t epaper = NULL;

static void epaper_power_on()
{
    epaper_power = new CPowerCtrl(EPAPER_POWER_CNTL_IO);
    epaper_power->on();
}

static void epaper_gpio_init()
{
    epaper_conf_t epaper_conf;
    epaper_conf.busy_pin = BUSY_PIN;
    epaper_conf.cs_pin = CS_PIN;
    epaper_conf.dc_pin = DC_PIN;
    epaper_conf.miso_pin = MISO_PIN;
    epaper_conf.mosi_pin = MOSI_PIN;
    epaper_conf.reset_pin = RST_PIN;
    epaper_conf.sck_pin = SCK_PIN;

    gpio_config_t epaper_gpio_config;
    epaper_gpio_config.pin_bit_mask = (1<<epaper_conf.busy_pin) | (1<<epaper_conf.miso_pin);
    epaper_gpio_config.mode = GPIO_MODE_INPUT;
    epaper_gpio_config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    epaper_gpio_config.pull_up_en = GPIO_PULLUP_DISABLE;
    epaper_gpio_config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&epaper_gpio_config);

    epaper_gpio_config.pin_bit_mask = (1<<epaper_conf.cs_pin)   | (1<<epaper_conf.dc_pin)   |
                                      (1<<epaper_conf.mosi_pin) | (1<<epaper_conf.reset_pin)| 
                                      (1<<epaper_conf.sck_pin);
    epaper_gpio_config.mode = GPIO_MODE_OUTPUT;

    gpio_config(&epaper_gpio_config);
    gpio_set_level((gpio_num_t)epaper_conf.cs_pin, 0);
    gpio_set_level((gpio_num_t)epaper_conf.dc_pin, 0);
    gpio_set_level((gpio_num_t)epaper_conf.mosi_pin, 0);
    gpio_set_level((gpio_num_t)epaper_conf.reset_pin, 0);
    gpio_set_level((gpio_num_t)epaper_conf.sck_pin, 0);
}

static void epaper_init()
{
    epaper_conf_t epaper_conf;
    epaper_conf.busy_pin = BUSY_PIN;
    epaper_conf.cs_pin = CS_PIN;
    epaper_conf.dc_pin = DC_PIN;
    epaper_conf.miso_pin = MISO_PIN;
    epaper_conf.mosi_pin = MOSI_PIN;
    epaper_conf.reset_pin = RST_PIN;
    epaper_conf.sck_pin = SCK_PIN;

    epaper_conf.rst_active_level = 0;
    epaper_conf.dc_lev_data = 1;
    epaper_conf.dc_lev_cmd = 0;

    epaper_conf.clk_freq_hz = 20 * 1000 * 1000;
    epaper_conf.spi_host = HSPI_HOST;

    epaper_conf.width = EPD_WIDTH;
    epaper_conf.height = EPD_HEIGHT;
    epaper_conf.color_inv = 1;

    ESP_LOGI(TAG, "before epaper init, heap: %d", esp_get_free_heap_size());
    epaper = iot_epaper_create(NULL, &epaper_conf);
    iot_epaper_set_rotate(epaper, E_PAPER_ROTATE_270);
}
  
void epaper_show_page(float hum, float temp, float lum, float supply_voltage)
{
    char supply_vol_str[6];
    char hum_str[6];
    char temp_str[6];
    char lum_str[6];
  
    iot_epaper_clean_paint(epaper, UNCOLORED);
 
    iot_epaper_draw_string(epaper, 190, 0, "@espressif", &epaper_font_12, COLORED);
    iot_epaper_draw_string(epaper, 10, 20, "Low Power Demo", &epaper_font_16, COLORED);
    iot_epaper_draw_string(epaper, 15, 65, "Humidity", &epaper_font_16, COLORED);
    iot_epaper_draw_string(epaper, 15, 105, "Temperature", &epaper_font_16, COLORED);
    iot_epaper_draw_string(epaper, 15, 145, "Luminance", &epaper_font_16, COLORED);
    memset(supply_vol_str, 0x00, sizeof(supply_vol_str));
    memset(hum_str, 0x00, sizeof(hum_str));
    memset(temp_str, 0x00, sizeof(temp_str));
    memset(lum_str, 0x00, sizeof(lum_str));
    sprintf(supply_vol_str, "%2.1fV", supply_voltage);
    sprintf(hum_str, "%2.1f", hum);
    sprintf(temp_str, "%2.1f", temp);
    sprintf(lum_str, "%2.1f", lum);

    iot_epaper_draw_string(epaper, 210, 20, supply_vol_str, &epaper_font_16, COLORED);
    iot_epaper_draw_string(epaper, 180, 65, hum_str, &epaper_font_16, COLORED);
    iot_epaper_draw_string(epaper, 180, 105, temp_str, &epaper_font_16, COLORED);
    iot_epaper_draw_string(epaper, 180, 145, lum_str, &epaper_font_16, COLORED);
    iot_epaper_draw_horizontal_line(epaper, 10, 37, 180, COLORED);
    iot_epaper_draw_horizontal_line(epaper, 10, 90, 240, COLORED);
    iot_epaper_draw_horizontal_line(epaper, 10, 130, 240, COLORED);
    iot_epaper_draw_vertical_line(epaper, 150, 50, 120, COLORED);
    iot_epaper_draw_rectangle(epaper, 10, 50, 250, 170, COLORED);
    ESP_LOGI(TAG, "EPD Display Fresh");
    /* Display the frame_buffer */
    iot_epaper_display_frame(epaper, NULL); /* dispaly the frame buffer */
}
  
void epaper_disable()
{
    iot_epaper_delete(epaper, true);
}

void epaper_display_init()
{
    epaper_gpio_init();
    epaper_power_on();
    vTaskDelay(10 / portTICK_RATE_MS);  // 10ms delay after power on
    epaper_init();
}

