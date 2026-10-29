## USB 扩展屏示例

使用 [LaunchPad](https://espressif.github.io/esp-launchpad/?flashConfigURL=https://dl.espressif.com/AE/esp-iot-solution/config.toml) 烧录该示例

USB 扩展屏示例可以将兼容的 ESP32-P4、ESP32-S31 或 ESP32-S3 开发板作为一块 Windows 副屏。板级外设由 [ESP Board Manager](https://github.com/espressif/esp-board-manager) 描述和初始化，应用代码不再绑定某一个 BSP。

本示例直接使用 Board Manager 板卡定义，并按约定的设备名获取板级外设。兼容板卡需要提供帧缓冲格式为 RGB565 或 RGB888 的横屏 `display_lcd` 设备；触摸功能要求设备名为 `lcd_touch`；USB 音频要求存在 `audio_dac` 和 `audio_adc`；如果板卡提供 `lcd_brightness`，示例会自动打开背光。接入自定义板卡时，请在其 Board Manager 板卡定义中沿用这些设备名。

支持以下功能：

* P4: 支持 1024×600@60FPS 的屏幕刷新速率

* S3: 根据 LCD 子板支持 800×480@13FPS 或 480×480

* S31: 支持 800*480@60FPS 的屏幕刷新速率

* 支持最多五点的屏幕触摸

* 支持音频的输入和输出，P4/S31 默认 48 kHz，S3 默认 16 kHz

## 所需硬件

* P4 开发板

   1. [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html#getting-started) 开发板
   2. 开发套件中的一块 1024*600 的 MIPI 屏幕
   3. 一个扬声器

* S3 开发板

   1. [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html#getting-started) 开发板
   2. 开发套件中的一块 800\*480 / 480\*480 的 RGB 屏幕
   3. 一个扬声器

* S31 开发板

   1. [ESP32-S31-Korvo](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html#getting-started) 开发板
   2. 开发套件中的一块 800\*480 RGB 屏幕
   3. 一个扬声器

## 硬件连接

* 连接

    1. 将开发板上的高速 USB 口连接到 PC 上

## 编译和烧录

### 设备端

构建项目并将其烧录到板子上，然后运行监控工具以查看串行输出：

1. 运行 `. ./export.sh` 以设置 IDF 环境。

2. 在当前 ESP-IDF Python 环境中安装一次 Board Manager action helper：

   ```bash
   pip install esp-bmgr-assist
   ```

3. 进入本示例目录，查看可用板卡并生成所选板卡配置：

   ```bash
   cd examples/usb/device/usb_extend_screen
   idf.py bmgr -l

   # 三选一
   idf.py bmgr -b esp32_p4_function_ev_board
   idf.py bmgr -b esp32_s31_korvo_1
   idf.py bmgr -b esp32_s3_lcd_ev_board
   ```

   `bmgr` 会自动选择目标芯片，并生成 `components/gen_bmgr_codes`，不需要再单独执行 `idf.py set-target`。

4. ESP32-S3-LCD-EV-Board 使用 800×480 子板时，改为叠加官方 amend：

   ```bash
   idf.py bmgr -b esp32_s3_lcd_ev_board -a sub_board_800_480_lcd
   ```

   默认板卡定义对应 480×480 子板；该 amend 会把 `display_lcd` 和 `lcd_touch` 替换为 800×480 子板定义。

5. 运行 `idf.py -p PORT flash monitor` 来构建、烧录并监控项目。

（要退出串行监视器，请按 `Ctrl-]`。）

请参阅《入门指南》了解配置和使用 ESP-IDF 构建项目的所有步骤。

> 注意：首次配置/编译会下载托管组件，请确保网络可用。

切换板卡时执行：

```bash
idf.py bmgr -x
idf.py fullclean
idf.py bmgr -b <新板卡名称>
```

如果旧 `sdkconfig` 仍然存在，需要重新生成，使新板卡默认配置生效。

### PC 端

准备工作，请参考 [windows_driver](./windows_driver/README_cn.md)

![Demo](https://dl.espressif.com/AE/esp-iot-solution/p4_usb_extern_screen.gif)

## 其他问题

### 触摸屏控制的不是设备端的屏幕

* 在控制面板中选择 `平板电脑设置`

* 在配置栏中选择 `设置`

* 按照提示选择扩展屏

### 调高/调低 JPEG 的图片质量

* 修改 `CONFIG_USB_EXTEND_SCREEN_JPEG_QUALITY`，数字越大质量越高，同样的一帧图像占用内存更多。

### 修改副屏分辨率

* 分辨率以及 RGB565/RGB888 帧缓冲格式直接来自所选板卡的 `display_lcd` 配置，并同时用于生成 USB vendor 字符串、HID 触摸坐标范围、帧校验和 JPEG 输出缓冲区。
* 更换屏幕时，请选择匹配的板卡定义，或创建/叠加 Board Manager amend，然后重新执行 `idf.py bmgr`。

Note: 目前驱动不支持竖屏的屏幕，请使用硬件上为横屏的屏幕。

### ESP32-P4 芯片版本与 JPEG 对齐

* ESP-IDF 对 ESP32-P4 `<3.0` 和 `>=3.0` 生成的固件互不兼容。Board Manager 负责选择板卡和目标芯片，但不代替芯片版本选择。使用 `<3.0` 芯片时，请在 `idf.py menuconfig` 中启用 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3`，然后重新完整编译；不要把这一选项写进通用板卡 defaults。
* 硬件 JPEG 解码输出会按输入 JPEG 的 MCU 对齐：YUV444 为 8×8、YUV422 为 16×8、YUV420 为 16×16。示例会解析每帧 JPEG 头、预留最坏情况下的 16×16 对齐空间，并在送屏前移除行尾填充，因此像 1024×600 这样的可见分辨率不需要改成 1024×608。
* `<3.0` 芯片还限制部分 YUV 到 YUV 的格式转换；本示例直接解码为屏幕配置的 RGB565 或 RGB888 帧缓冲格式，不使用这些受限组合。

### 修改图像输出帧率

* 修改 `CONFIG_USB_EXTEND_SCREEN_MAX_FPS`，降低该值可以有效的减少 USB 带宽，当 USB AUDIO 音频卡顿时，可以适当减少此值。

### 修改一帧图像的最大值

* 修改 `CONFIG_USB_EXTEND_SCREEN_FRAME_LIMIT_B`，可以限制 PC 驱动传来的图像最大长度。

### 不启用触摸屏功能

* 修改 `CONFIG_HID_TOUCH_ENABLE` 为 `n`
* 只有所选板卡提供 `lcd_touch` 时，此选项才可启用。

### 不启用音频功能

* 修改 `CONFIG_UAC_AUDIO_ENABLE` 为 `n`
* 只有所选板卡提供 Board Manager 音频编解码设备时，此选项才可启用。

Note: 当只启用副屏功能，请将 PID 修改为 `0x2987`
