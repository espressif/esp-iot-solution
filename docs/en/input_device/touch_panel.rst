Touch Panel 
================
:link_to_translation:`zh_CN:[中文]`

Touch panels are now standard components in display applications. ESP-IoT-Solution provides drivers for common types of touch panels and currently supports the following controllers:

+-----------------------+------------------------+
| Resistive Touch Panel | Capacitive Touch Panel |
+=======================+========================+
|        XPT2046        |         FT5216         |
+-----------------------+------------------------+
|         NS2016        |         FT5436         |
+-----------------------+------------------------+
|                       |         FT6336         |
+-----------------------+------------------------+
|                       |         FT5316         |
+-----------------------+------------------------+

The capacitive touch panel controllers listed above can usually be driven by ``FT5x06``.

Similar to the screen driver, some common functions are encapsulated in the :cpp:type:`touch_panel_driver_t` structure, in order to port them to different GUI libraries easily. After initializing the touch panel, users can conduct operations by calling functions inside the structure, without paying attention to specific touch panel models.

Touch Panel Calibration
-----------------------------

In actual applications, resistive touch panels must be calibrated before use, while capacitive touch panels are usually calibrated by controllers and do not require extra calibration steps. A calibration algorithm is integrated in the resistive touch panel driver. During the process, three points are used to calibrate, in which one point is used for verification. If the verified error exceeds a certain threshold value, it means the calibration has failed and a new round of calibration is started automatically until it succeeds.

The calibration process will be started by calling :c:func:`calibration_run`. After finished, the parameters are stored in NVS for next initialization to avoid repetitive work.

Press Touch Panel
-------------------------

Generally, there is an interrupt pin inside the touch panel controller (both resistive and capacitive) to signal touch events. However, this is not used in the touch panel driver, because IOs should be saved for other peripherals in screen applications as much as possible; on the other hand the information in this signal is not as accurate as data in registers.

For resistive touch panels, when the pressure in the Z direction exceeds the configured threshold, it is considered as pressed; for capacitive touch panels, the detection of over one touch point will be considered as pressed.

Touch Panel Rotation
--------------------------

A touch panel has eight directions, like the screen, defined in :cpp:type:`touch_panel_dir_t`. The rotation of a touch panel is achieved by software, which usually sets the direction of a touch panel and a screen as the same. But this should not be fixed, for example, when using a capacitive touch panel, the inherent direction of the touch panel may not fit with the original display direction of the screen. Simply setting these two directions as the same may not show the desired contents. Therefore, please adjust the directions according to the actual situation.

On top of that, the configuration of its resolution is also important since the converted display after a touch panel being rotated relies on the resolution of its width and height. An incorrect configuration of the resolution may give you a distorted display.

.. note:: 
    If you are using a resistive touch panel, the touch position can become inaccurate after it being rotated, since the resistance value in each direction may not be distributed uniformly. It is recommended to not rotate a resistive touch panel after it being calibrated.

Application Example
-------------------------

Initialize a Touch Panel
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

    - When using a capacitive touch panel, the call to the calibration function will return ``ESP_OK`` directly.
    - By default, only FT5x06 touch panel driver is enabled, please go to ``menuconfig -> Component config -> Touch Screen Driver -> Choose Touch Screen Driver`` to do configurations if you need to enable other drivers.

To Know If a Touch Panel is Pressed and Its Corresponding Position 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: c

    touch_panel_points_t points;
    touch.read_point_data(&points);
    int32_t x = points.curx[0];
    int32_t y = points.cury[0];
    if(TOUCH_EVT_PRESS == points.event) {
        ESP_LOGI(TAG, "Pressed, Touch point at (%d, %d)", x, y);
    }

API Reference
-----------------

.. include:: /_build/inc/touch_panel.inc
