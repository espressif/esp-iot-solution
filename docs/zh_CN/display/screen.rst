显示屏
===========
:link_to_translation:`en:[English]`

许多应用需要把信息显示给用户，因此显示屏是很重要的一个显示设备。ESP32/ESP32-S2 支持驱动包括 I2C、8080 并口和 SPI 接口的屏幕，已支持的屏幕控制器型号如下表所示：

+------------+----------------+------------+
| 控制器型号 | 最大分辨率     |    类型    |
+============+================+============+
|  NT35510   |     480 x 865  |   Color    |
+------------+----------------+------------+
|  ILI9806   |     480 x 865  |   Color    |
+------------+----------------+------------+
|  RM68120   |     480 x 865  |   Color    |
+------------+----------------+------------+
|  ILI9486   |     320 x 480  |   Color    |
+------------+----------------+------------+
|  ILI9341   |     240 x 320  |   Color    |
+------------+----------------+------------+
|  ST7789    |     240 x 320  |   Color    |
+------------+----------------+------------+
|  ST7796    |     320 x 480  |   Color    |
+------------+----------------+------------+
|  SSD1351   |     128 x 128  |   Color    |
+------------+----------------+------------+
|  SSD1306   |     128 x 64   |   Mono     |
+------------+----------------+------------+
|  SSD1307   |     128 x 39   |   Mono     |
+------------+----------------+------------+
|  SSD1322   |     480 x 128  |   Gray     |
+------------+----------------+------------+

.. note:: 8080 接口通过 ESP32 I2S 的 LCD 模式实现，故下文中有时也称之为 ``I2S 接口``。

驱动结构
----------

.. figure:: ../../_static/display/screen_driver_structure.png
   :align: center
   :width: 500px

   屏幕驱动结构框图

为了更加符合一个屏幕控制芯片拥有多个接口的实际情况，将屏幕驱动程序划分为 ``接口驱动`` 和 ``屏幕控制器驱动`` 两部分。

- 接口驱动：完成基本的命令和数据的读写
- 屏幕控制器驱动：通过接口驱动来完成屏幕的显示

在程序设计上，一个屏幕控制器驱动可以通过调用不同的接口驱动以实现切换硬件上不同的接口。

屏幕的分类
----------
对屏幕进行分类的讨论将有助于我们对驱动的理解，这里将按照屏幕可显示的色彩来分类，而非 OLED、LCD 等屏幕的面板材料。
一般情况下，屏幕显示的色彩决定了 BPP (Bits Per Pixel)，而 BPP 的不同导致程序的处理方式有一些不同，下面将更直观地列举几种 GRAM 映射到像素点的方式：

.. figure:: ../../_static/display/screen_driver_RGB565.png
   :align: center
   :width: 600px

   BPP = 16 GRAM 结构

.. figure:: ../../_static/display/screen_driver_mono.png
   :align: center
   :width: 600px

   BPP = 1 GRAM 结构

.. figure:: ../../_static/display/screen_driver_gray.png
   :align: center
   :width: 600px

   BPP = 4 GRAM 结构

从以上图中可以看出，映射方式大概可以分为两类：

- BBP >= 8，通常是支持 RGB888、RGB666、RGB565 等编码的彩色屏幕。
- BPP < 8，通常是单色的屏幕，可能是黑白的，也可能是灰阶的。

BPP < 8 时，一个字节映射到了多个像素，因此无法直接地控制单个像素。这时，驱动不支持 :c:func:`draw_pixel` 函数，且 :c:func:`set_window` 的参数也将受到限制。BPP >= 8 时，则可以轻松地访问单个像素。


.. attention:: 对于彩色屏幕，驱动仅支持 RGB565 颜色编码。

接口驱动
-----------

一个屏幕控制器通常有多种接口，在 ESP32 上通常使用 ``8080 并口``、``SPI`` 和 ``I2C`` 这三种接口与屏幕连接，可以在调用 :c:func:`scr_interface_create` 创建接口驱动时选用其中一种接口。

.. note:: 使用 :c:func:`scr_interface_create` 创建不同类型的接口时需要注意传入与之对应的配置参数类型，例如 I2S 接口是 :cpp:type:`i2s_lcd_config_t` 而 SPI 接口是 :cpp:type:`scr_interface_spi_config_t`。

为了方便在屏幕控制器驱动中统一使用这些接口，:component_file:`display/screen/screen_utility/interface_drv_def.h` 中定义了所有接口，可以使用简单的参数调用接口驱动。

.. note:: 大部分屏幕是大端模式，而 ESP32 是小端模式，因此可在使用的接口驱动中根据 ``swap_data`` 配置可选择进行大小端转换。**请注意：** 当使用 SPI 接口时，由于 IDF 的 SPI 驱动内部没有提供该功能，接口驱动将会对传入数据进行转换，这要求传入的数据是可写的，因此数据 **必须** 存放在 RAM 中。

屏幕控制器驱动
----------------

该部分将根据不同的屏幕控制器分别实现显示等功能，为了方便地移植到不同 GUI 库，对屏幕的一部分通用函数用 :cpp:type:`scr_driver_t` 进行了抽象。对于一些屏幕的非通用功能，需要自行调用其特定的函数完成。

对于这些通用函数，由于屏幕控制器本身的功能不尽相同，并不是所有的屏幕都全部实现了，例如：对于 BPP < 8 的屏幕不支持 :c:func:`draw_pixel` 函数。调用不支持的函数将返回 :cpp:enumerator:`ESP_ERR_NOT_SUPPORTED`。

显示方向
^^^^^^^^^

这里设置的屏幕显示方向是完全由屏幕硬件实现的，这个功能在不同的屏幕控制器上会有差异。一共有 8 种可能的显示方向，显示器可以旋转 0°、90°、180° 或 270°，也可以从顶部或底部查看，默认方向为 0° 和顶部查看。这 8 (4 × 2) 个不同的显示方向也可以表示为 3 个二进制开关的组合：X-mirroring、Y-mirroring 和 X/Y swapping。

下表将列出全部 8 种组合显示的方向。如果显示方向不正常，请查看下表中的配置开关使其正常工作。

==================  ======================  ====================  ===========================
|original|           0                      |mirror_y|             SCR_MIRROR_Y             
                     [SCR_DIR_LRTB]                                [SCR_DIR_LRBT]
------------------  ----------------------  --------------------  ---------------------------
|mirror_x|           SCR_MIRROR_X           |mirror_xy|            SCR_MIRROR_X|             
                     [SCR_DIR_RLTB]                                SCR_MIRROR_Y 
                                                                   [SCR_DIR_RLBT]
------------------  ----------------------  --------------------  ---------------------------
|swap_xy|            SCR_SWAP_XY            |swap_xy_mirror_y|     SCR_SWAP_XY|
                     [SCR_DIR_TBLR]                                SCR_MIRROR_Y  
                                                                   [SCR_DIR_BTLR]
------------------  ----------------------  --------------------  ---------------------------
|swap_xy_mirror_x|   SCR_SWAP_XY|           |swap_xy_mirror_xy|    SCR_SWAP_XY|
                     SCR_MIRROR_X                                  SCR_MIRROR_X|
                     [SCR_DIR_TBRL]                                SCR_MIRROR_Y  
                                                                   [SCR_DIR_BTRL]
==================  ======================  ====================  ===========================

.. |original| image:: ../../_static/display/original.png
              :height: 50px
              :width: 100px

.. |mirror_y| image:: ../../_static/display/mirror_y.png
              :height: 50px
              :width: 100px
.. |mirror_x| image:: ../../_static/display/mirror_x.png
              :height: 50px
              :width: 100px
.. |mirror_xy| image:: ../../_static/display/mirror_xy.png
              :height: 50px
              :width: 100px

.. |swap_xy| image:: ../../_static/display/swap_xy.png
              :height: 100px
              :width: 50px
.. |swap_xy_mirror_x| image:: ../../_static/display/swap_xy_mirror_x.png
              :height: 100px
              :width: 50px
.. |swap_xy_mirror_y| image:: ../../_static/display/swap_xy_mirror_y.png
              :height: 100px
              :width: 50px
.. |swap_xy_mirror_xy| image:: ../../_static/display/swap_xy_mirror_xy.png
              :height: 100px
              :width: 50px

对于不同屏幕控制器，屏幕显示方向的实现并不完全相同，通常分为以下的情况：

    - 对于彩色屏幕，支持 8 个方向的旋转。
    - 对于单色屏幕，如 SSD1306 等屏幕来说，只支持 :cpp:type:`scr_dir_t` 中定义的前 4 个方向，即不支持交换 XY 轴。

.. note:: 
    显示方向还和使用的屏幕面板有关系，你可能会发现两种异常的情况：

    - 显示方向设置为 :cpp:enumerator:`SCR_DIR_LRTB`，屏幕却不是按照上表中对应的显示方向。这可能是因为屏幕面板上的走线对 X/Y 方向上进行了镜像，这时应该根据实际情况调整旋转以得到期望的显示方向。
    - 旋转了屏幕后，显示内容不见了。这可能是因为屏幕面板的分辨率小于屏幕控制器的分辨率，导致旋转后显示区域没有完全落在屏幕面板上，这时应考虑设置正确的显示区域偏移。

显示区域的偏移
^^^^^^^^^^^^^^^^^^

在一些小尺寸的屏幕上，通常其可视区域分辨率小于所用屏幕控制器的分辨率。请参考以下示例图：

.. image:: ../../_static/display/screen_offset.png
    :align: center
    :width: 350px

图中 ``Controller window`` 是屏幕控制器的显示窗口，分辨率为 240 × 320，``Panel window`` 是屏幕面板窗口，分辨率为 135 × 240，可视的区域为屏幕面板区域。可以看出显示区域在水平方向上偏移了 52 个像素，在垂直方向上偏移了 40 个像素。

当屏幕逆时针旋转 90° 后，变成水平方向上偏移了 40 个像素，在垂直方向上偏移了 53 个像素，如下所示：

.. image:: ../../_static/display/screen_offset_rotate.png
    :align: center
    :width: 420px

屏幕控制器驱动会帮助你自动根据屏幕的旋转来改变偏移的值，以保持正确的显示区域。你需要做的是在 :cpp:type:`scr_controller_config_t` 中正确配置屏幕在 ``SCR_DIR_LRTB`` 方向时的偏移和屏幕面板尺寸。

.. note:: 

    - 显示偏移仅支持 BPP >= 8 的屏幕。
    - 当屏幕控制器是可选分辨率的时候，发现偏移不对可能是因为选择的分辨率与实际不符，此时应该修改程序，如： ILI9806 可尝试修改 ``ili9806.c`` 中的 ``ILI9806_RESOLUTION_VER`` 为实际的分辨率。

应用示例
------------

初始化屏幕
^^^^^^^^^^^

.. code:: c

    scr_driver_t g_lcd; // A screen driver
    esp_err_t ret = ESP_OK;

    /** Initialize 16bit 8080 interface */
    i2s_lcd_config_t i2s_lcd_cfg = {
        .data_width  = 16,
        .pin_data_num = {
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
        },
        .pin_num_cs = 45,
        .pin_num_wr = 34,
        .pin_num_rs = 33,
        .clk_freq = 20000000,
        .i2s_port = I2S_NUM_0,
        .buffer_size = 32000,
        .swap_data = false,
    };
    scr_interface_driver_t *iface_drv;
    scr_interface_create(SCREEN_IFACE_8080, &i2s_lcd_cfg, &iface_drv);

    /** Find screen driver for ILI9806 */
    ret = scr_find_driver(SCREEN_CONTROLLER_ILI9806, &g_lcd);
    if (ESP_OK != ret) {
        return;
        ESP_LOGE(TAG, "screen find failed");
    }

    /** Configure screen controller */
    scr_controller_config_t lcd_cfg = {
        .interface_drv = iface_drv,
        .pin_num_rst = -1,      // The reset pin is not connected
        .pin_num_bckl = -1,     // The backlight pin is not connected
        .rst_active_level = 0,
        .bckl_active_level = 1,
        .offset_hor = 0,
        .offset_ver = 0,
        .width = 480,
        .height = 854,
        .rotate = SCR_DIR_LRBT,
    };

    /** Initialize ILI9806 screen */
    g_lcd.init(&lcd_cfg);

.. note::

    默认情况下只打开了 ILI9341 屏幕的驱动，如果要使用其他的驱动，需要在 ``menuconfig -> Component config -> LCD Drivers -> Select Screen Controller`` 中使能对应屏幕驱动。

显示图像
^^^^^^^^^^^

.. code:: c

    /** Draw a red point at position (10, 20) */
    lcd.draw_pixel(10, 20, COLOR_RED);

    /** Draw a bitmap */
    lcd.draw_bitmap(0, 0, width_of_pic, height_of_pic, pic_data);

获取屏幕信息
^^^^^^^^^^^^^

.. code:: c

    scr_info_t lcd_info;
    lcd.get_info(&lcd_info);
    ESP_LOGI(TAG, "Screen name:%s | width:%d | height:%d", lcd_info.name, lcd_info.width, lcd_info.height);

API 参考
-----------------

.. include:: /_build/inc/screen_driver.inc

.. include:: /_build/inc/scr_interface_driver.inc
