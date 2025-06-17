Touch Button Sensor
=====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------

The ``touch_slider_sensor`` component provides enhanced touch slider detection functionality for ESP32 series chips.

.. note::
   - ESP32/ESP32-S2/ESP32-S3 touch-related components are intended for testing or demo purposes only. Due to the poor anti-interference capability of the touch functionality, it may not pass EMS testing, and therefore, it is not recommended for mass production products.
   - This component is currently applicable to ESP32, ESP32-S2, and ESP32-S3, and requires an IDF version greater than or equal to v5.3.

Key Features
^^^^^^^^^^^^^^

* FSM-based touch detection with configurable thresholds
* Support for slider gesture detection
* Callback-based event notification

Dependencies
^^^^^^^^^^^^^^

* touch_sensor_fsm
* touch_sensor_lowlevel

Configuration Parameters
-----------------------------

Touch Slider Configuration Structure
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The touch button can be configured via the :cpp:type:`touch_slider_config_t` structure:

.. code:: c

    typedef struct {
        uint32_t channel_num;         /*!< Number of touch slider sensor channels */
        uint32_t *channel_list;       /*!< Touch channel list */
        float *channel_threshold;     /*!< Threshold for touch detection for each channel */
        uint32_t *channel_gold_value; /*!< (Optional) Reference values for touch channels */
        uint32_t debounce_times;      /*!< Number of consecutive readings needed to confirm state change */
        uint32_t filter_reset_times;  /*!< Number of consecutive readings to reset position filter */
        uint32_t position_range;       /*!< Maximum position value of touch slider, range [0, position_range]. Higher values provide better position resolution */
        uint8_t calculate_window;    /*!< Window size for position calculation (should be <= channel_num). Set to 0 for auto-default: 2 for 2-channel, 3 for 3+ channels */
        float swipe_threshold;        /*!< The threshold for identifying swiping */
        float swipe_hysterisis;       /*!< The hysterisis for identifying swiping */
        float swipe_alpha;            /*!< Filter parameter for estimating speed */
        bool skip_lowlevel_init;      /*!< Skip low level initialization when working with existing touch driver */
    } touch_slider_config_t;

Parameter Descriptions
^^^^^^^^^^^^^^^^^^^^^^^

+--------------------+------------------------------------------------------------+---------------+
|     Parameter      |                        Description                         | Default Value |
+====================+============================================================+===============+
| channel_num        | Number of touch slider sensor channels                     | -             |
+--------------------+------------------------------------------------------------+---------------+
| channel_list       | Array of touch channel numbers to use                      | -             |
+--------------------+------------------------------------------------------------+---------------+
| channel_threshold  | Array of threshold values for each channel                 | -             |
+--------------------+------------------------------------------------------------+---------------+
| channel_gold_value | Reference values for touch channels (optional)             | NULL          |
+--------------------+------------------------------------------------------------+---------------+
| debounce_times     | Consecutive readings for state change                      | 3             |
+--------------------+------------------------------------------------------------+---------------+
| filter_reset_times | Consecutive readings to reset position filter              | -             |
+--------------------+------------------------------------------------------------+---------------+
| position_range     | Maximum slider position value (higher = better resolution) | -             |
+--------------------+------------------------------------------------------------+---------------+
| calculate_window   | Window size for position calculation                       | Auto (0)      |
+--------------------+------------------------------------------------------------+---------------+
| swipe_threshold    | Speed threshold for identifying swiping                    | -             |
+--------------------+------------------------------------------------------------+---------------+
| swipe_hysterisis   | Speed hysteresis for identifying swiping                   | -             |
+--------------------+------------------------------------------------------------+---------------+
| swipe_alpha        | Filter parameter for estimating swipe speed                | -             |
+--------------------+------------------------------------------------------------+---------------+
| skip_lowlevel_init | Skip touch driver initialization if exists                 | false         |
+--------------------+------------------------------------------------------------+---------------+

Calculate Window Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``calculate_window`` parameter determines how many adjacent touch channels are used for position calculation. This affects the precision and noise immunity of the slider.

**Default Values (when set to 0):**

- **2 channels**: ``calculate_window = 2`` (uses all available channels)
- **3+ channels**: ``calculate_window = 3`` (optimal balance between precision and noise immunity)

**Manual Configuration:**

- You can explicitly set any value from 2 to ``channel_num``
- Smaller values (2): Higher sensitivity but more susceptible to noise
- Larger values (3+): Better noise immunity but potentially lower resolution
- For high-precision applications: Recommended value is ``min(3, channel_num)``
- **Note**: Minimum value is 2, as slider position calculation requires at least 2 adjacent channels

**Backward Compatibility:**

- Setting ``calculate_window = 0`` enables automatic default selection
- Existing code that doesn't initialize this field will use optimal defaults
- Explicitly set values continue to work as before

API Usage Examples
---------------------

Create and Initialize
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: c

    uint32_t channel_list[] = {2, 4, 6, 12, 10, 8};
    float threshold[] = {0.005f, 0.005f, 0.005f, 0.005f, 0.005f, 0.005f};
    touch_slider_config_t config = {
        .channel_num = 6,
        .channel_list = channel_list,
        .channel_threshold = threshold,
        .filter_reset_times = 5,
        .position_range = 10000,
        .swipe_alpha = 0.9,
        .swipe_threshold = 50,
        .swipe_hysterisis = 40,
        .channel_gold_value = NULL,
        .debounce_times = 0,
        .calculate_window = 0,  // Use default value (auto-selected as 3 for 6 channels)
        .skip_lowlevel_init = false
    };

    // Test successful creation
    TEST_ASSERT_EQUAL(ESP_OK, touch_slider_sensor_create(&config, &s_touch_slider, touch_slider_position_callback, touch_slider_event_callback, NULL));
    TEST_ASSERT_NOT_NULL(s_touch_slider);

Event Callback
^^^^^^^^^^^^^^^^^^^^^^^^^^

The callback function is called when slide is detected. Gesture may be determined based on the swiping speed or displacement.

.. code:: c

    static void touch_slider_event_callback(touch_slider_handle_t handle, touch_slider_event_t event, int32_t data, void *cb_arg)
    {
        if (event == TOUCH_SLIDER_EVENT_RIGHT_SWIPE) {
            printf("Right swipe (speed)\n");
        } else if (event == TOUCH_SLIDER_EVENT_LEFT_SWIPE) {
            printf("Left swipe (speed)\n");
        } else if (event == TOUCH_SLIDER_EVENT_RELEASE) {
            printf("Slide %ld\n", data);
            if (data > 1000)
            {
                printf("Right swipe (displacement)\n");
            }
            else if (data < -1000)
            {
                printf("Left swipe (displacement)\n");
            }
        } else if (event == TOUCH_SLIDER_EVENT_POSITION)
        {
            printf("pos,%" PRId64 ",%lu\n", get_time_in_ms(), data);
        }
    }


Event Handling
^^^^^^^^^^^^^^^^

The touch button sensor component provides an event handling mechanism to process touch events in a non-blocking way. Events should be handled periodically in your application's main loop or in a dedicated task.

.. code:: c

    // In your main loop or task
    while (1) {
        // Process any pending touch events
        touch_slider_sensor_handle_events(s_touch_slider);
        
        // Add delay to prevent tight loop
        vTaskDelay(pdMS_TO_TICKS(20));  // 20ms interval is typically sufficient
    }


Examples
--------

- :example:`touch/touch_slider_sensor`

API Reference
---------------

.. include-build-file:: inc/touch_slider_sensor.inc
