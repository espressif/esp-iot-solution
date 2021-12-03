[English Version](./README.md)

## USB Camera LCD Display Demo 说明

该示例程序通过 `ESP32-S2` 或 `ESP32-S3` 系列 USB 主机功能，实现对 USB 摄像头 `MJPEG` 数据流读取、本地解码、屏幕刷新，支持以下功能：

* 支持 USB Camera 数据流获取和解析
* 支持 JPEG 本地软件解码
* 支持 LCD 屏幕显示

### 硬件准备

**USB 摄像头选型需要满足的参数（2021.03）：**

  1. 摄像头兼容 USB1.1 全速模式
  2. 摄像头自带 MJPEG 压缩
  3. 摄像头支持设置端点`wMaxPacketSize`为`512`
  4. MJPEG 支持 **320*240** 分辨率
  5. MJPEG 支持设置帧率到 15 帧/s

**USB 摄像头硬件接线：**
  1. USB 摄像头 VBUS 请使用 5V 电源独立供电，亦可使用 IO 控制 VBUS 通断
  2. USB 摄像头 D+ D- 数据线请按常规差分信号标准走线
  3. USB 摄像头 D+ (绿线) 接 ESP32-S2/S3 GPIO20
  4. USB 摄像头 D- (白线) 接 ESP32-S2/S3 GPIO19

### 编译示例代码

示例代码基于 `esp32-s2-kaluga` 开发板编写，可按以下过程直接编译烧写：

1. 添加 ESP-IDF 环境变量，Linux 方法如下，其它平台请查阅 [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables)
    ```
    . $HOME/esp/esp-idf/export.sh
    ```
2. 添加 ESP-IOT-SOLUTION 环境变量，Linux 方法如下，其它平台请查阅 [readme](../../../../README_CN.md)
    ```
    export IOT_SOLUTION_PATH=$HOME/esp/esp-iot-solution
    ```
3. 根据摄像头配置描述符，[修改摄像头配置项](../../../../components/usb/uvc_stream/README.md)
4. 设置编译目标为 `esp32s2` 或 `esp32s3`
    ```
    idf.py set-target esp32s2
    ```
5. 编译、下载、查看输出
    ```
    idf.py build flash monitor
    ```

### 使用说明

1. 注意 LCD 屏幕和摄像头接口接线即可
2. 屏幕默认直接输出摄像头图像
3. 用户可通过使能 `boot animation` 开启开机动画，验证 LCD 屏幕是否能够成功点亮

### 性能参数

**ESP32-S2 240Mhz**：

| 典型分辨率  | USB 典型传输帧率 | JPEG 最大解码+显示帧率* |
| :-----: | :--------------: | :----------------------: |
| 320*240 |        15        |           ~12            |
| 160*120 |        30        |           ~28            |

1. *JPEG 解码+ 显示帧率随 CPU 负载波动
2. 其它分辨率以实际测试为准，或按照解码时间和分辨率正相关估算
