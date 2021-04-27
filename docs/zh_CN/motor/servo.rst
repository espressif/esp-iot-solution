舵机
======
:link_to_translation:`en:[English]`

该组件使用 LEDC 外设产生 PWM 信号，可以实现对最多 16 路舵机的独立控制（ESP32 支持 16 路通道，ESP32-S2 支持 8 路通道），控制频率可选择为 50 ~ 400 Hz。使用该层 API，用户只需要指定舵机所在组、通道和目标角度，即可实现对舵机的角度操作。

舵机内部一般存在一个产生固定周期和脉宽的基准信号，通过与输入 PWM 信号进行比较，获得电压差输出，进而控制电机的转动方向和转动角度。常见的 180 度角旋转舵机一般以 20 ms (50 Hz) 为时钟周期，通过 0.5 ~ 2.5 ms 高电平脉冲，对应控制舵机在 0 ~ 180 度之间转动。

该组件可用于对控制精度要求较低的场景，例如玩具小车、遥控机器人、家庭自动化等。


使用方法
---------

1. 舵机初始化：使用 :cpp:func:`servo_init` 对一组通道进行初始化，ESP32 包含 ``LEDC_LOW_SPEED_MODE`` 和 ``LEDC_HIGH_SPEED_MODE`` 两组通道，有些芯片可能只支持一组。初始化配置项主要包括最大角度、信号频率、最小输入脉宽和最大输入脉宽，用于计算角度和占空比的对应关系；引脚和通道用于分别指定芯片引脚和 LEDC 通道的对应关系；
2. 设置目标角度：使用 :cpp:func:`servo_write_angle` 通过指定舵机所在组、所在通道和目标角度，对舵机角度进行控制；
3. 读取当前角度：可使用 :cpp:func:`servo_read_angle` 获取舵机当前角度，需要注意的是该角度是根据输入信号进行推算的理论角度；
4. 舵机去初始化：当一组多个舵机都不再使用时，可使用 :cpp:func:`servo_deinit` 对一组通道进行去初始化。

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

API 参考
-------------

.. include:: /_build/inc/iot_servo.inc
