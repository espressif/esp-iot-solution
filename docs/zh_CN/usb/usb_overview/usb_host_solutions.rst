USB Host 方案
----------------

:link_to_translation:`en:[English]`

ESP32-S2/S3/P4 等芯片内置 USB-OTG 外设，支持 USB 主机模式，基于 ESP-IDF 提供的 USB 主机协议栈和各种 USB 主机类驱动，可以通过 USB 接口连接多种多样的 USB 设备。以下介绍了 ESP32-S2/S3/P4 芯片支持的 USB Host 解决方案。

ESP USB Camera 视频方案
^^^^^^^^^^^^^^^^^^^^^^^^^^

支持通过 USB 接口连接摄像头模组，实现 MJPEG 格式视频流获取和传输，最高可支持 480*800@15fps（Full-Speed，ESP32-S2/S3） 和 1080*1920@60fps（High-Speed，ESP32-P4）。适用于猫眼门铃、智能门锁、内窥镜、倒车影像等场景。

特性:
~~~~~~~


* 快速启动
* 支持热插拔
* 支持 UVC1.1/1.5 规范的摄像头
* 支持自动解析描述符
* 支持动态配置分辨率
* 支持 MJPEG 视频流传输
* 支持批量和同步两种传输模式

硬件:
~~~~~~~~


* 芯片： ESP32-S2，ESP32-S3
* 外设：USB-OTG
* USB 摄像头：支持 MJPEG 格式，批量传输模式下 800*480@15fps，同步传输模式下 480*320@15fps，摄像头限制详见 `usb_stream API 说明 <https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_host/usb_stream.html>`_

对于 ESP32-P4，请使用 `usb_host_uvc` 组件，支持 MJPEG、YUY2、H264 与 H265 格式，

链接:
~~~~~~~~

* `usb_stream 组件 <https://components.espressif.com/components/espressif/usb_stream>`_
* `usb_host_uvc 组件 <https://components.espressif.com/components/espressif/usb_host_uvc>`_
* `usb_stream API 说明 <https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_host/usb_stream.html>`_
* `USB Camera Demo 视频 <https://www.bilibili.com/video/BV18841137qT>`_
* 示例代码: USB 摄像头 + WiFi 图传 :example:`usb/host/usb_camera_mic_spk`
* 示例代码: USB 摄像头 + LCD 本地刷屏 :example:`usb/host/usb_camera_lcd_display`


ESP USB Audio 音频方案
^^^^^^^^^^^^^^^^^^^^^^^^^

支持通过 USB 接口连接 USB 音频设备，实现 PCM 格式音频流获取和传输，可同时支持多路 48KHz 16bit 扬声器和多路 48KHz 16bit 马克风。支持 Type-C 接口耳机，适用于音频播放器等场景。支持和 UVC 同时工作，适用于门铃对讲等场景。

特性:
~~~~~~~


* 快速启动
* 支持热插拔
* 支持自动解析描述符
* 支持 PCM 音频流传输
* 支持动态修改采样率
* 支持多通道扬声器
* 支持多通道麦克风
* 支持音量、静音控制
* 支持和 USB Camera 同时工作

硬件:
~~~~~~~~

* 芯片： ESP32-S2，ESP32-S3，ESP32-P4
* 外设：USB-OTG
* USB 音频设备：支持 PCM 格式

链接:
~~~~~~~~

* `usb_stream 组件 <https://components.espressif.com/components/espressif/usb_stream>`_
* `usb_host_uac 组件 <https://components.espressif.com/components/espressif/usb_host_uac>`_
* `usb_stream API 说明 <https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_host/usb_stream.html>`_
* `USB Audio Demo 视频 <https://www.bilibili.com/video/BV1LP411975W>`_
* 示例代码: MP3 音乐播放器 + USB 耳机 :example:`usb/host/usb_audio_player`

ESP USB 4G 联网方案
^^^^^^^^^^^^^^^^^^^^^^

支持通过 USB 接口连接 4G Cat.1，Cat.4 模组，实现 PPP 拨号上网。支持通过 Wi-Fi SoftAP 热点共享互联网给其它设备。适用于物联网网关、MiFi 移动热点、智慧储能、广告灯箱等场景。

特性:
~~~~~~~

* 快速启动
* 支持热插拔
* 支持 Modem+AT 双接口（需要模组支持）
* 支持 PPP 标准协议 （大部分 4G 模组均支持）
* 支持 4G 转 Wi-Fi 热点
* 支持 NAPT 网络地址转换
* 支持电源管理
* 支持网络自动恢复
* 支持卡检测、信号质量检测
* 支持网页配置界面

硬件:
~~~~~~~~

* 芯片： ESP32-S2，ESP32-S3，ESP32-P4
* 外设：USB-OTG
* 4G 模组：支持 Cat.1 Cat.4 等网络制式 4G 模组，需要模组支持 PPP/ECM/RNDIS 协议

链接:
~~~~~~~~

* `USB 4G Demo 视频 <https://www.bilibili.com/video/BV1fj411K7bW>`_
* `iot_usbh_modem 组件 <https://components.espressif.com/components/espressif/iot_usbh_modem>`_
* `iot_usbh_ecm 组件 <https://components.espressif.com/components/espressif/iot_usbh_ecm>`_
* `iot_usbh_rndis 组件 <https://components.espressif.com/components/espressif/iot_usbh_rndis>`_
* 示例代码: 4G PPP 拨号上网 :example:`usb/host/usb_cdc_4g_module`
* 示例代码: 4G RNDIS 拨号上网 :example:`usb/host/usb_rndis_4g_module`
* 示例代码: 4G ECM 拨号上网 :example:`usb/host/usb_ecm_4g_module`

ESP USB 存储方案
^^^^^^^^^^^^^^^^^^

支持通过 USB 接口连接标准 U 盘设备（兼容 USB3.1/3.0/2.0 协议 U 盘），支持将 U 盘挂载到 FatFS 文件系统，实现文件的读写。适用于户外广告灯牌、考勤机、移动音响、记录仪等应用场景。

特性:
~~~~~~~

* 兼容 USB3.1/3.0/2.0 U 盘
* 默认支持最大 32G
* 支持热插拔
* 支持 Fat32/exFat 格式
* 支持文件系统读写
* 支持 U 盘 OTA

硬件:
~~~~~~~~

* 芯片： ESP32-S2，ESP32-S3，ESP32-P4
* 外设：USB-OTG
* U 盘：格式化为 Fat32 格式，默认支持 32GB 以内 U 盘。大于 32GB 需要在文件系统开启 exFat

链接:
~~~~~~~~

* `usb_host_msc 组件 <https://components.espressif.com/components/espressif/usb_host_msc>`_
* `U 盘 OTA 组件 <https://github.com/espressif/esp-iot-solution/tree/master/components/usb/esp_msc_ota>`_
* `挂载 U 盘 + 文件系统访问示例 <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/msc>`_

ESP USB Hub 方案
^^^^^^^^^^^^^^^^^^^

支持通过 USB Hub 连接多个 USB 设备，实现多设备同时工作。适用于多 USB 设备协同工作，例如双摄像头视频采集、音视频同步处理、外设扩展与数据存储等。

特性:
~~~~~~~~~

* 支持通过 USB Hub 连接多个 USB 设备
* 支持热插拔

硬件：
~~~~~~~~~

* 芯片：ESP32-S2，ESP32-S3，ESP32-P4
* 外设：USB-OTG

链接:
~~~~~~~~~~

* `USB Hub 双摄 Demo <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_hub_dual_camera>`_
