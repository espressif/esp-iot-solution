// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "soc/efuse_reg.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "spi_bus.h"
#include "st7789.h"
#include "screen_driver.h"
#include "uvc_stream.h"
#include "jpegd2.h"

#ifdef CONFIG_USE_PSRAM
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/spiram.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/spiram.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/spiram.h"
#else
#error Target CONFIG_IDF_TARGET is not supported
#endif
#endif

/* SPI LCD Screen IO Related MACROS */
#ifdef CONFIG_BOARD_KALUGA_1
/**< Screen SPI Interface pins */
#define BOARD_LCD_SPI_HOST SPI2_HOST
#define BOARD_LCD_SPI_CLOCK_FREQ 40000000
#define BOARD_LCD_SPI_MISO_PIN -1
#define BOARD_LCD_SPI_MOSI_PIN 9
#define BOARD_LCD_SPI_CLK_PIN 15
#define BOARD_LCD_SPI_CS_PIN 11
#define BOARD_LCD_SPI_DC_PIN 13
#define BOARD_LCD_SPI_RESET_PIN 16
#define BOARD_LCD_SPI_BL_PIN 6
#define BOARD_LCD_TYPE SCREEN_CONTROLLER_ST7789
#define BOARD_LCD_ST7789_DIR_MIRROR
#define BOARD_LCD_ST7789_INVERT
#elif CONFIG_BOARD_EXPERIMENT
/**< Screen SPI Interface pins */
#define BOARD_LCD_SPI_HOST SPI2_HOST
#define BOARD_LCD_SPI_CLOCK_FREQ 40000000
#define BOARD_LCD_SPI_MISO_PIN 40
#define BOARD_LCD_SPI_MOSI_PIN 38
#define BOARD_LCD_SPI_CLK_PIN 39
#define BOARD_LCD_SPI_CS_PIN 37
#define BOARD_LCD_SPI_DC_PIN 41
#define BOARD_LCD_SPI_RESET_PIN 0
#define BOARD_LCD_SPI_BL_PIN 21
#define BOARD_LCD_TYPE SCREEN_CONTROLLER_ILI9341
#elif CONFIG_BOARD_EXPERIMENT_S3
/**< Screen SPI Interface pins */
#define BOARD_LCD_SPI_HOST SPI2_HOST
#define BOARD_LCD_SPI_CLOCK_FREQ 80000000
#define BOARD_LCD_SPI_MISO_PIN 40
#define BOARD_LCD_SPI_MOSI_PIN 38
#define BOARD_LCD_SPI_CLK_PIN 39
#define BOARD_LCD_SPI_CS_PIN 37
#define BOARD_LCD_SPI_DC_PIN 41
#define BOARD_LCD_SPI_RESET_PIN -1
#define BOARD_LCD_SPI_BL_PIN 21
#define BOARD_LCD_TYPE SCREEN_CONTROLLER_ST7789
#define BOARD_LCD_ST7789_DIR_MIRROR
#define BOARD_LCD_ST7789_INVERT
#endif

/* USB PIN fixed in esp32-s2, can not use io matrix */
#define BOARD_USB_DP_PIN 20
#define BOARD_USB_DN_PIN 19

/* USB Camera Descriptors Related MACROS,
the quick demo skip the standred get descriptors process,
users need to get params from camera descriptors from PC side,
eg. run `lsusb -v` in linux,
then hardcode the related MACROS below
*/
#define DESCRIPTOR_CONFIGURATION_INDEX 1
#define DESCRIPTOR_FORMAT_MJPEG_INDEX  2

#define DESCRIPTOR_FRAME_640_480_INDEX 1
#define DESCRIPTOR_FRAME_352_288_INDEX 2
#define DESCRIPTOR_FRAME_320_240_INDEX 3
#define DESCRIPTOR_FRAME_160_120_INDEX 4

#define DESCRIPTOR_FRAME_5FPS_INTERVAL  2000000
#define DESCRIPTOR_FRAME_10FPS_INTERVAL 1000000
#define DESCRIPTOR_FRAME_15FPS_INTERVAL 666666
#define DESCRIPTOR_FRAME_30FPS_INTERVAL 333333

#define DESCRIPTOR_STREAM_INTERFACE_INDEX   1
#define DESCRIPTOR_STREAM_INTERFACE_ALT_MPS_128 1
#define DESCRIPTOR_STREAM_INTERFACE_ALT_MPS_256 2
#define DESCRIPTOR_STREAM_INTERFACE_ALT_MPS_512 3

#define DESCRIPTOR_STREAM_ISOC_ENDPOINT_ADDR 0x81

/* Demo Related MACROS */
#ifdef CONFIG_SIZE_320_240
#define DEMO_FRAME_WIDTH 320
#define DEMO_FRAME_HEIGHT 240
#define DEMO_XFER_BUFFER_SIZE (35 * 1024) //Double buffer
#define DEMO_FRAME_INDEX DESCRIPTOR_FRAME_320_240_INDEX
#define DEMO_FRAME_INTERVAL DESCRIPTOR_FRAME_15FPS_INTERVAL
#elif CONFIG_SIZE_160_120
#define DEMO_FRAME_WIDTH 160
#define DEMO_FRAME_HEIGHT 120
#define DEMO_XFER_BUFFER_SIZE (25 * 1024) //Double buffer
#define DEMO_FRAME_INDEX DESCRIPTOR_FRAME_160_120_INDEX
#define DEMO_FRAME_INTERVAL DESCRIPTOR_FRAME_30FPS_INTERVAL
#endif

/* max packet size of esp32-s2 is 1*512, bigger is not supported*/
#define DEMO_ISOC_EP_MPS 512
#define DEMO_ISOC_INTERFACE_ALT DESCRIPTOR_STREAM_INTERFACE_ALT_MPS_512

#ifdef CONFIG_BOOT_ANIMATION
#define BOOT_ANIMATION_MAX_SIZE (45 * 1024)
#endif

#define DEMO_SPI_MAX_TRANFER_SIZE (CONFIG_LCD_BUF_WIDTH * CONFIG_LCD_BUF_HIGHT * 2 + 64)

static const char *TAG = "uvc_demo";
static scr_driver_t s_lcd;
static scr_info_t s_lcd_info;
static bool s_lcd_inited = false;

static void *_malloc(size_t size)
{
#ifdef CONFIG_USE_PSRAM
#ifndef CONFIG_ESP32S2_SPIRAM_SUPPORT
#error CONFIG_SPIRAM no defined, Please enable "Component config → ESP32S2-specific → Support for external SPI ram"
#endif
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
}

/***************************************************************************************
 *LCD code to dispaly each frame from usb,
 *the lcd driver comes from esp-iot-solution, for test only,
 *users can implement their lcd driver instead.
*/
static void lcd_screen_clear(scr_driver_t *lcd, int color);

static void lcd_init(void)
{
    spi_config_t bus_conf = {
        .miso_io_num = BOARD_LCD_SPI_MISO_PIN,
        .mosi_io_num = BOARD_LCD_SPI_MOSI_PIN,
        .sclk_io_num = BOARD_LCD_SPI_CLK_PIN,
        .max_transfer_sz = DEMO_SPI_MAX_TRANFER_SIZE,
    };
    spi_bus_handle_t spi_bus = spi_bus_create(BOARD_LCD_SPI_HOST, &bus_conf);

    if (spi_bus == NULL) {
        ESP_LOGE(TAG, "spi_bus2 create failed");
    }

    scr_interface_spi_config_t spi_lcd_cfg = {
        .spi_bus = spi_bus,
        .pin_num_cs = BOARD_LCD_SPI_CS_PIN,
        .pin_num_dc = BOARD_LCD_SPI_DC_PIN,
        .clk_freq = BOARD_LCD_SPI_CLOCK_FREQ,
        .swap_data = false,
    };

    scr_interface_driver_t *iface_drv;
    scr_interface_create(SCREEN_IFACE_SPI, &spi_lcd_cfg, &iface_drv);
    esp_err_t ret = scr_find_driver(BOARD_LCD_TYPE, &s_lcd);

    if (ESP_OK != ret) {
        return;
        ESP_LOGE(TAG, "screen find failed");
    }

    scr_controller_config_t lcd_cfg = {
        .interface_drv = iface_drv,
        .pin_num_rst = BOARD_LCD_SPI_RESET_PIN,
        .pin_num_bckl = BOARD_LCD_SPI_BL_PIN,
        .rst_active_level = 0,
        .bckl_active_level = 1,
        .offset_hor = 0,
        .offset_ver = 0,
        .width = 240,
        .height = 320,
#ifdef BOARD_LCD_ST7789_DIR_MIRROR
        .rotate = SCR_DIR_TBRL,
#else
        .rotate = SCR_DIR_BTRL,
#endif
    };

    ret = s_lcd.init(&lcd_cfg);

    if (ESP_OK != ret) {
        return;
        ESP_LOGE(TAG, "screen initialize failed");
    }

    s_lcd.get_info(&s_lcd_info);
    ESP_LOGI(TAG, "Screen name:%s | width:%d | height:%d", s_lcd_info.name, s_lcd_info.width, s_lcd_info.height);

#ifdef BOARD_LCD_ST7789_INVERT
    lcd_st7789_set_invert(false);
#endif

    lcd_screen_clear(&s_lcd, COLOR_ESP_BKGD);
    vTaskDelay(pdMS_TO_TICKS(100));
    s_lcd_inited = true;
}

static void lcd_screen_clear(scr_driver_t *lcd, int color)
{
    scr_info_t lcd_info;
    lcd->get_info(&lcd_info);
    uint16_t *buffer = malloc(lcd_info.width * sizeof(uint16_t));

    if (NULL == buffer) {
        for (size_t y = 0; y < lcd_info.height; y++) {
            for (size_t x = 0; x < lcd_info.width; x++) {
                lcd->draw_pixel(x, y, color);
            }
        }
    } else {
        for (size_t i = 0; i < lcd_info.width; i++) {
            buffer[i] = color;
        }

        for (int y = 0; y < lcd_info.height; y++) {
            lcd->draw_bitmap(0, y, lcd_info.width, 1, buffer);
        }

        free(buffer);
    }
}

static bool lcd_write_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data)
{
    if (!s_lcd_inited) {
        ESP_LOGW(TAG, "lcd not inited");
        return false;
    }

    s_lcd.draw_bitmap(x, y, w, h, (uint16_t *)data);
    return true;
}

/* *******************************************************************************************
 * This callback function runs once per frame. Use it to perform any
 * quick processing you need, or have it put the frame into your application's
 * input queue. If this function takes too long, you'll start losing frames. */
static void frame_cb(uvc_frame_t *frame, void *ptr)
{
    assert(ptr);
    uint8_t *lcd_buffer = (uint8_t *)(ptr);
    ESP_LOGV(TAG, "callback! frame_format = %d, seq = %u, width = %d, height = %d, length = %u, ptr = %d",
             frame->frame_format, frame->sequence, frame->width, frame->height, frame->data_bytes, (int) ptr);

    switch (frame->frame_format) {
        case UVC_FRAME_FORMAT_MJPEG:
            mjpegdraw(frame->data, frame->data_bytes, lcd_buffer, lcd_write_bitmap);
            vTaskDelay(10 / portTICK_PERIOD_MS); /* add delay to free cpu to other tasks */
            break;

        default:
            ESP_LOGW(TAG, "Format not supported");
            assert(0);
            break;
    }
}


void app_main(void)
{
    /* Initialize lcd driver for display, the driver comes from esp-iot-solution,
    for test only, users can implement their driver for a specified lcd controller*/
    lcd_init();

    /* malloc a buffer for RGB565 data, as 320*240*2 = 153600B,
    here malloc a smaller buffer refresh lcd with steps */
    uint8_t *lcd_buffer = (uint8_t *)heap_caps_malloc(DEMO_SPI_MAX_TRANFER_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(lcd_buffer != NULL);

    /* Boot animation useful for LCD checking and camera power-on waiting, But consumes much flash */
#ifdef CONFIG_BOOT_ANIMATION
    esp_vfs_spiffs_conf_t spiffs_config = {
        .base_path              = "/spiffs",
        .partition_label        = NULL,
        .max_files              = 5,
        .format_if_mount_failed = false
    };

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffs_config));

    uint8_t *jpeg_buf = malloc(BOOT_ANIMATION_MAX_SIZE);
    assert(jpeg_buf != NULL);

    for (size_t i = 10; i <= 80; i += 2) {
        char file_name[64] = {0};
        sprintf(file_name, "/spiffs/video/r%03d.jpg", i);
        FILE *fd = fopen(file_name, "r");
        int read_bytes = fread(jpeg_buf, 1, BOOT_ANIMATION_MAX_SIZE, fd);
        fclose(fd);
        mjpegdraw(jpeg_buf, read_bytes, lcd_buffer, lcd_write_bitmap);
        ESP_LOGD(TAG, "file_name: %s, fd: %p, read_bytes: %d, free_heap: %d", file_name, fd, read_bytes, esp_get_free_heap_size());
    }

    free(jpeg_buf);
#endif

    /* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
    uint8_t *xfer_buffer_a = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    assert(xfer_buffer_a != NULL);
    uint8_t *xfer_buffer_b = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    assert(xfer_buffer_b != NULL);

    /* malloc frame buffer for a jpeg frame*/
    uint8_t *frame_buffer = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    assert(frame_buffer != NULL);

    /* the quick demo skip the standred get descriptors process,
    users need to get params from camera descriptors from PC side,
    eg. run `lsusb -v` in linux, then modify related MACROS */
    uvc_config_t uvc_config = {
        .dev_speed = USB_SPEED_FULL,
        .configuration = DESCRIPTOR_CONFIGURATION_INDEX,
        .format_index = DESCRIPTOR_FORMAT_MJPEG_INDEX,
        .frame_width = DEMO_FRAME_WIDTH,
        .frame_height = DEMO_FRAME_HEIGHT,
        .frame_index = DEMO_FRAME_INDEX,
        .frame_interval = DEMO_FRAME_INTERVAL,
        .interface = DESCRIPTOR_STREAM_INTERFACE_INDEX,
        .interface_alt = DEMO_ISOC_INTERFACE_ALT,
        .isoc_ep_addr = DESCRIPTOR_STREAM_ISOC_ENDPOINT_ADDR,
        .isoc_ep_mps = DEMO_ISOC_EP_MPS,
        .xfer_buffer_size = DEMO_XFER_BUFFER_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = DEMO_XFER_BUFFER_SIZE,
        .frame_buffer = frame_buffer,
    };

    /* pre-config UVC driver with params from known USB Camera Descriptors*/
    esp_err_t ret = uvc_streaming_config(&uvc_config);

    /* Start camera IN stream with pre-configs, uvc driver will create multi-tasks internal
    to handle usb data from different pipes, and user's callback will be called after new frame ready. */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uvc streaming config failed");
    } else {
        uvc_streaming_start(frame_cb, (void *)(lcd_buffer));
    }

    while (1) {
        /* task monitor code if necessary */
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}
