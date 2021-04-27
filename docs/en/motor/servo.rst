Servo
=======
:link_to_translation:`zh_CN:[中文]`

This component uses the LEDC peripheral to generate PWM signals for independent control of servos with up to 16 channels (ESP32 chips support 16 channels and ESP32-S2 chips support 8 channels) at a selectable frequency of 50 ~ 400 Hz. When using this layer of APIs, users only need to specify the servo group, channel and target angle to realize the angle control of a servo.

Generally, there is a reference signal inside the servo generating a fixed period and pulse width, which is used to compare with the input PWM signal to output a voltage difference so as to control the rotation direction and angle of a motor. A common 180 angular rotation servo usually takes 20 ms (50 Hz) as a clock period and 0.5 ~ 2.5 ms as its high level pulse, making it rotates between 0 ~ 180 degrees.

This component can be used in scenarios with lower control accuracy requirements, such as toy cars, remote control robots, home automation, etc.


Instructions
------------------

1. Initialization: Use :cpp:func:`servo_init` to initialize a channel. Please note that ESP32 contains two sets of channels as ``LEDC_LOW_SPEED_MODE`` and ``LEDC_HIGH_SPEED_MODE``, while some chip may only support one channel. The configuration items in this step mainly include maximum angle, signal frequency, and minimum and maximum input pulse width to calculate the correspondence between angle and duty cycle; as well as pins and channels to specify the correspondence with chip pins and LEDC channels, respectively;
2. Set a target angle: use :cpp:func:`servo_write_angle` to specify the servo group, channel and target angle so as to realize angle control of the servo;
3. Read the current angle: you can use :cpp:func:`servo_read_angle` to read the current angle of the servo. Please note that this is a theoretical number calculated based on the input signal;
4. De-initialization: you can use :cpp:func:`servo_deinit` to de-initialize a group of channels when a group of servos is used any more.

Application Example
-------------------------------

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
--------------------

.. include:: /_build/inc/iot_servo.inc
