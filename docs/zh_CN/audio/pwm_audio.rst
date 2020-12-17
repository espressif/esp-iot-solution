PWM 音频
==============

PWM 音频功能使用 ESP32 内部的 LEDC 外设产生 PWM 播放音频而无需使用外部的音频 codec 芯片，适用于对音质要求不高而对成本敏感的应用。


特性
------

 - 允许使用任意具有输出功能的 GPIO 作为音频输出管脚
 - 支持 8 ~ 10 位的 PWM 分辨率
 - 支持立体声
 - 支持 8KHz ~ 48KHz 采样率

结构
-----

.. figure:: ../../_static/audio/pwm_audio_diagram.png
   :align: center

1. 写入的数据首先会进行编码的调整以满足 PWM 的输入要求，包括数据的移位和偏移
2. 通过一个 Ring-buffer 将数据发送到 Timer Group 的 ISR(Interrupt Service Routines) 函数
3. Timer Group 按照设定的采样率将数据从 Ring-buffer 读出并写入到 LEDC

.. note:: 由于输出的是 PWM 信号，需要将输出进行低通滤波后才能得到音频波形。

PWM 频率
---------

输出 PWM 的频率是不可直接配置的，它是通过配置的 PWM 分辨率位数来计算得到的，计算公式如下：

.. math::

    f_{pwm}=\frac{f_{APB\_CLK}}{2^{res\_bits}}-\left (\frac{f_{APB\_CLK}}{2^{res\_bits}} MOD 1000\right ) 

其中 :math:`f_{APB\_CLK}` 为 80MHz，:math:`res\_bits` 为 PWM 分辨率位数，当分辨率为 :cpp:enumerator:`LEDC_TIMER_10_BIT` 时，频率为 78KHz。
更高的 PWM 频率和分辨率将更好的还原音频信号，但是从公式可以看出，增大 PWM 频率意味着更低的分辨率，提高分辨率意味着更低的频率，这需要根据实际应用调整得到一个良好的平衡。

应用示例
---------

 .. code::

    pwm_audio_config_t pac;
    pac.duty_resolution    = LEDC_TIMER_10_BIT;
    pac.gpio_num_left      = LEFT_CHANNEL_GPIO;
    pac.ledc_channel_left  = LEDC_CHANNEL_0;
    pac.gpio_num_right     = RIGHT_CHANNEL_GPIO;
    pac.ledc_channel_right = LEDC_CHANNEL_1;
    pac.ledc_timer_sel     = LEDC_TIMER_0;
    pac.tg_num             = TIMER_GROUP_0;
    pac.timer_num          = TIMER_0;
    pac.ringbuf_len        = 1024 * 8;

    pwm_audio_init(&pac));             /**< Initialize pwm audio */
    pwm_audio_set_param(48000, 8, 2);  /**< Set sample rate, bits and channel numner */
    pwm_audio_start();                 /**< Start to run */

    while(1) {

        //< Perpare audio data, such as decode mp3/wav file

        /**< Write data to paly */
        pwm_audio_write(audio_data, length, &written, 1000 / portTICK_PERIOD_MS);
    }

API Reference
-------------

.. include:: /_build/inc/pwm_audio.inc