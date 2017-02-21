// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_attr.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "touchpad.h"

#define TOUCHPAD_QUEUE_LEN      10

#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                             \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                                   \
        }

#define ERR_ASSERT(tag, param)  IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define RES_ASSERT(tag, res, ret)   IOT_CHECK(tag, (res) != pdFALSE, ret)

typedef struct {
    uint32_t last_tick;
    touch_pad_t touch_pad_num;
    uint32_t threshold;
    uint32_t filter_value;
    touchpad_trigger_t trig_mode;
    uint32_t sum_tick;
} touchpad_dev_t;
    
// Debug tag in esp log
static const char* TAG = "touchpad";

xQueueHandle xQueue_touch_pad = NULL;
/// Record time of last touch, used to eliminate jitter
static touchpad_dev_t* touchpad_group[TOUCH_PAD_MAX];

static void touch_pad_handler(void *arg)
{
    uint32_t pad_intr = READ_PERI_REG(SENS_SAR_TOUCH_CTRL2_REG) & 0x3ff;
    int i = 0;
    touchpad_dev_t* touchpad_dev;
    uint32_t rtc_intr = READ_PERI_REG(RTC_CNTL_INT_ST_REG);
    WRITE_PERI_REG(RTC_CNTL_INT_CLR_REG, rtc_intr);
    SET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_MEAS_EN_CLR);
    if (rtc_intr & RTC_CNTL_TOUCH_INT_ST) {
        for (i = 0; i < TOUCH_PAD_MAX; ++i) {
            if ((pad_intr >> i) & 0x01) {
                touchpad_dev = touchpad_group[i];
                if (touchpad_dev != NULL) {
                    TickType_t tick = xTaskGetTickCount();
                    uint32_t tick_diff = tick - touchpad_dev->last_tick;
                    if (touchpad_dev->trig_mode == TOUCHPAD_SERIAL_TRIGGER) {
                        touchpad_dev->sum_tick += tick_diff;
                    }
                    touchpad_dev->last_tick = tick;
                    if (tick_diff > touchpad_dev->filter_value / portTICK_PERIOD_MS || touchpad_dev->sum_tick > touchpad_dev->filter_value / portTICK_PERIOD_MS) {
                        touchpad_dev->sum_tick = 0;
                        xQueueSendToBackFromISR(xQueue_touch_pad, &touchpad_dev, 0);
                    }
                }
            }
        }
    }
}

touchpad_handle_t touchpad_create(touch_pad_t touch_pad_num, uint32_t threshold, uint32_t filter_value, touchpad_trigger_t trigger)
{
    if (xQueue_touch_pad == NULL) {
        xQueue_touch_pad = xQueueCreate(TOUCHPAD_QUEUE_LEN, sizeof(touchpad_handle_t));
        touch_pad_init();
        touch_pad_isr_handler_register(touch_pad_handler, NULL, 0, NULL);
    }
    IOT_CHECK(TAG, touch_pad_num < TOUCH_PAD_MAX, NULL);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) calloc(1, sizeof(touchpad_dev_t));
    touchpad_dev->last_tick = 0;
    touchpad_dev->threshold = threshold;
    touchpad_dev->filter_value = filter_value;
    touchpad_dev->touch_pad_num = touch_pad_num;
    touchpad_dev->trig_mode = trigger;
    touchpad_dev->sum_tick = 0;
    touch_pad_config(touch_pad_num, threshold);
    touchpad_group[touch_pad_num] = touchpad_dev;
    return (touchpad_handle_t) touchpad_dev;
}

esp_err_t touchpad_delete(touchpad_handle_t touchpad_handle)
{
    POINT_ASSERT(TAG, touchpad_handle);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    free(touchpad_handle);
    touchpad_group[touchpad_dev->touch_pad_num] = NULL;
    return ESP_OK;
}

touch_pad_t touchpad_num_get(const touchpad_handle_t touchpad_handle)
{
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    return touchpad_dev->touch_pad_num;
}

esp_err_t touchpad_set_threshold(const touchpad_handle_t touchpad_handle, uint32_t threshold)
{
    POINT_ASSERT(TAG, touchpad_handle);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    ERR_ASSERT(TAG, touch_pad_config(touchpad_dev->touch_pad_num, threshold));
    touchpad_dev->threshold = threshold;
    return ESP_OK;
}

esp_err_t touchpad_set_filter(const touchpad_handle_t touchpad_handle, uint32_t filter_value)
{
    POINT_ASSERT(TAG, touchpad_handle);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    touchpad_dev->filter_value= filter_value;
    return ESP_OK;
}

esp_err_t touchpad_set_trigger(const touchpad_handle_t touchpad_handle, touchpad_trigger_t trigger)
{
    POINT_ASSERT(TAG, touchpad_handle);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    touchpad_dev->trig_mode = trigger;
    return ESP_OK;
}

esp_err_t touchpad_read(const touchpad_handle_t touchpad_handle, uint16_t *touch_value_ptr)
{
    POINT_ASSERT(TAG, touchpad_handle);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    return touch_pad_read(touchpad_dev->touch_pad_num, touch_value_ptr);
}