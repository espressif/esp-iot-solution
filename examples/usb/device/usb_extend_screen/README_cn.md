## USB 扩展屏示例

使用 [LaunchPad](https://espressif.github.io/esp-launchpad/?flashConfigURL=https://dl.espressif.com/AE/esp-iot-solution/config.toml) 烧录该示例

USB 扩展屏示例可以将 P4 开发板作为一块 windows 的副屏。支持以下功能。

* 支持 1024*600@60FPS 的屏幕刷新速率

* 支持最多五点的屏幕触摸

* 支持音频的输入和输出

## 所需硬件

* 开发板

   1. [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html#getting-started) 开发板
   2. 开发套件中的一块 1024*600 的 MIPI 屏幕
   3. 一个扬声器

* 连接

    1. 将开发板上的高速 USB 口连接到 PC 上

## 编译和烧录

### P4 设备端

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

### 触摸屏控制的不是 P4 的屏幕

* 在控制面板中选择 `平板电脑设置`

* 在配置栏中选择 `设置`

* 按照提示选择 P4 扩展屏

### 调高/调低 JPEG 的图片质量

* 修改 `usb_descriptor.c` 文件中的 `string_desc_arr` Vendor 接口字符串，将 `Ejpg4` 修改为所需的图片质量，数字越大质量越高，同样的一帧图像占用内存更多。

### 修改屏幕分辨率

* 修改 `usb_descriptor.c` 文件中的 `string_desc_arr` Vendor 接口字符串，将 `R1024x600` 修改为所需的屏幕大小

Note: 目前驱动不支持竖屏的屏幕，请使用硬件上为横屏的屏幕。