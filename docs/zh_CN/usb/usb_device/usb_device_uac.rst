ESP Device UAC
================

:link_to_translation:`en:[English]`

``esp_device_uac`` 是基于 TinyUSB 的 USB Audio Class 驱动库，它支持将 ESP 芯片模拟成为一个音频设备，支持自定义音频采样率，麦克风通道数，扬声器通道数等。

特性：

1. 默认支持 ISO FeedBack 通信接口，根据 UAC FIFO 内存剩余大小自动向主机端同步， `参考 <https://github.com/hathach/tinyusb/pull/2328>`__。
2. 支持自定义音频采样率，麦克风通道数，扬声器通道数。
3. 支持扬声器数据到来时候先缓冲一段数据，再进行传输，有助于减少音频数据传输的中断频率。

USB Device UAC 用户指南
-------------------------

- 开发板

    1. 可以使用任何带有 USB 接口的 ESP32-S2/ESP32-S3/ESP32-P4 开发板

- USB MIC 回调函数

    1. ``uac_input_cb_t`` 回调函数用于将音频数据传输到 USB 主机端，用户应该按照时间轴传输音频，或者在该回调函数中堵塞的等待音频数据到来。
    2. 通过设置 ``CONFIG_UAC_MIC_INTERVAL_MS`` 宏定义来设置回调函数读取音频数据的长度。
        - 设置 ``CONFIG_UAC_MIC_INTERVAL_MS=10`` 在 48000HZ 采样率，16 位精度，单通道情况下，每次读取的数据量为 10ms * 48000HZ / 1000 * 2byte = 960byte
    3. 通过设置 ``UAC_SPK_INTERVAL_MS`` 宏定义来设置回调函数第一次写入音频的长度，为了防止音频数据传输的中断频率过高，默认为 10ms，后续音频写入会按照大约 1ms 的数据量进行传输。
    4. ``UAC_SPK_NEW_PLAY_INTERVAL`` 宏定义用于判断到来的音频是否是新音频，如果是新音频，会先缓冲一段数据，再进行传输，有助于减少音频数据传输的中断频率。
    5. ``UAC_SUPPORT_MACOS`` 宏用于支持 Macos 系统，注意开启该宏定义后，windows 系统可能无法识别设备。

USB Device UAC API 参考
--------------------------

1. 用户可通过调用 :cpp:type:`uac_device_config_t` 函数配置音频输入输出，静音，音量四个回调函数。

.. code:: c

    uac_device_config_t config = {
        .output_cb = uac_device_output_cb,          // Speaker output callback
        .input_cb = uac_device_input_cb,            // Microphone input callback
        .set_mute_cb = uac_device_set_mute_cb,      // Set mute callback
        .set_volume_cb = uac_device_set_volume_cb,  // Set volume callback
        .cb_ctx = NULL,
    };
    uac_device_init(&config);

Example
----------

- :example:`usb/device/usb_uac`

API Reference
----------------

.. include-build-file:: inc/usb_device_uac.inc
