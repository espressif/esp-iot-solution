## Sensor Hub Monitor

本示例演示了如何在 ESP32 或 ESP32-S2 上使用 sensor_hub 组件开发一个传感器应用程序。本示例可根据 i2c 总线的扫描结果，动态的发现和加载传感器驱动，以默认参数开启采集和结果上报。

## 如何使用该示例

### 硬件需求

1. 本示例默认支持 `common_components/boards` 组件中包含的所有开发板，以及任何使用 ESP32 或 ESP32-S2 的开发板（可能需要简单修改）; 
2. 本示例默认支持 `sensor_hub` 中已经添加的所有传感器，您可以按照以下接线图同时连接一个或多个已经支持的传感器。

3. 简单接线图：

    ```
    ESP32/ESP32S2 BOARD    Sensors (1)    Sensors (2) ....
                            --             --
                            | || VCC       | || VCC
        [SCL IO]  --------> | || SCL       | || SCL
        [SDA IO]  --------> | || SDA       | || SDA
                            | || GND       | || GND
                            --             --    
    ```

### 编译和烧写

1. 添加 `IOT_SOLUTION_PATH` 环境变量：

    ```
    export IOT_SOLUTION_PATH=PATH
    ```
    请将 `PATH` 替换为 `esp-iot-solution` 目录所在路径，例如 `~/esp-iot-solution`

2. 使用 `menuconfig` 在 `Board Options->Choose Target Board` 中选择一个目标开发板，指令为：

    ```
    idf.py menuconfig
    ```

    默认开发板为`ESP32 Devkit-V4`，该开发板默认的 `SCL` 引脚为 `22`，默认的 `SDA` 引脚为 `21`。

3. 使用`idf.py`工具编译、下载程序，指令为：

    ```
    idf.py -p PORT build flash
    ```

    请将 `PORT` 替换为当前使用的端口号


### 示例输出结果

以下是挂载了`VEML6040` 光照度传感器、 `SHT31` 温湿度传感器、`LIS2DH12` 陀螺仪加速度计三个传感器模块的测试输出结果：

1. 发现三个 i2c 设备，并根据地址推断出传感器型号：

```
I (319) Board_Common: Board Init Done !
I (329) SENSOR_HUB: Find lis2dh12 driver, type: IMU
I (329) SENSOR_HUB: Find sht3x driver, type: HUMITURE
I (339) SENSOR_HUB: Find veml6040 driver, type: LIGHTSENSOR
I (349) SENSOR_HUB: Find sht3x driver, type: HUMITURE
```

2. 创建并启动了对应的传感器：

```
I (349) SENSOR_HUB: Sensor created, Task name = SENSOR_HUB, Type = HUMITURE, Sensor Name = sht3x, Mode = MODE_POLLING, Min Delay = 200 ms
I (359) SENSOR_HUB: task: sensor_default_task created!
I (369) SENSOR_LOOP: event loop created succeed
I (379) SENSOR_LOOP: register a new handler to event loop succeed
I (379) Sensors Monitor: Timestamp = 850604 - sht3x_0x44 STARTED
```

3. 传感器启动以后，以设定的周期采集数据，并发送事件消息：

```
I (579) Sensors Monitor: Timestamp = 1044096 - sht3x_0x44 HUMI_DATA_READY - humiture=45.78
I (779) Sensors Monitor: Timestamp = 1243127 - sht3x_0x44 TEMP_DATA_READY - temperature=25.00

I (779) Sensors Monitor: Timestamp = 1243127 - sht3x_0x44 HUMI_DATA_READY - humiture=45.78
I (979) Sensors Monitor: Timestamp = 1444101 - sht3x_0x44 TEMP_DATA_READY - temperature=25.00
```