#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/touch_pad.h"
#include "touchpad.h"
#include "esp_log.h"

#define TOUCH_PAD_TEST 0
#if TOUCH_PAD_TEST
#define TOUCHPAD_THRESHOLD  1000
#define TOUCHPAD_FILTER_VALUE   150
static const char* TAG = "touchpad_test";
xQueueHandle xQueue_tp;
static touchpad_handle_t touchpad_dev0, touchpad_dev1;

static void touchpad_task(void* arg)
{
    portBASE_TYPE xStatus;
    touchpad_msg_t recv_value;
    uint16_t value;
    while(1) {
        //touchpad_read(touchpad_dev0, &value);
        //ESP_LOGI(TAG, "touchpad_value:%u", value);
        //vTaskDelay(100 / portTICK_RATE_MS);
        if (xQueue_tp != NULL) {
            xStatus = xQueueReceive(xQueue_tp, &recv_value, portMAX_DELAY);
            if (xStatus == pdPASS) {
                ESP_LOGI(TAG, "number of touch pad:%d", recv_value.num);
                switch (recv_value.event) {
                    case TOUCHPAD_EVENT_TAP:
                        ESP_LOGI(TAG, "touch pad event tap");
                        break;
                    case TOUCHPAD_EVENT_LONG_PRESS:
                        ESP_LOGI(TAG, "touch pad event long press");
                        break;
                    case TOUCHPAD_EVENT_PUSH:
                        ESP_LOGI(TAG, "touch pad event push");
                        break;
                    case TOUCHPAD_EVENT_RELEASE:
                        ESP_LOGI(TAG, "touch pad event release");
                        break;
                    default:
                        break;
                }
            }
        }
        else {
            ESP_LOGI(TAG, "touch pads are all deleted");
            vTaskDelete(NULL);
        }
    }
}

void touchpad_test()
{
    xQueue_tp = NULL;
    touchpad_dev0 = touchpad_create(TOUCH_PAD_NUM8, TOUCHPAD_THRESHOLD, TOUCHPAD_FILTER_VALUE, TOUCHPAD_SINGLE_TRIGGER, 5, &xQueue_tp, 20);
    touchpad_dev1 = touchpad_create(TOUCH_PAD_NUM9, TOUCHPAD_THRESHOLD, TOUCHPAD_FILTER_VALUE, TOUCHPAD_SERIAL_TRIGGER, 5, &xQueue_tp, 20);
    TaskHandle_t xHandle = NULL;
    xTaskCreate(touchpad_task, "touchpad_task", 2048, NULL, 5, &xHandle);
    vTaskDelay((30 * 1000) / portTICK_RATE_MS);
    ESP_LOGI(TAG, "touchpad 0 deleted");
    touchpad_delete(touchpad_dev0);
    vTaskDelay((30 * 1000) / portTICK_RATE_MS);
    ESP_LOGI(TAG, "touchpad 1 deleted"); 
    touchpad_delete(touchpad_dev1);
}
#endif
