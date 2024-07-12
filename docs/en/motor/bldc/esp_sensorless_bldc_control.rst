ESP Sensorless BLDC Control Components
=======================================

:link_to_translation:`zh_CN:[中文]`

This guide includes the following content:

.. contents:: Table of Contents
    :local:
    :depth: 2

The `esp_sensorless_bldc_control` component is a sensorless BLDC square wave control library based on the ESP32 series chips. It currently supports the following features:

    - Zero-crossing detection based on ADC sampling
    - Zero-crossing detection supported by comparators
    - Rotor initial phase detection based on the pulse method
    - Stall protection

This article mainly explains how to use the `esp_sensorless_bldc_control` component for brushless motor development and does not cover the principles. For more information on the principles, please refer to:

    - :doc:`./bldc_overview` Overview of Brushless Motor Control
    - :doc:`./bldc_snls_adc` Zero-crossing Detection Based on ADC Sampling
    - :doc:`./bldc_snls_comparer` Zero-crossing Detection Based on Comparators

The sensorless square wave control process can be mainly divided into the following parts:

    - INJECT: Injection phase, obtaining the initial phase through high-frequency voltage pulses :cpp:enumerator:`INJECT`
    - ALIGNMENT: Alignment phase, fixing the rotor to the initial phase :cpp:enumerator:`ALIGNMENT`
    - DRAG: Drag phase, rotating the rotor through six-step commutation :cpp:enumerator:`DRAG`
    - CLOSED_LOOP: Sensorless closed-loop control, commuting by detecting back EMF zero-crossing points :cpp:enumerator:`CLOSED_LOOP`
    - BLOCKED: Motor stall :cpp:enumerator:`BLOCKED`
    - STOP: Motor stop :cpp:enumerator:`STOP`
    - FAULT: Motor fault :cpp:enumerator:`FAULT`

Next, the specific processes of each part and the parameters to be noted will be introduced.

INJECT
------

The initial phase of the motor is obtained through pulse injection, and the bus current is collected at the low end of the inverter circuit. As shown in the figure below:

.. figure:: ../../../_static/motor/bldc/bldc_inject.png
    :align: center
    :width: 100%
    :alt: BLDC Bus Current Collection

    BLDC Bus Current Collection

.. note::
    Since the current cannot be collected directly, a sampling resistor is used to convert the current into voltage. Note that the voltage needs to be converted to a range that the ESP32 ADC can collect. Please refer to: `ESP32 ADC <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc_oneshot.html#adc-oneshot-unit-configuration>`__

Since the current only exists when both the upper and lower tubes are conducting, ADC sampling needs to be performed when the upper tube is conducting. Configure MCPWM in rising and falling mode and sample when the counter reaches the peak to accurately collect the bus voltage.

.. figure:: ../../..//_static/motor/bldc/bldc_mcpwm_rising_falling_mode.png
    :align: center
    :width: 100%
    :alt: BLDC MCPWM Rising and Falling Mode

    MCPWM Rising and Falling Mode

.. note::
    The LEDC driver does not support callback triggering at high levels, so the INJECT mode cannot be used with the LEDC driving method.

:c:macro:`INJECT_ENABLE` is set to 1 to enable INJECT mode, otherwise, it is disabled. The default is 0. The PWM generation mode must be MCPWM.

:c:macro:`INJECT_DUTY` is the injected voltage size, generally using high duty cycle injection.

:c:macro:`CHARGE_TIME` is the inductance charging time and pulse injection time, which affects the accuracy of initial phase detection. If this value is too small, the collected ADC value will be 0; if it is too large, the ADC value will be too high. Manually rotate the motor, and it is best to obtain stable phases 1-6 in one turn without errors phases 0 and 7.

ALIGNMENT
---------

To ensure the brushless motor can start normally, it is necessary to determine the position of the rotor when it is stationary. In practical applications, this is done by energizing a set of windings for a certain period, fixing the rotor in a specific phase, and preparing for subsequent forced commutation.

:c:macro:`ALIGNMENTNMS` Alignment time, if too long, it will overcurrent. If too short, the rotor may not be aligned to the correct phase.

:c:macro:`ALIGNMENTDUTY` Alignment force.

DRAG
----

The rotor is dragged through six-step commutation, using a boost and frequency increase method. Gradually increase the voltage and commutation frequency to give the motor an initial speed with a significant back EMF. The motor should drag smoothly without noise or stuttering. The drag time does not need to be too long.

:c:macro:`RAMP_TIM_STA` Initial delay time for dragging.

:c:macro:`RAMP_TIM_END` Minimum delay time for dragging.

:c:macro:`RAMP_TIM_STEP` Step increment for drag time.

:c:macro:`RAMP_DUTY_STA` Initial duty cycle for dragging.

:c:macro:`RAMP_DUTY_END` Maximum duty cycle for dragging.

:c:macro:`RAMP_DUTY_INC` Step increment for the duty cycle.

.. note::
    The strong drag parameters need to be tuned in the motor's working environment, and the no-load parameters may not apply to the loaded condition.

CLOSED_LOOP
-----------

Zero-crossing Detection Based on ADC Sampling
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ADC sampling detects the zero-crossing point by collecting the floating phase voltage and motor power supply voltage, and sampling must be performed when the upper tube is conducting.

.. note::
    ADC zero-crossing detection must use MCPWM as the driver.

:c:macro:`ENTER_CLOSE_TIME` Sets the time to enter the closed loop. By default, the closed loop control can be entered after a period of strong drag.

:c:macro:`ZERO_REPEAT_TIME` The zero-crossing point is considered valid after being detected continuously N times.

:c:macro:`AVOID_CONTINUE_CURRENT_TIME` After commutation, there will be an impact of continuous current. Delay detection to avoid continuous current.

Zero-crossing Detection Based on Comparator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Comparator zero-crossing detection compares the floating phase back EMF with the bus voltage using hardware comparators. The zero-crossing signal is detected by the GPIO pin. Due to many noise points in the actual process, multiple detections are needed to confirm the zero-crossing point.

:c:macro:`ZERO_STABLE_FLAG_CNT` Enter sensorless control after multiple stable zero-crossing signals are detected.

:c:macro:`ZERO_CROSS_DETECTION_ACCURACY` Continuous detection of the same signal N times is considered a stable signal. 0xFF means 8 times, 0XFFFF means 16 times. The current maximum supported filtering times are 0xFFFFFFFF. If it still cannot enter the closed loop state, check for hardware issues.

.. note::
    Hardware troubleshooting directions mainly include checking whether the filter capacitors for the three-phase terminal voltage and comparator output are set reasonably.

Advance Commutation
^^^^^^^^^^^^^^^^^^^

The zero-crossing signal generally arrives 30° before commutation. Once the zero-crossing signal is detected, a 30° delay is required. However, during motor rotation, due to variable electrical cycles, software filtering, and delays, a slight compensation for the commutation time is necessary.

:c:macro:`ZERO_CROSS_ADVANCE` Advance commutation time, the advance angle is 180 / ZERO_CROSS_ADVANCE, default is 6.

.. note::
    The commutation angle is not better the earlier it is. Use an oscilloscope to observe whether the calculated commutation angle matches the actual commutation angle.

Stall Protection
----------------

If the motor does not commutate for a long time, it is considered stalled. At this point, the motor stops running and enters stall protection status.

Speed Control
-------------

The speed is controlled by PID to achieve the set speed.

:c:macro:`SPEED_KP` P value of speed control.

:c:macro:`SPEED_KI` I value of speed control.

:c:macro:`SPEED_KD` D value of speed control.

:c:macro:`SPEED_MIN_INTEGRAL` Minimum integral value of speed control.

:c:macro:`SPEED_MAX_INTEGRAL` Maximum integral value of speed control.

:c:macro:`SPEED_MIN_OUTPUT` Minimum output value of speed control.

:c:macro:`SPEED_MAX_OUTPUT` Maximum output value of speed control, not exceeding the maximum duty cycle.

:c:macro:`SPEED_CAL_TYPE` Position PID or Incremental PID.

:c:macro:`SPEED_MAX_RPM` Maximum RPM.

:c:macro:`SPEED_MIN_RPM` Minimum RPM.

:c:macro:`MAX_SPEED_MEASUREMENT_FACTOR` To avoid erroneous speed detection, if the detected speed exceeds this set factor, it is considered an erroneous speed detection.

API Reference
-------------

.. include-build-file:: inc/bldc_control.inc

.. include-build-file:: inc/bldc_user_cfg.inc
