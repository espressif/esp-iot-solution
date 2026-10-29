触摸屏
================
:link_to_translation:`en:[English]`

触摸屏已成为显示应用中的标准组件。ESP-IoT-Solution 基于 ESP-BSP 的 `esp_lcd_touch <https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch/esp_lcd_touch>`_ 组件提供了常见触摸屏控制器的驱动程序。

支持的触摸控制器
-----------------------------

ESP-IoT-Solution 目前支持以下触摸屏控制器：

+-----------------------+------------------------+---------------------------+
| 触摸控制器            | 通信接口               | 组件名称                  |
+=======================+========================+===========================+
| ILI2118               | I2C                    | esp_lcd_touch_ili2118     |
+-----------------------+------------------------+---------------------------+
| SPD2010               | I2C                    | esp_lcd_touch_spd2010     |
+-----------------------+------------------------+---------------------------+
| ST7123                | I2C                    | esp_lcd_touch_st7123      |
+-----------------------+------------------------+---------------------------+

更多触摸控制器请参考 ESP 组件注册表上提供的 `esp_lcd_touch 驱动 <https://components.espressif.com/components?q=esp_lcd_touch>`_。

ESP-IDF 和 ESP-BSP 支持的常见触摸控制器包括：

- **GT911** / **GT1151** - 电容触摸控制器
- **FT5x06** 系列 (FT5216, FT5316, FT5436, FT6336) - 电容触摸控制器
- **CST816S** - 电容触摸控制器
- **TT21100** - 电容触摸控制器

架构
-------------

``esp_lcd_touch`` 组件为不同的触摸屏控制器提供了统一的抽象层。这种架构提供了：

- **统一 API**：所有触摸控制器使用相同的接口
- **易于集成**：与 LVGL 和其他 GUI 库无缝集成
- **硬件抽象**：在不修改应用代码的情况下切换触摸控制器
- **I2C 通信**：高效的基于 I2C 的触摸控制器通信

触摸屏特性
----------------------

**电容触摸屏：**

- 多点触控支持（取决于控制器）
- 无需校准（由控制器校准）
- 高精度和响应性
- 手势支持（滑动、缩放、旋转 - 取决于控制器）

**配置选项：**

- 坐标镜像（X/Y 轴）
- 坐标交换（交换 X 和 Y）
- 中断引脚支持
- 复位引脚支持

应用示例
----------------------

基本触摸屏初始化
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

以下是使用 I2C 通信初始化触摸屏的基本示例：

.. code:: c

    #include "esp_lcd_touch.h"
    #include "esp_lcd_touch_gt911.h"  // 以 GT911 为例
    #include "driver/i2c_master.h"

    // 初始化 I2C 总线
    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = GPIO_NUM_8,
        .scl_io_num = GPIO_NUM_18,
        .i2c_port = I2C_NUM_0,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_conf, &i2c_bus));

    // 初始化触摸 IO（I2C）
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    // 配置触摸屏
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 800,          // 屏幕宽度
        .y_max = 480,          // 屏幕高度
        .rst_gpio_num = GPIO_NUM_NC,  // 复位引脚（或 GPIO_NUM_NC）
        .int_gpio_num = GPIO_NUM_3,   // 中断引脚（或 GPIO_NUM_NC）
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    // 初始化触摸控制器
    esp_lcd_touch_handle_t touch_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(io_handle, &tp_cfg, &touch_handle));

读取触摸数据
^^^^^^^^^^^^^^^^^^^

读取触摸坐标：

.. code:: c

    // 读取触摸数据（定期调用）
    esp_lcd_touch_read_data(touch_handle);

    // 获取触摸坐标
    uint16_t touch_x[1];
    uint16_t touch_y[1];
    uint16_t touch_strength[1];
    uint8_t touch_cnt = 0;

    bool pressed = esp_lcd_touch_get_coordinates(touch_handle, 
                                                   touch_x, 
                                                   touch_y, 
                                                   touch_strength, 
                                                   &touch_cnt, 
                                                   1);

    if (pressed && touch_cnt > 0) {
        printf("触摸位置: X=%d, Y=%d, 强度=%d\n", 
               touch_x[0], touch_y[0], touch_strength[0]);
    }

与 LVGL 集成
^^^^^^^^^^^^^^^^^^^^^^^

ESP LVGL Adapter 组件提供了与 LVGL 的简易集成：

.. code:: c

    #include "esp_lv_adapter.h"

    // 在创建 touch_handle 后（如上所示）

    esp_lv_adapter_touch_config_t touch_cfg = 
        ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
    
    lv_indev_t *indev = esp_lv_adapter_register_touch(&touch_cfg);

使用 ESP-IoT-Solution 触摸驱动
---------------------------------------

添加组件到项目
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**方法 1：使用组件管理器**

使用 IDF 组件管理器将触摸驱动添加到项目：

.. code:: bash

    # 对于 SPD2010
    idf.py add-dependency "espressif/esp_lcd_touch_spd2010"

    # 对于 ILI2118  
    idf.py add-dependency "espressif/esp_lcd_touch_ili2118"

    # 对于 ST7123
    idf.py add-dependency "espressif/esp_lcd_touch_st7123"

**方法 2：使用 idf_component.yml**

在主组件中创建或编辑 ``idf_component.yml``：

.. code:: yaml

    dependencies:
      espressif/esp_lcd_touch_spd2010: "*"

SPD2010 示例
^^^^^^^^^^^^^^^^^^^^^

.. code:: c

    #include "esp_lcd_touch_spd2010.h"

    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_SPD2010_CONFIG();
    
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 320,
        .y_max = 320,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    esp_lcd_touch_handle_t tp;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_spd2010(io_handle, &tp_cfg, &tp));

触摸旋转和镜像
------------------------------

触摸坐标可以调整以匹配 LCD 方向：

**交换 XY 坐标：**

当触摸屏相对于 LCD 旋转 90° 或 270° 时很有用。

.. code:: c

    tp_cfg.flags.swap_xy = 1;  // 交换 X 和 Y 坐标

**镜像坐标：**

沿 X 或 Y 轴翻转坐标：

.. code:: c

    tp_cfg.flags.mirror_x = 1;  // 镜像 X 轴
    tp_cfg.flags.mirror_y = 1;  // 镜像 Y 轴

**设置分辨率：**

始终将 ``x_max`` 和 ``y_max`` 设置为与实际 LCD 分辨率匹配：

.. code:: c

    tp_cfg.x_max = 800;  // LCD 宽度
    tp_cfg.y_max = 480;  // LCD 高度

相关示例
------------------

- `LVGL Common Demo <https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_common_demo>`_ - 展示触摸与 LVGL 的集成
- `LVGL Dummy Draw <https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_dummy_draw>`_ - 带触摸的高级 LVGL 示例
- `RGB LCD with Touch <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/lcd/spi_lcd_touch>`_ - ESP-IDF 官方示例

组件链接
----------------

**ESP-IoT-Solution 触摸驱动：**

- `esp_lcd_touch_ili2118 <https://components.espressif.com/components/espressif/esp_lcd_touch_ili2118>`_
- `esp_lcd_touch_spd2010 <https://components.espressif.com/components/espressif/esp_lcd_touch_spd2010>`_
- `esp_lcd_touch_st7123 <https://components.espressif.com/components/espressif/esp_lcd_touch_st7123>`_

**组件注册表上的更多触摸驱动：**

搜索 `组件注册表上的 esp_lcd_touch <https://components.espressif.com/components?q=esp_lcd_touch>`_ 查找更多触摸屏驱动。

API 参考
--------------

详细的 API 文档请参考：

- `esp_lcd_touch API 文档 <https://github.com/espressif/esp-bsp/blob/master/components/lcd_touch/esp_lcd_touch/README.md>`_
- ``components/display/lcd_touch/`` 中组件特定的 README 文件
