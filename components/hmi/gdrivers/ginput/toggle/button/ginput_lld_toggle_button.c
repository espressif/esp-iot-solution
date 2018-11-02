/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

/* uGFX Config Include */
#include "sdkconfig.h"
#include "esp_log.h"

/* RTOS Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* GPIO Include */
#include "driver/gpio.h"

#define BUTTON_QUEUE_LENGTH   10
#define BUTTON_WAITQUEUE_TIME 20 // millisecond

#define GPIO_BUTTON_1        CONFIG_UGFX_TOGGLE_BUTTON_GPIO1
#define GPIO_BUTTON_2        CONFIG_UGFX_TOGGLE_BUTTON_GPIO2
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_BUTTON_1) | (1ULL<<GPIO_BUTTON_2))

#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    gpio_num_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_toggle_init()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_LOW_LEVEL;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set,e.g.GPIO4/15
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    if (gpio_evt_queue == NULL) {
        //create a queue to handle gpio event from isr
        gpio_evt_queue = xQueueCreate(BUTTON_QUEUE_LENGTH, sizeof(gpio_num_t));
    }

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_BUTTON_1, gpio_isr_handler, (void *)GPIO_BUTTON_1);
    gpio_isr_handler_add(GPIO_BUTTON_2, gpio_isr_handler, (void *)GPIO_BUTTON_2);
}

static unsigned get_gpio_num()
{
    gpio_num_t gpio_num;
    if (xQueueReceive(gpio_evt_queue, &gpio_num, BUTTON_WAITQUEUE_TIME / portTICK_PERIOD_MS) == pdTRUE) {
        return gpio_num;
    } else {
        return GPIO_NUM_MAX;
    }
}

#ifdef CONFIG_UGFX_GUI_ENABLE

/* uGFX Includes */
#include "gos_freertos_priv.h"
#include "gfx.h"

#if (GFX_USE_GINPUT && GINPUT_NEED_TOGGLE) /*|| defined(__DOXYGEN__)*/

#include "src/ginput/ginput_driver_toggle.h"

GINPUT_TOGGLE_DECLARE_STRUCTURE();

void ginput_lld_toggle_init(const GToggleConfig *ptc)
{
    gpio_toggle_init();
}

unsigned ginput_lld_toggle_getbits(const GToggleConfig *ptc)
{
    static unsigned btn = 0;
    switch (get_gpio_num()) {
    case GPIO_BUTTON_1:
        btn = 1; // number 1 toggle device touch
        break;

    case GPIO_BUTTON_2:
        btn = 2; // number 2 toggle device touch
        break;

    case GPIO_NUM_MAX:
        btn = 0; // no toggle device touch
        break;

    default:
        btn = 0; // no toggle device touch
        break;
    }
    return btn;
}

#endif /* GFX_USE_GINPUT && GINPUT_NEED_TOGGLE */

#endif /* CONFIG_UGFX_GUI_ENABLE */

#ifdef CONFIG_LVGL_GUI_ENABLE

/* lvgl include */
#include "lvgl_indev_config.h"

/*Function pointer to read data. Return 'true' if there is still data to be read (buffered)*/
static bool ex_tp_read(lv_indev_data_t *data)
{
    /*Save the lastly pressed/released button's ID*/
    static uint32_t btn = 0;
    data->state = LV_INDEV_STATE_REL;

    switch (get_gpio_num()) {
    case GPIO_BUTTON_1:
        data->state = LV_INDEV_STATE_PR;
        btn = 0; // 0 means: number 1 toggle device button
        break;

    case GPIO_BUTTON_2:
        data->state = LV_INDEV_STATE_PR;
        btn = 1; // 1 means: number 2 toggle device button
        break;

    case GPIO_NUM_MAX:
        data->state = LV_INDEV_STATE_REL;
        break; // no toggle device button

    default:
        data->state = LV_INDEV_STATE_REL;
        break; // no toggle device button
    }
    // please be sure that your button driver every time return old (last clcked) value.
    data->btn = btn; /*0 ... number of buttons-1*/

    // Return 'true' if there is still data to be read (buffered)*/
    return false;
}

/* Input device interface，Initialize your input device */
lv_indev_drv_t lvgl_indev_init()
{
    gpio_toggle_init();

    lv_indev_drv_t indev_drv;      /*Descriptor of an input device driver*/
    lv_indev_drv_init(&indev_drv); /*Basic initialization*/

    indev_drv.type = LV_INDEV_TYPE_BUTTON;  /*The touchpad is button type device*/
    indev_drv.read = ex_tp_read;            /*Library ready your button via this function*/

    lv_indev_drv_register(&indev_drv); /*Finally register the driver*/
    return indev_drv;
}

/**
 * Example :
 * static lv_points_t points[] = {{20, 30},{ 50, 70},{100, 150}};    // Points for 3 buttons
 * lv_indev_set_button_points(button_indev,  points); 
 */
#endif /* CONFIG_LVGL_GUI_ENABLE */
