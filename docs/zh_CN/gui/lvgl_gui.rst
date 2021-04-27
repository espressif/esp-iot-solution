LVGL 图形库
=============

:link_to_translation:`en:[English]`

`LVGL <https://lvgl.io/>`_ 是一个 C 语言编写的免费的开源图形库，提供了用于嵌入式 GUI 的各种元素。用户可以利用丰富的图形库资源，在消耗极低内存的情况下构建视觉效果丰富多彩的 GUI 。

主要特性
-------------

LVGL 具有以下特性：

- 超过 30 多种丰富的用户自定义控件，如按钮，滑条，文本框，键盘等
- 支持各种分辨率的屏幕，适配性好
- 接口简单，内存占用少
- 支持多个输入设备
- 提供抗锯齿，多边形，阴影等多种绘图元素
- 采用 UTF-8 编码，支持多语言，多字体的文本
- 支持各种图片类型，可从 Flash 和 SD 卡中读取图片显示
- 提供在线图片取模工具
- 支持 Micropython

配置要求
----------

运行 LVGL 的最低配置要求如下：

- 16、32、64 位的微控制器或处理器
- 时钟频率：大于 16 MHz
- Flash/ROM：大于 64 kB（推荐 180 kB）
- RAM：8 kB（推荐 24 kB）
- 需要一个帧缓存区
- 显示缓存：至少大于水平分辨率的像素
- C99 或更高版本的编译器

在线工具
----------

LVGL 提供了在线的 `字模提取工具 <https://lvgl.io/tools/fontconverter/>`_ 和 `图片取模工具 <https://lvgl.io/tools/imageconverter>`_。

示例方案
---------

官方例程
*********
LVGL 官方提供了 ESP32 上使用 LVGL 的 `LVGL ESP32 示例程序 <https://github.com/lvgl/lv_port_esp32/>`_。

除此之外，在 ESP-IoT-Solution 中也提供了一些应用 LVGL 的实例：

thermostat
************

使用 LVGL 设计了一个恒温计控制的界面：

.. figure:: ../../_static/hmi_solution/littlevgl/thermostat.jpg
   :align: center
   :width: 500

相应例程在 :example:`hmi/lvgl_thermostat`

coffee 
************

使用 LVGL 绘制了一个咖啡机的交互界面：

.. figure:: ../../_static/hmi_solution/littlevgl/lvgl_coffee.jpg
   :align: center
   :width: 500

相应例程在 :example:`hmi/lvgl_coffee`

wificonfig
************

ESP32 连接 Wi-Fi，利用 LVGL 绘制的 Wi-Fi 连接界面，可以显示附近 Wi-Fi 信息，在屏幕上输入密码等。

.. figure:: ../../_static/hmi_solution/littlevgl/lvgl_wificonfig0.jpg
   :align: center
   :width: 500

.. figure:: ../../_static/hmi_solution/littlevgl/lvgl_wificonfig1.jpg
   :align: center
   :width: 500

相应例程在 :example:`hmi/lvgl_wificonfig`

