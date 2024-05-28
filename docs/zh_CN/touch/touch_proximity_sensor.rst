触摸接近感应传感器
=======================

:link_to_translation:`en:[English]`

``touch_proximity_sensor`` 组件基于 ESP32S3 内置的触摸传感器进行开发，使用该组件可以轻松实现触摸接近感应功能。

.. note::
   - ESP32/ESP32-S2/ESP32-S3 触摸相关组件仅用于测试或演示目的。由于触摸功能的抗干扰能力差，可能无法通过 EMS 测试，因此不建议用于量产产品。
   - 该组件目前仅适用于 ESP32S3，并且需要 IDF 版本大于等于 v5.0。

Touch Proximity Sensor 用户指南
----------------------------------

-  开发板
   1. 可以使用的 `ESP-S2S3-Touch-DevKit-1 <https://docs.espressif.com/projects/espressif-esp-dev-kits/zh_CN/latest/esp32s2/esp32-s2-touch-devkit-1/user_guide.html>`__ 开发套件进行验证测试，主板为 ``MainBoard v1.1``，接近感应子板为 ``Proximity Board V1.0``。

Touch Proximity Sensor 配置参考
----------------------------------

1. 用户可通过 :cpp:type:`proxi_config_t` 配置接近感应传感器通道数等参数。
2. 使用 :cpp:func:`touch_proximity_sensor_create` 配置并创建接近感应传感器对象。
3. 使用 :cpp:func:`touch_proximity_sensor_start` 启动接近感应传感器。
4. 使用 :cpp:func:`touch_proximity_sensor_stop` 停止接近感应传感器。
5. 使用 :cpp:func:`touch_proximity_sensor_delete` 删除接近感应传感器对象，并释放资源。

Examples
-------------

1. :example:`touch/touch_proximity`

API Reference
-----------------

.. include-build-file:: inc/touch_proximity_sensor.inc
