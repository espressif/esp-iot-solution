USB Device 方案
------------------

:link_to_translation:`en:[English]`

ESP32-S2/S3/P4 等芯片内置 USB-OTG 外设，支持 USB 设备模式，可以通过 USB 连接到 PC 或者其他 USB 主机设备。结合设备 TinyUSB 协议栈和设备类驱动，可用于开发多种 USB 设备，如 HID 设备、CDC 设备、复合设备等。以下介绍基于 USB 设备的相关解决方案。

USB 音频设备方案
^^^^^^^^^^^^^^^^^

USB 音频设备方案基于 UAC 2.0 （USB Audio Class）协议标准 ，使乐鑫 SoC 可作为音频设备，提供便捷和高质量的音频传输功能，例如将其作为麦克风或扬声器，连接到计算机等支持 USB 音频的设备上，实现音频的输入和输出。

特性：
~~~~~~

* 支持 UAC 2.0
* 支持多种音频格式和采样率
* 支持音频输入和输出

硬件：
~~~~~~

* 芯片：ESP32-S2，ESP32-S3，ESP32-P4
* 外设：USB-OTG

链接：
~~~~~~

* `ESP-BOX 带屏 USB 音响 <https://github.com/espressif/esp-box/tree/master/examples/usb_headset>`_

USB 视频设备方案
^^^^^^^^^^^^^^^^^

USB UVC 设备方案基于 UVC（USB Video Class）协议标准，使乐鑫 SoC 可作为视频设备，提供便捷和高质量的视频传输功能，可应用在 USB 门铃摄像头，或 USB + Wi-Fi 双模网络摄像头。

特性：
~~~~~~

* 支持 UVC 1.5
* 支持同步和批量两种传输模式
* 支持作为虚拟摄像头设备

硬件：
~~~~~~

* 芯片：ESP32-S2，ESP32-S3，ESP32-P4
* 外设：USB-OTG

链接：
~~~~~~

* `USB 网络摄像头 <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_webcam>`_

USB 存储设备方案
^^^^^^^^^^^^^^^^^^

USB 存储设备方案基于 MSC（Mass Storage Class）协议标准，结合 Wi-Fi 功能，还可构建无线共享存储设备，如 USB 无线 U 盘、读卡器、数字音乐播放器、数字多媒体播放器等。

特性：
~~~~~~

* USB - Wi-Fi 双向访问
* 多设备接入
* 模拟 U 盘

硬件：
~~~~~~

* 芯片：ESP32-S2，ESP32-S3，ESP32-P4
* 外设：USB-OTG

链接：
~~~~~~

* `USB + Wi-Fi 无线 U 盘 <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_msc_wireless_disk>`_

USB HID 设备方案
^^^^^^^^^^^^^^^^^^

USB HID 设备方案基于 HID（Human Interface Device）协议标准，可作为 USB 键盘、鼠标、游戏手柄等设备，实现人机交互功能。结合 Wi-Fi，蓝牙，ESP-Now 等无线功能，还可构建无线 HID 设备。

特性：
~~~~~~

* 支持多种 HID 设备
* 支持自定义 HID 设备
* 支持 USB HID 和 BLE HID 双模

硬件：
~~~~~~

* 芯片：ESP32-S2，ESP32-S3，ESP32-P4
* 外设：USB-OTG

链接：
~~~~~~

* `USB HID 键盘和鼠标示例 <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_hid_device>`_
* `USB HID Surface Dial 示例 <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_surface_dial>`_
* `USB 客制化键盘示例 <https://github.com/espressif/esp-iot-solution/tree/master/examples/keyboard>`_

U 盘拖拽升级
^^^^^^^^^^^^^^^

基于 esp-tinyuf2 作为虚拟 U 盘，支持拖拽 UF2 固件到 U 盘实现 OTA。同时支持将 NVS 数据映射到 U 盘文件，通过修改文件改写 NVS。

特性:
~~~~~~

* 拖拽 UF2 固件进行 OTA
* 通过虚拟文件修改 NVS

硬件:
~~~~~~

* 芯片：ESP32-S2，ESP32-S3
* 外设：USB-OTG

链接：
~~~~~~

* `U 盘读写 NVS <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_uf2_nvs>`_
* `虚拟 U 盘 UF2 升级 <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_uf2_ota>`_

USB 扩展屏方案
^^^^^^^^^^^^^^^^^

USB 扩展屏方案通过 USB 总线将设备作为一块扩展副屏使用。支持通过一根 USB 数据线传输音频，触摸信息，视频图像等数据。可以应用在电脑屏幕，签字屏，扩展屏等应用场景。

上位机驱动
~~~~~~~~~~~~

USB2.0 驱动不支持 HDMI 传输，因此需要上位机将图像数据传输给设备，本应用方案暂时仅支持 Windows 平台，采用 Windows 驱动 `IDD <https://learn.microsoft.com/en-us/windows-hardware/drivers/display/indirect-display-driver-model-overview>`_，通过驱动可以拿到 Windows 的桌面图像，因为 USB 速率限制，在驱动中会先将图像处理成 JEPG 等格式，然后通过 USB vendor 接口将图像传输到设备。每一帧图像添加 16 字节的头，包含图像的宽高，图像的格式，图像的长度，图像的压缩类型等。

特性:
~~~~~~

* 支持通过 USB 传输图像
* 支持通过 USB 传输音频
* 支持通过 USB 传输触摸信息

硬件:
~~~~~~

* 芯片：ESP32-S2，ESP32-S3，ESP32-P4
* 外设：USB-OTG

链接:
~~~~~~

* `P4 USB 扩展屏 <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_extend_screen>`_
