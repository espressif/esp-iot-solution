// Copyright 2020-2022 Espressif Systems (Shanghai) PTE LTD
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
#include <sys/unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "file_manager.h"

static const char *TAG = "file manager";

#define FLN_MAX CONFIG_FATFS_MAX_LFN

#ifndef CONFIG_EXAMPLE_USE_SDMMC_HOST
#define USE_SPI_MODE
#else
#if defined CONFIG_IDF_TARGET_ESP32 || defined CONFIG_IDF_TARGET_ESP32S3
#include "driver/sdmmc_host.h"
static sdmmc_card_t *mount_card = NULL;
#endif // define USE_SPI_MODE /* To enable SPI mode, uncomment this line*/
#endif // end define CONFIG_EXAMPLE_USE_SDMMC_HOST

// ESP32-S2 doesn't have an SD Host peripheral, always use SPI:
#ifdef CONFIG_IDF_TARGET_ESP32S2
#ifndef USE_SPI_MODE
#define USE_SPI_MODE
#endif // USE_SPI_MODE
// on ESP32-S2, DMA channel must be the same as host id
#define SPI_DMA_CHAN    host.slot
#endif //CONFIG_IDF_TARGET_ESP32S2

// DMA channel to be used by the SPI peripheral
#ifndef SPI_DMA_CHAN
#define SPI_DMA_CHAN    1
#endif //SPI_DMA_CHAN

// When testing SD and SPI modes, keep in mind that once the card has been
// initialized in SPI mode, it can not be reinitialized in SD mode without
// toggling power to the card.

#ifdef USE_SPI_MODE
// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define PIN_NUM_MISO 2
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   13
#endif //USE_SPI_MODE

esp_err_t fm_sdcard_init(void)
{
    ESP_LOGI(TAG, "Initializing SD card");

#ifndef USE_SPI_MODE
    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
#ifdef CONFIG_CAMERA_MODULE_ESP_S3_EYE
    // To use 1-line SD mode, change this to 1:
    slot_config.width = 1;
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
    // On chips where the GPIOs used for SD card can be configured, set them in
    // the slot_config structure:
    slot_config.clk = 39;
    slot_config.cmd = 38;
    slot_config.d0 = 40;
    slot_config.d1 = -1;
    slot_config.d2 = -1;
    slot_config.d3 = -1;
#endif // end SOC_SDMMC_USE_GPIO_MATRIX

#else // if not s3-eye model
    // To use 1-line SD mode, uncomment the following line:
    slot_config.width = 4;
    /* open SD card in MMC 1 bit mode
     MMC4  MMC1  ESP32
      D2          12
      D3    CS    13
      CMD   MOSI  15
      CLK   SCK   14
      D0    MISO  2
      D1          4
  */
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = GPIO_NUM_14;
    slot_config.cmd = GPIO_NUM_15;
    slot_config.d0 = GPIO_NUM_2;
    slot_config.d1 = GPIO_NUM_4;
    slot_config.d2 = GPIO_NUM_12;
    slot_config.d3 = GPIO_NUM_13;
#endif
    // GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
    // Internal pull-ups are not sufficient. However, enabling internal pull-ups
    // does make a difference some boards, so we do that here.
    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
    gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
    gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes
#endif // end CONFIG_CAMERA_MODULE_ESP_S3_EYE
#else // if use SPI modes
    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = PIN_NUM_MISO;
    slot_config.gpio_mosi = PIN_NUM_MOSI;
    slot_config.gpio_sck = PIN_NUM_CLK;
    slot_config.gpio_cs = PIN_NUM_CS;
    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
#endif // end USE_SPI_MODE

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 10,
        .allocation_unit_size = 32 * 1024,
    };

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
    // Please check its source code and implement error recovery when developing
    // production applications.
    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_CARD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    mount_card = card;

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    return ESP_OK;
}

esp_err_t fm_unmount_sdcard(void)
{
#ifdef USE_SPI_MODE
    esp_err_t err = esp_vfs_fat_sdcard_unmount(SD_CARD_MOUNT_POINT, mount_card);
#else
    esp_err_t err = esp_vfs_fat_sdmmc_unmount();
#endif
    ESP_ERROR_CHECK(err);
#ifdef USE_SPI_MODE
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    err = spi_bus_free(host.slot);
#endif
    ESP_ERROR_CHECK(err);

    return err;
}

esp_err_t fm_spiffs_init(void)
{
    esp_err_t ret;
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_MOUNT_POINT,
        .partition_label = NULL,
        .max_files = 5,   // This decides the maximum number of files that can be created on the storage
        .format_if_mount_failed = true
    };

    ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);

    return ESP_OK;
}

static void TraverseDir(char *direntName, int level, int indent)
{
    DIR *p_dir = NULL;
    struct dirent *p_dirent = NULL;

    p_dir = opendir(direntName);

    if (p_dir == NULL) {
        printf("opendir error\n");
        return;
    }

    while ((p_dirent = readdir(p_dir)) != NULL) {
        char *backupDirName = NULL;

        if (p_dirent->d_name[0] == '.') {
            continue;
        }

        int i;

        for (i = 0; i < indent; i++) {
            // printf("|");
            printf("     ");
        }

        printf("|--- %s", p_dirent->d_name);

        /* Itme is a file */
        if (p_dirent->d_type == DT_REG) {
            int curDirentNameLen = strlen(direntName) + strlen(p_dirent->d_name) + 2;

            //prepare next path
            backupDirName = (char *)malloc(curDirentNameLen);
            struct stat *st = NULL;
            st = malloc(sizeof(struct stat));

            if (NULL == backupDirName || NULL == st) {
                goto _err;
            }

            memset(backupDirName, 0, curDirentNameLen);
            memcpy(backupDirName, direntName, strlen(direntName));

            strcat(backupDirName, "/");
            strcat(backupDirName, p_dirent->d_name);

            int statok = stat(backupDirName, st);

            if (0 == statok) {
                printf("[%dB]\n", (int)st->st_size);
            } else {
                printf("\n");
            }

            free(backupDirName);
            backupDirName = NULL;
        } else {
            printf("\n");
        }

        /* Itme is a directory */
        if (p_dirent->d_type == DT_DIR) {
            int curDirentNameLen = strlen(direntName) + strlen(p_dirent->d_name) + 2;

            //prepare next path
            backupDirName = (char *)malloc(curDirentNameLen);

            if (NULL == backupDirName) {
                goto _err;
            }

            memset(backupDirName, 0, curDirentNameLen);
            memcpy(backupDirName, direntName, curDirentNameLen);

            strcat(backupDirName, "/");
            strcat(backupDirName, p_dirent->d_name);

            if (level > 0) {
                TraverseDir(backupDirName, level - 1, indent + 1);
            }

            free(backupDirName);
            backupDirName = NULL;
        }
    }

_err:
    closedir(p_dir);
}

void fm_print_dir(char *direntName, int level)
{
    printf("Traverse directory %s\n", direntName);
    TraverseDir(direntName, level, 0);
    printf("\r\n");
}

const char *fm_get_basepath(void)
{
    return SPIFFS_MOUNT_POINT;
}

esp_err_t fm_file_table_create(char ***list_out, uint16_t *files_number, const char *filter_suffix)
{
    DIR *p_dir = NULL;
    struct dirent *p_dirent = NULL;

    p_dir = opendir(SPIFFS_MOUNT_POINT);

    if (p_dir == NULL) {
        ESP_LOGE(TAG, "opendir error");
        return ESP_FAIL;
    }

    uint16_t f_num = 0;
    while ((p_dirent = readdir(p_dir)) != NULL) {
        if (p_dirent->d_type == DT_REG) {
            f_num++;
        }
    }

    rewinddir(p_dir);

    *list_out = calloc(f_num, sizeof(char *));
    if (NULL == (*list_out)) {
        goto _err;
    }
    for (size_t i = 0; i < f_num; i++) {
        (*list_out)[i] = malloc(FLN_MAX);
        if (NULL == (*list_out)[i]) {
            ESP_LOGE(TAG, "malloc failed at %d", i);
            fm_file_table_free(list_out, f_num);
            goto _err;
        }
    }

    uint16_t index = 0;
    while ((p_dirent = readdir(p_dir)) != NULL) {
        if (p_dirent->d_type == DT_REG) {
            if (NULL != filter_suffix) {
                if (strstr(p_dirent->d_name, filter_suffix)) {
                    strncpy((*list_out)[index], p_dirent->d_name, FLN_MAX - 1);
                    (*list_out)[index][FLN_MAX - 1] = '\0';
                    index++;
                }
            } else {
                strncpy((*list_out)[index], p_dirent->d_name, FLN_MAX - 1);
                (*list_out)[index][FLN_MAX - 1] = '\0';
                index++;
            }
        }
    }
    (*files_number) = index;

    closedir(p_dir);
    return ESP_OK;
_err:
    closedir(p_dir);

    return ESP_FAIL;
}

esp_err_t fm_file_table_free(char ***list, uint16_t files_number)
{
    for (size_t i = 0; i < files_number; i++) {
        free((*list)[i]);
    }
    free((*list));
    return ESP_OK;
}

const char *fm_get_filename(const char *file)
{
    const char *p = file + strlen(file);
    while (p > file) {
        if ('/' == *p) {
            return (p + 1);
        }
        p--;
    }
    return file;
}

size_t fm_get_file_size(const char *filepath)
{
    struct stat siz = {0};
    stat(filepath, &siz);
    return siz.st_size;
}
