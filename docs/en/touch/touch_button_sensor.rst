Touch Button Sensor
=====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------

The ``touch_button_sensor`` component provides enhanced touch button detection functionality for ESP32 series chips. It uses multiple frequency sampling (for ESP32-P4) and FSM-based processing to provide reliable touch detection even in noisy environments.

.. note::
   - ESP32/ESP32-S2/ESP32-S3 touch-related components are intended for testing or demo purposes only. Due to the poor anti-interference capability of the touch functionality, it may not pass EMS testing, and therefore, it is not recommended for mass production products.
   - This component is currently applicable to ESP32, ESP32-S2, and ESP32-S3, and requires an IDF version greater than or equal to v5.3.

Key Features
^^^^^^^^^^^^^^

* Multi-frequency touch sampling for improved noise immunity
* FSM-based touch detection with configurable thresholds
* Debounce support for reliable state changes
* Support for multiple touch channels
* Callback-based event notification

Dependencies
^^^^^^^^^^^^^^

* touch_sensor_fsm
* touch_sensor_lowlevel

Configuration Parameters
-----------------------------

Touch Button Configuration Structure
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The touch button can be configured via the :cpp:type:`touch_button_config_t` structure:

.. code:: c

    typedef struct {
        uint32_t channel_num;          /*!< Number of touch button sensor channels */
        uint32_t *channel_list;        /*!< Touch channel list */
        float *channel_threshold;       /*!< Threshold for touch detection for each channel */
        uint32_t *channel_gold_value;   /*!< (Optional) Reference values for touch channels */
        uint32_t debounce_times;        /*!< Number of consecutive readings needed to confirm state change */
        bool skip_lowlevel_init;        /*!< Skip low level initialization when working with existing touch driver */
    } touch_button_config_t;

Parameter Descriptions
^^^^^^^^^^^^^^^^^^^^^^^

+--------------------+------------------------------------------------+---------------+
|     Parameter      |                  Description                   | Default Value |
+====================+================================================+===============+
| channel_num        | Number of touch button sensor channels         | -             |
+--------------------+------------------------------------------------+---------------+
| channel_list       | Array of touch channel numbers to use          | -             |
+--------------------+------------------------------------------------+---------------+
| channel_threshold  | Array of threshold values for each channel     | -             |
+--------------------+------------------------------------------------+---------------+
| channel_gold_value | Reference values for touch channels (optional) | NULL          |
+--------------------+------------------------------------------------+---------------+
| debounce_times     | Consecutive readings for state change          | 3             |
+--------------------+------------------------------------------------+---------------+
| skip_lowlevel_init | Skip touch driver initialization if exists     | false         |
+--------------------+------------------------------------------------+---------------+

API Usage Examples
---------------------

Create and Initialize
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: c

    touch_button_config_t config = {
        .channel_num = 1,
        .channel_list = (uint32_t[]){8},            // Using touch channel 8
        .channel_threshold = (float[]){0.02},       // 2% change threshold
        .channel_gold_value = NULL,                  // No reference values
        .debounce_times = 3,                        // 3 readings to confirm
        .skip_lowlevel_init = false                 // Initialize touch hardware
    };
    touch_button_handle_t btn_handle = NULL;
    esp_err_t ret = touch_button_sensor_create(&config, &btn_handle, button_state_callback, NULL);

State Change Callback
^^^^^^^^^^^^^^^^^^^^^^^^^^

The callback function is called when a touch button state changes:

.. code:: c

    void button_state_callback(touch_button_handle_t handle, uint32_t channel, touch_state_t state, void *arg)
    {
        switch (state) {
            case TOUCH_STATE_ACTIVE:
                printf("Button channel %d pressed\n", channel);
                break;
            case TOUCH_STATE_INACTIVE:
                printf("Button channel %d released\n", channel);
                break;
        }
    }


Event Handling
^^^^^^^^^^^^^^^^

The touch button sensor component provides an event handling mechanism to process touch events in a non-blocking way. Events should be handled periodically in your application's main loop or in a dedicated task.

.. code:: c

    // In your main loop or task
    while (1) {
        // Process any pending touch events
        touch_button_sensor_handle_events(btn_handle);
        
        // Add delay to prevent tight loop
        vTaskDelay(pdMS_TO_TICKS(20));  // 20ms interval is typically sufficient
    }


Examples
--------

- :example:`touch/touch_button_sensor`

API Reference
---------------

.. include-build-file:: inc/touch_button_sensor.inc
