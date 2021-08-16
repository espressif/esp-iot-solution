## USB Camera SD 卡存储 Demo 说明

该示例程序支持以下功能：

* 支持 USB Camera 数据流获取和解析
* 支持图像 SD 保存

> 该示例程序代码仅用于 ESP32-S2 USB Host UVC 功能测试与评估

### 硬件准备

* ESP32-S2 USB 摄像头选型需要满足的参数（2021.03）：

    1. 摄像头兼容 USB1.1 全速模式
    2. 摄像头自带 MJPEG 压缩
    3. 摄像头支持设置端点`wMaxPacketSize`为`512`
    4. MJPEG 支持 **320*240** 分辨率
    5. MJPEG 支持设置帧率到 15 帧/s

* ESP32-S2 USB 摄像头硬件接线：
    1. USB 摄像头 VBUS 请使用 5V 电源独立供电，亦可使用 IO 控制 VBUS 通断
    2. UBS 摄像头 D+ D- 数据线请按常规差分信号标准走线
    3. USB 摄像头 D+ (绿线) 接 ESP32-S2 GPIO20
    4. USB 摄像头 D- (白线) 接 ESP32-S2 GPIO19

* ESP32-S2 SD 卡硬件接线:
    1. MOSI_PIN GPIO38
    2. MISO_PIN GPIO40
    3. SCLK_PIN GPIO39
    4. CS_PIN   GPIO37

### 编译示例代码

示例代码可按以下直接编译烧写：

1. 确认 ESP-IDF 环境成功搭建，并按照说明文件添加补丁 [usb_support_workaround.patch](../../../usb/idf_usb_support_patch/readme.md)
2. 添加 ESP-IDF 环境变量，Linux 方法如下，其它平台请查阅 [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables)
    ```
    . $HOME/esp/esp-idf/export.sh
    ```
3. 添加 ESP-IOT-SOLUTION 环境变量，Linux 方法如下，其它平台请查阅 [readme](../../../../README_CN.md)
    ```
    export IOT_SOLUTION_PATH=$HOME/esp/esp-iot-solution
    ```
4. 根据摄像头配置描述符，[修改摄像头配置项](../../../../components/usb/uvc_stream/README.md)
5. 设置编译目标为 `esp32-s2` 或 `esp32s3`
    ```
    idf.py set-target esp32s2
    ```
6. 编译、下载、查看输出
    ```
    idf.py build flash monitor
    ```

## 使用说明

1. 注意 SD 卡和摄像头接口接线即可
