LCD 概述
===============

:link_to_translation:`en:[English]`

通常所说的 LCD 是 TFT-LCD (薄膜晶体管液晶显示器)的统称。它是⼀种常⻅的数字显示技术，常⽤于显示图像和⽂字。LCD 使⽤液晶材料和偏振光技术，当液晶分⼦受电场影响时，会改变光的偏振⽅向，从⽽控制光线强度，使图像或⽂字显示在屏幕上。

.. figure:: ../../../_static/display/screen/lcd_hardware.png
    :align: center
    :scale: 50%
    :alt: TFT-LCD 的硬件框图

    TFT-LCD 的硬件框图

LCD 具有很多优点，例如：能耗低、寿命⻓、可靠性⾼、清晰度⾼、占⽤空间⼩、颜⾊还原度⾼、抗眩光能⼒强等。因此，它已⼴泛应⽤于各种电⼦设备，如家电、便携式设备、可穿戴设备等。同时，LCD 技术持续进步和完善，其中包括不同的面板类型如 IPS、VA、TN，以及新的 LED 背光技术，这些都进一步提高了 LCD 的性能和用户体验。

本指南包含如下内容：

.. list::

  - `结构`_：LCD 模块的主要结构，主要包含面板、背光源、驱动 IC 和 FPC。
  - `形态`_：LCD 模块的常见形态，主要有矩形屏和圆形屏。
  - `驱动接口`_：LCD 模块的驱动接口，包含 SPI、QSPI、I80、RGB 和 MIPI-DSI。
  - `典型连接方式`_：LCD 模块的典型连接方式，包含 LCD 的通用引脚以及不同类型的接口引脚。
  - `帧率`_：LCD 应用的帧率，包含渲染帧率、接口帧率和屏幕刷新率。

术语表
-----------

请参阅 :ref:`LCD 术语表 <LCD_术语表>` 。

结构
---------------

为了让 LCD 能够稳定工作并且方便开发，厂商通常将 LCD 封装成一个 **集成的模块** 供用户使用，它主要由以下四个部分组成：

  - **面板**：⾯板决定了 LCD 模块的⾊彩、可视⻆度、分辨率。⾯板的价格⾛势直接影响到模块的价格，⾯板的质量、技术的好坏关系到模块整体性能的⾼低。常⻅的⾯板类型有 IPS、VA、TN 等。
  - **背光源**：液晶分⼦⾃身⽆法发光，因此若想出现画⾯，液晶屏需要专⻔的发光源来提供光线，然后经过液晶分⼦的偏转来产⽣不同的颜⾊。背光源起到的是提供光能的作⽤，⼀般可以通过 PWM 控制它的亮度。
  - **驱动 IC**：驱动 IC 通过特定的接⼝对外通信，并控制输出电压让液晶扭转，使其发⽣⾊阶及明暗的变化。它通常包含控制电路和驱动电路两部分。控制电路负责接收来⾃主控芯⽚的信号，以及图像信号的转换与处理， 驱动电路负责输出图像信号并显示到⾯板上。
  - **FPC**：FPC 是 LCD 模块的对外接⼝，⽤于连接驱动 IC、外部驱动电路与主控芯⽚。FPC 由于其出色的柔性和可靠性，可以解决传统刚性线路板的接触问题和较差的抗振动性，从而增强了模块的稳定性和使用寿命。

通常，LCD 模块的选型主要基于其 **面板** 和 **驱动 IC**。例如，我们会考虑面板的类型和分辨率以及驱动 IC 支持的接口类型和色彩格式。驱动 IC 体积很小，通常被贴在 FPC 与面板的连接部位，如图示。

.. figure:: ../../../_static/display/screen/lcd_driver_ic.png
    :align: center
    :scale: 50%
    :alt: LCD 模块的驱动 IC

    LCD 模块的驱动 IC

形态
---------------

对于 LCD 的面板外形，大多数形状都是采用矩形或圆形，生活中最常见到的就是矩形屏，圆形屏多为小尺寸屏幕。

.. figure:: ../../../_static/display/screen/lcd_shape_rect.png
    :align: center
    :scale: 40%
    :alt: LCD 矩形屏

    LCD 矩形屏

.. figure:: ../../../_static/display/screen/lcd_shape_round.png
    :align: center
    :scale: 25%
    :alt: LCD 圆形屏

    LCD 圆形屏

它们的特点及应用场景如下：

.. list-table::
    :widths: 10 30 30
    :header-rows: 1

    * - 类型
      - 特点
      - 应用场景
    * - 矩形屏
      - ⾯积⼤、效果好、信息呈现更多，应⽤范围更⼴
      - 手机、平板电脑、控制面板
    * - 圆形屏
      - 时尚轻便、占⽤空间⼩，有效利⽤设备⾯积
      - 智能穿戴、电动⻋仪表盘、汽⻋显示仪表、智能家电、智能⼿持设备

通常使用 LCD 面板的对角线长度来衡量其 **尺寸** 的大小，单位是英寸或寸，比如常说的 1.28 寸屏和 3.5 寸屏。除了屏幕的物理尺寸，开发者往往更加关心屏幕的 **分辨率**，它是指面板所能显示的像素点数量，代表了屏幕图像的精密度：可显示的像素越多，画面就越精细，同样的屏幕区域内能显示的信息越多，对主控芯片的性能要求也越高，所以分辨率是个非常重要的参数指标。

.. figure:: ../../../_static/display/screen/lcd_size.png
    :align: center
    :scale: 25%
    :alt: 屏幕尺寸

    屏幕尺寸

.. figure:: ../../../_static/display/screen/lcd_resolution.png
    :align: center
    :scale: 25%
    :alt: 屏幕分辨率

    屏幕分辨率

尺寸与分辨率之间不是一一对应的关系，但是总体呈正比的趋势，比如，一般情况下，2.4 寸或者 2.8 寸的屏幕常见分辨率为 320x240，3.2 寸或 3.5 寸的屏幕常见分辨率为 320x480。尺寸大的屏幕，其分辨率不一定会比更小尺寸的屏幕更高，因此，在进行屏幕选型前，需要根据应用场景和需求确定好屏幕的尺寸与分辨率。

.. _LCD_概述_驱动接口:

驱动接口
---------------

对于开发者而言，通常更加关心 LCD 的驱动接口，目前在物联网领域比较常见的接口类型有 ``SPI``、 ``QSPI``、 ``I80``、 ``RGB`` 和 ``MIPI-DSI``，它们在 ``占用 IO 数量``、 ``并行数据位数``、 ``数据传输带宽``、 ``GRAM 位置`` 等方面的参数对比如下：

参数对比
^^^^^^^^^^^^^^^

.. list-table::
    :widths: 10 75 5 5 5 10
    :header-rows: 1

    * - 类型
      - 描述
      - 占用 IO 数量
      - 并行数据位数
      - 数据传输带宽
      - GRAM 位置
    * - SPI
      - 串行接口，以 SPI 总线协议为基础，通常采用 4 线或 3 线模式
      - 最少
      - 1
      - 最小
      - LCD
    * - QSPI (Quad-SPI)
      - SPI 接口的一种扩展，可以使用 4 根数据线并行传输
      - 较少
      - 4
      - 较小
      - LCD 或主控
    * - I80 (MCU、DBI)
      - 并行接口，以 I80 总线协议为基础
      - 较多
      - 8/16
      - 较大
      - LCD
    * - RGB (DPI)
      - 并行接口，一般需搭配 3-wire SPI 接口
      - 最多
      - 8/16/18/24
      - 较大
      - 主控
    * - MIPI-DSI
      - 采⽤差分信号传输⽅式的串⾏接⼝，基于 MIPI 的⾼速、低功率可扩展串⾏互联的 D-PHY 物理层规范
      - 较多
      - 1/2/3/4
      - 最大
      - LCD 或主控

.. note::

  - 对于 ``QSPI`` 接口，不同型号的驱动 IC 可能采用不同的驱动方式，如 *SPD2010* 内置 GRAM，其驱动方式与 ``SPI/I80`` 接口类似，而 *ST77903* 没有内置 GRAM，其驱动方式与 ``RGB`` 接口类似。
  - 对于 ``MIPI-DSI`` 接口，采用 Command 模式需要 LCD 内置 GRAM，而 Video 模式则不需要。

总结如下：

  #. ``SPI`` 接口的数据传输带宽小，比较适用于低分辨率的屏幕。
  #. ``QSPI`` 和 ``I80`` 接口的数据传输带宽更大，所以能够支持较高分辨率的屏幕，但是 ``I80`` 接口要求 LCD 内置 GRAM，导致屏幕成本较高，并且难以做到大屏。
  #. ``RGB`` 与 ``I80`` 接口类似，但是 ``RGB`` 接口无需 LCD 内置 GRAM，因此适用于更高分辨率的屏幕。
  #. ``MIPI-DSI`` 接口适用于高分辨率、高刷新率的屏幕。

接口详解
^^^^^^^^^^^^^^^

驱动 LCD 的第一步是确定它的接口类型，对于大部分常见的驱动 IC，如 *ST7789*、 *GC9A01*、 *ILI9341* 等，它们一般都会支持多种接口，但是屏幕厂商在封装成模块的时候通常只对外留出其中一种接口（RGB LCD 通常会也会使用 SPI 接口）。以 *GC9A01* 为例，它的硬件框图如下：

.. figure:: ../../../_static/display/screen/lcd_gc9a01_block.png
    :align: center
    :scale: 50%
    :alt: GC9A01 的硬件框图

    GC9A01 的硬件框图

很多 LCD 驱动 IC 的实际接口类型是由其 ``IM[3:0]`` 引脚的高低电平来决定的，大部分屏幕在内部已经固定了这些引脚的配置，但是也有一些屏幕会预留出这些引脚以及所有的接口引脚，这种情况下用户可以自行配置。以 *ST7789* 为例，它的接口类型配置如下：

.. figure:: ../../../_static/display/screen/lcd_st7789_interface.png
    :align: center
    :scale: 50%
    :alt: ST7789 的驱动接口配置

    ST7789 的接口配置

因此，仅仅知道驱动 IC 的型号并不能确定屏幕的接口类型，在这种情况下可以咨询屏幕厂商，或者查阅屏幕的数据手册，也可以通过原理图结合经验进行判断，下面是各种接口的屏幕引脚对比：

.. list-table::
    :widths: 15 85
    :header-rows: 1

    * - 类型
      - 引脚
    * - LCD 通用
      - RST (RESET)、Backlight (LEDA、LEDK)、TE (tear effect)、Power (VCC、GND)
    * - SPI
      - CS、SCK(SCL)、SDA (MOSI)、SDO (MISO)、DC (RS)
    * - QSPI
      - CS、SCK (SCL)、SDA (DATA0)、DATA1、DATA2、DATA3
    * - I80
      - CS (CSX)、RD (RDX)、WR (WRX)、DC (D/CX)、D[15:0] (D[7:0])
    * - RGB
      - CS、SCK(SCL)、SDA(MOSI)、HSYNC、VSYNC、PCLK、DE、D[23:0] (D[17:0]/D[7:0])

常用接口 LCD 的详细介绍如下：

.. list::

  - :doc:`./spi_lcd`
  - :doc:`./rgb_lcd`
  - I80 LCD 详解（待更新）
  - QSPI LCD 详解（待更新）

典型连接方式
----------------------

对于通用的 LCD 引脚，通常采用如下的连接方式：

  - **RST (RESET)**：推荐连接至 GPIO，并根据 LCD 驱动 IC 的数据手册，在上电时输出复位时序。一般情况下也可以使用上拉/下拉电阻连接系统电源。
  - **Backlight (LEDA、LEDK)**：推荐 LEDA 连接至系统电源（阳极），LEDK 使用开关元器件连接至系统电源（阴极），并通过 GPIO 控制亮灭，或者通过 LEDC 外设输出 PWM 以调节背光亮度。
  - **TE (tear effect)**：推荐连接至 GPIO，通过 GPIO 中断来获取 TE 信号，以实现帧同步。
  - **Power (VCC、GND)**：推荐全部连接至对应的系统电源，而不要让一部分引脚浮空。

对于不同接口类型的引脚，主控 MCU 需要采用不同的连接方式，下面将分别介绍 ``SPI`` ``QSPI`` ``I80`` 和 ``RGB`` 四种接口的典型连接方式。

SPI 接口
^^^^^^^^^^^^^^^

``SPI`` 接口的 LCD 硬件设计请参考开发板 `ESP32-C3-LCDkit <https://docs.espressif.com/projects/espressif-esp-dev-kits/zh_CN/latest/esp32c3/esp32-c3-lcdkit/index.html>`_ 及其 `LCD 子板 <https://docs.espressif.com/projects/espressif-esp-dev-kits/zh_CN/latest/_static/esp32-c3-lcdkit/schematics/SCH_ESP32-C3-LCDkit-DB_V1.0_20230329.pdf>`__，其典型连接示意图如下：

.. figure:: ../../../_static/display/screen/lcd_connection_spi.png
    :align: center
    :scale: 50%
    :alt: SPI 接口典型连接示意图

    SPI 接口典型连接示意图

.. note::

  - ``Interface I 模式`` 仅需使用 ``SDA`` 一根数据线， ``Interface II 模式`` 需要使用 ``MISO & MOSI`` 两根数据线。
  - 通常情况下不需要从 LCD 读取数据，因此可以不连接 ``MISO``。如果有需要的话请注意，大多数 SPI LCD 读取时的最大时钟频率要远小于写入时的频率。
  - 由于 ``3-line 模式`` （无 D/C 信号线）下，每传输单位数据（通常为字节）都需要先传输 D/C 信号（1-bit），而目前 ESP 的 SPI 外设不支持直接传输 9-bit 数据，因此通常采用上图所示的 ``4-line 模式`` 。

QSPI 接口
^^^^^^^^^^^^^^^

``QSPI`` 接口的典型连接示意图如下：

.. figure:: ../../../_static/display/screen/lcd_connection_qspi.png
    :align: center
    :scale: 50%
    :alt: QSPI 接口典型连接示意图

    QSPI 接口典型连接示意图

.. note::

  - 不同型号驱动 IC 的 ``QSPI`` 接口连接方式可能不同，上图仅以 *ST77903* 为例。
  - 写入数据时需要使用 ``SDA0`` 和 ``SDA[1:3]`` 四根数据线，读取数据时仅使用 ``SDA0`` 一根数据线。

I80 接口
^^^^^^^^^^^^^^^

``I80`` 接口的 LCD 硬件设计请参考开发板 `ESP32-S3-LCD-EV-Board <https://docs.espressif.com/projects/espressif-esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-lcd-ev-board/index.html>`_ 及其 `LCD 子板 <https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/_static/esp32-s3-lcd-ev-board/schematics/SCH_ESP32-S3-LCD-EV-Board-SUB2_V1.2_20230509.pdf>`__ (3.5' LCD_ZJY)，其典型连接示意图如下：

.. figure:: ../../../_static/display/screen/lcd_connection_i80.png
    :align: center
    :scale: 50%
    :alt: I80 接口典型连接示意图

    I80 接口典型连接示意图

.. note::

  - 图中虚线表示可选引脚。
  - ESP 的 I80 外设不支持使用 ``RD`` 信号进行读取操作，因此实际连接时需要将该信号拉高。

RGB 接口
^^^^^^^^^^^^^^^

``RGB`` 接口的 LCD 硬件设计请参考开发板 `ESP32-S3-LCD-EV-Board <https://docs.espressif.com/projects/espressif-esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-lcd-ev-board/index.html>`_ 及其 `LCD 子板 <https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/_static/esp32-s3-lcd-ev-board/schematics/SCH_ESP32-S3-LCD-EV-Board-SUB2_V1.2_20230509.pdf>`__ (3.95' LCD_QMZX)，其典型连接示意图如下：

.. figure:: ../../../_static/display/screen/lcd_connection_rgb.png
    :align: center
    :scale: 50%
    :alt: RGB 接口典型连接示意图

    RGB 接口典型连接示意图

.. note::

  - 图中虚线表示可选引脚。
  - ``DE`` 用于 DE 模式下。
  - ``CS``、 ``SCK`` 和 ``SDA`` 为 3-wire (3-line) SPI 接口引脚，用于发送命令及参数对 LCD 进行配置，一些屏幕可能没有这些引脚，因此也不需要进行初始化配置。由于 ``3-wire SPI`` 接口可以仅用于进行 LCD 的初始化，而无需用于后续的屏幕刷新，因此，为了节省 IO 数量，可以将 ``SCK`` 和 ``SDA`` 与任意 ``RGB`` 接口引脚进行复用。

帧率
---------------

对于 LCD 应用来说，屏幕上的动画是通过显示多个连续的静止图像来实现的，这些图像被称为 **帧**。 **帧率** 就是显示新帧的速率，它通常表示为每秒变化的帧数，简称为 FPS。帧率越高，每秒显示的帧就越多，动画变化得也更平滑、更逼真。

但是一帧图像的显示并不是仅由主控一次性完成的，而是经过渲染、传输、显示等多个步骤，因此，帧率的高低不仅取决于主控的性能，还取决于 LCD 的接口类型和刷新率等因素。

渲染
^^^^^^^^^^^^^^^

渲染是指主控通过计算生成图像数据的过程，其快慢可以用 **渲染帧率** 来衡量。

渲染帧率一方面取决于主控的性能，另一方面也受动画复杂程度的影响，比如，局部变化的动画通常比全屏变化的动画渲染帧率更高，纯色填充通常图层混叠的渲染帧率更高。因此，渲染帧率在图像变化时一般是不固定的，如 LVGL 运行时统计的 FPS。

.. only:: latex

  参考 `LVGL 运行时统计的 FPS <https://dl.espressif.com/AE/esp-iot-solution/lcd_fps_lvgl.gif>`_.

.. only:: html

  .. figure:: https://dl.espressif.com/AE/esp-iot-solution/lcd_fps_lvgl.gif
      :height: 504 px
      :width: 453 px
      :align: center
      :alt: LVGL 运行时统计的 FPS

      LVGL 运行时统计的 FPS

传输
^^^^^^^^^^^^^^^

传输是指主控将渲染好的图像数据通过外设接口传输到 LCD 驱动 IC 的过程，其快慢可以用 **接口帧率** 来衡量。

接口帧率取决于 LCD 的接口类型和主控的数据传输带宽，通常在外设接口初始化完成后就会固定，因此可以通过公式计算得出：

.. math::

    接口帧率 = \frac{接口的数据传输带宽}{一帧图像的数据大小}

**对于 SPI/I80 接口**：

.. math::

    接口帧率 = \frac{时钟频率 \times 数据线位数}{色彩位数 \times 水平分辨率 \times 垂直分辨率}

**对于 RGB 接口**：

.. math::

    接口帧率 = \frac{时钟频率 \times 数据线位数}{色彩位数 \times 水平周期 \times 垂直周期}

    水平周期 = 水平脉冲宽度 + 水平后廊 + 水平分辨率 + 水平前廊

    垂直周期 = 垂直脉冲宽度 + 垂直后廊 + 垂直分辨率 + 垂直前廊

显示
^^^^^^^^^^^^^^^

显示是指 LCD 的驱动 IC 将接收到的图像数据显示到屏幕上的过程，其快慢可以用 **屏幕刷新率** 来衡量。

对于 SPI/I80 接口的 LCD，屏幕刷新率是由 LCD 驱动 IC 决定的，一般可以通过发送特定的命令来设置，如 *ST7789* 的 ``FRCTRL2(C6h)`` 命令；对于 RGB 接口的 LCD，屏幕刷新率是由主控决定的，其等价于接口帧率。

.. note::

  - 若需要在无 LCD 的情况下开发，可以通过 `esp_lcd_usb_display 组件 <https://components.espressif.com/components/espressif/esp_lcd_usb_display>`_ 通过 USB UVC 在 PC 显示器上模拟 LCD 的显示效果，从而调试应用程序。其对应示例为 `usb_lcd_display <https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_lcd_display>`_