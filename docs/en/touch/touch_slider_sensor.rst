Touch Slider Sensor
=====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------

The ``touch_slider_sensor`` component uses one touch FSM for all configured frequencies. It calculates the slider position from each channel's positive fused filtered amplitude and reports position, swipe, and release events. It is intended for testing and evaluation and requires ESP-IDF 5.5 or newer on the targets listed by its component manifest.

Configuration
-------------

Use :cpp:type:`touch_slider_config_t`:

.. code:: c

    typedef struct {
        uint32_t channel_num;
        uint32_t *channel_list;
        uint32_t *thresholds;
        uint32_t sample_period_ms;
        uint32_t sample_cfg_num;
        uint32_t debounce_active;
        uint32_t debounce_inactive;
        uint32_t filter_reset_times;
        uint32_t position_range;
        uint8_t calculate_window;
        float swipe_threshold;
        float swipe_hysterisis;
        float swipe_alpha;
        bool skip_lowlevel_init;
    } touch_slider_config_t;

``thresholds`` contains absolute normalized FSM decision counts. These values are in the fused decision domain, rather than physical raw counts or a ratio relative to a presumed 5000 baseline. ``calculate_window = 0`` selects the default (2 for two channels, otherwise 3); explicit values from 1 through ``channel_num`` are valid. ``position_range`` must fit the signed event data range.

Example
-------

.. code:: c

    static uint32_t channels[] = {2, 4, 6, 12, 10, 8};
    static uint32_t thresholds[] = {25, 25, 25, 25, 25, 25};
    touch_slider_config_t config = {
        .channel_num = 6,
        .channel_list = channels,
        .thresholds = thresholds,
        .sample_period_ms = 0,
        .sample_cfg_num = 0,
        .debounce_active = 0,
        .debounce_inactive = 0,
        .filter_reset_times = 5,
        .position_range = 10000,
        .calculate_window = 0,
        .swipe_threshold = 50,
        .swipe_hysterisis = 40,
        .swipe_alpha = 0.9f,
        .skip_lowlevel_init = false,
    };
    touch_slider_handle_t handle = NULL;
    ESP_ERROR_CHECK(touch_slider_sensor_create(&config, &handle,
                                               touch_slider_event_callback, NULL));

    while (true) {
        ESP_ERROR_CHECK(touch_slider_sensor_handle_events(handle));
        vTaskDelay(pdMS_TO_TICKS(20));
    }

The callback receives ``TOUCH_SLIDER_EVENT_POSITION`` with a position in ``[0, position_range]``. Release events carry a signed displacement. The optional ``touch_slider_sensor_get_data`` query returns the positive fused filtered amplitude for a configured channel; it no longer takes a frequency argument.

Examples
--------

- :example:`touch/touch_slider_sensor`

API Reference
-------------

.. include-build-file:: inc/touch_slider_sensor.inc
