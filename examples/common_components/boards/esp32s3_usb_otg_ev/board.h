// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
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

#ifndef _IOT_BOARD_H_
#define _IOT_BOARD_H_

#include "stdio.h"
#include "stdbool.h"
#include "esp_err.h"
#include "board_common.h"
#include "iot_button.h"

/**
 * Resource ID on Board,
 * You can use iot_board_get_handle(ID,handle) to refrence Resource's Handle
 * */
typedef enum {
    NULL_RESOURCE = 0,
    BOARD_I2C0_ID,
    BOARD_SPI2_ID,
    BOARD_SPI3_ID,
    BOARD_BTN_OK_ID,
    BOARD_BTN_UP_ID,
    BOARD_BTN_DW_ID,
    BOARD_BTN_MN_ID,
}board_res_id_t;

/*Definations of Board*/
#define BOARD_NAME "ESP32_S3_USB_OTG_EV"
#define BOARD_VENDOR "Espressif"
#define BOARD_URL "https://github.com/espressif/esp-iot-solution/blob/master"

/*Definations of MCU*/
#define BOARD_MCU_NAME "ESP32S3"
#define BOARD_MODULE_NAME "ESP32-S3-MINI"
#define BOARD_MCU_FLASH_SIZE "4MB"
#define BOARD_MCU_RAM_SIZE "512KB"
#define BOARD_MCU_RAM_USER_SIZE "384KB"

/* Free pins */
#define BOARD_IO_FREE_45 45
#define BOARD_IO_FREE_46 46
#define BOARD_IO_FREE_48 48 //esp32s2 not support
#define BOARD_IO_FREE_26 26
#define BOARD_IO_FREE_47 47 //esp32s2 not support
#define BOARD_IO_FREE_3 3

/*Definations of Button*/
#define BOARD_IO_BUTTON_OK 0
#define BOARD_IO_BUTTON_DW 11
#define BOARD_IO_BUTTON_UP 10
#define BOARD_IO_BUTTON_MENU 14

#define BOARD_IO_BUTTON_A BOARD_IO_BUTTON_DW
#define BOARD_IO_BUTTON_B BOARD_IO_BUTTON_UP
#define BOARD_IO_BUTTON_C BOARD_IO_BUTTON_MENU

#define BOARD_IO_LED_GREEN 15
#define BOARD_IO_LED_YELLOW 16

#define BOARD_IO_USB_DP 20
#define BOARD_IO_USB_DN 19

#define BOARD_IO_I2C0_SCL BOARD_IO_FREE_48
#define BOARD_IO_I2C0_SDA BOARD_IO_FREE_26

#define BOARD_IO_SPI3_SCK 36
#define BOARD_IO_SPI3_MOSI 35
#define BOARD_IO_SPI3_MISO 37
#define BOARD_IO_SPI3_CS 34

#define BOARD_IO_SPI2_SCK 6
#define BOARD_IO_SPI2_MOSI 7
#define BOARD_IO_SPI2_MISO -1

#define BOARD_IO_JTAG_MTMS 42
#define BOARD_IO_JTAG_MTDI 41
#define BOARD_IO_JTAG_MTDO 40
#define BOARD_IO_JTAG_MTCK 39

#define BOARD_IO_ADC_HOST_VOL 1
#define BOARD_IO_ADC_BAT_VOL 2

#ifdef CONFIG_IDF_TARGET_ESP32S2
#define BOARD_ADC_HOST_VOL_CHAN 0 //ADC1
#define BOARD_ADC_BAT_VOL_CHAN 1 //ADC2
#elif CONFIG_IDF_TARGET_ESP32S3
#define BOARD_ADC_HOST_VOL_CHAN 0//ADC1 CH0
#define BOARD_ADC_BAT_VOL_CHAN 1 //ADC1 CH1
#else
#error target not supported
#endif

/* Power Selector */
#define BOARD_IO_HOST_BOOST_EN 13
#define BOARD_IO_DEV_VBUS_EN 12
#define BOARD_IO_USB_SEL 18
#define BOARD_IO_IDEV_LIMIT_EN 17

/* Definations of Peripheral */
#define BOARD_I2C0_MODE I2C_MODE_MASTER
#define BOARD_I2C0_SPEED (100000)
#define BOARD_I2C0_SCL_PULLUP_EN _ENABLE
#define BOARD_I2C0_SDA_PULLUP_EN _ENABLE

#define BOARD_LED_NUM 2
#define BOARD_IO_LED_1 BOARD_IO_LED_GREEN
#define BOARD_IO_LED_2 BOARD_IO_LED_YELLOW
#define BOARD_LED_POLARITY_1 _POSITIVE
#define BOARD_LED_POLARITY_2 _POSITIVE

#define BOARD_LCD_SPI_HOST SPI2_HOST
#define BOARD_LCD_SPI_CLOCK_FREQ 40000000
#define BOARD_LCD_SPI_MISO_PIN BOARD_IO_SPI2_MISO
#define BOARD_LCD_SPI_MOSI_PIN BOARD_IO_SPI2_MOSI
#define BOARD_LCD_SPI_CLK_PIN BOARD_IO_SPI2_SCK
#define BOARD_LCD_SPI_CS_PIN 5
#define BOARD_LCD_SPI_DC_PIN 4
#define BOARD_LCD_SPI_RESET_PIN 8
#define BOARD_LCD_SPI_BL_PIN 9
#define BOARD_LCD_TYPE SCREEN_CONTROLLER_ST7789
#define BOARD_LCD_WIDTH 240
#define BOARD_LCD_HEIGHT 240

/* spisd mode */
#define BOARD_SDCARD_CMD BOARD_IO_SPI3_MOSI
#define BOARD_SDCARD_CLK BOARD_IO_SPI3_SCK
#define BOARD_SDCARD_DATA BOARD_IO_SPI3_MISO
#define BOARD_SDCARD_CD BOARD_IO_SPI3_CS

/* sdio mode */
#define BOARD_SDCARD_SDIO_CMD BOARD_IO_SPI3_MOSI
#define BOARD_SDCARD_SDIO_CLK BOARD_IO_SPI3_SCK
#define BOARD_SDCARD_SDIO_DATA0 37
#define BOARD_SDCARD_SDIO_DATA1 38
#define BOARD_SDCARD_SDIO_DATA2 33
#define BOARD_SDCARD_SDIO_DATA3 34

#define BOARD_IO_PIN_SEL_OUTPUT ((1ULL<<BOARD_IO_LED_1)\
                                |(1ULL<<BOARD_IO_LED_2)\
                                |(1ULL<<BOARD_IO_HOST_BOOST_EN)\
                                |(1ULL<<BOARD_IO_DEV_VBUS_EN)\
                                |(1ULL<<BOARD_IO_USB_SEL)\
                                |(1ULL<<BOARD_IO_IDEV_LIMIT_EN))

#define BOARD_IO_PIN_SEL_INPUT ((1ULL<<BOARD_IO_BUTTON_OK)\
                                |(1ULL<<BOARD_IO_BUTTON_DW)\
                                |(1ULL<<BOARD_IO_BUTTON_UP)\
                                |(1ULL<<BOARD_IO_BUTTON_MENU))

typedef enum {
    USB_DEVICE_MODE,
    USB_HOST_MODE,
} usb_mode_t;

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************** Extended board level API ***********************/
/**
 * @brief set board usb device power
 * 
 * @param on_off set true if power on the usb onboard, set false if power off them.
 * @param from_battery if power from battery
 * @return esp_err_t 
 */
esp_err_t iot_board_usb_device_set_power(bool on_off, bool from_battery);

/**
 * @brief get board usb device power
 * 
 * @return true if usb is power on
 * @return false if usb is power off
 */
bool iot_board_usb_device_get_power(void);

/**
 * @brief Got current host port voltage
 * 
 * @return float Host port voltage (Unit: V)
 */
float iot_board_get_host_voltage(void);

/**
 * @brief Got current battery voltage
 * 
 * @return float battery voltage (Unit: V)
 */
float iot_board_get_battery_voltage(void);

/**
 * @brief Set USB to Host or Device mode 
 * 
 * @param mode mode defined in usb_mode_t
 * @return esp_err_t 
 */
esp_err_t iot_board_usb_set_mode(usb_mode_t mode);

/**
 * @brief Get current USB mode
 * 
 * @return usb_mode_t 
 */
usb_mode_t iot_board_usb_get_mode(void);

/**
 * @brief Register the button event callback function.
 *
 * @param btn_handle A button handle to register
 * @param event Button event
 * @param cb Callback function.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t iot_board_button_register_cb(board_res_handle_t btn_handle, button_event_t event, button_cb_t cb);

/**
 * @brief Unregister the button event callback function.
 *
 * @param btn_handle A button handle to unregister
 * @param event Button event
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t iot_board_button_unregister_cb(board_res_handle_t btn_handle, button_event_t event);

/**
 * @brief set led state
 * 
 * @param gpio_num led gpio num defined in iot_board.h 
 * @param turn_on true if turn on the LED, false is turn off the LED
 * @return esp_err_t 
 */
esp_err_t iot_board_led_set_state(int gpio_num, bool turn_on);

/**
 * @brief set all led state
 * 
 * @param turn_on true if turn on the LED, false is turn off the LED
 * @return esp_err_t 
 */
esp_err_t iot_board_led_all_set_state(bool turn_on);

/**
 * @brief toggle led state
 * 
 * @param gpio_num led gpio num defined in iot_board.h 
 * @return esp_err_t 
 */
esp_err_t iot_board_led_toggle_state(int gpio_num);

static inline void _usb_otg_router_to_internal_phy()
{
#ifdef CONFIG_IDF_TARGET_ESP32S3
    uint32_t *usb_phy_sel_reg = (uint32_t *)(0x60008000 + 0x120);
    *usb_phy_sel_reg |= BIT(19) | BIT(20);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* _IOT_BOARD_H_ */