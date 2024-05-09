Touch Proximity Sensor
===========================

:link_to_translation:`zh_CN:[中文]`

The ``touch_proximity_sensor`` component is developed based on the built-in touch sensors of ESP32S3. Using this component makes it easy to implement touch proximity sensing functionality.

.. note::
   - ESP32/ESP32-S2/ESP32-S3 touch-related components are intended for testing or demo purposes only. Due to the poor anti-interference capability of the touch functionality, it may not pass EMS testing, and therefore, it is not recommended for mass production products.
   - This component is currently only applicable to the ESP32S3 and requires an IDF version greater than or equal to v5.0.


Touch Proximity Sensor User Guide
-------------------------------------

-  Development board
   1. Validation testing can be performed using the `ESP-S2S3-Touch-DevKit-1 <https://docs.espressif.com/projects/espressif-esp-dev-kits/zh_CN/latest/esp32s2/esp32-s2-touch-devkit-1/user_guide.html>`__ development kit. The mainboard is ``MainBoard v1.1``, and the proximity sensing subboard is ``Proximity Board V1.0``.

Touch Proximity Sensor Config Reference
-------------------------------------------

1. Users can configure parameters such as the number of channels for the proximity sensor using :cpp:type:`proxi_config_t`.
2. Use :cpp:func:`touch_proximity_sensor_create` to configure and create a proximity sensor object.
3. Use :cpp:func:`touch_proximity_sensor_start` to start the proximity sensor.
4. Use :cpp:func:`touch_proximity_sensor_stop` to stop the proximity sensor.
5. Use :cpp:func:`touch_proximity_sensor_delete` to delete the proximity sensor object and release resources.

Examples
------------

- :example:`touch/touch_proximity`

API Reference
-----------------

.. include-build-file:: inc/touch_proximity_sensor.inc
