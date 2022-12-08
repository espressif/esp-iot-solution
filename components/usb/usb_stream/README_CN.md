## USB Stream 组件说明

``usb_stream`` 是基于 ESP32-S2/ESP32-S3 的 USB UVC + UAC 主机驱动程序，支持从 USB 设备读取/写入/控制多媒体流。例如最多同时支持 1 路摄像头 + 1 路麦克风 + 1 路播放器数据流。

特性：

1. 支持通过 UVC Stream 接口获取视频流，支持批量和同步两种传输模式
2. 支持通过 UAC Stream 接口获取麦克风数据流，发送播放器数据流
3. 支持通过 UAC Control 接口控制麦克风音量、静音等特性
4. 支持对数据流暂停和恢复

### USB Stream 用户指南

请参考：https://docs.espressif.com/projects/espressif-esp-iot-solution/zh_CN/latest/usb/usb_stream.html

### Examples

[USB Camera + Audio stream](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_camera_mic_spk)