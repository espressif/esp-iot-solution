/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IOT_BOARD_H_
#define _IOT_BOARD_H_

#include "stdio.h"
#include "stdbool.h"
#include "esp_err.h"
#include "board_common.h"

/**
 * Resource ID on Board,
 * You can use iot_board_get_handle(ID,handle) to reference Resource's Handle
 * */
typedef enum {
    NULL_RESOURCE = 0,
    BOARD_I2C0_ID,
    BOARD_SPI2_ID,
    BOARD_SPI3_ID,
} board_res_id_t;

/*Definitions of Board Information*/
#define BOARD_NAME "ESP32_S3_USB_OTG_EV"
#define BOARD_VENDOR "Espressif"
#define BOARD_URL "https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-usb-otg/index.html"

/*Definitions of MCU Information*/
#define BOARD_MCU_NAME "ESP32S3"
#define BOARD_MODULE_NAME "ESP32-S3-MINI"
#define BOARD_MCU_FLASH_SIZE "4MB"
#define BOARD_MCU_RAM_SIZE "512KB"
#define BOARD_MCU_RAM_USER_SIZE "384KB"

/* Free pins */
#define BOARD_IO_FREE_45 45 //esp32s2/s3 strapping pins
#define BOARD_IO_FREE_46 46 //esp32s2/s3 strapping pins
#define BOARD_IO_FREE_48 48 //esp32s2 not support
#define BOARD_IO_FREE_26 26
#define BOARD_IO_FREE_47 47 //esp32s2 not support
#define BOARD_IO_FREE_3 3

/*Definitions of Button*/
#define BOARD_IO_BUTTON_OK 0
#define BOARD_IO_BUTTON_DW 11
#define BOARD_IO_BUTTON_UP 10
#define BOARD_IO_BUTTON_MENU 14

#define BOARD_BUTTON_NUM 4
#define BOARD_IO_BUTTON_1 BOARD_IO_BUTTON_OK
#define BOARD_IO_BUTTON_2 BOARD_IO_BUTTON_DW
#define BOARD_IO_BUTTON_3 BOARD_IO_BUTTON_UP
#define BOARD_IO_BUTTON_4 BOARD_IO_BUTTON_MENU
#define BOARD_BUTTON_POLARITY_1 _NEGATIVE
#define BOARD_BUTTON_POLARITY_2 _NEGATIVE
#define BOARD_BUTTON_POLARITY_3 _NEGATIVE
#define BOARD_BUTTON_POLARITY_4 _NEGATIVE

/*Definitions of LED*/
#define BOARD_IO_LED_GREEN 15
#define BOARD_IO_LED_YELLOW 16

#define BOARD_LED_NUM 2
#define BOARD_IO_LED_1 BOARD_IO_LED_GREEN
#define BOARD_IO_LED_2 BOARD_IO_LED_YELLOW
#define BOARD_LED_POLARITY_1 _POSITIVE
#define BOARD_LED_POLARITY_2 _POSITIVE

/*Definitions of GPIO*/
#define BOARD_IO_HOST_BOOST_EN 13
#define BOARD_IO_DEV_VBUS_EN 12
#define BOARD_IO_USB_SEL 18
#define BOARD_IO_IDEV_LIMIT_EN 17
#define BOARD_IO_PIN_SEL_OUTPUT ((1ULL<<BOARD_IO_HOST_BOOST_EN)\
                                |(1ULL<<BOARD_IO_DEV_VBUS_EN)\
                                |(1ULL<<BOARD_IO_USB_SEL)\
                                |(1ULL<<BOARD_IO_IDEV_LIMIT_EN))
/*Definitions of I2C0*/
#define BOARD_IO_I2C0_SCL BOARD_IO_FREE_3
#define BOARD_IO_I2C0_SDA BOARD_IO_FREE_26

/*Definitions of SPI2*/
#define BOARD_IO_SPI2_SCK 6
#define BOARD_IO_SPI2_MOSI 7
#define BOARD_IO_SPI2_MISO -1

/*********************************** ESP32-S3-USB-OTG board specified definition ***********************/
//USB Configuration (fixed)
#define BOARD_IO_USB_DP 20
#define BOARD_IO_USB_DN 19

//SPI3 Configuration
#define BOARD_IO_SPI3_SCK 36
#define BOARD_IO_SPI3_MOSI 35
#define BOARD_IO_SPI3_MISO 37
#define BOARD_IO_SPI3_CS 34

#define BOARD_IO_JTAG_MTMS 42
#define BOARD_IO_JTAG_MTDI 41
#define BOARD_IO_JTAG_MTDO 40
#define BOARD_IO_JTAG_MTCK 39

//ADC Configuration
#define BOARD_IO_ADC_HOST_VOL 1
#define BOARD_IO_ADC_BAT_VOL 2
#define BOARD_IO_0VER_CURRENT 21

#define BOARD_ADC_WIDTH (ADC_WIDTH_MAX-1)
#define BOARD_ADC_ATTEN ADC_ATTEN_DB_11
#define BOARD_ADC_DEFAULT_VREF    1100
#define BOARD_ADC_NO_OF_SAMPLES   16

#ifdef CONFIG_IDF_TARGET_ESP32S2
#define BOARD_ADC_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_TP
#define BOARD_ADC_UNIT ADC_UNIT_BOTH
#define BOARD_ADC_HOST_VOL_CHAN 0 //ADC1 CH0
#define BOARD_ADC_BAT_VOL_CHAN  1 //ADC2 CH1
#elif CONFIG_IDF_TARGET_ESP32S3
#define BOARD_ADC_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_TP_FIT
#define BOARD_ADC_UNIT ADC_UNIT_1
#define BOARD_ADC_HOST_VOL_CHAN 0 //ADC1 CH0
#define BOARD_ADC_BAT_VOL_CHAN  1 //ADC1 CH1
#endif

/* Definitions of Peripheral */
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

/* SD Card */
#define BOARD_SDCARD_BASE_PATH "/sdcard"
#define BOARD_SDCARD_FORMAT_IF_MOUNT_FAILED 0
#define BOARD_SDCARD_MAX_OPENED_FILE 9
#define BOARD_SDCARD_DISK_BLOCK_SIZE 512
/* spisd mode */
#define BOARD_SDCARD_SPI_HOST SPI3_HOST
#define BOARD_SDCARD_CMD BOARD_IO_SPI3_MOSI
#define BOARD_SDCARD_CLK BOARD_IO_SPI3_SCK
#define BOARD_SDCARD_DATA BOARD_IO_SPI3_MISO
#define BOARD_SDCARD_CD BOARD_IO_SPI3_CS
/* sdio mode */
#define BOARD_SDCARD_SDIO_CMD BOARD_IO_SPI3_MOSI
#define BOARD_SDCARD_SDIO_CLK BOARD_IO_SPI3_SCK
#define BOARD_SDCARD_SDIO_DATA_WIDTH 4
#define BOARD_SDCARD_SDIO_DATA0 37
#define BOARD_SDCARD_SDIO_DATA1 38
#define BOARD_SDCARD_SDIO_DATA2 33
#define BOARD_SDCARD_SDIO_DATA3 34

extern const board_specific_callbacks_t board_esp32_s3_otg_callbacks;
#define BOARD_CALLBACKS board_esp32_s3_otg_callbacks

typedef enum {
    USB_DEVICE_MODE,
    USB_HOST_MODE,
} usb_mode_t;

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************** ESP32-S3-USB-OTG board specified API ***********************/
/**
 * @brief Set usb host port power
 *
 * @param on_off true if power on, false if power off.
 * @param from_battery true if power from battery, false if power from usb device port
 * @return esp_err_t
 *         ESP_FAIL set power failed, power control gpio not ininted
 *         ESP_OK set power succeed
 */
esp_err_t iot_board_usb_host_set_power(bool on_off, bool from_battery);

/**
 * @brief Check if usb host port is powered
 *
 * @return bool
 *         true if usb is power on
 *         false if usb is power off
 */
bool iot_board_usb_host_get_power(void);

/**
 * @brief Check usb device interface voltage
 *
 * @return float
 *         device interface voltage from other host (Unit: V)
 */
float iot_board_usb_device_voltage(void);

/**
 * @brief Check battery voltage
 *
 * @return float
 *         battery voltage (Unit: V)
 */
float iot_board_battery_voltage(void);

/**
 * @brief Set board USB mode
 *
 * @param mode modes defined in usb_mode_t, if USB_DEVICE_MODE connect usb to device port, if USB_HOST_MODE connect usb to host port.
 * @return esp_err_t
 *         ESP_FAIL set mode failed, mode control gpio not ininted
 *         ESP_OK set mode succeed
 */
esp_err_t iot_board_usb_set_mode(usb_mode_t mode);

/**
 * @brief Get board current USB mode
 *
 * @return usb_mode_t
 *         USB_DEVICE_MODE if connecting to usb device port
 *         USB_HOST_MODE if connecting to usb host port.
 */
usb_mode_t iot_board_usb_get_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* _IOT_BOARD_H_ */
