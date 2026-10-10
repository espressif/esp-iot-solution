Touch Proximity Sensor
=========================

:link_to_translation:`zh_CN:[中文]`

Overview
--------

The ``touch_proximity_sensor`` component uses the ESP32-S3 proximity touch data path. Proximity remains an independent low-level data type: its accumulated measurements are not treated as ordinary touch raw counts, are not aligned to the ordinary 5000-count startup target, and do not use the ordinary touch frequency estimator. The component requires ESP-IDF 5.5 or newer on ESP32-S3.

The wrapper feeds complete proximity frames to one FSM. Its effective FSM period is the low-level scan period multiplied by the actual ``proximity_count`` reported by low-level. Tune thresholds from a target idle trace after the independent proximity baseline has settled.

Configuration
-------------

Use :cpp:type:`touch_proxi_config_t`:

.. code:: c

    typedef struct {
        uint32_t channel_num;
        uint32_t *channel_list;
        uint32_t *thresholds;
        uint32_t sample_period_ms;
        uint32_t sample_cfg_num;
        uint32_t proximity_count;
        uint32_t debounce_active;
        uint32_t debounce_inactive;
        bool skip_lowlevel_init;
    } touch_proxi_config_t;

``thresholds`` are absolute normalized FSM decision counts for the proximity data domain; they are not percentages or ordinary touch raw counts. A zero sampling field selects the low-level default. A zero ``proximity_count`` selects the Kconfig default (1 through 255). With ``skip_lowlevel_init = true``, the existing low-level instance must be a proximity instance and its actual count must match an explicitly requested count.

Example
-------

.. code:: c

    static uint32_t channels[] = {8};
    static uint32_t thresholds[] = {25};
    touch_proxi_config_t config = {
        .channel_num = 1,
        .channel_list = channels,
        .thresholds = thresholds,
        .sample_period_ms = 0,
        .sample_cfg_num = 0,
        .proximity_count = 0,
        .debounce_active = 2,
        .debounce_inactive = 2,
        .skip_lowlevel_init = false,
    };
    touch_proximity_handle_t handle = NULL;
    ESP_ERROR_CHECK(touch_proximity_sensor_create(&config, &handle,
                                                  proximity_callback, NULL));

    while (true) {
        ESP_ERROR_CHECK(touch_proximity_sensor_handle_events(handle));
        vTaskDelay(pdMS_TO_TICKS(20));
    }

Use :cpp:func:`touch_proximity_sensor_get_data` for the positive fused filtered amplitude and :cpp:func:`touch_proximity_sensor_get_state` for the stable state. The callback receives active and inactive transitions.

Examples
--------

- :example:`touch/touch_proximity`

API Reference
-------------

.. include-build-file:: inc/touch_proximity_sensor.inc
