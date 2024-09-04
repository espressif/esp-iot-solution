## USB 扩展屏示例

使用 [LaunchPad](https://espressif.github.io/esp-launchpad/?flashConfigURL=https://dl.espressif.com/AE/esp-iot-solution/config.toml) 烧录该示例

USB 扩展屏示例可以将 [ESP32_S3_LCD_EV_BOARD](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) / [ESP32_P4_FUNCTION_EV_BOARD](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) 开发板作为一块 windows 的副屏。支持以下功能。

* P4: 支持 1024*576@60FPS 的屏幕刷新速率

* S3: 支持 800*480@13FPS 的屏幕刷新速率

* 支持最多五点的屏幕触摸

* 支持音频的输入和输出

## 所需硬件

* P4 开发板

   1. [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html#getting-started) 开发板
   2. 开发套件中的一块 1024*600 的 MIPI 屏幕
   3. 一个扬声器

* S3 开发板

   1. [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html#getting-started) 开发板
   2. 开发套件中的一块 800\*480 / 480\*480 的 RGB 屏幕
   3. 一个扬声器

## 硬件连接

* 连接

    1. 将开发板上的高速 USB 口连接到 PC 上

## 编译和烧录

### 设备端

构建项目并将其烧录到板子上，然后运行监控工具以查看串行输出：

* 运行 `. ./export.sh` 以设置 IDF 环境
* 运行 `idf.py set-target esp32p4` 以设置目标芯片
* 如果上一步出现任何错误，请运行 `pip install "idf-component-manager~=1.1.4"` 来升级你的组件管理器
* 运行 `idf.py -p PORT flash monitor` 来构建、烧录并监控项目

（要退出串行监视器，请按 `Ctrl-]`。）

请参阅《入门指南》了解配置和使用 ESP-IDF 构建项目的所有步骤。

>> 注意：此示例将在线获取 AVI 文件。请确保首次编译时连接互联网。

### PC 端

准备工作，请参考 [windows_driver](./windows_driver/README_cn.md)

![Demo](https://dl.espressif.com/AE/esp-iot-solution/p4_usb_extern_screen.gif)

## 其他问题

### 触摸屏控制的不是设备端的屏幕

* 在控制面板中选择 `平板电脑设置`

* 在配置栏中选择 `设置`

* 按照提示选择扩展屏

### 调高/调低 JPEG 的图片质量

* 修改 `CONFIG_USB_EXYEEND_SCREEN_JEPG_QUALITY`，数字越大质量越高，同样的一帧图像占用内存更多。

### 修改副屏分辨率

* 修改 `CONFIG_USB_EXTEND_SCREEN_HEIGHT` 和 `CONFIG_USB_EXTEND_SCREEN_WIDTH`

Note: 目前驱动不支持竖屏的屏幕，请使用硬件上为横屏的屏幕。

### 修改图像输出帧率

* 修改 `CONFIG_USB_EXTEND_SCREEN_MAX_FPS`，降低该值可以有效的减少 USB 带宽，当 USB AUDIO 音频卡顿时，可以适当减少此值。

### 修改一帧图像的最大值

* 修改 `CONFIG_USB_EXTEND_SCREEN_FRAME_LIMIT_B`，可以限制 PC 驱动传来的图像最大长度。

### 不启用触摸屏功能

* 修改 `CONFIG_HID_TOUCH_ENABLE` 为 `n`

### 不启用音频功能

* 修改 `CONFIG_UAC_AUDIO_ENABLE` 为 `n`

Note: 当只启用副屏功能，请将 PID 修改为 `0x2987`
