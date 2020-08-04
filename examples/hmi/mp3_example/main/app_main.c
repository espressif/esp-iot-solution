// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
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

/*
                           +---------------+
                           |               |
                           |      HMI      |
                           |               |
                           +----^-----+----+
                                |     |
                                |     |
                                |     |
                                |     |
                                |     |
+---------------+          +----+-----v----+           +--------------+
|               |          |               |           |              |
|    SD-Card    +---------->     ESP32     +----------->   DAC-Audio  |
|               |          |               |           |              |
+---------------+          +---------------+           +--------------+
*/
/* component includes */
#include <stdio.h>

/* freertos includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_freertos_hooks.h"

/* esp includes */
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "ff.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

/* Param Include */
#include "iot_param.h"
#include "nvs_flash.h"

/* lvgl includes */
#include "iot_lvgl.h"
#include "lv_calibration.h"

/**********************
 *      MACROS
 **********************/
#define CONTROL_CURRENT -1
#define CONTROL_NEXT -2
#define CONTROL_PREV -3
#define MAX_PLAY_FILE_NUM 20

#define TAG "mp3_example"
#define USE_ADF_TO_PLAY CONFIG_USE_ADF_PLAY

#if USE_ADF_TO_PLAY
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "fatfs_stream.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#endif

/**********************
 *  STATIC VARIABLES
 **********************/
/* LVGL Object */
static lv_obj_t *current_music;
static lv_obj_t *button[3];
static lv_obj_t *img[3];
static lv_obj_t *list_music[MAX_PLAY_FILE_NUM];

/* Image and music resource */
static uint8_t filecount = 0;
static char directory[MAX_PLAY_FILE_NUM][100];
static bool play = false;
void *img_src[] = {SYMBOL_PREV, SYMBOL_PLAY, SYMBOL_NEXT, SYMBOL_PAUSE};

#if USE_ADF_TO_PLAY
static audio_pipeline_handle_t pipeline;
static audio_element_handle_t i2s_stream_writer, mp3_decoder;
#endif

static FILE *get_file(int next_file)
{
    static FILE *file;
    static int file_index = 1;

    if (next_file != CONTROL_CURRENT) {
        if (next_file == CONTROL_NEXT) {
            // advance to the next file
            if (++file_index > filecount - 1) {
                file_index = 0;
            }
        } else if (next_file == CONTROL_PREV) {
            // advance to the prev file
            if (--file_index < 0) {
                file_index = filecount - 1;
            }
        } else if (next_file >= 0 && next_file < filecount) {
            file_index = next_file;
        }
#if USE_ADF_TO_PLAY
        if (file != NULL) {
            fclose(file);
            file = NULL;
        }
#endif
        ESP_LOGI(TAG, "[ * ] File index %d", file_index);
    }
    // return a handle to the current file
    if (file == NULL) {
        lv_label_set_text(current_music, strstr(directory[file_index], "d/") + 2);
#if USE_ADF_TO_PLAY
        file = fopen(directory[file_index], "r");
        if (!file) {
            ESP_LOGE(TAG, "Error opening file");
            return NULL;
        }
#endif
    }
    return file;
}

static lv_res_t audio_next_prev(lv_obj_t *obj)
{
    if (obj == button[0]) {
        // prev song
#if USE_ADF_TO_PLAY
        ESP_LOGI(TAG, "[ * ] [Set] touch tap event");
        audio_pipeline_terminate(pipeline);
        ESP_LOGI(TAG, "[ * ] Stopped, advancing to the prev song");
#endif
        get_file(CONTROL_PREV);
#if USE_ADF_TO_PLAY
        audio_pipeline_run(pipeline);
#endif
        lv_img_set_src(img[1], img_src[3]);
        play = true;
    } else if (obj == button[1]) {
    } else if (obj == button[2]) {
        // next song
#if USE_ADF_TO_PLAY
        ESP_LOGI(TAG, "[ * ] [Set] touch tap event");
        audio_pipeline_terminate(pipeline);
        ESP_LOGI(TAG, "[ * ] Stopped, advancing to the next song");
#endif
        get_file(CONTROL_NEXT);
#if USE_ADF_TO_PLAY
        audio_pipeline_run(pipeline);
#endif
        lv_img_set_src(img[1], img_src[3]);
        play = true;
    }
    return LV_RES_OK;
}

static lv_res_t audio_control(lv_obj_t *obj)
{
#if USE_ADF_TO_PLAY
    audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
    switch (el_state) {
    case AEL_STATE_INIT:
        lv_img_set_src(img[1], img_src[3]);
        audio_pipeline_run(pipeline);
        break;
    case AEL_STATE_RUNNING:
        lv_img_set_src(img[1], img_src[1]);
        audio_pipeline_pause(pipeline);
        break;
    case AEL_STATE_PAUSED:
        lv_img_set_src(img[1], img_src[3]);
        audio_pipeline_resume(pipeline);
        break;
    default:
        break;
    }
#else
    play ? lv_img_set_src(img[1], img_src[1]) : lv_img_set_src(img[1], img_src[3]);
    play = !play;
#endif
    return LV_RES_OK;
}

//"Night theme\nAlien theme\nMaterial theme\nZen theme\nMono theme\nNemo theme"
static lv_res_t theme_change_action(lv_obj_t *roller)
{
    lv_theme_t *th;
    switch (lv_ddlist_get_selected(roller)) {
    case 0:
        th = lv_theme_night_init(100, NULL);
        break;

    case 1:
        th = lv_theme_alien_init(100, NULL);
        break;

    case 2:
        th = lv_theme_material_init(100, NULL);
        break;

    case 3:
        th = lv_theme_zen_init(100, NULL);
        break;

    case 4:
        th = lv_theme_mono_init(100, NULL);
        break;

    case 5:
        th = lv_theme_nemo_init(100, NULL);
        break;

    default:
        th = lv_theme_default_init(100, NULL);
        break;
    }
    lv_theme_set_current(th);
    return LV_RES_OK;
}

static lv_res_t play_list(lv_obj_t *obj)
{
    for (uint8_t i = 0; i < MAX_PLAY_FILE_NUM; i++) {
        if (obj == list_music[i]) {
#if USE_ADF_TO_PLAY
            audio_pipeline_stop(pipeline);
            audio_pipeline_wait_for_stop(pipeline);
            // audio_pipeline_terminate(pipeline);
#endif
            get_file(i);
#if USE_ADF_TO_PLAY
            audio_pipeline_run(pipeline);
#endif
            lv_img_set_src(img[1], img_src[3]);
            play = true;
            break;
        }
    }
    return LV_RES_OK;
}

static void littlevgl_mp3(void)
{
    lv_obj_t *scr = lv_obj_create(NULL, NULL);
    lv_scr_load(scr);

    lv_theme_t *th = lv_theme_zen_init(100, NULL);
    lv_theme_set_current(th);

    lv_obj_t *tabview = lv_tabview_create(lv_scr_act(), NULL);

    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, SYMBOL_AUDIO);
    lv_obj_t *tab2 = lv_tabview_add_tab(tabview, SYMBOL_LIST);
    lv_obj_t *tab3 = lv_tabview_add_tab(tabview, SYMBOL_SETTINGS);
    lv_tabview_set_tab_act(tabview, 0, false);

    lv_obj_t *cont = lv_cont_create(tab1, NULL);
    current_music = lv_label_create(cont, NULL);
    lv_label_set_long_mode(current_music, LV_LABEL_LONG_ROLL);
    lv_obj_set_width(current_music, 200);
    lv_obj_align(current_music, cont, LV_ALIGN_IN_TOP_MID, 0, 20); /*Align to center*/
    lv_label_set_text(current_music, strstr(directory[1], "d/") + 2);
    lv_obj_set_pos(current_music, 50, 20);

    lv_obj_set_size(cont, LV_HOR_RES - 20, LV_VER_RES - 85);
    lv_cont_set_fit(cont, false, false);
    for (uint8_t i = 0; i < 3; i++) {
        button[i] = lv_btn_create(cont, NULL);
        lv_obj_set_size(button[i], 50, 50);
        img[i] = lv_img_create(button[i], NULL);
        lv_img_set_src(img[i], img_src[i]);
    }
    lv_obj_align(button[0], cont, LV_ALIGN_IN_LEFT_MID, 35, 20);
    for (uint8_t i = 1; i < 3; i++) {
        lv_obj_align(button[i], button[i - 1], LV_ALIGN_OUT_RIGHT_MID, 40, 0);
    }
    lv_btn_set_action(button[0], LV_BTN_ACTION_CLICK, audio_next_prev);
    lv_btn_set_action(button[1], LV_BTN_ACTION_CLICK, audio_control);
    lv_btn_set_action(button[2], LV_BTN_ACTION_CLICK, audio_next_prev);

    lv_obj_t *list = lv_list_create(tab2, NULL);
    lv_obj_set_size(list, LV_HOR_RES - 20, LV_VER_RES - 85);
    for (uint8_t i = 0; i < filecount; i++) {
        list_music[i] = lv_list_add(list, SYMBOL_AUDIO, strstr(directory[i], "d/") + 2, play_list);
    }

    lv_obj_t *theme_label = lv_label_create(tab3, NULL);
    lv_label_set_text(theme_label, "Theme:");

    lv_obj_t *theme_roller = lv_roller_create(tab3, NULL);
    lv_obj_align(theme_roller, theme_label, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    lv_roller_set_options(theme_roller, "Night theme\nAlien theme\nMaterial theme\nZen theme\nMono theme\nNemo theme");
    lv_roller_set_selected(theme_roller, 1, false);
    lv_roller_set_visible_row_count(theme_roller, 3);
    lv_ddlist_set_action(theme_roller, theme_change_action);
    ESP_LOGI("LvGL", "app_main last: %d", esp_get_free_heap_size());
}

static void sdmmc_init()
{
    sdmmc_card_t *card;
    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // To use 1-line SD mode, uncomment the following line:
    // host.flags = SDMMC_HOST_FLAG_1BIT;

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_cd = (gpio_num_t)34;

    // GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
    // Internal pull-ups are not sufficient. However, enabling internal pull-ups
    // does make a difference some boards, so we do that here.
    gpio_set_pull_mode((gpio_num_t)15, GPIO_PULLUP_ONLY); // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode((gpio_num_t)2, GPIO_PULLUP_ONLY);  // D0, needed in 4- and 1- line modes
    gpio_set_pull_mode((gpio_num_t)4, GPIO_PULLUP_ONLY);  // D1, needed in 4-line mode only
    gpio_set_pull_mode((gpio_num_t)12, GPIO_PULLUP_ONLY); // D2, needed in 4-line mode only
    gpio_set_pull_mode((gpio_num_t)13, GPIO_PULLUP_ONLY); // D3, needed in 4- and 1- line modes

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
    // Please check its source code and implement error recovery when developing
    // production applications.
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
}

static void read_content(const char *path, uint8_t *filecount)
{
    char nextpath[300];
    struct dirent *de;
    *filecount = 0;
    DIR *dir = opendir(path);
    while (1) {
        de = readdir(dir);
        if (!de) {
            break;
        } else if (de->d_type == DT_REG) {
            if (strstr(de->d_name, ".mp3") || strstr(de->d_name, ".MP3")) {
                sprintf(directory[*filecount], "%s/%s", path, de->d_name);
                printf("%s\n", directory[*filecount]);
                if ((*filecount)++ >= MAX_PLAY_FILE_NUM) {
                    break;
                }
            }
        } else if (de->d_type == DT_DIR) {
            sprintf(nextpath, "%s/%s", path, de->d_name);
            read_content(nextpath, filecount);
        }
    }
    closedir(dir);
}

#if USE_ADF_TO_PLAY
/*
* Callback function to feed audio data stream from sdcard to mp3 decoder element
*/
static int my_sdcard_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    int read_len = fread(buf, 1, len, get_file(CONTROL_CURRENT));
    if (read_len == 0) {
        read_len = AEL_IO_DONE;
    }
    return read_len;
}

static void audio_sdcard_task(void *para)
{
    ESP_LOGI(TAG, "[ 1 ] Mount sdcard");
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    // Initialize SD Card peripheral
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = 34, //GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    // Start sdcard & button peripheral
    esp_periph_start(set, sdcard_handle);

    // Wait until sdcard was mounted
    while (!periph_sdcard_is_mounted(sdcard_handle)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "[2.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[2.1] Create i2s stream to write data to ESP32 internal DAC");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_INTERNAL_DAC_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[2.2] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);
    audio_element_set_read_cb(mp3_decoder, my_sdcard_read_cb, NULL);

    ESP_LOGI(TAG, "[2.3] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[2.4] Link it together [my_sdcard_read_cb]-->mp3_decoder-->i2s_stream-->[codec_chip]");
    audio_pipeline_link(pipeline, (const char *[]) {
        "mp3", "i2s"
    }, 2);

    ESP_LOGI(TAG, "[ 3 ] Setup event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[3.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[3.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 4 ] Listen for all pipeline events");
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)mp3_decoder && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(mp3_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            audio_element_setinfo(i2s_stream_writer, &music_info);
            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        // Advance to the next song when previous finishes
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)i2s_stream_writer && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
            audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
            if (el_state == AEL_STATE_FINISHED) {
                ESP_LOGI(TAG, "[ * ] Finished, advancing to the next song");
                audio_pipeline_stop(pipeline);
                audio_pipeline_wait_for_stop(pipeline);
                get_file(CONTROL_NEXT);
                audio_pipeline_run(pipeline);
            }
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)i2s_stream_writer && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (int)msg.data == AEL_STATUS_STATE_STOPPED) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }

    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_terminate(pipeline);

    /* Terminal the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(mp3_decoder);
    esp_periph_set_destroy(set);
    vTaskDelete(NULL);
}
#endif

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main()
{
    /* Initialize LittlevGL GUI */
    lvgl_init();

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    sdmmc_init();
    read_content("/sdcard", &filecount);
    esp_vfs_fat_sdmmc_unmount();
    ESP_LOGI(TAG, "[APP] Music File Count: %d", filecount);

#if USE_ADF_TO_PLAY
    gpio_set_pull_mode((gpio_num_t)15, GPIO_PULLUP_ONLY); // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode((gpio_num_t)2, GPIO_PULLUP_ONLY);  // D0, needed in 4- and 1- line modes
    gpio_set_pull_mode((gpio_num_t)4, GPIO_PULLUP_ONLY);  // D1, needed in 4-line mode only
    gpio_set_pull_mode((gpio_num_t)12, GPIO_PULLUP_ONLY); // D2, needed in 4-line mode only
    gpio_set_pull_mode((gpio_num_t)13, GPIO_PULLUP_ONLY); // D3, needed in 4- and 1- line modes
    xTaskCreate(audio_sdcard_task, "audio_sdcard_task", 1024 * 5, NULL, 4, NULL);
#endif

    /* mp3 example */
    littlevgl_mp3();

    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
}
