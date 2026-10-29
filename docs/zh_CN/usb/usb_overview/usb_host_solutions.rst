USB Host 方案
----------------

:link_to_translation:`en:[English]`

ESP32 系列内置 USB-OTG 外设的芯片（见 :ref:`USB 外设支持情况 <usb_peripheral-section>`），支持 USB 主机模式，基于 ESP-IDF 提供的 USB 主机协议栈和各种 USB 主机类驱动，可依托该方案快速实现外设拓展、数据采集、音视频传输、网络拓展、存储读写等多样化功能，广泛适配智能家居、工业物联网、车载终端、智能安防、便携设备等多领域产品开发。以下为细分标准化 USB Host 解决方案详情。

ESP USB Camera 视频方案
^^^^^^^^^^^^^^^^^^^^^^^^^^

支持通过 USB 接口连接摄像头模组，实现获取多种格式的视频流，最高可支持 480*800 @15fps（Full-Speed） 和 3840*2160 @30fps（High-Speed）。适用于猫眼门铃、智能门锁、内窥镜、倒车影像等场景。

特性:
~~~~~~~


* 快速启动
* 支持热插拔
* 支持 UVC1.1/1.5 规范的摄像头
* 支持自动解析描述符
* 支持动态配置分辨率
* 支持 MJPEG/YUV/H.264 等视频流传输
* 支持批量和同步两种传输模式

硬件:
~~~~~~~~


* 芯片： ESP32-S2，ESP32-S3，ESP32-S31, ESP32-P4
* 外设：USB-OTG
* USB 摄像头：适配市面主流 USB 摄像头模组，Full-Speed 接口需匹配对应全速摄像头设备

链接:
~~~~~~~~

* `USB Camera Demo 视频 <https://www.bilibili.com/video/BV18841137qT>`_
* 示例代码: USB 摄像头 + WiFi 图传 :example:`usb/host/usb_hub_dual_camera`
* 示例代码: USB 摄像头 + LCD 本地刷屏 :example:`usb/host/usb_camera_lcd_display`
* `usb_host_uvc 组件 <https://components.espressif.com/components/espressif/usb_host_uvc>`_

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

* 芯片： ESP32-S2，ESP32-S3，ESP32-S31, ESP32-P4
* 外设：USB-OTG
* USB 音频设备：支持 PCM 格式

链接:
~~~~~~~~

* `USB Audio Demo 视频 <https://www.bilibili.com/video/BV1LP411975W>`_
* 示例代码: MP3 音乐播放器 + USB 耳机 :example:`usb/host/usb_audio_player`
* 示例代码: 网页演示麦克风 + 扬声器 + 摄像头 :example:`usb/host/usb_camera_mic_spk`
* `usb_host_uac 组件 <https://components.espressif.com/components/espressif/usb_host_uac>`_

ESP USB 4G 联网方案
^^^^^^^^^^^^^^^^^^^^^^

支持通过 USB 接口连接 4G / 5G 模组实现上网。支持通过 Wi-Fi SoftAP 热点共享互联网给其它设备。适用于物联网网关、MiFi 移动热点、智慧储能、广告灯箱等场景。

特性:
~~~~~~~

* 快速启动
* 支持热插拔
* 支持 Modem + AT 双接口（需要模组支持）
* 支持 PPP/ECM/RNDIS 标准协议
* 支持 4G 转 Wi-Fi 热点
* 支持 NAPT 网络地址转换
* 支持电源管理
* 支持卡检测、信号质量检测
* 支持网页配置界面

硬件:
~~~~~~~~

* 芯片： ESP32-S2，ESP32-S3，ESP32-S31, ESP32-P4
* 外设：USB-OTG
* 4G/5G 模组：支持通用模组，需要模组支持 PPP/ECM/RNDIS 协议

链接:
~~~~~~~~

* `USB 4G Demo 视频 <https://www.bilibili.com/video/BV1fj411K7bW>`_
* `iot_usbh_modem 组件 <https://components.espressif.com/components/espressif/iot_usbh_modem>`_ ，:doc:`文档 <../usb_host/usb_ppp>`
* `iot_usbh_ecm 组件 <https://components.espressif.com/components/espressif/iot_usbh_ecm>`_ ，:doc:`文档 <../usb_host/usb_ecm>`
* `iot_usbh_rndis 组件 <https://components.espressif.com/components/espressif/iot_usbh_rndis>`_ ，:doc:`文档 <../usb_host/usb_rndis>`
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

* 芯片： ESP32-S2，ESP32-S3，ESP32-S31, ESP32-P4
* 外设：USB-OTG
* U 盘：格式化为 Fat32 格式，默认支持 32GB 以内 U 盘。大于 32GB 需要在文件系统开启 exFat

链接:
~~~~~~~~

* `usb_host_msc 组件 <https://components.espressif.com/components/espressif/usb_host_msc>`_
* `U 盘 OTA 组件 <https://github.com/espressif/esp-iot-solution/tree/master/components/usb/esp_msc_ota>`_
* `挂载 U 盘 + 文件系统访问示例 <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/msc>`_


ESP USB HID 设备方案
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

支持通过 USB 接口连接符合 HID Class 的键盘、鼠标、条码扫描枪、游戏手柄、自定义 HID 外设等设备，实现输入报告接收、设备信息获取和 HID 类请求控制。适用于本地人机交互、工业控制输入、扫码采集、UI 指针/按键控制等场景。

特性:
~~~~~~~

* 支持 HID 设备自动枚举和热插拔事件通知
* 支持接收 HID Input Report，并以 RAW 数据形式交由应用解析
* 支持 Boot Protocol 键盘、鼠标报告结构和常用键值定义
* 支持获取 HID Report Descriptor、VID/PID、厂商、产品和序列号等设备信息
* 支持 HID 类请求，包括 Get/Set Report、Get/Set Idle、Get/Set Protocol
* 支持多 HID 接口设备，应用可按接口打开、启动、停止和关闭

硬件:
~~~~~~~~

* 芯片： ESP32-S2，ESP32-S3，ESP32-S31, ESP32-P4
* 外设：USB-OTG
* USB HID 设备：支持符合 HID Class 的 USB 全速或高速设备；非 Boot Protocol 设备需要应用根据 Report Descriptor 自行解析 RAW Report

链接:
~~~~~~~~

* `usb_host_hid 组件 <https://components.espressif.com/components/espressif/usb_host_hid>`_
* `USB HID Host 示例 <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/hid>`_

ESP USB Hub 方案
^^^^^^^^^^^^^^^^^^^

支持通过 USB Hub 连接多个 USB 设备，实现多设备同时工作。适用于多 USB 设备协同工作，例如双摄像头视频采集、音视频同步处理、外设扩展与数据存储等。

特性:
~~~~~~~~~

* 支持通过 USB Hub 连接多个 USB 设备
* 支持热插拔

硬件：
~~~~~~~~~

* 芯片：ESP32-S2，ESP32-S3，ESP32-S31, ESP32-P4
* 外设：USB-OTG

链接:
~~~~~~~~~~

* `USB Hub 双摄 Demo <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_hub_dual_camera>`_
