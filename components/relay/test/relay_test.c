#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/rtc_io.h"
#include "relay.h"

#define RELAY_0_D_IO_NUM    2
#define RELAY_0_CP_IO_NUM   4
#define RELAY_1_D_IO_NUM    12
#define RELAY_1_CP_IO_NUM   13
#define TAG "relay"

static relay_handle_t relay_0, relay_1;

void relay_test()
{
    relay_io_t relay_io_0 = {
        .flip_io = {
            .d_io_num = RELAY_0_D_IO_NUM,
            .cp_io_num = RELAY_0_CP_IO_NUM,
        },
    };
    relay_io_t relay_io_1 = {
        .single_io = {
            .ctl_io_num = RELAY_1_D_IO_NUM,
        },
    };
    relay_0 = relay_create(relay_io_0, RELAY_CLOSE_HIGH, RELAY_DFLIP_CONTROL, RELAY_IO_RTC);
    relay_1 = relay_create(relay_io_1, RELAY_CLOSE_HIGH, RELAY_DFLIP_CONTROL, RELAY_IO_RTC);
    relay_state_write(relay_0, RELAY_STATUS_CLOSE);
    relay_state_write(relay_1, RELAY_STATUS_OPEN);
    ESP_LOGI(TAG, "relay0 state:%d", relay_state_read(relay_0));
    ESP_LOGI(TAG, "relay1 state:%d", relay_state_read(relay_1));
    vTaskDelay(1 / portTICK_RATE_MS);
    relay_state_write(relay_0, RELAY_STATUS_OPEN);
    relay_state_write(relay_1, RELAY_STATUS_CLOSE);
    ESP_LOGI(TAG, "relay0 state:%d", relay_state_read(relay_0));
    ESP_LOGI(TAG, "relay1 state:%d", relay_state_read(relay_1));
    vTaskDelay(1 / portTICK_RATE_MS);
    relay_state_write(relay_0, RELAY_STATUS_CLOSE);
    relay_state_write(relay_1, RELAY_STATUS_OPEN);
    ESP_LOGI(TAG, "relay0 state:%d", relay_state_read(relay_0));
    ESP_LOGI(TAG, "relay1 state:%d", relay_state_read(relay_1));
}