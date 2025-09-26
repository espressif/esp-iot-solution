Touch Panel 
================
:link_to_translation:`zh_CN:[中文]`

Touch panels are now standard components in display applications. ESP-IoT-Solution provides drivers for common types of touch panel controllers based on the `esp_lcd_touch <https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch/esp_lcd_touch>`_ component from ESP-BSP.

Supported Touch Controllers
-----------------------------

The following touch panel controllers are currently supported in ESP-IoT-Solution:

+-----------------------+------------------------+---------------------------+
| Touch Controller      | Communication Interface| Component Name            |
+=======================+========================+===========================+
| ILI2118               | I2C                    | esp_lcd_touch_ili2118     |
+-----------------------+------------------------+---------------------------+
| SPD2010               | I2C                    | esp_lcd_touch_spd2010     |
+-----------------------+------------------------+---------------------------+
| ST7123                | I2C                    | esp_lcd_touch_st7123      |
+-----------------------+------------------------+---------------------------+

For additional touch controllers, please refer to the `esp_lcd_touch drivers <https://components.espressif.com/components?q=esp_lcd_touch>`_ available on the ESP Component Registry.

Common touch controllers supported by ESP-IDF and ESP-BSP include:

- **GT911** / **GT1151** - Capacitive touch controllers
- **FT5x06** series (FT5216, FT5316, FT5436, FT6336) - Capacitive touch controllers  
- **CST816S** - Capacitive touch controller
- **TT21100** - Capacitive touch controller

Architecture
-------------

The ``esp_lcd_touch`` component provides a unified abstraction layer for different touch panel controllers. This architecture allows:

- **Unified API**: Same interface for all touch controllers
- **Easy Integration**: Seamless integration with LVGL and other GUI libraries
- **Hardware Abstraction**: Switch between touch controllers without changing application code
- **I2C Communication**: Efficient I2C-based communication with touch controllers

Touch Panel Features
----------------------

**Capacitive Touch Panels:**

- Multi-touch support (depends on controller)
- No calibration required (calibrated by controller)
- High accuracy and responsiveness
- Gesture support (swipe, zoom, rotate - controller dependent)

**Configuration Options:**

- Coordinate mirroring (X/Y axis)
- Coordinate swapping (swap X and Y)
- Interrupt pin support
- Reset pin support

Application Example
----------------------

Basic Touch Panel Initialization
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Here's a basic example of initializing a touch panel with I2C communication:

.. code:: c

    #include "esp_lcd_touch.h"
    #include "esp_lcd_touch_gt911.h"  // Example with GT911
    #include "driver/i2c_master.h"

    // Initialize I2C bus
    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = GPIO_NUM_8,
        .scl_io_num = GPIO_NUM_18,
        .i2c_port = I2C_NUM_0,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_conf, &i2c_bus));

    // Initialize touch IO (I2C)
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    // Configure touch panel
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 800,          // Screen width
        .y_max = 480,          // Screen height
        .rst_gpio_num = GPIO_NUM_NC,  // Reset pin (or GPIO_NUM_NC)
        .int_gpio_num = GPIO_NUM_3,   // Interrupt pin (or GPIO_NUM_NC)
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

    // Initialize touch controller
    esp_lcd_touch_handle_t touch_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(io_handle, &tp_cfg, &touch_handle));

Reading Touch Data
^^^^^^^^^^^^^^^^^^^

To read touch coordinates:

.. code:: c

    // Read touch data (call this periodically)
    esp_lcd_touch_read_data(touch_handle);

    // Get touch coordinates
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
        printf("Touch at: X=%d, Y=%d, Strength=%d\n", 
               touch_x[0], touch_y[0], touch_strength[0]);
    }

Integration with LVGL
^^^^^^^^^^^^^^^^^^^^^^^

The ESP LVGL Adapter component provides easy integration with LVGL:

.. code:: c

    #include "esp_lv_adapter.h"

    // After creating touch_handle as shown above

    esp_lv_adapter_touch_config_t touch_cfg = 
        ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
    
    lv_indev_t *indev = esp_lv_adapter_register_touch(&touch_cfg);

Using ESP-IoT-Solution Touch Drivers
---------------------------------------

Adding Components to Your Project
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Method 1: Using Component Manager**

Add the touch driver to your project using the IDF Component Manager:

.. code:: bash

    # For SPD2010
    idf.py add-dependency "espressif/esp_lcd_touch_spd2010"

    # For ILI2118  
    idf.py add-dependency "espressif/esp_lcd_touch_ili2118"

    # For ST7123
    idf.py add-dependency "espressif/esp_lcd_touch_st7123"

**Method 2: Using idf_component.yml**

Create or edit ``idf_component.yml`` in your main component:

.. code:: yaml

    dependencies:
      espressif/esp_lcd_touch_spd2010: "*"

Example with SPD2010
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

Touch Rotation and Mirroring
------------------------------

Touch coordinates can be adjusted to match your LCD orientation:

**Swap XY Coordinates:**

Useful when the touch panel is rotated 90° or 270° relative to the LCD.

.. code:: c

    tp_cfg.flags.swap_xy = 1;  // Swap X and Y coordinates

**Mirror Coordinates:**

Flip the coordinates along X or Y axis:

.. code:: c

    tp_cfg.flags.mirror_x = 1;  // Mirror X axis
    tp_cfg.flags.mirror_y = 1;  // Mirror Y axis

**Setting Resolution:**

Always set ``x_max`` and ``y_max`` to match your actual LCD resolution:

.. code:: c

    tp_cfg.x_max = 800;  // LCD width
    tp_cfg.y_max = 480;  // LCD height

Related Examples
------------------

- `LVGL Common Demo <https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_common_demo>`_ - Shows touch integration with LVGL
- `LVGL Dummy Draw <https://github.com/espressif/esp-iot-solution/tree/master/examples/display/gui/lvgl_dummy_draw>`_ - Advanced LVGL example with touch
- `RGB LCD with Touch <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/lcd/spi_lcd_touch>`_ - ESP-IDF official example

Component Links
----------------

**ESP-IoT-Solution Touch Drivers:**

- `esp_lcd_touch_ili2118 <https://components.espressif.com/components/espressif/esp_lcd_touch_ili2118>`_
- `esp_lcd_touch_spd2010 <https://components.espressif.com/components/espressif/esp_lcd_touch_spd2010>`_
- `esp_lcd_touch_st7123 <https://components.espressif.com/components/espressif/esp_lcd_touch_st7123>`_

**More Touch Drivers on Component Registry:**

Search for `esp_lcd_touch on Component Registry <https://components.espressif.com/components?q=esp_lcd_touch>`_ to find additional touch panel drivers.

API Reference
--------------

For detailed API documentation, please refer to:

- `esp_lcd_touch API Documentation <https://github.com/espressif/esp-bsp/blob/master/components/lcd_touch/esp_lcd_touch/README.md>`_
- Component-specific README files in ``components/display/lcd_touch/``
