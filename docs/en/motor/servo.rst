Servo
=======
:link_to_translation:`zh_CN:[中文]`

This component uses the LEDC peripheral to generate PWM signals for independent control of multiple servos through servo handles. The maximum number of servos is limited by the number of LEDC channels on the chip (ESP32 chips support 16 channels and ESP32-S2 chips support 8 channels) at a selectable frequency of 50 ~ 400 Hz. When using this layer of APIs, users only need to create a servo handle and specify the target angle to control a servo.

Generally, there is a reference signal inside the servo generating a fixed period and pulse width, which is used to compare with the input PWM signal to output a voltage difference so as to control the rotation direction and angle of a motor. A common 180 angular rotation servo usually takes 20 ms (50 Hz) as a clock period and 0.5 ~ 2.5 ms as its high level pulse, making it rotates between 0 ~ 180 degrees.

This component can be used in scenarios with lower control accuracy requirements, such as toy cars, remote control robots, home automation, etc.


Instructions
------------------

1. Initialization: Use :cpp:func:`iot_servo_new` to create a servo handle. Please note that ESP32 contains two sets of channels as ``LEDC_LOW_SPEED_MODE`` and ``LEDC_HIGH_SPEED_MODE``, while some chips may only support one group. The configuration items in this step mainly include maximum angle, signal frequency, and minimum and maximum input pulse width to calculate the correspondence between angle and duty cycle; as well as GPIO, LEDC timer, and LEDC channel to specify PWM output resources;
2. Set a target angle: use :cpp:func:`iot_servo_write_angle` to specify the servo handle and target angle so as to realize angle control of the servo;
3. Read the current angle: you can use :cpp:func:`iot_servo_read_angle` to read the current angle of the servo. Please note that this is a theoretical number calculated based on the input signal;
4. De-initialization: you can use :cpp:func:`iot_servo_del` to delete a servo handle when the servo is no longer used.

Each servo handle owns one LEDC channel, and a channel cannot be used by more than one servo handle at the same time. Multiple servo handles can share the same LEDC timer only when they use the same speed mode, frequency, and duty resolution. Use different LEDC timers for servos that require different PWM frequencies.

Application Example
-------------------------------

.. code:: c

    servo_handle_t servos[2] = { NULL };
    servo_config_t servo_cfg = SERVO_CONFIG_DEFAULT(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, LEDC_CHANNEL_0, SERVO_CH0_PIN);
    iot_servo_new(&servo_cfg, &servos[0]);

    servo_cfg.channel = LEDC_CHANNEL_1;
    servo_cfg.gpio_num = SERVO_CH1_PIN;
    iot_servo_new(&servo_cfg, &servos[1]);

    float angle = 100.0f;

    // Set angle to 100 degree
    iot_servo_write_angle(servos[0], angle);
    
    // Get current angle of servo
    iot_servo_read_angle(servos[0], &angle);

    //deinit servo
    iot_servo_del(servos[0]);
    iot_servo_del(servos[1]);

API Reference
--------------------

.. include-build-file:: inc/iot_servo.inc
