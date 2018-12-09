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
#include "lwip/api.h"
#include "iot_lcd.h"
#include "iot_wifi_conn.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "freertos/event_groups.h"
#include "lcd_image.h"

#define PROGMEM
#include "FreeSans9pt7b.h"
#include "unity.h"

static const char* TAG = "LCD_REFRESH";
static CEspLcd* tft = NULL;
static bool test_finish = false;

extern "C" void app_lcd_init(uint8_t m_clk_freq)
{
    static bool first = true;              //use this flag, avoid spi init repeat.
    lcd_conf_t lcd_pins = {
        .lcd_model = LCD_MOD_AUTO_DET,
        .pin_num_miso = CONFIG_LCD_MISO_GPIO,
        .pin_num_mosi = CONFIG_LCD_MOSI_GPIO,
        .pin_num_clk  = CONFIG_LCD_CLK_GPIO,
        .pin_num_cs   = CONFIG_LCD_CS_GPIO,
        .pin_num_dc   = CONFIG_LCD_DC_GPIO,
        .pin_num_rst  = CONFIG_LCD_RESET_GPIO,
        .pin_num_bckl = CONFIG_LCD_BL_GPIO,
        .clk_freq = m_clk_freq * 1000 * 1000,
        .rst_active_level = 0,
        .bckl_active_level = 0,
        .spi_host = HSPI_HOST,
        .init_spi_bus = first,
    };

    /*Initialize SPI Handler*/
    if (tft == NULL) {
        tft = new CEspLcd(&lcd_pins);
    }

    /*screen initialize*/
    tft->invertDisplay(false);
    tft->setRotation(1);
    tft->fillScreen(COLOR_GREEN);
    first = false;
}

extern "C" void app_lcd_task(void *clk_freq)
{
    uint8_t i = 0;
    uint32_t time = 0;
    app_lcd_init(*((uint8_t *)clk_freq));

    time = xTaskGetTickCount();
    while (true) {
        if((xTaskGetTickCount() - time) > 1000 / portTICK_RATE_MS ) {
           ESP_LOGI(TAG, "clk_freq: %d MHz, LCD refresh %d fps", *((uint8_t *)clk_freq), i);
           time = xTaskGetTickCount();
           i = 0;
           break;
        }
        i++;
        tft->drawBitmap(0, 0, (uint16_t *)Status_320_240, 320, 240);
    }
    delete tft;         //destruct CEspLcd object of tft pointed
    tft = NULL;

    if (*(uint8_t *)clk_freq < CONFIG_MAX_LCD_CLK_FREQ) {
        *(uint8_t *)clk_freq %= CONFIG_MAX_LCD_CLK_FREQ;
        *(uint8_t *)clk_freq += CONFIG_LCD_CLK_TEST_INCREASE;
        xTaskCreate(&app_lcd_task, "app_lcd_task", 4096, clk_freq, 4, NULL);
    } else {
        test_finish = true;
    }
    vTaskDelete(NULL);
}

TEST_CASE("LCD refresh test", "[lcd_refresh][iot]")
{
    uint8_t m_clk_freq = 0;
    ESP_LOGI(TAG, "Free heap: %u", xPortGetFreeHeapSize());
    ESP_LOGI(TAG, "get free size of 32BIT heap : %d", 
        heap_caps_get_free_size(MALLOC_CAP_32BIT));
    ESP_LOGD(TAG, "Starting app_lcd_task...");

    m_clk_freq = 10;
    xTaskCreate(&app_lcd_task, "app_lcd_task", 4096, &m_clk_freq, 4, NULL);
    
    while (!test_finish); //wait test finish, otherwise m_clk_freq is correct in task
}
