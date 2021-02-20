:orphan:

ESP32 触摸功能开发套件
======================

:link_to_translation:`en:[English]`

1. 概述
-------

ESP32-Sense 开发套件用于评估和开发 ESP32触摸传感器功能。评估套件包含一个主板和若干子板，主板包含显示单元，主控单元，调试单元；子板的触摸电极的形状和排列方式多样，包括线性滑条、矩阵按键、弹簧按键、轮式滑条等，可以满足不同的使用场景。用户也可以自行设计子板以满足特殊的使用场景。
下图是 ESP32-Sense 开发套件：

.. figure:: ../../_static/hw-reference/touch_eb/touch_eb_overview.jpg
   :align: center

2. 相关资源
-----------

-  **开发环境**

   -  `ESP-IDF <https://github.com/espressif/esp-idf>`__ 是 ESP32
      平台的软件开发包。文档 `Get
      Started <https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html>`__
      介绍编译环境的搭建和软件开发包的使用说明。
   -  :doc:`ESP-Prog <ESP-Prog_guide>` 是 ESP32 调试工具，有下载和Jtag 调试功能。

-  **硬件手册**

   -  ESP32-Sense 开发套件的硬件原理图，PCB 文件，BOM 等文件参考
      `乐鑫官网 <http://espressif.com/zh-hans/support/download/documents?keys=参考设计>`__\ 。
   -  ESP-Prog 的介绍请参考文档 :doc:`ESP-Prog <ESP-Prog_guide>` 。

-  **其他参考资料**

   -  `Espressif 官网 <http://www.espressif.com>`__

   -  `ESP32编程指南 <https://docs.espressif.com/projects/esp-idf/en/stable/index.html>`__: ESP32 相关开发文档的汇总平台，包含硬件手册，软件 API 介绍等。

-  **技术支持**

   -  若在 ESP32-Sense 开发套件使用时遇到问题，请在 ESP32-Sense
      工程中提交
      `issue <https:404/issues>`__。

-  **购买方式**

   -  微信公众号： espressif\_systems
   -  `商务咨询 <http://www.espressif.com/en/company/contact/pre-sale-questions-crm>`__

3. 开发板操作
-------------

-  **保护盖板安装**

   塑料材质保护盖板厚度应小于 3
   mm。空气间隙会导致手指的触摸感应变弱，所以保护盖板应紧贴子板安装。如果有空气间隙可以使用双面胶填充。弹簧子板上应使用
   7 mm 螺柱安装保护盖板。

   .. figure:: ../../_static/hw-reference/touch_eb/overlay_mount.jpg
      :align: center

-  **子板安装**

   子板与主板之间使用连接器连接，连接器既有连通作用，也可以固定住子板。子板安装孔可安装
   4 个 7 mm 塑料镙柱，使子板水平支撑在主板上，如下图：

   .. figure:: ../../_static/hw-reference/touch_eb/board_setup.png
      :align: center

-  **ESP-Prog 调试器设置**

   ESP-Prog 起到下载程序和供电作用。调试器上有两组跳针，IO0
   跳针和供电跳针，供电跳针上的供电电压选择为 5V。因为 IO0
   既有启动模式（下载模式、运行模式）选择功能，同时也是触摸功能管脚，如果
   IO0 作为触摸功能使用，则运行模式时，调试器上 IO0
   跳针应断开。调试器设置如下图：

   .. figure:: ../../_static/hw-reference/touch_eb/board_pwr_sel.jpg
      :align: center

-  **ESP-Prog 调试器连接主板**

   调试器包含 Jtag 接口和 Program 接口。与主板连接使用 Program
   下载接口。

   .. figure:: ../../_static/hw-reference/touch_eb/board_pgm_connection.jpg
      :align: center


-  **编译下载**

   执行命令 ``make menuconfig``
   配置参数，如下图所示。执行命令
   ``make flash``，程序会自动下载到开发板中。

   .. figure:: ../../_static/hw-reference/touch_eb/menuconfig-1.png
      :align: center

   .. figure:: ../../_static/hw-reference/touch_eb/menuconfig-2.png
      :align: center

-  **更换子板**

   ESP32上电时会检测子板上的分压电阻的分压值，识别不同的子板。更换子板后需给开发板重新上电。

4. 硬件介绍
-----------

4.1 主板
~~~~~~~~

-  **主板系统框图**

   下图是主板的功能框图。

   .. figure:: ../../_static/hw-reference/touch_eb/touch_eb_block_diagram.png
      :align: center

-  **主板硬件资源**

   显示单元包括数码管和 RGB 三色灯电路。调试单元包括 ESP-Prog
   调试器接口。主控单元包含 ESP32 模组。Mini USB 供电作用。

   .. figure:: ../../_static/hw-reference/touch_eb/board_description.png
      :align: center

-  **电源管理系统**

   ESP32-Sense 开发套件上 Mini USB 和 ESP-Prog
   均可供电，两者之间有保护二极管隔离，供电不相互影响。USB
   只具有供电功能。ESP-Prog
   接口除了供电功能还具有自动烧写固件功能。下图是电源管理系统的硬件原理图：

   .. figure:: ../../_static/hw-reference/touch_eb/board_pwr_supply.png
      :align: center

-  **显示电路**

   ESP32-Sense
   开发套件的主板上有显示单元，可直观地反馈触摸动作。三个数码管分别显示被触摸按键的位置和触摸动作的持续时间。数码管驱动芯片为
   CH455G，使用 I2C 接口控制。RGB
   灯用于触摸时的颜色反馈。用户滑动滑条，RGB 灯的颜色会相应变化。
   下图是显示单元的硬件原理图：

   .. figure:: ../../_static/hw-reference/touch_eb/board_7seg_display.png
      :align: center

   .. figure:: ../../_static/hw-reference/touch_eb/board_rgb_module.png
      :align: center  

4.2 子板
~~~~~~~~

-  **子板分压电阻**

   子板上的触摸电极有多种形状和排列方式。每个子板上有分压电阻，每个分压电阻的阻值不同，主板应用程序通过
   ADC 读取分压值，识别不同类型的子板。分压电路如下图所示：

   .. figure:: ../../_static/hw-reference/touch_eb/board_adc.png
      :align: center 

   主板上的分压电阻是 10 KΩ，下表是各子板上对应的分压电阻阻值：

+------------+-------------------+-------------------+------------------+
| 子板       | 分压电阻 (Kohm)   | ADC 读数（Min）   | ADC 读数 (Max)   |
+============+===================+===================+==================+
| 弹簧按键   | 0                 | 0                 | 250              |
+------------+-------------------+-------------------+------------------+
| 线性滑条   | 4.7               | 805               | 1305             |
+------------+-------------------+-------------------+------------------+
| 矩阵按键   | 10                | 1400              | 1900             |
+------------+-------------------+-------------------+------------------+
| 双工滑条   | 19.1              | 1916              | 2416             |
+------------+-------------------+-------------------+------------------+
| 轮式滑条   | 47                | 2471              | 2971             |
+------------+-------------------+-------------------+------------------+

5. 应用程序介绍
---------------

ESP32 IoT Solution 工程下的 ESP32-Sense 是 ESP32-Sense
开发套件对应的应用程序。目录结构如下图所示：

::

    ├── main
    │   ├── evb_adc.c               //使用 ADC 功能识别不同子板，设置每个子板对应的 ADC 阈值
    │   ├── evb.h                   //主板应用程序参数设置（触摸阈值，ADC I/O，I2C I/O）
    │   ├── evb_led.cpp             //RGB LED 初始化程序
    │   ├── evb_seg_led.c           //数码管驱动程序
    │   ├── evb_touch_button.cpp    //子板驱动程序-触摸按键
    │   ├── evb_touch_wheel.cpp     //子板驱动程序-轮式滑条
    │   ├── evb_touch_matrix.cpp    //子板驱动程序-矩阵按键
    │   ├── evb_touch_seq_slide.cpp //子板驱动程序-双工滑条
    │   ├── evb_touch_slide.cpp     //子板驱动程序-线性滑条
    │   ├── evb_touch_spring.cpp    //子板驱动程序-弹簧按键
    │   ├── Kconfig.projbuild
    │   └── main.cpp                //主程序入口
    ├── Makefile
    └── sdkconfig.defaults

5.1. 参数配置
~~~~~~~~~~~~~

当使用不同厚度或不同材质的盖板时，需要重新设置各通道触摸变化率参数，即灵敏度。各通道触摸变化率是由脉冲计数值计算得到。计算公式为：(Non-touch
value - Touch value) / Non-touch value。“Non-touch value”
是指不触摸时通道的脉冲计数值。“Touch value”
是指正常触摸时通道的脉冲计数值。这两个参数需要用户测量得出。系统初始化时，由触摸变化率自动计算出触摸阈值，触摸阈值与触摸变化率成正比关系。


5.2. 效果展示
~~~~~~~~~~~~~



+------------------------------------------------------------------+--------------------------------------------------------------------------+
| .. figure:: ../../_static/hw-reference/touch_eb/touch_spring.jpg | .. figure:: ../../_static/hw-reference/touch_eb/touch_matrix.jpg         |
+==================================================================+==========================================================================+
| .. centered:: Spring Button                                      | .. centered:: Matrix Button                                              |
+------------------------------------------------------------------+--------------------------------------------------------------------------+
| .. figure:: ../../_static/hw-reference/touch_eb/touch_slide.jpg  | .. figure:: ../../_static/hw-reference/touch_eb/touch_diplexed_slide.jpg |
+------------------------------------------------------------------+--------------------------------------------------------------------------+
| .. centered:: Liner Slider                                       | .. centered:: Duplex Slider                                              |
+------------------------------------------------------------------+--------------------------------------------------------------------------+
| .. figure:: ../../_static/hw-reference/touch_eb/touch_wheel.jpg  |                                                                          |                                                                          
+------------------------------------------------------------------+--------------------------------------------------------------------------+
|  .. centered::      Wheel Slider                                 |                                                                          |
+------------------------------------------------------------------+--------------------------------------------------------------------------+
