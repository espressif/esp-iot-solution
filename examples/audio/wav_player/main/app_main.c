/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "pwm_audio.h"
#include "file_manager.h"

static const char *TAG = "wav palyer";

typedef struct {
    // The "RIFF" chunk descriptor
    uint8_t ChunkID[4];
    int32_t ChunkSize;
    uint8_t Format[4];
    // The "fmt" sub-chunk
    uint8_t Subchunk1ID[4];
    int32_t Subchunk1Size;
    int16_t AudioFormat;
    int16_t NumChannels;
    int32_t SampleRate;
    int32_t ByteRate;
    int16_t BlockAlign;
    int16_t BitsPerSample;
    // The "data" sub-chunk
    uint8_t Subchunk2ID[4];
    int32_t Subchunk2Size;
} wav_header_t;

#ifdef CONFIG_STORAGE_SDCARD
static char **g_file_list = NULL;
static uint16_t g_file_num = 0;
#endif

static esp_err_t play_wav(const char *filepath)
{
    FILE *fd = NULL;
    struct stat file_stat;

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "file stat info: %s (%ld bytes)...", filepath, file_stat.st_size);
    fd = fopen(filepath, "r");

    if (NULL == fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        return ESP_FAIL;
    }
    const size_t chunk_size = 4096;
    uint8_t *buffer = malloc(chunk_size);

    if (NULL == buffer) {
        ESP_LOGE(TAG, "audio data buffer malloc failed");
        fclose(fd);
        return ESP_FAIL;
    }

    /**
     * read head of WAV file
     */
    wav_header_t wav_head;
    int len = fread(&wav_head, 1, sizeof(wav_header_t), fd);
    if (len <= 0) {
        ESP_LOGE(TAG, "Read wav header failed");
        fclose(fd);
        return ESP_FAIL;
    }
    if (NULL == strstr((char *)wav_head.Subchunk1ID, "fmt") &&
            NULL == strstr((char *)wav_head.Subchunk2ID, "data")) {
        ESP_LOGE(TAG, "Header of wav format error");
        fclose(fd);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "frame_rate=%d, ch=%d, width=%d", wav_head.SampleRate, wav_head.NumChannels, wav_head.BitsPerSample);

    pwm_audio_set_param(wav_head.SampleRate, wav_head.BitsPerSample, wav_head.NumChannels);
    pwm_audio_start();

    /**
     * read wave data of WAV file
     */
    size_t write_num = 0;
    size_t cnt;

    do {
        /* Read file in chunks into the scratch buffer */
        len = fread(buffer, 1, chunk_size, fd);
        if (len <= 0) {
            break;
        }
        pwm_audio_write(buffer, len, &cnt, 1000 / portTICK_PERIOD_MS);

        write_num += len;
    } while (1);

    pwm_audio_stop();
    fclose(fd);
    ESP_LOGI(TAG, "File reading complete, total: %d bytes", write_num);
    return ESP_OK;
}

void app_main()
{
    esp_err_t ret;

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    pwm_audio_config_t pac;
    pac.duty_resolution    = LEDC_TIMER_10_BIT;
    pac.gpio_num_left      = 25;
    pac.ledc_channel_left  = LEDC_CHANNEL_0;
    pac.gpio_num_right     = 26;
    pac.ledc_channel_right = LEDC_CHANNEL_1;
    pac.ledc_timer_sel     = LEDC_TIMER_0;
    pac.tg_num             = TIMER_GROUP_0;
    pac.timer_num          = TIMER_0;
    pac.ringbuf_len        = 1024 * 8;
    pwm_audio_init(&pac);

#ifdef CONFIG_STORAGE_SDCARD
    ret = fm_sdcard_init();
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "sdcard initial failed, exit");
        return;
    }
    fm_print_dir(MOUNT_POINT, 2);
    fm_file_table_create(&g_file_list, &g_file_num, ".wav");
    for (size_t i = 0; i < g_file_num; i++) {
        ESP_LOGI(TAG, "have file [%d:%s]", i, g_file_list[i]);
    }
    if (0 == g_file_num) {
        ESP_LOGW(TAG, "Can't found any wav file in sdcard!");
        return;
    }

    for (size_t i = 0; i < g_file_num; i++)
    {
        ESP_LOGI(TAG, "Start to play [%d:%s]", i, g_file_list[i]);
        char path_buf[256] = {0};
        sprintf(path_buf, "%s/%s", MOUNT_POINT, g_file_list[i]);
        play_wav(path_buf);
    }
    fm_file_table_free(&g_file_list, g_file_num);
    
#else
    ret = fm_spiffs_init();
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "sdcard initial failed, exit");
        return;
    }
    play_wav(MOUNT_POINT "/sample.wav");
#endif

}