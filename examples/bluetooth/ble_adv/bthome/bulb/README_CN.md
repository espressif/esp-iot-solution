# BTHome bulb 示例说明

``BTHome bulb`` 基于 ``BTHome component``，通过 BLE 扫描接收符合 BTHome 协议的加密广播包，与 [dimmer 示例](../dimmer/) 配对，并根据按键与调光事件控制 LED 灯带。

软件实现的效果：

  * 被动扫描 dimmer 发送的 BTHome 广播（通过 MAC 地址白名单过滤）。
  * 解密并解析 BTHome 按键与调光事件。
  * 按键单击切换 LED 开关，旋钮旋转调节亮度。

### 支持的芯片

  * **ESP32-H2**：默认目标，与 ESP32-H2 上的 dimmer 示例配对使用。
  * **ESP32-H4**：已支持；LED 灯带 GPIO 默认为 37（典型 EV 板接线）。

### 硬件需求

  * 支持 BLE 的 ESP32 开发板。
  * WS2812 LED 灯带，数据线接至配置的 GPIO（默认 GPIO 8；ESP32-H4 默认为 GPIO 37）。

  LED 灯带 GPIO、灯珠数量与 RMT 分辨率可在 menuconfig 的 `BTHome Bulb Configuration` 中调整。

### 编译和烧写

1. 进入 `bulb` 目录：

    ```linux
    cd ./esp-iot-solution/examples/bluetooth/ble_adv/bthome/bulb
    ```

2. 使用 `idf.py` 工具设置编译芯片，随后编译下载，指令分别为：

    ```linux
    # ESP32-H2
    idf.py set-target esp32h2

    # ESP32-H4
    idf.py set-target esp32h4

    # 编译并下载
    idf.py -p PORT build flash
    ```

    请将 `PORT` 替换为当前使用的端口号。

### 示例输出

  在一块开发板上烧录 [dimmer 示例](../dimmer/)，在另一块开发板上烧录本 bulb 示例。按压 dimmer 按键可切换 LED 开关，旋转旋钮可调节亮度。

关于 BTHome 实现的具体细节请参考：[BTHome 主页](https://bthome.io/)

关于如何将 BTHome 设备接入到 HomeAssistant 请参考：[BTHome Guide](https://www.home-assistant.io/integrations/bthome/)
