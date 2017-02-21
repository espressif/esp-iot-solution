#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/touch_pad.h"
#include "touchpad.h"
#include "esp_log.h"

#define TOUCHPAD_THRESHOLD  1000
#define TOUCHPAD_FILTER_VALUE   200
static const char* TAG = "touchpad_test";
extern xQueueHandle xQueue_touch_pad;
static touchpad_handle_t touchpad_dev0, touchpad_dev1;

static void touchpad_task(void* arg)
{
    portBASE_TYPE xStatus;
    touchpad_handle_t recv_value;
    uint16_t touch_value;
    while(1) {
        xStatus = xQueueReceive(xQueue_touch_pad, &recv_value, portMAX_DELAY);
        if (xStatus == pdPASS) {
            ESP_LOGI(TAG, "number of touch pad:%d", touchpad_num_get(recv_value));
            touchpad_read(recv_value, &touch_value);
            ESP_LOGI(TAG, "sample value of touch pad:%d", touch_value);
            if (recv_value == touchpad_dev0) {
                //touchpad_set_trigger(touchpad_dev1, TOUCHPAD_SINGLE_TRIGGER);
                //touchpad_set_threshold(touchpad_dev1, 3000);
                //touchpad_delete(touchpad_dev1);
                //touchpad_dev1 = NULL;
            }
        }
    }
}

void touchpad_test()
{
    touchpad_dev0 = touchpad_create(TOUCH_PAD_NUM8, TOUCHPAD_THRESHOLD, TOUCHPAD_FILTER_VALUE, TOUCHPAD_SINGLE_TRIGGER);
    touchpad_dev1 = touchpad_create(TOUCH_PAD_NUM9, TOUCHPAD_THRESHOLD, TOUCHPAD_FILTER_VALUE, TOUCHPAD_SERIAL_TRIGGER);
    TaskHandle_t xHandle = NULL;
    xTaskCreate(touchpad_task, "touchpad_task", 2048, NULL, 5, &xHandle);
}