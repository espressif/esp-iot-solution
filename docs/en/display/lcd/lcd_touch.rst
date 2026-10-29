LCD Touch Support
***********************

:link_to_translation:`zh_CN:[中文]`

ESP LCD Touch drivers are based on the `esp_lcd_touch <https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch/esp_lcd_touch>`_ abstraction. Applications usually call ``esp_lcd_touch_read_data()`` to read controller data and then call ``esp_lcd_touch_get_data()`` to fetch touch points.

Maintained Components
=====================

.. list-table::
    :widths: 20 15 35 20
    :header-rows: 1

    * - Touch controller
      - Interface
      - Component
      - Source
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
      - :component:`esp_lcd_touch_ft6336u <display/lcd_touch/esp_lcd_touch_ft6336u>`
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
      - :component:`esp_lcd_touch_st77926 <display/lcd_touch/esp_lcd_touch_st77926>`
      - esp-iot-solution
    * - STMPE610
      - SPI
      - `esp_lcd_touch_stmpe610 <https://components.espressif.com/components/espressif/esp_lcd_touch_stmpe610>`_
      - esp-bsp
    * - TT21100
      - I2C
      - `esp_lcd_touch_tt21100 <https://components.espressif.com/components/espressif/esp_lcd_touch_tt21100>`_
      - esp-bsp
