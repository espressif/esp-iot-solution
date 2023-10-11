**Button**
===========

:link_to_translation:`zh_CN:[中文]`

The button component supports GPIO and ADC mode, and it allows the creation of two different kinds of the button at the same time. The following figure shows the hardware design of the button:

.. figure:: ../../_static/button_hardware.png
    :width: 650

- GPIO button: The advantage of the GPIO button is that each button occupies an independent IO and therefore does not affect each other, and has high stability however when the number of buttons increases, it may take too many IO resources.
- ADC button: The advantage of using the ADC button is that one ADC channel can share multiple buttons and occupy fewer IO resources. The disadvantages include that you cannot press multiple buttons at the same time, and instability increases due to increase in the closing resistance of the button due to oxidation and other factors. 

.. note:: 

    - The GPIO button needs to pay attention to the problem of pull-up and pull-down resistor inside the chip, which will be enabled by default. But there is no such resistor inside the IO that only supports input, **external connection requires**.
    - The voltage of the ADC button should not exceed the ADC range. 

Button event
------------

Triggering conditions for each button event are enlisted in the table below:


+--------------------------+-----------------------------------+
|          Event           |         Trigger Condition         |
+==========================+===================================+
| BUTTON_PRESS_DOWN        | Pressed                           |
+--------------------------+-----------------------------------+
| BUTTON_PRESS_UP          | Released                          |
+--------------------------+-----------------------------------+
| BUTTON_PRESS_REPEAT      | Pressed and released >= 2 times   |
+--------------------------+-----------------------------------+
| BUTTON_PRESS_REPEAT_DONE | Repeated press completed          |
+--------------------------+-----------------------------------+
| BUTTON_SINGLE_CLICK      | Pressed and released once         |
+--------------------------+-----------------------------------+
| BUTTON_DOUBLE_CLICK      | Pressed and released twice        |
+--------------------------+-----------------------------------+
| BUTTON_MULTIPLE_CLICK    | Pressed and released N times      |
|                          | specified, triggers when achieved |
+--------------------------+-----------------------------------+
| BUTTON_LONG_PRESS_START  | Instant when held for a threshold |
|                          | duration of time                  |
+--------------------------+-----------------------------------+
| BUTTON_LONG_PRESS_HOLD   | Triggered continuously during     |
|                          | long press                        |
+--------------------------+-----------------------------------+
| BUTTON_LONG_PRESS_UP     | Released after a long press       |
+--------------------------+-----------------------------------+
| BUTTON_PRESS_REPEAT_DONE | Repeated press and release ended  |
+--------------------------+-----------------------------------+

Each button supports **call-back** and **pooling** mode.

- Call-back: Each event of a button can register a call-back function for it, and the call-back function will be called when an event is generated. This method has high efficiency and real-time performance, and no events will be lost. 
- Polling: Periodically call :c:func:`iot_button_get_event` in the program to query the current event of the button. This method is easy to use and is suitable for occasions with simple tasks

.. note:: you can also combine the above two methods.

.. attention:: No blocking operations such as **TaskDelay** are allowed in the call-back function

.. image:: https://dl.espressif.com/button_v2/button.svg
   :alt: Button

Configuration
-------------

- BUTTON_PERIOD_TIME_MS : scan cycle

- BUTTON_DEBOUNCE_TICKS : debounce time

- BUTTON_SHORT_PRESS_TIME_MS : short press down effective time 

- BUTTON_LONG_PRESS_TIME_MS : long press down effective time 

- ADC_BUTTON_MAX_CHANNEL : maximum number of channel for ADC

- ADC_BUTTON_MAX_BUTTON_PER_CHANNEL : maximum number of ADC buttons per channel

- ADC_BUTTON_SAMPLE_TIMES : ADC sample time

- BUTTON_SERIAL_TIME_MS : call-back interval triggered by long press time

- BUTTON_LONG_PRESS_TOLERANCE_MS: Used to set the tolerance time for long presses.

Demonstration
--------------

Create a button
^^^^^^^^^^^^^^^^
.. code:: c

    // create gpio button
    button_config_t gpio_btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_ticks = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_ticks = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = 0,
            .active_level = 0,
        },
    };
    button_handle_t gpio_btn = iot_button_create(&gpio_btn_cfg);
    if(NULL == gpio_btn) {
        ESP_LOGE(TAG, "Button create failed");
    }

    // create adc button
    button_config_t adc_btn_cfg = {
        .type = BUTTON_TYPE_ADC,
        .long_press_ticks = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_ticks = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .adc_button_config = {
            .adc_channel = 0,
            .button_index = 0,
            .min = 100,
            .max = 400,
        },
    };
    button_handle_t adc_btn = iot_button_create(&adc_btn_cfg);
    if(NULL == adc_btn) {
        ESP_LOGE(TAG, "Button create failed");
    }

    // create matrix keypad button
    button_config_t matrix_button_cfg = {
        .type = BUTTON_TYPE_MATRIX,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .matrix_button_config = {
            .row_gpio_num = 0,
            .col_gpio_num = 1,
        }
    };
    button_handle_t matrix_button = iot_button_create(&matrix_button_cfg);
    if(NULL == matrix_button) {
        ESP_LOGE(TAG, "Button create failed");
    }

.. Note::
    When the IDF version is greater than or equal to release/5.0, the ADC button uses ADC1. If ADC1 is used elsewhere in the project, please provide the `adc_handle` and `adc_channel` to configure the ADC button.

    .. code::C
        adc_oneshot_unit_handle_t adc1_handle;
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
        };
        //-------------ADC1 Init---------------//
        adc_oneshot_new_unit(&init_config1, &adc1_handle);
        // create adc button
        button_config_t adc_btn_cfg = {
            .type = BUTTON_TYPE_ADC,
            .adc_button_config = {
                .adc_handle = &adc1_handle,
                .adc_channel = 1,
            },
        };
        button_handle_t adc_btn = iot_button_create(&adc_btn_cfg);
        if(NULL == adc_btn) {
            ESP_LOGE(TAG, "Button create failed");
        }

Register call-back 
^^^^^^^^^^^^^^^^^^^

The Button component supports registering callback functions for multiple events, with each event capable of having its own callback function. When an event occurs, the callback function will be invoked.

In this context:

- The :cpp:enumerator:`BUTTON_LONG_PRESS_START` and :cpp:enumerator:`BUTTON_LONG_PRESS_UP` enumerations support setting specific long press times.
- The :cpp:enumerator:`BUTTON_MULTIPLE_CLICK` enumeration supports setting the number of consecutive button presses.

- Here's a simple example:

    .. code:: c

        static void button_single_click_cb(void *arg,void *usr_data)
        {
            ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
        }

        iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, button_single_click_cb,NULL);


- And here's an example involving multiple callback functions:

    .. code:: C
        
        static void button_long_press_1_cb(void *arg,void *usr_data)
        {
            ESP_LOGI(TAG, "BUTTON_LONG_PRESS_START_1");
        }

        static void button_long_press_2_cb(void *arg,void *usr_data)
        {
            ESP_LOGI(TAG, "BUTTON_LONG_PRESS_START_2");
        }
        button_event_config_t cfg = {
            .event = BUTTON_LONG_PRESS_START,
            .event_data.long_press.press_time = 2000,
        };

        iot_button_register_event_cb(gpio_btn, cfg, BUTTON_LONG_PRESS_START, button_long_press_1_cb, NULL);

        cfg.event_data.long_press.press_time = 5000;
        iot_button_register_event_cb(gpio_btn, cfg, BUTTON_LONG_PRESS_START, button_long_press_2_cb, NULL);

Dynamically Modifying Default Button Values
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: c

    iot_button_set_param(btn, BUTTON_LONG_PRESS_TIME_MS, 5000);

Find an event
^^^^^^^^^^^^^^

.. code:: c

    button_event_t event;
    event = iot_button_get_event(button_handle);

Low power
^^^^^^^^^^^

.. code:: c

    // stop button
    iot_button_stop();
    // resume button
    iot_button_resume();

API Reference
-----------------

.. include-build-file:: inc/iot_button.inc