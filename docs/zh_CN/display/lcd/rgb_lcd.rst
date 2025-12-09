RGB LCD 详解
===========================

:link_to_translation:`en:[English]`

.. contents:: 目录
    :local:
    :depth: 2

术语表
-----------

请参阅 :ref:`LCD 术语表 <LCD_术语表>` 。

接口模式
---------------------

大多数 RGB LCD 采用 ``SPI + RGB`` 接口，它们需要通过 ``SPI`` 接口发送命令对 LCD 进行初始化，也可以在初始化后根据需要动态修改相关配置，如垂直/水平镜像，更具灵活性。一些 RGB LCD 仅采用 ``RGB`` 接口，它们无需发送命令对 LCD 进行初始化，但也无法修改任何配置，驱动方法更加简单。下图为 *ST7701S* 的接口类型选择：

.. figure:: ../../../_static/display/screen/lcd_st7701_interface_type.png
    :align: center
    :scale: 80%
    :alt: ST7701S 的接口类型选择

    ST7701S 的接口类型选择

从上图中可以看出， *ST7701S* 是通过 ``IM[3:0]`` 引脚来选择 ``SPI + RGB`` 接口的配置。通常来说，这类 LCD 会选择 ``3-wire SPI + RGB`` 的接口类型，对应于上图中的 ``RGB+9b_SPI(rise/fall)``，其中， ``9b_SPI`` 表示 ``SPI`` 接口的 :ref:`3-line 模式 <spi_3/4-line_模式>` (一般称为 ``3-wire``)， ``rise/fall`` 表示 ``SCL`` 信号的有效边沿， ``rise`` 表示上升沿有效 (SPI 模式 0/3)， ``fall`` 表示下降沿 (SPI 模式 1/2)。

下图为 *ST7701S* 的 ``SPI`` 和 ``RGB`` 接口的引脚描述：

.. figure:: ../../../_static/display/screen/lcd_st7701_spi_pin.png
    :align: center
    :scale: 80%
    :alt: ST7701S SPI 接口的引脚描述

    ST7701S SPI 接口的引脚描述

.. figure:: ../../../_static/display/screen/lcd_st7701_rgb_pin.png
    :align: center
    :scale: 60%
    :alt: ST7701S RGB 接口的引脚描述

    ST7701S RGB 接口的引脚描述

注：RGB 引脚名称：CS、SCK(SCL)、SDA(MOSI)、HSYNC、VSYNC、PCLK、DE、D[23:0](D[17:0]/D[7:0])

对于采用 ``SPI + RGB`` 接口的 LCD，一般可以通过命令配置 ``RGB`` 接口为 ``DE 模式`` 或者 ``SYNC 模式``，下面以 *ST7701S* 为例介绍这两种模式。

模式选择
^^^^^^^^^^^^^^^^

.. figure:: ../../../_static/display/screen/lcd_st7701_rgb_select.png
    :align: center
    :scale: 60%
    :alt: ST7701S RGB 接口的模式选择

    ST7701S RGB 接口的模式选择

从图中可以看出， *ST7701S* 可以通过命令 ``C3h`` 配置 RGB 的模式。需注意，不同型号的 LCD 驱动 IC 可能使用不同的命令，如 *GC9503* 是通过命令 ``B0h`` 进行配置的。

DE 模式
^^^^^^^^^^^^^^^^

.. figure:: ../../../_static/display/screen/lcd_st7701_rgb_de.png
    :align: center
    :scale: 50%
    :alt: ST7701S DE 模式的时序图

    ST7701S DE 模式的时序图

SYNC 模式
^^^^^^^^^^^^^^^^

.. figure:: ../../../_static/display/screen/lcd_st7701_rgb_sync.png
    :align: center
    :scale: 50%
    :alt: ST7701S SYNC 模式的时序图

    ST7701S SYNC 模式的时序图

模式对比
^^^^^^^^^^^^^^^^

通过对比 ``DE 模式`` 和 ``SYNC 模式`` 的时序图，可以看出它们的主要区别在于是否使用 DE 信号线以及对于消隐区域（Blanking Porch）的配置要求，总结为下表：

.. list-table::
    :widths: 20 30 50 10
    :header-rows: 1

    * - 模式
      - 是否使用 DE 信号线
      - 是否配置消隐区域寄存器
      - ESP 是否支持
    * - DE 模式
      - 是
      - 否
      - 是
    * - SYNC 模式
      - 否
      - 是
      - 是

色彩格式
---------------------

大多数 RGB LCD 支持多种色彩（输入数据）格式，包括 ``RGB565`` 、 ``RGB666`` 、 ``RGB888`` 等，通常可以使用 ``COLMOD(3Ah)`` 命令来配置。下图为 *ST7701S* 的色彩格式配置：

.. figure:: ../../../_static/display/screen/lcd_st7701_color_format.png
    :align: center
    :scale: 80%
    :alt: ST7701S 的色彩格式配置

    ST7701S 的色彩格式配置

从上图可以看出， *ST7701S* 支持 ``16-bit RGB565`` 、 ``18-bit RGB666`` 、 ``24-bit RGB888`` 三种色彩格式，其中 ``N-bit`` 表示接口的数据线位数，并且是通过 ``COLMOD(3Ah)：VIPF[2:0]`` 和 ``COLCTRL(CDh)：MDT`` 命令来进行选择。 **需注意，命令配置需要与硬件接口保持一致** ，例如 LCD 模块仅提供了 18-bit 的数据线，那么软件一定不能配置色彩格式为 ``24-bit RGB888`` ，并且在此情况下只有在数据线为 ``D[21:16]，D[13:8]，D[5:0]`` 时才能配置为 ``16-bit RGB565``。

**除此之外，色彩格式的位数并不等于接口的有效数据线位数** ，下图为 *ST77903* 的接口类型选择和色彩格式配置：

.. figure:: ../../../_static/display/screen/lcd_st77903_interface_type.png
    :align: center
    :scale: 70%
    :alt: ST77903 RGB 接口的类型选择

    ST77903 RGB 接口的类型选择

.. figure:: ../../../_static/display/screen/lcd_st77903_color_format.png
    :align: center
    :scale: 100%
    :alt: ST77903 的色彩格式配置

    ST77903 的色彩格式配置

从上图可以看出， *ST77903* 支持 ``6-bit RGB565`` 、 ``6-bit RGB666`` 和 ``8-bit RGB888`` 三种色彩格式，而它们的位数分别为 ``16-bit`` 、 ``18-bit`` 和 ``24-bit`` 。多数 LCD 的 ``RGB`` 接口仅需一个时钟周期即可并行传输单个像素的色彩数据，而像 ST77903 这类 LCD 接口则需要多个时钟周期传输单个像素的色彩数据，所以这类接口也被称为 **串行 RGB 接口** (SRGB)。

.. note::

    虽然 ESP32-S3 仅支持 ``16-bit RGB565`` 和 ``8-bit RGB888`` 两种色彩格式，但是通过特殊的硬件连接方式可以使其驱动支持 ``18-bit RGB666`` 或 ``24-bit RGB888`` 色彩格式的 LCD ，连接方式请参考开发板 `ESP32-S3-LCD-EV-Board <https://docs.espressif.com/projects/espressif-esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-lcd-ev-board/index.html>`_ 的 `LCD 子板 2 <https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/_static/esp32-s3-lcd-ev-board/schematics/SCH_ESP32-S3-LCD-EV-Board-SUB2_V1.2_20230509.pdf>`_ (3.95' LCD_QMZX) 和 `LCD 子板 3 <https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/_static/esp32-s3-lcd-ev-board/schematics/SCH_ESP32-S3-LCD-EV-Board-SUB3_V1.1_20230315.pdf>`_ 原理图。

RGB LCD 驱动流程
------------------------------

RGB LCD 驱动流程可大致分为三个部分：初始化接口设备、移植驱动组件和初始化 LCD 设备。

.. _rgb_初始化接口设备:

初始化接口设备
---------------------------

下面是使用 `esp_lcd_panel_io_additions <https://components.espressif.com/components/espressif/esp_lcd_panel_io_additions>`_ 组件来创建 ``3-wire SPI`` 接口设备的代码说明：

.. code-block:: c

    #include "esp_check.h"        // 依赖的头文件
    #include "esp_lcd_panel_io.h"
    #include "esp_lcd_panel_io_additions.h"

    esp_lcd_panel_io_3wire_spi_config_t io_config = {
        .line_config = {
            .cs_io_type = IO_TYPE_GPIO,                 // 设置为 `IO_TYPE_EXPANDER` 表示使用 IO 扩展芯片的引脚，否则使用 GPIO
            .cs_gpio_num = EXAMPLE_LCD_IO_SPI_CS,       // 连接 LCD CS 信号的 GPIO 编号
            // .cs_expander_pin = EXAMPLE_LCD_IO_SPI_CS,   // 连接 LCD CS 信号的扩展 IO 芯片引脚编号
            .scl_io_type = IO_TYPE_GPIO,                // 设置为 `IO_TYPE_EXPANDER` 表示使用 IO 扩展芯片的引脚，否则使用 GPIO
            .scl_gpio_num = EXAMPLE_LCD_IO_SPI_SCK,     // 连接 LCD SCK（SCL）信号的 GPIO 编号
            // .scl_expander_pin = EXAMPLE_LCD_IO_SPI_SCK, // 连接 LCD SCK（SCL）信号的扩展 IO 芯片引脚编号
            .sda_io_type = IO_TYPE_GPIO,                // 设置为 `IO_TYPE_EXPANDER` 表示使用 IO 扩展芯片的引脚，否则使用 GPIO
            .sda_gpio_num = EXAMPLE_LCD_IO_SPI_SDO,     // 连接 LCD MOSI（SDO、SDA） 信号的 GPIO 编号
            // .sda_expander_pin = EXAMPLE_LCD_IO_SPI_SDO, // 连接 LCD MOSI（SDO、SDA） 信号的扩展 IO 芯片引脚编号
            .io_expander = NULL,                        // 若使用 IO 扩展芯片的引脚，则需要传入已经初始化好的设备句柄
        },
        .expect_clk_speed = PANEL_IO_3WIRE_SPI_CLK_MAX, // 期望的 SPI 时钟频率，由于采用软件模拟的方式，实际可能有较大误差，
                                                        // 默认设为 `PANEL_IO_3WIRE_SPI_CLK_MAX` 即可
        .spi_mode = 0,                  // SPI 模式（0-3），需根据 LCD 驱动 IC 的数据手册以及硬件的配置确定（如 IM[3:0]）
        .lcd_cmd_bytes = 1,             // 单位 LCD 命令的字节数（1-4），通常设为 `1` 即可
        .lcd_param_bytes = 1,           // 单位 LCD 参数的字节数（1-4），通常设为 `1` 即可
        .flags = {
            .use_dc_bit = 1,            // 默认设为 `1` 即可
            .del_keep_cs_inactive = 1,  // 默认设为 `1` 即可
        },
    }
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

**对于仅采用 RGB 接口的 LCD** ，因为它们不支持传输命令及参数，所以这里不需要初始化接口设备，请直接参考 :ref:`初始化 LCD 设备  <rgb_初始化_lcd>`。

**对于采用 3-wire SPI 和 RGB 接口的 LCD** ，这里仅需创建 ``3-wire SPI`` 接口设备。由于 ESP 的 SPI 外设不支持直接传输 9-bit 数据，并且该接口仅用于传输数据量较小的命令及参数，而且对于数据传输的带宽以及时序要求不高，因此可以使用 GPIO 或者 IO 扩展芯片引脚 （如 `TCA9554 <https://components.espressif.com/components/espressif/esp_io_expander_tca9554>`_ ） 通过软件模拟 SPI 协议的方式来实现。

通过创建接口设备可以获取数据类型为 ``esp_lcd_panel_io_handle_t`` 的句柄，然后能够使用 ``esp_lcd_panel_io_tx_param()`` 给 LCD 的驱动 IC 发送 **命令** 。

.. _rgb_移植驱动组件:

移植驱动组件
---------------------------

**对于仅采用 RGB 接口的 LCD** ，由于 `RGB 接口驱动 <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/src/esp_lcd_panel_rgb.c>`_ 中已经通过注册回调函数的方式实现了结构体 `esp_lcd_panel_t <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/interface/esp_lcd_panel_interface.h>`_ 中的各项功能，并且提供了函数 ``esp_lcd_new_rgb_panel()`` 用于创建数据类型为 ``esp_lcd_panel_handle_t`` 的 LCD 设备，使得应用程序能够使用 `LCD 通用 APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ 来操作 LCD 设备。因此，这种 LCD 不需要移植驱动组件，请直接参考 :ref:`初始化 LCD 设备  <rgb_初始化_lcd>`。

**对于采用 3-wire SPI 和 RGB 接口的 LCD** ，在上述 `RGB 接口驱动 <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/src/esp_lcd_panel_rgb.c>`_ 的基础上，还需要通过 ``3-wire SPI`` 接口发送命令及参数。因此，实现这种 LCD 驱动组件的基本原理包含以下三点：

  #. 基于数据类型为 ``esp_lcd_panel_io_handle_t`` 的接口设备发送指定格式的命令及参数。
  #. 使用函数 ``esp_lcd_new_rgb_panel()`` 创建一个 LCD 设备，然后通过注册回调函数的方式 **保存和覆盖** 该设备中的 **部分** 功能。
  #. 实现一个函数用于提供数据类型为 ``esp_lcd_panel_handle_t`` 的 LCD 设备句柄，使得应用程序能够利用 `LCD 通用 APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ 来操作 LCD 设备。

下面是 ``esp_lcd_panel_handle_t`` 各项功能的实现说明以及和 `RGB 接口驱动 <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/src/esp_lcd_panel_rgb.c>`_ 还有 `LCD 通用 APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ 的对应关系：

.. list-table::
    :widths: 10 20 20 50
    :header-rows: 1

    * - 功能
      - RGB 接口驱动
      - LCD 通用 APIs
      - 实现说明
    * - reset()
      - rgb_panel_reset()
      - esp_lcd_panel_reset()
      - 若设备连接了复位引脚，则通过该引脚进行硬件复位，否则通过命令 ``LCD_CMD_SWRESET (01h)`` 进行软件复位，最后使用 ``rgb_panel_reset()`` 复位 ``RGB`` 接口。
    * - init()
      - rgb_panel_init()
      - esp_lcd_panel_init()
      - 若 ``3-wire SPI`` 接口没有与 ``RGB`` 接口复用引脚，则通过发送一系列的命令及参数来初始化 LCD 设备，否则需要提前在 LCD 创建时进行初始化，最后使用 ``rgb_panel_init()`` 初始化 ``RGB`` 接口。
    * - del()
      - rgb_panel_del()
      - esp_lcd_panel_del()
      - 释放驱动占用的资源，包括申请的存储空间和使用的 IO，还要使用 ``rgb_panel_del()`` 删除 ``RGB`` 接口。
    * - draw_bitmap()
      - rgb_panel_draw_bitmap()
      - esp_lcd_panel_draw_bitmap()
      - 无需保存和覆盖，使用 ``rgb_panel_draw_bitmap()`` 发送图像数据。
    * - mirror()
      - rgb_panel_mirror()
      - esp_lcd_panel_mirror()
      - 根据用户配置，既可以通过命令，也可以使用 ``rgb_panel_mirror()`` 通过软件实现镜像 X 轴和 Y 轴。
    * - swap_xy()
      - rgb_panel_swap_xy()
      - esp_lcd_panel_swap_xy()
      - 无需保存和覆盖，使用 ``rgb_panel_swap_xy()`` 通过软件实现交换 X 轴和 Y 轴。
    * - set_gap()
      - rgb_panel_set_gap()
      - esp_lcd_panel_set_gap()
      - 无需保存和覆盖，使用 ``rgb_panel_set_gap()`` 通过软件修改画图时的起始和终止坐标，从而实现画图的偏移。
    * - invert_color()
      - rgb_panel_invert_color()
      - esp_lcd_panel_invert_color()
      - 无需保存和覆盖，使用 ``rgb_panel_invert_color()`` 通过硬件实现像素的色彩数据按位取反（0xF0F0 -> 0x0F0F）。
    * - disp_on_off()
      - rgb_panel_disp_on_off()
      - esp_lcd_panel_disp_on_off()
      - 根据用户配置来实现 LCD 显示的开关。如果没有配置 ``disp_gpio_num``，则可以通过 LCD 命令 ``LCD_CMD_DISON(29h)`` 和 ``LCD_CMD_DISOFF(28h)`` 来进行控制。另外，如果配置了 ``disp_gpio_num``，则可以通过调用函数 ``rgb_panel_disp_on_off()`` 来实现控制。

对于大多数 RGB LCD，其驱动 IC 的命令及参数与上述实现说明中的兼容，因此可以通过以下步骤完成移植：

#. 在 :ref:`LCD 驱动组件 <lcd_驱动组件>`  中选择一个型号相似的 RGB LCD 驱动组件。
#. 通过查阅目标 LCD 驱动 IC 的数据手册，确认其与所选组件中各功能使用到的命令及参数是否一致，若不一致则需要修改相关代码。
#. 即使 LCD 驱动 IC 的型号相同，不同制造商的屏幕也通常需要使用各自提供的初始化命令配置。因此，需要修改初始化函数 ``init()`` 中发送的命令和参数。这些初始化命令通常以特定的格式存储在一个静态数组中。此外，需要注意不要在初始化命令中包含由驱动 IC 控制的命令，例如 ``LCD_CMD_COLMOD(3Ah)``，以确保成功初始化 LCD 设备。
#. 可使用编辑器的字符搜索和替换功能，将组件中的 LCD 驱动 IC 名称替换为目标名称，如将 ``gc9503`` 替换为 ``st7701``。

.. _rgb_初始化_lcd:

初始化 LCD 设备
---------------------------

下面是以 ESP-IDF release/v5.1 中 `rgb_panel <https://github.com/espressif/esp-idf/tree/release/v5.1/examples/peripherals/lcd/rgb_panel>`_ 为例的代码说明：

.. code-block:: c

    #include "esp_check.h"        // 依赖的头文件
    #include "esp_lcd_panel_ops.h"
    #include "esp_lcd_panel_rgb.h"

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_rgb_panel_config_t panel_config = {   // RGB 接口的配置参数
        .data_width = EXAMPLE_LCD_DATA_WIDTH,               // RGB 接口的数据线位数，如 `16-bit RGB565`: 16，`8-bit RGB888`：8
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,        // 色彩格式的位数，可能与 RGB 接口的数据线位数不相等，
                                                            // 如 `16-bit RGB565`: 16，`8-bit RGB888`：24
        .psram_trans_align = 64,                            // 默认设为 `64` 即可
        .num_fbs = EXAMPLE_LCD_NUM_FB,                      // RGB 接口的帧缓存数，默认设为 `1`，大于 `1` 时用于实现多缓冲防撕裂
        .bounce_buffer_size_px = 10 * EXAMPLE_LCD_H_RES,    // 用于提升 RGB 接口的数据传输带宽，通常设为 `10 * EXAMPLE_LCD_H_RES`
        .clk_src = LCD_CLK_SRC_DEFAULT,                     // 默认设为 `LCD_CLK_SRC_DEFAULT` 即可
        .disp_gpio_num = EXAMPLE_PIN_NUM_DISP_EN,           // 连接 LCD DISP 信号的引脚编号，可以设置为 `-1` 表示不使用
        .pclk_gpio_num = EXAMPLE_PIN_NUM_PCLK,              // 连接 LCD PCLK 信号的引脚编号
        .vsync_gpio_num = EXAMPLE_PIN_NUM_VSYNC,            // 连接 LCD VSYNC 信号的引脚编号
        .hsync_gpio_num = EXAMPLE_PIN_NUM_HSYNC,            // 连接 LCD HSYNC 信号的引脚编号
        .de_gpio_num = EXAMPLE_PIN_NUM_DE,                  // 连接 LCD DE 信号的引脚编号，可以设置为 `-1` 表示不使用
        .data_gpio_nums = {                                 // 连接 LCD D[15:0] 信号的引脚编号，有效数量由 `data_width` 指定，
                                                            // 8-bit 时设置 D[7:0] 即可
            EXAMPLE_PIN_NUM_DATA0,
            EXAMPLE_PIN_NUM_DATA1,
            EXAMPLE_PIN_NUM_DATA2,
            EXAMPLE_PIN_NUM_DATA3,
            EXAMPLE_PIN_NUM_DATA4,
            EXAMPLE_PIN_NUM_DATA5,
            EXAMPLE_PIN_NUM_DATA6,
            EXAMPLE_PIN_NUM_DATA7,
            EXAMPLE_PIN_NUM_DATA8,
            EXAMPLE_PIN_NUM_DATA9,
            EXAMPLE_PIN_NUM_DATA10,
            EXAMPLE_PIN_NUM_DATA11,
            EXAMPLE_PIN_NUM_DATA12,
            EXAMPLE_PIN_NUM_DATA13,
            EXAMPLE_PIN_NUM_DATA14,
            EXAMPLE_PIN_NUM_DATA15,
        },
        .timings = {        // 以下为 RGB 时序的相关参数，需根据 LCD 驱动 IC 的数据手册以及硬件的配置确定
            .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
            .h_res = EXAMPLE_LCD_H_RES,
            .v_res = EXAMPLE_LCD_V_RES,
            .hsync_back_porch = 40,         // 在 DE 模式下，HSYNC 和  VSYNC 的相关参数可以根据期望的刷新率进行调整
            .hsync_front_porch = 20,        // 在 SYNC 模式下，HSYNC 和  VSYNC 的相关参数需要和软件初始化命令中的配置保持一致
            .hsync_pulse_width = 1,
            .vsync_back_porch = 8,
            .vsync_front_porch = 4,
            .vsync_pulse_width = 1,
            .flgas = {      // 由于一些 LCD 可以通过硬件引脚配置这些参数，需要确保它们与配置保持一致，但通常情况下均为 `0`
              .hsync_idle_low = 0,    // HSYNC 信号空闲时的电平，0：高电平，1：低电平
              .vsync_idle_low = 0,    // VSYNC 信号空闲时的电平，0 表示高电平，1：低电平
              .de_idle_high = 0,      // DE 信号空闲时的电平，0：高电平，1：低电平
              .pclk_active_neg = 0,   // 时钟信号的有效边沿，0：上升沿有效，1：下降沿有效
              .pclk_idle_high = 0,    // PCLK 信号空闲时的电平，0：高电平，1：低电平
            },
        },
        .flags.fb_in_psram = 1,       // 默认设置为 `1` 即可
    };
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    /* 以下函数可以根据需要调用 */
    // ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));   // 通过硬件实现像素的色彩数据按位取反（0xF0F0 -> 0x0F0F）
    // ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));   // 通过软件实现镜像 X 轴和 Y 轴
    // ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));        // 通过软件实现交换 X 轴和 Y 轴
    // ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 0));        // 通过软件修改画图时的起始和终止坐标，从而实现画图的偏移
    // ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));    // 通过 `disp_gpio_num` 引脚控制 LCD 显示的开关，
                                                                          // 仅当该引脚设置且不为 `-1` 时可用，否则会报错

**时序参数对应关系说明：**

上述代码中的时序参数与前面 `DE 模式` 和 `SYNC 模式` 时序图中的符号对应关系如下：

.. list-table:: 水平时序参数对应关系
    :widths: 30 30 40
    :header-rows: 1

    * - 代码参数名称
      - 时序图符号
      - 说明
    * - ``hsync_back_porch``
      - Thbp
      - 水平同步后沿
    * - ``hsync_front_porch``
      - Thfp
      - 水平同步前沿
    * - ``hsync_pulse_width``
      - Thpw
      - 水平同步脉冲宽度

.. list-table:: 垂直时序参数对应关系
    :widths: 30 30 40
    :header-rows: 1

    * - 代码参数名称
      - 时序图符号
      - 说明
    * - ``vsync_back_porch``
      - Tvbp
      - 垂直同步后沿
    * - ``vsync_front_porch``
      - Tvfp
      - 垂直同步前沿
    * - ``vsync_pulse_width``
      - Tvs
      - 垂直同步脉冲宽度

**对于采用 3-wire SPI 和 RGB 接口的 LCD** ，首先通过 `RGB 接口驱动 <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/src/esp_lcd_panel_rgb.c>`_ 中的 ``esp_lcd_new_rgb_panel()`` 函数创建 LCD 设备并获取数据类型为 ``esp_lcd_panel_handle_t`` 的句柄，然后使用 `LCD 通用 APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ 来初始化 LCD 设备.

关于 ``RGB`` 接口的参数配置和一些功能函数的说明，请参考 :ref:`RGB 参数配置及功能函数 <rgb_参数配置及功能函数>`

下面是以 `ST7701S <https://components.espressif.com/components/espressif/esp_lcd_st7701>`_ 为例的代码说明：

.. code-block:: c

    #include "esp_check.h"          // 依赖的头文件
    #include "esp_lcd_panel_ops.h"
    #include "esp_lcd_panel_rgb.h"
    #include "esp_lcd_panel_vendor.h"
    #include "esp_lcd_st7701.h"     // 目标驱动组件的头文件

    /**
    * 用于存放 LCD 驱动 IC 的初始化命令及参数
    */
    // static const st7701_lcd_init_cmd_t lcd_init_cmds[] = {
    // //   cmd   data        data_size  delay_ms
    //    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    //    {0xEF, (uint8_t []){0x08}, 1, 0},
    //    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    //    {0xC0, (uint8_t []){0x3B, 0x00}, 2, 0},
    //     ...
    // };

    /* 创建 LCD 设备 */
    esp_lcd_rgb_panel_config_t rgb_config = {   // RGB 接口的配置参数
        .data_width = EXAMPLE_LCD_DATA_WIDTH,               // RGB 接口的数据线位数，如 `16-bit RGB565`: 16，`8-bit RGB888`：8
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,        // 色彩格式的位数，可能与 RGB 接口的数据线位数不相等，
                                                            // 如 `16-bit RGB565`: 16，`8-bit RGB888`：24
        .psram_trans_align = 64,                            // 默认设为 `64` 即可
        .num_fbs = EXAMPLE_LCD_NUM_FB,                      // RGB 接口的帧缓存数量，默认设为 `1`，大于 `1` 时用于实现多缓冲防撕裂
        .bounce_buffer_size_px = 10 * EXAMPLE_LCD_H_RES,    // 用于提升 RGB 接口的数据传输带宽，通常设为 `10 * EXAMPLE_LCD_H_RES`
        .clk_src = LCD_CLK_SRC_DEFAULT,                     // 默认设为 `LCD_CLK_SRC_DEFAULT` 即可
        .disp_gpio_num = EXAMPLE_PIN_NUM_DISP_EN,           // 连接 LCD DISP 信号的引脚编号，可以设置为 -1 表示不使用
        .pclk_gpio_num = EXAMPLE_PIN_NUM_PCLK,              // 连接 LCD PCLK 信号的引脚编号
        .vsync_gpio_num = EXAMPLE_PIN_NUM_VSYNC,            // 连接 LCD VSYNC 信号的引脚编号
        .hsync_gpio_num = EXAMPLE_PIN_NUM_HSYNC,            // 连接 LCD HSYNC 信号的引脚编号
        .de_gpio_num = EXAMPLE_PIN_NUM_DE,                  // 连接 LCD DE 信号的引脚编号，可以设置为 -1 表示不使用
        .data_gpio_nums = {                                 // 连接 LCD D[15:0] 信号的引脚编号，有效数量由 `data_width` 指定，
                                                            // 8-bit 时设置 D[7:0] 即可
            EXAMPLE_PIN_NUM_DATA0,
            EXAMPLE_PIN_NUM_DATA1,
            EXAMPLE_PIN_NUM_DATA2,
            EXAMPLE_PIN_NUM_DATA3,
            EXAMPLE_PIN_NUM_DATA4,
            EXAMPLE_PIN_NUM_DATA5,
            EXAMPLE_PIN_NUM_DATA6,
            EXAMPLE_PIN_NUM_DATA7,
            EXAMPLE_PIN_NUM_DATA8,
            EXAMPLE_PIN_NUM_DATA9,
            EXAMPLE_PIN_NUM_DATA10,
            EXAMPLE_PIN_NUM_DATA11,
            EXAMPLE_PIN_NUM_DATA12,
            EXAMPLE_PIN_NUM_DATA13,
            EXAMPLE_PIN_NUM_DATA14,
            EXAMPLE_PIN_NUM_DATA15,
        },
        .timings = {        // 以下为 RGB 时序的相关参数，需根据 LCD 驱动 IC 的数据手册以及软硬件的配置确定
            .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
            .h_res = EXAMPLE_LCD_H_RES,
            .v_res = EXAMPLE_LCD_V_RES,
            .hsync_back_porch = 40,         // 在 DE 模式下，HSYNC 和  VSYNC 的相关参数可以根据期望的刷新率进行调整
            .hsync_front_porch = 20,        // 在 SYNC 模式下，HSYNC 和  VSYNC 的相关参数需要和软件初始化命令中的配置保持一致
            .hsync_pulse_width = 1,
            .vsync_back_porch = 8,
            .vsync_front_porch = 4,
            .vsync_pulse_width = 1,
            .flgas = {      // 由于一些 LCD 可以通过硬件引脚或者软件命令配置这些参数，需要确保它们与配置保持一致，但通常情况下均为 `0`
              .hsync_idle_low = 0,    // HSYNC 信号空闲时的电平，0：高电平，1：低电平
              .vsync_idle_low = 0,    // VSYNC 信号空闲时的电平，0 表示高电平，1：低电平
              .de_idle_high = 0,      // DE 信号空闲时的电平，0：高电平，1：低电平
              .pclk_active_neg = 0,   // 时钟信号的有效边沿，0：上升沿有效，1：下降沿有效
              .pclk_idle_high = 0,    // PCLK 信号空闲时的电平，0：高电平，1：低电平
            },
        },
        .flags.fb_in_psram = 1,       // 默认设置为 `1` 即可
    };
    st7701_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,    // RGB 接口的配置参数
        // .init_cmds = lcd_init_cmds,    // 用于替换驱动组件中的初始化命令及参数
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st7701_lcd_init_cmd_t),
        .flags = {          // LCD 驱动 IC 的配置参数
            .mirror_by_cmd = 1,       // 若为 `1` 则使用 LCD 命令实现镜像功能（esp_lcd_panel_mirror()），若为 `0` 则通过软件实现
            .enable_io_multiplex = 0, // 若为 `1` 则在删除 LCD 设备时自动删除接口设备，此时应设置所有名称为 `*_by_cmd` 的参数为 `0`，
                                      // 若为 `0` 则不删除。如果 3-wire SPI 接口的引脚与 RGB 接口的复用，那么需要设置此参数为 `1`
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_LCD_IO_RST,           // 连接 LCD 复位信号的 IO 编号，可以设为 `-1` 表示不使用
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // 像素色彩的元素顺序（RGB/BGR），
                                                        //  一般通过命令 `LCD_CMD_MADCTL（36h）` 控制
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,    // 色彩格式的位数（RGB565：16，RGB666：18，RGB888：24），
                                                        // 一般通过命令 `LCD_CMD_COLMOD（3Ah）` 控制
        .vendor_config = &vendor_config,                // RGB 接口及 LCD 驱动 IC 的配置参数
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(io_handle, &panel_config, &panel_handle));

    /* 初始化 LCD 设备 */
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // 以下函数可以根据需要使用
    // ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    // ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));
    // ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    // ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 0));
    // ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

**对于采用 3-wire SPI 和 RGB 接口的 LCD** ，首先通过移植好的驱动组件创建 LCD 设备并获取数据类型为 ``esp_lcd_panel_handle_t`` 的句柄，然后使用 `LCD 通用 APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ 来初始化 LCD 设备。

.. _rgb_参数配置及功能函数:

关于 ``RGB`` 接口配置参数更加详细的说明，请参考 `ESP-IDF 编程指南 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html#rgb-interfaced-lcd>`_。下面是一些关于使用函数 ``esp_lcd_panel_draw_bitmap()`` 刷新 RGB LCD 图像的说明：

  - 该函数是通过内存拷贝的方式刷新帧缓存里的图像数据，也就是说该函数调用完成后帧缓存内的图像数据也已经更新完成，而 ``RGB`` 接口本身是通过 DMA 从帧缓存中获取图像数据来刷新 LCD，这两个过程是异步进行的。
  - 该函数会判断传入参数 ``color_data`` 的值是否为 ``RGB`` 接口内部的帧缓存地址，若是，则不会进行上述的内存拷贝操作，而是直接将 ``RGB`` 接口的 DMA 传输地址设置为该缓存地址，从而在具有多个帧缓存的情况下实现切换的功能。

除了 `LCD 通用 APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ 之外， `RGB 接口驱动 <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/src/esp_lcd_panel_rgb.c>`_ 中还提供了一些特殊功能的函数，下面是一些常用函数的使用说明：

  - ``esp_lcd_rgb_panel_set_pclk()``：动态修改时钟频率，可以在 LCD 初始化后使用。
  - ``esp_lcd_rgb_panel_restart()``：复位数据传输，用于在屏幕发生偏移时调用可以使其恢复正常。
  - ``esp_lcd_rgb_panel_get_frame_buffer()``：获取帧缓存的地址，可用数量由配置参数 ``num_fbs`` 决定，用于多缓冲防撕裂。
  - ``esp_lcd_rgb_panel_register_event_callbacks()``：注册多种事件的回调函数，示例代码及说明如下：

    .. code-block:: c

        static bool example_on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
        {
            /* 可以在此处进行一些操作 */

            return false;
        }

        static bool example_on_bounce_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
        {
            /* 可以在此处进行一些操作 */

            return false;
        }

        esp_lcd_rgb_panel_event_callbacks_t cbs = {
            .on_vsync = example_on_vsync_event,                 // 刷新完一帧图像时的回调函数
            .on_bounce_frame_finish = example_on_bounce_event,  // 通过 Bounce Buffer 机制搬运完一帧图像时的回调函数
                                                                // 需注意，此时 RGB 接口还未传输完该帧图像
        };
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, &example_user_ctx));

.. note::

  针对 ESP32-P4 RGB 屏幕使用场景的说明：

  - 由于 ESP32-P4 有多路 VDDPST，其控制对应区域的 GPIO 最大输出电平（对应关系见 Datasheet）。因此如果不配置 VDDPST，可能部分 GPIO 最大输出 1.8V 导致引脚电压不足，从而导致 RGB 屏幕颜色异常。
  - ESP32-S3 最大可支持 30 M 的 RGB 时钟频率。而 ESP32-P4 的 RGB 时钟频率无 30M 限制，因此接口帧率可以更高，可支持更大分辨率的 RGB 屏幕。

相关文档
---------------------

- `ST7701S 数据手册 <https://dl.espressif.com/AE/esp-iot-solution/ST7701S_SPEC_%20V1.4.pdf>`_
- `ST77903 数据手册 <https://dl.espressif.com/AE/esp-iot-solution/ST77903_SPEC_P0.5.pdf>`_
- `GC9503 数据手册 <https://github.com/espressif/esp-dev-kits/blob/master/docs/_static/esp32-s3-lcd-ev-board/datasheets/3.95_480x480_SmartDisplay/GC9503NP_DataSheet_V1.7.pdf>`_
