## USB 转串口 Bridge

使用 ESP32-S2/S3 的 USB 和 UART 功能，制作可一个可用于调试和下载的 USB 转串口工具，功能如下：

1. USB - UART 双向数据透传
2. 可配置串口参数（波特率最大支持 3000000 bps）
3. 兼容 `esptool` 自动下载功能，可为其它 `ESP` 芯片下载固件

```c
/*
 *                                │
 *                                │USB
 *                                │
 *                                ▼
 *                            ┌──────┐
 *                            │      │
 *                            │ O  O │
 *                            │      │
 *                        ┌───┴──────┴───┐
 *                        │              │
 * ┌───────────┐          │ ESP32-S3-OTG │
 * │           │          │              │
 * │           │ RX       │ ┌──┐         │
 * │           │◄─────────┼─┤  │expend IO│
 * │           │ TX       │ │  │         │
 * │Target_MCU │◄─────────┼─┤  │         │
 * │           │ IO0      │ │  │         │
 * │           │◄─────────┼─┤  │         │
 * │           │ EN       │ │  │         │
 * │           │◄─────────┼─┤  │         │
 * │           │          │ └──┘         │
 * └───────────┘          │              │
 *                        │              │
 *                        │              │
 *                        └───┬──────┬───┘
 *                            │ [  ] │
 *                            └──────┘
 */
```

### 默认引脚定义

默认使用 ESP32-S3-OTG 开发板，USB 转串口 Demo 利用了其中 4 个 扩展 IO，定义如下：

| 开发板 IO |     功能     |      备注      |
| :-------: | :----------: | :------------: |
|    20     |   USB D+    | USB 固定引脚   |
|    19     |   USB D-    | USB 固定引脚  |
|    46     |   UART-RX    | 连接目标板 TX  |
|    45     |   UART-TX    | 连接目标板 RX  |
|    26     | 自动下载-IO0 | 连接目标板 IO0 |
|     3     | 自动下载-EN  | 连接目标板 EN  |

```
用户也可在 `menuconfig → Board Options → Choose Target Board` 中选择 `esp32s2-saola` 开发板，或修改源码配置 IO。
```

### 编译示例代码

1. 确认 `ESP-IDF` 环境成功搭建，并按照说明文件切换到指定 commit [idf_usb_support_patch](../../../usb/idf_usb_support_patch/readme.md)

2. 确认已经完整下载 `ESP-IOT-SOLUTION` 仓库，并切换到 `usb/add_usb_solutions` 分支

    ```bash
    git clone -b usb/add_usb_solutions --recursive https://github.com/espressif/esp-iot-solution
    ```

3. 添加 `ESP-IDF` 环境变量，Linux 方法如下，其它平台请查阅 [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables)

    ```bash
    $HOME/esp/esp-idf/export.sh
    ```

4. 添加 `ESP-IOT-SOLUTION` 环境变量，Linux 方法如下:

    ```bash
    export IOT_SOLUTION_PATH=$HOME/esp/esp-iot-solution
    ```

5. 设置编译目标为 `esp32s2` 或 `esp32s3`

    ```bash
    idf.py set-target esp32s2
    ```

6. 选择目标开发板，该示例开发板默认为 `ESP32-S3-OTG`

    `menuconfig → Board Options → Choose Target Board`

7. 编译、下载、查看输出

    ```bash
    idf.py build flash monitor
    ```

## esptool 自动下载功能

该功能使用 ESP 芯片制作固件下载器，可为其它无 `CP2102` 等串口芯片的 ESP 开发板下载固件：

* 自动下载功能默认为开启状态，可在 `menuconfig → USB UART Bridge Demo → Enable auto download` 中关闭该选项
* 使用自动下载功能时，请将开发板 `AUTODLD_EN_PIN` 和 `AUTODLD_IO0_PIN` 引脚分别连接到目标 MCU IO0（Boot）和 EN （RST）引脚
* 自动下载功能可能对某些开发板无效（RC 电路导致的时延问题），需要微调代码 `gpio_set_level` 软件延时