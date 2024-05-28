## Touch Proximity sensor example

本示例演示了如何在 ESP32-S3 上使用 touch_proximity_sensor 组件开发一个触摸接近感应传感器应用程序，当手与感应开发板距离小于设定参数阈值时，应用程序会驱动蜂鸣器发出警报，接近感应被触发。该示例可应用于各种需要无接触式的接近感应场景，如无接触式电子门铃。

> 注: 该示例仅用于测试和演示目的。由于触摸功能的抗干扰能力较差，可能无法通过EMS测试，因此不建议用于量产产品。

## 如何使用该示例

### 硬件需求

本示例使用的主板为 `ESP32-S2S3-Touch-DevKit-1 MainBoard v1.1`，触摸子板为 `ESP32-S2S3-Touch-DevKit-1 Proximity Board V1.0`，详情请见[参考文档](https://docs.espressif.com/projects/espressif-esp-dev-kits/zh_CN/latest/esp32s2/esp32-s2-touch-devkit-1/user_guide.html)。

### 编译和烧写

使用`idf.py`工具编译、下载程序，指令为：

```
idf.py -p PORT build flash
```

请将 `PORT` 替换为当前使用的端口号


### 示例输出结果

```
I (117) main_task: Started on CPU0
I (118) main_task: Calling app_main()
I (119) gpio: GPIO[15]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (120) touch-proximity-sensor: IoT Touch Proximity Driver Version: 0.1.1
I (161) touch-proximity-sensor: proxi daemon task start!
I (361) touch-proximity-example: touch proximity sensor has started! when you approach the touch sub-board, the buzzer will sound.
I (361) main_task: Returned from app_main()
I (2406) touch-proximity-example: CH8, active!
I (2646) touch-proximity-example: CH8, inactive!
I (3568) touch-proximity-example: CH8, active!
I (4177) touch-proximity-example: CH8, inactive!
I (4767) touch-proximity-example: CH8, active!
I (5561) touch-proximity-example: CH8, inactive!
I (6096) touch-proximity-example: CH8, active!
I (7277) touch-proximity-example: CH8, inactive!
```
