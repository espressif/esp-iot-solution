TinyUSB 应用指南
==================

:link_to_translation:`en:[English]`

本指南包含以下内容：

.. contents:: 目录
    :local:
    :depth: 2

TinyUSB 简介
----------------

TinyUSB 是一个开源的嵌入式 USB 主机/设备栈库，主要用于支持小型微控制器上的 USB 功能。它由 Adafruit 开发和维护，旨在提供一个轻量级、跨平台、易于集成的 USB 协议栈。TinyUSB 支持多种 USB 设备类型，包括 HID（人机接口设备）、MSC（大容量存储）、CDC（通信设备类）、MIDI（音乐设备数字接口）等，适用于各种嵌入式系统和物联网设备。基于原生的 TinyUSB 封装了以下的组件。

.. figure:: https://dl.espressif.com/AE/esp-iot-solution/tinyusb_components.png
    :align: center
    :alt: TinyUSB Components

    TinyUSB Components

芯片选型
~~~~~~~~~~

.. list-table::
    :widths: 10 30 30
    :header-rows: 1

    * - Soc
      - USB1.1 Full Speed
      - USB2.0 High Speed
    * - ESP32-S2
      - |supported|
      -
    * - ESP32-S3
      - |supported|
      -
    * - ESP32-P4
      - |supported|
      - |supported|

.. |supported| image:: https://img.shields.io/badge/-Supported-green

TinyUSB 组件
-------------------

esp_tinyusb 组件
~~~~~~~~~~~~~~~~~~

`esp_tinyusb <https://components.espressif.com/components/espressif/esp_tinyusb>`_ 组件封装了一系列的 TinyUSB API，可以方便地集成 USB CDC-ACM，MSC， MIDI， HID， DFU， ECM/NCM/RNDIS 类在自己的工程中。

如何使用

    * 在工程目录下运行 ``idf.py add-dependency esp_tinyusb~1.0.0`` 添加对 ``esp_tinyusb`` 库的依赖。
    * 在 ``menuconfig`` 中配置需要使用的 USB 类。
    * 在工程中使用 esp_tinyusb API。

`esp_tinyusb 应用示例 <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device>`_ 提供了 U盘，串口，HID 设备，复合设备等示例

    - `USB 串口和 U 盘 复合设备示例 <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_composite_msc_serialdevice>`_
    - `USB 串口设备示例 <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_serial_device>`_
    - `通过 USB 输出 LOG 示例 <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_serial_device>`_
    - `USB HID 设备示例 <https://github.com/espressif/esp-idf/blob/master/examples/peripherals/usb/device/tusb_hid/README.md>`_
    - `USB MIDI 音乐设备示例 <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_midi>`_
    - `USB U 盘设备示例 <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_msc>`_
    - `USB 网卡设备示例 <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_ncm>`_

.. note::
    c 库封装了许多 USB 类，使得开发一些被 esp_tinyusb 支持的 USB 类非常容易，值得注意的是，这也使得进行某些改动很困难。适合于简单的 USB 应用。

.. _espressif/tinyusb:

espressif/tinyusb 组件
~~~~~~~~~~~~~~~~~~~~~~~~~~

`组件 espressif/tinyusb <https://components.espressif.com/components/espressif/tinyusb>`_ 是基于原生 `tinyusb <https://github.com/hathach/tinyusb>`_ 仓库的一个组件，主要是将 tinyusb 仓库中的代码封装成一个组件，方便用户在自己的工程中使用。

.. note::
    该组件需要使用 ESP-IDF release/v5.0 及以上的版本。

如何使用:

    * 在工程目录下运行 ``idf.py add-dependency "tinyusb~0.15.10"`` 添加对 ``espressif/tinyusb`` 库的依赖。
    * 编写 ``tusb_config.h`` 文件，该文件通过定义一系列宏来决定 TinyUSB 的配置，并反向提供给 espressif/tinyusb 组件，同时在 main 组件的 CMakeLists.txt 添加以下代码：

        .. code:: C

            idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
            target_include_directories(${tusb_lib} PRIVATE path_to_your_tusb_config)

    * 编写 ``usb_descriptor.h`` 文件，该文件定义了 USB 设备的描述符，包括设备描述符，配置描述符，接口描述符等。
    * 编写 ``usb_descriptors.c`` 文件，该文件用于给 tinyusb 提供 USB 描述符回调`

espressif/tinyusb 应用示例：

    - `USB 键盘鼠标设备 示例 <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_hid_device>`_
    - `USB 无线 U 盘 示例 <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_msc_wireless_disk>`_
    - `Windows Surface Dial HID 示例 <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_surface_dial>`_
    - `串口转 USB 示例 <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_uart_bridge>`_

.. note::
    espressif/tinyusb 库提供了更多的灵活性，可以更加方便的定制 USB 设备，适合于复杂的 USB 应用。在 idf release/v4.4 上可以使用 `组件 leeebo/tinyusb_src <https://components.espressif.com/components/leeebo/tinyusb_src>`_，该组件与 espressif/tinyusb 作用相同。主要是补全了 espressif/tinyusb 对 ESP-IDF release/v4.4 的支持。

USB Device 介绍
------------------------

USB 音频（UAC）
~~~~~~~~~~~~~~~~~

TinyUSB 支持 USB `UAC2.0 <http://dl.project-voodoo.org/usb-audio-spec/USB%20Audio%20v2.0/Audio20%20final.pdf>`_ 标准，用于通过 USB 传输音频数据。主要有以下特点。

- 最高支持 32位/384kHZ 的音频流
- 兼容 USB1.1 Full Speed 和 USB2.0 High Speed
- 延迟更低

传输方式：
^^^^^^^^^^^

UAC 仅支持 USB 传输中的同步传输，所以 UAC 音频设备的数据端点都是同步端点。因为同步传输不进行重传，并且低延迟。同时因为主机和从机之间的传输是不同步的，可能会产生短暂静音/爆音，由此产生了三种同步方式。

- SYNC 同步
    将输出时钟于每个 Frame 的 SOF 包同步

- 自适应
    根据主机传输数据的速率调整输出的采样率

- ASYNC 异步
    相比另外两种多了反馈端口，从机通过主机当前的速率来告知主机后续的发送速率，从而完成数据的补发或少发。从而不需要再适应主机的发送频率。

关于 ASYNC 异步传输的反馈端点
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

通过启用宏 ``CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP`` 来实现反馈速率的计算，TinyUSB 提供了多种反馈的的数据计算，其中基于 FIFO 的反馈计算(``AUDIO_FEEDBACK_METHOD_FIFO_COUNT``)最为简单且实用。需要实现下面的虚函数，完成设置。

.. code:: C

    void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t* feedback_param)
    {
        (void)func_id;
        (void)alt_itf;
        // Set feedback method to fifo counting
        feedback_param->method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
        feedback_param->sample_freq = s_uac_device->current_sample_rate;

        ESP_LOGD(TAG, "Feedback method: %d, sample freq: %d", feedback_param->method, feedback_param->sample_freq);
    }

工作原理是，UAC Class 内部维护了一块软件 FIFO 大小为 ``CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ``， 通过将此 内存大小设置为 10ms 的数据大小，让 UAC 驱动内部有一块缓冲区，驱动会通过反馈端点将 FIFO 的水位维持在二分之一大小，当数据缺失，主机会一包中多发数据，当数据缺少，主机会少发数据。

应用上建议，首先在每一次新音频传输的时候（例大于 100ms 没有数据到来，则认为是一个新音频）先让 UAC 内部 fifo 缓冲一半缓冲区大小的数据，再开始播放，这样可以保证 I2S 一直有数据可以取，不会产生爆破音和噪声。同时基于反馈端点，软件 FIFO 的大小会一直维持在一个稳定的水平。

具体可参考 :doc:`/usb/usb_device/usb_device_uac`

USB 视频（UVC）
~~~~~~~~~~~~~~~~~

TinyUSB 支持 USB `UVC1.5 <https://www.usb.org/sites/default/files/USB_Video_Class_1_5.zip>`_ 标准，用于通过 USB 传输视频数据，能够传输多种视频格式，包括未压缩格式 YUV 格式，压缩格式 MJPEG，H264，H265 等

传输方式：
^^^^^^^^^^^

- 当视频流接口（USB Video streaming）传输视频的时候，其传输端点为同步传输或者批量传输端点。
- 当视频流接口传输静态图像的时候，其传输类型为批量传输端点。

传输图像：
^^^^^^^^^^^^^^^^

UVC 能够传输多种视频格式，这些图像格式是通过视频描述符的 Format 和 Frame 来定义的。

.. list-table::
    :widths: 10 30 30
    :header-rows: 1

    * - 图像类型
      - Format
      - Frame
    * - MJPEG
      - FORMAT_MJPEG：0x06
      - FRAME_MJPEG：0x07
    * - YUV2/NV12/M420/I420
      - FORMAT_UNCOMPRESSED：0x04
      - FRAME_UNCOMPRESSED：0x05
    * - H264
      - FORMAT_H264：0x013
      - FRAME_H264：0x014
    * - H265
      - FORMAT_FRAME_BASED：0x10
      - FRAME_FRAME_BASED：0x11

其中 Frame based 格式比较特殊，可以存储任意的图像格式，只要图像是按照帧来存储的。如 MJPG，H264，H265 等。通过 GUID 字段来表示具体的图像格式。

双摄摄像头
^^^^^^^^^^^^^

UVC 设备中，一个物理摄像头会有一个 VC （video control）描述符，而一个 VC 描述符可以有多个 VS （video streaming）描述符。表示这个摄像头可以传输多种格式图像。但是在一些特殊的硬件中，会有两个硬件摄像头，这时就需要有两个 VC 描述符。

.. code:: C

    USB Descriptor
        |
        |-- Video Control
        |   |-- Video Streaming
        |
        |-- Video Control
            |-- Video Streaming

具体可参考 :doc:`/usb/usb_device/usb_device_uvc`
