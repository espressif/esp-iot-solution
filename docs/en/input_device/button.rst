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

+--------------------------+-----------------------------------------------+
|          Event           |              Trigger Conditions               |
+==========================+===============================================+
| BUTTON_PRESS_DOWN        | Button press down                             |
+--------------------------+-----------------------------------------------+
| BUTTON_PRESS_UP          | Button release                                |
+--------------------------+-----------------------------------------------+
| BUTTON_PRESS_REPEAT      | Button press down and release (>= 2-times)    |
+--------------------------+-----------------------------------------------+
| BUTTON_SINGLE_CLICK      | Button press down and release (single time)   |
+--------------------------+-----------------------------------------------+
| BUTTON_DOUBLE_CLICK      | Button press down and release (double times)  |
+--------------------------+-----------------------------------------------+
| BUTTON_LONG_PRESS_START  | Button press reaches the long-press threshold |
+--------------------------+-----------------------------------------------+
| BUTTON_LONG_PRESS_HOLD   | Button press for long time                    |
+--------------------------+-----------------------------------------------+
| BUTTON_PRESS_REPEAT_DONE | Multiple press down and release               |
+--------------------------+-----------------------------------------------+

Each button supports **call-back** and **pooling** mode.

- Call-back: Each event of a button can register a call-back function for it, and the call-back function will be called when an event is generated. This method has high efficiency and real-time performance, and no events will be lost. 
- Polling: Periodically call :c:func:`iot_button_get_event` in the program to query the current event of the button. This method is easy to use and is suitable for occasions with simple tasks

.. note:: you can also combine the above two methods.

.. attention:: No blocking operations such as **TaskDelay** are allowed in the call-back function

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

.. Note::
    please pass adc_handle and adc_channel , when there are other places in the project that use ADC1.

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

.. code:: c

    static void button_single_click_cb(void *arg,void *usr_data)
    {
        ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
    }

    iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, button_single_click_cb,NULL);

Find an event
^^^^^^^^^^^^^^

.. code:: c

    button_event_t event;
    event = iot_button_get_event(button_handle);

API Reference
-----------------

.. include-build-file:: inc/iot_button.inc