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

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event_loop.h"

/*Include desired font here*/
#define PROGMEM
#include "FreeSans9pt7b.h"
#include "unity.h"

static CEspLcd* lcd_obj = NULL;

wifi_scan_config_t scan_config = {
    .ssid = 0,
    .bssid = 0,
    .channel = 0,
    .show_hidden = true,
    .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    .scan_time = {
        .passive = 0,
    },
};

const int CONNECTED_BIT = BIT0;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/*EVT handler for Wifi status*/
extern "C" esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}


extern "C" void esp_draw()
{
    /*Initilize ESP32 to scan for Access points*/
    nvs_flash_init();
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );

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
        .clk_freq     = 20 * 1000 * 1000,
        .rst_active_level = 0,
        .bckl_active_level = 0,
        .spi_host = HSPI_HOST,
        .init_spi_bus = true,
    };

    if (lcd_obj == NULL) {
        lcd_obj = new CEspLcd(&lcd_pins);
    }
    printf("lcd id: 0x%08x\n", lcd_obj->id.id);

    /*Welcome screen*/
    int x = 0, y = 0;
    int dim = 6;
    uint16_t rand_color;
    lcd_obj->setRotation(3);
    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < 10 - 2 * i; j++) {
            rand_color = rand();
            lcd_obj->fillRect(x * 32, y * 24, 32, 24, rand_color);
            ets_delay_us(20000);
            x++;
        }
        x--;
        for (int j = 0; j < 10 - 2 * i; j++) {
            rand_color = rand();
            lcd_obj->fillRect(x * 32, y * 24, 32, 24, rand_color);
            ets_delay_us(20000);
            y++;
        }
        y--;
        for (int j = 0; j < 10 - 2 * i - 1; j++) {
            rand_color = rand();
            lcd_obj->fillRect(x * 32, y * 24, 32, 24, rand_color);
            ets_delay_us(20000);
            x--;
        }
        x++;
        for (int j = 0; j < 10 - 2 * i - 1; j++) {
            rand_color = rand();
            lcd_obj->fillRect((x - 1) * 32, y * 24, 32, 24, rand_color);
            ets_delay_us(20000);
            y--;
        }
        y++;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    /*ESPecifications*/
    lcd_obj->setRotation(2);
    lcd_obj->fillScreen(COLOR_ESP_BKGD);
    lcd_obj->setTextSize(1);
    lcd_obj->drawBitmap(0, 0, esp_logo, 137, 26);

    lcd_obj->setTextColor(COLOR_GREEN, COLOR_ESP_BKGD);
    lcd_obj->setFont(&FreeSans9pt7b);
    lcd_obj->drawString("CPU",                                     3, 40);
    lcd_obj->setFont(NULL);
    lcd_obj->setTextColor(COLOR_YELLOW, COLOR_ESP_BKGD);
    lcd_obj->drawString("Xtensa Dual-Core 32-bit LX6 MPU",         3, 50);
    lcd_obj->drawString("Max Clock Speed at 240 MHz & 600 DMIPS ", 3, 60);
    lcd_obj->drawString("at up to 600 DMIPS",                      3, 70);
    lcd_obj->drawString("Memory: 520 KiB SRAM",                    3, 80);

    lcd_obj->setTextColor(COLOR_GREEN, COLOR_ESP_BKGD);
    lcd_obj->setFont(&FreeSans9pt7b);
    lcd_obj->drawString("Wireless connectivity",               3,  110);
    lcd_obj->setFont(NULL);
    lcd_obj->setTextColor(COLOR_YELLOW, COLOR_ESP_BKGD);
    lcd_obj->drawString("Wi-Fi: 802.11 b/g/n/e/i",            3,  120);
    lcd_obj->drawString("Bluetooth: v4.2 BR/EDR and BLE",      3,  130);

    lcd_obj->setTextColor(COLOR_GREEN, COLOR_ESP_BKGD);
    lcd_obj->setFont(&FreeSans9pt7b);
    lcd_obj->drawString("Power Management",                     3, 160);
    lcd_obj->setFont(NULL);
    lcd_obj->setTextColor(COLOR_YELLOW, COLOR_ESP_BKGD);
    lcd_obj->drawString("Internal LDO",                         3, 170);
    lcd_obj->drawString("Individual power domain for RTC",      3, 180);
    lcd_obj->drawString("5uA deep sleep current",               3, 190);
    lcd_obj->drawString("Wake up from GPIO interrupt" ,         3, 200);
    lcd_obj->drawString("Wake up from timer, ADC measurements", 3, 210);
    lcd_obj->drawString("Wake up from capacitive sensor intr",  3, 220);

    lcd_obj->setTextColor(COLOR_GREEN, COLOR_ESP_BKGD);
    lcd_obj->setFont(&FreeSans9pt7b);
    lcd_obj->drawString("Security",                               3, 250);
    lcd_obj->setFont(NULL);
    lcd_obj->setTextColor(COLOR_YELLOW, COLOR_ESP_BKGD);
    lcd_obj->drawString("IEEE 802.11 standard security features", 3, 260);
    lcd_obj->drawString("Secure boot & Flash Encryption",         3, 270);
    lcd_obj->drawString("Cryptographic Hardware Acceleration",    3, 280);
    lcd_obj->drawString("AES, RSA, SHA-2, EEC, RNG",              3, 290);
    lcd_obj->drawString("1024-bit OTP",                           3, 300);

    vTaskDelay(4000 / portTICK_PERIOD_MS);
    lcd_obj->fillRect(0, 28, 240, 320, COLOR_ESP_BKGD);

    lcd_obj->setTextColor(COLOR_GREEN, COLOR_ESP_BKGD);
    lcd_obj->setFont(&FreeSans9pt7b);
    lcd_obj->drawString("Peripheral Interfaces",               3, 40);
    lcd_obj->setFont(NULL);
    lcd_obj->setTextColor(COLOR_YELLOW, COLOR_ESP_BKGD);
    lcd_obj->drawString("12-bit DAC, 18 channels",             3, 50);
    lcd_obj->drawString("8-bit  DAC,  2 channels",             3, 60);
    lcd_obj->drawString("SPI,  4 channels",                   3, 70);
    lcd_obj->drawString("I2S,  4 channels",                   3, 80);
    lcd_obj->drawString("I2C,  2 channels",                   3, 90);
    lcd_obj->drawString("UART, 3 channels",                   3, 100);
    lcd_obj->drawString("SD/SDIO/MMC Host",                   3, 110);
    lcd_obj->drawString("SDIO/SPI Slave",                     3, 120);
    lcd_obj->drawString("Ethernet MAC with DMA & IEEE 1588",   3, 130);
    lcd_obj->drawString("CAN bus 2.0",                        3, 140);
    lcd_obj->drawString("IR/RMT (Tx/Rx)",                     3, 150);
    lcd_obj->drawString("Motor PWM",                              3, 160);
    lcd_obj->drawString("LED PWM, 16 channels",               3, 170);
    lcd_obj->drawString("Ultra Low Power Analog Pre-Amp",      3, 180);
    lcd_obj->drawString("Hall Effect Sensor",                 3, 190);
    lcd_obj->drawString("Capacitive Touch Sense, 10 channels", 3, 200);
    lcd_obj->drawString("Temperature Sensor",                  3, 210);
    vTaskDelay(4000 / portTICK_PERIOD_MS);

    lcd_obj->fillScreen(COLOR_ESP_BKGD);
    lcd_obj->drawBitmap(0, 0, esp_logo, 137, 26);
    lcd_obj->drawRoundRect(0, 0, 240, 320, 3, COLOR_WHITE);
    lcd_obj->drawFastHLine(0, 25, 320, COLOR_WHITE);
    lcd_obj->setTextColor(COLOR_WHITE, COLOR_ESP_BKGD);
    lcd_obj->drawString("Wifi-scan", 180, 10);
    lcd_obj->setFont(&FreeSans9pt7b);
    lcd_obj->drawString("AP Name",    10, 50);
    lcd_obj->drawString("RSSI",      180, 50);
    lcd_obj->setFont(NULL);

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    uint16_t ap_num = 20;
    wifi_ap_record_t ap_records[20];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));
    printf("Found %d access points:\n", ap_num);

    /*Print 10 of them on the screen*/
    for (uint8_t i = 0; i < ap_num; i++) {
        lcd_obj->drawNumber(i + 1, 10, 60 + (i * 10));
        lcd_obj->setTextColor(COLOR_YELLOW, COLOR_ESP_BKGD);
        lcd_obj->drawString((char *) ap_records[i].ssid, 30, 60 + (i * 10));
        lcd_obj->setTextColor(COLOR_GREEN, COLOR_ESP_BKGD);
        lcd_obj->drawNumber(100 + ap_records[i].rssi, 200, 60 + (i * 10));
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}


TEST_CASE("LCD cpp test", "[lcd][iot]")
{
    esp_draw();
}


