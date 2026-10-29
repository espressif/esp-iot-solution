# BTHome dimmer 示例说明

``BTHome dimmer`` 基于 ``BTHome component``，演示了如何使用 BTHome 协议实现包含一个按键和一个旋钮的通用蓝牙调光器，并通过 BLE 广播发送符合 BTHome 协议的加密广播包。

软件实现的效果：

  * 读取按键与旋钮的值并通过 BLE ADV 发送。
  * 仅在按键按压或旋钮旋转时广播，300 ms 后自动停止广播。
  * 支持 light sleep 自动休眠，按键与旋钮通过 RTC IO 唤醒。

### 支持的芯片

  * **ESP32-H2**：默认目标，对应 ESP-Dimmer 硬件参考设计。
  * **ESP32-H4**：低功耗优化版本，选择该目标后会自动加载 `sdkconfig.defaults.esp32h4`。

### 硬件需求

  参考 ESP-Dimmer 硬件设计，开源地址: https://oshwhub.com/esp-college/esp32-h2-switch

### GPIO 引脚约束

  * **ESP32-H4**：按键与旋钮引脚必须为 RTC GPIO 0–5，默认启用 RTC IO 驱动（`CONFIG_EXAMPLE_BUTTON_KNOB_USE_RTC_IO`）。
  * **ESP32-H2**：启用 light sleep 外设掉电时，按键与旋钮引脚需使用 RTC 唤醒 GPIO 7–14。

### 编译和烧写

1. 进入 `dimmer` 目录：

    ```linux
    cd ./esp-iot-solution/examples/bluetooth/ble_adv/bthome/dimmer
    ```

2. 使用 `idf.py` 工具设置编译芯片，随后编译下载，指令分别为：

    ```linux
    # ESP32-H2（默认硬件参考设计）
    idf.py set-target esp32h2

    # ESP32-H4（低功耗优化，自动加载 sdkconfig.defaults.esp32h4）
    idf.py set-target esp32h4

    # 编译并下载
    idf.py -p PORT build flash
    ```

    请将 `PORT` 替换为当前使用的端口号。

### 功耗分析

  示例通过电源管理与动态调频实现低功耗运行：

  * **ESP32-H4 默认配置**（`sdkconfig.defaults.esp32h4`）：CPU 动态调频 16–32 MHz、FreeRTOS tickless idle、light sleep 外设掉电、BLE 控制器休眠、按键/旋钮 RTC IO 唤醒。
  * **RTC IO 驱动**：ESP32-H4 上按键与旋钮使用 `iot_button_new_rtc_device()` 和 `iot_knob_create_rtc()` 实现低功耗唤醒，替代 GPIO 驱动。
  * **外置 32 kHz 晶振**：ESP32-H4 推荐使用（`CONFIG_RTC_CLK_SRC_EXT_CRYS`），休眠电流低于内部 RC 振荡器。

  GPIO 与 CPU 频率选项可在 menuconfig 的 `Example Configuration` 中调整。

### 示例输出

  为了优化功耗，配置项默认关闭了日志输出，在按压和旋转时可以使用手机 APP 搜索到名为 `DIY` 的设备。

关于 BTHome 实现的具体细节请参考：[BTHome 主页](https://bthome.io/)

关于如何将 BTHome 设备接入到 HomeAssistant 请参考：[BTHome Guide](https://www.home-assistant.io/integrations/bthome/)
