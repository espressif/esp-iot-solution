#pragma once

#if (defined CONFIG_TRIGGER_PIN) && (!defined CONFIG_LCD_INTERFACE_I2S)
#include <string.h>
#include <time.h>
#include "driver/gpio.h"
#define TRIGGER_XFER_PIN 10 //hcd level trigger
#define TRIGGER_URB_PIN 11 //URB level trigger
#define TRIGGER_NEW_FARAME 12 //frame level trigger
#define TRIGGER_DECODER_PIN 13 //user level trigger
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<TRIGGER_DECODER_PIN) | (1ULL<<TRIGGER_NEW_FARAME) | (1ULL<<TRIGGER_URB_PIN) | (1ULL<<TRIGGER_XFER_PIN))

#define TRIGGER_INIT() trigger_init() //call in app_main
#define TRIGGER_XFER_RUN() gpio_set_level(TRIGGER_XFER_PIN, 1)
#define TRIGGER_XFER_LEAVE() gpio_set_level(TRIGGER_XFER_PIN, 0)
#define TRIGGER_URB_ENQUEUE() gpio_set_level(TRIGGER_URB_PIN, 1)
#define TRIGGER_URB_DEQUEUE() gpio_set_level(TRIGGER_URB_PIN, 0)
#define TRIGGER_NEW_FRAME() toggle_pin2(TRIGGER_NEW_FARAME)
#define TRIGGER_PIPE_EVENT() toggle_pin(TRIGGER_XFER_PIN)
#define TRIGGER_DECODER_RUN() gpio_set_level(TRIGGER_DECODER_PIN, 1)
#define TRIGGER_DECODER_LEAVE() gpio_set_level(TRIGGER_DECODER_PIN, 0)

static inline void trigger_init(void) {
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    gpio_set_level(TRIGGER_XFER_PIN, 0);
    gpio_set_level(TRIGGER_URB_PIN, 0);
    gpio_set_level(TRIGGER_NEW_FARAME, 0);
    gpio_set_level(TRIGGER_DECODER_PIN, 0);
}

static inline void toggle_pin(int pin)
{
    static bool state = 0;
    state = !state;
    gpio_set_level(pin, state);
}

static inline void toggle_pin2(int pin)
{
    static bool state = 0;
    state = !state;
    gpio_set_level(pin, state);
}

#else
#define TRIGGER_INIT()
#define TRIGGER_XFER_RUN()
#define TRIGGER_XFER_LEAVE()
#define TRIGGER_URB_ENQUEUE()
#define TRIGGER_URB_DEQUEUE()
#define TRIGGER_NEW_FRAME()
#define TRIGGER_DECODER_RUN()
#define TRIGGER_DECODER_LEAVE()
#define TRIGGER_PIPE_EVENT() 
#endif

