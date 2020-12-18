舵机
======

舵机在一些控制应用中十分常见，ESP32 可以通过 LEDC 外设产生 PWM 信号来控制舵机。
LEDC 有 ``low_speed`` 和 ``high_speed`` 两个模式，每个模式下有 8 个输出通道，最多可同时产生 16 通道的信号，控制频率可从 50Hz 到 400Hz，允许用户使用任何一个具有输出功能的引脚来输出信号。

应用示例
----------

.. code:: c

    servo_config_t servo_cfg = {
        .max_angle = 180,
        .min_width_us = 500,
        .max_width_us = 2500,
        .freq = 50,
        .timer_number = LEDC_TIMER_0,
        .channels = {
            .servo_pin = {
                SERVO_CH0_PIN,
                SERVO_CH1_PIN,
                SERVO_CH2_PIN,
                SERVO_CH3_PIN,
                SERVO_CH4_PIN,
                SERVO_CH5_PIN,
                SERVO_CH6_PIN,
                SERVO_CH7_PIN,
            },
            .ch = {
                LEDC_CHANNEL_0,
                LEDC_CHANNEL_1,
                LEDC_CHANNEL_2,
                LEDC_CHANNEL_3,
                LEDC_CHANNEL_4,
                LEDC_CHANNEL_5,
                LEDC_CHANNEL_6,
                LEDC_CHANNEL_7,
            },
        },
        .channel_number = 8,
    } ;
    iot_servo_init(LEDC_LOW_SPEED_MODE, &servo_cfg);

    float angle = 100.0f;

    // Set angle to 100 degree
    iot_servo_write_angle(LEDC_LOW_SPEED_MODE, 0, angle);
    
    // Get current angle of servo
    iot_servo_read_angle(LEDC_LOW_SPEED_MODE, 0, &angle);

    //deinit servo
    iot_servo_deinit(LEDC_LOW_SPEED_MODE);

API Reference
-------------

.. include:: /_build/inc/servo.inc
