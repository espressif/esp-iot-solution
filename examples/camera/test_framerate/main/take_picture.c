// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <mbedtls/base64.h>
#include "esp_log.h"

#include "esp_camera.h"
#include "camera_pin.h"

#define TEST_ESP_OK(ret) assert(ret == ESP_OK)
#define TEST_ASSERT_NOT_NULL(ret) assert(ret != NULL)
#define FB_COUNT_IN_RAM (2) // Frame buffer count used to storage frame passed by the sensor
#define PIC_COUNT_TEST (36) // Total number of pictures to be acquired for testing

static const char *TAG = "test camera";

static esp_err_t init_camera(uint32_t xclk_freq_hz, pixformat_t pixel_format, framesize_t frame_size, uint8_t fb_count)
{
    camera_config_t camera_config = {
        .pin_pwdn = CAMERA_PIN_PWDN,
        .pin_reset = CAMERA_PIN_RESET,
        .pin_xclk = CAMERA_PIN_XCLK,
        .pin_sscb_sda = CAMERA_PIN_SIOD,
        .pin_sscb_scl = CAMERA_PIN_SIOC,

        .pin_d7 = CAMERA_PIN_D7,
        .pin_d6 = CAMERA_PIN_D6,
        .pin_d5 = CAMERA_PIN_D5,
        .pin_d4 = CAMERA_PIN_D4,
        .pin_d3 = CAMERA_PIN_D3,
        .pin_d2 = CAMERA_PIN_D2,
        .pin_d1 = CAMERA_PIN_D1,
        .pin_d0 = CAMERA_PIN_D0,
        .pin_vsync = CAMERA_PIN_VSYNC,
        .pin_href = CAMERA_PIN_HREF,
        .pin_pclk = CAMERA_PIN_PCLK,

        // EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode
        .xclk_freq_hz = xclk_freq_hz,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = pixel_format, // YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = frame_size,    // QQVGA-UXGA, sizes above QVGA when not JPEG format are not recommended.

        .jpeg_quality = 30, // 0-63 lower number means higher quality
        .fb_count = fb_count,       // if more than one, i2s runs in continuous mode.
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
        .fb_location = CAMERA_FB_IN_PSRAM
    };

    //initialize the camera
    esp_err_t ret = esp_camera_init(&camera_config);

    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);//flip it back

    return ret;
}

static bool camera_test_fps(uint16_t times, float *fps, uint32_t *size)
{
    *fps = 0.0f;
    *size = 0;
    uint32_t s = 0;
    uint32_t num = 0;
    uint64_t total_time = esp_timer_get_time();
    for (size_t i = 0; i < times; i++) {
        camera_fb_t *pic = esp_camera_fb_get();
        if (NULL == pic) {
            ESP_LOGW(TAG, "fb get failed");
            return 0;
        } else {
            s += pic->len;
            num++;
        }
        esp_camera_fb_return(pic);
    }
    total_time = esp_timer_get_time() - total_time;
    if (num) {
        *fps = num * 1000000.0f / total_time;
        *size = s / num;
    }
    return 1;
}

static void __attribute__((noreturn)) task_fatal_error(void)
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}

static void camera_performance_test_with_format(uint32_t xclk_freq, uint32_t pic_num, pixformat_t pixel_format, framesize_t frame_size)
{
    esp_err_t ret = ESP_OK;
    uint64_t t1 = 0;
    // detect sensor information
    t1 = esp_timer_get_time();
    TEST_ESP_OK(init_camera(10000000, pixel_format, frame_size, 2));
    printf("Cam prob %llu\n", (esp_timer_get_time() - t1) / 1000);
    // get sensor info
    sensor_t *s = esp_camera_sensor_get();
    camera_sensor_info_t *info = esp_camera_sensor_get_info(&s->id);
    TEST_ASSERT_NOT_NULL(info);
    // deinit sensor
    TEST_ESP_OK(esp_camera_deinit());
    vTaskDelay(500 / portTICK_RATE_MS);

    struct fps_result {
        float fps;
        uint32_t size;
    };
    struct fps_result results = {0};

    ret = init_camera(xclk_freq, pixel_format, frame_size, FB_COUNT_IN_RAM);
    vTaskDelay(100 / portTICK_RATE_MS);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "Testing init failed :-(, skip this item");
        task_fatal_error();
    }
    camera_test_fps(pic_num, &results.fps, &results.size);
    TEST_ESP_OK(esp_camera_deinit());

    printf("FPS Result\n");
    printf("fps, size \n");

    printf("%5.2f,     %7d \n",
        results.fps, results.size);

    printf("----------------------------------------------------------------------------------------\n");
}

void app_main()
{
    camera_performance_test_with_format(20 * 1000000, PIC_COUNT_TEST, PIXFORMAT_RGB565, FRAMESIZE_QVGA);
}
