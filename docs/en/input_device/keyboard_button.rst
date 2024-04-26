**Keyboard Scanning**
======================

:link_to_translation:`zh_CN:[中文]`

The `Keyboard Scanning Component <https://components.espressif.com/components/espressif/keyboard_button>`_ implements fast and efficient keyboard scanning, supporting key debouncing, key release and press event reporting, as well as combination keys.

This component uses matrix key row-column scanning and, through special circuit design, achieves full-key rollover circuit detection.

.. figure:: ../../_static/input_device/keyboard_button/keyboard_hardware.png
    :width: 650

- In this circuit, rows output high-level signals sequentially, detecting whether columns have high-level signals. If they do, it indicates the key is pressed.

.. note::

    - Since the logic of this component does not involve swapping row-column scanning, it is not suitable for traditional row-column scanning circuits and is only applicable to full-key rollover circuits for keyboards.

**Component Events**
----------------------

- :cpp:enumerator:`KBD_EVENT_PRESSED`: Reports data when there is a change in key states.

    * `key_pressed_num`: Number of keys pressed.
    * `key_release_num`: Number of keys released.
    * `key_change_num`: Number of keys with changed states compared to the previous state. **>0** indicates an increase in pressed keys, **<0** indicates a decrease.
    * `key_data`: Information of the currently pressed keys, with positions (x, y). Indexed in the order they were pressed, with smaller indexes being pressed earlier.
    * `key_release_data`: Information of keys released compared to the previous state, with positions (x, y).

- :cpp:enumerator:`KBD_EVENT_COMBINATION`: Combination key event. Triggered when a combination key is pressed.

    * `key_num`: Number of keys in the combination.
    * `key_data`: Position information of the combination keys. For setting a combination key (1,1) and (2,2), (1,1) must be pressed first followed by (2,2) to trigger the combination key event. Combination keys only trigger with increasing combinations.

**Application Example**
-------------------------

**Initializing Keyboard Scanning**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: c

    keyboard_btn_config_t cfg = {
        .output_gpios = (int[])
        {
            40, 39, 38, 45, 48, 47
        },
        .output_gpio_num = 6,
        .input_gpios = (int[])
        {
            21, 14, 13, 12, 11, 10, 9, 4, 5, 6, 7, 15, 16, 17, 18
        },
        .input_gpio_num = 15,
        .active_level = 1,
        .debounce_ticks = 2,
        .ticks_interval = 500,      // us
        .enable_power_save = false, // enable power save
    };
    keyboard_btn_handle_t kbd_handle = NULL;
    keyboard_button_create(&cfg, &kbd_handle);

**Registering Callback Functions**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Registration of `KBD_EVENT_PRESSED` event is as follows:

.. code:: C

    keyboard_btn_cb_config_t cb_cfg = {
        .event = KBD_EVENT_PRESSED,
        .callback = keyboard_cb,
    };
    keyboard_button_register_cb(kbd_handle, cb_cfg, NULL);

- Registration of `KBD_EVENT_COMBINATION` event requires passing combination key information through the `combination` member:

.. code:: C

    keyboard_btn_cb_config_t cb_cfg = {
        .event = KBD_EVENT_COMBINATION,
        .callback = keyboard_combination_cb1,
        .event_data.combination.key_num = 2,
        .event_data.combination.key_data = (keyboard_btn_data_t[]) {
            {5, 1},
            {1, 1},
        },
    };

    keyboard_button_register_cb(kbd_handle, cb_cfg, NULL);

.. note:: Additionally, multiple callbacks can be registered for each event. When registering multiple callbacks, it's recommended to save **keyboard_btn_cb_handle_t *rtn_cb_hdl** for later unbinding of specific callbacks.

**Key Scanning Efficiency**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Testing with `ESP32S3` chip scanning a `5*16` matrix keyboard, the maximum scanning rate can reach 20K.

**Low Power Support**
^^^^^^^^^^^^^^^^^^^^^^^^^^

- Set enable_power_save to true during initialization to activate the low-power mode. In this mode, key scanning is suspended when no key changes occur, allowing the CPU to enter a sleep state. The CPU wakes up when a key is pressed.

.. Note:: This feature only ensures that it does not occupy the CPU; it does not guarantee that the CPU will necessarily enter low-power mode. Currently, only Light Sleep mode is supported.

**API Reference**
---------------------

.. include-build-file:: inc/keyboard_button.inc
