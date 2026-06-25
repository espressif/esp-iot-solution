舵机
======
:link_to_translation:`en:[English]`

该组件使用 LEDC 外设产生 PWM 信号，可以通过独立的舵机句柄控制多路舵机，最大数量受芯片 LEDC 通道数量限制（ESP32 支持 16 路通道，ESP32-S2 支持 8 路通道），控制频率可选择为 50 ~ 400 Hz。使用该层 API，用户只需要创建舵机句柄并指定目标角度，即可实现对舵机的角度操作。

舵机内部一般存在一个产生固定周期和脉宽的基准信号，通过与输入 PWM 信号进行比较，获得电压差输出，进而控制电机的转动方向和转动角度。常见的 180 度角旋转舵机一般以 20 ms (50 Hz) 为时钟周期，通过 0.5 ~ 2.5 ms 高电平脉冲，对应控制舵机在 0 ~ 180 度之间转动。

该组件可用于对控制精度要求较低的场景，例如玩具小车、遥控机器人、家庭自动化等。


使用方法
---------

1. 舵机初始化：使用 :cpp:func:`iot_servo_new` 创建舵机句柄，ESP32 包含 ``LEDC_LOW_SPEED_MODE`` 和 ``LEDC_HIGH_SPEED_MODE`` 两组通道，有些芯片可能只支持一组。初始化配置项主要包括最大角度、信号频率、最小输入脉宽和最大输入脉宽，用于计算角度和占空比的对应关系；引脚、LEDC 定时器和通道用于指定 PWM 输出资源；
2. 设置目标角度：使用 :cpp:func:`iot_servo_write_angle` 通过指定舵机句柄和目标角度，对舵机角度进行控制；
3. 读取当前角度：可使用 :cpp:func:`iot_servo_read_angle` 获取舵机当前角度，需要注意的是该角度是根据输入信号进行推算的理论角度；
4. 舵机去初始化：当舵机不再使用时，可使用 :cpp:func:`iot_servo_del` 删除舵机句柄。

每个舵机句柄会占用一个 LEDC 通道，同一个通道不能同时被多个舵机句柄使用。多个舵机句柄可以共享同一个 LEDC 定时器，但必须使用相同的 speed mode、频率和占空比分辨率；如果舵机需要不同的 PWM 频率，应使用不同的 LEDC 定时器。

应用示例
----------

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

API 参考
-------------

.. include-build-file:: inc/iot_servo.inc
