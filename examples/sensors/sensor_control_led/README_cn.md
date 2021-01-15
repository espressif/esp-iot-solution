## 传感器控制 LED

本示例演示了如何在 `esp32-meshkit-sense` 开发板上使用 `sensor_hub` 组件快速开发一个光照传感器控制 LED 灯的应用程序，`sensor_hub` 可以帮助用户处理底层传感器的复杂实现，通过事件消息将数据包发送给应用程序，大大降低了传感器应用的开发难度。

## 如何使用该示例

### 硬件需求

1. 本示例默认使用 `common_components/boards` 组件中包含的 `esp32-meshkit-sense` 开发板，您也可根据 `examples/common_components/boards/esp32-meshkit-sense/board.h` 中的开发板引脚定义使用任意 `ESP32` 或 `ESP32S2` 开发板实现; 
2. 本示例默认使用 `sensor_hub` 中已支持光线传感器 `BH1750` ( `sensor_id = SENSOR_BH1750_ID` ) 进行实验，您可以直接切换 `sensor_hub` 中支持的其它光线传感器，或按照接口定义自行实现新的光线传感器。

3. 传感器接线图：

    ```
    esp32-meshkit-sense      BH1750
                            --       
                            | || VCC
        [SCL 32]  --------> | || SCL
        [SDA 33]  --------> | || SDA
                            | || GND
                            --      
    ```

### 编译和烧写

1. 添加 `IOT_SOLUTION_PATH` 环境变量：

    ```
    export IOT_SOLUTION_PATH=PATH
    ```
    请将 `PATH` 替换为 `esp-iot-solution` 目录所在路径，例如 `~/esp-iot-solution`

2. 使用`idf.py`工具编译、下载程序，指令为：

    ```
    idf.py -p PORT build flash
    ```

    请将 `PORT` 替换为当前使用的端口号

3. 可使用 `monitor` 查看程序输出，指令为：

    ```
    idf.py -p PORT monitor
    ```

### 示例输出结果

以下是挂载了`BH1750` 光照度传感器输出结果：

```
(0) cpu_start: Starting scheduler on APP CPU.
I (346) gpio: GPIO[2]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (356) gpio: GPIO[4]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (366) gpio: GPIO[12]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (376) gpio: GPIO[14]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (386) gpio: GPIO[15]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (396) gpio: GPIO[27]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (406) i2c_bus: i2c0 bus inited
I (406) Board: Board Info: ESP32-MESHKIT-SENSE_V1_1
I (416) Board: Init Done ...
I (416) SENSOR_HUB: Sensor created, Task name = SENSOR_HUB, Type = LIGHTSENSOR, Sensor ID = 49, Mode = MODE_POLLING, Min Delay = 200 ms
I (426) SENSOR_HUB: task: sensor_default_task created!
I (436) SENSOR_LOOP: event loop created succeed
I (436) SENSOR_LOOP: register a new handler to event loop succeed
I (446) LIGHT_SENSOR_TRIGGER: Timestamp = 204615 - LIGHTSENSOR SENSOR_STARTED
I (676) LIGHT_SENSOR_TRIGGER: Timestamp = 430814 - SENSOR_LIGHT_DATA_READY - light=440.00
I (876) LIGHT_SENSOR_TRIGGER: Timestamp = 630671 - SENSOR_LIGHT_DATA_READY - light=440.00
I (1076) LIGHT_SENSOR_TRIGGER: Timestamp = 830671 - SENSOR_LIGHT_DATA_READY - light=440.00
I (1276) LIGHT_SENSOR_TRIGGER: Timestamp = 1030667 - SENSOR_LIGHT_DATA_READY - light=440.00
I (1476) LIGHT_SENSOR_TRIGGER: Timestamp = 1230671 - SENSOR_LIGHT_DATA_READY - light=440.00
I (1676) LIGHT_SENSOR_TRIGGER: Timestamp = 1430668 - SENSOR_LIGHT_DATA_READY - light=436.67
I (1876) LIGHT_SENSOR_TRIGGER: Timestamp = 1630671 - SENSOR_LIGHT_DATA_READY - light=300.00
I (2076) LIGHT_SENSOR_TRIGGER: Timestamp = 1830667 - SENSOR_LIGHT_DATA_READY - light=190.00
I (2076) LIGHT_SENSOR_TRIGGER: LED turn on
I (2276) LIGHT_SENSOR_TRIGGER: Timestamp = 2030672 - SENSOR_LIGHT_DATA_READY - light=156.67
I (2476) LIGHT_SENSOR_TRIGGER: Timestamp = 2230668 - SENSOR_LIGHT_DATA_READY - light=63.33
I (2676) LIGHT_SENSOR_TRIGGER: Timestamp = 2430672 - SENSOR_LIGHT_DATA_READY - light=66.67
I (2876) LIGHT_SENSOR_TRIGGER: Timestamp = 2630668 - SENSOR_LIGHT_DATA_READY - light=283.33
I (3076) LIGHT_SENSOR_TRIGGER: Timestamp = 2830672 - SENSOR_LIGHT_DATA_READY - light=426.67
I (3076) LIGHT_SENSOR_TRIGGER: LED turn off
```
