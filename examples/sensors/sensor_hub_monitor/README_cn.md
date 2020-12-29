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

以下是挂载了`BH1750` 光照度传感器、 `SHT31` 温湿度传感器、`MPU6050` 陀螺仪加速度计三个传感器模块的测试输出结果：

1. 发现三个 i2c 设备，并根据地址推断出传感器型号：

```
I (348) i2c_bus: i2c0 bus inited
I (348) Board: Board Info: ESP32-Devkitc
I (358) Board: Board Init Done ...
I (358) SENSOR_LOOP: event loop created succeed
I (368) SENSOR_LOOP: register a new handler to event loop succeed
I (378) i2c_bus: found i2c device address = 0x23
I (388) i2c_bus: found i2c device address = 0x44
I (398) i2c_bus: found i2c device address = 0x68
I (398) SENSOR_HUB: address 0x23 might be BH1750 (Light Intensity sensor)

I (408) SENSOR_HUB: address 0x23 found 1 matched info

I (418) SENSOR_HUB: address 0x44 might be SHT31 (Humi/Temp sensor)

I (428) SENSOR_HUB: address 0x44 found 1 matched info

I (428) SENSOR_HUB: address 0x68 might be MPU6050 (Gyro/Acce sensor)

I (438) SENSOR_HUB: address 0x68 found 1 matched info
```

2. 分别创建并启动了这三个传感器：

```
I (448) SENSOR_HUB: Sensor created, Task name = SENSOR_HUB, Type = LIGHTSENSOR, Sensor ID = 49, Mode = MODE_POLLING, Min Delay = 200 ms
I (458) SENSOR_HUB: task: sensor_default_task created!
I (468) Sensors Monitor: Timestamp = 226526 - LIGHTSENSOR SENSOR_STARTED
I (478) Sensors Monitor: BH1750 (Light Intensity sensor) created
I (488) SENSOR_HUB: Sensor created, Task name = SENSOR_HUB, Type = HUMITURE, Sensor ID = 17, Mode = MODE_POLLING, Min Delay = 200 ms
I (498) Sensors Monitor: Timestamp = 253110 - HUMITURE SENSOR_STARTED
I (498) Sensors Monitor: SHT31 (Humi/Temp sensor) created
I (508) MPU6050: mpu6050 device address is: 0x68

I (518) SENSOR_HUB: Sensor created, Task name = SENSOR_HUB, Type = IMU, Sensor ID = 33, Mode = MODE_POLLING, Min Delay = 200 ms

I (528) Sensors Monitor: Timestamp = 283925 - IMU SENSOR_STARTED
I (538) Sensors Monitor: MPU6050 (Gyro/Acce sensor) created
```

3. 传感器启动以后，以设定的周期采集数据，并发送事件消息：

```
I (698) Sensors Monitor: Timestamp = 450729 - SENSOR_LIGHT_DATA_READY - light=353.33
I (698) Sensors Monitor: Timestamp = 451679 - SENSOR_TEMP_DATA_READY - temperature=24.47

I (698) Sensors Monitor: Timestamp = 451679 - SENSOR_HUMI_DATA_READY - humiture=38.58
I (728) Sensors Monitor: Timestamp = 483688 - SENSOR_GYRO_DATA_READY - gyro_x=0.26, gyro_y=-2.00, gyro_z=-0.92

I (728) Sensors Monitor: Timestamp = 483688 - SENSOR_ACCE_DATA_READY - acce_x=0.07, acce_y=0.43, acce_z=0.80

I (898) Sensors Monitor: Timestamp = 650686 - SENSOR_LIGHT_DATA_READY - light=353.33
I (898) Sensors Monitor: Timestamp = 651049 - SENSOR_TEMP_DATA_READY - temperature=24.47

I (898) Sensors Monitor: Timestamp = 651049 - SENSOR_HUMI_DATA_READY - humiture=38.58
I (928) Sensors Monitor: Timestamp = 683593 - SENSOR_GYRO_DATA_READY - gyro_x=0.44, gyro_y=-2.18, gyro_z=-0.93

I (928) Sensors Monitor: Timestamp = 683593 - SENSOR_ACCE_DATA_READY - acce_x=0.08, acce_y=0.43, acce_z=0.80
```