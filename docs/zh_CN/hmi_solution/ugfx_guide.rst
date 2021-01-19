:orphan:

μGFX Guide
==========

:link_to_translation:`en:[English]`

μGFX 简介
---------

1. 什么是 μGFX？

   `μGFX <https://ugfx.io/>`__ 是用于显示屏和触摸屏的轻量级嵌入式 GUI
   库，可构建全功能嵌入式
   GUI。该库非常小并且速度很快，因为每个未使用的功能都被禁用并且不会链接到已完成的二进制文件中。μGFX
   具有以下优点：

   -  轻量级
   -  模块化
   -  便携式
   -  完全开源
   -  开发活跃

2. μGFX 库提供完整的解决方案

   μGFX 被设计为用于嵌入式显示屏和触摸屏最小、最快、最先进的 GUI
   库。具有以下特点：

   -  小巧轻便
   -  完全可定制和可扩展
   -  高度便携
   -  支持所有显示类型：单色、灰度、全彩色显示
   -  支持硬件加速
   -  超过 50 个即用型驱动程序
   -  用 C 语言编写，可以与 C++ 一起使用
   -  免费用于非商业用途
   -  完全透明：整个源代码是开放的
   -  适用于低 RAM 系统。大多数显示屏不需要帧缓冲
   -  完全多线程可重入。绘图可以随时从任何线程进行！

3. 桌面 GUI 设计工具

   μGFX-Studio 是一个桌面应用程序，使用它可以快速设计用户图形界面。

   μGFX-Studio 可在 Windows，Linux 和 Mac OS X 上运行。μGFX-Studio
   目前还未完全开放。

μGFX 模块
~~~~~~~~~

μGFX 库包含以下模块
 
- `GWIN`_
   - GWIN模块使用其他模块，并将它们组合成一个完整的 GUI工具包。它提供了按钮、复选框、列表等图形控件。 
- GAUDIO 
   - GAUDIO提供用于处理音频输入和输出的API。该模块提供内置解码器，也允许使用外部编解码器。 
- GFILE 
   - GFILE提供独立于操作系统的读写文件的方式，无论它们是存储在 RAM，FLASH，SD卡还是主机操作系统自带的文件系统中。 
- GOS 
   - GOS 是在 μGFX和底层系统之间构建抽象层的模块。底层系统可以是 RTOS，如 ChibiOS 或FreeRTOS，也可以只是裸机系统。
- `GDISP`_
    - GDISP模块为图形显示提供了一个简单的界面，并提供了一组功能丰富的API，用于绘制线条，圆形和矩形等原始形状，以及字体渲染，剪切和其他高级功能。
- GINPUT 
   - GINPUT提供了一种简单的方法来将不同的硬件外围设备（如触摸屏）连接到库的其余部分。GINPUT提供了触摸屏的校准程序。 
- GTRANS 
   - GTRANS模块允许您管理语言翻译。可以使用 GTRANS模块在运行时动态更改应用程序的语言。 
- GEVENT 
   - GEVENT模块是一个非常强大的事件系统。它提供了一种在源和侦听器之间创建多对多关系的方法。
- GTIMER 
   - GTIMER 模块提供高级的、简单的和不依赖于硬件的定时器。 
- GQUEUE 
   - GQUEUE 模块提供多种队列和缓冲区的实现。

GWIN
^^^^

GWIN 模块包含： 

- `Windows`_
   - Console 
   - Graph window 
- `Widgets`_
   - Label 
   - PushButton
   - RadioButton 
   - CheckBox 
   - List 
   - Slider 
   - Progressbar 
   - ImageBox 
   - TextEdit 
   - Keyboard 
- `Containers`_
   - Container 
   - Tabset 
   - Frame

GDISP
^^^^^

GDISP 模块包含： 
- Drawing 
- Font rendering 
- Images

License
~~~~~~~

Espressif 已经获得 μGFX 的商业许可，使用 Espressif
系列芯片的用户，可以免费使用 μGFX 提供的驱动和服务。有关 µGFX License
请查阅 `License <https://ugfx.io/license.html>`__\ 。

µGFX 使用
---------

iot-solution 中已经做了一些驱动适配，驱动路径：
``components/hmi/gdrivers``\ 。

在基于 iot-solution 的工程中使用 µGFX 的步骤：

1. 搭建 iot-solution
   环境：\ `Preparation <https:404#preparation>`__
2. 在工程源代码中添加头文件 ``#include "iot_ugfx.h"``
3. 在 ``menuconfig`` 中使能 µGFX GUI
   （\ ``IoT Solution settings > IoT Components Management > HMI components > uGFX GUI Enable``\ ）
4. 在 ``menuconfig`` 中进行 µGFX GUI `相关配置 <#µgfx-配置>`__
   （\ ``IoT Solution settings > IoT Components Management > HMI components > uGFX Settings``\ ）
5. 根据示例工程 ``ugfx_example`` 所示完成 µGFX 的初始化
6. 根据实际工程进行 GUI 的开发

µGFX 相关 `API Reference <https://api.ugfx.io/>`__

µGFX 配置
~~~~~~~~~

在 iot-solution 中进行 µGFX 配置主要有两种方式：

1. 在 ``menuconfig`` 中进行 µGFX 配置

   对于部分使用频率较高的配置选项，将其添加到 ``menuconfig``
   中以便于配置。例如：驱动配置、触摸屏使能、屏幕分辨率、旋转方向等。µGFX
   配置菜单位于
   ``IoT Solution settings > IoT Components Management > HMI components > uGFX Settings``\ 。

2. 修改 ``gfxconf.h`` 文件进行 µGFX 配置

   μGFX 的所有项目的特定选项都在文件 ``gfxconf.h`` 中定义，该文件在
   ``esp-iot-solution/components/hmi/gdrivers/include/gfxconf.h``\ ，用户可自行修改。在每个部分中，第一个选项为启用或禁用整个模块。该部分以下所有子选项仅在启用模块时生效。详细的
   ``gfxconf.h`` 文件说明，请看
   `Configuration <https://wiki.ugfx.io/index.php/Configuration>`__\ 。

``menuconfig`` 中 µGFX 的配置选项，如下图所示：

.. figure:: ../../_static/hmi_solution/ugfx/ugfx_menuconfig.jpg
    :align: center

    图 1. µGFX menuconfig

1. 驱动配置

   在 µGFX Settings
   菜单中可以选择显示屏和触摸屏的驱动。路径：\ ``Config Driver->Choose Touch Screen Driver``
   和 ``Config Driver->Choose Screen Driver``\ 。

2. 触摸屏使能

   在 µGFX Settings
   菜单中可以选择使能或禁止触摸屏。路径：\ ``uGFX Touch Screen Enable``\ 。

3. 屏幕分辨率

   在 µGFX Settings
   菜单中可以选择显示屏的屏幕分辨率。路径：\ ``Config Driver->uGFX Screen Width (pixels)``
   和 ``Config Driver->uGFX Screen Height (pixels)``\ 。

4. 旋转方向

   在 µGFX Settings
   菜单中可以选择显示屏旋转的方向。路径：\ ``Choose Screen Rotate``\ 。

显示驱动模式
~~~~~~~~~~~~

µGFX 显示驱动程序可以属于以下三种模式之一。与桌面图形处理器不同，嵌入式
LCD
通常具有不同的访问模式，这意味着传统的图形库根本不支持它们。某些图形控制器在不同情况下可能需要不同的模式。

1. Framebuffer 模式

   这是大多数图形库支持的模式，最适合高级图形处理器。它要求图形硬件提供一个帧缓冲器，它是一块
   RAM，可以作为 CPU 的普通存储器进行像素寻址。然后，图形硬件通过查看
   CPU 对帧缓冲区所做的更改来在后台更新显示。这也是 µGFX
   中支持的硬件类型。

   许多其他图形库试图通过将系统 RAM
   分配给虚拟帧缓冲区，然后提供同步调用，以将帧缓冲区刷新到真实显示屏，来支持其他类型的硬件。这里有一些问题，例如：

   -  它分配了大量的系统 RAM，这通常是嵌入式环境中的宝贵资源，并且；
   -  同步调用通常非常低效，因为必须更新整个显示屏或必须进行差异比较。

   可能存在需要同步的其他原因（例如：仅允许在垂直刷新期间更新显示），因此
   µGFX
   仍支持同步调用。但建议您不要使用此模式，除非您的图形硬件支持本地帧缓冲。

   需要实现的函数：

   -  ``board_init()`` - 初始化帧缓冲区并返回其地址和显示属性

   可选的函数：

   -  ``board_flush()`` - 将帧缓冲区刷新（同步）到显示屏
   -  ``board_backlight()`` - 调整显示屏背光
   -  ``board_contrast()`` - 调整显示对比度
   -  ``board_power()`` - 进入/退出睡眠模式

2. Window 模式

   大多数嵌入式 LCD
   都使用这种模式的控制器。不幸的是，大多数图形库都不能有效地支持这些控制器。

   在该模式中，硬件提供可编程窗口区域。通过顺序地将像素发送到图形控制器来写入该窗口区域。当像素到达窗口中一行的末尾时，控制器将换行到窗口中下一行的开头。当它到达窗口的底部时，它可能（或可能不）回绕到窗口的开头。

   通常不支持从显示屏读取，如果是，则使用相同的窗口方法。

   由于显示屏不是 RAM 可寻址的，且物理连接通常是通过慢速总线（至少与 RAM
   寻址相比），如 SPI，I2C
   或字节并行，因此读写速度可能很慢。这意味着绘图操作的效率非常重要，并且与帧缓冲相比，需要使用完全不同的绘图方法。µGFX
   自动处理所有这些差异。

   这些控制器无法实现仅软件屏幕旋转（与帧缓冲区不同）。需要一些硬件支持。通过旋转光标在绘图窗口中移动的方式，或通过相对于内部帧缓冲旋转显示屏本身，有两种可能的方法。根据策略，如果控制器支持两者，我们更喜欢实现第一种方法。此首选项允许保留现有显示内容，旋转仅影响新的绘图操作（对最终用户应用程序更灵活）。

   需要实现的函数：

   -  ``gdisp_lld_init()`` - 初始化控制器和显示
   -  ``gdisp_lld_write_start()`` - 启动窗口写入操作
   -  ``gdisp_lld_write_color()`` - 将一个像素发送到当前位置的当前窗口
   -  ``gdisp_lld_write_stop()`` - 停止窗口写操作

   可选的函数：

   -  ``gdisp_lld_write_pos()`` -
      在写入窗口内设置当前位置（提高绘图效率）
   -  ``gdisp_lld_read_start()`` - 启动窗口化读取操作
   -  ``gdisp_lld_read_color()`` - 从当前位置的当前窗口读取一个像素
   -  ``gdisp_lld_read_stop()`` - 停止窗口化读取操作
   -  ``gdisp_lld_set_clip()`` -
      设置硬件剪辑区域。所有的写入都被剪切到此区域（无论当前窗口如何）
   -  ``gdisp_lld_control()`` -
      处理背光，对比度，屏幕旋转方向和驱动程序特定的控制命令
   -  ``gdisp_lld_query()`` - 查询一些驱动程序特定的变量值
   -  任意 Point and Block 模式函数（如下所述）

3. Point and Block 模式

   在该模式中，控制器提供基本的绘图操作，例如画点，填充块，从图像填充块。许多相同的考虑适用于上面的窗口模式。通常不支持从显示屏读取。

   驱动程序可以将此模式中的函数混合到上面的 Window 模式中。如果在 Window
   模式驱动程序中提供了特定的画点，填充块或图像填充块函数，则它将优先于上面的一般
   Window 模式调用。当这样的调用混合时，驱动程序仍然被认为是一个 Window
   模式驱动程序。例如：控制器可以有更有效的画点命令，其可以优先于 Window
   模式中单像素写入方式使用。

   需要实现的函数：

   -  ``gdisp_lld_init()`` - 初始化控制器和显示
   -  ``gdisp_lld_draw_pixel()`` - 设置一个像素

   可选的函数：

   -  ``gdisp_lld_fill_area()`` - 用颜色填充块
   -  ``gdisp_lld_blit_area()`` - 从像素数组中填充块
   -  ``gdisp_lld_vertical_scroll()`` - 向上或向下滚动显示屏的窗口区域
   -  ``gdisp_lld_get_pixel_color()`` - 获取单个像素的颜色
   -  ``gdisp_lld_set_clip()`` -
      设置硬件剪辑区域。所有的写入都被剪切到此区域
   -  ``gdisp_lld_control()`` -
      处理背光，对比度，屏幕旋转方向和驱动程序特定的控制命令
   -  ``gdisp_lld_query()`` - 查询一些驱动程序特定的变量值

设置字体
~~~~~~~~

1. 字体用法

   在使用字体之前，首先必须通过调用 ``gdispOpenFont()`` 函数来打开字体。

   例：\ ``font_t font = gdispOpenFont("DejaVuSans32_aa");``

       如果找不到指定的字体名称，将使用配置文件中最后一个启用的字体。\ ``gdispOpenFont("*");``
       表示打开第一个启用的字体。

   如果您不再需要字体，则应调用 ``gdispCloseFont(font)``
   函数以释放所有已分配的资源。

   打开字体后，可以将字体变量传递给任何带有字体参数的 API。在查看不同的
   GWIN 系统之前，您可以先阅读基本的 GDISP 文本绘制函数。

2. 设置默认字体

   调用 ``gwinSetDefaultFont(font_t font)`` 函数设置所有 GUI
   元素的默认字体。

   示例：

   ::

       #include "iot_ugfx.h"

       static font_t font;

       int main(void) {
       // Initialize uGFX and the underlying system
       gfxInit();
       // Set the widget defaults
       font = gdispOpenFont("DejaVuSans16");
       gwinSetDefaultFont(font);
       }

3. 设置某个 GUI 元素的字体

   调用 ``gwinSetFont(GHandle gh, font_t font)`` 函数设置某个 GUI
   元素的字体。

4. µGFX 现有字体

   可以通过 μGFX 显示 ``.ttf`` 或 ``.bdf`` 格式的字体。但是，µGFX
   已经添加了一些不同大小和版本的字体，可以涵盖大多数工程。使用字体名称作为
   ``gdispOpenFont()`` 函数的参数。

   请注意，必须在配置文件中启用这些字体。UI 字体由 μGFX
   开发人员创建的默认字体。

+------------------------------------+------------------------+
| **Font**                           | **Font name**          |
+====================================+========================+
| DejaVu Sans 10                     | DejaVuSans10           |
+------------------------------------+------------------------+
| DejaVu Sans 12                     | DejaVuSans12           |
+------------------------------------+------------------------+
| DejaVu Sans 12 Bold                | DejaVuSansBold12       |
+------------------------------------+------------------------+
| DejaVu Sans 12 Anti-Aliased        | DejaVuSans12\_aa       |
+------------------------------------+------------------------+
| DejaVu Sans 12 Anti-Aliased Bold   | DejaVuSansBold12\_aa   |
+------------------------------------+------------------------+
| DejaVu Sans 16                     | DejaVuSans16           |
+------------------------------------+------------------------+
| DejaVu Sans 16 Anti-Aliased        | DejaVuSans16\_aa       |
+------------------------------------+------------------------+
| DejaVu Sans 20                     | DejaVuSans20           |
+------------------------------------+------------------------+
| DejaVu Sans 20 Anti-Aliased        | DejaVuSans20\_aa       |
+------------------------------------+------------------------+
| DejaVu Sans 24                     | DejaVuSans24           |
+------------------------------------+------------------------+
| DejaVu Sans 24 Anti-Aliased        | DejaVuSans24\_aa       |
+------------------------------------+------------------------+
| DejaVu Sans 32                     | DejaVuSans32           |
+------------------------------------+------------------------+
| DejaVu Sans 32 Anti-Aliased        | DejaVuSans32\_aa       |
+------------------------------------+------------------------+
| Fixed 10x20                        | fixed\_10x20           |
+------------------------------------+------------------------+
| Fixed 7x14                         | fixed\_7x14            |
+------------------------------------+------------------------+
| Fixed 5x8                          | fixed\_5x8             |
+------------------------------------+------------------------+
| UI1                                | UI1                    |
+------------------------------------+------------------------+
| UI1 Double                         | UI1 Double             |
+------------------------------------+------------------------+
| UI1 Narrow                         | UI1 Narrow             |
+------------------------------------+------------------------+
| UI2                                | UI2                    |
+------------------------------------+------------------------+
| UI2 Double                         | UI2 Double             |
+------------------------------------+------------------------+
| UI2 Narrow                         | UI2 Narrow             |
+------------------------------------+------------------------+
| Large numbers                      | LargeNumbers           |
+------------------------------------+------------------------+

显示图像
~~~~~~~~

RAM 使用
^^^^^^^^

GDISP
模块带有内置图像解码器。解码器允许它打开各种格式的图像并显示它。由于
GFILE
模块在内部使用，因此图像可以存储在不同的位置上，例如内部闪存或外部存储器，如
SD 卡。

图像解码器需要使用 RAM 来解码和显示图像。尽管 µGFX
的图像处理程序是从零开始编写以尽可能较少的使用 RAM，但对于 RAM
有限的微控制器，仍应谨慎选择要使用的图像格式。与大多数其他图像解码器一样，图像处理程序不分配
RAM
来存储完整的解压缩位图，而是在需要显示图像时再次对图像进行解码。因此，唯一使用
RAM 的是： - 一些 RAM 用于保存图像本身的信息，通常为 200 到 300
个字节。打开图像时保持此
RAM。对于某些具有特定图像格式的图像（具有调色板等），它可能略有不同。 -
在解码过程中分配的 RAM，并在解码完成后释放。GIF 图像格式需要大约 12 KB
的 RAM 来解码图像。BMP 和 NATIVE 图像不需要任何额外的 RAM 进行解码。 -
如果您决定缓存图像，则完整解码图像需要
RAM。对于低内存微处理器，不应考虑这一点。例如：在每像素 2
个字节的显示屏上缓存 320x240 图像将需要 150 KB 的 RAM （加上正常的解码
RAM ）。 -
堆栈空间。如果在尝试解码图像时遇到异常，则可能需要增加可用堆栈空间。某些图像格式需要几百字节的堆栈空间来解码图像。

µGFX
的图像解码器是从零开始编写的而没有采用现有的解码库，以保持图像解码器尽可能精简和一致。µGFX
目前的解码器比其他可用的解码库所使用的 RAM 少很多。

缓存
^^^^

可以调用 ``gdispImageCache()`` 函数将解码后的图像缓存到 RAM
中。如果不缓存图像，将始终从闪存中重新读取、解码然后显示。使用缓存图像时，只需从
RAM 加载并显示即可。这种方式更快，特别是对于 PNG，JPG 和 GIF
格式，因为这些需要非常复杂的解码算法。但是，缓存图像需要大量的
RAM。特别是当您使用多帧 GIF 图像或大尺寸图像时。

如果需要缓存图像，则必须先打开图像，然后才能显示图像。当关闭图像时，它将释放解码器使用的所有内存，包括缓存的图像。

调用缓存函数并不能保证正确缓存图像。例如：当没有足够的 RAM
可用，则不会缓存图像。在这种情况下，由于缓存完全是可选的，因此在调用绘图函数时仍然可以通过从闪存中重新读取、解码图像进行绘制。

ROMFS 文件系统下的图像文件
^^^^^^^^^^^^^^^^^^^^^^^^^^

ROMFS - 在代码本身中存储文件的文件系统（通常在 ROM/FLASH 中）。

使用 file2c 工具将图像文件转为头文件，file2c 工具位于
``esp-iot-solution/components/hmi/ugfx_gui/ugfx/tools/file2c/src``\ 。

首先进入 file2c 工具所在目录下，运行 make 命令，之后运行
``./file2c -dcs image_flie header_flie``\ ，请替换
``image_flie``\ ，\ ``header_flie`` 为真实的文件名。

在工程的 ``romfs_files.h`` 文件中添加该头文件，便可使用该图像。

图像格式
^^^^^^^^

μGFX 目前有以下图像解码器：

+------------+--------------------------------------------------------------------+
| **格式**   | **描述**                                                           |
+============+====================================================================+
| BMP        | 包括 BMP1, BMP4, BMP4\_RLE, BMP8, BMP8\_RLE, BMP16, BMP24, BMP32   |
+------------+--------------------------------------------------------------------+
| GIF        | 包括透明度和多帧支持（动画）                                       |
+------------+--------------------------------------------------------------------+
| PNG        | 包括透明度和 alpha 支持                                            |
+------------+--------------------------------------------------------------------+
| NATIVE     | 使用显示驱动程序原始格式                                           |
+------------+--------------------------------------------------------------------+

示例：

::

    #include "iot_ugfx.h"

    /**
     * The image file must be stored on a GFILE file-system.
     * Use either GFILE_NEED_NATIVEFS or GFILE_NEED_ROMFS (or both).
     *
     * The ROMFS uses the file "romfs_files.h" to describe the set of files in the ROMFS.
     */

    static gdispImage myImage;

    int main(void) {
      coord_t   swidth, sheight;

      // Initialize uGFX and the underlying system
      gfxInit();

      // Get the display dimensions
      swidth = gdispGetWidth();
      sheight = gdispGetHeight();

      // Set up IO for our image
      gdispImageOpenFile(&myImage, "myImage.bmp");
      gdispImageDraw(&myImage, 0, 0, swidth, sheight, 0, 0);
      gdispImageClose(&myImage);

      while(1) {
        gfxSleepMilliseconds(1000);
      }
      return 0;
    }

有关详细介绍，请查阅
`Images <https://wiki.ugfx.io/index.php/Images>`__\ 。

默认控件介绍
~~~~~~~~~~~~

µGFX GUI 的所有默认控件都在 GWIN 模块中，如上 `GWIN <#gwin>`__
中所示，包含有 Windows、Widgets、Containers 三大部分。

Windows
^^^^^^^

Window 是最基本的 GWIN
元素。所有其他元素（Widgets，Containers）都基于此。Window 包含以下属性：
- 位置 - 大小 - 前景色 - 背景色 - 字体

因此，Window 是完全被动的元素。它不接受任何类型的输入。

以下是当前实现的 Windows：

1. Console

   Console 是具有前景色和背景色的矩形窗口。Console
   不接受任何输入，只能调用 ``gwinPrintf()`` 函数输出文本。Console
   能够在文本到达窗口末尾时处理换行符，并且当窗口已经填充满，在底部插入新行时，它还可以处理文本滚动。

   Console 示例：
   ``/esp-iot-solution/components/hmi/ugfx_gui/ugfx/demos/modules/gwin/console``

2. Graph

   Graph
   允许在矩形窗口中轻松绘制具有不同颜色和形状的曲线和其他数据集。Graph
   window 不接受任何用户输入。

   Graph 示例：
   ``/esp-iot-solution/components/hmi/ugfx_gui/ugfx/demos/modules/gwin/graph``

Widgets
^^^^^^^

Widget 基于 Window。除了 Window 的功能外，它还实现了以下功能： - Widget
含有文本 - Widget 可以重绘自己 - Widget 能够接受用户输入，例如：触摸屏 -
Widget
可以覆盖其绘图函数。例如：有预定义的按钮绘制可绘制为圆形按钮、图像按钮、箭头按钮等，还有普通的按钮绘制函数
- Widget 支持样式。通过更改样式，您可以改变用于绘制控件的颜色，类似于在
Windows 和 Linux 中应用颜色方案的方式

以下是当前实现的 Widgets：

1.  Label

    Label 是一个简单的矩形控件，不需要输入。Label
    会自动重绘已更改的文本。如果 Label
    小于其显示的文本，则会剪切文本。可以调用 ``gwinSetText()`` 函数设置
    Label 的文本。

2.  PushButton

    PushButton 是一个独立的控件，具有静态大小和文本，其中文本以
    PushButton 所在区域为中心绘制。PushButton 可以是按下或未按下状态。

3.  RadioButton

    RadioButton 是一个只能工作在包含两个或多个 RadioButton
    的组中的控件。在这组 RadioButton 内，一次只能选中一个
    RadioButton。当您单击另一个 RadioButton 时，当前选中的一个
    RadioButton 会被取消选中，新的一个 RadioButton 变为选中状态。

4.  CheckBox

    CheckBox 是一个独立的 GUI
    元素，它只有已被选中和未被选中两个状态。默认情况下，CheckBox
    的左侧会显示 CheckBox 的文本。CheckBox 控件的宽度是包含文本的宽度。

5.  List

    List
    是一个矩形控件，其中包含多个列表条目。列表条目是链接到每个列表唯一
    ID
    的简单字符串。可以有不同的输入方式，例如：使用触摸屏可以直接触摸来选择列表条目。此外，如果有多个列表条目可以显示，则列表控件会自动在右侧显示向上和向下箭头以在列表中向上和向下滚动。如果触摸列表的空白部分（在最后一个条目下方），则重置所有选择。列表可以是单选也可以是多选。此外，可以在列表条目字符串的左侧添加一个图像显示。可包含两个图像
    - 一个用于选中，另一个用于未选中。

    下图显示了列表控件的默认绘图方式。请注意，此时您可以使用所需的自定义渲染函数替换它们。左侧的第一个列表是显示滚动条的普通单选列表。中间的第二个列表是多选列表
    -
    也带有滚动条。右侧的第三个图像显示了一个列表，其中的图像在列表条目文本的前面绘制。这是一个没有滚动条的多选列表，因为所有列表项都可以显示在列表的空间内。

    .. figure:: ../../_static/hmi_solution/ugfx/ugfx_gwin_list.jpg
         :align: center

         图 2. List Widget

    您可以使用调用 ``gwinListViewItem()`` 函数让列表条目在列表中可见。

6.  Slider

    Slider 是条形的 GUI
    元素，滑条可以从最低（0）移动到最高（100）值。Slider 的文本显示在
    Slider 的正中央。

7.  Progressbar

    Progressbar 是一个矩形框，用于显示操作的进度。可以手动或自动控制
    Progressbar 控件。在这两种情况下，可以通过调用
    ``gwinProgressbarSetRange()`` 函数更改 Progressbar 的范围。默认值为
    0 到 100。此外，可以通过调用 ``gwinProgressbarSetResolution()``
    函数修改分辨率。这会更改 Progressbar
    增加或减少每一步的大小。默认分辨率为 1。

8.  ImageBox

    ImageBox 简单地采用 GDISP 图像解码器功能并将它们包裹在 GWIN 控件中。

    ImageBox 示例：
    ``/esp-iot-solution/components/hmi/ugfx_gui/ugfx/demos/modules/gwin/imagebox``

9.  TextEdit

    TextEdit 控件允许用户使用 GUI
    输入文本。文本输入源可以是一个物理键盘（或小键盘）通过 GINPUT
    模块或虚拟屏幕键盘。

    TextEdit 示例：
    ``/esp-iot-solution/components/hmi/ugfx_gui/ugfx/demos/modules/gwin/textedit``

10. Keyboard

    Keyboard
    控件提供虚拟屏幕键盘。可以动态更改键盘布局。该控件带有一组内置布局，如
    QWERTY 和 NumPad，但也可以定义自定义布局。

    Keyboard 示例：
    ``/esp-iot-solution/components/hmi/ugfx_gui/ugfx/demos/modules/gwin/keyboard``

    TextEdit 和 Keyboard 示例：
    ``/esp-iot-solution/components/hmi/ugfx_gui/ugfx/demos/modules/gwin/textedit_virtual_keyboard``

Widgets 示例：
``/esp-iot-solution/components/hmi/ugfx_gui/ugfx/demos/modules/gwin/widgets``

Containers
^^^^^^^^^^

Container 基于 Widget。Container 的主要特性是它可以包含子
Window。其中，子 Window 继承了父 Window 的属性。

以下是当前实现的 Container：

1. Container

   基本 Container
   可用于将其他控件组合在一起。它是一个简单的空白矩形，可以用作其他控件的父
   Windows。

   通过将 Container 的 GHandle
   添加到子控件的初始化结构体中，可以将控件作为子项添加到 Container 中。

   Container 示例：
   ``/esp-iot-solution/components/hmi/ugfx_gui/ugfx/demos/modules/gwin/container``

2. Frame

   Frame 控件基于
   Container。和电脑上的窗口类似。包含一个边框，一个窗口标题和一个可选的按钮。

   Frame 示例：
   ``/esp-iot-solution/components/hmi/ugfx_gui/ugfx/demos/modules/gwin/frame``

3. Tabset

   Tabset 是一个特殊的 Container 控件，用于管理不同的选项卡。和 Web
   浏览器中的选项卡非常类似。您可以根据需要创建任意数量的页面，并为每个页面添加控件。只有活动页面上的控件才会对用户可见。

   此控件通常用于创建基于标签的简单菜单。为此，Tabset
   放置在屏幕原点（\ ``x = 0``\ ，\ ``y = 0``\ ）并覆盖整个显示屏大小（\ ``width = gdispGetWidth()``\ ，\ ``height = gdispGetHeight()``\ ）。为避免在显示屏边缘绘制边框，在创建
   Tabset 时，需要将 ``gwinTabsetCreate()`` 函数的第一个参数置为 0。

   Tabset 示例：
   ``/esp-iot-solution/components/hmi/ugfx_gui/ugfx/demos/modules/gwin/tabset``

FAQs
----

在进行程序问题查找时，请检查每个函数的返回值以便追踪问题。

没有显示图像
~~~~~~~~~~~~

现象：程序编译能够通过，并且运行时没有提示任何错误，但没有显示图像。这种行为可能是由各种不同的问题引起的：

- 首先确认显示屏能正常驱动并显示基本控件，应选择正确的 IO 和驱动 
- 配置文件中尚未启用相应的解码器 
- 图像无法打开 
- 图像解码器无法分配足够的内存 
- 从文件加载图像时，可能已达到最大文件句柄数并且打开文件失败。在这种情况下，请在配置文件中增加 ``GFILE_MAX_GFILES`` 或关闭不再需要的已打开文件。
