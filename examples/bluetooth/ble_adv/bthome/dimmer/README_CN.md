# BTHome dimmer 示例说明

``BTHome dimmer`` 基于 ``BTHome component``，演示了如何使用 BTHome 协议实现包含一个按键和一个旋钮的通用蓝牙调光器，并通过 BLE 广播发送符合 BTHome 协议的加密广播包。

软件实现的效果：
  * 读取按键与旋钮的值并通过 BLE ADV 发送。
  * 优化了低功耗配置项。

### 硬件需求：
  参考 ESP-Dimmer 硬件设计，开源地址: https://oshwhub.com/esp-college/esp32-h2-switch

### 编译和烧写

1. 进入 `dimmer` 目录：

    ```linux
    cd ./esp-iot-solution/examples/bluetooth/ble_adv/bthome/dimmer
    ```

2. 使用 `idf.py` 工具设置编译芯片，随后编译下载，指令分别为：

    ```linux
    # 设置编译芯片
    idf.py idf.py set-target esp32h2

    # 编译并下载
    idf.py -p PORT build flash
    ```

    请将 `PORT` 替换为当前使用的端口号
### 功耗分析

### 示例输出
  为了优化功耗，配置项默认关闭了日志输出，在按压和旋转时可以使用手机 APP 搜索到名为 `DIY` 的设备。


关于 BTHome 实现的具体细节请参考：![BTHome 主页](https://bthome.io/)
关于如何将 BTHome 设备接入到 HomeAssistant 请参考：![BTHome Guide](https://www.home-assistant.io/integrations/bthome/)