// Copyright 2016-2021 Espressif Systems (Shanghai) PTE LTD
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
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "uvc_stream.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_types.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

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

/* SPISD IO Related MACROS */
#define BOARD_SDCARD_MOSI_PIN 38
#define BOARD_SDCARD_MISO_PIN 40
#define BOARD_SDCARD_SCLK_PIN 39
#define BOARD_SDCARD_CS_PIN 37

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

static const char *TAG = "uvc_demo";

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

/****************** SD CARD code ******************/
static sdmmc_card_t *mount_card = NULL;
#define BASE_PATH "/disk"

/**
 * @brief Mount SD card to VFS
 * 
 * @param card_handle card handle for operate
 * @return ** esp_err_t 
 */
static esp_err_t sdcard_fat_init(sdmmc_card_t **card_handle)
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    esp_err_t err = ESP_FAIL;
    const char base_path[] = BASE_PATH;
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formated before

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 64 * 512
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BOARD_SDCARD_MOSI_PIN,
        .miso_io_num = BOARD_SDCARD_MISO_PIN,
        .sclk_io_num = BOARD_SDCARD_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
        .intr_flags = ESP_INTR_FLAG_LEVEL3,
    };

    err = spi_bus_initialize(host.slot, &bus_cfg, host.slot);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ESP_FAIL;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = BOARD_SDCARD_CS_PIN;
    slot_config.host_id = host.slot;
    
    sdmmc_card_t *card;
    err = esp_vfs_fat_sdspi_mount(base_path, &host, &slot_config, &mount_config, &card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return ESP_FAIL;
    }
    sdmmc_card_print_info(stdout, card);
    if(card_handle) *card_handle = card;

#if 0
    /*test code*/    
    FILE *f = fopen(BASE_PATH"/hello.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read hello.txt from SD Card: '%s'", line);
#endif

    return ESP_OK;
}

static void frame_cb_sdcard(uvc_frame_t *frame, void *ptr)
{
    FILE *fp;
    static int jpeg_count = 0;
    const char *MJPEG_FILE = ".jpg";
    const int trigger_threshold = 30;
    static int trigger_counter = 0; 
    char filename[32];
    ESP_LOGI(TAG, "callback! frame_format = %d, seq = %u, width = %d, height = %d, length = %u, ptr = %d",
            frame->frame_format, frame->sequence, frame->width, frame->height, frame->data_bytes, (int) ptr);

    if (trigger_counter < trigger_threshold){
        trigger_counter++;
        return;
    }
    trigger_counter = 0;
    switch (frame->frame_format) {
        case UVC_FRAME_FORMAT_MJPEG:
            uvc_streaming_suspend();
            sprintf(filename, "/disk/%03d%s", jpeg_count++, MJPEG_FILE);
            fp = fopen(filename, "wb");
            if (fp == NULL) {
                ESP_LOGW(TAG, "Open file %s failed", filename);
                return;    /* code */
            }
            fwrite(frame->data, 1, frame->data_bytes, fp);
            fclose(fp);
            ESP_LOGI(TAG, "Write file %s suceed", filename);
            uvc_streaming_resume();
            break;
        default:
            ESP_LOGW(TAG, "Format %d not supported", frame->frame_format);
            break;
    }
}


void app_main(void)
{
    /* Initialize SD Card driver for frames saving */
    sdcard_fat_init(&mount_card);
    if (mount_card == NULL) {
        ESP_LOGE(TAG, "init SD Card failed !");
        abort();
    }

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
        uvc_streaming_start(frame_cb_sdcard, NULL);
    }

    while (1) {
        /* task monitor code if necessary */
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}
