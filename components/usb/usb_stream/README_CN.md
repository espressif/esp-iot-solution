[![Component Registry](https://components.espressif.com/components/espressif/usb_stream/badge.svg)](https://components.espressif.com/components/espressif/usb_stream)

## USB Stream 组件说明

``usb_stream`` 是基于 ESP32-S2/ESP32-S3 的 USB UVC + UAC 主机驱动程序，支持从 USB 设备读取/写入/控制多媒体流。例如最多同时支持 1 路摄像头 + 1 路麦克风 + 1 路播放器数据流。

特性：

1. 支持通过 UVC Stream 接口获取视频流，支持批量和同步两种传输模式
2. 支持通过 UAC Stream 接口获取麦克风数据流，发送播放器数据流
3. 支持通过 UAC Control 接口控制麦克风音量、静音等特性
4. 支持对数据流暂停和恢复

> Note: 对于 ESP-IDF V5.3.3 及以上的版本，若使能 UVC，请在 `Component config → USB-OTG → Hardware FIFO size biasing` 中使能 `Bias IN`

### USB Stream 用户指南

请参考：https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_host/usb_stream.html

### 添加组件到工程

请使用组件管理器指令 `add-dependency` 将 `usb_stream` 添加到项目的依赖项, 在 `CMake` 执行期间该组件将被自动下载到工程目录。

```
idf.py add-dependency "espressif/usb_stream=*"
```

### 示例程序

请使用组件管理器指令 `create-project-from-example` 在当前文件夹下载该组件的默认示例程序。

* USB Camera WIFI 图传
```
idf.py create-project-from-example "espressif/usb_stream=*:usb_camera_mic_spk"
```

* USB Camera 本地刷屏
```
idf.py create-project-from-example "espressif/usb_stream=*:usb_camera_lcd_display"
```

之后进入示例程序目录，可以进行下一步的编译和烧录。

> 您也可以从 esp-iot-solution 仓库直接下载示例程序: [USB Camera + Audio stream](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_camera_mic_spk), [USB Camera LCD Display](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_camera_lcd_display).


### 问答

Q1. 我在使用包管理器添加组件时遇到以下问题

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username
```

A1. 这是因为您使用了一个老的组件管理器版本, 请在 ESP-IDF 环境中运行 `pip install -U idf-component-manager` 更新组件。