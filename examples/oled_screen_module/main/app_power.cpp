/* OLED screen Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include "esp_log.h"
#include "driver/gpio.h"

class CPowerCtrl
{
private:
    gpio_num_t io_num;
public:
    CPowerCtrl(gpio_num_t io);
    ~CPowerCtrl();

    void on();
    void off();
};

CPowerCtrl::CPowerCtrl(gpio_num_t io)
{
    io_num = io;
    gpio_config_t conf;
    conf.pin_bit_mask = (1ULL << io_num);
    conf.mode = GPIO_MODE_OUTPUT;
    conf.pull_up_en = (gpio_pullup_t) 0;
    conf.pull_down_en = (gpio_pulldown_t) 0;
    conf.intr_type = (gpio_int_type_t) 0;
    gpio_config(&conf);
}

void CPowerCtrl::on()
{
    gpio_set_level(io_num, 0);
}

void CPowerCtrl::off()
{
    gpio_set_level(io_num, 1);
}

