## UVC Stream 组件说明

`UVC Stream` 是基于 `UVC` 协议开发的 USB 摄像头驱动，用户可使用 ESP32-S2/ESP32-S3 作为 USB 主机，请求和并连续接收 USB 摄像头 `MJPEG` 图像帧。配合 `ESP-IOT-Solution` 图像解码或网络传输组件，可以实现屏幕显示或 IPC 等应用。

用户可通过简单的 API 接口控制视频流启动、暂停、重启、和停止操作。通过注册回调函数，可在得到完整图像帧时，对图像数据进行应用层处理。

该组件可支持批量传输或同步传输两种模式的摄像头。

### 开发环境准备

1. 搭建 ESP-IDF `master` 分支开发环境：[installation-step-by-step](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html#installation-step-by-step)
2. 搭建 `ESP-IOT-Solution` 环境：[Setup ESP-IOT-Solution Environment](../../../README.md)

### 硬件准备

* 开发板

  1. 本仓库已经适配的芯片为 `ESP32-S2` 、`ESP32-S3`
  2. 资源使用情况详见示例程序说明文件

* USB 摄像头

  1. 摄像头必须兼容 `USB1.1` 全速模式
  2. 摄像头需要自带 `MJPEG` 压缩
  3. 如使用同步传输摄像头，需要支持设置接口 `Max Packet Size` 为 `512`
  4. 同步传输模式图像数据流 USB 传输总带宽应小于 `4 Mbps` （500 KB/s）
  5. 批量传输模式图像数据流 USB 传输总带宽应小于 `8.8 Mbps` （1100 KB/s）
  6. 分辨率等要求详见示例程序说明文件

### UVC Stream API 使用说明

1. 用户需提前了解待适配摄像头`配置描述符`的详细参数，据此填写 `uvc_config_t` 的配置项，参数对应关系如下：

```
    uvc_config_t uvc_config = {
        .xfer_type = UVC_XFER_ISOC,  //设置摄像头模式为同步传输
        .dev_speed = USB_SPEED_FULL, //固定为 USB_SPEED_FULL
        .configuration = 1, //配置描述符编号，一般为 1
        .format_index = 1, // MJPEG 对应的 bFormatIndex, 例如为 1
        .frame_width = 320, // MJPEG 横向像素，例如 320
        .frame_height = 240, // MJPEG 纵向像素，例如 240
        .frame_index = 1, //MJPEG 320*240 对应的 bFrameIndex, 例如为 1
        .frame_interval = 666666, //可选的帧率 dwFrameInterval，例如 15fps
        .interface = 1, // 可选的视频流接口 bInterfaceNumber，一般为 1
        .interface_alt = 1, // 接口选项 bAlternateSetting, 例如 1 对应端点 MPS 最大支持 512
        .isoc_ep_addr = 0x81, // 接口选项对应的 bEndpointAddress, 例如为 0x81
        .isoc_ep_mps = 512, // 接口选项的确定的 MPS， 例如为 512
        .xfer_buffer_size = 32*1024, //单帧图像大小，需要根据实际测试确定，320*240 一般小于 35KB
        .xfer_buffer_a = pointer_buffer_a, // 已经申请的 buffer 指针
        .xfer_buffer_b = pointer_buffer_b, // 已经申请的 buffer 指针
        .frame_buffer_size = 32*1024, //单帧图像大小，需要根据实际测试确定
        .frame_buffer = pointer_frame_buffer, // 已经申请的 buffer 指针
    };
```

2. 使用 `uvc_streaming_config` 将第 1 步确定的 `uvc_config_t` 参数传入驱动；
3. 使用 `uvc_streaming_start` 以第 2 步传入的参数开启视频流，如果参数通过协商，摄像头将持续输出数据流。该驱动将在检测到完整的图像帧时，调用用户的回调函数对图像进行解码、刷屏、传输等操作；
4. 如果第 3 步执行出错，请降低分辨率或帧率目标，回到第 1 步修改参数；
5. 使用 `uvc_streaming_suspend` 可暂停摄像头视频流；
6. 使用 `uvc_streaming_resume` 可重启摄像头视频流；
7. 使用 `uvc_streaming_stop` 停止视频流，USB 资源将被完全释放。

### USB 摄像头示例程序

1. [USB Camera + Wi-Fi 图传](../../../examples/usb/host/usb_camera_wifi_transfer)
2. [USB Camera + LCD 本地显示](../../../examples/usb/host/usb_camera_lcd_display)
3. [USB Camera + SD 卡存储](../../../examples/usb/host/usb_camera_sd_card)
