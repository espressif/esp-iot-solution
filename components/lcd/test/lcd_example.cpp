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
 and simply pass the address of its GFXfont to setFontStyle API
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
#include "Adafruit_lcd_fast_as.h"
#include "Adafruit_GFX_AS.h"
#include "image.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event_loop.h"

/*Include desired font here*/
#include "FreeSans9pt7b.h"

#include "unity.h"


static spi_device_handle_t spi = NULL;
static Adafruit_lcd tft(spi);  //Global def for LCD

wifi_scan_config_t scan_config = {
	.ssid = 0,
	.bssid = 0,
	.channel = 0,
	.show_hidden = true
};

const int CONNECTED_BIT = BIT0;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/*EVT handler for Wifi status*/
extern "C" esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
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
	tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );

	/*Initialize LCD*/
	lcd_pin_conf_t lcd_pins = {
        .lcd_model    = ST7789,
        .pin_num_miso = GPIO_NUM_25,
        .pin_num_mosi = GPIO_NUM_23,
        .pin_num_clk  = GPIO_NUM_19,
        .pin_num_cs   = GPIO_NUM_22,
        .pin_num_dc   = GPIO_NUM_21,
        .pin_num_rst  = GPIO_NUM_18,
        .pin_num_bckl = GPIO_NUM_5,
    };
    
    lcd_init(&lcd_pins, &spi);
    tft.setSpiBus(spi);

	/*Welcome screen*/
    int x = 0, y = 0;
    int dim = 6;
    uint16_t rand_color;
    tft.setRotation(3);
    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < 10 - 2 * i; j++) {
            rand_color = rand();
            tft.fillRect(x * 32, y * 24, 32, 24, rand_color);
            ets_delay_us(20000);
            x++;
        }
        x--;
        for (int j = 0; j < 10 - 2 * i; j++) {
            rand_color = rand();
            tft.fillRect(x * 32, y * 24, 32, 24, rand_color);
            ets_delay_us(20000);
            y++;
        }
        y--;
        for (int j = 0; j < 10 - 2 * i - 1; j++) {
            rand_color = rand();
            tft.fillRect(x * 32, y * 24, 32, 24, rand_color);
            ets_delay_us(20000);
            x--;
        }
        x++;
        for (int j = 0; j < 10 - 2 * i - 1; j++) {
            rand_color = rand();
            tft.fillRect((x - 1) * 32, y * 24, 32, 24, rand_color);
            ets_delay_us(20000);
            y--;
        }
        y++;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);

	/*ESPecifications*/
	tft.setRotation(2);
    tft.fillScreen(COLOR_ESP_BKGD);
	tft.setTextSize(1);
    tft.drawBitmap(0, 0, esp_logo, 137, 26);

	tft.setTextColor(COLOR_GREEN, COLOR_ESP_BKGD);
	tft.setFontStyle(&FreeSans9pt7b);
	tft.drawString("CPU",                                     3, 40);
	tft.setFontStyle(NULL);
	tft.setTextColor(COLOR_YELLOW, COLOR_ESP_BKGD);
    tft.drawString("Xtensa Dual-Core 32-bit LX6 MPU",         3, 50);
	tft.drawString("Max Clock Speed at 240 MHz & 600 DMIPS ", 3, 60);
	tft.drawString("at up to 600 DMIPS",                      3, 70);
    tft.drawString("Memory: 520 KiB SRAM",                    3, 80);
	
	tft.setTextColor(COLOR_GREEN, COLOR_ESP_BKGD);
	tft.setFontStyle(&FreeSans9pt7b);
	tft.drawString("Wireless connectivity",               3,  110);
	tft.setFontStyle(NULL);
	tft.setTextColor(COLOR_YELLOW, COLOR_ESP_BKGD);
	tft.drawString("Wi-Fi: 802.11 b/g/n/e/i",	          3,  120);
	tft.drawString("Bluetooth: v4.2 BR/EDR and BLE",      3,  130);

	tft.setTextColor(COLOR_GREEN, COLOR_ESP_BKGD);
	tft.setFontStyle(&FreeSans9pt7b);
	tft.drawString("Power Management",                     3, 160);
	tft.setFontStyle(NULL);
	tft.setTextColor(COLOR_YELLOW, COLOR_ESP_BKGD);
	tft.drawString("Internal LDO",                         3, 170);
	tft.drawString("Individual power domain for RTC",      3, 180);
	tft.drawString("5uA deep sleep current",               3, 190);
	tft.drawString("Wake up from GPIO interrupt" ,         3, 200);
	tft.drawString("Wake up from timer, ADC measurements", 3, 210);
	tft.drawString("Wake up from capacitive sensor intr",  3, 220);

	tft.setTextColor(COLOR_GREEN, COLOR_ESP_BKGD);
	tft.setFontStyle(&FreeSans9pt7b);
	tft.drawString("Security",                               3, 250);
	tft.setFontStyle(NULL);
	tft.setTextColor(COLOR_YELLOW, COLOR_ESP_BKGD);
	tft.drawString("IEEE 802.11 standard security features", 3, 260);
	tft.drawString("Secure boot & Flash Encryption",         3, 270);
	tft.drawString("Cryptographic Hardware Acceleration",    3, 280);
	tft.drawString("AES, RSA, SHA-2, EEC, RNG",              3, 290);
	tft.drawString("1024-bit OTP",                           3, 300);

	vTaskDelay(4000 / portTICK_PERIOD_MS);
	tft.fillRect(0, 28, 240, 320, COLOR_ESP_BKGD);

	tft.setTextColor(COLOR_GREEN, COLOR_ESP_BKGD);
	tft.setFontStyle(&FreeSans9pt7b);
	tft.drawString("Peripheral Interfaces",               3, 40);
	tft.setFontStyle(NULL);
	tft.setTextColor(COLOR_YELLOW, COLOR_ESP_BKGD);
	tft.drawString("12-bit DAC, 18 channels",             3, 50);
	tft.drawString("8-bit  DAC,  2 channels",             3, 60);
	tft.drawString("SPI,  4 channels",				      3, 70);
	tft.drawString("I2S,  4 channels",				      3, 80);
	tft.drawString("I2C,  2 channels",				      3, 90);
	tft.drawString("UART, 3 channels",				      3, 100);
	tft.drawString("SD/SDIO/MMC Host",		              3, 110);
	tft.drawString("SDIO/SPI Slave",	         	      3, 120);
	tft.drawString("Ethernet MAC with DMA & IEEE 1588",   3, 130);
	tft.drawString("CAN bus 2.0",	         	          3, 140);
	tft.drawString("IR/RMT (Tx/Rx)",		              3, 150);
	tft.drawString("Motor PWM",	         	              3, 160);
	tft.drawString("LED PWM, 16 channels",		          3, 170);
	tft.drawString("Ultra Low Power Analog Pre-Amp",      3, 180);
	tft.drawString("Hall Effect Sensor",	              3, 190);
	tft.drawString("Capacitive Touch Sense, 10 channels", 3, 200);
	tft.drawString("Temperature Sensor",                  3, 210);
	vTaskDelay(4000 / portTICK_PERIOD_MS);
	
	tft.fillScreen(COLOR_ESP_BKGD);
	tft.drawBitmap(0, 0, esp_logo, 137, 26);
	tft.drawRoundRect(0, 0, 240, 320, 3, COLOR_WHITE);
	tft.drawFastHLine(0, 25, 320, COLOR_WHITE);
	tft.setTextColor(COLOR_WHITE, COLOR_ESP_BKGD);
	tft.drawString("Wifi-scan", 180, 10);
	tft.setFontStyle(&FreeSans9pt7b);
	tft.drawString("AP Name",    10, 50);
	tft.drawString("RSSI",      180, 50);
	tft.setFontStyle(NULL);

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    uint16_t ap_num = 20;
    wifi_ap_record_t ap_records[20];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));
    printf("Found %d access points:\n", ap_num);

    /*Print 10 of them on the screen*/
    for (uint8_t i = 0; i < ap_num; i++) {
        tft.drawNumber(i + 1, 10, 60 + (i * 10));
        tft.setTextColor(COLOR_YELLOW, COLOR_ESP_BKGD);
        tft.drawString((char *) ap_records[i].ssid, 30, 60 + (i * 10));
        tft.setTextColor(COLOR_GREEN, COLOR_ESP_BKGD);
        tft.drawNumber(100 + ap_records[i].rssi, 200, 60 + (i * 10));
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}


TEST_CASE("LCD cpp test", "[lcd][iot]")
{
    esp_draw();
}


