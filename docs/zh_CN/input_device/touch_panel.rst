触摸屏驱动
===============
:link_to_translation:`en:[English]`

触摸屏现已是显示屏应用中的标配，ESP-IoT-Solution 提供了常见类型的触摸屏驱动，已支持如下控制芯片：

+------------+------------+
| 电阻触摸屏 | 电容触摸屏 |
+============+============+
|   XPT2046  |   FT5216   |
+------------+------------+
|   NS2016   |   FT5436   |
+------------+------------+
|            |   FT6336   |
+------------+------------+
|            |   FT5316   |
+------------+------------+

上面列出的电容触摸屏控制芯片通常可使用 ``FT5x06`` 驱动。

与屏幕驱动相似，为了方便移植到不同的 GUI 库，将一部分通用的函数封装到了一个 :cpp:type:`touch_panel_driver_t` 结构体中。完成初始化后将通过调用结构体里面的函数完成对触摸屏的操作，而无需关心是具体哪一个触摸屏型号。

校准触摸屏
-----------

在实际应用中，电阻触摸屏必须在使用前进行校准，而电容触摸屏则一般由控制芯片完成该工作，无需额外的校准步骤。驱动中已经集成了电阻触摸屏的校准算法，校准过程使用了三个点来校准，用一个点来验证，当最后验证的误差大于某个阈值将导致校准失败，然后自动重新进行校准，直到校准成功。

调用校准函数 :c:func:`calibration_run` 将会在屏幕上开始校准的过程，校准完成后，参数将保存在 NVS 中用于下次启动，避免每次使用前的重复校准。

触摸屏的按下
-------------

不论是电阻还是电容触摸屏，通常的触摸屏控制芯片会有一个用于通知触摸事件的中断引脚。但是驱动中没有使用该信号，一方面是因为对于有屏幕的应用需要尽量节省出 IO 给其他外设；另一方面是触摸控制器给出的该信号不如程序通过寄存器数据判断的准确性高。

对于电阻触摸屏来说，判断按下的依据是 Z 方向的压力大于配置的阈值；对于电容触摸屏则是判断至少有一个触摸点存在。

触摸屏的旋转
-------------

触摸屏具有与显示屏一样的 8 个方向，定义在 :cpp:type:`touch_panel_dir_t` 中。这里的旋转是通过软件换算来实现的，通常把二者的方向设置为相同。但这并不是一成不变的，例如：在使用电容触摸屏时，有可能触摸屏固有的方向与显示屏原始显示方向不一致，如果简单的将这两个方向设置为相同后，将无法正确的点击屏幕内容，这时需要根据实际情况调整。

触摸屏的分辨率设置也是很重要的，因为触摸屏旋转后的换算依赖于触摸屏的宽和高分辨率大小，设置不当将无法得到正确的旋转效果。

.. note:: 
    在使用电阻屏时，由于电阻屏在每个方向上的电阻值不一定均匀分布，这会导致经过旋转换算后触摸位置的不准确，所以建议电阻屏校准后不再进行旋转操作。

应用示例
------------

初始化触摸屏
^^^^^^^^^^^^^^^^^^

.. code:: c

    touch_panel_driver_t touch; // a touch panel driver

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 35,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = 36,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_NUM_0, &i2c_conf);

    touch_panel_config_t touch_cfg = {
        .interface_i2c = {
            .i2c_bus = i2c_bus,
            .clk_freq = 100000,
            .i2c_addr = 0x38,
        },
        .interface_type = TOUCH_PANEL_IFACE_I2C,
        .pin_num_int = -1,
        .direction = TOUCH_DIR_LRTB,
        .width = 800,
        .height = 480,
    };

    /* Initialize touch panel controller FT5x06 */
    touch_panel_find_driver(TOUCH_PANEL_CONTROLLER_FT5X06, &touch);
    touch.init(&touch_cfg);

    /* start to run calibration */
    touch.calibration_run(&lcd, false);

.. note::

    - 当使用的是电容触摸屏时，调用校准函数将直接返回 ``ESP_OK``。

    - 默认情况下只打开了 FT5x06 触摸屏的驱动，如果要使用其他的驱动，需要在 ``menuconfig -> Component config -> Touch Screen Driver -> Choose Touch Screen Driver`` 中使能对应驱动。

获取触摸屏是否按下及其触点坐标
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: c

    touch_panel_points_t points;
    touch.read_point_data(&points);
    int32_t x = points.curx[0];
    int32_t y = points.cury[0];
    if(TOUCH_EVT_PRESS == points.event) {
        ESP_LOGI(TAG, "Pressed, Touch point at (%d, %d)", x, y);
    }

API 参考
-----------------

.. include:: /_build/inc/touch_panel.inc
