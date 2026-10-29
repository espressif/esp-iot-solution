**Zero_Detection**
==================

:link_to_translation:`zh_CN:[中文]`

The zero cross detection driver is a component designed to analyze zero cross signals. By examining the period and triggering edges of zero cross signals, it can determine the signal's validity, invalidity, whether it exceeds the expected frequency range, and if there are signal losses, among other conditions.

The zero cross detection component supports the detection of two types of zero cross signals.

- Square Wave: Inverts the current voltage level when the signal crosses zero.
- Pulse Wave: A pulse generated when the signal crosses zero.

Due to factors such as response delay, this component supports two driving modes, including GPIO interrupt and MCPWM capture.

Users can flexibly configure the component's drive modes, as well as adjust parameters such as the effective frequency range and the number of valid signal judgments. This provides a high level of flexibility.

The program returns results and data in the form of events, meeting the user's need for timely processing of signals.

Zero Detection event
--------------------

Triggering conditions for each zero detection event are enlisted in the table below:

+--------------------------+------------------------------------------------------------------------------------------+
| Event                    | Trigger Condition                                                                        |
+==========================+==========================================================================================+
| SIGNAL_RISING_EDGE       | Rising edge                                                                              |
+--------------------------+------------------------------------------------------------------------------------------+
| SIGNAL_FALLING_EDGE      | Falling edge                                                                             |
+--------------------------+------------------------------------------------------------------------------------------+
| SIGNAL_VALID             | The number of times the frequency is within the valid range exceeds the valid times.     |
+--------------------------+------------------------------------------------------------------------------------------+
| SIGNAL_INVALID           | The number of times the frequency is within the invalid range exceeds the invalid times. |
+--------------------------+------------------------------------------------------------------------------------------+
| SIGNAL_LOST              | No rising or falling edges in the signal within 100ms.                                   |
+--------------------------+------------------------------------------------------------------------------------------+
| SIGNAL_FREQ_OUT_OF_RANGE | The calculated frequency is outside the set frequency range.                             |
+--------------------------+------------------------------------------------------------------------------------------+


.. attention:: No blocking operations such as **TaskDelay** are allowed in the call-back function

Configuration
-------------

- USE_GPTIMER : Decide whether to use the GPTimer driver; default is to use the ESPTimer

Demonstration
--------------

Create a zero detection
^^^^^^^^^^^^^^^^^^^^^^^
.. code:: c

    void IRAM_ATTR zero_detection_event_cb(zero_detect_event_t zero_detect_event, zero_detect_cb_param_t *param, void *usr_data)  //User's callback API
    {
        switch (zero_detect_event) {
        case SIGNAL_FREQ_OUT_OF_RANGE:
            ESP_LOGE(TAG, "SIGNAL_FREQ_OUT_OF_RANGE");
            break;
        case SIGNAL_VALID:
            ESP_LOGE(TAG, "SIGNAL_VALID");
            break;
        case SIGNAL_LOST:
            ESP_LOGE(TAG, "SIGNAL_LOST");
            break;
        default:
            break;
        }
    }

    // Create a zero detection and register call-back
    zero_detect_config_t config = {
        .capture_pin = 2,
        .freq_range_max_hz = 65,
        .freq_range_min_hz = 45,  //Hz
        .valid_times = 6,
        .invalid_times = 5,
        .zero_signal_type = SQUARE_WAVE,
        .zero_driver_type = MCPWM_TYPE,
    };
    zero_detect_handle_t *g_zcds = zero_detect_create(&config);
    if(NULL == g_zcds) {
        ESP_LOGE(TAG, "Zero Detection create failed");
    }
    zero_detect_register_cb(g_zcds, zero_detection_event_cb, NULL);

API Reference
-------------

.. include-build-file:: inc/zero_detection.inc