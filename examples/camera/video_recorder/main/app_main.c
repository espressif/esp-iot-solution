/* video recorder Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "camera_pin.h"
#include "esp_camera.h"
#include "file_manager.h"
#include "avi_recorder.h"
#include "app_wifi.h"

#include "sensor.h"

#define EXAMPLE_SENSOR_FRAME_SIZE FRAMESIZE_VGA
#define EXAMPLE_FB_COUNT          (8)

static const char *TAG = "video_recorder";

esp_err_t camera_init(uint32_t mclk_freq, const pixformat_t pixel_fromat, const framesize_t frame_size, size_t frame_buffer_count)
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

        //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
        .xclk_freq_hz = mclk_freq,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = pixel_fromat, //YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = frame_size,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

        .jpeg_quality = 9, //0-63 lower number means higher quality
        .fb_count = frame_buffer_count, //if more than one, i2s runs in continuous mode. Use only with JPEG
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
        .fb_location = CAMERA_FB_IN_PSRAM,
    };

    // camera init
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1); //flip it back
    s->set_hmirror(s, 1);
    s->set_saturation(s, 1);
    //initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
        s->set_brightness(s, 1);  //up the blightness just a bit
        s->set_saturation(s, -2); //lower the saturation
    }
    return ESP_OK;
}

static uint32_t camera_test_fps(uint16_t times)
{
    uint32_t image_size = 0;
    uint32_t ret;
    ESP_LOGI(TAG, "satrt to test fps");
    esp_camera_fb_return(esp_camera_fb_get());
    esp_camera_fb_return(esp_camera_fb_get());

    uint64_t total_time = esp_timer_get_time();
    for (size_t i = 0; i < times; i++) {
        camera_fb_t *pic = esp_camera_fb_get();
        if (NULL == pic) {
            ESP_LOGW(TAG, "fb get failed");
            continue;
        }

        image_size += pic->len;
        esp_camera_fb_return(pic);
    }
    total_time = esp_timer_get_time() - total_time;
    float fps = times / (total_time / 1000000.0f);
    ret = image_size / times;
    ESP_LOGI(TAG, "fps=%f, image_average_size=%u", fps, ret);
    return ret;
}

static int _get_frame(void **buf, size_t *len)
{
    camera_fb_t *image_fb = esp_camera_fb_get();
    if (!image_fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        return -1;
    } else {
        // ESP_LOGI(TAG, "len=%d", image_fb->len);
        *buf = &image_fb->buf;
        *len = image_fb->len;
    }
    return 0;
}

static int _return_frame(void *inbuf)
{
    camera_fb_t *image_fb = __containerof(inbuf, camera_fb_t, buf);
    esp_camera_fb_return(image_fb);
    return 0;
}

#if CONFIG_EXAMPLE_FILE_SERVER_ENABLED
esp_err_t example_start_file_server(const char *base_path);

static esp_err_t server_start(void)
{
    /* This helper function configures Wi-Fi */
    app_wifi_main();

    /* Start the file server */
    ESP_ERROR_CHECK(example_start_file_server(SD_CARD_MOUNT_POINT));

    return ESP_OK;
}
#endif

void app_main()
{
    const char *video_file = SD_CARD_MOUNT_POINT"/recorde.avi";
    int ret = camera_init(20000000, PIXFORMAT_JPEG, EXAMPLE_SENSOR_FRAME_SIZE, EXAMPLE_FB_COUNT);
    if (ret != 0) {
        ESP_LOGE(TAG, "camera init fail");
    } else {
        camera_test_fps(16);

        ESP_ERROR_CHECK(fm_sdcard_init()); /* Initialize file storage */

        avi_recorder_start(video_file, _get_frame, _return_frame, resolution[EXAMPLE_SENSOR_FRAME_SIZE].width, resolution[EXAMPLE_SENSOR_FRAME_SIZE].height, 10 * 2, 1);
    }

#if CONFIG_EXAMPLE_FILE_SERVER_ENABLED
    server_start();
#else
    avi_recorder_stop();
    fm_unmount_sdcard();
#endif
}
