球泡灯驱动
=============
:link_to_translation:`en:[English]`

球泡灯驱动组件将球泡灯中常用的多款调光方案做了封装，使用一个抽象层管理这些方案，便于开发者集成到自己的应用程序中，目前所有 ESP32 系列芯片都已经完成支持。

调光方案
--------

PWM 调光方案
^^^^^^^^^^^^

PWM 调光是一种通过调节脉冲宽度来控制 LED 亮度的技术，核心在于通过改变电流脉冲的占空比（即高电平时间在整个周期时间中的比例）来调节。当占空比较高时，LED
获得的电流时间长，亮度高；反之，占空比低时，LED 获得的电流时间短，亮度低。所有 ESP 芯片均支持使用硬件 `LEDC
驱动 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/peripherals/ledc.html?highlight=ledc>`__
/
`MCPWM <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/peripherals/mcpwm.html?highlight=mcpwm>`__
驱动输出 PWM，推荐优先使用 LEDC 驱动实现，LEDC 驱动支持硬件渐变，频率占空比均可配置，分辨率最高为 20 bit。目前已支持下述 PWM 调光驱动类型：

   -  RGB + C/W
   -  RGB + CCT/Brightnes

PWM 方案使用案例
~~~~~~~~~~~~~~~~

使用 PWM 方案点亮一个 3 路 RGB 灯珠球泡灯，PWM 频率 为 4000Hz，使用软件混色，启用渐变功能，上电点亮为红色。

.. code:: c

    lightbulb_config_t config = {
        //1. 选择 PWM 输出并进行参数配置
        .type = DRIVER_ESP_PWM,
        .driver_conf.pwm.freq_hz = 4000,

        //2. 功能选择，根据你的需要启用/禁用
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        /* 如果你的驱动白光输出为硬件单独控制而不是软件混色，需要启用此功能 */
        .capability.enable_hardware_cct = true,
        .capability.enable_status_storage = true,
        /* 用于配置 LED 灯珠组合 */
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,

        //3. 配置 PWM 输出的硬件管脚
        .io_conf.pwm_io.red = 25,
        .io_conf.pwm_io.green = 26,
        .io_conf.pwm_io.blue = 27,

        //4. 限制参数
        .external_limit = NULL,

        //5. 颜色校准参数
        .gamma_conf = NULL,

        //6. 初始化照明参数，如果 on 置位将在初始化驱动时点亮球泡灯
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_init(&config);

I2C 调光方案
^^^^^^^^^^^^

`I2C 调光 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/peripherals/i2c.html>`__
通过 I2C 总线发送控制命令到调光芯片，通过改变调光芯片的输出电流，调节 LED 的亮度。I2C 总线由两条线构成：数据线 (SDA) 和时钟线 (SCL)。所有 ESP
芯片均支持使用硬件 I2C 通信调光芯片。目前已支持下述调光芯片：

   -  SM2135EH
   -  SM2X35EGH (SM2235EGH/SM2335EGH)
   -  BP57x8D (BP5758/BP5758D/BP5768)
   -  BP1658CJ
   -  KP18058

I2C 调光使用案例
~~~~~~~~~~~~~~~~

使用 I2C 驱动 BP5758D 芯片点亮一个 5 路 RGBCW 灯珠球泡灯，I2C 通信速率为 300KHz，RGB 灯珠驱动电流为 50 mA，CW 灯珠驱动电流为 30mA，使用软件混色，启用渐变功能，上电点亮为红色。

.. code:: c

    lightbulb_config_t config = {
        //1. 选择需要的芯片并进行参数配置，每款芯片配置的参数存在不同，请仔细参阅芯片手册
        .type = DRIVER_BP57x8D,
        .driver_conf.bp57x8d.freq_khz = 300,
        .driver_conf.bp57x8d.enable_iic_queue = true,
        .driver_conf.bp57x8d.iic_clk = 4,
        .driver_conf.bp57x8d.iic_sda = 5,
        .driver_conf.bp57x8d.current = {50, 50, 50, 30, 30},

        //2. 驱动功能选择，根据你的需要启用/禁用
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,

        .capability.enable_status_storage = true,
        .capability.led_beads = LED_BEADS_5CH_RGBCW,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,

        //3. 配置 IIC 芯片的硬件管脚
        .io_conf.iic_io.red = OUT3,
        .io_conf.iic_io.green = OUT2,
        .io_conf.iic_io.blue = OUT1,
        .io_conf.iic_io.cold_white = OUT5,
        .io_conf.iic_io.warm_yellow = OUT4,

        //4. 限制参数，使用细则请参考后面小节
        .external_limit = NULL,

        //5. 颜色校准参数
        .gamma_conf = NULL,

        //6. 初始化照明参数，如果 on 置位将在初始化驱动时点亮球泡灯
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_init(&config);

单线调光方案
^^^^^^^^^^^^

单线调光方案是一种通过单根通信线控制 LED 亮度的方法，核心是通过特定的通信协议发送控制信号来调节 LED
的亮度，此类方案在 ESP 芯片上可使用 `RMT 外设 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/peripherals/rmt.html>`__\ 或
`SPI外设 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/peripherals/spi_master.html>`__
实现，推荐优先使用 SPI 实现灯珠通信控制。目前已支持下述灯珠类型：

-  WS2812

WS2812 使用案例
~~~~~~~~~~~~~~~~

使用 SPI 驱动点亮 10 颗 WS2812 灯珠，启用渐变功能，上电为红色。

.. code:: c

    lightbulb_config_t config = {
        //1. 选择 WS2812 输出并进行参数配置
        .type = DRIVER_WS2812,
        .driver_conf.ws2812.led_num = 10,
        .driver_conf.ws2812.ctrl_io = 4,

        //2. 驱动功能选择，根据你的需要启用/禁用
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_status_storage = true,

        /* 对于 WS2812 只能选择 LED_BEADS_3CH_RGB */
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,

        //3. 限制参数，使用细则请参考后面小节
        .external_limit = NULL,

        //4. 颜色校准参数
        .gamma_conf = NULL,

        //5. 初始化照明参数，如果 on 置位将在初始化驱动时点亮球泡灯
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_init(&config);

渐变原理
---------

球泡灯组件中的渐变是用软件实现的，每个通道将记录驱动输出的当前值，最终值，步长，步数等。当使用 API 设置颜色时，会更改最终值，步长，步数，并启用渐变定时器，定时器每间隔 12ms 触发一次回调，回调函数中会检查每个通道的步数，只要存在未执行完的步数，就会根据步长加减当前值，并更新到底层驱动中。
当所有通道步数为 0 时，便代表渐变完成，此时将停止定时器。

低功耗实现流程
---------------

球泡灯如果要通过 T20 等功耗认证，优化灯板电源后，软件上需要做一些低功耗配置，除了应用 `低功耗模式使用指南 <htps://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/low-power-mode.html>`__ 中提到的配置外，驱动部分也需要进行一些处理，
球泡灯组件中已经在 PWM 方案与 I2C 2 种调光驱动方案增加了相关内容，具体逻辑为开关灯时，I2C 方案调用调光芯片自身的低功耗指令，退出或进入低功耗，PWM 方案中，ESP32 由于使用 APB 时钟源，需要通过电源锁管理，否则会出现灯光闪烁，开灯时拿电源锁，禁用动态调频，关灯时释放，
其他芯片采用 XTAL 时钟源，无需采取任何措施。

颜色校准方案
------------

色温模式
^^^^^^^^^^^^

    色温模式的校准需要配置下述结构体。

.. code:: c

    union {
        struct {
            uint16_t kelvin_min;
            uint16_t kelvin_max;
        } standard;
        struct {
            lightbulb_cct_mapping_data_t *table;
            int table_size;
        } precise;
    } cct_mix_mode;


- 标准模式：标定最大最小值开尔文值，使用线性插值的方式补全中间值，然后根据比色温值调节冷、暖灯珠输出的比例。

- 精准模式：校准不同色温所需要的红、绿、蓝、冷、暖灯珠输出比例，使用这些校准点直接输出对应比例，校准的点越多，色温越精准。

彩色模式
^^^^^^^^^^^^

    色彩模式的校准需要配置下述结构体。

.. code:: c

    union {
        struct {
            lightbulb_color_mapping_data_t *table;
            int table_size;
        } precise;
    } color_mix_mode;

- 标注模式：无需参数配置，内部将使用理论算法将 HSV、 XYY 等颜色模型转化为 RGB 比例，直接按此比例点亮灯珠。

- 精准模式：使用 HSV 模型标定颜色，测量一些不同色相与饱和度颜色所需要的红、绿、蓝、冷、暖灯珠输出比例作为校准点，使用线性插值的方式补全中间值，使用这些校准点后配比点亮灯珠。


功率限制方案
------------

    功率限制用于平衡、微调某个通道或整体的输出电流，以满足功率需求。
    
    如果需要对整体功率进行限制需要配置下述结构体。

.. code:: c

    typedef struct {
        /* Scale the incoming value
         * range: 10% <= value <= 100%
         * step: 1%
         * default min: 10%
         * default max: 100%
         */
        uint8_t white_max_brightness; /**< Maximum brightness limit for white light output. */
        uint8_t white_min_brightness; /**< Minimum brightness limit for white light output. */
        uint8_t color_max_value;      /**< Maximum value limit for color light output. */
        uint8_t color_min_value;      /**< Minimum value limit for color light output. */

        /* Dynamically adjust the final power
         * range: 100% <= value <= 500%
         * step: 10%
         */
        uint16_t white_max_power;     /**< Maximum power limit for white light output. */
        uint16_t color_max_power;     /**< Maximum power limit for color light output. */
    } lightbulb_power_limit_t;

-  ``white_max_brightness`` 与 ``white_min_brightness`` 用于色温模式，将色温 API 设置的 ``brightness`` 参数限制在该最大最小值之间。
-  ``color_max_value`` 与 ``color_min_value`` 用于彩色模式，将彩色 API 设置的 ``value`` 参数限制在该最大最小值之间。
-  ``white_max_power`` 用于色温模式限制功率，默认值为 100，即输出的最大功率为满功率的 1/2，如果为 200 则能达到冷、暖灯珠的最大功率。
-  ``color_max_power`` 用于彩色模式限制功率，默认值为 100，即输出的最大功率为满功率的 1/3，如果为 300 则能达到红、绿、蓝灯珠的最大功率。

如果需要对单灯珠功率进行限制需要配置下述结构体：

.. code:: c

    typedef struct {
        float balance_coefficient[5]; /**< Array of float coefficients for adjusting the intensity of each color channel (R, G, B, C, W).
                                           These coefficients help in achieving the desired color balance for the light output. */

        float curve_coefficient;      /**< Coefficient for gamma correction. This value is used to modify the luminance levels
                                           to suit the non-linear characteristics of human vision, thus improving the overall
                                           visual appearance of the light. */
    } lightbulb_gamma_config_t;
  

- ``balance_coefficient`` 用于微调每个灯珠输出电流，驱动最终输出将按该比例进行衰减，默认值为 1.0，即不衰减。
- ``curve_coefficient`` 用于将渐变时的线性变化转化为曲线变化。

.. Note::

    修改 ``balance_coefficient`` 将影响颜色校准的准确性，需要先调整该参数在进行颜色校准。该参数最大的用途在于，某些 I2C 调光芯片输出的电流为 5mA 或者 10mA 的整倍数，
    如果需要特定电流便可使用该参数调整。


API 参考
-----------------

.. include-build-file:: inc/lightbulb.inc
