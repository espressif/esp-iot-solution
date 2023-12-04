数码管驱动
============
:link_to_translation:`en:[English]`

数码管/LED 点阵是嵌入式系统中常见的显示方案，该方案比 LCD 显示屏占用更少的引脚和内存资源，实现也更加简单，比较适合计时、计数、状态显示等具有单一显示需求的应用场景。 

ESP-IoT-Solution 已经适配的数码管/LED 显示驱动器如下：

+------------+-------------------------------------------+------+-----------------------------------------------------------+-------------------------------------------------------------------------------------+
|    名称    |                   功能                    | 接口 |                           驱动                            |                                      数据手册                                       |
+============+===========================================+======+===========================================================+=====================================================================================+
| CH450      | 数码管显示驱动芯片，支持 6 位数码管       | I2C  | :component:`ch450 <display/digital_tube/ch450>`           | `CH450 <https://pdf1.alldatasheet.com/datasheet-pdf/view/1145655/WCH/CH450H.html>`_ |
+------------+-------------------------------------------+------+-----------------------------------------------------------+-------------------------------------------------------------------------------------+
| HT16C21    | 20×4/16×8 LCD 控制器，支持 RAM 映射       | I2C  | :component:`ht16c21 <display/digital_tube/ht16c21>`       | `HT16C21 <https://www.holtek.com.tw/documents/10179/11842/HT16C21v110.pdf>`_        |
+------------+-------------------------------------------+------+-----------------------------------------------------------+-------------------------------------------------------------------------------------+
| IS31FL3XXX | LED 点阵控制器                            | I2C  | :component:`is31fl3xxx <display/digital_tube/is31fl3xxx>` | `IS31FL3XXX <https://www.alldatasheet.com/view.jsp?Searchword=IS31FL3&sField=2>`_   |
+------------+-------------------------------------------+------+-----------------------------------------------------------+-------------------------------------------------------------------------------------+

CH450 驱动
-------------

CH450 是一款数码管显示驱动芯片，可以用于驱动 6 位数码管或 48 点 LED 矩阵，可通过 ``I2C`` 接口与 ESP32 进行通信。


.. figure:: ../../_static/ch450_typical_application_circuit.png
    :align: center
    :width: 70%

    CH450 典型应用电路图

该驱动对 CH450 的基本操作进行了封装，用户可以直接调用 :c:func:`ch450_write` 或 :c:func:`ch450_write_num` 接口在数码管上进行数字显示。

示例
^^^^^^^^

.. code:: c

    i2c_bus_handle_t i2c_bus = NULL;
    ch450_handle_t seg = NULL;
    i2c_config_t conf = {    
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    seg = ch450_create(i2c_bus);

    for (size_t i = 0; i < 10; i++) {
        for (size_t index = 0; index < 6; index++) {
            ch450_write_num(seg, index, i);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ch450_delete(seg);
    i2c_bus_delete(&i2c_bus);


HT16C21 驱动
----------------

HT16C21 是一款支持 RAM 映射的 LCD 控制/驱动芯片，可用于驱动 ``20 x 4`` 或 ``16 x 8`` 段码式液晶屏，该芯片通过 ``I2C`` 接口与 ESP32 进行通信。

.. figure:: ../../_static/ht16c21_drive_mode_waveform.png
   :align: center
   :width: 60%

   HT16C21 典型驱动模型

该驱动对 HT16C21 的基本操作进行了封装，用户使用 ``ht16c21_create`` 创建实例之后，通过 ``ht16c21_param_config`` 对驱动器参数进行配置，之后即可直接调用 ``ht16c21_ram_write`` 进行写入操作。

示例
^^^^^^^^

.. code:: c

    i2c_bus_handle_t i2c_bus = NULL;
    ht16c21_handle_t seg = NULL;
    uint8_t lcd_data[8] = { 0x10, 0x20, 0x30, 0x50, 0x60, 0x70, 0x80 };

    i2c_config_t conf = {    
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    seg = ht16c21_create(i2c_bus, HT16C21_I2C_ADDRESS_DEFAULT);

    ht16c21_config_t ht16c21_conf = {    
        .duty_bias = HT16C21_4DUTY_3BIAS;
        .oscillator_display = HT16C21_OSCILLATOR_ON_DISPLAY_ON;
        .frame_frequency = HT16C21_FRAME_160HZ;
        .blinking_frequency = HT16C21_BLINKING_OFF;
        .pin_and_voltage = HT16C21_VLCD_PIN_VOL_ADJ_ON;
        .adjustment_voltage = 0;
    };
    ht16c21_param_config(seg, &ht16c21_conf);
    ht16c21_ram_write(seg, 0x00, lcd_data, 8);

    ht16c21_delete(seg);
    i2c_bus_delete(&i2c_bus);


IS31FL3XXX 驱动
-------------------

IS31FL3XXX 系列芯片可用于驱动不同规模的 LED 点阵屏幕。其中 IS31FL3218 支持 18 个恒流通道，每个通道由独立的 PWM 控制，最大输出电流 38 mA，可直接驱动 LED 进行显示。IS31FL3736 支持更多的通道，最大可组成 ``12 x 8`` LED 矩阵，每个通道由一个 8 位 PWM 驱动，最大支持 256 级渐变。

.. figure:: ../../_static/IS31FL3218_typical_application_circuit.png
   :align: center
   :width: 70%

   IS31FL3218 典型应用电路图

该驱动对 IS31FL3XXX 的基本操作进行了封装，示例如下节所示。

IS31FL3218 示例
^^^^^^^^^^^^^^^^^^^^

.. code:: c

    i2c_bus_handle_t i2c_bus = NULL;
    is31fl3218_handle_t fxled = NULL;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    fxled = is31fl3218_create(i2c_bus);
    is31fl3218_channel_set(fxled, 0x00ff, 128); // set PWM 1 ~ PWM 8 duty cycle 50%
    is31fl3218_delete(fxled);
    i2c_bus_delete(&i2c_bus);

