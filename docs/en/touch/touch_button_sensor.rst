Touch Button Sensor
=====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------

The ``touch_button_sensor`` component (published as version 0.3.0) combines the touch low-level driver and the touch FSM. It accepts complete multi-frequency frames, reports the positive fused amplitude, and emits debounced active/inactive callbacks. The component is intended for testing and evaluation. It requires ESP-IDF 5.5 or newer on the targets listed by its component manifest.

The low-level driver performs its own startup alignment before the button callbacks are registered. A button held during startup can therefore affect the initial operating point; tune the threshold from the target's measured FSM decision-domain amplitude.

Configuration
-------------

Use :cpp:type:`touch_button_config_t`:

.. code:: c

    typedef struct {
        uint32_t channel_num;
        uint32_t *channel_list;
        uint32_t *thresholds;
        uint32_t sample_period_ms;
        uint32_t sample_cfg_num;
        uint32_t debounce_active;
        uint32_t debounce_inactive;
        bool skip_lowlevel_init;
    } touch_button_config_t;

``thresholds`` contains one absolute normalized FSM decision threshold per channel. It is not a percentage and does not use a legacy reference-value array. Zero sample settings select the low-level defaults. Zero debounce settings select the FSM defaults. When ``skip_lowlevel_init`` is true, the application must own and start a compatible low-level instance first.

Example
-------

.. code:: c

    static uint32_t channels[] = {1, 2, 3, 4};
    static uint32_t thresholds[] = {70, 70, 70, 70};
    touch_button_config_t config = {
        .channel_num = 4,
        .channel_list = channels,
        .thresholds = thresholds,
        .sample_period_ms = 20,
        .sample_cfg_num = 0,
        .debounce_active = 0,
        .debounce_inactive = 0,
        .skip_lowlevel_init = false,
    };
    touch_button_handle_t handle = NULL;
    ESP_ERROR_CHECK(touch_button_sensor_create(&config, &handle,
                                               button_state_callback, NULL));

    while (true) {
        ESP_ERROR_CHECK(touch_button_sensor_handle_events(handle));
        vTaskDelay(pdMS_TO_TICKS(10));
    }

The callback receives the channel and stable state. Use :cpp:func:`touch_button_sensor_get_data` for the current positive fused amplitude and :cpp:func:`touch_button_sensor_get_state` for the current state. The published component exposes only this public button API; it does not provide a wrapper debug switch or the FSM private headers. For raw, baseline, fusion, or FSM snapshot diagnostics, build the repository FSM example, which owns the debug configuration and private include path.

.. code:: c

    static void button_state_callback(touch_button_handle_t handle,
                                      uint32_t channel, touch_state_t state,
                                      void *arg)
    {
        (void)handle;
        (void)arg;
        printf("channel=%" PRIu32 " state=%d\n", channel, state);
    }

Examples
--------

- :example:`touch/touch_button_sensor`

API Reference
-------------

The structured public API header is maintained with the published component:

`touch_button_sensor public API <https://components.espressif.com/components/espressif/touch_button_sensor>`_.
