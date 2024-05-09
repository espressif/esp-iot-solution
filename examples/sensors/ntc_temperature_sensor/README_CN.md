# 温度传感器应用示例

[English Version](./README.md)

本示例演示了如何在 ESP32-C2 / ESP32-C3 / ESP32 / ESP32-C6 / ESP32-S3 系列芯片上使用 `ntc_driver` 组件开发一个热敏电阻传感器温度读取应用程序。具体演示的功能如下：

- 基于最底层驱动 API 的控制
- 基于热敏电阻 CMFA103J3950HANT 驱动的演示
- 可选择配置不同的热敏电阻驱动

CMFA103J3950HANT传感器接线图：

    ```
    ESP8684-DevKitM-1       CMFA103J3950HANT
                            --    
    [ADC1 CHANEL3]  --------> | || NTC IO
                            --     
    ```

## 如何使用该示例

### 硬件需求

1. 通过 `idf.py menuconfig` 选择需要演示的驱动，并进行驱动配置

2. 硬件连接:
    - 对于 CMFA103J3950HANT 驱动，仅需要将信号端口连接到所配置的 GPIO 上。

### 编译和烧写

1. 进入 `**` 目录：

    ```linux
    cd ./esp-iot-solution/examples/sensors/ntc_temperature_sensor
    ```

2. 使用 `idf.py` 工具设置编译芯片，随后编译下载，指令分别为：

    ```linux
    # 设置编译芯片
    idf.py set-target esp32c2

    # 编译并下载
    idf.py -p PORT build flash
    ```

    请将 `PORT` 替换为当前使用的端口号

3. 可使用 `monitor` 查看程序输出，指令为：

    ```
    idf.py -p PORT monitor
    ```


### 示例输出结果

以下为 CMFA103J3950HANT 驱动测试 log:

```log
I (333) app_start: Starting scheduler on CPU0
I (337) app_start: Starting scheduler on CPU1
I (337) main_task: Started on CPU0
I (347) main_task: Calling app_main()
I (347) ntc driver: IoT Ntc Driver Version: 0.1.0
I (347) ntc driver: calibration scheme version is Line Fitting
I (357) ntc driver: Calibration Success
I (367) NTC demo: NTC temperature = 23.44 ℃
I (367) NTC demo: NTC temperature = 23.47 ℃
I (377) NTC demo: NTC temperature = 23.47 ℃
I (377) NTC demo: NTC temperature = 23.47 ℃
I (387) NTC demo: NTC temperature = 23.50 ℃
I (387) NTC demo: NTC temperature = 23.50 ℃
I (397) NTC demo: NTC temperature = 23.44 ℃
I (397) NTC demo: NTC temperature = 23.44 ℃
I (407) NTC demo: NTC temperature = 23.43 ℃
I (407) NTC demo: NTC temperature = 23.43 ℃
```
