LCD Touch 支持汇总
***********************

:link_to_translation:`en:[English]`

ESP LCD Touch 驱动基于 `esp_lcd_touch <https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch/esp_lcd_touch>`_ 抽象层实现。该抽象层统一触摸控制器的初始化、数据读取和坐标获取流程，应用侧通常通过 ``esp_lcd_touch_read_data()`` 读取控制器数据，再通过 ``esp_lcd_touch_get_data()`` 获取触摸点。

已维护组件
====================

.. list-table::
    :widths: 20 15 35 20
    :header-rows: 1

    * - 触摸控制器
      - 接口
      - 组件
      - 来源
    * - CST816S
      - I2C
      - `esp_lcd_touch_cst816s <https://components.espressif.com/components/espressif/esp_lcd_touch_cst816s>`_
      - esp-bsp
    * - FT5x06
      - I2C
      - `esp_lcd_touch_ft5x06 <https://components.espressif.com/components/espressif/esp_lcd_touch_ft5x06>`_
      - esp-bsp
    * - FT6336U
      - I2C
      - `esp_lcd_touch_ft6336u <https://components.espressif.com/components/espressif/esp_lcd_touch_ft6336u>`_
      - esp-iot-solution
    * - GT1151
      - I2C
      - `esp_lcd_touch_gt1151 <https://components.espressif.com/components/espressif/esp_lcd_touch_gt1151>`_
      - esp-bsp
    * - GT911
      - I2C
      - `esp_lcd_touch_gt911 <https://components.espressif.com/components/espressif/esp_lcd_touch_gt911>`_
      - esp-bsp
    * - ILI2118
      - I2C
      - `esp_lcd_touch_ili2118 <https://components.espressif.com/components/espressif/esp_lcd_touch_ili2118>`_
      - esp-iot-solution
    * - SPD2010
      - I2C
      - `esp_lcd_touch_spd2010 <https://components.espressif.com/components/espressif/esp_lcd_touch_spd2010>`_
      - esp-iot-solution
    * - ST7123
      - I2C
      - `esp_lcd_touch_st7123 <https://components.espressif.com/components/espressif/esp_lcd_touch_st7123>`_
      - esp-iot-solution
    * - ST77926
      - I2C
      - `esp_lcd_touch_st77926 <https://components.espressif.com/components/espressif/esp_lcd_touch_st77926>`_
      - esp-iot-solution
    * - STMPE610
      - SPI
      - `esp_lcd_touch_stmpe610 <https://components.espressif.com/components/espressif/esp_lcd_touch_stmpe610>`_
      - esp-bsp
    * - TT21100
      - I2C
      - `esp_lcd_touch_tt21100 <https://components.espressif.com/components/espressif/esp_lcd_touch_tt21100>`_
      - esp-bsp
